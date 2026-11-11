/* SPDX-License-Identifier: MIT */
/*
 * pdspk-test: exercises the speaker volume lock of PDAppleSound through
 * /dev/pdspeaker-safety the way a safety daemon would, WITHOUT ever asking for a volume
 * above the locked one that it expects to be granted: every request above -100 dB is
 * made while the lock is closed and must be refused. Prints one PDSPK_TEST line per
 * check and ends with PDSPK_TEST_RESULT pass|fail.
 *
 * Sequence (the speakers play the kernel's test tone meanwhile, at -100 dB):
 *   closed lock: info, a volume request above the lock is refused, a wrong magic is refused
 *   ping: the lock opens; the volumes are left where they are; pings every 100 ms keep it open
 *   no ping for 400 ms: the lock closes by itself, the next ping reports ETIMEDOUT once
 *   sense: one second of the current/voltage stream is read and summarised
 *   close: the lock is closed
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include "pd_speaker_safety.h"

static int failures;

static void
check(const char *what, int ok, const char *detail)
{
	printf("PDSPK_TEST %s %s %s\n", ok ? "ok  " : "FAIL", what, detail);
	if (!ok)
		failures++;
}

static int
sysctl_int(const char *name)
{
	int v = -1;
	size_t len = sizeof(v);
	sysctlbyname(name, &v, &len, NULL, 0);
	return v;
}

static int
ping(int fd, uint32_t magic)
{
	return ioctl(fd, PDSPK_IOC_PING, &magic) == 0 ? 0 : errno;
}

int
main(void)
{
	struct pdspk_info info;
	struct pdspk_volume vol;
	char text[200];
	int fd = open(PDSPK_DEVICE, O_RDONLY), err, tone = 6;

	if (fd < 0) {
		printf("PDSPK_TEST FAIL open %s: %s\nPDSPK_TEST_RESULT fail\n", PDSPK_DEVICE, strerror(errno));
		return 1;
	}
	check("second opener refused", open(PDSPK_DEVICE, O_RDONLY) < 0 && errno == EBUSY, "");

	err = ioctl(fd, PDSPK_IOC_INFO, &info) == 0 ? 0 : errno;
	snprintf(text, sizeof(text), "err=%d unlocked=%u max=%u volume=%u,%u,%u,%u gain=0x%02x rate=%u", err, info.unlocked,
	    info.volume_max, info.volume[0], info.volume[1], info.volume[2], info.volume[3], info.amp_gain, info.sample_rate);
	check("closed lock at open", err == 0 && !info.unlocked && info.volume_max == PDSPK_VOLUME_LOCKED &&
	    info.amp_gain == PDSPK_AMP_GAIN_REG, text);

	for (int i = 0; i < PDSPK_SPEAKERS; i++)
		vol.volume[i] = 100;
	err = ioctl(fd, PDSPK_IOC_SET_VOLUME, &vol) == 0 ? 0 : errno;
	snprintf(text, sizeof(text), "errno=%d", err);
	check("volume above the lock refused", err == EINVAL, text);
	err = ping(fd, 0x12345678);
	snprintf(text, sizeof(text), "errno=%d unlocked=%d", err, sysctl_int("kern.pd_speaker.unlocked"));
	check("wrong magic refused", err == EINVAL && sysctl_int("kern.pd_speaker.unlocked") == 0, text);

	/* the speakers play the test tone from here; nothing raises their volume */
	sysctlbyname("kern.pd_speaker.tone", NULL, NULL, &tone, sizeof(tone));
	for (int i = 0; i < 30 && sysctl_int("kern.pd_speaker.playing") != 1; i++)
		usleep(100000);
	check("speakers playing", sysctl_int("kern.pd_speaker.playing") == 1, "");

	err = ping(fd, PDSPK_MAGIC);
	ioctl(fd, PDSPK_IOC_INFO, &info);
	snprintf(text, sizeof(text), "errno=%d unlocked=%u max=%u volume=%u,%u,%u,%u", err, info.unlocked, info.volume_max,
	    info.volume[0], info.volume[1], info.volume[2], info.volume[3]);
	check("ping opens the lock, volumes stay", err == 0 && info.unlocked && info.volume_max == PDSPK_VOLUME_MAX &&
	    info.volume[0] == PDSPK_VOLUME_LOCKED && info.volume[3] == PDSPK_VOLUME_LOCKED, text);
	for (int i = 0; i < PDSPK_SPEAKERS; i++)
		vol.volume[i] = PDSPK_VOLUME_LOCKED;
	err = ioctl(fd, PDSPK_IOC_SET_VOLUME, &vol) == 0 ? 0 : errno;
	snprintf(text, sizeof(text), "errno=%d", err);
	check("volume request at the locked value accepted", err == 0, text);

	err = 0;
	for (int i = 0; i < 10 && err == 0; i++) {
		usleep(100000);
		err = ping(fd, PDSPK_MAGIC);
	}
	snprintf(text, sizeof(text), "errno=%d unlocked=%d timeouts=%d", err, sysctl_int("kern.pd_speaker.unlocked"),
	    sysctl_int("kern.pd_speaker.lease_timeouts"));
	check("pings every 100 ms keep it open", err == 0 && sysctl_int("kern.pd_speaker.unlocked") == 1, text);

	usleep(400000);
	ioctl(fd, PDSPK_IOC_INFO, &info);
	snprintf(text, sizeof(text), "unlocked=%u max=%u timeouts=%d", info.unlocked, info.volume_max,
	    sysctl_int("kern.pd_speaker.lease_timeouts"));
	check("no ping for 400 ms closes the lock", !info.unlocked && info.volume_max == PDSPK_VOLUME_LOCKED, text);
	err = ping(fd, PDSPK_MAGIC);
	snprintf(text, sizeof(text), "errno=%d", err);
	check("next ping reports the lost lease", err == ETIMEDOUT, text);
	err = ping(fd, PDSPK_MAGIC);
	snprintf(text, sizeof(text), "errno=%d unlocked=%d", err, sysctl_int("kern.pd_speaker.unlocked"));
	check("ping after that opens it again", err == 0 && sysctl_int("kern.pd_speaker.unlocked") == 1, text);

	/* one second of sense while pinging */
	{
		static int16_t frames[PDSPK_SENSE_CHANNELS * 4800];
		long got = 0, peak[PDSPK_SENSE_CHANNELS] = { 0 }, sum[PDSPK_SENSE_CHANNELS] = { 0 };

		while (got < 48000) {
			ssize_t n = read(fd, frames, sizeof(frames));
			if (n <= 0)
				break;
			n /= (ssize_t)(PDSPK_SENSE_CHANNELS * sizeof(int16_t));
			for (long i = 0; i < n; i++)
				for (int c = 0; c < PDSPK_SENSE_CHANNELS; c++) {
					long v = frames[PDSPK_SENSE_CHANNELS * i + c];
					sum[c] += v;
					if (v < 0)
						v = -v;
					if (v > peak[c])
						peak[c] = v;
				}
			got += n;
			if (ping(fd, PDSPK_MAGIC) != 0)
				break;
		}
		snprintf(text, sizeof(text), "frames=%ld peak I/V LF=%ld/%ld RF=%ld/%ld LR=%ld/%ld RR=%ld/%ld mean LF=%ld/%ld",
		    got, peak[0], peak[1], peak[2], peak[3], peak[4], peak[5], peak[6], peak[7], got ? sum[0] / got : 0,
		    got ? sum[1] / got : 0);
		check("sense stream read", got >= 48000, text);
	}

	close(fd);
	snprintf(text, sizeof(text), "unlocked=%d owner=%d", sysctl_int("kern.pd_speaker.unlocked"),
	    sysctl_int("kern.pd_speaker.owner"));
	check("close closes the lock", sysctl_int("kern.pd_speaker.unlocked") == 0 && sysctl_int("kern.pd_speaker.owner") == 0, text);
	printf("PDSPK_TEST_RESULT %s\n", failures ? "fail" : "pass");
	return failures ? 1 : 0;
}
