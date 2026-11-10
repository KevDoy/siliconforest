/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Apple T8103 PCIe host bridge for PureDarwin (J293 port 0, BCM4378).
 * Port handling follows Linux drivers/pci/controller/pcie-apple.c
 * (omacom/linux-aurora):
 *   Copyright (C) 2021 Alyssa Rosenzweig <alyssa@rosenzweig.io>
 *   Copyright (C) 2021 Google LLC
 *   Copyright (C) 2021 Corellium LLC
 *   Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 */
#ifndef _PDAPCIET8103_H
#define _PDAPCIET8103_H

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIBridge.h>
#include <IOKit/pci/IOPCIDevice.h>

/* IOResources-matched loader: promotes /arm-io/apcie to an IOPlatformDevice nub. */
class PDApcieT8103Bridge : public IOService
{
    OSDeclareDefaultStructors(PDApcieT8103Bridge)
public:
    IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

/* The host bridge itself, on the apcie nub. */
class PDApcieT8103Host : public IOPCIBridge
{
    OSDeclareDefaultStructors(PDApcieT8103Host)

    IOMemoryMap *ecamMap;
    IOMemoryMap *portMap;
    volatile UInt8 *ecamBase;
    volatile UInt8 *portBase;
    UInt8 busFirst;
    UInt8 busLast;

    volatile UInt8 *configAddress(IOPCIAddressSpace space, UInt8 offset) const;
    UInt32 portRead(UInt32 offset) const;
    /* MSI: message data n on this port arrives as AIC interrupt msiBase + n */
    UInt32 msiBase;
    UInt32 msiCount;
    UInt32 msiDoorbell;
    UInt32 msiUsed;
    IOReturn setupMSI(IOService *client, IOPCIDevice *device, UInt32 *vectorOut);

public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
    void free(void) APPLE_KEXT_OVERRIDE;
    /* "PDApcieSetupMSI" (client service, IOPCIDevice, UInt32 *aicVector): one MSI for a device below this bridge */
    IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                  void *param1, void *param2, void *param3, void *param4) APPLE_KEXT_OVERRIDE;
    IODeviceMemory *ioDeviceMemory(void) APPLE_KEXT_OVERRIDE;

    UInt8 firstBusNum(void) APPLE_KEXT_OVERRIDE;
    UInt8 lastBusNum(void) APPLE_KEXT_OVERRIDE;
    IOPCIAddressSpace getBridgeSpace(void) APPLE_KEXT_OVERRIDE;

    UInt32 configRead32(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
    void configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) APPLE_KEXT_OVERRIDE;
    UInt16 configRead16(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
    void configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) APPLE_KEXT_OVERRIDE;
    UInt8 configRead8(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
    void configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) APPLE_KEXT_OVERRIDE;
};

#endif
