// SPDX-License-Identifier: GPL-2.0-only OR MIT
/*
 * pd_dcp_iommu.c - Linux's IOMMU and DMA API over pd_dart.h for the devices behind the
 * display DARTs. LinuxKPI side; see pd_dcp_iommu.h for what it stands in for.
 *
 * What it does for a bound device follows Linux at the pin (omacom/linux-aurora 6a08a08e):
 *   dma_alloc_coherent      drivers/iommu/dma-iommu.c iommu_dma_alloc(): pages, a range of
 *                           the stream's address space, a mapping, a kernel view of the pages
 *   dma_get_sgtable         iommu_dma_get_sgtable(): the pages behind such an allocation
 *   dma_map_resource        iommu_dma_map_resource(): a physical range at a new address
 *   iommu_map_sgtable       drivers/iommu/iommu.c: each segment at consecutive addresses
 *   iommu_iova_to_phys      drivers/iommu/apple-dart.c:1728: mask to the input size, look up
 * A device that is not bound gets the layer's own implementation of every call.
 */
#define PD_DCP_IOMMU_NO_REDIRECT
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "pd_dcp_iommu.h"
#include "pd_dart.h"
#include "pd_dart_table.h"

#define PD_MAX_BOUND	8
/* the streams' input size on T8103: addresses are masked to 32 bits before a lookup */
#define PD_IOVA_MASK	0xffffffffULL
/* asc-dram-mask of the dcp (iop-dcp-nub): what its firmware ORs onto an address */
#define PD_DCP_DRAM_MASK 0xf00000000ULL

int pd_dcp_iommu_dva_mask;

struct pd_binding {
	struct device *dev;
	int state;	/* 0 free, 1 bound, 2 looked at: not behind a DART */
	struct pd_iommu_domain dom;
};

struct pd_alloc {
	struct list_head link;
	struct device *dev;
	void *cpu;
	struct page **pages;
	unsigned int npages;
	u64 iova;
	bool vmapped;
};

static struct pd_binding pd_bound[PD_MAX_BOUND];
static LIST_HEAD(pd_allocs);
/* 0: no lock yet, 1: being set up, 2: ready */
static int pd_lock_state;
static struct mutex pd_lock;

static struct mutex *pd_lock_get(void)
{
	if (__sync_bool_compare_and_swap(&pd_lock_state, 0, 1)) {
		mutex_init(&pd_lock);
		WRITE_ONCE(pd_lock_state, 2);
	}
	while (READ_ONCE(pd_lock_state) != 2)
		cpu_relax();
	return &pd_lock;
}

static struct pd_binding *pd_slot(struct device *dev, bool make)
{
	struct pd_binding *free = NULL;
	int i;

	for (i = 0; i < PD_MAX_BOUND; i++) {
		if (pd_bound[i].state && pd_bound[i].dev == dev)
			return &pd_bound[i];
		if (!pd_bound[i].state && !free)
			free = &pd_bound[i];
	}
	if (make && free)
		free->dev = dev;
	return make ? free : NULL;
}

static int pd_bind_stream(struct device *dev, struct pd_dart_stream *s)
{
	struct mutex *lock = pd_lock_get();
	struct pd_binding *b;
	int ret = 0;

	if (!s)
		return -ENODEV;
	mutex_lock(lock);
	b = pd_slot(dev, true);
	if (!b) {
		ret = -ENOSPC;
	} else {
		b->dom.type = IOMMU_DOMAIN_DMA;
		b->dom.pgsize_bitmap = PD_DART_PAGE;
		b->dom.pd_stream = s;
		b->dom.pd_mask = PD_IOVA_MASK;
		b->state = 1;
	}
	mutex_unlock(lock);
	return ret;
}

int pd_dcp_iommu_bind(struct device *dev, const char *dart_path, u32 sid)
{
	if (!dev || !dart_path)
		return -EINVAL;
	return pd_bind_stream(dev, pd_dart_open(dart_path, 0, sid));
}

/* `iommus = <&dart sid ...>` of np, the DART found by its node's reg (of_dma_configure) */
int pd_of_dma_configure(struct device *dev, struct device_node *np, bool force_dma)
{
	struct of_phandle_args args;
	struct resource res;

	if (!dev || !np)
		return -ENODEV;
	if (of_parse_phandle_with_args(np, "iommus", "#iommu-cells", 0, &args) || !args.np ||
	    args.args_count < 1)
		return -ENODEV;
	if (of_address_to_resource(args.np, 0, &res))
		return -ENODEV;
	return pd_bind_stream(dev, pd_dart_open(NULL, res.start, args.args[0]));
}

/* The device's domain; binds from the device's node on the first call. NULL: not behind a DART. */
static struct pd_iommu_domain *pd_domain(struct device *dev)
{
	struct mutex *lock = pd_lock_get();
	struct pd_binding *b;
	int state;

	if (!dev)
		return NULL;
	mutex_lock(lock);
	b = pd_slot(dev, false);
	state = b ? b->state : 0;
	mutex_unlock(lock);
	if (state == 1)
		return &b->dom;
	if (state == 2)
		return NULL;

	if (dev->of_node && pd_of_dma_configure(dev, dev->of_node, true) == 0) {
		mutex_lock(lock);
		b = pd_slot(dev, false);
		mutex_unlock(lock);
		return b && b->state == 1 ? &b->dom : NULL;
	}
	/* remember the answer; devices that are not ours come by often */
	mutex_lock(lock);
	b = pd_slot(dev, true);
	if (b)
		b->state = 2;
	mutex_unlock(lock);
	return NULL;
}

int pd_dcp_iommu_bound(struct device *dev)
{
	return pd_domain(dev) != NULL;
}

/* the address a device or its firmware is given for a stream address */
static dma_addr_t pd_to_dma(u64 iova)
{
	return pd_dcp_iommu_dva_mask ? (iova | PD_DCP_DRAM_MASK) : iova;
}

/* ---- IOMMU API ---- */

struct pd_iommu_domain *pd_iommu_get_domain_for_dev(struct device *dev)
{
	return pd_domain(dev);
}

phys_addr_t pd_iommu_iova_to_phys(struct pd_iommu_domain *domain, dma_addr_t iova)
{
	if (!domain || !domain->pd_stream)
		return 0;
	return pd_dart_translate(domain->pd_stream, iova & domain->pd_mask);
}

static unsigned int pd_prot(int prot)
{
	return ((prot & IOMMU_READ) ? PD_DART_PROT_READ : 0) | ((prot & IOMMU_WRITE) ? PD_DART_PROT_WRITE : 0);
}

int pd_iommu_map(struct pd_iommu_domain *domain, unsigned long iova, phys_addr_t paddr,
		 size_t size, int prot, gfp_t gfp)
{
	if (!domain || !domain->pd_stream)
		return -ENODEV;
	return pd_dart_map(domain->pd_stream, iova & domain->pd_mask, paddr, size, pd_prot(prot));
}

size_t pd_iommu_unmap(struct pd_iommu_domain *domain, unsigned long iova, size_t size)
{
	if (!domain || !domain->pd_stream)
		return 0;
	return pd_dart_unmap(domain->pd_stream, iova & domain->pd_mask, size);
}

ssize_t pd_iommu_map_sgtable(struct pd_iommu_domain *domain, unsigned long iova,
			     struct sg_table *sgt, int prot)
{
	struct scatterlist *sg;
	size_t mapped = 0;
	int i, ret;

	if (!domain || !domain->pd_stream || !sgt)
		return -ENODEV;
	iova &= domain->pd_mask;
	for_each_sgtable_sg(sgt, sg, i) {
		phys_addr_t phys = sg_phys(sg);
		size_t len = ALIGN(sg->length, PD_DART_PAGE);

		if (phys & (PD_DART_PAGE - 1)) {
			ret = -EINVAL;
			goto undo;
		}
		ret = pd_dart_map_deferred(domain->pd_stream, iova + mapped, phys, len, pd_prot(prot));
		if (ret)
			goto undo;
		mapped += len;
	}
	/* one invalidate for the whole table */
	ret = pd_dart_flush(domain->pd_stream);
	if (ret)
		goto undo;
	return mapped;
undo:
	if (mapped)
		pd_dart_unmap(domain->pd_stream, iova, mapped);
	return ret;
}

bool pd_iommu_is_dma_domain(struct pd_iommu_domain *domain)
{
	return domain && domain->type == IOMMU_DOMAIN_DMA;
}

int pd_iommu_device_use_default_domain(struct device *dev)
{
	/* Linux pins the default DMA domain; here the binding is that domain and never changes */
	return pd_domain(dev) ? 0 : -ENODEV;
}

void pd_iommu_device_unuse_default_domain(struct device *dev)
{
}

struct iommu_group *pd_iommu_group_get(struct device *dev)
{
	return NULL;
}

void pd_iommu_group_put(struct iommu_group *group)
{
}

int pd_iommu_group_for_each_dev(struct iommu_group *group, void *data,
				int (*fn)(struct device *, void *))
{
	return 0;
}

/* ---- DMA API ---- */

int pd_dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	/* the stream's 32-bit address range is inside any mask the driver asks for (42 bits) */
	if (pd_domain(dev))
		return mask >= PD_IOVA_MASK ? 0 : -EIO;
	return dma_set_mask_and_coherent(dev, mask);
}

static struct pd_alloc *pd_alloc_find(struct device *dev, void *cpu)
{
	struct pd_alloc *a;

	list_for_each_entry(a, &pd_allocs, link)
		if (a->dev == dev && a->cpu == cpu)
			return a;
	return NULL;
}

static void pd_alloc_drop(struct pd_alloc *a, unsigned int have)
{
	unsigned int i;

	for (i = 0; i < have; i++)
		__free_page(a->pages[i]);
	kfree(a->pages);
	kfree(a);
}

void *pd_dma_alloc_attrs(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp,
			 unsigned long attrs)
{
	struct pd_iommu_domain *dom = pd_domain(dev);
	struct mutex *lock;
	struct pd_alloc *a;
	unsigned int i, n;
	u64 iova;

	if (!dom)
		return dma_alloc_attrs(dev, size, dma_handle, gfp, attrs);
	if (!size || !dma_handle)
		return NULL;
	n = ALIGN(size, PD_DART_PAGE) >> PD_DART_PAGE_SHIFT;
	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return NULL;
	a->pages = kcalloc(n, sizeof(*a->pages), GFP_KERNEL);
	if (!a->pages) {
		kfree(a);
		return NULL;
	}
	/* single pages, as iommu-dma: the DART makes them contiguous for the device */
	for (i = 0; i < n; i++) {
		a->pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!a->pages[i]) {
			pd_alloc_drop(a, i);
			return NULL;
		}
	}
	if (pd_dart_iova_alloc(dom->pd_stream, (u64)n << PD_DART_PAGE_SHIFT, &iova)) {
		pd_alloc_drop(a, n);
		return NULL;
	}
	for (i = 0; i < n; i++) {
		if (pd_dart_map_deferred(dom->pd_stream, iova + ((u64)i << PD_DART_PAGE_SHIFT),
					 page_to_phys(a->pages[i]), PD_DART_PAGE,
					 PD_DART_PROT_READ | PD_DART_PROT_WRITE))
			break;
	}
	/* one invalidate for the whole buffer; a failed one counts as a failed mapping */
	if (pd_dart_flush(dom->pd_stream))
		i = min(i, n - 1);
	if (i == n) {
		a->cpu = n == 1 ? page_address(a->pages[0]) : vmap(a->pages, n, VM_MAP, PAGE_KERNEL);
		a->vmapped = n != 1;
	}
	if (i < n || !a->cpu) {
		pd_dart_unmap(dom->pd_stream, iova, (u64)n << PD_DART_PAGE_SHIFT);
		pd_dart_iova_free(dom->pd_stream, iova, (u64)n << PD_DART_PAGE_SHIFT);
		pd_alloc_drop(a, n);
		return NULL;
	}
	a->dev = dev;
	a->npages = n;
	a->iova = iova;
	lock = pd_lock_get();
	mutex_lock(lock);
	list_add(&a->link, &pd_allocs);
	mutex_unlock(lock);
	*dma_handle = pd_to_dma(iova);
	return a->cpu;
}

void pd_dma_free_attrs(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle,
		       unsigned long attrs)
{
	struct pd_iommu_domain *dom = pd_domain(dev);
	struct mutex *lock;
	struct pd_alloc *a;

	if (!dom) {
		dma_free_attrs(dev, size, cpu_addr, dma_handle, attrs);
		return;
	}
	lock = pd_lock_get();
	mutex_lock(lock);
	a = pd_alloc_find(dev, cpu_addr);
	if (a)
		list_del(&a->link);
	mutex_unlock(lock);
	if (!a)
		return;
	/* unmap and invalidate first: the device must not reach pages that are being freed */
	pd_dart_unmap(dom->pd_stream, a->iova, (u64)a->npages << PD_DART_PAGE_SHIFT);
	pd_dart_iova_free(dom->pd_stream, a->iova, (u64)a->npages << PD_DART_PAGE_SHIFT);
	if (a->vmapped)
		vunmap(a->cpu);
	pd_alloc_drop(a, a->npages);
}

/*
 * Map `n` physical pages, in order, at one contiguous device address of the device's stream
 * (added for PDDcp's import of a buffer another kext owns: the pages are the exporter's and
 * stay pinned by it; nothing is allocated or copied here). One invalidate for the range.
 * Returns 0 and the address, -ENODEV for a device not behind a DART, -ENOMEM/-EIO otherwise.
 */
int pd_dma_map_phys_pages(struct device *dev, const u64 *phys, unsigned int n, dma_addr_t *dma_handle)
{
	struct pd_iommu_domain *dom = pd_domain(dev);
	unsigned int i;
	u64 iova;

	if (!dom)
		return -ENODEV;
	if (!phys || !n || !dma_handle)
		return -EINVAL;
	for (i = 0; i < n; i++)
		if (phys[i] & (PD_DART_PAGE - 1))
			return -EINVAL;
	if (pd_dart_iova_alloc(dom->pd_stream, (u64)n << PD_DART_PAGE_SHIFT, &iova))
		return -ENOMEM;
	for (i = 0; i < n; i++) {
		if (pd_dart_map_deferred(dom->pd_stream, iova + ((u64)i << PD_DART_PAGE_SHIFT), phys[i],
					 PD_DART_PAGE, PD_DART_PROT_READ | PD_DART_PROT_WRITE))
			break;
	}
	if (pd_dart_flush(dom->pd_stream) || i < n) {
		pd_dart_unmap(dom->pd_stream, iova, (u64)n << PD_DART_PAGE_SHIFT);
		pd_dart_iova_free(dom->pd_stream, iova, (u64)n << PD_DART_PAGE_SHIFT);
		return -EIO;
	}
	*dma_handle = pd_to_dma(iova);
	return 0;
}

/* Undo pd_dma_map_phys_pages(): unmap, invalidate, give the address range back. */
void pd_dma_unmap_phys_pages(struct device *dev, dma_addr_t dma_handle, unsigned int n)
{
	struct pd_iommu_domain *dom = pd_domain(dev);
	u64 iova;

	if (!dom || !n)
		return;
	iova = dma_handle & dom->pd_mask;
	pd_dart_unmap(dom->pd_stream, iova, (u64)n << PD_DART_PAGE_SHIFT);
	pd_dart_iova_free(dom->pd_stream, iova, (u64)n << PD_DART_PAGE_SHIFT);
}

void *pd_dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp)
{
	return pd_dma_alloc_attrs(dev, size, dma_handle, gfp, 0);
}

void pd_dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle)
{
	pd_dma_free_attrs(dev, size, cpu_addr, dma_handle, 0);
}

int pd_dma_get_sgtable_attrs(struct device *dev, struct sg_table *sgt, void *cpu_addr,
			     dma_addr_t dma_addr, size_t size, unsigned long attrs)
{
	struct mutex *lock;
	struct scatterlist *sg;
	struct pd_alloc *a;
	unsigned int i, n;
	int ret;

	if (!pd_domain(dev))
		return -ENXIO; /* the layer has no dma_get_sgtable for an unbound device */
	lock = pd_lock_get();
	mutex_lock(lock);
	a = pd_alloc_find(dev, cpu_addr);
	mutex_unlock(lock);
	if (!a)
		return -EINVAL;
	n = min_t(unsigned int, a->npages, ALIGN(size, PD_DART_PAGE) >> PD_DART_PAGE_SHIFT);
	ret = sg_alloc_table(sgt, n, GFP_KERNEL);
	if (ret)
		return ret;
	for_each_sgtable_sg(sgt, sg, i)
		sg_set_page(sg, a->pages[i], PD_DART_PAGE, 0);
	return 0;
}

dma_addr_t pd_dma_map_resource(struct device *dev, phys_addr_t phys, size_t size,
			       enum dma_data_direction dir, unsigned long attrs)
{
	struct pd_iommu_domain *dom = pd_domain(dev);
	u64 off = phys & (PD_DART_PAGE - 1), len = ALIGN(off + size, PD_DART_PAGE), iova;

	if (!dom)
		return dma_map_resource(dev, phys, size, dir, attrs);
	if (!size || pd_dart_iova_alloc(dom->pd_stream, len, &iova))
		return DMA_MAPPING_ERROR;
	if (pd_dart_map(dom->pd_stream, iova, phys - off, len, PD_DART_PROT_READ | PD_DART_PROT_WRITE)) {
		pd_dart_iova_free(dom->pd_stream, iova, len);
		return DMA_MAPPING_ERROR;
	}
	return pd_to_dma(iova + off);
}

void pd_dma_unmap_resource(struct device *dev, dma_addr_t dma, size_t size,
			   enum dma_data_direction dir, unsigned long attrs)
{
	struct pd_iommu_domain *dom = pd_domain(dev);
	u64 iova, off, len;

	if (!dom) {
		dma_unmap_resource(dev, dma, size, dir, attrs);
		return;
	}
	iova = dma & dom->pd_mask;
	off = iova & (PD_DART_PAGE - 1);
	len = ALIGN(off + size, PD_DART_PAGE);
	pd_dart_unmap(dom->pd_stream, iova - off, len);
	pd_dart_iova_free(dom->pd_stream, iova - off, len);
}

int pd_dma_mapping_error(struct device *dev, dma_addr_t dma)
{
	if (pd_domain(dev))
		return dma == DMA_MAPPING_ERROR;
	return dma_mapping_error(dev, dma);
}

/* A table of whole pages at consecutive addresses; one segment's address covers the lot. */
int pd_dma_map_sgtable(struct device *dev, struct sg_table *sgt, enum dma_data_direction dir,
		       unsigned long attrs)
{
	struct pd_iommu_domain *dom = pd_domain(dev);
	struct scatterlist *sg;
	u64 total = 0, iova, at = 0;
	int i;

	if (!dom)
		return dma_map_sgtable(dev, sgt, dir, attrs);
	for_each_sgtable_sg(sgt, sg, i) {
		if (sg->offset || (sg->length & (PD_DART_PAGE - 1)))
			return -EINVAL;
		total += sg->length;
	}
	if (!total || pd_dart_iova_alloc(dom->pd_stream, total, &iova))
		return -ENOMEM;
	for_each_sgtable_sg(sgt, sg, i) {
		if (pd_dart_map_deferred(dom->pd_stream, iova + at, sg_phys(sg), sg->length,
					 PD_DART_PROT_READ | PD_DART_PROT_WRITE)) {
			pd_dart_unmap(dom->pd_stream, iova, at);
			pd_dart_iova_free(dom->pd_stream, iova, total);
			return -EIO;
		}
		sg_dma_address(sg) = pd_to_dma(iova + at);
		sg_dma_len(sg) = sg->length;
		at += sg->length;
	}
	if (pd_dart_flush(dom->pd_stream)) {
		pd_dart_unmap(dom->pd_stream, iova, total);
		pd_dart_iova_free(dom->pd_stream, iova, total);
		return -EIO;
	}
	sgt->nents = sgt->orig_nents;
	return 0;
}

void pd_dma_unmap_sgtable(struct device *dev, struct sg_table *sgt, enum dma_data_direction dir,
			  unsigned long attrs)
{
	struct pd_iommu_domain *dom = pd_domain(dev);
	struct scatterlist *sg;
	u64 total = 0, iova;
	int i;

	if (!dom) {
		dma_unmap_sgtable(dev, sgt, dir, attrs);
		return;
	}
	if (!sgt->sgl)
		return;
	iova = sg_dma_address(sgt->sgl) & dom->pd_mask;
	for_each_sgtable_sg(sgt, sg, i)
		total += sg->length;
	pd_dart_unmap(dom->pd_stream, iova, total);
	pd_dart_iova_free(dom->pd_stream, iova, total);
}

int pd_dma_buffer_pages(struct device *dev, void *cpu_addr, size_t size, uint32_t *pages)
{
	struct mutex *lock = pd_lock_get();
	struct pd_alloc *a;
	unsigned int i, n = size >> PD_DART_PAGE_SHIFT;
	int ret = 0;

	if (!pages || !size || (size & (PD_DART_PAGE - 1)))
		return -EINVAL;
	mutex_lock(lock);
	a = pd_alloc_find(dev, cpu_addr);
	if (!a)
		ret = -ENOENT;
	else if (n > a->npages)
		ret = -EINVAL;
	else
		for (i = 0; i < n; i++)
			pages[i] = (uint32_t)(page_to_phys(a->pages[i]) >> PAGE_SHIFT);
	mutex_unlock(lock);
	return ret;
}

void pd_dcp_iommu_report(struct device *dev, const char *when)
{
	struct pd_iommu_domain *dom = pd_domain(dev);

	if (dom)
		pd_dart_check(dom->pd_stream, when);
}
