// SPDX-License-Identifier: MIT
/* See pd_nvme_wring.h. */
#include "pd_nvme_wring.h"

#ifdef PD_WRING_HOST_TEST
void (*pd_wring_hook)(int stage, uint8_t *slot);
#define HOOK(stage, slot) do { if (pd_wring_hook) pd_wring_hook(stage, slot); } while (0)
#else
#define HOOK(stage, slot) do { } while (0)
#endif

/*
 * fence->open is 1 or 0 and nothing else (the gate reads it as a truth value). A fence that was
 * opened keeps first/last when it is closed: that is how "closed for good" is told from "never opened".
 */

int
pd_wfence_open(struct pd_nvme_fence *f, uint64_t first, uint64_t blocks, uint64_t nsBlocks, int verified, int writeArg)
{
	if (f == 0 || f->open != 0 || f->first != 0 || f->last != 0)
		return 1;
	if (verified != 1 || writeArg != 1)
		return 2;
	if (first != PD_SF_FENCE_FIRST || blocks != PD_SF_FENCE_BLOCKS)
		return 3;
	if (nsBlocks == 0 || first >= nsBlocks || blocks > nsBlocks - first)
		return 4;
	f->first = PD_SF_FENCE_FIRST;
	f->last = PD_SF_FENCE_FIRST + PD_SF_FENCE_BLOCKS - 1;
	f->open = 1;
	return 0;
}

void
pd_wfence_close(struct pd_nvme_fence *f)
{
	if (f != 0)
		f->open = 0;
}

int
pd_wabs_ok(const struct pd_nvme_fence *f, uint64_t lba, uint64_t nblks)
{
	if (f == 0 || f->open != 1 || f->last < f->first || nblks == 0)
		return 0;
	if (lba < f->first || lba > f->last)
		return 0;
	return nblks - 1 <= f->last - lba;
}

uint64_t
pd_wdev_blocks(const struct pd_nvme_fence *f)
{
	if (f == 0 || f->last < f->first || (f->first == 0 && f->last == 0))
		return 0;
	return f->last - f->first + 1;
}

int
pd_wdev_map(const struct pd_nvme_fence *f, uint64_t block, uint64_t nblks, uint64_t *lba)
{
	uint64_t size = pd_wdev_blocks(f);

	if (lba == 0 || size == 0 || nblks == 0 || block >= size || nblks > size - block)
		return 1;
	*lba = f->first + block;        /* block < size, so this is at most f->last */
	return 0;
}

static int
has_pointer(const uint8_t sqe[64])
{
	for (unsigned i = 24; i < 32; i++)
		if (sqe[i] != 0)
			return 1;
	return 0;
}

int
pd_wring_dir(unsigned queue, const uint8_t sqe[64], int hasData)
{
	int pointer = has_pointer(sqe);

	if (queue == PD_NVME_Q_IO) {
		switch (sqe[0]) {
		case PD_NVME_IO_READ:   return pointer && hasData ? PD_TCB_DMA_FROM_DEVICE : -1;
		case PD_NVMEW_IO_WRITE: return pointer && hasData ? PD_TCB_DMA_TO_DEVICE : -1;
		case PD_NVMEW_IO_FLUSH: return !pointer && !hasData ? PD_TCB_DMA_NONE : -1;
		default:                return -1;
		}
	}
	if (queue != PD_NVME_Q_ADMIN)
		return -1;
	/*
	 * The admin queue as in the read-only build (and Linux): no pointer, no DMA; the odd
	 * opcodes of the allowlist are Create I/O CQ/SQ, whose pointer is the queue itself.
	 */
	if (!pointer || !hasData)
		return PD_TCB_DMA_NONE;
	if (sqe[0] == PD_NVME_ADMIN_CREATE_SQ || sqe[0] == PD_NVME_ADMIN_CREATE_CQ)
		return PD_TCB_DMA_TO_DEVICE;
	return (sqe[0] & 1) ? -1 : PD_TCB_DMA_FROM_DEVICE;
}

int
pd_wring_prepare(unsigned queue, unsigned tag, unsigned depth, int usable, const uint8_t in[64], int hasData,
    uint64_t nsBlocks, const struct pd_nvme_fence *f, uint8_t slot[64], uint8_t tcb[PD_TCB_SIZE])
{
	uint8_t sqe[64];
	int reason, dir;

	if (!usable || tag >= depth || tag > 0xff)
		return PD_WRING_UNUSABLE;
	for (unsigned i = 0; i < 64; i++)
		sqe[i] = in[i];
	sqe[2] = (uint8_t)tag;
	sqe[3] = 0;
	/* 1: the command as built */
	reason = pd_nvmew_gate(queue, sqe, nsBlocks, f);
	if (reason != PD_NVME_GATE_OK)
		return reason;
	for (unsigned i = 0; i < 64; i++)
		slot[i] = sqe[i];
	HOOK(1, slot);
	/* 2: the bytes the controller will read */
	reason = pd_nvmew_gate(queue, slot, nsBlocks, f);
	HOOK(2, slot);
	if (reason == PD_NVME_GATE_OK) {
		dir = pd_wring_dir(queue, slot, hasData);
		if (dir < 0)
			reason = PD_WRING_DIRECTION;
		/* 3: the second look, on the slot, the last thing before the TCB and the doorbell */
		else if (!pd_nvmew_inside(queue, slot, f))
			reason = PD_WRING_SECOND;
	}
	if (reason != PD_NVME_GATE_OK) {
		for (unsigned i = 0; i < 64; i++)
			slot[i] = 0;
		return reason;
	}
	tcb[PD_TCB_OPCODE] = 0;
	tcb[PD_TCB_DMA_FLAGS] = (uint8_t)dir;
	tcb[PD_TCB_COMMAND_ID] = (uint8_t)tag;
	tcb[PD_TCB_COMMAND_ID + 1] = 0;
	tcb[PD_TCB_LENGTH] = slot[48];
	tcb[PD_TCB_LENGTH + 1] = slot[49];
	for (unsigned i = 0; i < 16; i++)
		tcb[PD_TCB_PRP1 + i] = slot[24 + i];
	return PD_NVME_GATE_OK;
}
