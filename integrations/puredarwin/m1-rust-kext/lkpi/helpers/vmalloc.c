// SPDX-License-Identifier: GPL-2.0
/*
 * PureDarwin replacement for rust/helpers/vmalloc.c. LinuxKPI has no vrealloc(); vmalloc()
 * and vfree() are the kmalloc allocator there (linux/slab.h: vfree is kfree), so
 * vrealloc_node_align is krealloc with the same alignment limit as in slab.c.
 */
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

__rust_helper void *
rust_helper_vrealloc_node_align(const void *p, size_t size, unsigned long align,
				gfp_t flags, int node)
{
	if (align > PAGE_SIZE)
		return NULL;
	if (align > ARCH_KMALLOC_MINALIGN && size != 0 && size <= PAGE_SIZE)
		return NULL;	/* only the page-sized path (contigmalloc) is page aligned */
	return krealloc(p, size, flags);
}

__rust_helper bool rust_helper_is_vmalloc_addr(const void *x)
{
	return is_vmalloc_addr(x);
}
