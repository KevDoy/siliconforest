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

void
pd_dcp_linux_init(void)
{
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
 * Depth-first walk of the whole tree from of_root; returns the first node compatible with
 * `compat` that comes after `from` in that order (NULL `from`: the first). Linux's contract
 * for of_find_compatible_node(): `from` is released, the result is held.
 */
struct pd_dcp_walk {
	struct device_node *from;
	const char *compat;
	bool past;
};

static struct device_node *
pd_dcp_walk(struct device_node *np, struct pd_dcp_walk *w, int depth)
{
	struct device_node *child, *found;

	if (np == NULL || depth > 16)
		return (NULL);
	if (w->past) {
		if (of_device_is_compatible(np, w->compat))
			return (of_node_get(np));
	} else if (np == w->from) {
		w->past = true;
	}
	for_each_child_of_node(np, child) {
		found = pd_dcp_walk(child, w, depth + 1);
		if (found != NULL) {
			of_node_put(child);
			return (found);
		}
	}
	return (NULL);
}

#undef of_find_compatible_node
struct device_node *
pd_dcp_of_find_compatible_node(struct device_node *from, const char *type, const char *compat)
{
	struct pd_dcp_walk w = { .from = from, .compat = compat, .past = from == NULL };
	struct device_node *np = pd_dcp_walk(of_root, &w, 0);

	if (np == NULL && from == NULL)
		kprintf("PDDcp: of: no node compatible with \"%s\" in the tree\n", compat);
	if (from != NULL)
		of_node_put(from);
	return (np);
}

struct device_node *
of_find_matching_node(struct device_node *from, const struct of_device_id *matches)
{
	/*
	 * One compatible at a time, each walked to its end before the next: the order differs
	 * from Linux's tree order, the set of nodes does not. `from` belongs to exactly one of
	 * the walks: the one whose compatible it matches.
	 */
	const struct of_device_id *m;
	struct device_node *np;

	for (m = matches; m != NULL && m->compatible[0] != '\0'; m++) {
		if (from != NULL && !of_device_is_compatible(from, m->compatible))
			continue;
		np = pd_dcp_of_find_compatible_node(from != NULL ? of_node_get(from) : NULL, NULL, m->compatible);
		if (np != NULL) {
			if (from != NULL)
				of_node_put(from);
			return (np);
		}
		/* this compatible is exhausted: the first node of each later one follows */
		for (m++; m->compatible[0] != '\0'; m++) {
			np = pd_dcp_of_find_compatible_node(NULL, NULL, m->compatible);
			/* a node that also matched an earlier entry was already returned */
			if (np != NULL) {
				const struct of_device_id *e;
				bool dup = false;

				for (e = matches; e != m; e++)
					dup |= of_device_is_compatible(np, e->compatible);
				if (!dup) {
					if (from != NULL)
						of_node_put(from);
					return (np);
				}
				of_node_put(np);
			}
		}
		break;
	}
	if (from != NULL)
		of_node_put(from);
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

struct platform_device *
of_platform_device_create(struct device_node *np, const char *bus_id, struct device *parent)
{
	struct platform_device *pdev = kzalloc(sizeof(*pdev), GFP_KERNEL);

	if (pdev == NULL)
		return (NULL);
	pdev->name = np->name;
	pdev->id = -1;
	device_initialize(&pdev->dev);
	dev_set_name(&pdev->dev, "%s", np->full_name != NULL ? np->full_name : np->name);
	pdev->dev.parent = parent;
	pdev->dev.of_node = of_node_get(np);
	pdev->dev.release = pd_dcp_child_release;
	kprintf("PDDcp: device %s created for node %s\n", dev_name(&pdev->dev), np->name);
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
