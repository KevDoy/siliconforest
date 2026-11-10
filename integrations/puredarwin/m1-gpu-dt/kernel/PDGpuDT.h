/* SPDX-License-Identifier: MIT */
/*
 * PDGpuDT: builds the Linux-style GPU device-tree nodes for T8103 from Apple's
 * device tree, logs a summary, and (pdgpudt=2) publishes them in gIODTPlane.
 */
#ifndef PD_GPU_DT_H
#define PD_GPU_DT_H

#include <IOKit/IOService.h>

extern "C" {
#include "gpudt.h"
}

class PDGpuDT : public IOService
{
    OSDeclareDefaultStructors(PDGpuDT);

public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;

private:
    bool publish(const struct gpudt_tree *tree, bool ofHasStringProps);
};

#endif
