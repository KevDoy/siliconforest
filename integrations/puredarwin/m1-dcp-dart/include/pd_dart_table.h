/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/*
 * pd_dart_table.h - Apple DART page tables, T8020 ("DART1") format, 16K pages, two levels
 * below the TTBR: the table code of the display DARTs, free of any kernel dependency so it
 * can be tested on a host. Layout from Linux drivers/iommu/io-pgtable-dart.c and m1n1
 * src/dart.c (omacom pins); behaviour is m1n1's for an inherited ("locked") stream: an
 * existing entry is never changed, a new second-level table is added only in a first-level
 * slot that reads 0, a leaf is written only where it reads 0.
 */
#ifndef PD_DART_TABLE_H
#define PD_DART_TABLE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PD_DART_PAGE_SHIFT  14
#define PD_DART_PAGE        (1ULL << PD_DART_PAGE_SHIFT)
#define PD_DART_PTES        2048u
#define PD_DART_L2_SPAN     ((uint64_t)PD_DART_PTES << PD_DART_PAGE_SHIFT) /* 32 MiB */

#define PD_DART_PTE_PADDR   0x0000000ffffff000ULL /* APPLE_DART1_PADDR_MASK */
#define PD_DART_PTE_SP_END  (0xfffULL << 40)      /* subpage end 0xfff, start 0 */
#define PD_DART_PTE_NO_READ (1ULL << 8)
#define PD_DART_PTE_NO_WRITE (1ULL << 7)
#define PD_DART_PTE_SP_DIS  (1ULL << 1)
#define PD_DART_PTE_VALID   (1ULL << 0)

#define PD_DART_PROT_READ   1u
#define PD_DART_PROT_WRITE  2u

/* How the table code reaches memory. */
struct pd_dart_table_ops {
	void *ctx;
	/* the table page at this physical address, readable and writable; NULL if unreachable */
	uint64_t *(*table)(void *ctx, uint64_t phys);
	/* a new zeroed table page; returns its physical address, 0 on failure */
	uint64_t (*alloc_table)(void *ctx);
};

struct pd_dart_tables {
	uint64_t *l1;	/* the live first-level table (the page TTBR 0 names) */
	struct pd_dart_table_ops ops;
	/* counters, for logs and tests */
	uint32_t l1_writes, leaf_writes, leaf_clears, tables_added;
};

/* What a walk saw. */
struct pd_dart_walk {
	uint64_t l1e;		/* first-level entry */
	uint64_t l2_phys;	/* second-level table, 0 if the first level is empty */
	uint64_t pte;		/* leaf entry, 0 if none */
	uint64_t phys;		/* translated address including the page offset, 0 if not mapped */
};

/* Read-only. iova is the address inside the stream (already masked to its input size). */
int pd_dart_table_walk(struct pd_dart_tables *t, uint64_t iova, struct pd_dart_walk *w);

/*
 * Maps [iova, iova + size) to [phys, phys + size), all page aligned. Fails without changing
 * anything it did not add itself: -EEXIST if a leaf in the range is in use, -ENOMEM if a
 * table is needed and cannot be had or reached, -EINVAL for bad arguments. On failure the
 * leaves written by this call are cleared again (tables added stay, empty).
 */
int pd_dart_table_map(struct pd_dart_tables *t, uint64_t iova, uint64_t phys, uint64_t size,
		      unsigned int prot);

/* Clears the leaves of [iova, iova + size). Returns the number of bytes that were mapped. */
uint64_t pd_dart_table_unmap(struct pd_dart_tables *t, uint64_t iova, uint64_t size);

/*
 * Finds where a physical range is mapped: the lowest iova in [lo, hi) at which `size` bytes
 * translate contiguously to `phys` (m1n1 dart_search_range / dart_get_mapping). Returns 0
 * and *iova, or -ENOENT.
 */
int pd_dart_table_find(struct pd_dart_tables *t, uint64_t phys, uint64_t size, uint64_t lo,
		       uint64_t hi, uint64_t *iova);

/* Whether every leaf of [iova, iova + size) is empty (and so the range can be mapped). */
int pd_dart_table_range_free(struct pd_dart_tables *t, uint64_t iova, uint64_t size);

#ifdef __cplusplus
}
#endif

#endif
