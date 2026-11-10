/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Per-buffer DMA mapping behind dart-apcie0 (T8020 DART), stream 1 (J293 WLAN/BT).
 *
 * Every address the device is given is an IOVA from one window; nothing is mapped that a
 * caller did not ask for. Mapping is page granular (16 KiB): a buffer that does not start
 * on a page boundary maps its whole page(s), and its device address is iova + offset.
 *
 * TLB discipline, after Linux apple-dart.c (which invalidates after a map as well as after
 * an unmap): a new mapping may only be used by the device after pd_dart_sync() returned 0.
 * An unmap clears the entries at once but the IOVA is only handed out again after the next
 * invalidate, so unmaps cost no invalidate of their own. Define PD_DART_STRICT to invalidate
 * inside every map and unmap instead.
 *
 * All functions return 0 or an errno value unless stated. They take one mutex and may
 * allocate: call them from thread context (a work loop is fine), never from a primary
 * interrupt.
 */
#ifndef _PD_DART_MAP_H
#define _PD_DART_MAP_H
#include <sys/types.h>
#include <stdint.h>

#define PD_DART_MAP_PAGE_SHIFT  14
#define PD_DART_MAP_PAGE_SIZE   (1UL << PD_DART_MAP_PAGE_SHIFT)
#define PD_DART_MAP_PAGE_MASK   (PD_DART_MAP_PAGE_SIZE - 1)

#ifdef __cplusplus
extern "C" {
#endif

/* Programs stream 1 with an empty table and turns translation on. Idempotent. */
int pd_dart_init(void);

/*
 * Maps npages physical pages, which need not be adjacent, at consecutive IOVAs.
 * Every pa[] must be non-zero, 16 KiB aligned and below 2^36. *iova is page aligned.
 */
int pd_dart_map_pages(const uint64_t *pa, unsigned npages, uint64_t *iova);

/*
 * Maps one physically contiguous run. pa may point into a page: the whole pages that
 * [pa, pa + size) touches are mapped and *dva is the device address of pa itself.
 */
int pd_dart_map(uint64_t pa, size_t size, uint64_t *dva);

/* Unmaps the pages that [dva, dva + size) touches; takes what pd_dart_map() returned. */
int pd_dart_unmap(uint64_t dva, size_t size);

/* Invalidates the stream's TLB if a mapping was made since the last invalidate. */
int pd_dart_sync(void);

/*
 * Wired, zeroed, page aligned kernel memory, mapped and already synced. The memory is not
 * physically contiguous but its IOVA range is. NULL on failure.
 */
void *pd_dart_alloc(size_t size, uint64_t *iova);
void  pd_dart_free(void *kva);

/* Physical address behind a wired kernel virtual address, 0 when there is none. */
uint64_t pd_dart_kvtophys(const void *kva);

/* Logs and clears a pending DART fault. Returns the raw error register (0 = none). */
uint32_t pd_dart_check(const char *why);

/* One line: pages mapped now and at the peak, tables, operations, failures. */
void pd_dart_report(const char *who);

#ifdef __cplusplus
}
#endif
#endif
