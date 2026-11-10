// SPDX-License-Identifier: GPL-2.0
/*
 * PDRustLKPI: the IOKit face of the kext. Matches IOResources, initialises the fbsdkpi
 * layer and runs the Rust `kernel`-crate self-test once (glue.c, rust/lib.rs).
 * Replaces fbsdkpi/xnu/PDDRMPlatformDevice.cpp, which would want a device-tree node.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include "pd_fbsd_os.h"
#include "pd_fbsd_xnu.h"

extern "C" int pd_rustlkpi_start(void);

class PDRustLKPI : public IOService
{
    OSDeclareDefaultStructors(PDRustLKPI)

public:
    bool start(IOService *provider) override;
};

OSDefineMetaClassAndStructors(PDRustLKPI, IOService)

bool PDRustLKPI::start(IOService *provider)
{
    if (!IOService::start(provider))
        return false;

    IOLog("PDRustLKPI: start\n");
    pdos_xnu_init();
    int failures = pd_rustlkpi_start();
    if (failures == 0)
        IOLog("PDRustLKPI: ALL OK\n");
    else
        IOLog("PDRustLKPI: FAILED (%d)\n", failures);
    setProperty("PDRustLKPIResult", failures == 0 ? "ok" : "failed");
    registerService();
    return true;
}
