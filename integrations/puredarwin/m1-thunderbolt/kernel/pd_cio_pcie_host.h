/* SPDX-License-Identifier: GPL-2.0 */
/* Native PCIe-C host adapter, following PDApcieT8103Host / pcie-apple.c.
 * Copyright (C) 2021 Alyssa Rosenzweig <alyssa@rosenzweig.io>
 * Copyright (C) 2021 Google LLC
 * Copyright (C) 2021 Corellium LLC
 * Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 */
#ifndef PD_CIO_PCIE_HOST_H
#define PD_CIO_PCIE_HOST_H
#include <IOKit/pci/IOPCIBridge.h>
#include "pd_soc.h"
#include "pd_pciec_config.h"
#if PD_CIO_PCI_MSI
#include "pd_cio_pci_msi.h"
#include "pd_pciec_msi_setup.h"
#endif
/* Enumeration-only native adapter, wired only in the explicit CM profile.
 * The caller must activate the real tunnel, cold-init/start PCIe-C, retain
 * its actual power/PHY ownership and omit PCI endpoint personalities before
 * starting this object. A registry marker alone is NOT binding/DMA proof.
 * Default profile supplies no endpoint mapper, MSI or driver admission.
 * Explicit SDK-only MSI variant still strips every bus-master request.
 * All config calls are thread-context only. Block configuration before
 * power/link teardown; failed start/stop retains ownership until real child
 * teardown and final object release. No DMA drain is inferred by this API.
 */
class PDCIOPCIHost : public IOPCIBridge {
    OSDeclareDefaultStructors(PDCIOPCIHost)
    IOService *leaseOwner;
    void *powerContext;
    bool (*powerProof)(void *);
    IOLock *accessLock;
    PDMmio config,port,powerGate;
    pd_pciec_config_view view;
    bool startAttempted,stopped;
#if PD_CIO_PCI_MSI
    PDCIOPCIMSIController *msi;
    UInt32 interruptAdmitted;
    static bool interruptPower(void *);
    static int msiRead(void *,uint32_t,uint32_t *);
    static int msiWrite(void *,uint32_t,uint32_t);
    bool msiPhysicalAccess();
#endif
#if PD_CIO_I210_NETWORK
    struct Grant { IOPCIDevice *device; UInt32 id,code; UInt8 bus,slot,function; } grants[8];
    UInt32 grantCount,bmeActive;
    IOPCIDevice *networkEndpoint;
    bool networkAttempted;
    IOReturn admitI210(IOPCIDevice *);
    bool busMasterAllowed(IOPCIAddressSpace);
#endif
    bool physicalPower(); // Always-on PMGR proof before any PCIe-C access.
    volatile uint8_t *address(IOPCIAddressSpace,UInt8,unsigned);
    UInt32 readConfig(IOPCIAddressSpace,UInt8,unsigned);
    void writeConfig(IOPCIAddressSpace,UInt8,unsigned,UInt32);
public:
    bool initForEnumeration(IOService *,void *,bool (*)(void *));
    bool start(IOService *) override;
    void stop(IOService *) override;
    void free() override;
#if PD_CIO_PCI_MSI
    IOReturn callPlatformFunction(const OSSymbol *,bool,void *,void *,void *,void *) override;
#endif
#if PD_CIO_I210_NETWORK
    int bindI210Access(IOPCIDevice *);
    int publishI210Resources(IOPCIDevice *);
#endif
    IOReturn blockConfiguration();
    bool configure(IOService *) override;
    IODeviceMemory *ioDeviceMemory() override;
    UInt8 firstBusNum() override;
    UInt8 lastBusNum() override;
    IOPCIAddressSpace getBridgeSpace() override;
    UInt32 configRead32(IOPCIAddressSpace,UInt8) override;
    UInt16 configRead16(IOPCIAddressSpace,UInt8) override;
    UInt8 configRead8(IOPCIAddressSpace,UInt8) override;
    void configWrite32(IOPCIAddressSpace,UInt8,UInt32) override;
    void configWrite16(IOPCIAddressSpace,UInt8,UInt16) override;
    void configWrite8(IOPCIAddressSpace,UInt8,UInt8) override;
};
#endif
