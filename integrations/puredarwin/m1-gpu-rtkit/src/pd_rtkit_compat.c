// SPDX-License-Identifier: GPL-2.0-only OR MIT
/*
 * pd_rtkit_compat.c - the code behind pd_rtkit_compat.h: logging, a bare
 * struct device, interrupt handler thunks, of_machine_is_compatible().
 * LinuxKPI side; reaches Darwin only through pd_rtkit_platform.h.
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include "pd_rtkit_compat.h"

/* pd_linux_of.c of the layer: the device_node mirroring an IORegistryEntry */
struct device_node *pd_of_node_for_entry(void *entry);

int pd_rtkit_debug;

#ifdef PD_LKPI_DRM
/* the layer's printf with Linux's %p extensions (%pad is used by rtkit.c) */
int pd_drm_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
#define	pd_rtkit_vsnprintf	pd_drm_vsnprintf
#else
#define	pd_rtkit_vsnprintf	vsnprintf
#endif

void
pd_rtkit_printk(const struct device *dev, const char *fmt, ...)
{
	char line[320];
	const char *name = NULL;
	size_t n = 0;
	va_list ap;

	if (dev != NULL)
		name = kobject_name(&dev->kobj);
	if (name != NULL && name[0] != '\0') {
		n = strlcpy(line, name, sizeof(line) - 2);
		if (n > sizeof(line) - 3)
			n = sizeof(line) - 3;
		line[n++] = ':';
		line[n++] = ' ';
	}
	va_start(ap, fmt);
	pd_rtkit_vsnprintf(line + n, sizeof(line) - n, fmt, ap);
	va_end(ap);
	/* one line per call: the platform adds the newline */
	n = strlen(line);
	while (n > 0 && line[n - 1] == '\n')
		line[--n] = '\0';
	pd_rtkit_platform_log(line);
}

static void
pd_rtkit_device_release(struct device *dev)
{
}

struct device *
pd_rtkit_device_create(const char *name, void *dt_entry)
{
	struct device *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL)
		return (NULL);
	kobject_init(&dev->kobj, &linux_dev_ktype);
	kobject_set_name(&dev->kobj, "%s", name);
	spin_lock_init(&dev->devres_lock);
	INIT_LIST_HEAD(&dev->devres_head);
	INIT_LIST_HEAD(&dev->irqents);
	dev->parent = &linux_root_device;
	dev->of_node = dt_entry != NULL ? pd_of_node_for_entry(dt_entry) : NULL;
	dev->release = pd_rtkit_device_release;
	return (dev);
}

/*
 * Interrupt thunks: the platform calls int (*)(void *), Linux handlers are
 * irqreturn_t (*)(int, void *). One per handle, found again by handle on free.
 */
struct pd_rtkit_irq_thunk {
	int		irq;
	irq_handler_t	handler;
	void		*data;
};

#define	PD_RTKIT_MAX_THUNKS	16
static struct pd_rtkit_irq_thunk *pd_rtkit_thunks[PD_RTKIT_MAX_THUNKS];

static int
pd_rtkit_irq_thunk_fn(void *arg)
{
	struct pd_rtkit_irq_thunk *t = arg;

	pd_rtkit_enter();
	return (t->handler(t->irq, t->data) != IRQ_NONE);
}

/* Like request_irq(..., IRQF_NO_AUTOEN, ...): the line stays masked until enable_irq(). */
int
pd_rtkit_request_irq(int irq, irq_handler_t handler, const char *name, void *data)
{
	struct pd_rtkit_irq_thunk *t;
	int i, ret;

	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return (-ENOMEM);
	t->irq = irq;
	t->handler = handler;
	t->data = data;
	for (i = 0; i < PD_RTKIT_MAX_THUNKS; i++) {
		if (__sync_bool_compare_and_swap(&pd_rtkit_thunks[i], NULL, t))
			break;
	}
	if (i == PD_RTKIT_MAX_THUNKS) {
		kfree(t);
		return (-ENOSPC);
	}
	ret = pd_rtkit_platform_irq_request(irq, pd_rtkit_irq_thunk_fn, t, name);
	if (ret != 0) {
		pd_rtkit_thunks[i] = NULL;
		kfree(t);
	}
	return (ret);
}

void
pd_rtkit_free_irq(int irq, void *data)
{
	int i;

	/* the platform waits for a running handler before it returns */
	pd_rtkit_platform_irq_free(irq);
	for (i = 0; i < PD_RTKIT_MAX_THUNKS; i++) {
		struct pd_rtkit_irq_thunk *t = pd_rtkit_thunks[i];

		if (t != NULL && t->irq == irq && t->data == data) {
			pd_rtkit_thunks[i] = NULL;
			kfree(t);
			return;
		}
	}
}

/*
 * "apple,t8103" is asked of Apple's tree as "arm-io,t8103", the compatible of
 * /arm-io. Anything that is not an "apple,<soc>" string is not known here.
 */
bool
pd_rtkit_of_machine_is_compatible(const char *compat)
{
	char adt[48];

	if (compat == NULL || strncmp(compat, "apple,t", 7) != 0)
		return (false);
	snprintf(adt, sizeof(adt), "arm-io,%s", compat + 6);
	return (of_find_compatible_node(NULL, NULL, adt) != NULL);
}
