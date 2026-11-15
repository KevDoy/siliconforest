// SPDX-License-Identifier: GPL-2.0
/*
 * J293/T8103 port-1 lifecycle adapter for the directly ported ATC/DWC3 code.
 * Copyright (C) The Asahi Linux Contributors (ATC, Apple DWC3, PMGR)
 * ATC author: Sven Peter <sven@kernel.org>
 * DWC3 core: Copyright (C) 2010-2011 Texas Instruments Incorporated.
 * Apple glue based on Copyright (c) 2018 The Linux Foundation and
 * Copyright (c) 2015 Texas Instruments Incorporated.
 * ADT tunable mapping: m1n1 MIT, Copyright The Asahi Linux Contributors;
 * see the bundled NOTICE for its permission/warranty notice.
 *
 * Port 0 is never accessed. This initial host session uses USB2 with the
 * SuperSpeed lanes off, and refuses DP/TBT/USB4 ownership. No PD commands.
 */
#include "pd_usb_dart.h"
#include "pd_cd321x.h"
#include "pd_dwc3_host.h"
#include "pd_atc_usb2.h"
#include "pd_atc_tunables.h"
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <pexpert/pexpert.h>

#define LOG(fmt, ...) IOLog("PDAppleUSB: " fmt "\n", ##__VA_ARGS__)
static const char *const atcPath="/arm-io/atc-phy1";
static const char *const usbPath="/arm-io/usb-drd1";

class PDAppleUSBHostNub : public IOPlatformDevice {
    OSDeclareDefaultStructors(PDAppleUSBHostNub)
public:
    PDUSBDart *domain;
    // This is a prepared controller child of PDAppleUSB, not a platform
    // expert's DT child. IOPlatformDevice's default methods cast the parent
    // to IOPlatformExpert; bypass those delegations for this synthetic nub.
    bool compareName(OSString *name, OSString **matched) const override {
        return IORegistryEntry::compareName(name,matched);
    }
    IOReturn getResources() override {
        return domain && getDeviceMemoryCount()==1 ? kIOReturnSuccess : kIOReturnNotReady;
    }
    IOReturn callPlatformFunction(const OSSymbol *name, bool wait,
                                  void *a, void *b, void *c, void *d) override {
        if (name && name->isEqualTo("PDAppleUSBDart")) {
            if (!a || !domain) return kIOReturnNotReady;
            *(PDUSBDart **)a=domain;
            return kIOReturnSuccess;
        }
        return IOPlatformDevice::callPlatformFunction(name,wait,a,b,c,d);
    }
};
OSDefineMetaClassAndStructors(PDAppleUSBHostNub, IOPlatformDevice)

class PDAppleUSB : public IOService {
    OSDeclareDefaultStructors(PDAppleUSB)
    PDMmio core, usb2, pipe, lpdptx, axi, dwc, fuses;
    OSData *common[10];
    PDUSBDart *domain; // tables and domain deliberately live for this kernel
    PDAppleUSBHostNub *nub;
    IOWorkLoop *loop;
    IOTimerEventSource *timer;
    bool mapped, active, reversed, quarantined;
    uint32_t session, lastSample;
    int lastError;
    pd_atc_io atcIO;
    pd_dwc3_io dwcIO;
    PDMmio &window(enum pd_atc_window w) {
        switch(w) {
        case PD_ATC_CORE: return core;
        case PD_ATC_USB2: return usb2;
        case PD_ATC_PIPE: return pipe;
        default: return lpdptx;
        }
    }
    static uint32_t readAtc(void *ctx, enum pd_atc_window w,uint32_t r) {
        return ((PDAppleUSB*)ctx)->window(w).r32(r);
    }
    static void writeAtc(void *ctx, enum pd_atc_window w,uint32_t r,uint32_t v) {
        ((PDAppleUSB*)ctx)->window(w).w32(r,v);
    }
    static uint32_t readDwc(void *ctx,uint32_t r) { return ((PDAppleUSB*)ctx)->dwc.r32(r); }
    static void writeDwc(void *ctx,uint32_t r,uint32_t v) { ((PDAppleUSB*)ctx)->dwc.w32(r,v); }
    static void delay(void *, unsigned us) {
        if (us >= 1000) IOSleep((us+999)/1000); else IODelay(us);
    }
    static uint32_t readTunable(void *ctx,uint32_t r) { return ((PDMmio*)ctx)->r32(r); }
    static void writeTunable(void *ctx,uint32_t r,uint32_t v) { ((PDMmio*)ctx)->w32(r,v); }
    static int applyCommon(void *ctx);
    bool map();
    int begin(bool flip);
    bool end();
    void sample();
    static void tick(OSObject *owner,IOTimerEventSource *);
public:
    IOService *probe(IOService *,SInt32 *) override;
    bool start(IOService *) override;
    void stop(IOService *) override;
    void free() override;
};
OSDefineMetaClassAndStructors(PDAppleUSB, IOService)

// m1n1 kboot_atc.c common-b ordering. AXI2AF is a different physical window.
static const struct {
    const char *name; uint32_t base; bool optional;
} groups[] = {
    {"tunable_ATC0AXI2AF",0,false},
    {"tunable_ATC_FABRIC",0x45000,false},
    {"tunable_USB_ACIOPHY_TOP",0,false},
    {"tunable_AUS_CMN_SHM",0xa00,false},
    {"tunable_AUS_CMN_TOP",0x800,false},
    {"tunable_AUSPLL_CORE",0x2200,false},
    {"tunable_AUSPLL_TOP",0x2000,false},
    {"tunable_CIO3PLL_CORE",0x2a00,false},
    {"tunable_CIO3PLL_TOP",0x2800,false},
    {"tunable_CIO_CIO3PLL_TOP",0x2800,true},
};

bool PDAppleUSB::map()
{
    if (mapped) return true;
    IORegistryEntry *entry=IORegistryEntry::fromPath(atcPath,gIODTPlane);
    if (!entry) return false;
    // Validate the entire immutable firmware bundle before any hardware write.
    bool valid=true;
    for (unsigned i=0;i<10;++i) {
        OSSafeReleaseNULL(common[i]);
        common[i]=OSDynamicCast(OSData,entry->copyProperty(groups[i].name));
        if ((!common[i] && !groups[i].optional) ||
            (common[i] && pd_atc_tunable_validate(common[i]->getBytesNoCopy(),common[i]->getLength(),0x4000))) {
            LOG("invalid or missing ADT %s; no PHY writes",groups[i].name);
            valid=false;
        }
    }
    entry->release();
    if (!valid) return false;
    // Resolve actual ADT entries first, then verify this board's Linux register
    // layout before widening the core/LPDPTX views to their documented spans.
    if (!core.mapNode(atcPath,3) || core.phys!=0x503000000ULL || core.len<0x4000 ||
        !usb2.mapNode(atcPath,0) || usb2.phys!=0x502a90000ULL || usb2.len<0x20 ||
        !pipe.mapNode(usbPath,3) || pipe.phys!=0x502a84000ULL || pipe.len<0x24 ||
        !lpdptx.mapNode(atcPath,20) || lpdptx.phys!=0x503050000ULL ||
        !axi.mapNode(atcPath,24) || axi.phys!=0x500000000ULL ||
        !dwc.mapNode(usbPath,0) || dwc.phys!=0x502280000ULL || dwc.len<0x10000 ||
        !fuses.mapNode(atcPath,23) || fuses.phys!=0x23d2bc000ULL || fuses.len<0x444 ||
        !core.mapPhys(0x503000000ULL,0x4c000) ||
        !lpdptx.mapPhys(0x503050000ULL,0x8000) ||
        !axi.mapPhys(0x500000000ULL,0x4000)) return false;
    atcIO={this,readAtc,writeAtc,delay,applyCommon};
    dwcIO={this,readDwc,writeDwc,delay};
    mapped=true;
    return true;
}

int PDAppleUSB::applyCommon(void *ctx)
{
    PDAppleUSB *self=(PDAppleUSB*)ctx;
    // m1n1 T8103 port-1 fuse extraction, in original order including its
    // split-field/workaround writes. Fuse registers are READ ONLY here.
    static const struct { unsigned off, bit, len, reg, mask; } fuse[] = {
        {0x438,19,6,0x2a38,0x7f}, {0x438,25,6,0x2a38,0xfe0000},
        {0x438,31,1,0x2aa4,0x60000}, {0x438,14,5,0xa04,0x7c},
        {0x43c,0,1,0x2aa4,0x60000}, {0x43c,1,3,0x2a20,0x1c000},
        {0x438,7,2,0x222c,0x600}, {0x438,4,3,0x222c,0x7000},
        {0x438,12,2,0x22a4,0x60000}, {0x438,9,3,0x2220,0x1c000},
        {0x438,14,4,0xa04,0x7c},
    };
    for (unsigned i=0;i<sizeof(fuse)/sizeof(fuse[0]);++i) {
        const auto &f=fuse[i];
        uint64_t raw=self->fuses.r32(f.off) | ((uint64_t)self->fuses.r32(f.off+4)<<32);
        uint32_t val=(uint32_t)((raw>>f.bit)&((1u<<f.len)-1));
        unsigned shift=__builtin_ctz(f.mask);
        self->core.mask32(f.reg,f.mask,(val<<shift)&f.mask);
    }
    for (unsigned i=0;i<10;++i) {
        if (!self->common[i]) continue;
        PDMmio *dest=i==0?&self->axi:&self->core;
        int ret=pd_atc_tunable_apply(self->common[i]->getBytesNoCopy(),self->common[i]->getLength(),
            0x4000,groups[i].base,dest,readTunable,writeTunable);
        if (ret) return ret;
    }
    return 0;
}

int PDAppleUSB::begin(bool flip)
{
    if (quarantined || !map()) return ENODEV;
    int ret=pd_pmgr_enable_node(atcPath,-1,0,true);
    if (!ret) ret=pd_pmgr_enable_node(usbPath,-1,0,true);
    if (ret) return ret;
    // Never take a controller from another owner. Before changing DART or
    // PHY, require its current host view to be stopped (or externally reset).
    if (pipe.r32(0x1c)&1) {
        uint32_t cap=dwc.r32(0); unsigned op=cap&0xff;
        if ((dwc.r32(0xc704)&(1u<<31)) || cap==UINT32_MAX || op<0x20 || op>0x100 || (dwc.r32(op)&1) || !(dwc.r32(op+4)&1))
            return EBUSY;
    }
    // Initial Linux probe establishes OFF before configuring the cable mode.
    ret=pd_atc_usb2_stop(&atcIO);
    if (!ret && !domain) domain=new PDUSBDart;
    if (!ret) ret=domain?domain->prepare():ENOMEM;
    if (!ret) ret=pd_atc_usb2_start(&atcIO,flip);
    if (!ret) {
        pd_atc_usb2_release_reset(&atcIO);
        ret=pd_dwc3_host_init(&dwcIO);
    }
    if (ret) {
        int cleanup=pd_atc_usb2_stop(&atcIO);
        if (cleanup) quarantined=true;
        LOG("session initialization failed %d, PHY cleanup=%d",ret,cleanup);
        return ret;
    }
    pd_atc_usb2_select_host(&atcIO);
    nub=OSTypeAlloc(PDAppleUSBHostNub);
    if (!nub || !nub->init()) { OSSafeReleaseNULL(nub); pd_atc_usb2_stop(&atcIO); return ENOMEM; }
    nub->domain=domain;
    nub->setName("pd,apple-dwc3-host");
    nub->setProperty("port-index",1,32);
    nub->setProperty("usb2-only",true);
    nub->setProperty("pd-usb-dma-live",false);
    IOMemoryDescriptor *md=IOMemoryDescriptor::withPhysicalAddress(dwc.phys,dwc.len,kIODirectionInOut);
    OSArray *mem=md?OSArray::withObjects((const OSObject**)&md,1):NULL;
    if (mem) nub->setDeviceMemory(mem);
    OSSafeReleaseNULL(mem); OSSafeReleaseNULL(md);
    if (!nub->getDeviceMemory() || !nub->attach(this)) {
        nub->release(); nub=NULL; pd_atc_usb2_stop(&atcIO); return ENOMEM;
    }
    active=true; reversed=flip; ++session;
    setProperty("host-session",session,32);
    setProperty("host-active",true);
    nub->registerService();
    LOG("port 1 session %u USB2 host nub published (enumeration not established)",session);
    return 0;
}

bool PDAppleUSB::end()
{
    if (!active) return !quarantined;
    auto clients=nub?nub->getClientIterator():nullptr;
    if (nub && !clients) return false;
    IOService *client=clients?OSDynamicCast(IOService,clients->getNextObject()):NULL;
    if (client) {
        IOReturn ret=client->callPlatformFunction("PDAppleUSBDrain",false,NULL,NULL,NULL,NULL);
        if (ret!=kIOReturnSuccess) { LOG("host drain busy; retaining PHY/DMA"); return false; }
    }
    if (nub && nub->getProperty("pd-usb-dma-live")==kOSBooleanTrue) {
        LOG("partial attachment still owns DMA; retaining PHY"); return false;
    }
    // Synchronous termination must finish class stop before resetting PHY.
    if (nub && !nub->terminate(kIOServiceRequired|kIOServiceSynchronous)) return false;
    // Matching/start may have raced the first lookup. Termination serializes
    // with it; a failed-start driver that quarantined DMA must still block us.
    if (nub && nub->getProperty("pd-usb-dma-live")==kOSBooleanTrue) return false;
    if (nub) { nub->detach(this); nub->release(); nub=NULL; }
    int ret=pd_atc_usb2_stop(&atcIO);
    active=false;
    setProperty("host-active",false);
    if (ret) { quarantined=true; LOG("PHY stop failed %d; no new session",ret); return false; }
    LOG("port 1 session %u stopped",session);
    return true;
}

void PDAppleUSB::sample()
{
    OSObject *object=getProvider()->copyProperty("port-state");
    OSData *data=OSDynamicCast(OSData,object);
    pd_cd321x_snapshot state={};
    bool valid=data && data->getLength()==sizeof(state);
    if (valid) bcopy(data->getBytesNoCopy(),&state,sizeof(state));
    OSSafeReleaseNULL(object);
    if (!valid || state.error || state.sample==lastSample) return;
    lastSample=state.sample;
    bool host=pd_cd321x_usb_role(state.status,state.data_status)==PD_CD321X_USB_HOST;
    bool alt=state.data_status&(PD_TPS_DATA_DP|PD_TPS_DATA_TBT|PD_CD321X_DATA_USB4);
    bool flip=state.status&PD_TPS_STATUS_UPSIDE_DOWN;
    if (active && (!host || alt || flip!=reversed)) {
        if (!end()) { setProperty("removal-blocked",true); return; }
    }
    setProperty("removal-blocked",false);
    setProperty("alternate-mode-deferred",alt);
    if (host && !alt && !active && !quarantined) {
        int ret=begin(flip);
        setProperty("last-error",(uint32_t)ret,32);
        if (ret && ret!=lastError) LOG("port 1 host start error=%d",ret);
        lastError=ret;
    }
    if (domain) {
        uint32_t faults=domain->faults();
        if (faults) {
            setProperty("dart-fault",faults,32);
            // Stop producers; preserve fault evidence and reject another session.
            end(); quarantined=true;
        }
    }
}
void PDAppleUSB::tick(OSObject *owner,IOTimerEventSource *source)
{
    PDAppleUSB *self=OSDynamicCast(PDAppleUSB,owner);
    if (!self) return;
    self->sample(); source->setTimeoutMS(100);
}
IOService *PDAppleUSB::probe(IOService *provider,SInt32 *score)
{
    int enabled=0; uint32_t chip=0;
    OSNumber *port=OSDynamicCast(OSNumber,provider->getProperty("port-index"));
    OSData *mode=OSDynamicCast(OSData,provider->getProperty("firmware-mode"));
    if (!mode || mode->getLength()!=4 || memcmp(mode->getBytesNoCopy(),"APP ",4)) return NULL;
    if (!PE_parse_boot_argn("pdusbhost",&enabled,sizeof(enabled)) || enabled!=1 ||
        !pd_adt_u32("/chosen","chip-id",0,&chip) || chip!=0x8103 ||
        !port || port->unsigned32BitValue()!=1) return NULL;
    return IOService::probe(provider,score);
}
bool PDAppleUSB::start(IOService *provider)
{
    if (!IOService::start(provider)) return false;
    setProperty("port-index",1,32); setProperty("usb2-only",true);
    setProperty("host-active",false);
    loop=IOWorkLoop::workLoop(); timer=IOTimerEventSource::timerEventSource(this,tick);
    if (!loop || !timer || loop->addEventSource(timer)!=kIOReturnSuccess) return false;
    timer->setTimeoutMS(1); registerService();
    LOG("port 1 host lifecycle enabled; port 0 reserved, DP/TBT/USB4 deferred");
    return true;
}
void PDAppleUSB::stop(IOService *provider)
{
    if (timer) { timer->cancelTimeout(); timer->disable(); if(loop)loop->removeEventSource(timer); }
    if (!end()) { retain(); quarantined=true; LOG("stop retains failed-drain session"); }
    IOService::stop(provider);
}
void PDAppleUSB::free()
{
    if (active || quarantined) { LOG("quarantined session resources retained"); return; }
    OSSafeReleaseNULL(timer); OSSafeReleaseNULL(loop);
    for(unsigned i=0;i<10;++i) OSSafeReleaseNULL(common[i]);
    core.unmap();usb2.unmap();pipe.unmap();lpdptx.unmap();axi.unmap();dwc.unmap();fuses.unmap();
    IOService::free();
}
