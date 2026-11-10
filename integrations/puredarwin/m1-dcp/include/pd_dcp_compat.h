/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/*
 * pd_dcp_compat.h - force-included in front of every file of drivers/gpu/drm/apple built for
 * PDDcp: names Linux 7.2 has and the layer (FreeBSD LinuxKPI + drm-kmod at 6.14 with
 * m1-drm72's additions) spells differently or lacks. Each entry says which.
 */
#ifndef PD_DCP_COMPAT_H
#define PD_DCP_COMPAT_H

/* Linux 7.2 renamed struct drm_atomic_state to struct drm_atomic_commit; same object */
#define drm_atomic_commit drm_atomic_state

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/uuid.h>
#include <linux/of.h>
#include <linux/cleanup.h>
#include <linux/interrupt.h>
#include <linux/device.h>

/* workqueues: 7.x's default-unbound and freezable system queues; nothing freezes here */
#ifndef system_dfl_wq
#define system_dfl_wq		system_unbound_wq
#endif
#ifndef system_freezable_wq
#define system_freezable_wq	system_wq
#endif
/* the layer's takes (name, flags); Linux's takes a format, flags and arguments */
#undef alloc_ordered_workqueue
#define alloc_ordered_workqueue(fmt, flags, ...)	linux_create_workqueue_common(fmt, 1)

/* no KUnit: test-visible functions are static */
#ifndef VISIBLE_IF_KUNIT
#define VISIBLE_IF_KUNIT	static
#define EXPORT_SYMBOL_IF_KUNIT(sym)
#endif

#ifndef KTIME_MIN
#define KTIME_MIN		(-KTIME_MAX - 1)
#endif

/*
 * scoped_guard(): the layer's linux/cleanup.h pastes `___label_ ## __COUNTER__`, which does not
 * expand __COUNTER__, so two scoped_guard() in one function define the same label. Fixed in
 * the layer by patches-layer/0016; repeated here until every kext is on that layer.
 */
#define __PD_DCP_CAT2(a, b)	a ## b
#define __PD_DCP_CAT(a, b)	__PD_DCP_CAT2(a, b)
#undef scoped_guard
#define scoped_guard(_n, ...)	_scoped_guard(_n, __PD_DCP_CAT(___label_, __COUNTER__), ##__VA_ARGS__)

/* linux/of.h: scope-based put of a device node */
DEFINE_FREE(device_node, struct device_node *, if (_T) of_node_put(_T))

/*
 * linux/of.h iteration the layer's of.h does not have. The functions underneath
 * (of_find_compatible_node, of_get_next_available_child, of_find_property, of_prop_next_u32)
 * are the layer's or have to be added to pd_linux_of.c (README, "needed from the layer").
 */
struct property;
struct device_node *of_find_compatible_node(struct device_node *from, const char *type, const char *compat);
struct device_node *of_get_next_available_child(const struct device_node *node, struct device_node *prev);
const __be32 *of_prop_next_u32(const struct property *prop, const __be32 *cur, u32 *pu);
#ifndef for_each_compatible_node
#define for_each_compatible_node(dn, type, compatible)				\
	for (dn = of_find_compatible_node(NULL, type, compatible); dn;		\
	     dn = of_find_compatible_node(dn, type, compatible))
#endif
#ifndef for_each_child_of_node_scoped
#define for_each_child_of_node_scoped(parent, child)				\
	for (struct device_node *child __free(device_node) =			\
	     of_get_next_child(parent, NULL);					\
	     child != NULL; child = of_get_next_child(parent, child))
#endif
#ifndef for_each_available_child_of_node_scoped
#define for_each_available_child_of_node_scoped(parent, child)			\
	for (struct device_node *child __free(device_node) =			\
	     of_get_next_available_child(parent, NULL);				\
	     child != NULL; child = of_get_next_available_child(parent, child))
#endif
#ifndef of_property_for_each_u32
#define of_property_for_each_u32(np, propname, u)				\
	for (struct { const struct property *prop; const __be32 *item; } _it =	\
		{ .prop = of_find_property(np, propname, NULL),			\
		  .item = of_prop_next_u32(_it.prop, NULL, &u) };		\
	     _it.item; _it.item = of_prop_next_u32(_it.prop, _it.item, &u))
#endif

#include <linux/completion.h>
#ifndef DECLARE_COMPLETION_ONSTACK
#define DECLARE_COMPLETION_ONSTACK(c)	struct completion c; init_completion(&c)
#endif

/* linux/iommu.h protection bits (the DART backend, m1-dcp-dart, uses Linux's values) */
#ifndef IOMMU_READ
#define IOMMU_READ		(1 << 0)
#define IOMMU_WRITE		(1 << 1)
#endif

/* device links are not modelled: flags accepted and ignored by device_link_add() */
#ifndef DL_FLAG_AUTOREMOVE_CONSUMER
#define DL_FLAG_AUTOREMOVE_CONSUMER	(1 << 1)
#define DL_FLAG_AUTOREMOVE_SUPPLIER	(1 << 4)
#endif

#ifndef IRQF_NO_AUTOEN
#define IRQF_NO_AUTOEN		0x00080000
#endif
#ifndef IRQF_TRIGGER_RISING
#define IRQF_TRIGGER_RISING	0x00000001
#define IRQF_TRIGGER_FALLING	0x00000002
#endif
#ifndef IRQF_ONESHOT
#define IRQF_ONESHOT		0x00002000
#endif

#ifndef ARCH_DMA_MINALIGN
#define ARCH_DMA_MINALIGN	128	/* arm64 */
#endif

#endif
