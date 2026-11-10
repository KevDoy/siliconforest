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

/*
 * ---- DCP mode (pdrtkitprobe=3), SF-H item H1 ----
 *
 * What Linux's display coprocessor driver does to the DCP's RTKit before any display
 * endpoint exists, and nothing after it (drivers/gpu/drm/apple/dcp.c, dcp_comp_bind()):
 *
 *   Linux                                                         here
 *   CPU_CONTROL |= RUN                              dcp.c:2533    not written: the caller stops unless RUN is already set
 *   devm_apple_rtkit_init(dev, dcp, "mbox", 0, ops) dcp.c:2538    apple_rtkit_init(dev, cookie, NULL, 0, ops)
 *   apple_rtkit_wake(dcp->rtk)                      dcp.c:2543    apple_rtkit_wake(rtk)
 *   dcp_start(): systemep, [ibootep, dptxep], iomfb dcp.c:1883    NOT done
 *
 * The ops are Linux's rtkit_ops for DCP (dcp.c:1466): flags 0, crashed, recv_message,
 * shmem_setup, shmem_destroy, no recv_message_early. Linux's shmem_setup either maps a
 * buffer the firmware placed itself (request carries an iova: iommu_iova_to_phys() through
 * the dcp DART, memremap, and RTKit sends no reply) or allocates one through the dcp DART
 * (iova 0: dma_alloc_coherent, and RTKit replies with the new iova). There is no DART
 * driver here. Both kinds are refused, so the first puts nothing different on the wire
 * than Linux does and the second leaves the firmware without its reply: that request,
 * logged with endpoint, size and iova, is what H2 has to provide.
 */
#include <linux/spinlock.h>
#include <linux/soc/apple/mailbox.h>

#define dlog(fmt, ...)	pd_rtkit_printk(NULL, "dcp " fmt, ##__VA_ARGS__)

/* received messages printed in full; after that only counted */
#define DCP_TRACE_MAX	96

struct dcp_probe_state {
	void (*orig_rx)(struct apple_mbox *mbox, struct apple_mbox_msg msg, void *cookie);
	unsigned int rx_total;
	unsigned int endpoints_announced;
	unsigned int buf_fw_placed;	/* request with an iova: no reply expected */
	unsigned int buf_need_alloc;	/* request without: the AP must allocate, map and reply */
	unsigned int shmem_calls;
	unsigned int app_messages;
	unsigned int crashes;
};

static struct dcp_probe_state dcp_state;

/* rtkit.c system endpoints; drivers/gpu/drm/apple/dcp-internal.h:66-75 */
static const char *dcp_ep_name(unsigned int ep)
{
	switch (ep) {
	case 0x00: return "management";
	case 0x01: return "crashlog";
	case 0x02: return "syslog";
	case 0x03: return "debug";
	case 0x04: return "ioreport";
	case 0x08: return "oslog";
	case 0x0a: return "tracekit";
	case 0x20: return "system";
	case 0x21: return "test";
	case 0x22: return "dcp-expert";
	case 0x23: return "disp0/iBoot";
	case 0x28: return "dpavserv";
	case 0x29: return "av";
	case 0x2a: return "dptx";
	case 0x2b: return "hdcp";
	case 0x2d: return "remote-alloc";
	case 0x37: return "iomfb";
	default: return "unknown";
	}
}

static void dcp_note_buffer_request(unsigned int ep, u64 size, u64 iova)
{
	if (iova) {
		dcp_state.buf_fw_placed++;
		dlog("buffer-request: ep 0x%02x (%s) size 0x%llx iova 0x%llx: placed by the firmware; Linux maps it through the dcp DART and sends no reply; not mapped here",
		     ep, dcp_ep_name(ep), (unsigned long long)size, (unsigned long long)iova);
	} else {
		dcp_state.buf_need_alloc++;
		dlog("buffer-request: ep 0x%02x (%s) size 0x%llx iova 0: the AP must allocate, map through the dcp DART and reply; NOT answered (needs H2)",
		     ep, dcp_ep_name(ep), (unsigned long long)size);
	}
}

/*
 * Sits between mailbox.c and rtkit.c's receive function: prints the message, then passes
 * it on unchanged. Runs on the mailbox interrupt thread with rx_lock held, like the
 * function it wraps. Field layouts are rtkit.c's (APPLE_RTKIT_MGMT_*, _BUFFER_REQUEST_*,
 * _OSLOG_*).
 */
static void dcp_rx_trace(struct apple_mbox *mbox, struct apple_mbox_msg msg, void *cookie)
{
	unsigned int ep = msg.msg1 & 0xff;
	u64 m = msg.msg0;
	unsigned int n = ++dcp_state.rx_total;
	unsigned int type = (m >> 52) & 0xff;
	unsigned int i;

	if (n <= DCP_TRACE_MAX)
		dlog("rx #%u: ep 0x%02x (%s) msg 0x%016llx", n, ep, dcp_ep_name(ep),
		     (unsigned long long)m);
	else if (n == DCP_TRACE_MAX + 1)
		dlog("rx: more than %d messages, the rest are only counted", DCP_TRACE_MAX);

	if (ep == 0x00 && type == 1) {
		dlog("hello: firmware speaks RTKit protocol versions %u..%u",
		     (unsigned int)(m & 0xffff), (unsigned int)((m >> 16) & 0xffff));
	} else if (ep == 0x00 && type == 8) {
		unsigned int base = (m >> 32) & 0x7;
		u32 bitmap = (u32)m;

		for (i = 0; i < 32; i++) {
			if (!(bitmap & (1u << i)))
				continue;
			dcp_state.endpoints_announced++;
			dlog("endpoint: 0x%02x (%s)", 32 * base + i, dcp_ep_name(32 * base + i));
		}
		if (m & (1ULL << 51))
			dlog("endpoint-map: complete, %u endpoints announced",
			     dcp_state.endpoints_announced);
	} else if (ep == 0x00 && type == 7) {
		dlog("power: IOP power state ack 0x%x", (unsigned int)(m & 0xffff));
	} else if (ep == 0x00 && type == 0xb) {
		dlog("power: AP power state ack 0x%x", (unsigned int)(m & 0xffff));
	} else if ((ep == 0x01 || ep == 0x02 || ep == 0x04) && type == 1) {
		dcp_note_buffer_request(ep, ((m >> 44) & 0xff) << 12, m & ((1ULL << 44) - 1));
	} else if (ep == 0x08 && ((m >> 56) & 0xff) == 1) {
		dcp_note_buffer_request(ep, (m >> 36) & 0xfffff, (m & ((1ULL << 36) - 1)) << 12);
	}

	if (dcp_state.orig_rx)
		dcp_state.orig_rx(mbox, msg, cookie);
}

static void dcp_crashed(void *cookie, const void *crashlog, size_t crashlog_size)
{
	struct dcp_probe_state *st = cookie;

	st->crashes++;
	dlog("CRASH: the coprocessor reported a crash (crashlog %zu bytes)", crashlog_size);
}

static void dcp_recv_message(void *cookie, u8 endpoint, u64 message)
{
	struct dcp_probe_state *st = cookie;

	st->app_messages++;
	dlog("app-message: endpoint 0x%02x (%s) message 0x%016llx; not answered (no endpoint above 0x1f was started)",
	     endpoint, dcp_ep_name(endpoint), (unsigned long long)message);
}

/* The request itself is printed, with its endpoint, by dcp_rx_trace(). */
static int dcp_shmem_setup(void *cookie, struct apple_rtkit_shmem *bfr)
{
	struct dcp_probe_state *st = cookie;

	st->shmem_calls++;
	return -ENOMEM;
}

static void dcp_shmem_destroy(void *cookie, struct apple_rtkit_shmem *bfr)
{
}

static const struct apple_rtkit_ops dcp_probe_ops = {
	.flags = 0,
	.crashed = dcp_crashed,
	.recv_message = dcp_recv_message,
	.shmem_setup = dcp_shmem_setup,
	.shmem_destroy = dcp_shmem_destroy,
};

int pd_rtkitprobe_dcp_wake(void)
{
	struct apple_rtkit *rtk;
	struct apple_mbox *mbox;
	struct device *dev;
	unsigned long flags;
	char eps[200];
	size_t n = 0;
	int ep, ret;

	pd_rtkit_enter();
	pd_rtkit_debug = 1;

	/* stands in for Linux's dcp@231c00000 platform device; its mboxes = <&dcp_mbox> */
	dev = pd_rtkit_device_create("dcp", NULL);
	if (!dev)
		return -ENOMEM;
	ret = pd_apple_mbox_bind(dev, 0, "/arm-io/dcp");
	if (ret) {
		dlog("rtkit-init: could not bind the mailbox: %d", ret);
		return ret;
	}

	rtk = apple_rtkit_init(dev, &dcp_state, NULL, 0, &dcp_probe_ops);
	if (IS_ERR(rtk)) {
		ret = PTR_ERR(rtk);
		dlog("rtkit-init: apple_rtkit_init failed: %d", ret);
		return ret;
	}

	/* the same provider object apple_rtkit_init() attached to */
	mbox = apple_mbox_get(dev, 0);
	if (IS_ERR(mbox)) {
		ret = PTR_ERR(mbox);
		dlog("rtkit-init: mailbox lookup for the trace failed: %d; going no further", ret);
		return ret;
	}
	spin_lock_irqsave(&mbox->rx_lock, flags);
	dcp_state.orig_rx = mbox->rx;
	mbox->rx = dcp_rx_trace;
	spin_unlock_irqrestore(&mbox->rx_lock, flags);
	dlog("rtkit-init: ok, mailbox interrupts on, receive armed and traced; nothing sent yet");

	dlog("wake: apple_rtkit_wake(): send IOP power state 0x220, answer HELLO and the endpoint map, start the system endpoints (below 0x20), wait for the IOP power ack, set AP power 0x20");
	ret = apple_rtkit_wake(rtk);
	dlog("wake: apple_rtkit_wake returned %d; is_running %d, is_crashed %d", ret,
	     apple_rtkit_is_running(rtk), apple_rtkit_is_crashed(rtk));

	eps[0] = '\0';
	for (ep = 0; ep < 0x100 && n + 6 < sizeof(eps); ep++)
		if (apple_rtkit_has_endpoint(rtk, ep))
			n += snprintf(eps + n, sizeof(eps) - n, " 0x%02x", ep);
	dlog("endpoints:%s", eps);
	dlog("summary: %u messages received, %u endpoints announced, buffer requests: %u firmware-placed, %u needing an AP allocation (%u shmem_setup calls refused), %u application messages, %u crash reports",
	     dcp_state.rx_total, dcp_state.endpoints_announced, dcp_state.buf_fw_placed,
	     dcp_state.buf_need_alloc, dcp_state.shmem_calls, dcp_state.app_messages,
	     dcp_state.crashes);
	return ret;
}
