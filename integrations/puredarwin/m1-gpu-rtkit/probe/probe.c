// SPDX-License-Identifier: GPL-2.0
/*
 * probe.c - PDRtkitProbe mode 2, LinuxKPI side: the calls the Asahi GPU driver
 * makes into the RTKit library before it touches a GPU endpoint, and nothing
 * after them.
 *
 *   Linux (sources/gpu-plan/linux-aurora)                    here
 *   rust/kernel/soc/apple/rtkit.rs  RtKit::new()             apple_rtkit_init(dev, cookie, NULL, 0, &ops)
 *   drivers/gpu/drm/asahi/gpu.rs    GpuManager::init()       apple_rtkit_boot(rtk)
 *     rtk.start_endpoint(EP_FIRMWARE / EP_DOORBELL)          NOT done
 *     rtk.send_message(EP_FIRMWARE, MSG_INIT | initdata)     NOT done
 *
 * The ops are the ones rtkit.rs builds for the G13 GpuManager: flags 0,
 * crashed, recv_message, recv_message_early, shmem_setup, shmem_destroy. On
 * Linux shmem_setup allocates a GEM object and maps it into the firmware's
 * kernel address space through the UAT page tables (gpu.rs shmem_alloc). There
 * is no UAT here, so shmem_setup refuses: the firmware's buffer requests are
 * logged and left unanswered rather than answered with an address the
 * firmware cannot use.
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/soc/apple/rtkit.h>

#include "pd_rtkit_compat.h"
#include "pd_rtkitprobe.h"

#define plog(fmt, ...)	pd_rtkit_printk(NULL, fmt, ##__VA_ARGS__)

struct probe_state {
	unsigned int buffer_requests;
	unsigned int app_messages;
	unsigned int crashes;
};

static struct probe_state probe_state;

static void probe_crashed(void *cookie, const void *crashlog, size_t crashlog_size)
{
	struct probe_state *st = cookie;

	st->crashes++;
	plog("ops.crashed: the coprocessor reported a crash (crashlog %zu bytes)", crashlog_size);
}

static void probe_recv_message(void *cookie, u8 endpoint, u64 message)
{
	struct probe_state *st = cookie;

	st->app_messages++;
	plog("ops.recv_message: endpoint 0x%02x message 0x%016llx (not expected: no application endpoint was started)",
	     endpoint, (unsigned long long)message);
}

/* rtkit.rs installs this for every client; its default returns false */
static bool probe_recv_message_early(void *cookie, u8 endpoint, u64 message)
{
	return false;
}

static int probe_shmem_setup(void *cookie, struct apple_rtkit_shmem *bfr)
{
	struct probe_state *st = cookie;

	st->buffer_requests++;
	plog("ops.shmem_setup: firmware asks for a 0x%zx byte buffer (iova 0x%llx): refused, no UAT mapping to give it",
	     bfr->size, (unsigned long long)bfr->iova);
	return -ENOMEM;
}

static void probe_shmem_destroy(void *cookie, struct apple_rtkit_shmem *bfr)
{
}

static const struct apple_rtkit_ops probe_ops = {
	.flags = 0,
	.crashed = probe_crashed,
	.recv_message = probe_recv_message,
	.recv_message_early = probe_recv_message_early,
	.shmem_setup = probe_shmem_setup,
	.shmem_destroy = probe_shmem_destroy,
};

int pd_rtkitprobe_rtkit_boot(void)
{
	struct apple_rtkit *rtk;
	struct device *dev;
	char eps[160];
	size_t n = 0;
	int ep, ret;

	pd_rtkit_enter();
	/* dev_dbg() of mailbox.c and rtkit.c: HELLO versions, endpoint map, power states */
	pd_rtkit_debug = 1;

	/* stands in for Linux's gpu@206400000 platform device; its mboxes = <&agx_mbox> */
	dev = pd_rtkit_device_create("gpu", NULL);
	if (!dev)
		return -ENOMEM;
	ret = pd_apple_mbox_bind(dev, 0, "/arm-io/gfx-asc");
	if (ret) {
		plog("step 4: could not bind the mailbox: %d", ret);
		return ret;
	}

	plog("step 4: apple_rtkit_init(dev, cookie, NULL, 0, ops), as rtkit.rs RtKit::new: mailbox interrupts on, receive armed");
	rtk = apple_rtkit_init(dev, &probe_state, NULL, 0, &probe_ops);
	if (IS_ERR(rtk)) {
		ret = PTR_ERR(rtk);
		plog("step 4: apple_rtkit_init failed: %d", ret);
		return ret;
	}
	plog("step 4: done");

	plog("step 5: apple_rtkit_boot(), as gpu.rs init(): wait 1 s each for the endpoint map and the IOP power ack, then set AP power ON");
	ret = apple_rtkit_boot(rtk);
	plog("step 5: apple_rtkit_boot returned %d; is_running %d, is_crashed %d", ret,
	     apple_rtkit_is_running(rtk), apple_rtkit_is_crashed(rtk));

	eps[0] = '\0';
	for (ep = 0; ep < 0x100 && n + 6 < sizeof(eps); ep++)
		if (apple_rtkit_has_endpoint(rtk, ep))
			n += snprintf(eps + n, sizeof(eps) - n, " 0x%02x", ep);
	plog("endpoints known (0x00 is always set; the rest come from the firmware's map):%s", eps);
	plog("buffer requests refused %u, application messages %u, crash reports %u",
	     probe_state.buffer_requests, probe_state.app_messages, probe_state.crashes);

	if (ret == 0)
		plog("RTKit reports ready (IOP and AP power states ON). Stopping here: endpoints 0x20/0x21 are NOT started, no initdata is sent");
	else
		plog("RTKit boot did not complete (%d). Stopping here", ret);
	plog("the RTKit instance stays attached: firmware syslog traffic is acknowledged, a crash is reported");
	return ret;
}
