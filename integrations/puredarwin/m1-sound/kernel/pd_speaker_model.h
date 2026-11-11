/* SPDX-License-Identifier: MIT */
/*
 * Voice-coil temperature model of the J293's speakers, in the kernel.
 *
 * The model, its equations and its limits are those of the Asahi Linux speaker safety
 * daemon, speakersafetyd 2.0.1 (https://github.com/AsahiLinux/speakersafetyd, MIT,
 * Copyright The Asahi Linux Contributors): src/types.rs, Speaker::new, run_model and
 * skip_model. The parameters are that project's conf/apple/j293.conf (MIT), carried as
 * data in pd_speaker_model.c. ../safetyd holds a port of the daemon itself, kept as the
 * reference this file is checked against on the host (../model-test).
 *
 * This file is plain C without floating point and without any kernel interface, so the
 * same object runs in the kext and in the host tests. Where the arithmetic cannot be the
 * reference's (single and double precision floats) it errs to the hot side:
 *   - the ambient temperature is PD_SPK_MODEL_BIAS_MC higher,
 *   - products are rounded up,
 *   - while the speakers do not play, the coil cools towards the magnet temperature of
 *     the start of each block instead of the falling one,
 *   - the gain limit is rounded to the next half dB down, and any reduction at all costs
 *     at least one step (the reference ignores reductions under 0.01 dB and rounds to
 *     the nearest step),
 *   - the hysteresis is evaluated at every call, the reference's once per 4096 frames.
 * The caller serialises all calls.
 */
#ifndef _PD_SPEAKER_MODEL_H
#define _PD_SPEAKER_MODEL_H

#include <stdint.h>

#define PD_SPK_MODEL_SPEAKERS   4
#define PD_SPK_MODEL_CHANNELS   8       /* per frame: current, voltage of each speaker */
#define PD_SPK_MODEL_BIAS_MC    50      /* added to the ambient temperature, millidegrees */
#define PD_SPK_MODEL_WINDOW     4096    /* frames over which the mean power is judged, the daemon's period */

/* why run() says the amplifiers must be shut down */
enum {
	kPDSpkModelOk = 0,
	kPDSpkModelCoilTooHot,          /* above t_limit + t_headroom */
	kPDSpkModelMagnetTooHot,
	kPDSpkModelNegativePower,       /* mean power of a window below -0.01 W: the sense data is wrong */
};

struct pd_spk_model_speaker {
	int64_t t_coil, t_magnet;       /* degrees in Q23.40 */
	int64_t t_coil_hyst, t_magnet_hyst;
	int64_t power_sum;              /* sum of voltage * current sample products of the open window */
	int32_t power_mw;               /* mean power of the last complete window, milliwatts */
};

struct pd_spk_model {
	uint32_t rate;
	int64_t  alpha_coil, alpha_magnet;      /* per-sample filter coefficients, Q0.40 */
	int64_t  block_coil, block_magnet;      /* (1 - alpha)^1024, Q0.40, rounded up: idle cooling */
	uint64_t idle_ns;                       /* idle time not yet applied */
	uint32_t window_fill;                   /* frames in the open power window */
	int      fault;                         /* kPDSpkModel..., sticky */
	unsigned fault_speaker;
	struct pd_spk_model_speaker spk[PD_SPK_MODEL_SPEAKERS];
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cold start, as the daemon after a boot: coil one degree under the temperature at
 * which limiting starts. `amp_gain_reg` must be the gain the parameters were derived
 * for (0x0f, 18.5 dBV); anything else, or a rate other than 44100 or 48000, returns -1.
 */
int pd_spk_model_init(struct pd_spk_model *m, uint32_t rate, uint32_t amp_gain_reg);
/* A change of sample rate between sessions; the temperatures are kept. -1 for an unsupported rate. */
int pd_spk_model_set_rate(struct pd_spk_model *m, uint32_t rate);
/*
 * `frames` frames of PD_SPK_MODEL_CHANNELS signed 16-bit samples captured while the
 * speakers played. Returns kPDSpkModelOk or the reason the amplifiers must be shut down
 * (which then stays until the next init).
 */
int pd_spk_model_run(struct pd_spk_model *m, const int16_t *sense, unsigned frames);
/* `ns` nanoseconds passed without playback: the speakers cool. */
void pd_spk_model_idle(struct pd_spk_model *m, uint64_t ns);
/*
 * How far every speaker's volume must be below 0 dB now, in half-dB steps (0: no
 * limit). All speakers are one group, as in j293.conf: the hottest decides.
 */
unsigned pd_spk_model_limit_steps(const struct pd_spk_model *m);
/* Temperatures in millidegrees, for display. */
int32_t pd_spk_model_coil_mc(const struct pd_spk_model *m, unsigned speaker);
int32_t pd_spk_model_magnet_mc(const struct pd_spk_model *m, unsigned speaker);

#ifdef __cplusplus
}
#endif

#endif
