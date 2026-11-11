/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/*
 * The always-on coprocessor (AOP) of T8103 as a polled client: ASC mailbox, RTKit
 * management and system endpoints, the AFK ring transport of the application endpoints
 * and EPIC calls on the services they announce.
 *
 * Ported from the Omacom Linux tree: drivers/soc/apple/aop.rs, aop_ring.rs, rtkit.c and
 * mailbox.c (see pd_aop.cpp). The sequence is the one of that driver for
 * "apple,t8103-aop"; nothing is sent that it does not send.
 *
 * One thread owns a PDAop: every function is called from it, and nothing here takes a
 * lock or runs in interrupt context. The mailbox is polled.
 */
#ifndef _PD_AOP_H
#define _PD_AOP_H

#include "pd_dart_fixed.h"
#include "pd_soc.h"

#define PD_AOP_EP_FIRST    0x20        /* AFK_ENDPOINT_START */
#define PD_AOP_EP_COUNT    0x0f        /* AFK_ENDPOINT_COUNT */
#define PD_AOP_MAX_CALLS   8           /* AOP_MAX_CALLS */
#define PD_AOP_MAX_SERVICES 48
#define PD_AOP_NAME_LEN    32          /* EPIC_ANNOUNCE_NAME_LEN */

struct PDAopRing {
	uint32_t off, block, size;      /* in the endpoint's shared buffer */
	bool     valid;
};

struct PDAopEndpoint {
	uint8_t   index;                /* RTKit endpoint number */
	bool      started;
	uint8_t  *buf;                  /* shared buffer (kernel address), NULL before GetBuf */
	uint64_t  iova;
	uint32_t  bufSize;
	PDAopRing tx, rx;
	uint16_t  seq;
	/* 0 free, 1 pending, 2 answered, 3 abandoned (the reply never came: the tag stays reserved) */
	uint8_t   call[PD_AOP_MAX_CALLS];
	uint32_t  retcode[PD_AOP_MAX_CALLS];
	uint32_t  dropped;
	uint32_t  reports, replies, announces;
};

struct PDAopService {
	char     name[PD_AOP_NAME_LEN + 1];
	uint8_t  ep;
	uint32_t channel;
};

struct PDAop {
	PDMmio        asc;              /* /arm-io/aop reg 0: CPU control, mailbox at +0x8000 */
	PDMmio        sram;             /* /arm-io/aop reg 2: the firmware's memory, boot arguments */
	PDDartFixed   dart;             /* stream 0 of dart-aop: everything the firmware is handed */
	uint32_t      dartUsed;         /* bytes of the window given out */
	bool          verbose;          /* log every mailbox message */

	/* RTKit */
	uint32_t      endpoints[8];     /* bitmap of the endpoint map */
	uint32_t      version;
	bool          helloSeen, epmapDone, bootFailed, crashed;
	uint32_t      iopPower, apPower;
	bool          iopAck, apAck;
	uint8_t      *syslogBuf, *crashlogBuf, *ioreportBuf, *oslogBuf;
	uint32_t      syslogSize, crashlogSize, ioreportSize, oslogSize;
	uint64_t      syslogIova, crashlogIova, ioreportIova, oslogIova;
	uint32_t      syslogEntries, syslogMsgSize;
	uint32_t      syslogLines, rxMessages, txMessages, unknownMessages;

	/* AFK / EPIC */
	PDAopEndpoint ep[PD_AOP_EP_COUNT];
	PDAopService  services[PD_AOP_MAX_SERVICES];
	unsigned      nservices;
	uint8_t      *rxScratch;        /* one received ring entry */
	uint32_t      rxScratchSize;
	uint32_t      epicCalls, epicErrors, epicTimeouts;

	/*
	 * Maps the registers, programs the DART's address filter as the boot loader does
	 * before Linux (m1n1 dapf_init), attaches the DMA window, completes the firmware's
	 * boot arguments, starts the coprocessor and runs the RTKit handshake up to AP power
	 * on (apple_rtkit_wake). 0 or an errno value; `why` receives a short reason.
	 */
	int boot(uint64_t windowIova, uint32_t windowBytes, char *why, size_t whyLen);
	/* AopData::start_afk(): starts every advertised endpoint 0x20..0x2e and its AFK handshake. */
	int startEndpoints(void);
	/* Takes and handles every message waiting in the mailbox; the number handled. */
	unsigned poll(void);
	/* Polls for `ms` milliseconds. */
	void pollFor(uint32_t ms);
	/* The service of that name, once announced. */
	const PDAopService *findService(const char *name) const;
	/*
	 * AOP::epic_call(): one notify with `subtype` on the service's channel, then polls
	 * (at most 5 s) for the reply and returns the firmware's return code in *retcode.
	 * 0, or an errno value when the call could not be made or was not answered.
	 */
	int epicCall(const PDAopService *svc, uint16_t subtype, const void *data, uint32_t len, uint32_t *retcode);
	bool hasEndpoint(unsigned n) const { return n < 256 && (endpoints[n / 32] & (1u << (n % 32))) != 0; }
	bool running(void) const { return !crashed && (iopPower & 0xff) == 0x20 && (apPower & 0xff) == 0x20; }

	/* internal */
	bool mboxRecv(uint64_t *msg, uint8_t *ep);
	bool mboxSend(uint8_t ep, uint64_t msg);
	bool mgmtSend(uint8_t type, uint64_t msg);
	bool startEp(uint8_t ep);
	bool waitFlag(const bool *flag, uint32_t ms);
	void *windowAlloc(uint32_t bytes, uint64_t *iova);
	void rxMgmt(uint64_t msg);
	void rxBuffer(uint8_t ep, uint64_t msg, uint8_t **buf, uint32_t *size, uint64_t *iova, const char *what);
	void rxSyslog(uint64_t msg);
	void rxIoreport(uint64_t msg);
	void rxOslog(uint64_t msg);
	void rxCrashlog(uint64_t msg);
	void rxAfk(PDAopEndpoint *e, uint64_t msg);
	bool afkParseRing(PDAopEndpoint *e, uint64_t msg, PDAopRing *out);
	void afkRecvRing(PDAopEndpoint *e);
	int afkHandle(PDAopEndpoint *e, uint32_t channel, const uint8_t *msg, uint32_t len);
	int afkWrite(PDAopEndpoint *e, uint32_t channel, uint32_t type, const uint8_t *hdr, uint32_t hdrLen,
	    const uint8_t *data, uint32_t len, uint64_t *doorbell);
	void afkDrop(PDAopEndpoint *e, const char *why, uint32_t a, uint32_t b, uint32_t c);
	int patchBootargs(void);
	int dapfInit(void);
};

#endif
