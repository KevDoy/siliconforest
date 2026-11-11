/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cirrus Logic CS42L83 headphone codec (the CS42L42 with other reset defaults) on an
 * Apple I2C bus: playback path only, as an I2S clock consumer.
 *
 * Callers serialise all calls (the page register is shared state).
 */
#ifndef _PD_CS42L83_H
#define _PD_CS42L83_H

#include "pd_gpio.h"
#include "pd_i2c.h"

struct PDCs42l83Status {
	uint8_t pllLock;        /* bit 0: locked */
	uint8_t aspRx;          /* sticky: 0 no LRCK, 1 early, 2 late, 3 error, 4 overload */
	uint8_t src;            /* sticky sample-rate converter lock/unlock events */
	uint8_t codec;          /* sticky codec status */
	uint8_t plug;           /* tip sense: 3 plugged, 0 unplugged, 1 or 2 moving */
	uint8_t det1;           /* detect status 1 (bit 7: tip sense level) */
	uint8_t oscSwitch;      /* which clock the digital core runs from */
	uint8_t pwr1, hpCtl, volA, volB, aspClk, mclkSrc, pllCtl1;
	int     error;          /* errno of the first failed read, 0 when all were read */
};

struct PDCs42l83 {
	PDI2C   *bus;
	uint8_t  addr;
	int      page;          /* page register as last written, -1 unknown */
	uint32_t devid;
	uint8_t  revid;
	bool     ready;         /* identified and initialised */
	bool     onSclk;        /* running from the I2S bit clock */
	unsigned pll;           /* entry of the PLL table in use */

	PDCs42l83() : bus(NULL), addr(0), page(-1), devid(0), revid(0), ready(false), onSclk(false), pll(0) {}

	int read(uint16_t reg, uint8_t *value);
	int write(uint16_t reg, uint8_t value);
	int update(uint16_t reg, uint8_t mask, uint8_t value);

	/*
	 * Pulses the reset pin, checks the device ID and leaves the chip initialised, all
	 * blocks but the detection logic powered down, headphone output muted.
	 */
	int probe(PDI2C *bus, uint8_t addr, PDGpio *resetGpio, unsigned resetPin);
	/* I2S, two 32-bit slots, 64 bit clocks per frame at `rate` (44100 or 48000). */
	int configure(uint32_t rate);
	/*
	 * Powers the playback path and moves the chip onto the bit clock, which must be
	 * running. The output stays muted. attenuation: 0..62 dB below full scale.
	 */
	int powerUp(unsigned attenuation);
	int setVolume(unsigned attenuation);
	int mute(bool on);
	/* Mutes, returns to the internal oscillator (the bit clock must still run) and powers down. */
	int powerDown(void);
	void status(PDCs42l83Status *out);
	/*
	 * True once the input sample-rate converter has locked to the incoming frames
	 * (reads the sticky converter status, which clears it).
	 */
	bool inputLocked(void);
};

#endif
