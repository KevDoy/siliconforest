/* SPDX-License-Identifier: MIT */
/*
 * The one place that decides whether an NVMe command may be handed to the controller.
 *
 * READ-ONLY BUILD (the only build there is): the admin queue may carry Identify, Get Log
 * Page (SMART/health only), Create/Delete of I/O queue 1; the I/O queue may carry Read of
 * namespace 1 inside the namespace. Everything else is refused: Write, Write Zeroes,
 * Write Uncorrectable, Dataset Management, Flush, Compare, Copy, Verify, reservations,
 * Format NVM, Sanitize, Firmware Download/Commit, Namespace Management/Attachment,
 * Set Features, Security Send/Receive, vendor commands, and every opcode not listed.
 *
 * The driver calls pd_nvme_gate() on the 64 bytes of the submission queue entry itself
 * (the bytes the controller will read), immediately before the doorbell write. It has no
 * other path to a doorbell. This file is plain C so that the same object is tested on
 * the build host over every opcode (host-test/gate_host.c).
 */
#ifndef _PD_NVME_GATE_H
#define _PD_NVME_GATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	PD_NVME_Q_ADMIN = 0,
	PD_NVME_Q_IO    = 1,
};

/* The opcodes of the allowlist. No other NVMe opcode has a name in this driver. */
enum {
	PD_NVME_ADMIN_DELETE_SQ    = 0x00,
	PD_NVME_ADMIN_CREATE_SQ    = 0x01,
	PD_NVME_ADMIN_GET_LOG_PAGE = 0x02,
	PD_NVME_ADMIN_DELETE_CQ    = 0x04,
	PD_NVME_ADMIN_CREATE_CQ    = 0x05,
	PD_NVME_ADMIN_IDENTIFY     = 0x06,
	PD_NVME_IO_READ            = 0x02,
};

enum {
	PD_NVME_LOG_SMART = 0x02,
	PD_NVME_NSID      = 1,          /* the only namespace this driver reads */
	PD_NVME_IO_QID    = 1,          /* the only I/O queue pair */
	PD_NVME_MAX_READ_BLOCKS = 256,  /* per command (1 MiB of 4 KiB blocks) */
};

/* Reasons, for the log. 0 is "allowed". */
enum {
	PD_NVME_GATE_OK = 0,
	PD_NVME_GATE_QUEUE,             /* not the admin or the I/O queue */
	PD_NVME_GATE_OPCODE,            /* opcode not on the queue's allowlist */
	PD_NVME_GATE_FIELDS,            /* an allowed opcode with fields outside what this driver sends */
	PD_NVME_GATE_RANGE,             /* a read outside namespace 1 or past its end */
};

/* The opcode alone. */
int pd_nvme_opcode_allowed(unsigned queue, uint8_t opcode);

/*
 * The whole submission queue entry. nsBlocks is the size of namespace 1 in blocks
 * (0 before it is known: then no read is allowed at all).
 */
int pd_nvme_gate(unsigned queue, const uint8_t sqe[64], uint64_t nsBlocks);

#ifdef __cplusplus
}
#endif
#endif
