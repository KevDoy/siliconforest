// SPDX-License-Identifier: GPL-2.0-only
/*
 * CS42L83 headphone codec, playback path.
 *
 * Ported from the Omacom Linux tree, sound/soc/codecs/cs42l42.c and cs42l83-i2c.c
 * (cs42l42_common_probe, cs42l42_init, cs42l42_setup_hs_type_detect,
 * cs42l42_set_dai_fmt, cs42l42_pcm_hw_params, cs42l42_pll_config, cs42l42_asp_config,
 * cs42l42_src_config, cs42l42_mute_stream, the DAPM playback widgets) and
 * include/sound/cs42l42.h:
 *   Copyright 2016-2022 Cirrus Logic, Inc.
 *   Authors: James Schulman, Brian Austin, Michael White (Cirrus Logic)
 *   cs42l83-i2c.c: Martin Povišer
 */
#include "pd_cs42l83.h"

#include <IOKit/IOLib.h>
#include <sys/errno.h>

#define LOG(fmt, ...) IOLog("PDCs42l83: " fmt "\n", ##__VA_ARGS__)

#define CS42L42_PAGE_REGISTER           0x00
#define CS42L83_CHIP_ID                 0x42A83

#define CS42L42_DEVID_AB                0x1001
#define CS42L42_DEVID_CD                0x1002
#define CS42L42_DEVID_E                 0x1003
#define CS42L42_REVID                   0x1005
#define CS42L42_MCLK_CTL                0x1009
#define CS42L42_INTERNAL_FS_MASK        (1 << 1)
#define CS42L42_SFTRAMP_RATE            0x100A
#define CS42L42_PWR_CTL1                0x1101
#define CS42L42_ASP_DAO_PDN_MASK        (1 << 7)
#define CS42L42_ASP_DAI_PDN_MASK        (1 << 6)
#define CS42L42_MIXER_PDN_MASK          (1 << 5)
#define CS42L42_EQ_PDN_MASK             (1 << 4)
#define CS42L42_HP_PDN_MASK             (1 << 3)
#define CS42L42_ADC_PDN_MASK            (1 << 2)
#define CS42L42_PDN_ALL_MASK            (1 << 0)
#define CS42L42_OSC_SWITCH              0x1107
#define CS42L42_SCLK_PRESENT_MASK       (1 << 0)
#define CS42L42_OSC_SWITCH_STATUS       0x1109
#define CS42L42_TSENSE_CTL              0x1113
#define CS42L42_TS_RISE_DBNCE_TIME_MASK (7 << 0)
#define CS42L42_TS_FALL_DBNCE_TIME_MASK (7 << 3)
#define CS42L42_TS_INV_MASK             (1 << 7)
#define CS42L42_TS_DBNCE_1000           5
#define CS42L42_HS_CLAMP_DISABLE        0x1129
#define CS42L42_MCLK_SRC_SEL            0x1201
#define CS42L42_MCLK_SRC_SEL_MASK       (1 << 0)
#define CS42L42_FSYNC_PW_LOWER          0x1203
#define CS42L42_FSYNC_PW_UPPER          0x1204
#define CS42L42_FSYNC_P_LOWER           0x1205
#define CS42L42_FSYNC_P_UPPER           0x1206
#define CS42L42_ASP_CLK_CFG             0x1207
#define CS42L42_ASP_SCLK_EN_MASK        (1 << 5)
#define CS42L42_ASP_MODE_MASK           (1 << 4)
#define CS42L42_ASP_SCPOL_MASK          (3 << 2)
#define CS42L42_ASP_LCPOL_MASK          (3 << 0)
#define CS42L42_ASP_LCPOL_INV           3
#define CS42L42_ASP_FRM_CFG             0x1208
#define CS42L42_ASP_STP_MASK            (1 << 4)
#define CS42L42_ASP_5050_MASK           (1 << 3)
#define CS42L42_ASP_FSD_MASK            (7 << 0)
#define CS42L42_ASP_FSD_1_0             2
#define CS42L42_FS_RATE_EN              0x1209
#define CS42L42_FS_EN_MASK              0x0f
#define CS42L42_FS_EN_IASRC_96K         0x1
#define CS42L42_FS_EN_OASRC_96K         0x2
#define CS42L42_IN_ASRC_CLK             0x120A
#define CS42L42_OUT_ASRC_CLK            0x120B
#define CS42L42_PLL_DIV_CFG1            0x120C
#define CS42L42_SCLK_PREDIV_MASK        (3 << 0)
#define CS42L42_SRC_STATUS              0x1303
#define CS42L42_SRC_ILK_MASK            (1 << 0)
#define CS42L42_ASP_RX_STATUS           0x1304
#define CS42L42_CODEC_STATUS            0x1308
#define CS42L42_PLL_LOCK_STATUS         0x130E
#define CS42L42_TSRS_PLUG_STATUS        0x130F
#define CS42L42_TS_PLUG_SHIFT           2
#define CS42L42_PLL_CTL1                0x1501
#define CS42L42_PLL_START_MASK          (1 << 0)
#define CS42L42_PLL_DIV_FRAC0           0x1502
#define CS42L42_PLL_DIV_FRAC1           0x1503
#define CS42L42_PLL_DIV_FRAC2           0x1504
#define CS42L42_PLL_DIV_INT             0x1505
#define CS42L42_PLL_CTL3                0x1508
#define CS42L42_PLL_CAL_RATIO           0x150A
#define CS42L42_PLL_CTL4                0x151B
#define CS42L42_PLL_MODE_MASK           (3 << 0)
#define CS42L42_TIPSENSE_CTL            0x1B73
#define CS42L42_TIP_SENSE_DEBOUNCE_MASK (3 << 0)
#define CS42L42_TIP_SENSE_INV_MASK      (1 << 5)
#define CS42L42_TIP_SENSE_CTRL_MASK     (3 << 6)
#define CS42L42_MISC_DET_CTL            0x1B74
#define CS42L42_DETECT_MODE_MASK        (3 << 3)
#define CS42L42_MIC_DET_CTL1            0x1B75
#define CS42L42_HS_DET_LEVEL_MASK       0x3f
#define CS42L42_EVENT_STAT_SEL_MASK     (1 << 6)
#define CS42L42_LATCH_TO_VP_MASK        (1 << 7)
#define CS42L42_HS_DET_LEVEL_15         0x0F
#define CS42L42_DET_STATUS1             0x1B77
#define CS42L42_HS_BIAS_CTL             0x1C03
#define CS42L42_HSBIAS_RAMP_MASK        (3 << 0)
#define CS42L42_HSBIAS_RAMP_SLOW        2
#define CS42L42_HP_CTL                  0x2001
#define CS42L42_HP_ANA_BMUTE_MASK       (1 << 3)
#define CS42L42_HP_ANA_AMUTE_MASK       (1 << 2)
#define CS42L42_MIXER_CHA_VOL           0x2301
#define CS42L42_MIXER_CHB_VOL           0x2303
#define CS42L42_MIXER_CH_VOL_MASK       0x3f
#define CS42L42_SP_RX_CH_SEL            0x2501
#define CS42L42_SP_RX_CHB_SEL_MASK      (3 << 2)
#define CS42L42_ASP_RX_DAI0_EN          0x2A01
#define CS42L42_ASP_RX0_CH_EN_MASK      (0xf << 2)
#define CS42L42_ASP_RX_DAI0_CH1_AP_RES  0x2A02
#define CS42L42_ASP_RX_DAI0_CH1_BIT_MSB 0x2A03
#define CS42L42_ASP_RX_DAI0_CH1_BIT_LSB 0x2A04
#define CS42L42_ASP_RX_DAI0_CH2_AP_RES  0x2A05
#define CS42L42_ASP_RX_DAI0_CH2_BIT_MSB 0x2A06
#define CS42L42_ASP_RX_DAI0_CH2_BIT_LSB 0x2A07
#define CS42L42_ASP_RX_CH_AP_MASK       (1 << 6)
#define CS42L42_ASP_RX_CH_RES_MASK      (3 << 0)
#define CS42L42_ASP_RX_CH_RES_32        3

#define CS42L42_BOOT_TIME_US            3000
#define CS42L42_PLL_DIVOUT_TIME_US      800
#define CS42L42_CLOCK_SWITCH_DELAY_US   150
#define CS42L42_HP_ADC_EN_TIME_US       20000

/* The two rows of pll_ratio_table (datasheet table 4-5) for 64 bit clocks per frame. */
static const struct {
	uint32_t sclk;
	uint8_t  sclk_prediv, pll_div_int;
	uint32_t pll_div_frac;
	uint8_t  pll_mode, pll_divout;
	uint32_t mclk_int;
	uint8_t  pll_cal_ratio, n;
} kPll[] = {
	{ 2822400, 0x00, 0x40, 0x000000, 0x03, 0x10, 11289600, 128, 1 },   /* 44.1 kHz */
	{ 3072000, 0x00, 0x3E, 0x800000, 0x03, 0x10, 12000000, 125, 1 },   /* 48 kHz */
};

int
PDCs42l83::read(uint16_t reg, uint8_t *value)
{
	int err = 0;
	if (page != (reg >> 8)) {
		err = bus->writeReg(addr, CS42L42_PAGE_REGISTER, (uint8_t)(reg >> 8));
		page = err ? -1 : (reg >> 8);
	}
	if (err == 0)
		err = bus->readReg(addr, (uint8_t)reg, value);
	return err;
}

int
PDCs42l83::write(uint16_t reg, uint8_t value)
{
	int err = 0;
	if (page != (reg >> 8)) {
		err = bus->writeReg(addr, CS42L42_PAGE_REGISTER, (uint8_t)(reg >> 8));
		page = err ? -1 : (reg >> 8);
	}
	if (err == 0)
		err = bus->writeReg(addr, (uint8_t)reg, value);
	return err;
}

int
PDCs42l83::update(uint16_t reg, uint8_t mask, uint8_t value)
{
	uint8_t old = 0;
	int err = read(reg, &old);
	uint8_t now = (uint8_t)((old & ~mask) | (value & mask));
	if (err == 0 && now != old)
		err = write(reg, now);
	return err;
}

int
PDCs42l83::probe(PDI2C *i2c, uint8_t address, PDGpio *resetGpio, unsigned resetPin)
{
	uint8_t ab = 0, cd = 0, e = 0;
	int err;

	bus = i2c;
	addr = address;
	page = -1;
	ready = false;
	onSclk = false;

	/* cs42l42_common_probe(): reset low for the minimum pulse, then the boot time */
	resetGpio->output(resetPin, 0);
	IOSleep(1);
	resetGpio->output(resetPin, 1);
	IOSleep(2 * CS42L42_BOOT_TIME_US / 1000);

	/* cirrus_read_device_id() */
	err = read(CS42L42_DEVID_AB, &ab);
	if (err == 0)
		err = read(CS42L42_DEVID_CD, &cd);
	if (err == 0)
		err = read(CS42L42_DEVID_E, &e);
	if (err != 0) {
		LOG("no answer at 0x%02x (error %d)", addr, err);
		return err;
	}
	devid = ((uint32_t)ab << 12) | ((uint32_t)cd << 4) | (e >> 4);
	err = read(CS42L42_REVID, &revid);
	if (err != 0)
		return err;
	if (devid != CS42L83_CHIP_ID) {
		LOG("device ID 0x%05x, expected 0x%05x", devid, CS42L83_CHIP_ID);
		return ENODEV;
	}

	/* cs42l42_init(): leave global power-down, every block still off */
	err = update(CS42L42_PWR_CTL1,
	    CS42L42_ASP_DAO_PDN_MASK | CS42L42_ASP_DAI_PDN_MASK | CS42L42_MIXER_PDN_MASK | CS42L42_EQ_PDN_MASK |
	    CS42L42_HP_PDN_MASK | CS42L42_ADC_PDN_MASK | CS42L42_PDN_ALL_MASK,
	    CS42L42_ASP_DAO_PDN_MASK | CS42L42_ASP_DAI_PDN_MASK | CS42L42_MIXER_PDN_MASK | CS42L42_EQ_PDN_MASK |
	    CS42L42_HP_PDN_MASK | CS42L42_ADC_PDN_MASK);
	/* a faster digital ramp, so streams do not fade in audibly */
	if (err == 0)
		err = write(CS42L42_SFTRAMP_RATE, (10 << 4) | 1);
	/* cs42l42_handle_device_data() defaults: tip sense debounce 1 s up, none down; slow bias ramp */
	if (err == 0)
		err = update(CS42L42_TSENSE_CTL, CS42L42_TS_RISE_DBNCE_TIME_MASK | CS42L42_TS_FALL_DBNCE_TIME_MASK,
		    CS42L42_TS_DBNCE_1000);
	if (err == 0)
		err = update(CS42L42_HS_BIAS_CTL, CS42L42_HSBIAS_RAMP_MASK, CS42L42_HSBIAS_RAMP_SLOW);
	/* cs42l42_setup_hs_type_detect() */
	if (err == 0)
		err = update(CS42L42_MISC_DET_CTL, CS42L42_DETECT_MODE_MASK, 0);
	if (err == 0)
		err = update(CS42L42_MIC_DET_CTL1,
		    CS42L42_LATCH_TO_VP_MASK | CS42L42_EVENT_STAT_SEL_MASK | CS42L42_HS_DET_LEVEL_MASK,
		    CS42L42_LATCH_TO_VP_MASK | CS42L42_HS_DET_LEVEL_15);
	if (err == 0)
		err = update(CS42L42_HS_CLAMP_DISABLE, 1, 1);
	/* tip sense on; the J293 jack switch is inverted (cirrus,ts-inv = 1) */
	if (err == 0)
		err = update(CS42L42_TSENSE_CTL, CS42L42_TS_INV_MASK, CS42L42_TS_INV_MASK);
	if (err == 0)
		err = update(CS42L42_TIPSENSE_CTL,
		    CS42L42_TIP_SENSE_CTRL_MASK | CS42L42_TIP_SENSE_INV_MASK | CS42L42_TIP_SENSE_DEBOUNCE_MASK,
		    (3 << 6) | (0 << 5) | 2);
	/* the interrupt masks stay at their reset values (all masked): status is polled */
	if (err != 0) {
		LOG("initialisation failed (error %d)", err);
		return err;
	}
	ready = true;
	return 0;
}

int
PDCs42l83::configure(uint32_t rate)
{
	unsigned i = rate == 44100 ? 0 : 1;
	const uint32_t fsync = 64;
	int err;

	if (!ready)
		return ENXIO;
	if (onSclk)
		return EBUSY;
	if (rate != 44100 && rate != 48000)
		return EINVAL;
	pll = i;

	/* cs42l42_set_dai_fmt(): clock consumer, I2S, inverted bit clock and frame */
	err = update(CS42L42_ASP_FRM_CFG, CS42L42_ASP_STP_MASK | CS42L42_ASP_5050_MASK | CS42L42_ASP_FSD_MASK,
	    CS42L42_ASP_5050_MASK | CS42L42_ASP_FSD_1_0);
	if (err == 0)
		err = update(CS42L42_ASP_CLK_CFG, CS42L42_ASP_MODE_MASK | CS42L42_ASP_SCPOL_MASK | CS42L42_ASP_LCPOL_MASK,
		    CS42L42_ASP_LCPOL_INV);

	/* cs42l42_pcm_hw_params(), playback: 32-bit samples, channel 1 on low LRCLK, 2 on high */
	if (err == 0)
		err = update(CS42L42_ASP_RX_DAI0_CH1_AP_RES, CS42L42_ASP_RX_CH_AP_MASK | CS42L42_ASP_RX_CH_RES_MASK,
		    CS42L42_ASP_RX_CH_RES_32);
	if (err == 0)
		err = update(CS42L42_ASP_RX_DAI0_CH2_AP_RES, CS42L42_ASP_RX_CH_AP_MASK | CS42L42_ASP_RX_CH_RES_MASK,
		    CS42L42_ASP_RX_CH_AP_MASK | CS42L42_ASP_RX_CH_RES_32);
	if (err == 0)
		err = write(CS42L42_ASP_RX_DAI0_CH1_BIT_MSB, 0);
	if (err == 0)
		err = write(CS42L42_ASP_RX_DAI0_CH1_BIT_LSB, 0);
	if (err == 0)
		err = write(CS42L42_ASP_RX_DAI0_CH2_BIT_MSB, 0);
	if (err == 0)
		err = write(CS42L42_ASP_RX_DAI0_CH2_BIT_LSB, 0);
	/* channel B from the second channel; both LRCLK slots enabled */
	if (err == 0)
		err = update(CS42L42_SP_RX_CH_SEL, CS42L42_SP_RX_CHB_SEL_MASK, 1 << 2);
	if (err == 0)
		err = update(CS42L42_ASP_RX_DAI0_EN, CS42L42_ASP_RX0_CH_EN_MASK, (1 << 2) | (1 << 3));

	/* cs42l42_pll_config() */
	if (err == 0)
		err = update(CS42L42_MCLK_CTL, CS42L42_INTERNAL_FS_MASK,
		    kPll[i].mclk_int != 12000000 ? CS42L42_INTERNAL_FS_MASK : 0);
	if (err == 0)
		err = update(CS42L42_PLL_DIV_CFG1, CS42L42_SCLK_PREDIV_MASK, kPll[i].sclk_prediv);
	if (err == 0)
		err = write(CS42L42_PLL_DIV_INT, kPll[i].pll_div_int);
	if (err == 0)
		err = write(CS42L42_PLL_DIV_FRAC0, (uint8_t)kPll[i].pll_div_frac);
	if (err == 0)
		err = write(CS42L42_PLL_DIV_FRAC1, (uint8_t)(kPll[i].pll_div_frac >> 8));
	if (err == 0)
		err = write(CS42L42_PLL_DIV_FRAC2, (uint8_t)(kPll[i].pll_div_frac >> 16));
	if (err == 0)
		err = update(CS42L42_PLL_CTL4, CS42L42_PLL_MODE_MASK, kPll[i].pll_mode);
	if (err == 0)
		err = write(CS42L42_PLL_CTL3, (uint8_t)(kPll[i].pll_divout * kPll[i].n));
	if (err == 0)
		err = write(CS42L42_PLL_CAL_RATIO, kPll[i].pll_cal_ratio);

	/* cs42l42_asp_config(): LRCLK period and 50 % duty cycle */
	if (err == 0)
		err = write(CS42L42_FSYNC_P_LOWER, (uint8_t)(fsync - 1));
	if (err == 0)
		err = write(CS42L42_FSYNC_P_UPPER, 0);
	if (err == 0)
		err = write(CS42L42_FSYNC_PW_LOWER, (uint8_t)(fsync / 2 - 1));
	if (err == 0)
		err = write(CS42L42_FSYNC_PW_UPPER, 0);

	/* cs42l42_src_config(): rates of 96 kHz or lower, converter clock for up to 48 kHz */
	if (err == 0)
		err = update(CS42L42_FS_RATE_EN, CS42L42_FS_EN_MASK, CS42L42_FS_EN_IASRC_96K | CS42L42_FS_EN_OASRC_96K);
	if (err == 0)
		err = update(CS42L42_IN_ASRC_CLK, 1, 0);
	if (err == 0)
		err = update(CS42L42_OUT_ASRC_CLK, 1, 0);
	if (err != 0)
		LOG("configuration for %u Hz failed (error %d)", rate, err);
	return err;
}

int
PDCs42l83::setVolume(unsigned attenuation)
{
	uint8_t v = attenuation > 0x3f ? 0x3f : (uint8_t)attenuation;
	int err;

	if (!ready)
		return ENXIO;
	err = update(CS42L42_MIXER_CHA_VOL, CS42L42_MIXER_CH_VOL_MASK, v);
	if (err == 0)
		err = update(CS42L42_MIXER_CHB_VOL, CS42L42_MIXER_CH_VOL_MASK, v);
	return err;
}

int
PDCs42l83::mute(bool on)
{
	if (!ready)
		return ENXIO;
	return update(CS42L42_HP_CTL, CS42L42_HP_ANA_AMUTE_MASK | CS42L42_HP_ANA_BMUTE_MASK,
	    on ? (CS42L42_HP_ANA_AMUTE_MASK | CS42L42_HP_ANA_BMUTE_MASK) : 0);
}

int
PDCs42l83::powerUp(unsigned attenuation)
{
	uint8_t lock = 0;
	int err;

	if (!ready)
		return ENXIO;
	err = mute(true);
	if (err == 0)
		err = setVolume(attenuation);
	/* the DAPM playback widgets in power-up order: supplies, mixer, DAC */
	if (err == 0)
		err = update(CS42L42_ASP_CLK_CFG, CS42L42_ASP_SCLK_EN_MASK, CS42L42_ASP_SCLK_EN_MASK);
	if (err == 0)
		err = update(CS42L42_PWR_CTL1, CS42L42_ASP_DAI_PDN_MASK, 0);
	if (err == 0)
		err = update(CS42L42_PWR_CTL1, CS42L42_MIXER_PDN_MASK, 0);
	if (err == 0)
		err = update(CS42L42_PWR_CTL1, CS42L42_HP_PDN_MASK, 0);
	if (err != 0)
		return err;
	IOSleep(CS42L42_HP_ADC_EN_TIME_US / 1000 + 1);

	/*
	 * cs42l42_mute_stream(unmute), without the unmute: start the PLL from the bit
	 * clock, wait for lock, and only then leave the oscillator. Without a locked PLL
	 * the chip would have no clock and its I2C interface would stop answering.
	 */
	err = update(CS42L42_PLL_CTL1, CS42L42_PLL_START_MASK, 1);
	if (err == 0 && kPll[pll].n > 1) {
		IODelay(CS42L42_PLL_DIVOUT_TIME_US);
		err = write(CS42L42_PLL_CTL3, kPll[pll].pll_divout);
	}
	for (int i = 0; err == 0 && i < 20; i++) {
		err = read(CS42L42_PLL_LOCK_STATUS, &lock);
		if (lock & 1)
			break;
		IOSleep(1);
	}
	if (err == 0 && !(lock & 1)) {
		LOG("the PLL did not lock (status 0x%02x); staying on the oscillator", lock);
		update(CS42L42_PLL_CTL1, CS42L42_PLL_START_MASK, 0);
		return ETIMEDOUT;
	}
	if (err == 0)
		err = update(CS42L42_MCLK_SRC_SEL, CS42L42_MCLK_SRC_SEL_MASK, CS42L42_MCLK_SRC_SEL_MASK);
	if (err == 0)
		err = write(CS42L42_OSC_SWITCH, CS42L42_SCLK_PRESENT_MASK);
	IODelay(CS42L42_CLOCK_SWITCH_DELAY_US);
	if (err == 0)
		onSclk = true;
	return err;
}

int
PDCs42l83::powerDown(void)
{
	int err;

	if (!ready)
		return ENXIO;
	err = mute(true);
	if (onSclk) {
		/* cs42l42_mute_stream(mute): oscillator first, then disconnect and stop the PLL */
		int e = write(CS42L42_OSC_SWITCH, 0);
		IODelay(CS42L42_CLOCK_SWITCH_DELAY_US);
		if (e == 0)
			e = update(CS42L42_MCLK_SRC_SEL, CS42L42_MCLK_SRC_SEL_MASK, 0);
		IODelay(200);
		if (e == 0)
			e = update(CS42L42_PLL_CTL1, CS42L42_PLL_START_MASK, 0);
		if (e == 0)
			onSclk = false;
		else if (err == 0)
			err = e;
	}
	/* the DAPM widgets in power-down order */
	update(CS42L42_PWR_CTL1, CS42L42_HP_PDN_MASK, CS42L42_HP_PDN_MASK);
	update(CS42L42_PWR_CTL1, CS42L42_MIXER_PDN_MASK, CS42L42_MIXER_PDN_MASK);
	update(CS42L42_PWR_CTL1, CS42L42_ASP_DAI_PDN_MASK, CS42L42_ASP_DAI_PDN_MASK);
	update(CS42L42_ASP_CLK_CFG, CS42L42_ASP_SCLK_EN_MASK, 0);
	return err;
}

bool
PDCs42l83::inputLocked(void)
{
	uint8_t v = 0;
	return ready && read(CS42L42_SRC_STATUS, &v) == 0 && (v & CS42L42_SRC_ILK_MASK);
}

void
PDCs42l83::status(PDCs42l83Status *out)
{
	static const uint16_t regs[] = {
		/* grouped by page: each change of page is one more transfer */
		CS42L42_PLL_LOCK_STATUS, CS42L42_ASP_RX_STATUS, CS42L42_SRC_STATUS, CS42L42_CODEC_STATUS,
		CS42L42_TSRS_PLUG_STATUS, CS42L42_DET_STATUS1, CS42L42_OSC_SWITCH_STATUS, CS42L42_PWR_CTL1,
		CS42L42_HP_CTL, CS42L42_MIXER_CHA_VOL, CS42L42_MIXER_CHB_VOL, CS42L42_ASP_CLK_CFG,
		CS42L42_MCLK_SRC_SEL, CS42L42_PLL_CTL1,
	};
	uint8_t v[sizeof(regs) / sizeof(regs[0])];

	bzero(out, sizeof(*out));
	bzero(v, sizeof(v));
	if (!ready) {
		out->error = ENXIO;
		return;
	}
	for (unsigned i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
		int err = read(regs[i], &v[i]);
		if (err != 0 && out->error == 0)
			out->error = err;
	}
	out->pllLock = v[0];
	out->aspRx = v[1];
	out->src = v[2];
	out->codec = v[3];
	out->plug = (v[4] >> CS42L42_TS_PLUG_SHIFT) & 3;
	out->det1 = v[5];
	out->oscSwitch = v[6];
	out->pwr1 = v[7];
	out->hpCtl = v[8];
	out->volA = v[9];
	out->volB = v[10];
	out->aspClk = v[11];
	out->mclkSrc = v[12];
	out->pllCtl1 = v[13];
}
