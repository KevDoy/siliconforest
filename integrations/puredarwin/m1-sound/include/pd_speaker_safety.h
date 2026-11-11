/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/*
 * /dev/pdspeaker-safety: the kernel side of the speaker protection split of the J293
 * (four TAS5770L amplifiers), for the speaker safety daemon and nobody else.
 *
 * It carries what the Linux macaudio machine driver gives speakersafetyd through ALSA:
 *   the "Speaker Volume Unlock" control      PDSPK_IOC_PING
 *   the four "Speaker Playback Volume"s      PDSPK_IOC_SET_VOLUME
 *   "Speaker Sample Rate", amplifier gain    PDSPK_IOC_INFO
 *   the "Speaker Sense" capture PCM          read()
 *
 * Rules, the same as macaudio's (macaudio_vlimit_*, macaudio_slk_*):
 * - One opener at a time; it is the owner of the unlock control and of all four volumes
 *   (the ELEM_LOCKs of Linux). Root only.
 * - Without an owner, and whenever the owner's lease has run out, every volume is held
 *   at control value 1 (-100 dB) and nothing can raise it.
 * - A ping with the magic value renews the lease to PDSPK_LEASE_MS. The lease only runs
 *   down while the speakers play; it is frozen while they do not.
 * - When the lease runs out the volumes go back to 1 and the next ping fails once with
 *   ETIMEDOUT, so the owner knows it lost the lock. Closing the device locks at once.
 * - The amplifier gain is fixed by the kernel (PDSPK_AMP_GAIN_REG) and not settable.
 *
 * Speaker order everywhere: 0 left front, 1 right front, 2 left rear, 3 right rear.
 * read() returns whole frames of PDSPK_SENSE_CHANNELS signed 16-bit little-endian
 * samples at the playback rate: current then voltage of each speaker in that order
 * (the is_chan/vs_chan of speakersafetyd's j293.conf). It blocks while the speakers do
 * not play, and returns EINTR on a signal.
 */
#ifndef _PD_SPEAKER_SAFETY_H
#define _PD_SPEAKER_SAFETY_H

#include <stdint.h>
#include <sys/ioccom.h>

#define PDSPK_DEVICE            "/dev/pdspeaker-safety"
#define PDSPK_MAGIC             0xdec1be15u     /* SPEAKER_MAGIC_VALUE */
#define PDSPK_LEASE_MS          250             /* SPEAKER_LOCK_TIMEOUT */
#define PDSPK_SPEAKERS          4
#define PDSPK_SENSE_CHANNELS    8
#define PDSPK_VOLUME_MAX        201             /* control value of 0 dB; dB = -100.5 + 0.5 * value */
#define PDSPK_VOLUME_LOCKED     1               /* -100 dB */
#define PDSPK_AMP_GAIN_REG      0x0f            /* 18.5 dBV; dBV = 11 + 0.5 * value */

struct pdspk_info {
	uint32_t version;                       /* 1 */
	uint32_t sample_rate;                   /* 0 while the speakers do not play */
	uint32_t unlocked;                      /* 1 while the volumes may be raised */
	uint32_t amp_gain;                      /* register value, see PDSPK_AMP_GAIN_REG */
	uint32_t volume_max;                    /* highest control value accepted now */
	uint32_t volume[PDSPK_SPEAKERS];        /* control values in force */
	int32_t  die_millicelsius[PDSPK_SPEAKERS]; /* amplifier die temperature, INT32_MIN unknown */
	uint32_t tripped;                       /* non-zero: the kernel shut the amplifiers down (fault) */
	uint32_t reserved[4];
};

struct pdspk_volume {
	uint32_t volume[PDSPK_SPEAKERS];        /* control values, each at most volume_max: EINVAL otherwise */
};

#define PDSPK_IOC_INFO          _IOR('S', 1, struct pdspk_info)
#define PDSPK_IOC_PING          _IOW('S', 2, uint32_t)
#define PDSPK_IOC_SET_VOLUME    _IOW('S', 3, struct pdspk_volume)

#endif
