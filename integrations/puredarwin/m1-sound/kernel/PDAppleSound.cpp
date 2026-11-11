// SPDX-License-Identifier: GPL-2.0-only
/*
 * PDAppleSound: headphone-jack playback on the 13" M1 MacBook Pro (J293, T8103) for
 * PureDarwin.
 *
 * The pieces, each in its own file and usable by other drivers:
 *   pd_soc         register mappings by device tree node, PMGR power domains
 *   pd_gpio        Apple GPIO pins
 *   pd_i2c         Apple (PA Semi) I2C controller, polled
 *   pd_dart_fixed  one fixed DMA window behind a T8020 DART stream
 *   pd_mca         NCO clock, MCA cluster and I2S port, ADMAC transmit channel
 *   pd_cs42l83     CS42L83 headphone codec
 * This file owns the order in which they are brought up (that of the Omacom Linux
 * sound/soc/apple/macaudio.c "Headphone Jack" link: I2S, 64 bit clocks per frame, the
 * SoC provides the clocks, front-end cluster 0 routed to the port of cluster 2), the
 * playback ring, a test tone and the user interfaces.
 *
 * Apple device tree nodes used (J293):
 *   /arm-io/i2c2                     I2C controller, clock-gates I2C2
 *   /arm-io/i2c2/audio-codec-output  CS42L83, reg 0x48, reset nub-gpio pin 11,
 *                                    interrupt gpio pin 183 (not used: status is polled)
 *   /arm-io/gpio, /arm-io/nub-gpio   pins; 162/163 are the i2c2 pins (function 1)
 *   /arm-io/nco                      clock synthesiser, one channel per cluster
 *   /arm-io/mca-switch               reg 0: clusters (0x4000 each), reg 1: DMA switch;
 *                                    clock-gates MCA0..MCA5 (parent AUDIO_P)
 *   /arm-io/admac-sio                audio DMA controller, channel 4 * cluster
 *   /arm-io/dart-sio                 its DART; stream 2 (mapper-admac)
 *
 * Not touched: the speaker amplifiers (TAS5770L on i2c1/i2c3, shutdown gpio pin 181)
 * and the I2S ports of clusters 0 and 1 that feed them; microphones.
 *
 * Interfaces:
 *   /dev/pdaudio          write-only, one writer: signed 16-bit little-endian stereo
 *                         frames at kern.pd_sound.rate. Playback starts with the first
 *                         frames and stops when the writer has closed and all of it
 *                         has been played.
 *   kern.pd_sound.*       state, codec and transport registers, counters, and
 *     tone=<seconds>      play the test tone (tone_hz, tone_db below full scale)
 *     volume_db=<0..63>   codec attenuation in dB (63 mutes); default 30
 *     rate=<44100|48000>  sample rate, while nothing plays
 *     preroll_ms=<ms>     silence played while the codec's converter locks (see holding())
 *
 * Boot arguments:
 *   pdsound=0|1|2|3       0: nothing; 1: I2C and codec identification only; 2: also
 *                         DART and transport set up, no playback; 3 (default): all
 *   pdsound_tone=<s>      play the test tone for s seconds once brought up
 *   pdsound_delay=<ms>    wait before touching hardware (default 2000)
 *   pdsound_fe=<0..3>     front-end cluster (default 0, as Linux); pdsound_port=<n>
 *                         the cluster whose I2S pins are driven (default 2, the jack)
 *
 * Locking: one mutex (gLock) covers all driver state and every register sequence.
 * No interrupt is used: one kernel thread polls the DMA rings every few milliseconds
 * while playing, so nothing depends on which processor takes an interrupt. The same
 * thread reads the status registers that the sysctls report, so a reader never holds
 * the mutex across an I2C transfer.
 */
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOService.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <libkern/OSAtomic.h>
#include <miscfs/devfs/devfs.h>
#include <pexpert/pexpert.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include "pd_cs42l83.h"
#include "pd_dart_fixed.h"
#include "pd_gpio.h"
#include "pd_i2c.h"
#include "pd_mca.h"
#include "pd_soc.h"

#define LOG(fmt, ...) IOLog("PDSound: " fmt "\n", ##__VA_ARGS__)

#define T8103_CHIP_ID       0x8103

#define PIN_I2C2_SDA        162     /* /arm-io/gpio, function 1 */
#define PIN_I2C2_SCL        163
#define PIN_SPEAKER_SDZ     181     /* /arm-io/gpio: low keeps the speaker amplifiers shut down */
#define PIN_CODEC_IRQ       183
#define PIN_CODEC_RESET     11      /* /arm-io/nub-gpio */
#define CODEC_ADDR          0x48
#define ADMAC_STREAM        2       /* /arm-io/dart-sio/mapper-admac */

#define PERIOD_FRAMES       1024
#define FRAME_BYTES         8       /* two 32-bit slots */
#define PERIOD_BYTES        (PERIOD_FRAMES * FRAME_BYTES)
#define RING_PERIODS        8       /* at most 4 queued and 1 in flight; the rest are free to fill */
#define RING_BYTES          (RING_PERIODS * PERIOD_BYTES)
#define RING_IOVA           0x04000000ULL
#define FIFO_FRAMES         16384   /* what a writer may queue ahead */
#define TRAILING_SILENCE    6       /* silent periods queued behind the last data before stopping */
#define ADMAC_COUNTER_HZ    24000000ULL

#define DEFAULT_RATE        48000
#define DEFAULT_ATTENUATION 30      /* dB below the codec's full scale */
#define DEFAULT_TONE_HZ     1000
#define DEFAULT_TONE_DB     20      /* dB below digital full scale */
#define MIN_TONE_DB         6

enum { kStateOff, kStateFailed, kStateCodecOnly, kStateIdle, kStatePlaying };
static const char *const kStateNames[] = { "off", "failed", "codec-only", "idle", "playing" };

static IOLock     *gLock;
static PDGpio      gGpio, gNub;
static PDI2C       gI2C;
static PDCs42l83   gCodec;
static PDDartFixed gDart;
static PDMca       gMca;

static uint32_t gLevel = 3;                 /* pdsound= */
static uint32_t gDelayMs = 2000;
static uint32_t gFe = 0, gPort = 2;
static int      gState = kStateOff;
static char     gFailure[96];               /* why the bring-up stopped */

static uint32_t gRate = DEFAULT_RATE;
static uint32_t gAttenuation = DEFAULT_ATTENUATION;
static uint32_t gToneHz = DEFAULT_TONE_HZ;
static uint32_t gToneDb = DEFAULT_TONE_DB;
static uint64_t gToneLeft;                  /* frames of tone still to generate */
static uint32_t gTonePhase;

/* writer -> thread, frames of two int32 */
static int32_t *gFifo;
static uint32_t gFifoHead, gFifoCount;
static bool     gDevOpen;
static int      gFifoEvent;                 /* sleep channel of a writer waiting for room */
static int      gMajor = -1;
static void    *gDevNode;

/* one playback session */
static uint32_t gNext;                      /* ring period to fill next */
static uint32_t gSilentRun;                 /* silent periods queued in a row */
static uint64_t gSubmitted, gDone;          /* periods */
static uint64_t gFirstCounter, gLastCounter, gDoneAtFirst;
static uint32_t gLate;                      /* times the descriptor ring was found empty */
static uint32_t gRingErrors;
static uint64_t gFramesData, gFramesTone;
static uint64_t gUnmutedAt;                 /* uptime in ns */
static uint32_t gPrerollMs = 1500;          /* longest wait for the converter lock before data is played */
static uint64_t gFramesHeld;                /* silent frames played while waiting for it */
static int32_t  gLockMs = -1;               /* unmute to converter lock; -1 while not locked */
/* totals over all sessions */
static uint32_t gSessions;
static uint64_t gTotalDone;
static uint32_t gLastRateMilliHz;           /* measured over the last finished or running session */
static int      gJack = -1;                 /* 1 plugged, 0 not, -1 unknown */
static uint32_t gIdleTicks, gPlayTicks;
/* what the sysctls report: read by the thread, so readers never wait for the I2C bus */
static PDCs42l83Status gCodecSeen;
static PDMcaStatus     gMcaSeen;

/* ---- test tone ------------------------------------------------------------------------ */

/* 10^(-n/20) in Q16 for n = 0..5; each further 6 dB is one shift (6.02 dB, close enough) */
static const uint32_t kGainQ16[6] = { 65536, 58409, 52057, 46396, 41350, 36854 };

/* sin(pi * t / 32768) in Q15 for t in [-32768, 32767]: a parabola and one correction step */
static int32_t
sine_q15(int32_t t)
{
	int32_t a = t < 0 ? -t : t;
	int32_t y = (int32_t)(((int64_t)4 * t * (32768 - a)) >> 15);
	int32_t ay = y < 0 ? -y : y;
	int32_t z = (int32_t)(((int64_t)y * ay) >> 15);

	y += (int32_t)(((int64_t)7373 * (z - y)) >> 15);
	return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

static int32_t
tone_sample(void)
{
	uint32_t db = gToneDb < MIN_TONE_DB ? MIN_TONE_DB : gToneDb;
	uint32_t gain = db / 6 >= 16 ? 0 : kGainQ16[db % 6] >> (db / 6);
	int32_t s = sine_q15((int32_t)(gTonePhase >> 16) - 32768);

	gTonePhase += (uint32_t)(((uint64_t)gToneHz << 32) / gRate);
	return (int32_t)((int64_t)s * gain);    /* Q15 * Q16: full scale is 2^31 */
}

/* ---- playback ------------------------------------------------------------------------- */

static uint64_t
uptime_ns(void)
{
	uint64_t abs = 0, ns = 0;

	clock_get_uptime(&abs);
	absolutetime_to_nanoseconds(abs, &ns);
	return ns;
}

/*
 * The codec's input converter takes about a second to lock to the frames after a start
 * (measured: 1.05 s at 48 kHz, 1.10 s at 44.1 kHz). Until it has, or kern.pd_sound.preroll_ms
 * have passed, silence is played and the data waits.
 */
static bool
holding(void)
{
	if (gLockMs >= 0 || gPrerollMs == 0)
		return false;
	return gUnmutedAt == 0 || uptime_ns() - gUnmutedAt < (uint64_t)gPrerollMs * 1000000;
}

/* Fills one ring period from the writer's FIFO, else the tone, else silence. */
static void
fill_period(uint32_t index)
{
	int32_t *dst = (int32_t *)(void *)(gDart.buf + index * PERIOD_BYTES);
	bool any = false;
	uint32_t taken = 0;

	if (holding()) {
		bzero(dst, PERIOD_BYTES);
		gFramesHeld += PERIOD_FRAMES;
		gSilentRun = 0;
		return;
	}

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
		dst[2 * i] = l;
		dst[2 * i + 1] = r;
	}
	gFramesData += taken;
	gSilentRun = any ? 0 : gSilentRun + 1;
	if (taken > 0)
		IOLockWakeup(gLock, &gFifoEvent, false);
}

static void
queue_periods(void)
{
	while (!gMca.descRingFull()) {
		fill_period(gNext);
		gMca.submit(gDart.iova + (uint64_t)gNext * PERIOD_BYTES, PERIOD_BYTES);
		gNext = (gNext + 1) % RING_PERIODS;
		gSubmitted++;
	}
}

static uint32_t
measured_milli_hz(void)
{
	uint64_t periods = gDone - gDoneAtFirst;
	uint64_t ticks = gLastCounter - gFirstCounter;

	if (gFirstCounter == 0 || periods == 0 || ticks == 0)
		return 0;
	return (uint32_t)(periods * PERIOD_FRAMES * ADMAC_COUNTER_HZ * 1000 / ticks);
}

/* Reads the codec's and the transport's status registers into what the sysctls show. */
static void
refresh_status(void)
{
	int was = gJack;

	gCodec.status(&gCodecSeen);
	if (gCodecSeen.error == 0 && (gCodecSeen.plug == 3 || gCodecSeen.plug == 0))
		gJack = gCodecSeen.plug == 3;
	if (gJack != was)
		LOG("jack: %s (plug status %u)", gJack ? "plugged" : "not plugged", gCodecSeen.plug);
	if (gMca.attached)
		gMca.status(&gMcaSeen);
}

static void
log_codec(const char *when)
{
	refresh_status();
	const PDCs42l83Status &s = gCodecSeen;

	LOG("codec %s: pll_lock=0x%02x asp_rx=0x%02x src=0x%02x codec=0x%02x plug=%u det1=0x%02x osc=0x%02x "
	    "pwr1=0x%02x hp_ctl=0x%02x vol=0x%02x/0x%02x asp_clk=0x%02x mclk_src=0x%02x pll_ctl1=0x%02x i2c_err=%d",
	    when, s.pllLock, s.aspRx, s.src, s.codec, s.plug, s.det1, s.oscSwitch, s.pwr1, s.hpCtl, s.volA, s.volB,
	    s.aspClk, s.mclkSrc, s.pllCtl1, s.error);
}

static void
log_transport(const char *when)
{
	gMca.status(&gMcaSeen);
	const PDMcaStatus &s = gMcaSeen;

	LOG("transport %s: nco=0x%08x/0x%08x mclk=0x%x syncgen=0x%x serdes=0x%x conf=0x%08x port en=0x%x clk=0x%x "
	    "data=0x%x mca_int=0x%08x admac tx=0x%x desc=0x%08x report=0x%08x status=0x%08x residue=0x%x",
	    when, s.ncoCtrl, s.ncoDiv, s.mclkStatus, s.syncgenStatus, s.serdesStatus, s.serdesConf, s.portEnables,
	    s.portClockSel, s.portDataSel, s.mcaIntState, s.admacTxEnabled, s.descRing, s.reportRing, s.chanStatus,
	    s.residue);
}

static void stop_playback(const char *why);

static void
start_playback(void)
{
	int err;

	gNext = 0;
	gSilentRun = 0;
	gSubmitted = gDone = 0;
	gFirstCounter = gLastCounter = gDoneAtFirst = 0;
	gLate = gRingErrors = 0;
	gFramesData = gFramesTone = gFramesHeld = 0;
	gTonePhase = 0;
	gUnmutedAt = 0;
	gLockMs = -1;

	/* clocks to the codec first: its PLL locks to the bit clock */
	err = gMca.startClocks(gRate);
	if (err == 0)
		err = gCodec.configure(gRate);
	if (err == 0)
		err = gCodec.powerUp(gAttenuation);
	if (err != 0) {
		LOG("could not start playback at %u Hz (error %d)", gRate, err);
		gToneLeft = 0;
		gFifoCount = 0;
		IOLockWakeup(gLock, &gFifoEvent, false);
		gCodec.powerDown();
		gMca.stopClocks();
		return;
	}
	gMca.prepareDma();
	queue_periods();
	gMca.startDma();
	gCodec.mute(false);
	gUnmutedAt = uptime_ns();
	gState = kStatePlaying;
	gPlayTicks = 0;
	gSessions++;
	LOG("playing at %u Hz, codec attenuation %u dB, tone %llu frames at %u Hz -%u dB, writer %s", gRate,
	    gAttenuation, gToneLeft + gFramesTone, gToneHz, gToneDb, gDevOpen ? "open" : "none");
	log_transport("at start");
}

static void
service_playback(void)
{
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
		gLastRateMilliHz = measured_milli_hz();
	}
	if (gMca.clearError()) {
		if (gRingErrors++ < 4)
			log_transport("ring error");
	}
	gDart.check("playback");
	/* an empty descriptor ring here means this thread fell several periods behind */
	if (gDone > 0 && gMca.descRingEmpty())
		gLate++;
	queue_periods();
	/*
	 * Until the codec's input converter has locked to the frames (its output is not
	 * valid before), ask every 50 ms: one short I2C transfer.
	 */
	if (gLockMs < 0 && gPlayTicks % 10 == 0 && gCodec.inputLocked()) {
		gLockMs = (int32_t)((uptime_ns() - gUnmutedAt) / 1000000);
		LOG("codec input converter locked %d ms after the unmute", gLockMs);
	}
	/* once a second, after the ring is full again: the registers the sysctls show */
	if (++gPlayTicks % 200 == 0)
		refresh_status();
	if (gToneLeft == 0 && !gDevOpen && gFifoCount == 0 && gSilentRun >= TRAILING_SILENCE)
		stop_playback("end of data");
}

static void
stop_playback(const char *why)
{
	uint32_t mhz = measured_milli_hz();

	log_codec("before stop");
	log_transport("before stop");
	/* the codec leaves the bit clock while it still runs */
	gCodec.powerDown();
	gMca.stopDma();
	gMca.stopClocks();
	gDart.check("stop");
	gState = kStateIdle;
	gIdleTicks = 0;
	LOG("stopped (%s): %llu periods queued, %llu played (%llu frames: %llu written, %llu tone, %llu held silent "
	    "before the lock), measured %u.%03u Hz against %u, converter lock %d ms, late %u, ring errors %u, "
	    "dart faults %u, i2c errors %u", why, gSubmitted, gDone, gDone * PERIOD_FRAMES, gFramesData, gFramesTone,
	    gFramesHeld, mhz / 1000, mhz % 1000, gRate, gLockMs, gLate, gRingErrors, gDart.faults, gI2C.errors);
}

/* ---- bring-up ------------------------------------------------------------------------- */

static void
fail(const char *what, int err)
{
	snprintf(gFailure, sizeof(gFailure), "%s (error %d)", what, err);
	LOG("bring-up stopped: %s", gFailure);
}

static void
bring_up(void)
{
	uint32_t addr = CODEC_ADDR, sid = ADMAC_STREAM;
	int err;

	gState = kStateFailed;
	if (!gGpio.attach("/arm-io/gpio") || !gNub.attach("/arm-io/nub-gpio")) {
		fail("GPIO blocks", ENXIO);
		return;
	}
	LOG("pins before: i2c2 sda(%u)=0x%08x scl(%u)=0x%08x, codec reset(nub %u)=0x%08x irq(%u)=0x%08x, "
	    "speaker shutdown(%u)=0x%08x (not written)", PIN_I2C2_SDA, gGpio.raw(PIN_I2C2_SDA), PIN_I2C2_SCL,
	    gGpio.raw(PIN_I2C2_SCL), PIN_CODEC_RESET, gNub.raw(PIN_CODEC_RESET), PIN_CODEC_IRQ, gGpio.raw(PIN_CODEC_IRQ),
	    PIN_SPEAKER_SDZ, gGpio.raw(PIN_SPEAKER_SDZ));
	/* i2c2_pins of the Linux device tree */
	gGpio.function(PIN_I2C2_SCL, 1);
	gGpio.function(PIN_I2C2_SDA, 1);
	if (!gI2C.attach("/arm-io/i2c2", 100000)) {
		fail("I2C controller i2c2", ENXIO);
		return;
	}
	if (pd_adt_u32("/arm-io/i2c2/audio-codec-output", "reg", 0, &addr))
		addr &= 0x7f;
	err = gCodec.probe(&gI2C, (uint8_t)addr, &gNub, PIN_CODEC_RESET);
	if (err != 0) {
		fail("CS42L83 probe", err);
		return;
	}
	LOG("CS42L83 at i2c2 0x%02x: device ID 0x%05x revision 0x%02x, reset pin now 0x%08x", addr, gCodec.devid,
	    gCodec.revid, gNub.raw(PIN_CODEC_RESET));
	log_codec("after init");
	gState = kStateCodecOnly;
	if (gLevel < 2)
		return;

	pd_adt_u32("/arm-io/dart-sio/mapper-admac", "reg", 0, &sid);
	err = gDart.attach("/arm-io/dart-sio", sid, RING_IOVA, RING_BYTES);
	if (err != 0) {
		fail("dart-sio", err);
		return;
	}
	err = gMca.attach(gFe, gPort);
	if (err != 0) {
		fail("audio transport", err);
		return;
	}
	gFifo = (int32_t *)IOMalloc(FIFO_FRAMES * 2 * sizeof(int32_t));
	if (gFifo == NULL) {
		fail("FIFO", ENOMEM);
		return;
	}
	log_transport("attached");
	if (gLevel < 3) {
		LOG("transport set up; playback is off (pdsound=%u)", gLevel);
		return;
	}
	gState = kStateIdle;
}

/* ---- /dev/pdaudio --------------------------------------------------------------------- */

static int
pd_audio_open(__unused dev_t dev, __unused int flags, __unused int devtype, __unused struct proc *p)
{
	int err = 0;

	IOLockLock(gLock);
	if (gState < kStateIdle)
		err = ENXIO;
	else if (gDevOpen)
		err = EBUSY;
	else
		gDevOpen = true;
	IOLockUnlock(gLock);
	return err;
}

static int
pd_audio_close(__unused dev_t dev, __unused int flags, __unused int devtype, __unused struct proc *p)
{
	IOLockLock(gLock);
	gDevOpen = false;
	IOLockUnlock(gLock);
	return 0;
}

static int
pd_audio_write(__unused dev_t dev, struct uio *uio, __unused int ioflag)
{
	int16_t chunk[2 * 256];
	int err = 0;

	while (err == 0 && uio_resid(uio) > 0) {
		user_ssize_t left = uio_resid(uio);
		unsigned frames = left >= 4 ? (unsigned)(left / 4 > 256 ? 256 : left / 4) : 0;

		if (frames == 0) {
			/* a trailing partial frame is dropped */
			err = uiomove((char *)chunk, (int)left, uio);
			break;
		}
		err = uiomove((char *)chunk, (int)(frames * 4), uio);
		if (err != 0)
			break;
		IOLockLock(gLock);
		for (unsigned i = 0; i < frames && err == 0; i++) {
			while (gFifoCount == FIFO_FRAMES && gState >= kStateIdle) {
				if (IOLockSleep(gLock, &gFifoEvent, THREAD_ABORTSAFE) != THREAD_AWAKENED) {
					err = EINTR;
					break;
				}
			}
			if (err == 0 && gState < kStateIdle)
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

static struct cdevsw pd_audio_cdevsw = {
	pd_audio_open, pd_audio_close, eno_rdwrt, pd_audio_write,
	eno_ioctl, eno_stop, eno_reset, 0, eno_select, eno_mmap,
	eno_strat, eno_getc, eno_putc, 0
};

/* devfs is not there yet when the driver starts: keep trying from the thread. */
static void
publish_node(void)
{
	if (gDevNode != NULL)
		return;
	if (gMajor < 0)
		gMajor = cdevsw_add(-1, &pd_audio_cdevsw);
	if (gMajor >= 0)
		gDevNode = devfs_make_node(makedev(gMajor, 0), DEVFS_CHAR, 0, 0, 0666, "pdaudio");
	if (gDevNode != NULL)
		LOG("/dev/pdaudio published (major %d)", gMajor);
}

/* ---- the thread ----------------------------------------------------------------------- */

static void
pd_sound_thread(__unused void *arg, __unused wait_result_t wr)
{
	uint32_t tone = 0;
	unsigned tick = 0;

	IOSleep(gDelayMs);
	IOLockLock(gLock);
	bring_up();
	if (gState == kStateIdle && PE_parse_boot_argn("pdsound_tone", &tone, sizeof(tone)) && tone > 0 && tone <= 600)
		gToneLeft = (uint64_t)tone * gRate;
	IOLockUnlock(gLock);

	for (;;) {
		bool playing;

		IOLockLock(gLock);
		if (gState == kStatePlaying) {
			service_playback();
		} else if (gState == kStateIdle) {
			if (gToneLeft > 0 || gFifoCount > 0) {
				start_playback();
			} else if (++gIdleTicks % 20 == 0) {
				/* the jack, every two seconds */
				refresh_status();
			}
		} else if (gState == kStateCodecOnly && ++gIdleTicks % 20 == 0) {
			refresh_status();
		}
		playing = gState == kStatePlaying;
		IOLockUnlock(gLock);

		if (gState >= kStateIdle && gDevNode == NULL && tick % (playing ? 200 : 10) == 0)
			publish_node();
		tick++;
		IOSleep(playing ? 5 : 100);
	}
}

/* ---- sysctl --------------------------------------------------------------------------- */

enum {
	kCtlTone, kCtlToneHz, kCtlToneDb, kCtlVolume, kCtlRate, kCtlJack, kCtlPeriods, kCtlFrames, kCtlLate,
	kCtlRingErrors, kCtlDartFaults, kCtlI2CErrors, kCtlRateMilliHz, kCtlSessions, kCtlPlaying, kCtlLockMs,
	kCtlPreroll,
};

static int
pd_sysctl_int(struct sysctl_oid *oidp, __unused void *arg1, int arg2, struct sysctl_req *req)
{
	int value = 0, error;

	IOLockLock(gLock);
	switch (arg2) {
	case kCtlTone:        value = (int)(gToneLeft / gRate); break;
	case kCtlToneHz:      value = (int)gToneHz; break;
	case kCtlToneDb:      value = (int)gToneDb; break;
	case kCtlVolume:      value = (int)gAttenuation; break;
	case kCtlRate:        value = (int)gRate; break;
	case kCtlJack:        value = gJack; break;
	case kCtlPeriods:     value = (int)gTotalDone; break;
	case kCtlFrames:      value = (int)(gTotalDone * PERIOD_FRAMES); break;
	case kCtlLate:        value = (int)gLate; break;
	case kCtlRingErrors:  value = (int)gRingErrors; break;
	case kCtlDartFaults:  value = (int)gDart.faults; break;
	case kCtlI2CErrors:   value = (int)gI2C.errors; break;
	case kCtlRateMilliHz: value = (int)gLastRateMilliHz; break;
	case kCtlSessions:    value = (int)gSessions; break;
	case kCtlPlaying:     value = gState == kStatePlaying; break;
	case kCtlLockMs:      value = gLockMs; break;
	case kCtlPreroll:     value = (int)gPrerollMs; break;
	}
	IOLockUnlock(gLock);

	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == USER_ADDR_NULL)
		return error;

	IOLockLock(gLock);
	switch (arg2) {
	case kCtlTone:
		if (gState < kStateIdle)
			error = ENXIO;
		else if (value < 0 || value > 600)
			error = EINVAL;
		else
			gToneLeft = (uint64_t)value * gRate;
		break;
	case kCtlToneHz:
		if (value < 20 || value > 20000)
			error = EINVAL;
		else
			gToneHz = (uint32_t)value;
		break;
	case kCtlToneDb:
		if (value < MIN_TONE_DB || value > 96)
			error = EINVAL;
		else
			gToneDb = (uint32_t)value;
		break;
	case kCtlVolume:
		if (value < 0 || value > 63) {
			error = EINVAL;
		} else {
			gAttenuation = (uint32_t)value;
			if (gState == kStatePlaying)
				error = gCodec.setVolume(gAttenuation);
		}
		break;
	case kCtlPreroll:
		if (value < 0 || value > 5000)
			error = EINVAL;
		else
			gPrerollMs = (uint32_t)value;
		break;
	case kCtlRate:
		if (value != 44100 && value != 48000)
			error = EINVAL;
		else if (gState == kStatePlaying || gFifoCount > 0 || gToneLeft > 0)
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
pd_sysctl_string(__unused struct sysctl_oid *oidp, __unused void *arg1, int arg2, struct sysctl_req *req)
{
	char text[640];

	text[0] = 0;
	IOLockLock(gLock);
	switch (arg2) {
	case 0:
		snprintf(text, sizeof(text), "%s%s%s codec=0x%05x rev=0x%02x jack=%d rate=%u fe=%u port=%u",
		    kStateNames[gState], gFailure[0] ? ": " : "", gFailure, gCodec.devid, gCodec.revid, gJack, gRate,
		    gFe, gPort);
		break;
	case 1:
		if (gState >= kStateCodecOnly) {
			const PDCs42l83Status &s = gCodecSeen;
			snprintf(text, sizeof(text), "pll_lock=0x%02x asp_rx=0x%02x src=0x%02x codec=0x%02x plug=%u "
			    "det1=0x%02x osc=0x%02x pwr1=0x%02x hp_ctl=0x%02x vol=0x%02x/0x%02x asp_clk=0x%02x "
			    "mclk_src=0x%02x pll_ctl1=0x%02x i2c_err=%d", s.pllLock, s.aspRx, s.src, s.codec, s.plug,
			    s.det1, s.oscSwitch, s.pwr1, s.hpCtl, s.volA, s.volB, s.aspClk, s.mclkSrc, s.pllCtl1, s.error);
		}
		break;
	case 2:
		if (gMca.attached) {
			const PDMcaStatus &s = gMcaSeen;
			snprintf(text, sizeof(text), "nco=0x%08x/0x%08x mclk=0x%x syncgen=0x%x serdes=0x%x conf=0x%08x "
			    "port_en=0x%x port_clk=0x%x port_data=0x%x mca_int=0x%08x admac_tx=0x%x desc=0x%08x "
			    "report=0x%08x status=0x%08x residue=0x%x queued=%llu played=%llu", s.ncoCtrl, s.ncoDiv,
			    s.mclkStatus, s.syncgenStatus, s.serdesStatus, s.serdesConf, s.portEnables, s.portClockSel,
			    s.portDataSel, s.mcaIntState, s.admacTxEnabled, s.descRing, s.reportRing, s.chanStatus,
			    s.residue, gSubmitted, gDone);
		}
		break;
	}
	IOLockUnlock(gLock);
	return SYSCTL_OUT(req, text, strlen(text) + 1);
}

SYSCTL_DECL(_kern);
SYSCTL_NODE(_kern, OID_AUTO, pd_sound, CTLFLAG_RD | CTLFLAG_LOCKED, 0, "headphone jack audio");

#define PD_INT(name, id, flags, text) \
	SYSCTL_PROC(_kern_pd_sound, OID_AUTO, name, CTLTYPE_INT | (flags) | CTLFLAG_LOCKED, 0, id, pd_sysctl_int, "I", text)
#define PD_STR(name, id, text) \
	SYSCTL_PROC(_kern_pd_sound, OID_AUTO, name, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED, 0, id, pd_sysctl_string, "A", text)

PD_STR(state, 0, "driver state, codec identification, jack");
PD_STR(codec, 1, "CS42L83 status registers as last read (every second while playing, every two otherwise)");
PD_STR(transport, 2, "NCO, MCA and ADMAC registers as last read");
PD_INT(tone, kCtlTone, CTLFLAG_RW, "seconds of test tone left; write to play");
PD_INT(tone_hz, kCtlToneHz, CTLFLAG_RW, "test tone frequency");
PD_INT(tone_db, kCtlToneDb, CTLFLAG_RW, "test tone level, dB below full scale");
PD_INT(volume_db, kCtlVolume, CTLFLAG_RW, "codec attenuation in dB, 63 mutes");
PD_INT(rate, kCtlRate, CTLFLAG_RW, "sample rate");
PD_INT(jack, kCtlJack, CTLFLAG_RD, "1 plugged, 0 not, -1 unknown");
PD_INT(periods, kCtlPeriods, CTLFLAG_RD, "DMA periods played since boot");
PD_INT(frames, kCtlFrames, CTLFLAG_RD, "frames played since boot");
PD_INT(late, kCtlLate, CTLFLAG_RD, "times the descriptor ring ran empty in the last session");
PD_INT(ring_errors, kCtlRingErrors, CTLFLAG_RD, "DMA ring errors in the last session");
PD_INT(dart_faults, kCtlDartFaults, CTLFLAG_RD, "DART faults since boot");
PD_INT(i2c_errors, kCtlI2CErrors, CTLFLAG_RD, "failed I2C transfers since boot");
PD_INT(rate_mhz, kCtlRateMilliHz, CTLFLAG_RD, "sample rate measured from the DMA timestamps, in mHz");
PD_INT(sessions, kCtlSessions, CTLFLAG_RD, "playback sessions started");
PD_INT(playing, kCtlPlaying, CTLFLAG_RD, "1 while a session runs");
PD_INT(preroll_ms, kCtlPreroll, CTLFLAG_RW, "longest silent wait for the codec's converter lock before data plays; 0: none");
PD_INT(lock_ms, kCtlLockMs, CTLFLAG_RD, "last session: ms from unmute to the codec's converter lock, -1 if it never locked");

static struct sysctl_oid *const gOids[] = {
	&sysctl__kern_pd_sound,
	&sysctl__kern_pd_sound_state, &sysctl__kern_pd_sound_codec, &sysctl__kern_pd_sound_transport,
	&sysctl__kern_pd_sound_tone, &sysctl__kern_pd_sound_tone_hz, &sysctl__kern_pd_sound_tone_db,
	&sysctl__kern_pd_sound_volume_db, &sysctl__kern_pd_sound_rate, &sysctl__kern_pd_sound_jack,
	&sysctl__kern_pd_sound_periods, &sysctl__kern_pd_sound_frames, &sysctl__kern_pd_sound_late,
	&sysctl__kern_pd_sound_ring_errors, &sysctl__kern_pd_sound_dart_faults, &sysctl__kern_pd_sound_i2c_errors,
	&sysctl__kern_pd_sound_rate_mhz, &sysctl__kern_pd_sound_sessions, &sysctl__kern_pd_sound_playing,
	&sysctl__kern_pd_sound_lock_ms, &sysctl__kern_pd_sound_preroll_ms,
};

/* ---- IOKit ---------------------------------------------------------------------------- */

class PDAppleSound : public IOService {
	OSDeclareDefaultStructors(PDAppleSound);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void systemWillShutdown(IOOptionBits specifier) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(PDAppleSound, IOService);

bool
PDAppleSound::start(IOService *provider)
{
	IORegistryEntry *chosen;
	OSData *chip;
	uint32_t chipID = 0, value = 0;
	thread_t thread;

	if (!IOService::start(provider))
		return false;

	chosen = IORegistryEntry::fromPath("/chosen", gIODTPlane);
	chip = chosen ? OSDynamicCast(OSData, chosen->getProperty("chip-id")) : NULL;
	if (chip != NULL && chip->getLength() >= sizeof(uint32_t))
		chipID = *(const uint32_t *)chip->getBytesNoCopy();
	OSSafeReleaseNULL(chosen);
	if (chipID != T8103_CHIP_ID) {
		LOG("chip 0x%x is not T8103; nothing is done", chipID);
		return false;
	}

	PE_parse_boot_argn("pdsound", &gLevel, sizeof(gLevel));
	if (PE_parse_boot_argn("pdsound_delay", &value, sizeof(value)) && value <= 60000)
		gDelayMs = value;
	if (PE_parse_boot_argn("pdsound_fe", &value, sizeof(value)) && value <= 3)
		gFe = value;
	if (PE_parse_boot_argn("pdsound_port", &value, sizeof(value)) && value <= 5)
		gPort = value;
	if (gLevel == 0) {
		LOG("off (pdsound=0)");
		return false;
	}

	gLock = IOLockAlloc();
	if (gLock == NULL)
		return false;
	for (unsigned i = 0; i < sizeof(gOids) / sizeof(gOids[0]); i++)
		sysctl_register_oid(gOids[i]);
	if (kernel_thread_start(pd_sound_thread, NULL, &thread) != KERN_SUCCESS)
		return false;
	thread_deallocate(thread);
	registerService();
	return true;
}

/* Mute and power the codec down before the system goes, so nothing keeps sounding. */
void
PDAppleSound::systemWillShutdown(IOOptionBits specifier)
{
	if (gLock != NULL) {
		IOLockLock(gLock);
		if (gState == kStatePlaying) {
			gToneLeft = 0;
			gFifoCount = 0;
			stop_playback("shutdown");
		}
		if (gState >= kStateIdle)
			gState = kStateCodecOnly;
		IOLockWakeup(gLock, &gFifoEvent, false);
		IOLockUnlock(gLock);
	}
	IOService::systemWillShutdown(specifier);
}
