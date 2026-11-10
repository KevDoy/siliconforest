// SPDX-License-Identifier: GPL-2.0-only OR MIT
/*
 * pd_dart_table.c - Apple DART page tables (T8020 format, 16K pages) for an inherited
 * stream. See pd_dart_table.h. No kernel dependency: memory is reached through the ops.
 *
 * Layout: Linux drivers/iommu/io-pgtable-dart.c (Copyright (C) 2022 The Asahi Linux
 * Contributors, based on io-pgtable-arm, Copyright (C) 2014 ARM Limited). Rules for an
 * inherited root: m1n1 src/dart.c dart_get_l2() / dart_map_page() / dart_search_range()
 * (Copyright The Asahi Linux Contributors).
 */
#include "pd_dart_table.h"

/* errno values used here are the same on Linux, FreeBSD and Darwin */
#define PD_ENOENT 2
#define PD_ENOMEM 12
#define PD_EEXIST 17
#define PD_EINVAL 22

#define PAGE_MASK_ (PD_DART_PAGE - 1)

static uint64_t *l2_of(struct pd_dart_tables *t, uint64_t l1e)
{
	if (!(l1e & PD_DART_PTE_VALID))
		return NULL;
	return t->ops.table(t->ops.ctx, l1e & PD_DART_PTE_PADDR & ~PAGE_MASK_);
}

int pd_dart_table_walk(struct pd_dart_tables *t, uint64_t iova, struct pd_dart_walk *w)
{
	uint64_t l1i = iova >> 25;
	uint64_t *l2;

	w->l1e = w->l2_phys = w->pte = w->phys = 0;
	if (l1i >= PD_DART_PTES)
		return -PD_EINVAL;
	w->l1e = t->l1[l1i];
	if (!(w->l1e & PD_DART_PTE_VALID))
		return 0;
	w->l2_phys = w->l1e & PD_DART_PTE_PADDR & ~PAGE_MASK_;
	l2 = l2_of(t, w->l1e);
	if (!l2)
		return -PD_ENOMEM;
	w->pte = l2[(iova >> PD_DART_PAGE_SHIFT) & (PD_DART_PTES - 1)];
	if (w->pte & PD_DART_PTE_VALID)
		w->phys = (w->pte & PD_DART_PTE_PADDR & ~PAGE_MASK_) | (iova & PAGE_MASK_);
	return 0;
}

int pd_dart_table_range_free(struct pd_dart_tables *t, uint64_t iova, uint64_t size)
{
	uint64_t a;

	for (a = iova; a < iova + size; a += PD_DART_PAGE) {
		struct pd_dart_walk w;

		if (pd_dart_table_walk(t, a, &w) || w.pte != 0)
			return 0;
	}
	return 1;
}

int pd_dart_table_map(struct pd_dart_tables *t, uint64_t iova, uint64_t phys, uint64_t size,
		      unsigned int prot)
{
	uint64_t flags = PD_DART_PTE_SP_END | PD_DART_PTE_SP_DIS | PD_DART_PTE_VALID;
	uint64_t off;
	int ret = 0;

	if (!size || ((iova | phys | size) & PAGE_MASK_) || (phys & ~PD_DART_PTE_PADDR & ~PAGE_MASK_) ||
	    ((phys + size - 1) & ~PD_DART_PTE_PADDR & ~PAGE_MASK_) ||
	    (iova + size - 1) >> 25 >= PD_DART_PTES || iova + size < iova)
		return -PD_EINVAL;
	/* io-pgtable-dart.c dart_prot_to_pte() */
	if (!(prot & PD_DART_PROT_READ))
		flags |= PD_DART_PTE_NO_READ;
	if (!(prot & PD_DART_PROT_WRITE))
		flags |= PD_DART_PTE_NO_WRITE;

	for (off = 0; off < size; off += PD_DART_PAGE) {
		uint64_t a = iova + off;
		uint64_t l1i = a >> 25, l2i = (a >> PD_DART_PAGE_SHIFT) & (PD_DART_PTES - 1);
		uint64_t l1e = t->l1[l1i];
		uint64_t *l2;

		if (!(l1e & PD_DART_PTE_VALID)) {
			uint64_t tp;

			/* only a slot that reads 0 gets a table: anything else is not ours to replace */
			if (l1e != 0) {
				ret = -PD_EEXIST;
				break;
			}
			tp = t->ops.alloc_table(t->ops.ctx);
			if (!tp || (tp & PAGE_MASK_) || (tp & ~PD_DART_PTE_PADDR)) {
				ret = -PD_ENOMEM;
				break;
			}
			/* dart_install_table(): address and valid, nothing else */
			t->l1[l1i] = tp | PD_DART_PTE_VALID;
			t->l1_writes++;
			t->tables_added++;
			l1e = t->l1[l1i];
		}
		l2 = l2_of(t, l1e);
		if (!l2) {
			ret = -PD_ENOMEM;
			break;
		}
		if (l2[l2i] != 0) {
			ret = -PD_EEXIST;
			break;
		}
		l2[l2i] = ((phys + off) & PD_DART_PTE_PADDR) | flags;
		t->leaf_writes++;
	}
	if (ret) {
		/* take back what this call wrote; `off` is the first page that was not written */
		uint64_t undo;

		for (undo = 0; undo < off; undo += PD_DART_PAGE) {
			uint64_t a = iova + undo;
			uint64_t *l2 = l2_of(t, t->l1[a >> 25]);

			if (l2) {
				l2[(a >> PD_DART_PAGE_SHIFT) & (PD_DART_PTES - 1)] = 0;
				t->leaf_clears++;
			}
		}
	}
	return ret;
}

uint64_t pd_dart_table_unmap(struct pd_dart_tables *t, uint64_t iova, uint64_t size)
{
	uint64_t a, done = 0;

	if ((iova | size) & PAGE_MASK_)
		return 0;
	for (a = iova; a < iova + size && (a >> 25) < PD_DART_PTES; a += PD_DART_PAGE) {
		uint64_t *l2 = l2_of(t, t->l1[a >> 25]);
		uint64_t l2i = (a >> PD_DART_PAGE_SHIFT) & (PD_DART_PTES - 1);

		if (!l2 || !(l2[l2i] & PD_DART_PTE_VALID))
			continue;
		l2[l2i] = 0;
		t->leaf_clears++;
		done += PD_DART_PAGE;
	}
	return done;
}

int pd_dart_table_find(struct pd_dart_tables *t, uint64_t phys, uint64_t size, uint64_t lo,
		       uint64_t hi, uint64_t *iova)
{
	uint64_t want = phys & ~PAGE_MASK_;
	uint64_t pages = ((phys & PAGE_MASK_) + size + PAGE_MASK_) >> PD_DART_PAGE_SHIFT;
	uint64_t l1i;

	if (!size)
		return -PD_EINVAL;
	for (l1i = lo >> 25; l1i < PD_DART_PTES && (l1i << 25) < hi; l1i++) {
		uint64_t *l2 = l2_of(t, t->l1[l1i]);
		uint64_t l2i;

		if (!l2)
			continue;
		for (l2i = 0; l2i < PD_DART_PTES; l2i++) {
			uint64_t start = (l1i << 25) | (l2i << PD_DART_PAGE_SHIFT), n;

			if (start < lo || !(l2[l2i] & PD_DART_PTE_VALID) ||
			    (l2[l2i] & PD_DART_PTE_PADDR & ~PAGE_MASK_) != want)
				continue;
			/* the first page matches: the rest must follow, across tables if need be */
			for (n = 1; n < pages; n++) {
				struct pd_dart_walk w;

				if (pd_dart_table_walk(t, start + (n << PD_DART_PAGE_SHIFT), &w) ||
				    !(w.pte & PD_DART_PTE_VALID) ||
				    (w.pte & PD_DART_PTE_PADDR & ~PAGE_MASK_) != want + (n << PD_DART_PAGE_SHIFT))
					break;
			}
			if (n == pages && start + (pages << PD_DART_PAGE_SHIFT) <= hi) {
				*iova = start | (phys & PAGE_MASK_);
				return 0;
			}
		}
	}
	return -PD_ENOENT;
}
