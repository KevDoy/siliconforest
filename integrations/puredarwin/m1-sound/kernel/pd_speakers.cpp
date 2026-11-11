// SPDX-License-Identifier: GPL-2.0-only
/*
 * The built-in speakers of the J293 and the rules that keep them from being damaged.
 *
 * Hardware, as the Omacom Linux device tree (t8103-j293.dts) describes it: four
 * TAS5770L amplifiers, left front and left rear on i2c1 (0x32, 0x31) and the I2S port
 * of cluster 0, right front and right rear on i2c3 (0x35, 0x34) and the port of
 * cluster 1; one shutdown line for all (gpio pin 181, low = hardware shutdown). The
 * stream is Linux's "Secondary" front end: cluster 1 provides the clocks to both
 * ports, 8 slots of 32 bits (256 bit clocks per frame), one slot per amplifier in the
 * order left front, right front, left rear, right rear. Each amplifier returns its
 * current and voltage in 16-bit slots on its port's input line; cluster 2 captures
 * them as a clock consumer (Linux's "Speaker Sense" front end).
 *
 * The policy is that of sound/soc/apple/macaudio.c (macaudio_j293_cfg,
 * macaudio_set_speaker, macaudio_vlimit_*, macaudio_slk_*), Copyright (C) The Asahi
 * Linux Contributors, and it is enforced here, in the kernel, whatever userspace does:
 *
 *  1. The amplifier gain register is written by this file only, always SPK_AMP_GAIN
 *     (18.5 dBV, the cap of macaudio_j293_cfg). Nothing can change it.
 *  2. The digital volume of every amplifier is held at control value 1 (-100 dB)
 *     unless the volume lock is open. It opens only for the one process that holds
 *     /dev/pdspeaker-safety (the safety daemon, root) and has sent the magic value
 *     within the last 250 ms of playback. The lease runs down only while the speakers
 *     play. When it runs out, or the device is closed, the volumes go back to -100 dB
 *     at once and the next ping fails with ETIMEDOUT.
 *  3. No other interface reaches the volume: /dev/pdspeaker takes samples only, and
 *     the sysctls are read-only apart from the test tone.
 *  4. Beyond Linux: while the speakers play, the amplifiers' latched fault bits, their
 *     power state and the gain and volume registers are read back four times a second.
 *     A fault, a register that is not what was written, any failed I2C transfer, a DMA
 *     ring error or a ring that ran dry, or a DART fault shuts all four amplifiers
 *     down (software shutdown, then the hardware shutdown line) and stops the stream.
 *     After SPK_MAX_TRIPS such stops the speakers stay off until the next boot.
 *  5. Between sessions the amplifiers are in hardware shutdown; every session starts
 *     from a software reset.
 *
 * Interfaces:
 *   /dev/pdspeaker          write-only, one writer: signed 16-bit little-endian stereo
 *                           at kern.pd_speaker.rate; left goes to both left
 *                           amplifiers, right to both right ones
 *   /dev/pdspeaker-safety   the safety daemon's (../include/pd_speaker_safety.h)
 *   kern.pd_speaker.*       state, counters, sense statistics; tone=<seconds> plays
 *                           the test tone (at the volume in force: -100 dB when locked)
 *
 * Locking: one mutex for everything here; one thread services the DMA rings every
 * 5 ms while playing. No interrupts.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include "pd_i2c.h"
#include "pd_mca.h"
#include "pd_speaker_safety.h"
#include "pd_speakers.h"
#include "pd_tas2770.h"

#define LOG(fmt, ...) IOLog("PDSpeakers: " fmt "\n", ##__VA_ARGS__)

#define PIN_SPEAKER_SDZ     181     /* /arm-io/gpio: high lets the amplifiers run */
#define SPK_AMP_GAIN        PDSPK_AMP_GAIN_REG
#define SPK_VOLUME_REG(ctl) ((uint8_t)(PDSPK_VOLUME_MAX - (ctl)))  /* the control is inverted: 0xc8 is -100 dB */
#define SPK_FE_CLUSTER      1       /* Linux's Secondary front end */
#define SPK_PORTS           0x3     /* the I2S ports of clusters 0 and 1 */
#define SPK_SLOTS           8
#define SPK_SENSE_CLUSTER   2
#define SPK_SENSE_SLOTS     16
#define SPK_MAX_TRIPS       3

#define PERIOD_FRAMES       1024
#define PLAY_FRAME_BYTES    16      /* 4 x 32 bits */
#define SENSE_FRAME_BYTES   16      /* 8 x 16 bits */
#define PERIOD_BYTES        (PERIOD_FRAMES * 16)
#define RING_PERIODS        8
#define FIFO_FRAMES         16384   /* writer -> thread, stereo */
#define SENSE_FIFO_FRAMES   16384   /* thread -> safety daemon */
#define TRAILING_SILENCE    6
#define CHECK_TICKS         50      /* amplifier registers every 250 ms */
#define ADMAC_COUNTER_HZ    24000000ULL
#define MIN_TONE_DB         20      /* the test tone is never louder than this below full scale */

static const struct {
	const char *name;
	unsigned    bus;        /* 0: i2c1, 1: i2c3 */
	uint8_t     addr;
	uint8_t     slot, imon, vmon, pdm;
	bool        pulldown;   /* dai-tdm-idle-mode = "pulldown": one amplifier per data line */
} kAmps[PDSPK_SPEAKERS] = {
	{ "left front",  0, 0x32, 0, 0,  2,  4,  true },
	{ "right front", 1, 0x35, 1, 4,  6,  8,  true },
	{ "left rear",   0, 0x31, 2, 8,  10, 12, false },
	{ "right rear",  1, 0x34, 3, 12, 14, 16, false },
};

enum { kOff, kFailed, kIdle, kPlaying, kDisabled };
static const char *const kStateNames[] = { "off", "failed", "idle", "playing", "disabled after faults" };

static IOLock      *gLock;
static PDGpio      *gGpio;
static PDDartFixed *gDart;
static PDI2C        gBus[2];
static PDTas2770    gAmp[PDSPK_SPEAKERS];
static PDMca        gMca;
static PDMcaCapture gSense;
static uint8_t     *gPlayRing, *gSenseRing;
static uint64_t     gPlayIova, gSenseIova;
static int          gState = kOff;
static char         gFailure[96];
static uint32_t     gRate = 48000;

/* the volume lock (macaudio's speaker_volume_unlocked, speaker_lock_remain, ...) */
static bool     gOwner;                 /* /dev/pdspeaker-safety is open */
static bool     gUnlocked;
static bool     gWasLocked;             /* the lease ran out: the next ping says so */
static uint64_t gLeaseNs;               /* remaining while not playing */
static uint64_t gLeaseDeadline;         /* uptime at which it runs out, while playing */
static uint32_t gVolume[PDSPK_SPEAKERS] = { PDSPK_VOLUME_LOCKED, PDSPK_VOLUME_LOCKED, PDSPK_VOLUME_LOCKED,
	                                    PDSPK_VOLUME_LOCKED };
static uint32_t gPings, gLeaseTimeouts, gVolumeRefused, gVolumeSets;

/* writer and tone */
static int32_t *gFifo;                  /* stereo frames of int32 */
static uint32_t gFifoHead, gFifoCount;
static bool     gDevOpen;
static int      gFifoEvent;
static uint64_t gToneLeft;
static uint32_t gTonePhase, gToneHz = 1000, gToneDb = 30;

/* sense: thread -> daemon */
static int16_t *gSenseFifo;             /* frames of 8 int16 */
static uint32_t gSenseHead, gSenseCount;
static int      gSenseEvent;
static uint32_t gSenseDropped;
static int32_t  gSensePeak[PDSPK_SENSE_CHANNELS];       /* largest |sample| of the last period */
static int32_t  gSenseMean[PDSPK_SENSE_CHANNELS];       /* mean of the last period (offset) */
static int32_t  gSensePeakSession[PDSPK_SENSE_CHANNELS];

/* one session */
static uint32_t gNext, gSenseNext, gSilentRun, gTicks;
static uint64_t gSubmitted, gDone, gSenseDone, gFirstCounter, gLastCounter, gDoneAtFirst;
static uint64_t gFramesData, gFramesTone;
static uint32_t gLate, gRingErrors;
/* since boot */
static uint32_t gSessions, gTrips, gLastRateMilliHz;
static uint64_t gTotalDone, gTotalSense;
static char     gTripReason[96];
static uint8_t  gAmpRev[PDSPK_SPEAKERS], gLat0[PDSPK_SPEAKERS], gLat1[PDSPK_SPEAKERS], gPwr[PDSPK_SPEAKERS],
                gGainSeen[PDSPK_SPEAKERS], gVolSeen[PDSPK_SPEAKERS];
static int32_t  gDieMilli[PDSPK_SPEAKERS] = { INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN };

static int   gMajor = -1, gSafetyMajor = -1;
static void *gNode, *gSafetyNode;

static uint64_t
uptime_ns(void)
{
	uint64_t abs = 0, ns = 0;

	clock_get_uptime(&abs);
	absolutetime_to_nanoseconds(abs, &ns);
	return ns;
}

/* ---- amplifiers ----------------------------------------------------------------------- */

/* Software shutdown where the bus still answers, then the hardware shutdown line. Never fails. */
static void
amps_off(void)
{
	for (unsigned i = 0; i < PDSPK_SPEAKERS; i++)
		gAmp[i].upd(0x02, 0x03, kPDTasShutdown);
	gGpio->output(PIN_SPEAKER_SDZ, 0);
}

/* Out of hardware shutdown and configured, in software shutdown, at the volume the lock allows. */
static int
amps_on(void)
{
	int err = 0;

	gGpio->output(PIN_SPEAKER_SDZ, 1);
	IOSleep(2);
	for (unsigned i = 0; i < PDSPK_SPEAKERS && err == 0; i++) {
		err = gAmp[i].init(kAmps[i].slot, kAmps[i].vmon, kAmps[i].imon, kAmps[i].pdm, kAmps[i].pulldown,
		    SPK_AMP_GAIN, SPK_VOLUME_REG(gUnlocked ? gVolume[i] : PDSPK_VOLUME_LOCKED));
		gAmpRev[i] = gAmp[i].rev;
		if (err != 0)
			LOG("amplifier %s (0x%02x) did not take its configuration (error %d)", kAmps[i].name, kAmps[i].addr, err);
	}
	return err;
}

/* Reads back what protects the speakers. 0, or the index + 1 of the amplifier that is not as it must be. */
static unsigned
amps_check(char *why, size_t len)
{
	for (unsigned i = 0; i < PDSPK_SPEAKERS; i++) {
		PDTas2770 &a = gAmp[i];
		uint8_t want = SPK_VOLUME_REG(gUnlocked ? gVolume[i] : PDSPK_VOLUME_LOCKED);

		if (a.faults(&gLat0[i], &gLat1[i]) != 0 || a.rd(0x02, &gPwr[i]) != 0 || a.rd(0x03, &gGainSeen[i]) != 0 ||
		    a.rd(0x05, &gVolSeen[i]) != 0) {
			snprintf(why, len, "%s: I2C error %d", kAmps[i].name, a.error);
			return i + 1;
		}
		a.dieTemp(&gDieMilli[i]);
		if ((gLat0[i] & PD_TAS_FAULT0_MASK) || (gLat1[i] & PD_TAS_FAULT1_MASK)) {
			snprintf(why, len, "%s: fault latched 0x%02x/0x%02x", kAmps[i].name, gLat0[i], gLat1[i]);
			return i + 1;
		}
		if ((gPwr[i] & 0x03) != kPDTasActive || (gPwr[i] & 0x0c) != 0) {
			snprintf(why, len, "%s: power control 0x%02x, not playing with sense on", kAmps[i].name, gPwr[i]);
			return i + 1;
		}
		if ((gGainSeen[i] & 0x1f) != SPK_AMP_GAIN || gVolSeen[i] != want) {
			snprintf(why, len, "%s: gain 0x%02x volume 0x%02x, written 0x%02x and 0x%02x", kAmps[i].name,
			    gGainSeen[i], gVolSeen[i], SPK_AMP_GAIN, want);
			return i + 1;
		}
	}
	return 0;
}

/* ---- the volume lock ------------------------------------------------------------------ */

/* macaudio_vlimit_unlock(false): every volume to -100 dB, now. */
static void
lock_volumes(const char *why)
{
	if (gUnlocked)
		LOG("speaker volumes locked: %s", why);
	gUnlocked = false;
	for (unsigned i = 0; i < PDSPK_SPEAKERS; i++) {
		gVolume[i] = PDSPK_VOLUME_LOCKED;
		if (gState == kPlaying)
			gAmp[i].setVolume(SPK_VOLUME_REG(PDSPK_VOLUME_LOCKED));
	}
}

/* macaudio_vlimit_update(): open only with an owner whose lease has time left. */
static void
update_lock(void)
{
	uint64_t left = gState == kPlaying ? (gLeaseDeadline > uptime_ns() ? gLeaseDeadline - uptime_ns() : 0) : gLeaseNs;

	if (gOwner && left > 0) {
		if (!gUnlocked)
			LOG("speaker volumes unlocked");
		gUnlocked = true;
	} else {
		lock_volumes(!gOwner ? "no safety daemon" : "the daemon's lease ran out");
	}
}

/* macaudio_vlimit_timeout_work(), polled from the thread while playing. */
static void
lease_tick(void)
{
	if (gOwner && gUnlocked && uptime_ns() >= gLeaseDeadline) {
		gLeaseNs = 0;
		gLeaseDeadline = 0;
		gWasLocked = true;
		gLeaseTimeouts++;
		lock_volumes("the daemon's lease ran out");
	}
}

/* ---- playback and sense --------------------------------------------------------------- */

static const uint32_t kGainQ16[6] = { 65536, 58409, 52057, 46396, 41350, 36854 };

static int32_t
tone_sample(void)
{
	uint32_t db = gToneDb < MIN_TONE_DB ? MIN_TONE_DB : gToneDb;
	uint32_t gain = db / 6 >= 16 ? 0 : kGainQ16[db % 6] >> (db / 6);
	int32_t t = (int32_t)(gTonePhase >> 16) - 32768, a = t < 0 ? -t : t;
	int32_t y = (int32_t)(((int64_t)4 * t * (32768 - a)) >> 15), ay = y < 0 ? -y : y;

	/* sin(pi t / 32768) in Q15: a parabola and one correction step */
	y += (int32_t)(((int64_t)7373 * ((int32_t)(((int64_t)y * ay) >> 15) - y)) >> 15);
	y = y > 32767 ? 32767 : y < -32767 ? -32767 : y;
	gTonePhase += (uint32_t)(((uint64_t)gToneHz << 32) / gRate);
	return (int32_t)((int64_t)y * gain);
}

static void
fill_period(uint32_t index)
{
	int32_t *dst = (int32_t *)(void *)(gPlayRing + index * PERIOD_BYTES);
	bool any = false;
	uint32_t taken = 0;

	for (unsigned i = 0; i < PERIOD_FRAMES; i++) {
		int32_t l = 0, r = 0;

		if (gFifoCount > 0) {
			l = gFifo[2 * gFifoHead];
			r = gFifo[2 * gFifoHead + 1];
			gFifoHead = (gFifoHead + 1) % FIFO_FRAMES;
			gFifoCount--;
			taken++;
			any = true;
		} else if (gToneLeft > 0) {
			l = r = tone_sample();
			gToneLeft--;
			gFramesTone++;
			any = true;
		}
		/* slots: left front, right front, left rear, right rear */
		dst[4 * i] = l;
		dst[4 * i + 1] = r;
		dst[4 * i + 2] = l;
		dst[4 * i + 3] = r;
	}
	gFramesData += taken;
	gSilentRun = any ? 0 : gSilentRun + 1;
	if (taken > 0)
		IOLockWakeup(gLock, &gFifoEvent, false);
}

/* One captured period: statistics, and into the daemon's FIFO (the oldest frames give way). */
static void
take_sense(uint32_t index)
{
	const int16_t *src = (const int16_t *)(const void *)(gSenseRing + index * PERIOD_BYTES);
	int32_t peak[PDSPK_SENSE_CHANNELS] = { 0 };
	int64_t sum[PDSPK_SENSE_CHANNELS] = { 0 };

	for (unsigned i = 0; i < PERIOD_FRAMES; i++) {
		for (unsigned c = 0; c < PDSPK_SENSE_CHANNELS; c++) {
			int32_t v = src[PDSPK_SENSE_CHANNELS * i + c];
			sum[c] += v;
			if (v < 0)
				v = -v;
			if (v > peak[c])
				peak[c] = v;
		}
		if (gOwner) {
			if (gSenseCount == SENSE_FIFO_FRAMES) {
				gSenseHead = (gSenseHead + 1) % SENSE_FIFO_FRAMES;
				gSenseCount--;
				gSenseDropped++;
			}
			memcpy(gSenseFifo + PDSPK_SENSE_CHANNELS * ((gSenseHead + gSenseCount) % SENSE_FIFO_FRAMES),
			    src + PDSPK_SENSE_CHANNELS * i, SENSE_FRAME_BYTES);
			gSenseCount++;
		}
	}
	for (unsigned c = 0; c < PDSPK_SENSE_CHANNELS; c++) {
		gSensePeak[c] = peak[c];
		gSenseMean[c] = (int32_t)(sum[c] / PERIOD_FRAMES);
		if (peak[c] > gSensePeakSession[c])
			gSensePeakSession[c] = peak[c];
	}
	if (gOwner)
		IOLockWakeup(gLock, &gSenseEvent, false);
}

static void
stop_streams(void)
{
	gSense.stop();
	gMca.stopDma();
	gMca.stopClocks();
	gDart->check("speakers");
}

/* Rule 4: everything off, now. */
static void
trip(const char *why)
{
	amps_off();
	stop_streams();
	gTrips++;
	snprintf(gTripReason, sizeof(gTripReason), "%s", why);
	gLeaseNs = gLeaseDeadline > uptime_ns() ? gLeaseDeadline - uptime_ns() : 0;
	gToneLeft = 0;
	gFifoCount = 0;
	gState = gTrips >= SPK_MAX_TRIPS ? kDisabled : kIdle;
	lock_volumes("amplifiers shut down");
	IOLockWakeup(gLock, &gFifoEvent, false);
	IOLockWakeup(gLock, &gSenseEvent, false);
	LOG("SHUT DOWN: %s (stop %u of %u before the speakers stay off)", why, gTrips, SPK_MAX_TRIPS);
}

static void
start_playback(void)
{
	char why[96];
	int err;

	gNext = gSenseNext = gSilentRun = gTicks = 0;
	gSubmitted = gDone = gSenseDone = gFirstCounter = gLastCounter = gDoneAtFirst = 0;
	gFramesData = gFramesTone = 0;
	gLate = gRingErrors = 0;
	gTonePhase = 0;
	bzero(gSensePeakSession, sizeof(gSensePeakSession));
	bzero(gSenseRing, PD_SPK_SENSE_RING_BYTES);

	err = amps_on();
	if (err == 0)
		err = gMca.startClocks(gRate);
	for (unsigned i = 0; i < PDSPK_SPEAKERS && err == 0; i++)
		err = gAmp[i].setRate(gRate);
	/* powered and muted, sense on: the amplifiers see the clocks from here */
	for (unsigned i = 0; i < PDSPK_SPEAKERS && err == 0; i++)
		err = gAmp[i].power(kPDTasMute);
	if (err == 0)
		err = gSense.prepare();
	if (err != 0) {
		snprintf(why, sizeof(why), "could not start at %u Hz (error %d)", gRate, err);
		trip(why);
		return;
	}
	while (!gSense.descRingFull()) {
		gSense.submit(gSenseIova + (uint64_t)gSenseNext * PERIOD_BYTES, PERIOD_BYTES);
		gSenseNext = (gSenseNext + 1) % RING_PERIODS;
	}
	gSense.start();
	gMca.prepareDma();
	while (!gMca.descRingFull()) {
		fill_period(gNext);
		gMca.submit(gPlayIova + (uint64_t)gNext * PERIOD_BYTES, PERIOD_BYTES);
		gNext = (gNext + 1) % RING_PERIODS;
		gSubmitted++;
	}
	gMca.startDma();
	/* what latched while the clocks came up is not a fault of this session */
	for (unsigned i = 0; i < PDSPK_SPEAKERS; i++)
		gAmp[i].faults(&gLat0[i], &gLat1[i]);
	for (unsigned i = 0; i < PDSPK_SPEAKERS && err == 0; i++)
		err = gAmp[i].power(kPDTasActive);
	gState = kPlaying;
	/* macaudio_vlimit_enable_timeout(): the lease runs from here with what was left of it */
	gLeaseDeadline = uptime_ns() + gLeaseNs;
	update_lock();
	if (err != 0 || amps_check(why, sizeof(why)) != 0) {
		if (err != 0)
			snprintf(why, sizeof(why), "unmute failed (error %d)", err);
		trip(why);
		return;
	}
	gSessions++;
	LOG("playing at %u Hz: volumes %s (0x%02x 0x%02x 0x%02x 0x%02x), gain 0x%02x, tone %llu frames at -%u dB, "
	    "writer %s, daemon %s", gRate, gUnlocked ? "unlocked" : "locked at -100 dB", gVolSeen[0], gVolSeen[1],
	    gVolSeen[2], gVolSeen[3], gGainSeen[0], gToneLeft + gFramesTone, gToneDb, gDevOpen ? "open" : "none",
	    gOwner ? "present" : "absent");
}

static void
stop_playback(const char *why)
{
	uint32_t st[3];
	uint64_t periods = gDone - gDoneAtFirst, ticks = gLastCounter - gFirstCounter;
	uint32_t mhz = gFirstCounter && periods && ticks ?
	    (uint32_t)(periods * PERIOD_FRAMES * ADMAC_COUNTER_HZ * 1000 / ticks) : 0;

	gSense.status(st);
	amps_off();
	stop_streams();
	/* macaudio_vlimit_disable_timeout(): what is left of the lease is kept */
	gLeaseNs = gLeaseDeadline > uptime_ns() ? gLeaseDeadline - uptime_ns() : 0;
	gState = kIdle;
	gLastRateMilliHz = mhz;
	LOG("stopped (%s): %llu periods played (%llu written frames, %llu tone), %llu sense periods, measured %u.%03u Hz, "
	    "late %u, ring errors %u, sense receiver 0x%x/0x%08x sync 0x%x", why, gDone, gFramesData, gFramesTone,
	    gSenseDone, mhz / 1000, mhz % 1000, gLate, gRingErrors, st[0], st[1], st[2]);
	LOG("sense peaks of the session (current/voltage, of 32768): LF %d/%d RF %d/%d LR %d/%d RR %d/%d; die %d %d %d %d mC",
	    gSensePeakSession[0], gSensePeakSession[1], gSensePeakSession[2], gSensePeakSession[3], gSensePeakSession[4],
	    gSensePeakSession[5], gSensePeakSession[6], gSensePeakSession[7], gDieMilli[0], gDieMilli[1], gDieMilli[2],
	    gDieMilli[3]);
}

static void
service_playback(void)
{
	char why[96];
	uint64_t counter = 0;
	unsigned n = gMca.drainReports(&counter);

	if (n > 0) {
		gDone += n;
		gTotalDone += n;
		gLastCounter = counter;
		if (gFirstCounter == 0) {
			gFirstCounter = counter;
			gDoneAtFirst = gDone;
		}
	}
	/* captured periods come back in the order they were queued */
	n = gSense.drainReports(NULL);
	for (unsigned i = 0; i < n; i++) {
		take_sense((uint32_t)(gSenseDone % RING_PERIODS));
		gSenseDone++;
		gTotalSense++;
	}
	while (!gSense.descRingFull()) {
		gSense.submit(gSenseIova + (uint64_t)gSenseNext * PERIOD_BYTES, PERIOD_BYTES);
		gSenseNext = (gSenseNext + 1) % RING_PERIODS;
	}

	lease_tick();
	if (gMca.clearError() || gSense.clearError()) {
		gRingErrors++;
		trip("DMA ring error");
		return;
	}
	if (gDone > 0 && gMca.descRingEmpty()) {
		gLate++;
		trip("the playback ring ran dry");
		return;
	}
	if (gDart->check("speakers") != 0) {
		trip("DART fault");
		return;
	}
	while (!gMca.descRingFull()) {
		fill_period(gNext);
		gMca.submit(gPlayIova + (uint64_t)gNext * PERIOD_BYTES, PERIOD_BYTES);
		gNext = (gNext + 1) % RING_PERIODS;
		gSubmitted++;
	}
	if (++gTicks % CHECK_TICKS == 0 && amps_check(why, sizeof(why)) != 0) {
		trip(why);
		return;
	}
	if (gToneLeft == 0 && !gDevOpen && gFifoCount == 0 && gSilentRun >= TRAILING_SILENCE)
		stop_playback("end of data");
}

static void
pd_speaker_thread(__unused void *arg, __unused wait_result_t wr)
{
	unsigned tick = 0;

	for (;;) {
		bool playing;

		IOLockLock(gLock);
		if (gState == kPlaying)
			service_playback();
		else if (gState == kIdle && (gToneLeft > 0 || gFifoCount > 0))
			start_playback();
		playing = gState == kPlaying;
		IOLockUnlock(gLock);

		/* devfs is not there when the driver starts */
		if ((gNode == NULL || gSafetyNode == NULL) && tick % (playing ? 200 : 10) == 0) {
			if (gNode == NULL && gMajor >= 0)
				gNode = devfs_make_node(makedev(gMajor, 0), DEVFS_CHAR, 0, 0, 0666, "pdspeaker");
			if (gSafetyNode == NULL && gSafetyMajor >= 0)
				gSafetyNode = devfs_make_node(makedev(gSafetyMajor, 0), DEVFS_CHAR, 0, 0, 0600, "pdspeaker-safety");
			if (gNode != NULL && gSafetyNode != NULL)
				LOG("/dev/pdspeaker and /dev/pdspeaker-safety published");
		}
		tick++;
		IOSleep(playing ? 5 : 100);
	}
}

/* ---- /dev/pdspeaker ------------------------------------------------------------------- */

static int
spk_open(__unused dev_t dev, __unused int flags, __unused int devtype, __unused struct proc *p)
{
	int err = 0;

	IOLockLock(gLock);
	if (gState != kIdle && gState != kPlaying)
		err = ENXIO;
	else if (gDevOpen)
		err = EBUSY;
	else
		gDevOpen = true;
	IOLockUnlock(gLock);
	return err;
}

static int
spk_close(__unused dev_t dev, __unused int flags, __unused int devtype, __unused struct proc *p)
{
	IOLockLock(gLock);
	gDevOpen = false;
	IOLockUnlock(gLock);
	return 0;
}

static int
spk_write(__unused dev_t dev, struct uio *uio, __unused int ioflag)
{
	int16_t chunk[2 * 256];
	int err = 0;

	while (err == 0 && uio_resid(uio) > 0) {
		user_ssize_t left = uio_resid(uio);
		unsigned frames = left >= 4 ? (unsigned)(left / 4 > 256 ? 256 : left / 4) : 0;

		if (frames == 0) {
			err = uiomove((char *)chunk, (int)left, uio);   /* a trailing partial frame is dropped */
			break;
		}
		err = uiomove((char *)chunk, (int)(frames * 4), uio);
		if (err != 0)
			break;
		IOLockLock(gLock);
		for (unsigned i = 0; i < frames && err == 0; i++) {
			while (gFifoCount == FIFO_FRAMES && (gState == kIdle || gState == kPlaying)) {
				if (IOLockSleep(gLock, &gFifoEvent, THREAD_ABORTSAFE) != THREAD_AWAKENED) {
					err = EINTR;
					break;
				}
			}
			if (err == 0 && gState != kIdle && gState != kPlaying)
				err = ENXIO;
			if (err != 0)
				break;
			uint32_t at = (gFifoHead + gFifoCount) % FIFO_FRAMES;
			gFifo[2 * at] = (int32_t)chunk[2 * i] * 65536;
			gFifo[2 * at + 1] = (int32_t)chunk[2 * i + 1] * 65536;
			gFifoCount++;
		}
		IOLockUnlock(gLock);
	}
	return err;
}

static struct cdevsw spk_cdevsw = {
	spk_open, spk_close, eno_rdwrt, spk_write,
	eno_ioctl, eno_stop, eno_reset, 0, eno_select, eno_mmap,
	eno_strat, eno_getc, eno_putc, 0
};

/* ---- /dev/pdspeaker-safety ------------------------------------------------------------ */

/* macaudio_slk_lock(): a new owner starts without a lease */
static int
safety_open(__unused dev_t dev, __unused int flags, __unused int devtype, __unused struct proc *p)
{
	int err = 0;

	if (!kauth_cred_issuser(kauth_cred_get()))
		return EPERM;
	IOLockLock(gLock);
	if (gState == kOff || gState == kFailed) {
		err = ENXIO;
	} else if (gOwner) {
		err = EBUSY;
	} else {
		gOwner = true;
		gLeaseNs = 0;
		gLeaseDeadline = 0;
		gWasLocked = false;
		gSenseHead = gSenseCount = 0;
		update_lock();
	}
	IOLockUnlock(gLock);
	return err;
}

/* macaudio_slk_unlock(): the owner is gone, lock at once */
static int
safety_close(__unused dev_t dev, __unused int flags, __unused int devtype, __unused struct proc *p)
{
	IOLockLock(gLock);
	gOwner = false;
	gLeaseNs = 0;
	gLeaseDeadline = 0;
	update_lock();
	IOLockWakeup(gLock, &gSenseEvent, false);
	IOLockUnlock(gLock);
	return 0;
}

static int
safety_ioctl(__unused dev_t dev, u_long cmd, caddr_t data, __unused int fflag, __unused struct proc *p)
{
	int err = 0;

	IOLockLock(gLock);
	switch (cmd) {
	case PDSPK_IOC_INFO: {
		struct pdspk_info *info = (struct pdspk_info *)(void *)data;

		bzero(info, sizeof(*info));
		info->version = 1;
		info->sample_rate = gState == kPlaying ? gRate : 0;
		info->unlocked = gUnlocked;
		info->amp_gain = SPK_AMP_GAIN;
		info->volume_max = gUnlocked ? PDSPK_VOLUME_MAX : PDSPK_VOLUME_LOCKED;
		for (unsigned i = 0; i < PDSPK_SPEAKERS; i++) {
			info->volume[i] = gUnlocked ? gVolume[i] : PDSPK_VOLUME_LOCKED;
			info->die_millicelsius[i] = gDieMilli[i];
		}
		info->tripped = gTrips;
		break;
	}
	case PDSPK_IOC_PING:
		/* macaudio_slk_put() */
		if (*(uint32_t *)(void *)data != PDSPK_MAGIC) {
			err = EINVAL;
		} else if (gWasLocked) {
			gWasLocked = false;
			err = ETIMEDOUT;
		} else {
			gPings++;
			gLeaseNs = (uint64_t)PDSPK_LEASE_MS * 1000000;
			if (gState == kPlaying)
				gLeaseDeadline = uptime_ns() + gLeaseNs;
			update_lock();
		}
		break;
	case PDSPK_IOC_SET_VOLUME: {
		const struct pdspk_volume *v = (const struct pdspk_volume *)(void *)data;
		uint32_t max = gUnlocked ? PDSPK_VOLUME_MAX : PDSPK_VOLUME_LOCKED;

		for (unsigned i = 0; i < PDSPK_SPEAKERS; i++)
			if (v->volume[i] > max)
				err = EINVAL;
		if (err != 0) {
			gVolumeRefused++;
			break;
		}
		gVolumeSets++;
		for (unsigned i = 0; i < PDSPK_SPEAKERS; i++) {
			gVolume[i] = v->volume[i];
			if (gState == kPlaying && gAmp[i].setVolume(SPK_VOLUME_REG(gVolume[i])) != 0) {
				trip("I2C error while setting a volume");
				err = EIO;
				break;
			}
		}
		break;
	}
	default:
		err = ENOTTY;
	}
	IOLockUnlock(gLock);
	return err;
}

/* The sense stream: whole frames, blocking while there are none. */
static int
safety_read(__unused dev_t dev, struct uio *uio, __unused int ioflag)
{
	int16_t chunk[PDSPK_SENSE_CHANNELS * 64];
	int err = 0;
	bool any = false;

	while (err == 0 && uio_resid(uio) >= SENSE_FRAME_BYTES) {
		unsigned want = (unsigned)(uio_resid(uio) / SENSE_FRAME_BYTES), got = 0;

		if (want > 64)
			want = 64;
		IOLockLock(gLock);
		while (gSenseCount == 0 && !any && err == 0) {
			if (IOLockSleep(gLock, &gSenseEvent, THREAD_ABORTSAFE) != THREAD_AWAKENED)
				err = EINTR;
			else if (!gOwner)
				err = ENXIO;
		}
		while (got < want && gSenseCount > 0) {
			memcpy(chunk + PDSPK_SENSE_CHANNELS * got, gSenseFifo + PDSPK_SENSE_CHANNELS * gSenseHead,
			    SENSE_FRAME_BYTES);
			gSenseHead = (gSenseHead + 1) % SENSE_FIFO_FRAMES;
			gSenseCount--;
			got++;
		}
		IOLockUnlock(gLock);
		if (got == 0)
			break;          /* what there was has been returned */
		err = uiomove((char *)chunk, (int)(got * SENSE_FRAME_BYTES), uio);
		any = true;
	}
	return err;
}

static struct cdevsw safety_cdevsw = {
	safety_open, safety_close, safety_read, eno_rdwrt,
	safety_ioctl, eno_stop, eno_reset, 0, eno_select, eno_mmap,
	eno_strat, eno_getc, eno_putc, 0
};

/* ---- sysctl --------------------------------------------------------------------------- */

enum {
	kCtlTone, kCtlToneDb, kCtlRate, kCtlPlaying, kCtlUnlocked, kCtlOwner, kCtlPings, kCtlLeaseTimeouts,
	kCtlVolumeRefused, kCtlVolumeSets, kCtlPeriods, kCtlSensePeriods, kCtlTrips, kCtlSessions, kCtlRateMilliHz,
	kCtlSenseDropped, kCtlTestShutdown,
};

static int
spk_sysctl_int(struct sysctl_oid *oidp, __unused void *arg1, int arg2, struct sysctl_req *req)
{
	int value = 0, error;

	IOLockLock(gLock);
	switch (arg2) {
	case kCtlTone:          value = (int)(gToneLeft / gRate); break;
	case kCtlToneDb:        value = (int)gToneDb; break;
	case kCtlRate:          value = (int)gRate; break;
	case kCtlPlaying:       value = gState == kPlaying; break;
	case kCtlUnlocked:      value = gUnlocked; break;
	case kCtlOwner:         value = gOwner; break;
	case kCtlPings:         value = (int)gPings; break;
	case kCtlLeaseTimeouts: value = (int)gLeaseTimeouts; break;
	case kCtlVolumeRefused: value = (int)gVolumeRefused; break;
	case kCtlVolumeSets:    value = (int)gVolumeSets; break;
	case kCtlPeriods:       value = (int)gTotalDone; break;
	case kCtlSensePeriods:  value = (int)gTotalSense; break;
	case kCtlTrips:         value = (int)gTrips; break;
	case kCtlSessions:      value = (int)gSessions; break;
	case kCtlRateMilliHz:   value = (int)gLastRateMilliHz; break;
	case kCtlSenseDropped:  value = (int)gSenseDropped; break;
	}
	IOLockUnlock(gLock);
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == USER_ADDR_NULL)
		return error;

	IOLockLock(gLock);
	switch (arg2) {
	case kCtlTone:
		if (gState != kIdle && gState != kPlaying)
			error = ENXIO;
		else if (value < 0 || value > 60)
			error = EINVAL;
		else
			gToneLeft = (uint64_t)value * gRate;
		break;
	case kCtlToneDb:
		if (value < MIN_TONE_DB || value > 96)
			error = EINVAL;
		else
			gToneDb = (uint32_t)value;
		break;
	case kCtlTestShutdown:
		/* takes the same path as a fault; it can only turn the speakers off */
		if (gState != kPlaying)
			error = ENXIO;
		else
			trip("shutdown requested through kern.pd_speaker.test_shutdown");
		break;
	case kCtlRate:
		if (value != 44100 && value != 48000)
			error = EINVAL;
		else if (gState == kPlaying || gFifoCount > 0 || gToneLeft > 0)
			error = EBUSY;
		else
			gRate = (uint32_t)value;
		break;
	default:
		error = EPERM;
	}
	IOLockUnlock(gLock);
	return error;
}

static int
spk_sysctl_string(__unused struct sysctl_oid *oidp, __unused void *arg1, int arg2, struct sysctl_req *req)
{
	char text[640];

	text[0] = 0;
	IOLockLock(gLock);
	switch (arg2) {
	case 0:
		snprintf(text, sizeof(text), "%s%s%s rate=%u amps=0x%02x,0x%02x,0x%02x,0x%02x die_mc=%d,%d,%d,%d trips=%u%s%s",
		    kStateNames[gState], gFailure[0] ? ": " : "", gFailure, gRate, gAmpRev[0], gAmpRev[1], gAmpRev[2],
		    gAmpRev[3], gDieMilli[0], gDieMilli[1], gDieMilli[2], gDieMilli[3], gTrips, gTripReason[0] ? " last: " : "",
		    gTripReason);
		break;
	case 1:
		snprintf(text, sizeof(text), "peak LF=%d/%d RF=%d/%d LR=%d/%d RR=%d/%d mean LF=%d/%d RF=%d/%d LR=%d/%d RR=%d/%d",
		    gSensePeak[0], gSensePeak[1], gSensePeak[2], gSensePeak[3], gSensePeak[4], gSensePeak[5], gSensePeak[6],
		    gSensePeak[7], gSenseMean[0], gSenseMean[1], gSenseMean[2], gSenseMean[3], gSenseMean[4], gSenseMean[5],
		    gSenseMean[6], gSenseMean[7]);
		break;
	case 2:
		snprintf(text, sizeof(text), "%s control=%u,%u,%u,%u registers=0x%02x,0x%02x,0x%02x,0x%02x gain=0x%02x,0x%02x,"
		    "0x%02x,0x%02x power=0x%02x,0x%02x,0x%02x,0x%02x latched=0x%02x/0x%02x,0x%02x/0x%02x,0x%02x/0x%02x,0x%02x/0x%02x",
		    gUnlocked ? "unlocked" : "locked", gUnlocked ? gVolume[0] : 1, gUnlocked ? gVolume[1] : 1,
		    gUnlocked ? gVolume[2] : 1, gUnlocked ? gVolume[3] : 1, gVolSeen[0], gVolSeen[1], gVolSeen[2], gVolSeen[3],
		    gGainSeen[0], gGainSeen[1], gGainSeen[2], gGainSeen[3], gPwr[0], gPwr[1], gPwr[2], gPwr[3], gLat0[0],
		    gLat1[0], gLat0[1], gLat1[1], gLat0[2], gLat1[2], gLat0[3], gLat1[3]);
		break;
	}
	IOLockUnlock(gLock);
	return SYSCTL_OUT(req, text, strlen(text) + 1);
}

SYSCTL_DECL(_kern);
SYSCTL_NODE(_kern, OID_AUTO, pd_speaker, CTLFLAG_RD | CTLFLAG_LOCKED, 0, "built-in speakers");

#define SPK_INT(name, id, flags, text) \
	SYSCTL_PROC(_kern_pd_speaker, OID_AUTO, name, CTLTYPE_INT | (flags) | CTLFLAG_LOCKED, 0, id, spk_sysctl_int, "I", text)
#define SPK_STR(name, id, text) \
	SYSCTL_PROC(_kern_pd_speaker, OID_AUTO, name, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED, 0, id, spk_sysctl_string, "A", text)

SPK_STR(state, 0, "state, amplifier revisions and die temperatures, shutdowns");
SPK_STR(sense, 1, "current/voltage sense of the last period, of 32768: largest magnitude and mean");
SPK_STR(volume, 2, "the volume lock and the amplifier registers as last read back");
SPK_INT(tone, kCtlTone, CTLFLAG_RW, "seconds of test tone left; write to play (at the volume in force)");
SPK_INT(tone_db, kCtlToneDb, CTLFLAG_RW, "test tone level, dB below digital full scale (at least 20)");
SPK_INT(rate, kCtlRate, CTLFLAG_RW, "sample rate");
SPK_INT(playing, kCtlPlaying, CTLFLAG_RD, "1 while the speakers play");
SPK_INT(unlocked, kCtlUnlocked, CTLFLAG_RD, "1 while the safety daemon holds the volume lock open");
SPK_INT(owner, kCtlOwner, CTLFLAG_RD, "1 while a safety daemon has the safety device open");
SPK_INT(pings, kCtlPings, CTLFLAG_RD, "accepted keep-alive pings");
SPK_INT(lease_timeouts, kCtlLeaseTimeouts, CTLFLAG_RD, "times the daemon's lease ran out while playing");
SPK_INT(volume_refused, kCtlVolumeRefused, CTLFLAG_RD, "volume requests above what the lock allows");
SPK_INT(volume_sets, kCtlVolumeSets, CTLFLAG_RD, "accepted volume requests");
SPK_INT(periods, kCtlPeriods, CTLFLAG_RD, "DMA periods played since boot");
SPK_INT(sense_periods, kCtlSensePeriods, CTLFLAG_RD, "sense periods captured since boot");
SPK_INT(trips, kCtlTrips, CTLFLAG_RD, "times the amplifiers were shut down for a fault");
SPK_INT(sessions, kCtlSessions, CTLFLAG_RD, "playback sessions started");
SPK_INT(rate_mhz, kCtlRateMilliHz, CTLFLAG_RD, "sample rate of the last session from the DMA timestamps, in mHz");
SPK_INT(test_shutdown, kCtlTestShutdown, CTLFLAG_RW, "write while playing: shut the amplifiers down as a fault would");
SPK_INT(sense_dropped, kCtlSenseDropped, CTLFLAG_RD, "sense frames the daemon did not read in time");

static struct sysctl_oid *const gOids[] = {
	&sysctl__kern_pd_speaker,
	&sysctl__kern_pd_speaker_state, &sysctl__kern_pd_speaker_sense, &sysctl__kern_pd_speaker_volume,
	&sysctl__kern_pd_speaker_tone, &sysctl__kern_pd_speaker_tone_db, &sysctl__kern_pd_speaker_rate,
	&sysctl__kern_pd_speaker_playing, &sysctl__kern_pd_speaker_unlocked, &sysctl__kern_pd_speaker_owner,
	&sysctl__kern_pd_speaker_pings, &sysctl__kern_pd_speaker_lease_timeouts, &sysctl__kern_pd_speaker_volume_refused,
	&sysctl__kern_pd_speaker_volume_sets, &sysctl__kern_pd_speaker_periods, &sysctl__kern_pd_speaker_sense_periods,
	&sysctl__kern_pd_speaker_trips, &sysctl__kern_pd_speaker_sessions, &sysctl__kern_pd_speaker_rate_mhz,
	&sysctl__kern_pd_speaker_sense_dropped, &sysctl__kern_pd_speaker_test_shutdown,
};

/* ---- bring-up ------------------------------------------------------------------------- */

static int
fail(const char *what, int err)
{
	snprintf(gFailure, sizeof(gFailure), "%s (error %d)", what, err);
	LOG("unavailable: %s", gFailure);
	gState = kFailed;
	return err;
}

int
pd_speakers_bring_up(PDGpio *gpio, PDDartFixed *dart, uint8_t *playRing, uint64_t playIova, uint8_t *senseRing,
    uint64_t senseIova)
{
	thread_t thread;
	int err;

	gLock = IOLockAlloc();
	if (gLock == NULL)
		return ENOMEM;
	gGpio = gpio;
	gDart = dart;
	gPlayRing = playRing;
	gPlayIova = playIova;
	gSenseRing = senseRing;
	gSenseIova = senseIova;
	for (unsigned i = 0; i < sizeof(gOids) / sizeof(gOids[0]); i++)
		sysctl_register_oid(gOids[i]);

	LOG("shutdown line (pin %u) 0x%08x before; i2c1 pins 0x%08x 0x%08x, i2c3 pins 0x%08x 0x%08x", PIN_SPEAKER_SDZ,
	    gpio->raw(PIN_SPEAKER_SDZ), gpio->raw(201), gpio->raw(199), gpio->raw(73), gpio->raw(72));
	gpio->output(PIN_SPEAKER_SDZ, 0);
	/* i2c1_pins and i2c3_pins of the Linux device tree */
	gpio->function(201, 1);
	gpio->function(199, 1);
	gpio->function(73, 1);
	gpio->function(72, 1);
	if (!gBus[0].attach("/arm-io/i2c1", 100000) || !gBus[1].attach("/arm-io/i2c3", 100000))
		return fail("I2C controllers i2c1/i2c3", ENXIO);
	for (unsigned i = 0; i < PDSPK_SPEAKERS; i++) {
		gAmp[i].bus = &gBus[kAmps[i].bus];
		gAmp[i].addr = kAmps[i].addr;
		gAmp[i].name = kAmps[i].name;
	}

	/* identify the four amplifiers: out of hardware shutdown, reset, read, back into it */
	IOLockLock(gLock);
	err = amps_on();
	for (unsigned i = 0; i < PDSPK_SPEAKERS && err == 0; i++) {
		uint8_t pwr = 0, gain = 0, vol = 0;
		err = gAmp[i].rd(0x02, &pwr);
		if (err == 0)
			err = gAmp[i].rd(0x03, &gain);
		if (err == 0)
			err = gAmp[i].rd(0x05, &vol);
		gAmp[i].faults(&gLat0[i], &gLat1[i]);
		gAmp[i].dieTemp(&gDieMilli[i]);
		LOG("amplifier %s at i2c%u 0x%02x: revision 0x%02x, power 0x%02x, gain 0x%02x, volume 0x%02x, latched 0x%02x/0x%02x",
		    kAmps[i].name, kAmps[i].bus ? 3 : 1, kAmps[i].addr, gAmp[i].rev, pwr, gain, vol, gLat0[i], gLat1[i]);
		if (err == 0 && ((pwr & 0x03) != kPDTasShutdown || (gain & 0x1f) != SPK_AMP_GAIN ||
		    vol != SPK_VOLUME_REG(PDSPK_VOLUME_LOCKED)))
			err = EIO;
	}
	amps_off();
	IOLockUnlock(gLock);
	if (err != 0)
		return fail("amplifier identification", err);

	err = gMca.attach(SPK_FE_CLUSTER, SPK_PORTS, SPK_SLOTS, PDSPK_SPEAKERS, 1);
	if (err == 0)
		err = gSense.attach(SPK_SENSE_CLUSTER, SPK_PORTS, 0, SPK_SENSE_SLOTS, PDSPK_SENSE_CHANNELS);
	if (err != 0)
		return fail("speaker transport", err);
	gFifo = (int32_t *)IOMalloc(FIFO_FRAMES * 2 * sizeof(int32_t));
	gSenseFifo = (int16_t *)IOMalloc(SENSE_FIFO_FRAMES * SENSE_FRAME_BYTES);
	if (gFifo == NULL || gSenseFifo == NULL)
		return fail("FIFOs", ENOMEM);
	gMajor = cdevsw_add(-1, &spk_cdevsw);
	gSafetyMajor = cdevsw_add(-1, &safety_cdevsw);
	gState = kIdle;
	if (kernel_thread_start(pd_speaker_thread, NULL, &thread) != KERN_SUCCESS)
		return fail("thread", ENOMEM);
	thread_deallocate(thread);
	LOG("ready: amplifiers in hardware shutdown, gain fixed at 0x%02x, volumes locked at -100 dB", SPK_AMP_GAIN);
	return 0;
}

void
pd_speakers_quiesce(void)
{
	if (gLock == NULL)
		return;
	IOLockLock(gLock);
	if (gState == kPlaying)
		stop_playback("shutdown");
	if (gState == kIdle)
		gState = kDisabled;
	if (gGpio != NULL && gAmp[0].bus != NULL)
		amps_off();
	IOLockWakeup(gLock, &gFifoEvent, false);
	IOLockWakeup(gLock, &gSenseEvent, false);
	IOLockUnlock(gLock);
}
