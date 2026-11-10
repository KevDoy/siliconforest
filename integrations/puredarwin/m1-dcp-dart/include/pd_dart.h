/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/*
 * pd_dart.h - one stream of an Apple T8020 DART that firmware provisioned (the display
 * DARTs of T8103): the stream's TCR and TTBR are never written, its live root is reused,
 * tables and entries are only added. Plain C types; implemented by src/pd_dart.cpp (IOKit).
 *
 * Streams used by the display on J293:
 *   /arm-io/dart-dcp    stream 0   the dcp device (RTKit buffers, AFK rings, shared memory)
 *   /arm-io/dart-disp0  stream 0   the display-subsystem device (framebuffers; iBoot's live
 *                                  boot framebuffer is mapped here)
 *   /arm-io/dart-disp0  stream 4   the dcp's piodma child
 */
#ifndef PD_DART_H
#define PD_DART_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pd_dart_stream;

struct pd_dart_info {
	char		path[64];	/* device-tree path of the DART */
	uint64_t	regs_phys;
	uint32_t	sid;
	uint32_t	params1, config, tcr, ttbr[4], enabled_streams, error;
	int		locked;		/* CONFIG bit 15 */
	uint64_t	l1_phys;	/* root named by TTBR 0 */
	uint32_t	l1_valid;	/* valid first-level entries */
	uint64_t	window_lo, window_hi;	/* iova range allocations come from */
};

/*
 * Opens (once; later calls return the same object) the stream `sid` of the DART at
 * device-tree path `path`, or of the DART whose reg 0 is `regs_phys` when path is NULL.
 * Read-only: maps the registers and the live root, checks that the stream is enabled and
 * translating with a valid TTBR 0, and sets up the allocation window. NULL on failure
 * (the reason is logged).
 */
struct pd_dart_stream *pd_dart_open(const char *path, uint64_t regs_phys, uint32_t sid);
void pd_dart_get_info(struct pd_dart_stream *s, struct pd_dart_info *info);

/* Read-only: physical address for an iova of this stream, 0 if not mapped. */
uint64_t pd_dart_translate(struct pd_dart_stream *s, uint64_t iova);
/* Read-only: both entries of the walk, for logs. Returns the physical address or 0. */
uint64_t pd_dart_walk(struct pd_dart_stream *s, uint64_t iova, uint64_t *l1e, uint64_t *pte);
/* Read-only: lowest iova at which [phys, phys + size) is mapped contiguously; 0 on success. */
int pd_dart_find(struct pd_dart_stream *s, uint64_t phys, uint64_t size, uint64_t *iova);

/*
 * Maps [phys, phys + size) at `iova` (page aligned), then invalidates the stream's TLB.
 * prot: PD_DART_PROT_READ | PD_DART_PROT_WRITE (pd_dart_table.h). 0 or a negative errno;
 * -EEXIST when part of the range is already mapped.
 */
int pd_dart_map(struct pd_dart_stream *s, uint64_t iova, uint64_t phys, uint64_t size, unsigned int prot);
/*
 * The same without the TLB invalidate, for a caller that maps many ranges: every run of
 * pd_dart_map_deferred() calls must end with one pd_dart_flush(), also when a call failed.
 */
int pd_dart_map_deferred(struct pd_dart_stream *s, uint64_t iova, uint64_t phys, uint64_t size, unsigned int prot);
int pd_dart_flush(struct pd_dart_stream *s);
/* Clears the range and invalidates. Returns the number of bytes that were mapped. */
uint64_t pd_dart_unmap(struct pd_dart_stream *s, uint64_t iova, uint64_t size);

/*
 * Reserves `size` bytes of iova in the stream's window (first fit, page granular) without
 * mapping anything. Returns 0 and *iova, or -ENOSPC. pd_dart_iova_free gives it back.
 */
int pd_dart_iova_alloc(struct pd_dart_stream *s, uint64_t size, uint64_t *iova);
void pd_dart_iova_free(struct pd_dart_stream *s, uint64_t iova, uint64_t size);

/* The DART's error register (read, never cleared); a flagged fault is logged with `when`. */
uint32_t pd_dart_check(struct pd_dart_stream *s, const char *when);

/* Counters: first-level entries written, leaves written, leaves cleared, TLB invalidates. */
void pd_dart_stats(struct pd_dart_stream *s, uint32_t *l1_writes, uint32_t *leaf_writes,
		   uint32_t *leaf_clears, uint32_t *invalidates);

/* Line prefix for this module's kernel log lines (default "PDDcpDart: "); string must stay valid. */
void pd_dart_set_log_prefix(const char *prefix);

#ifdef __cplusplus
}
#endif

#endif
