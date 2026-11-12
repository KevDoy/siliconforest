/* SPDX-License-Identifier: MIT */
/*
 * The command gate of the WRITE-FENCED build (never part of the read-only kext: this file and
 * pd_nvme_wgate.c are compiled only into PDAppleNVMeRW). It admits exactly what the read-only
 * gate admits (pd_nvme_gate(), unchanged, called from here) plus, on the I/O queue only:
 *   Write (0x01)  namespace 1, at most 256 blocks, EVERY block inside the fence [first, last],
 *                 no flags (no FUA, no protection information, no hints, no fused operation)
 *   Flush (0x00)  namespace 1, nothing else in the command
 * and those two only while the fence is open. The fence is one LBA range, set once, after the
 * partition has been identified in both GPT copies (pd_nvme_part.c); closed, the build
 * behaves as the read-only one. Plain C: the kext and the host test compile this same file.
 */
#ifndef _PD_NVME_WGATE_H
#define _PD_NVME_WGATE_H

#include "pd_nvme_gate.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	PD_NVMEW_IO_FLUSH = 0x00,
	PD_NVMEW_IO_WRITE = 0x01,
	PD_NVMEW_MAX_WRITE_BLOCKS = 256,
	PD_NVMEW_GATE_FENCE = 16,       /* a write with a block outside the fence, or the fence closed */
};

struct pd_nvme_fence {
	uint64_t first, last;           /* inclusive, in blocks of namespace 1 */
	int      open;                  /* 0: no write and no flush is admitted */
};

int pd_nvmew_gate(unsigned queue, const uint8_t sqe[64], uint64_t nsBlocks, const struct pd_nvme_fence *fence);

/*
 * The second look, written independently of the gate and called next to the doorbell on the
 * bytes in the queue slot: nonzero only for a command that is not a write, or a write whose
 * blocks all lie inside the fence.
 */
int pd_nvmew_inside(unsigned queue, const uint8_t slot[64], const struct pd_nvme_fence *fence);

#ifdef __cplusplus
}
#endif
#endif
