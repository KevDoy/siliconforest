// SPDX-License-Identifier: GPL-2.0
/*
 * PureDarwin replacement for rust/helpers/slab.c.
 *
 * Linux 7.2 has krealloc_node_align()/kvrealloc_node_align(); LinuxKPI (6.14 level) has
 * krealloc()/kvrealloc() without alignment or node. On PureDarwin every kmalloc ends in
 * pdos_malloc(): IOMalloc() plus a size header, so the only alignment that holds is
 * ARCH_KMALLOC_MINALIGN (8). A request for more cannot be met by a pointer that kfree()
 * would still accept, so it fails (NULL, the Rust side reports ENOMEM) instead of handing
 * out misaligned memory.
 */
#include <linux/slab.h>

#define PD_KMALLOC_MAX_ALIGN ARCH_KMALLOC_MINALIGN

__rust_helper void *
rust_helper_krealloc_node_align(const void *objp, size_t new_size, unsigned long align,
				gfp_t flags, int node)
{
	if (align > PD_KMALLOC_MAX_ALIGN)
		return NULL;
	return krealloc(objp, new_size, flags);
}

__rust_helper void *
rust_helper_kvrealloc_node_align(const void *p, size_t size, unsigned long align,
				 gfp_t flags, int node)
{
	if (align > PD_KMALLOC_MAX_ALIGN)
		return NULL;
	/* LinuxKPI: kvmalloc and kmalloc are the same allocator, kvfree is kfree */
	return krealloc(p, size, flags);
}
