/* SPDX-License-Identifier: MIT */
/* CM-only coherent allocations: never route through LinuxKPI's physical DMA. */
#ifndef PD_TB_DMA_API_H
#define PD_TB_DMA_API_H
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include "pd_tb_dma.h"
_Static_assert(sizeof(dma_addr_t)==sizeof(uint64_t),"NHI DMA handle width");
#define dma_alloc_coherent(d,s,h,f) pd_tb_dma_alloc((d),(s),(h),!!((f)&GFP_KERNEL))
#define dma_zalloc_coherent(d,s,h,f) dma_alloc_coherent((d),(s),(h),(f))
#define dma_free_coherent(d,s,p,h) pd_tb_dma_free((d),(s),(p),(h))
#define dma_set_mask(d,m) pd_tb_dma_mask((d),(m))
#define dma_set_coherent_mask(d,m) pd_tb_dma_mask((d),(m))
#define dma_set_mask_and_coherent(d,m) pd_tb_dma_mask((d),(m))
#define dma_pool_create(n,d,s,a,b) pd_tb_pool_create((n),(d),(s),(a),(b))
#define dma_pool_destroy(p) pd_tb_pool_destroy((p))
#define dma_pool_alloc(p,f,h) pd_tb_pool_alloc((p),(h),!!((f)&GFP_KERNEL))
#define dma_pool_zalloc(p,f,h) dma_pool_alloc((p),(f),(h))
#define dma_pool_free(p,a,h) pd_tb_pool_free((p),(a),(h))
#endif
