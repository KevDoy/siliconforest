/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The Touch Bar's digitizer (Apple "Z2", /arm-io/spi0/multi-touch) on SPI0 with a GPIO
 * chip select, reset line and interrupt line. Ported from Linux
 * drivers/input/touchscreen/apple_z2.c (omacom/linux-aurora). The firmware is this
 * machine's own file (never in the repository), the calibration is the device tree's.
 * All calls from one thread at a time.
 */
#ifndef _PD_Z2_H
#define _PD_Z2_H

#include "pd_gpio.h"
#include "pd_spi.h"

#define PD_Z2_RX_BYTES 4000

/* One finger of a report: state 3 (started) and 4 (moved) mean it touches. */
typedef void (*PDZ2Finger)(void *ctx, unsigned finger, unsigned state, unsigned x, unsigned y);

struct PDZ2 {
	PDSpi    *spi;
	PDGpio   *gpio;
	unsigned  pinCS, pinReset;
	uint32_t  hz;
	bool      booted;
	int       indexParity;
	PDZ2Finger finger;
	void     *fingerCtx;
	uint8_t   tx[16];
	uint8_t  *rx;                   /* PD_Z2_RX_BYTES */

	/* protocol counters */
	uint32_t  polls;                /* read-interrupt commands sent */
	uint32_t  replies;              /* answered with an interrupt-data header */
	uint32_t  reports;              /* packets read and parsed */
	uint32_t  badReports;           /* packets with an impossible length or finger count */
	uint32_t  fingersSeen;
	uint32_t  spiErrors;
	uint32_t  blobs;                /* firmware blobs sent */
	uint8_t   lastHeader[16];       /* reply to the last read-interrupt command */
	uint8_t   lastReport[32];       /* start of the last packet */
	uint32_t  lastReportLen;

	PDZ2() : spi(NULL), gpio(NULL), pinCS(0), pinReset(0), hz(0), booted(false), indexParity(0), finger(NULL),
	    fingerCtx(NULL), rx(NULL), polls(0), replies(0), reports(0), badReports(0), fingersSeen(0), spiErrors(0),
	    blobs(0), lastReportLen(0) {}

	/* Holds the device in reset (apple_z2_probe). */
	bool attach(PDSpi *spi, PDGpio *gpio, unsigned pinCS, unsigned pinReset, uint32_t hz);
	void reset(bool asserted);
	/*
	 * Sends the firmware (apple_z2_upload_firmware) with the calibration (may be NULL) and
	 * reads the first packet. `wait` waits up to `ms` for the device's interrupt after a
	 * blob. 0 or an errno value; the device is left in reset on failure.
	 */
	int upload(const uint8_t *fw, size_t fwLen, const uint8_t *cal, size_t calLen, void (*wait)(unsigned ms));
	/* Asks for interrupt data and parses a packet if there is one (apple_z2_read_packet). */
	int readPacket();
	/* Parses a report as readPacket() does after the 5-byte transport header. */
	void parse(const uint8_t *msg, size_t len);
};

#endif
