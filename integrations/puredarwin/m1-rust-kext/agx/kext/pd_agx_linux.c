// SPDX-License-Identifier: GPL-2.0
/*
 * pd_agx_linux.c - PDAgx: what the Asahi driver finds in a Linux device tree and kernel
 * and does not find on PureDarwin, supplied from the LinuxKPI side without touching the
 * driver.
 *
 *  - the device, right before the driver's probe (the layer's pd_platform_pre_probe):
 *    the RTKit port gets its IOService, GFX is checked to be powered (the driver's first
 *    act is a register write in the SGX window; PDGpuDT powered the domain before it
 *    published the node this kext matches, this is the check that it still is), and the
 *    GPU's mailbox is named (Apple's tree has no mboxes phandle).
 *  - drivers/gpu/drm/asahi/sysfs.c: device attributes. The layer has no sysfs
 *    (device_create_file() fails with -ENOMEM: its sysctl backing is absent), and the
 *    driver's probe fails when registration fails. PD_STUB below.
 */
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "pd_rtkit_platform.h"
#include "pd_rtkit_compat.h"

void kprintf(const char *, ...);
extern int (*pd_platform_pre_probe)(struct platform_device *pdev);
extern int (*pd_driver_vsnprintf)(char *, size_t, const char *, va_list);
int pd_drm_vsnprintf(char *, size_t, const char *, va_list);
/* PDAgxHost.cpp: the PDAgx IOService attached to the nub */
void *pd_agx_host_of_nub(void *nub);
/* lkpi/helpers/pd_device.c, in the Rust object */
void pd_dev_printk_selftest(const struct device *dev);

#define PD_AGX_MBOX	"/arm-io/gfx-asc"

static int
pd_agx_pre_probe(struct platform_device *pdev)
{
	uint32_t raw = 0;
	void *host;
	int ret;

	if (pdev->dev.of_node == NULL ||
	    !of_device_is_compatible(pdev->dev.of_node, "apple,agx-t8103"))
		return (0);

	host = pd_agx_host_of_nub(pdev->pd_nub);
	if (host == NULL) {
		kprintf("PDAgx: pre-probe FAILED: no PDAgx service on the nub\n");
		return (-ENODEV);
	}
	ret = pd_rtkit_platform_init(host);
	if (ret != 0) {
		kprintf("PDAgx: pre-probe FAILED: pd_rtkit_platform_init %d\n", ret);
		return (ret);
	}
	pd_rtkit_platform_set_log_prefix("PDAgx: rtkit: ");

	ret = pd_rtkit_platform_power_state(PD_AGX_MBOX, &raw);
	kprintf("PDAgx: GFX power state 0x%08x (%s)\n", raw,
	    ret == 1 ? "powered" : ret == 0 ? "NOT powered" : "could not be read");
	if (ret != 1) {
		kprintf("PDAgx: pre-probe FAILED: GFX must be on before the driver touches SGX/ASC "
		    "(boot with pdgpudt=2, PDGpuDT powers it)\n");
		return (ret < 0 ? ret : -ENODEV);
	}

	/*
	 * The driver reports every stage with dev_info!, which ends in this helper. One line
	 * through it from C, so a boot log shows whether that path prints: expected
	 * "asahi gpu@206400000: self-test".
	 */
	kprintf("PDAgx: _dev_printk self-test, the next line should start \"asahi %s: \"\n",
	    dev_name(&pdev->dev));
	pd_dev_printk_selftest(&pdev->dev);

	ret = pd_apple_mbox_bind(&pdev->dev, 0, PD_AGX_MBOX);
	kprintf("PDAgx: mailbox 0 of %s is %s (%d)\n", dev_name(&pdev->dev), PD_AGX_MBOX, ret);
	return (ret);
}

void
pd_agx_linux_init(void)
{
	/* LinuxKPI built with PD_LKPI_DRM prints through this (Linux %p extensions) */
	pd_driver_vsnprintf = pd_drm_vsnprintf;
	pd_platform_pre_probe = pd_agx_pre_probe;
}

/*
 * PD_STUB (owner: layer, sysfs): asahi_sysfs_register()/_register_idle()/_unregister() of
 * drivers/gpu/drm/asahi/sysfs.c. The attributes agx_stats, gpu_memory (and G17's
 * idle_off_delay_ms) do not exist on PureDarwin; nothing reads the snapshot or the ledger.
 * The handle is a real allocation so that unregister has something to own, as in sysfs.c.
 */
struct pd_agx_sysfs {
	const void *snapshot;
	const void *memory;
};

int
asahi_sysfs_register(struct device *dev, const void *snapshot, int export_enabled,
    const void *memory, void **handle)
{
	struct pd_agx_sysfs *s;

	if (!dev || !handle || !memory || (export_enabled && !snapshot))
		return (-ENODEV);
	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (s == NULL)
		return (-ENOMEM);
	s->snapshot = snapshot;
	s->memory = memory;
	*handle = s;
	kprintf("PDAgx: PD_STUB: no sysfs here, agx_stats and gpu_memory are not exported\n");
	return (0);
}

int
asahi_sysfs_register_idle(struct device *dev, u32 *word)
{
	return (-ENODEV);
}

void
asahi_sysfs_unregister(struct device *dev, void *handle)
{
	kfree(handle);
}

/*
 * memset_pattern16(): a libcall on Darwin targets that the compilers (clang at -Os, rustc)
 * may emit for a loop storing a 16-byte pattern. xnu does not export one to kexts.
 * optnone: must not be turned back into a call to itself.
 */
__attribute__((optnone)) void
memset_pattern16(void *b, const void *pattern16, size_t len)
{
	unsigned char *d = b;
	const unsigned char *p = pattern16;
	size_t i;

	for (i = 0; i < len; i++)
		d[i] = p[i & 15];
}
