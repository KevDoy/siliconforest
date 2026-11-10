// SPDX-License-Identifier: GPL-2.0
/*
 * dma_fence_init64() for drm-kmod's 6.14-level dma-fence (new file; goes away when the DRM C
 * core is brought to 7.2, where it is a real function). In 6.14 a fence has 64-bit sequence
 * numbers when its ops say so; kernel::dma_fence sets ops->use_64bit_seqno (patch 0005).
 */
#include <linux/dma-fence.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
__rust_helper void rust_helper_dma_fence_init64(struct dma_fence *fence,
						const struct dma_fence_ops *ops,
						spinlock_t *lock, u64 context, u64 seqno)
{
	dma_fence_init(fence, ops, lock, context, seqno);
}
#endif
