// SPDX-License-Identifier: GPL-2.0
// glue.c - PDRustLKPI: the FreeBSD-side entry of the kext. No hardware driver: it brings the
// layer up the way a LinuxKPI DRM kext does and then runs the Rust self-test.

#include <sys/bus.h>

HAIKU_FBSD_DRIVERS_GLUE(rustlkpi);
// the linux workqueues are FreeBSD taskqueues
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 1;
}

void kprintf(const char *, ...);
int pd_fbsd_init(void);
unsigned int pd_rust_lkpi_run(void);

// no device to probe
void
__haiku_init_hardware()
{
}

// no network interface comes out of this kext
void pdos_net_input(void *ifp_cookie, const void *frame, size_t len) { }
void pdos_net_input_chain(void *ifp_cookie, void *fbsd_mbuf) { }
void pdos_net_link(void *ifp_cookie, int up, uint64_t baudrate) { }

// Called by PDRustLKPI::start() after pdos_xnu_init(). Returns the number of failed checks,
// or -1 when the layer itself did not come up.
int
pd_rustlkpi_start(void)
{
	int status = pd_fbsd_init();

	if (status != 0) {
		kprintf("PDRustLKPI: pd_fbsd_init failed: %d\n", status);
		return -1;
	}
	kprintf("PDRustLKPI: LinuxKPI runtime up (mutexes, callouts, taskqueues, sysinits)\n");
	return (int)pd_rust_lkpi_run();
}
