// SPDX-License-Identifier: GPL-2.0-only OR MIT
// glue.c - PDDcp: the FreeBSD-side entry of the kext. The driver's two platform drivers
// (dcp.c "apple,dcp", apple_drv.c "apple,display-subsystem") register themselves from
// their module_init, which LinuxKPI runs as a SYSINIT; this only installs the pre-probe
// hook before the layer offers the first device-tree nub to the platform bus.

#include <sys/bus.h>

HAIKU_FBSD_DRIVERS_GLUE(dcp);
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 1;
}

void kprintf(const char *, ...);
void pd_dcp_linux_init(void);
extern int pd_dcp_mode;

void
__haiku_init_hardware()
{
	static int done;

	if (done)
		return;
	done = 1;
	pd_dcp_linux_init();
	kprintf("PDDcp: LinuxKPI runtime up, pddcp=%d\n", pd_dcp_mode);
}

// no network interface comes out of this kext
void pdos_net_input(void *ifp_cookie, const void *frame, size_t len) { }
void pdos_net_input_chain(void *ifp_cookie, void *fbsd_mbuf) { }
void pdos_net_link(void *ifp_cookie, int up, uint64_t baudrate) { }
