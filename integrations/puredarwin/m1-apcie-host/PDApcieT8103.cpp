// SPDX-License-Identifier: GPL-2.0
/*
 * Apple T8103 PCIe host bridge for PureDarwin (J293 port 0, BCM4378).
 *
 * Register names and the port model follow Linux
 * drivers/pci/controller/pcie-apple.c (omacom/linux-aurora):
 *   Copyright (C) 2021 Alyssa Rosenzweig <alyssa@rosenzweig.io>
 *   Copyright (C) 2021 Google LLC
 *   Copyright (C) 2021 Corellium LLC
 *   Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 *   Author: Alyssa Rosenzweig <alyssa@rosenzweig.io>
 *   Author: Marc Zyngier <maz@kernel.org>
 * The BAR0 window constants follow Linux brcmfmac/pcie.c (ISC, Broadcom).
 *
 * This revision inherits a link that the launcher brought up from the m1n1
 * proxy (pcie_init, SMC radio power, port link sequence). It enumerates and
 * assigns resources; it does not yet own link bring-up, MSI or DART.
 */
#include "PDApcieT8103.h"

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <libkern/OSAtomic.h>
#include <libkern/OSByteOrder.h>
#include <pexpert/pexpert.h>

#define LOG(fmt, ...) IOLog("PDApcieT8103: " fmt "\n", ##__VA_ARGS__)

/* pcie-apple.c */
#define PORT_INTSTAT        0x00100
#define PORT_LINKSTS        0x00208
#define   PORT_LINKSTS_UP   (1u << 0)
#define PORT_APPCLK         0x00800
#define PORT_STATUS         0x00804
#define   PORT_STATUS_READY (1u << 0)
#define PORT_REFCLK         0x00810
#define PORT_PERST          0x00814
#define PORT_RID2SID        0x00828
#define   PORT_RID2SID_VALID     (1u << 31)
#define   PORT_RID2SID_SID_SHIFT 16
#define   PORT_RID2SID_BUS_SHIFT 8

/* ADT reg indices of /arm-io/apcie (m1n1 src/pcie.c) */
#define APCIE_REG_ECAM      0
#define APCIE_REG_PORT0     6

#define ECAM_BUS_SHIFT      20
#define ECAM_DEVICE_SHIFT   15
#define ECAM_FUNCTION_SHIFT 12

/* 32-bit memory window: PCI 0xc0000000 -> CPU 0x6c0000000 through the ADT ranges */
#define APCIE_MEM32_BUS_BASE 0xc0000000ULL
#define APCIE_MEM32_SIZE     0x40000000ULL

/* ---- loader -------------------------------------------------------------- */

#undef super
#define super IOService
OSDefineMetaClassAndStructors(PDApcieT8103Bridge, IOService)

static volatile UInt32 gLoaderClaimed = 0;

IOService *PDApcieT8103Bridge::probe(IOService *provider, SInt32 *score)
{
    int enabled = 0;
    if (!PE_parse_boot_argn("pdapciebridge", &enabled, sizeof(enabled)) || enabled != 1)
        return NULL;
    if (!OSCompareAndSwap(0, 1, &gLoaderClaimed))
        return NULL;
    return super::probe(provider, score);
}

bool PDApcieT8103Bridge::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    IORegistryEntry *entry = IORegistryEntry::fromPath("/arm-io/apcie", gIODTPlane);
    if (!entry) {
        LOG("no /arm-io/apcie in the device tree");
        return false;
    }

    IOService *nub = OSDynamicCast(IOService, entry);
    if (nub) {
        const OSMetaClass *meta = nub->getMetaClass();
        IOService *parent = nub->getProvider();
        LOG("/arm-io/apcie is already a service: class %s name %s provider %s registered=%d",
            meta ? meta->getClassName() : "?", nub->getName(), parent ? parent->getName() : "(none)",
            (int)((nub->getState() & kIOServiceRegisteredState) != 0));
    } else {
        IOPlatformDevice *made = OSTypeAlloc(IOPlatformDevice);
        bool ok = made && made->init(entry, gIODTPlane);
        entry->release();
        entry = NULL;
        if (ok)
            ok = made->attach(getPlatform());
        if (!ok) {
            LOG("could not publish the apcie nub");
            OSSafeReleaseNULL(made);
            return false;
        }
        LOG("published apcie nub %s", made->getName());
        nub = made;
    }

    /* Device tree services on this kernel are not in the service plane; put ours there. */
    if (!nub->inPlane(gIOServicePlane)) {
        bool attached = nub->attach(getPlatform());
        LOG("attached apcie to the platform expert in the service plane: %s", attached ? "ok" : "FAILED");
    }

    /* Attach and start the host bridge directly: no dependence on how the nub matches. */
    PDApcieT8103Host *host = OSTypeAlloc(PDApcieT8103Host);
    bool ok = host && host->init();
    if (ok)
        ok = host->attach(nub);
    if (ok) {
        ok = host->start(nub);
        if (!ok)
            host->detach(nub);
    }
    LOG("host bridge %s", ok ? "started" : "did not start");
    OSSafeReleaseNULL(host);
    nub->release();
    return true;
}

/* ---- host bridge --------------------------------------------------------- */

#undef super
#define super IOPCIBridge
OSDefineMetaClassAndStructors(PDApcieT8103Host, IOPCIBridge)

UInt32 PDApcieT8103Host::portRead(UInt32 offset) const
{
    UInt32 v = OSReadLittleInt32((volatile void *)portBase, offset);
    OSSynchronizeIO();
    return v;
}

bool PDApcieT8103Host::start(IOService *provider)
{
    /* Resolve "reg" through the device tree directly; the nub may not have resources yet. */
    OSArray *regs = IODTResolveAddressing(provider, "reg", NULL);
    IOMemoryDescriptor *ecam = regs ? OSDynamicCast(IOMemoryDescriptor, regs->getObject(APCIE_REG_ECAM)) : NULL;
    IOMemoryDescriptor *port = regs ? OSDynamicCast(IOMemoryDescriptor, regs->getObject(APCIE_REG_PORT0)) : NULL;
    LOG("apcie reg entries: %u", regs ? regs->getCount() : 0);
    UInt32 range[2] = { 0, 0 };
    OSData *busRange = OSDynamicCast(OSData, provider->getProperty("bus-range"));

    if (!ecam || !port) {
        LOG("apcie nub has no ECAM/port0 reg (ecam=%p port=%p)", ecam, port);
        return false;
    }
    LOG("ecam phys=0x%llx len=0x%llx port0 phys=0x%llx len=0x%llx",
        (unsigned long long)ecam->getPhysicalSegment(0, NULL, kIOMemoryMapperNone), (unsigned long long)ecam->getLength(),
        (unsigned long long)port->getPhysicalSegment(0, NULL, kIOMemoryMapperNone), (unsigned long long)port->getLength());

    busFirst = 0;
    busLast = 8;
    if (busRange && busRange->getLength() >= sizeof(range)) {
        memcpy(range, busRange->getBytesNoCopy(), sizeof(range));
        if (range[0] <= range[1] && range[1] <= 255) {
            busFirst = (UInt8)range[0];
            busLast = (UInt8)range[1];
        }
    }
    UInt64 buses = ecam->getLength() >> ECAM_BUS_SHIFT;
    if (buses == 0)
        return false;
    if ((UInt64)busLast - busFirst + 1 > buses)
        busLast = (UInt8)(busFirst + buses - 1);

    portMap = port->map(kIOMapInhibitCache);
    if (!portMap) {
        LOG("could not map port0");
        return false;
    }
    portBase = (volatile UInt8 *)portMap->getVirtualAddress();

    UInt32 status = portRead(PORT_STATUS), link = portRead(PORT_LINKSTS);
    LOG("port0 appclk=0x%08x status=0x%08x refclk=0x%08x perst=0x%08x linksts=0x%08x intstat=0x%08x",
        portRead(PORT_APPCLK), status, portRead(PORT_REFCLK), portRead(PORT_PERST), link,
        portRead(PORT_INTSTAT));
    if (!(status & PORT_STATUS_READY) || !(link & PORT_LINKSTS_UP)) {
        LOG("port0 link is not up; the launcher must bring it up first. Not enumerating.");
        return false;
    }

    /* Map only the buses we may touch. */
    IOByteCount ecamLen = ((IOByteCount)(busLast - busFirst) + 1) << ECAM_BUS_SHIFT;
    ecamMap = ecam->map(kIOMapInhibitCache);
    if (!ecamMap || ecamMap->getLength() < ecamLen) {
        LOG("could not map the ECAM window");
        return false;
    }
    ecamBase = (volatile UInt8 *)ecamMap->getVirtualAddress();

    IOPCIAddressSpace root;
    root.bits = 0;
    LOG("root port id=0x%08x class=0x%08x buses=0x%08x; bus range %u-%u",
        configRead32(root, kIOPCIConfigVendorID), configRead32(root, kIOPCIConfigRevisionID),
        configRead32(root, 0x18), busFirst, busLast);

    /* regs is owned by the registry (IODTResolveAddressing publishes it); not released here. */
    bool ok = super::start(provider);
    LOG("IOPCIBridge start %s", ok ? "ok" : "FAILED");
    if (ok) {
        IOPCIAddressSpace dev;
        dev.bits = 0;
        dev.s.busNum = (configRead32(root, 0x18) >> 8) & 0xff;
        for (unsigned fn = 0; fn < 2 && dev.s.busNum; fn++) {
            dev.s.functionNum = fn;
            LOG("after scan %u:0.%u id=0x%08x cmd=0x%08x bar0=0x%08x%08x bar2=0x%08x%08x",
                dev.s.busNum, fn, configRead32(dev, kIOPCIConfigVendorID),
                configRead32(dev, kIOPCIConfigCommand),
                configRead32(dev, 0x14), configRead32(dev, 0x10),
                configRead32(dev, 0x1c), configRead32(dev, 0x18));
        }
        /*
         * apple_pcie_enable_device(): requester ID -> DART stream. Linux iommu-map
         * sends every function on bus 1 to stream 1 of dart-apcie0.
         */
        for (unsigned fn = 0; fn < 2 && dev.s.busNum; fn++) {
            UInt32 entry = PORT_RID2SID_VALID | (1u << PORT_RID2SID_SID_SHIFT) |
                           ((UInt32)dev.s.busNum << PORT_RID2SID_BUS_SHIFT) | fn;
            OSWriteLittleInt32((volatile void *)portBase, PORT_RID2SID + 4 * fn, entry);
            OSSynchronizeIO();
            LOG("rid2sid[%u] = 0x%08x (readback 0x%08x)", fn, entry, portRead(PORT_RID2SID + 4 * fn));
        }
        /* Bring-up aid: report port error/interrupt status once a second for two minutes. */
        retain();
        thread_t watcher;
        if (kernel_thread_start(&PDApcieT8103Host::watchThread, this, &watcher) == KERN_SUCCESS)
            thread_deallocate(watcher);
        LOG("root port after scan buses=0x%08x membase/limit=0x%08x",
            configRead32(root, 0x18), configRead32(root, 0x20));
    }
    return ok;
}

void PDApcieT8103Host::watchThread(void *arg, wait_result_t)
{
    PDApcieT8103Host *self = (PDApcieT8103Host *)arg;
    UInt32 last = 0xffffffff, lastLink = 0xffffffff;
    for (int i = 0; i < 120 && self->portBase; i++) {
        UInt32 stat = self->portRead(PORT_INTSTAT), link = self->portRead(PORT_LINKSTS);
        if (stat != last || link != lastLink) {
            LOG("port0 intstat=0x%08x linksts=0x%08x (t+%ds)", stat, link, i);
            last = stat;
            lastLink = link;
        }
        IOSleep(1000);
    }
    self->release();
}

bool PDApcieT8103Host::configure(IOService *provider)
{
    bool ok = addBridgeMemoryRange((IOPhysicalAddress)APCIE_MEM32_BUS_BASE,
                                   (IOPhysicalLength)APCIE_MEM32_SIZE, true);
    LOG("memory window pci 0x%llx size 0x%llx %s", APCIE_MEM32_BUS_BASE, APCIE_MEM32_SIZE,
        ok ? "added" : "NOT added");
    return super::configure(provider);
}

void PDApcieT8103Host::free(void)
{
    OSSafeReleaseNULL(ecamMap);
    OSSafeReleaseNULL(portMap);
    ecamBase = NULL;
    portBase = NULL;
    super::free();
}

/* No I/O port space behind this bridge. */
IODeviceMemory *PDApcieT8103Host::ioDeviceMemory(void) { return NULL; }

UInt8 PDApcieT8103Host::firstBusNum(void) { return busFirst; }
UInt8 PDApcieT8103Host::lastBusNum(void) { return busLast; }

IOPCIAddressSpace PDApcieT8103Host::getBridgeSpace(void)
{
    IOPCIAddressSpace space;
    space.bits = 0;
    space.s.busNum = busFirst;
    return space;
}

volatile UInt8 *PDApcieT8103Host::configAddress(IOPCIAddressSpace space, UInt8 offset) const
{
    if (!ecamBase || space.s.busNum < busFirst || space.s.busNum > busLast)
        return NULL;
    /*
     * Root bus: only port 0 (device 0, function 0) is initialised on the J293.
     * Below it: a PCIe link has exactly one device, number 0.
     */
    if (space.s.deviceNum != 0)
        return NULL;
    if (space.s.busNum == busFirst && space.s.functionNum != 0)
        return NULL;

    IOByteCount address = ((IOByteCount)(space.s.busNum - busFirst) << ECAM_BUS_SHIFT) |
                          ((IOByteCount)space.s.deviceNum << ECAM_DEVICE_SHIFT) |
                          ((IOByteCount)space.s.functionNum << ECAM_FUNCTION_SHIFT) |
                          ((IOByteCount)space.es.registerNumExtended << 8) | offset;
    return ecamBase + address;
}

UInt32 PDApcieT8103Host::configRead32(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset & ~3);
    if (!address)
        return 0xffffffff;
    UInt32 value = OSReadLittleInt32((volatile void *)address, 0);
    OSSynchronizeIO();
    return value;
}

void PDApcieT8103Host::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data)
{
    volatile UInt8 *address = configAddress(space, offset & ~3);
    if (!address)
        return;
    OSWriteLittleInt32((volatile void *)address, 0, data);
    OSSynchronizeIO();
}

UInt16 PDApcieT8103Host::configRead16(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset & ~1);
    if (!address)
        return 0xffff;
    UInt16 value = OSReadLittleInt16((volatile void *)address, 0);
    OSSynchronizeIO();
    return value;
}

void PDApcieT8103Host::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data)
{
    volatile UInt8 *address = configAddress(space, offset & ~1);
    if (!address)
        return;
    OSWriteLittleInt16((volatile void *)address, 0, data);
    OSSynchronizeIO();
}

UInt8 PDApcieT8103Host::configRead8(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address)
        return 0xff;
    UInt8 value = *address;
    OSSynchronizeIO();
    return value;
}

void PDApcieT8103Host::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address)
        return;
    *address = data;
    OSSynchronizeIO();
}

