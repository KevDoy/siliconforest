// SPDX-License-Identifier: MIT
/*
 * Voice-coil temperature model of the J293's speakers. See pd_speaker_model.h.
 *
 * The equations are speakersafetyd's (src/types.rs, MIT, Copyright The Asahi Linux
 * Contributors); per sample, with v and i the sensed voltage and current:
 *     p        = v * i
 *     t_coil   += (t_magnet  + p * tr_coil   - t_coil)   * alpha_coil
 *     t_magnet += (t_ambient + p * tr_magnet - t_magnet) * alpha_magnet
 *     alpha    = (1 / rate) / (tau + 1 / rate)
 * and per evaluation
 *     hyst      = min(max(hyst, t), t + t_hysteresis)       for coil and magnet
 *     reduction = (max(hyst_coil, hyst_magnet) - (t_limit - t_window)) / t_window
 *     gain      = min_gain * max(reduction, 0)               in dB
 *     min_gain  = 10 log10(max_power / peak_power)
 *     max_power = (t_limit - t_ambient) / (tr_coil + tr_magnet)
 *     peak_power = 2 * 10^(amp_gain_dBV / 10) / z_nominal
 */
#include "pd_speaker_model.h"

/*
 * conf/apple/j293.conf of speakersafetyd 2.0.1 (MIT), the same for all four speakers,
 * which form one group. Units here: millidegrees, milliseconds, and so on.
 *   [Globals] t_ambient = 50.0, t_hysteresis = 5.0, t_window = 20.0
 *   [Speaker/...] tr_coil = 38.30, tr_magnet = 49.10, tau_coil = 2.80, tau_magnet = 79.70,
 *               t_limit = 130.0, t_headroom = 10.0, z_nominal = 9.70,
 *               is_scale = 3.75, vs_scale = 14
 *   is_chan/vs_chan: left front 0/1, right front 2/3, left rear 4/5, right rear 6/7
 */
#define T_AMBIENT_MC    50000
#define T_HYSTERESIS_MC 5000
#define T_WINDOW_MC     20000
#define T_LIMIT_MC      130000
#define T_HEADROOM_MC   10000
#define TR_COIL_MK_W    38300           /* millikelvin per watt */
#define TR_MAGNET_MK_W  49100
#define TAU_COIL_MS     2800
#define TAU_MAGNET_MS   79700
#define IS_SCALE_MA     3750            /* amperes at full scale, in mA */
#define VS_SCALE_MV     14000
/*
 * min_gain for the amplifier gain 18.5 dBV (register 0x0f) and z_nominal 9.70 ohm:
 * max_power = 80 / 87.4 = 0.91533 W, peak_power = 2 * 10^1.85 / 9.7 = 14.5968 W,
 * 10 log10 of the ratio = -12.0268 dB (the daemon logs "Min gain: -12.03 dB").
 * In hundredths of a dB, magnitude rounded up.
 */
#define AMP_GAIN_REG    0x0f
#define MIN_GAIN_CB     1203

#define Q               40
#define ONE             ((int64_t)1 << Q)
/* millidegrees to Q23.40; everything here stays within 64 bits except the products in mul_up() */
#define DEG(mc)         ((((int64_t)(mc)) << Q) / 1000)
/* milliwatts that the product of two full-scale samples (2^30) stands for */
#define FULL_SCALE_MW   ((int64_t)VS_SCALE_MV * IS_SCALE_MA / 1000)

/* a * b >> Q, rounded towards plus infinity */
static int64_t
mul_up(int64_t a, int64_t b)
{
	__int128 p = (__int128)a * b;
	return (int64_t)((p + (ONE - 1)) >> Q);
}

/* 1 / (tau * rate + 1) in Q0.40; tau * rate is an integer for both rates */
static int64_t
alpha(uint32_t tau_ms, uint32_t rate)
{
	uint64_t n = (uint64_t)tau_ms * rate / 1000 + 1;
	return (int64_t)(((uint64_t)ONE + n / 2) / n);
}

/* (1 - a)^1024 in Q0.40, rounded up at every squaring */
static int64_t
block_factor(int64_t a)
{
	int64_t f = ONE - a;
	for (int i = 0; i < 10; i++)
		f = mul_up(f, f);
	return f;
}

int
pd_spk_model_set_rate(struct pd_spk_model *m, uint32_t rate)
{
	if (rate != 44100 && rate != 48000)
		return -1;
	m->rate = rate;
	m->alpha_coil = alpha(TAU_COIL_MS, rate);
	m->alpha_magnet = alpha(TAU_MAGNET_MS, rate);
	m->block_coil = block_factor(m->alpha_coil);
	m->block_magnet = block_factor(m->alpha_magnet);
	return 0;
}

int
pd_spk_model_init(struct pd_spk_model *m, uint32_t rate, uint32_t amp_gain_reg)
{
	const int64_t amb = DEG(T_AMBIENT_MC + PD_SPK_MODEL_BIAS_MC);
	/* "warm but not warm enough to limit" */
	const int64_t coil = DEG(T_LIMIT_MC - T_WINDOW_MC - 1000 + PD_SPK_MODEL_BIAS_MC);

	if (amp_gain_reg != AMP_GAIN_REG || pd_spk_model_set_rate(m, rate) != 0)
		return -1;
	m->idle_ns = 0;
	m->window_fill = 0;
	m->fault = kPDSpkModelOk;
	m->fault_speaker = 0;
	for (unsigned i = 0; i < PD_SPK_MODEL_SPEAKERS; i++) {
		struct pd_spk_model_speaker *s = &m->spk[i];
		s->t_coil = coil;
		/* t_ambient + (t_coil - t_ambient) * tr_magnet / (tr_magnet + tr_coil), rounded up */
		s->t_magnet = amb + ((coil - amb) * TR_MAGNET_MK_W + (TR_MAGNET_MK_W + TR_COIL_MK_W - 1)) /
		    (TR_MAGNET_MK_W + TR_COIL_MK_W);
		s->t_coil_hyst = 0;
		s->t_magnet_hyst = 0;
		s->power_sum = 0;
		s->power_mw = 0;
	}
	return 0;
}

/* hyst = min(max(hyst, t), t + t_hysteresis) */
static void
hysteresis(int64_t *hyst, int64_t t)
{
	if (*hyst < t)
		*hyst = t;
	if (*hyst > t + DEG(T_HYSTERESIS_MC))
		*hyst = t + DEG(T_HYSTERESIS_MC);
}

int
pd_spk_model_run(struct pd_spk_model *m, const int16_t *sense, unsigned frames)
{
	/*
	 * A product of two samples is at most 2^30 and stands for vs_scale * is_scale
	 * watts at 2^30. Degrees per unit of product, in Q.40 (the J293 values give
	 * 2059008 and 2639616 exactly); a positive product is scaled by one unit more,
	 * so heating is never understated and cooling by negative products never overstated.
	 */
	const int64_t k_coil = (((int64_t)VS_SCALE_MV * IS_SCALE_MA * TR_COIL_MK_W) << (Q - 30)) / 1000000000;
	const int64_t k_magnet = (((int64_t)VS_SCALE_MV * IS_SCALE_MA * TR_MAGNET_MK_W) << (Q - 30)) / 1000000000;
	const int64_t amb = DEG(T_AMBIENT_MC + PD_SPK_MODEL_BIAS_MC);
	const int64_t too_hot = DEG(T_LIMIT_MC + T_HEADROOM_MC);

	if (m->fault != kPDSpkModelOk)
		return m->fault;
	m->idle_ns = 0;
	for (unsigned f = 0; f < frames; f++) {
		for (unsigned i = 0; i < PD_SPK_MODEL_SPEAKERS; i++) {
			struct pd_spk_model_speaker *s = &m->spk[i];
			int64_t p = (int64_t)sense[PD_SPK_MODEL_CHANNELS * f + 2 * i + 1] * sense[PD_SPK_MODEL_CHANNELS * f + 2 * i];
			int64_t coil_target = s->t_magnet + p * (k_coil + (p > 0));
			int64_t magnet_target = amb + p * (k_magnet + (p > 0));

			s->t_coil += mul_up(coil_target - s->t_coil, m->alpha_coil);
			s->t_magnet += mul_up(magnet_target - s->t_magnet, m->alpha_magnet);
			s->power_sum += p;
			if (s->t_coil > too_hot || s->t_magnet > too_hot) {
				m->fault = s->t_coil > too_hot ? kPDSpkModelCoilTooHot : kPDSpkModelMagnetTooHot;
				m->fault_speaker = i;
				return m->fault;
			}
		}
		if (++m->window_fill == PD_SPK_MODEL_WINDOW) {
			m->window_fill = 0;
			for (unsigned i = 0; i < PD_SPK_MODEL_SPEAKERS; i++) {
				struct pd_spk_model_speaker *s = &m->spk[i];
				/* milliwatts = sum / window * full scale / 2^30; the sum is below 2^43, the product below 2^59 */
				int64_t scaled = s->power_sum * FULL_SCALE_MW;

				s->power_mw = (int32_t)(scaled / ((int64_t)PD_SPK_MODEL_WINDOW << 30));
				s->power_sum = 0;
				/* "this really shouldn't happen other than rounding error": below -0.01 W */
				if (scaled < -((int64_t)PD_SPK_MODEL_WINDOW << 30) * 10) {
					m->fault = kPDSpkModelNegativePower;
					m->fault_speaker = i;
					return m->fault;
				}
			}
		}
	}
	for (unsigned i = 0; i < PD_SPK_MODEL_SPEAKERS; i++) {
		hysteresis(&m->spk[i].t_coil_hyst, m->spk[i].t_coil);
		hysteresis(&m->spk[i].t_magnet_hyst, m->spk[i].t_magnet);
	}
	return kPDSpkModelOk;
}

void
pd_spk_model_idle(struct pd_spk_model *m, uint64_t ns)
{
	const int64_t amb = DEG(T_AMBIENT_MC + PD_SPK_MODEL_BIAS_MC);
	/* one block is 1024 sample periods */
	const uint64_t block_ns = 1024ULL * 1000000000ULL / m->rate + 1;

	m->idle_ns += ns;
	/* a pause never leaves a half-filled power window behind */
	m->window_fill = 0;
	for (unsigned i = 0; i < PD_SPK_MODEL_SPEAKERS; i++)
		m->spk[i].power_sum = 0;
	while (m->idle_ns >= block_ns) {
		m->idle_ns -= block_ns;
		for (unsigned i = 0; i < PD_SPK_MODEL_SPEAKERS; i++) {
			struct pd_spk_model_speaker *s = &m->spk[i];
			/* the coil first, towards the magnet as it is before this block's cooling */
			if (s->t_coil > s->t_magnet)
				s->t_coil = s->t_magnet + mul_up(s->t_coil - s->t_magnet, m->block_coil);
			if (s->t_magnet > amb)
				s->t_magnet = amb + mul_up(s->t_magnet - amb, m->block_magnet);
			hysteresis(&s->t_coil_hyst, s->t_coil);
			hysteresis(&s->t_magnet_hyst, s->t_magnet);
		}
	}
}

unsigned
pd_spk_model_limit_steps(const struct pd_spk_model *m)
{
	const int64_t start = DEG(T_LIMIT_MC - T_WINDOW_MC);
	int64_t hottest = 0, over;

	for (unsigned i = 0; i < PD_SPK_MODEL_SPEAKERS; i++) {
		if (m->spk[i].t_coil_hyst > hottest)
			hottest = m->spk[i].t_coil_hyst;
		if (m->spk[i].t_magnet_hyst > hottest)
			hottest = m->spk[i].t_magnet_hyst;
		/* the hysteresis values are 0 until the first evaluation */
		if (m->spk[i].t_coil > hottest)
			hottest = m->spk[i].t_coil;
		if (m->spk[i].t_magnet > hottest)
			hottest = m->spk[i].t_magnet;
	}
	over = hottest - start;
	if (over <= 0)
		return 0;
	/* gain = MIN_GAIN * over / t_window, in half-dB steps (50 hundredths), rounded up */
	{
		int64_t num = over * MIN_GAIN_CB, den = DEG(T_WINDOW_MC) * 50;
		return (unsigned)((num + den - 1) / den);
	}
}

int32_t
pd_spk_model_coil_mc(const struct pd_spk_model *m, unsigned speaker)
{
	return speaker < PD_SPK_MODEL_SPEAKERS ? (int32_t)((m->spk[speaker].t_coil * 1000) >> Q) : 0;
}

int32_t
pd_spk_model_magnet_mc(const struct pd_spk_model *m, unsigned speaker)
{
	return speaker < PD_SPK_MODEL_SPEAKERS ? (int32_t)((m->spk[speaker].t_magnet * 1000) >> Q) : 0;
}
