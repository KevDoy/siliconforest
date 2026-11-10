// SPDX-License-Identifier: GPL-2.0-only OR MIT
/*
 * pd_dcp_linux.c - PDDcp: what drivers/gpu/drm/apple finds in a Linux kernel and device tree
 * and does not find on PureDarwin, supplied from the LinuxKPI side so that the driver's own
 * files stay as they are at the pin.
 *
 *  1. the device right before each probe (the layer's pd_platform_pre_probe): boot-arg gate,
 *     the RTKit port's IOService, the coprocessor's mailbox;
 *  2. small kernel functions the layer lacks (device links, devres groups, a few of_*);
 *  3. entry points of driver files that are not built, each with the reason J293 (the 13-inch
 *     M1 MacBook Pro, internal panel, firmware 13.5) does not reach it. PD_STUB marks them.
 *
 * Compiled with the same forced includes as the driver.
 */
#include <linux/backlight.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include "pd_rtkit_platform.h"
#include "pd_rtkit_compat.h"

void kprintf(const char *, ...);
extern int (*pd_platform_pre_probe)(struct platform_device *pdev);
extern int (*pd_driver_vsnprintf)(char *, size_t, const char *, va_list);
int pd_drm_vsnprintf(char *, size_t, const char *, va_list);
struct linux_file;
extern int (*pd_drm_mmap_pages)(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg);
int pd_dcp_gem_dma_mmap_pages(struct linux_file *filp, off_t offset, size_t size, int prot,
    uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg);
void *pd_dcp_host_of_nub(void *nub);		/* PDDcpHost.cpp */
extern struct device_node *of_root;

#define PD_DCP_MBOX	"/arm-io/dcp"

/* boot-arg pddcp: 0 off, 1 bind and start the coprocessor, 2 the same with KMS for clients */
int pd_dcp_mode;

/* boot-arg pddcp_debug=1: the driver's dev_dbg lines and every repeat of a callback */
int pd_dcp_debug;

/*
 * One line the first time the firmware makes each IOMFB callback, with whether the driver
 * has a handler for it (patches/0002 calls this from dcpep_handle_cb()).
 */
void
pd_dcp_note_callback(struct device *dev, int tag, const char *fourcc, u32 in_len, u32 out_len,
    unsigned int context, bool handled)
{
	static unsigned long seen[16];
	bool first = true;

	if (tag >= 0 && tag < (int)(sizeof(seen) * 8)) {
		unsigned long bit = 1UL << (tag % (8 * sizeof(long)));
		unsigned long *word = &seen[tag / (8 * sizeof(long))];

		first = (*word & bit) == 0;
		*word |= bit;
	}
	if (!first && !pd_dcp_debug)
		return;
	kprintf("PDDcp: %s: callback %c%c%c%c (in %u, out %u) context %u: %s\n", dev_name(dev),
	    fourcc[3], fourcc[2], fourcc[1], fourcc[0], in_len, out_len, context,
	    handled ? "answered" : "NO HANDLER");
}

/* ---- 1. pre-probe -------------------------------------------------------------------- */

static int
pd_dcp_pre_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	void *host;
	int ret;

	if (np == NULL)
		return (0);
	if (of_device_is_compatible(np, "apple,display-subsystem")) {
		kprintf("PDDcp: display-subsystem %s: probing (pddcp=%d)\n", dev_name(&pdev->dev), pd_dcp_mode);
		return (0);
	}
	if (!of_device_is_compatible(np, "apple,dcp"))
		return (0);

	host = pd_dcp_host_of_nub(pdev->pd_nub);
	if (host == NULL) {
		kprintf("PDDcp: pre-probe FAILED: no PDDcp service on the nub of %s\n", dev_name(&pdev->dev));
		return (-ENODEV);
	}
	ret = pd_rtkit_platform_init(host);
	if (ret != 0) {
		kprintf("PDDcp: pre-probe FAILED: pd_rtkit_platform_init %d\n", ret);
		return (ret);
	}
	pd_rtkit_platform_set_log_prefix("PDDcp: rtkit: ");
	ret = pd_apple_mbox_bind(&pdev->dev, 0, PD_DCP_MBOX);
	kprintf("PDDcp: dcp %s: mailbox 0 is %s (%d), probing\n", dev_name(&pdev->dev), PD_DCP_MBOX, ret);
	return (ret);
}

/*
 * "fixed-clock" provider. The display tree gives the dcp a fixed clock (clock-disp0, as
 * Linux's t8103.dtsi does) and the layer has no provider for that compatible, so
 * devm_clk_get() deferred the probe forever (boot dp03: "no provider for clocks of
 * dcp@231c00000 on clock-disp0"). A fixed clock has a rate and nothing else.
 */
#include <linux/pd_providers.h>

struct pd_dcp_fixed_clk { unsigned long rate; };

static void *
pd_dcp_fixed_clk_xlate(void *priv, const struct of_phandle_args *spec)
{
	struct pd_dcp_fixed_clk *clk;
	u32 hz = 0;

	if (spec == NULL || spec->np == NULL)
		return (NULL);
	if (of_property_read_u32(spec->np, "clock-frequency", &hz) != 0)
		kprintf("PDDcp: fixed clock %s has no clock-frequency\n", spec->np->name);
	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (clk != NULL)
		clk->rate = hz;
	kprintf("PDDcp: fixed clock %s: %u Hz\n", spec->np->name, hz);
	return (clk);
}

static int pd_dcp_fixed_clk_enable(void *hw) { return (0); }
static void pd_dcp_fixed_clk_disable(void *hw) { }
static unsigned long pd_dcp_fixed_clk_get_rate(void *hw) { return (((struct pd_dcp_fixed_clk *)hw)->rate); }
static long pd_dcp_fixed_clk_round_rate(void *hw, unsigned long rate) { return ((long)((struct pd_dcp_fixed_clk *)hw)->rate); }

static int
pd_dcp_fixed_clk_set_rate(void *hw, unsigned long rate)
{
	return (rate == ((struct pd_dcp_fixed_clk *)hw)->rate ? 0 : -EINVAL);
}

static const struct pd_clk_ops pd_dcp_fixed_clk_ops = {
	.xlate = pd_dcp_fixed_clk_xlate,
	.enable = pd_dcp_fixed_clk_enable,
	.disable = pd_dcp_fixed_clk_disable,
	.set_rate = pd_dcp_fixed_clk_set_rate,
	.get_rate = pd_dcp_fixed_clk_get_rate,
	.round_rate = pd_dcp_fixed_clk_round_rate,
};

void
pd_dcp_linux_init(void)
{
	int clk_ret = pd_clk_provider_add("fixed-clock", &pd_dcp_fixed_clk_ops, NULL);

	if (clk_ret != 0)
		kprintf("PDDcp: fixed-clock provider not registered: %d\n", clk_ret);
	pd_drm_mmap_pages = pd_dcp_gem_dma_mmap_pages;	/* kext/pd_dcp_mmap.c */
	pd_dcp_mode = pd_rtkit_platform_bootarg("pddcp", 0);
	pd_dcp_debug = pd_rtkit_platform_bootarg("pddcp_debug", 0);
	pd_driver_vsnprintf = pd_drm_vsnprintf;
	pd_platform_pre_probe = pd_dcp_pre_probe;
}

/* ---- 2. kernel functions the layer lacks ----------------------------------------------- */

/* Device links order suspend/resume and unbind on Linux; neither exists here. */
struct device_link { struct device *consumer, *supplier; };

struct device_link *
device_link_add(struct device *consumer, struct device *supplier, u32 flags)
{
	struct device_link *link = kzalloc(sizeof(*link), GFP_KERNEL);

	if (link != NULL) {
		link->consumer = consumer;
		link->supplier = supplier;
	}
	return (link);
}

void
device_set_wakeup_path(struct device *dev)
{
}

void
devm_clk_put(struct device *dev, struct clk *clk)
{
}

/* no deferred probing here: a missing supplier is final */
int
driver_deferred_probe_check_state(struct device *dev)
{
	return (-ENODEV);
}

/*
 * devres groups (drivers/base/component.c brackets a bind with one, to release what a failed
 * bind allocated). LinuxKPI's devres has no groups: resources of a failed bind stay until the
 * device goes away. The id is returned so callers see success.
 */
void *
devres_open_group(struct device *dev, void *id, gfp_t gfp)
{
	return (id != NULL ? id : dev);
}

void
devres_close_group(struct device *dev, void *id)
{
}

int
devres_release_group(struct device *dev, void *id)
{
	return (0);
}

int
devres_remove_group(struct device *dev, void *id)
{
	return (0);
}

/* not device-managed: the backlight lives until the kext does */
struct backlight_device *
devm_backlight_device_register(struct device *dev, const char *name, struct device *parent,
    void *devdata, const struct backlight_ops *ops, const struct backlight_properties *props)
{
	struct backlight_device *bd;

	bd = backlight_device_register(name, parent, devdata, ops, (struct backlight_properties *)props);
	return (bd != NULL ? bd : ERR_PTR(-ENOMEM));
}

void
devm_backlight_device_unregister(struct device *dev, struct backlight_device *bd)
{
	backlight_device_unregister(bd);
}

/*
 * The RTKit port already answers this from Apple's tree ("apple,tXXXX" asked as
 * "arm-io,tXXXX") and renames the Linux name to its own in pd_rtkit_compat.h; the driver's
 * files do not include that header and call the Linux name.
 */
#undef of_machine_is_compatible
bool
of_machine_is_compatible(const char *compat)
{
	return (pd_rtkit_of_machine_is_compatible(compat));
}

/*
 * of_find_matching_node() on the layer's of_find_compatible_node() (which continues after
 * `from` since patches-layer/0018). One compatible at a time, each walked to its end before
 * the next: the order differs from Linux's tree order, the set of nodes does not. A node that
 * matches two entries is returned for the first only.
 */
struct device_node *
of_find_matching_node(struct device_node *from, const struct of_device_id *matches)
{
	const struct of_device_id *m = matches, *e;
	struct device_node *np;

	if (matches == NULL)
		return (NULL);
	/* the entry `from` was found under: the first it is compatible with */
	if (from != NULL)
		while (m->compatible[0] != '\0' && !of_device_is_compatible(from, m->compatible))
			m++;
	for (; m->compatible[0] != '\0'; m++, from = NULL) {
		for (np = of_find_compatible_node(from, NULL, m->compatible); np != NULL;
		    np = of_find_compatible_node(np, NULL, m->compatible)) {
			bool earlier = false;

			for (e = matches; e != m; e++)
				earlier |= of_device_is_compatible(np, e->compatible);
			if (!earlier)
				return (np);
		}
	}
	if (from == NULL && m == matches)
		kprintf("PDDcp: of: nothing in the tree matches the table starting \"%s\"\n", matches->compatible);
	return (NULL);
}

/* the layer has no `status` handling: every child is available */
struct device_node *
of_get_next_available_child(const struct device_node *node, struct device_node *prev)
{
	return (of_get_next_child(node, prev));
}

struct device_node *
of_get_compatible_child(const struct device_node *parent, const char *compatible)
{
	struct device_node *child;

	for_each_child_of_node(parent, child) {
		if (of_device_is_compatible(child, compatible))
			return (child);
	}
	return (NULL);
}

int
of_property_count_u8_elems(const struct device_node *np, const char *propname)
{
	int len = 0;

	if (of_get_property(np, propname, &len) == NULL)
		return (-EINVAL);
	return (len);
}

int
of_property_read_u8_array(const struct device_node *np, const char *propname, u8 *out, size_t sz)
{
	int len = 0;
	const void *val = of_get_property(np, propname, &len);

	if (val == NULL)
		return (-EINVAL);
	if ((size_t)len < sz)
		return (-EOVERFLOW);
	memcpy(out, val, sz);
	return (0);
}

int
of_property_read_variable_u32_array(const struct device_node *np, const char *propname,
    u32 *out_values, size_t sz_min, size_t sz_max)
{
	int len = 0;
	const __be32 *val = of_get_property(np, propname, &len);
	size_t n, i;

	if (val == NULL)
		return (-EINVAL);
	n = (size_t)len / sizeof(u32);
	if (n < sz_min)
		return (-EOVERFLOW);
	if (n > sz_max)
		n = sz_max;
	for (i = 0; i < n; i++)
		out_values[i] = be32_to_cpup(&val[i]);
	return ((int)n);
}

const __be32 *
of_prop_next_u32(const struct property *prop, const __be32 *cur, u32 *pu)
{
	const void *curv = cur;

	if (prop == NULL)
		return (NULL);
	if (cur == NULL) {
		curv = prop->value;
	} else {
		curv = (const char *)curv + sizeof(*cur);
		if ((const char *)curv >= (const char *)prop->value + prop->length)
			return (NULL);
	}
	if (prop->length < (int)sizeof(*cur) || curv == NULL)
		return (NULL);
	*pu = be32_to_cpup(curv);
	return (curv);
}

/*
 * of_platform_device_create(): dcp.c makes the `piodma` child of the dcp node a device of
 * its own (the coprocessor's second DART stream). A bare platform device on the node: no
 * driver binds to it, the driver only does DMA through it.
 */
static void
pd_dcp_child_release(struct device *dev)
{
}

/* the layer's own way of making a struct device for a node (pd/lkpi/pd_linux_platform.c) */
extern const struct kobj_type linux_dev_ktype;
extern struct device linux_root_device;
device_t pd_platform_bsddev(const char *name, const char *desc);

struct platform_device *
of_platform_device_create(struct device_node *np, const char *bus_id, struct device *parent)
{
	struct platform_device *pdev;
	struct device *dev;

	if (np == NULL) {
		kprintf("PDDcp: of_platform_device_create: no node\n");
		return (NULL);
	}
	pdev = kzalloc(sizeof(*pdev), GFP_KERNEL);
	if (pdev == NULL)
		return (NULL);
	pdev->name = np->name;
	pdev->id = -1;
	dev = &pdev->dev;
	/*
	 * Not device_initialize(): boot dp02 took a NULL dereference in it (far 0x48) for a
	 * device that has no FreeBSD device behind it. This is the sequence the platform bus
	 * uses for the devices it creates.
	 */
	kobject_init(&dev->kobj, &linux_dev_ktype);
	kobject_set_name(&dev->kobj, "%s", np->full_name != NULL ? np->full_name : np->name);
	spin_lock_init(&dev->devres_lock);
	INIT_LIST_HEAD(&dev->devres_head);
	INIT_LIST_HEAD(&dev->irqents);
	dev->parent = parent != NULL ? parent : &linux_root_device;
	dev->of_node = np;
	dev->release = pd_dcp_child_release;
	dev->bsddev = pd_platform_bsddev(np->name, np->full_name != NULL ? np->full_name : np->name);
	kprintf("PDDcp: device %s created for node %s (parent %s)\n", dev_name(dev), np->name,
	    parent != NULL ? dev_name(parent) : "none");
	return (pdev);
}

int
of_platform_device_destroy(struct device *dev, void *data)
{
	return (0);
}

/* iomfb_v26_6.c:1035 looks up the "disp0" alias; the layer has no path or alias lookup */
struct device_node *
of_find_node_by_path(const char *path)
{
	return (NULL);
}

/* iomfb_v14_7.c:1849 (firmware 14.7 mapping check) */
int
of_translate_dma_region_checked(struct device_node *np, const __be32 *maps, int cells,
    phys_addr_t *iova, size_t *length)
{
	return (-ENODEV);
}

/* live tree edits: refused (dcp-fabric.c:3614, boards with a display crossbar) */
void of_changeset_init(struct of_changeset *ocs) { }
void of_changeset_destroy(struct of_changeset *ocs) { }
int of_changeset_apply(struct of_changeset *ocs) { return (-ENOSYS); }
int
of_changeset_update_prop_string(struct of_changeset *ocs, struct device_node *np,
    const char *prop_name, const char *str)
{
	return (-ENOSYS);
}

/*
 * Managed coherent allocations through the DART backend (see include/pd_dcp_compat.h). The
 * forced include of pd_dcp_iommu.h makes dma_alloc_coherent()/dma_free_coherent() below the
 * backend's.
 */
struct pd_dcp_dmam {
	struct device *dev;
	size_t size;
	void *vaddr;
	dma_addr_t dma;
};

static void
pd_dcp_dmam_release(void *data)
{
	struct pd_dcp_dmam *m = data;

	if (m->vaddr != NULL)
		dma_free_coherent(m->dev, m->size, m->vaddr, m->dma);
	kfree(m);
}

void *
pd_dcp_dmam_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp)
{
	struct pd_dcp_dmam *m = kzalloc(sizeof(*m), GFP_KERNEL);

	if (m == NULL)
		return (NULL);
	m->dev = dev;
	m->size = size;
	m->vaddr = dma_alloc_coherent(dev, size, &m->dma, gfp);
	if (m->vaddr == NULL) {
		kprintf("PDDcp: %s: managed coherent allocation of %zu bytes failed\n", dev_name(dev), size);
		kfree(m);
		return (NULL);
	}
	{
		void *vaddr = m->vaddr;
		dma_addr_t dma = m->dma;
		int ret;

		/* on failure the action has already run: buffer and record are gone */
		ret = devm_add_action_or_reset(dev, pd_dcp_dmam_release, m);
		if (ret != 0)
			return (NULL);
		*dma_handle = dma;
		return (vaddr);
	}
}

/*
 * An early free is not honoured: the buffer stays until the device's devres list is released
 * (freeing here would leave the devres action to free it a second time).
 */
void
pd_dcp_dmam_free_coherent(struct device *dev, size_t size, void *vaddr, dma_addr_t dma_handle)
{
	kprintf("PDDcp: %s: managed coherent buffer %p (%zu bytes) kept until the device is released\n",
	    dev_name(dev), vaddr, size);
}

/* ---- 3. PD_STUB: entry points of files that are not built ------------------------------ */

struct apple_dcp;

/*
 * dcpext_scanout.c (external-display scanout, 1,1xx lines; needs reserved_mem and
 * dma-map-ops). All five callers in dcp.c test dcp->external or run on an external DCP's
 * paths (dcp.c:1100, 1238-1243, 1906, 2168, 4931); J293 has one DCP, "apple,dcp", whose
 * hw data is apple_dcp_hw_dcp with external false.
 */
static void
pd_dcp_stub_hit(const char *what)
{
	kprintf("PDDcp: PD_STUB reached: %s (not expected on this machine)\n", what);
}

bool dcpext_scanout_terminal(struct apple_dcp *dcp) { return (false); }
bool dcpext_scanout_requested(struct apple_dcp *dcp) { return (false); }
void dcpext_scanout_invalidate(struct apple_dcp *dcp) { pd_dcp_stub_hit("dcpext_scanout_invalidate"); }
void dcpext_scanout_link_restored(struct apple_dcp *dcp) { pd_dcp_stub_hit("dcpext_scanout_link_restored"); }
/* iomfb_v14_7.c:1478, a firmware 14.7 external-display path */
void dcpext_scanout_fault(struct apple_dcp *dcp, int error) { pd_dcp_stub_hit("dcpext_scanout_fault"); }
int dcpext_scanout_register(struct apple_dcp *dcp) { pd_dcp_stub_hit("dcpext_scanout_register"); return (-ENODEV); }

/*
 * j613_dcp_activate.c (J613/T8122 firmware 25G83 cold plug; needs of_changeset).
 * apple_drv.c:1317 calls the cold plug from module init and takes 0 as "nothing to do";
 * dcp.c:3466 asks whether the board is that one.
 */
int apple_j613_25g83_coldplug(void) { return (0); }
/* iomfb_v26_6.c:1047,1159 (firmware 26.6 probe): 0 is "not that board" */
unsigned int apple_j613_25g83_clock_hz(void) { return (0); }
bool apple_t8122_25g83_board(void) { return (false); }

/*
 * PD_STUB: TTM is not built into this kext's DRM core (kext/CMakeLists.txt, PD_DRM_KEEP_ALL).
 * The layer's pd_drm_gem_helpers.c still carries the GEM-TTM helpers for drivers that use
 * TTM and names these three; no driver in this kext has a TTM object, so they are not
 * reached. Defined so that the collection has no unresolved symbol.
 */
struct ttm_buffer_object;
struct vm_area_struct;
struct iosys_map;
int ttm_bo_mmap_obj(struct vm_area_struct *vma, struct ttm_buffer_object *bo) { return (-ENODEV); }
int ttm_bo_vmap(struct ttm_buffer_object *bo, struct iosys_map *map) { return (-ENODEV); }
void ttm_bo_vunmap(struct ttm_buffer_object *bo, struct iosys_map *map) { }
