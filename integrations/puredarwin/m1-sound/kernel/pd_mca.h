/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * The Apple audio transport of T8103: an NCO clock channel, one MCA cluster (frame sync
 * generator and transmit serialiser) routed to an I2S port, and one ADMAC transmit
 * channel feeding it from memory behind dart-sio.
 *
 * Playback only, two 32-bit slots per frame (I2S, the SoC provides both clocks).
 * Callers serialise all calls; nothing here runs in interrupt context: the ADMAC's
 * descriptor and report rings are polled.
 */
#ifndef _PD_MCA_H
#define _PD_MCA_H

#include "pd_soc.h"

struct PDMcaStatus {
	uint32_t ncoCtrl, ncoDiv;
	uint32_t mclkStatus, syncgenStatus, serdesStatus, serdesConf;
	uint32_t portEnables, portClockSel, portDataSel;
	uint32_t mcaIntState;
	uint32_t admacTxEnabled, descRing, reportRing, chanStatus, residue;
};

struct PDMca {
	PDMmio   nco, mca, sw, admac;
	uint32_t ncoRefHz;
	unsigned fe;            /* cluster whose serialiser and clocks are used */
	unsigned port;          /* cluster whose I2S pins are driven */
	unsigned txChan;        /* ADMAC channel number of the cluster's first transmitter */
	bool     attached, clocksOn, running;

	PDMca() : ncoRefHz(0), fe(0), port(0), txChan(0), attached(false), clocksOn(false), running(false) {}

	/* Powers and resets the audio blocks and maps them. 0 or errno. */
	int attach(unsigned feCluster, unsigned portCluster);
	/*
	 * Bit clock = rate * 64 on the port, frame sync at `rate`, serialiser configured
	 * and idle. From here the codec sees its clocks.
	 */
	int startClocks(uint32_t rate);
	/* Resets the serialiser and the DMA rings; nothing moves until descriptors are queued. */
	void prepareDma(void);
	bool descRingFull(void) const;
	bool descRingEmpty(void) const;
	/* Queues [iova, iova + bytes) for transmission. */
	void submit(uint64_t iova, uint32_t bytes);
	/* Starts the DMA channel (at least one descriptor must be queued), then the serialiser. */
	void startDma(void);
	/*
	 * Takes the completion reports that are waiting. Returns their number; *counter
	 * (optional) is the 24 MHz timestamp of the last one.
	 */
	unsigned drainReports(uint64_t *counter);
	/* Clears a latched ring error; true when there was one. */
	bool clearError(void);
	void stopDma(void);
	void stopClocks(void);
	void status(PDMcaStatus *out) const;
};

#endif
