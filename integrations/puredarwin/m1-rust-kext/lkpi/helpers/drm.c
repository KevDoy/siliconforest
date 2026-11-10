// SPDX-License-Identifier: GPL-2.0
/*
 * PureDarwin replacement for rust/helpers/drm.c: the same three GEM helpers, without the
 * GEM shmem helper part. FreeBSD's drm-kmod does not carry drm_gem_shmem_helper.[ch] at
 * all (PureDarwin's pd_drm_shmem.c is the shmem *file* backend such a helper would sit on,
 * linux_shmem_file_setup() and friends), and Linux's file includes that header
 * unconditionally.
 */
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>

#ifdef CONFIG_DRM

__rust_helper void rust_helper_drm_gem_object_get(struct drm_gem_object *obj)
{
	drm_gem_object_get(obj);
}

__rust_helper void rust_helper_drm_gem_object_put(struct drm_gem_object *obj)
{
	drm_gem_object_put(obj);
}

__rust_helper __u64
rust_helper_drm_vma_node_offset_addr(struct drm_vma_offset_node *node)
{
	return drm_vma_node_offset_addr(node);
}

#endif
