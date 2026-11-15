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
 * Port 0 is never accessed. Direct USB2/USB3 hosts use runtime calibration;
 * DP/TBT/USB4 clients acquire a shared PHY lease. No PD commands.
 */
#include "pd_usb_dart.h"
#include "pd_cd321x.h"
#include "pd_dwc3_host.h"
#include "pd_atc_usb2.h"
#include "pd_atc_tunables.h"
#include "pd_atc_calibration.h"
#include "pd_atc_pipe.h"
#include "pd_atc_dp.h"
#include "pd_usbc_phy_lease.h"
#include <IOKit/IOCommandGate.h>
#include <kern/thread_call.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOTimerEventSource.h>
#include <pexpert/pexpert.h>

#define LOG(fmt, ...) IOLog("PDAppleUSB: " fmt "\n", ##__VA_ARGS__)
int pd_usbc_pmgr_node_off(const char *,uint32_t *);

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
    OSDictionary *laneData; // retained immutable per-session calibration
    pd_atc_pipe_state pipeState;
    PDUSBDart *domain; // tables and domain deliberately live for this kernel
    PDAppleUSBHostNub *nub;
    IOWorkLoop *loop;
    IOTimerEventSource *timer;
    IOCommandGate *gate;
    thread_call_t revokeCall;
    pd_usbc_phy_lease lease;
    pd_atc_dp directDP;
    pd_atc_tunnel tunnel;
    IOService *altClient;
    uint64_t noticeToken;
    bool altTouched, auxTouched, stopping;
    bool mapped, active, reversed, superSpeed, quarantined;
    uint32_t session, lastSample;
    uint32_t acceptingSnapshots,snapshotChecks;
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
    static const void *getLane(void *,const char *,const char *,size_t *);
    int stopPHY();
    int priorACIOOff();
    int begin(bool flip,bool ss);
    bool end();
    void sample();
    bool readCable(pd_cd321x_cable_snapshot &);
    int observeCable(const pd_cd321x_cable_snapshot &);
    int beginAlt(pd_usbc_phy_mode,bool);
    int stopAlt();
    int phyRequest(pd_usbc_phy_request *);
    int snapshotCalibration(bool beforeACIO=false);
    void queueRevoke();
    static IOReturn phyAction(OSObject *,void *,void *,void *,void *);
    static IOReturn stopAction(OSObject *,void *,void *,void *,void *);
    static IOReturn noticeAction(OSObject *,void *,void *,void *,void *);
    static void notifyRevoke(thread_call_param_t,thread_call_param_t);
    static void tick(OSObject *owner,IOTimerEventSource *);
public:
    IOService *probe(IOService *,SInt32 *) override;
    bool start(IOService *) override;
    void stop(IOService *) override;
    void free() override;
    IOReturn message(UInt32,IOService *,void *) override;
    IOReturn callPlatformFunction(const OSSymbol *,bool,void *,void *,void *,void *) override;
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

const void *PDAppleUSB::getLane(void *ctx,const char *path,const char *name,size_t *bytes)
{
    PDAppleUSB *self=(PDAppleUSB*)ctx;
    *bytes=0;
    if (strcmp(path,atcPath) || !self->laneData) return NULL;
    OSData *data=OSDynamicCast(OSData,self->laneData->getObject(name));
    if (!data) {
        IORegistryEntry *entry=IORegistryEntry::fromPath(path,gIODTPlane);
        OSObject *value=entry?entry->copyProperty(name):NULL;
        data=OSDynamicCast(OSData,value);
        // Copy the bytes: even an unexpected property mutation cannot change
        // a validated bundle between preflight and apply.
        OSData *copy=data?OSData::withBytes(data->getBytesNoCopy(),data->getLength()):NULL;
        bool kept=copy && self->laneData->setObject(name,copy);
        OSSafeReleaseNULL(copy);OSSafeReleaseNULL(value);OSSafeReleaseNULL(entry);
        if (!kept) return NULL;
        data=OSDynamicCast(OSData,self->laneData->getObject(name));
    }
    *bytes=data->getLength();return data->getBytesNoCopy();
}
int PDAppleUSB::snapshotCalibration(bool beforeACIO)
{
    // Retained, validated runtime tuning only. Never read the fuse controller,
    // firmware storage, arbitrary addresses, or a window outside this lease.
    if(lease.mode!=PD_USBC_PHY_TBT && lease.mode!=PD_USBC_PHY_USB4)return EOPNOTSUPP;
    auto addresses=OSData::withCapacity(4096);auto samples=OSData::withCapacity(6144);
    if(!addresses || !samples){OSSafeReleaseNULL(addresses);OSSafeReleaseNULL(samples);return ENOMEM;}
    auto addAddress=[&](uint32_t window,uint32_t offset)->int {
        PDMmio &mmio=window?axi:core;
        if(window>1 || !mmio.va || mmio.len<4 || (offset&3) || offset>mmio.len-4)return EINVAL;
        if(addresses->getLength()/8>=512)return EOVERFLOW;
        uint32_t pair[]={window,offset};return addresses->appendBytes(pair,sizeof(pair))?0:ENOMEM;
    };
    auto addGroup=[&](OSData *data,unsigned span,unsigned base,unsigned window,bool allowEmpty)->int {
        const auto raw=data?(const uint8_t *)data->getBytesNoCopy():NULL;
        int ret=pd_atc_tunable_observation_validate(data!=NULL,raw,data?data->getLength():0,span,allowEmpty);
        if(ret)return ret;
        for(unsigned n=0;n<data->getLength();n+=12){
            ret=addAddress(window,base+(pd_atc_le32(raw+n)&0xffffff));if(ret)return ret;
        }
        return 0;
    };
    int ret=0;uint32_t emptyGroups=0;
    const char *groupError=beforeACIO?"phy-pre-acio-calibration-group-error":"phy-calibration-group-error";
    setProperty(groupError,"none");
    for(unsigned i=0;!ret && i<10;++i){
        if(!common[i] && groups[i].optional)continue;
        // map()/applyCommon() accept a present empty common group; retain
        // that exact contract. Lane tuning still requires nonempty data.
        if(common[i] && !common[i]->getLength()){
            ++emptyGroups;setProperty("phy-calibration-empty-group",groups[i].name);
        }
        ret=addGroup(common[i],0x4000,groups[i].base,i==0?1:0,true);
        if(ret)setProperty(groupError,groups[i].name);
    }
    setProperty("phy-calibration-empty-groups",emptyGroups,32);
    static const struct {const char *name;unsigned base;} lanes[]={
        {"tunable_CIO_LN0_AUSPMA_TX_TOP",0xc000},{"tunable_CIO_LN0_AUSPMA_RX_TOP",0x9000},
        {"tunable_CIO_LN0_AUSPMA_RX_SHM",0xb000},{"tunable_CIO_LN0_AUSPMA_RX_EQ",0xa000},
        {"tunable_CIO_LN1_AUSPMA_TX_TOP",0x13000},{"tunable_CIO_LN1_AUSPMA_RX_TOP",0x10000},
        {"tunable_CIO_LN1_AUSPMA_RX_SHM",0x12000},{"tunable_CIO_LN1_AUSPMA_RX_EQ",0x11000}
    };
    for(unsigned i=0;!ret && i<sizeof(lanes)/sizeof(lanes[0]);++i){
        ret=addGroup(laneData?OSDynamicCast(OSData,laneData->getObject(lanes[i].name)):NULL,0x1000,lanes[i].base,0,false);
        if(ret)setProperty(groupError,lanes[i].name);
    }
    static const uint32_t analog[]={0x2a38,0x2aa4,0xa04,0x2a20,0x222c,0x22a4,0x2220};
    for(unsigned i=0;!ret && i<sizeof(analog)/sizeof(analog[0]);++i)ret=addAddress(0,analog[i]);
    // Complete validation/capacity proof before the first calibration read.
    for(unsigned n=0;!ret && n<addresses->getLength();n+=8){
        uint32_t row[3];memcpy(row,(const uint8_t *)addresses->getBytesNoCopy()+n,8);
        row[2]=(row[0]?axi:core).r32(row[1]);
        if(!samples->appendBytes(row,sizeof(row)))ret=ENOMEM;
    }
    if(!ret && (!setProperty(beforeACIO?"phy-pre-acio-calibration-registers":"phy-calibration-registers",samples) ||
        !setProperty(beforeACIO?"phy-pre-acio-calibration-count":"phy-calibration-count",(uint64_t)(samples->getLength()/12),32)))ret=ENOMEM;
    addresses->release();samples->release();return ret;
}

int PDAppleUSB::priorACIOOff()
{
    // A stopped DWC3 is not proof of stopped ACIO. Check before even
    // enabling PHY clocks: an inherited controller must not wake on them.
    uint32_t acioState=0;
    int ret=pd_usbc_pmgr_node_off("/arm-io/acio1",&acioState);
    setProperty("prior-acio-power-state",acioState,32);
    setProperty("prior-acio-off-error",(uint32_t)ret,32);
    // T8103 M3 is inside ATC1_CIO. Its clock-gate entry is a virtual
    // reconfigure command, not another physical power domain.
    setProperty("prior-acio-power-covers-m3",!ret);
    return ret;
}

int PDAppleUSB::stopPHY()
{
    // Callers have either validated the initial stopped host or completed
    // the xHCI drain + synchronous child termination. No live bypass.
    int ret=pd_atc_pipe_park(&atcIO,&pipeState,true);
    if (!ret) ret=pd_atc_usb2_stop(&atcIO);
    return ret;
}
int PDAppleUSB::begin(bool flip,bool ss)
{
    if (quarantined || !map()) return ENODEV;
    OSSafeReleaseNULL(laneData);
    pd_atc_cal_bundle bundle={};bundle.io=&atcIO;bundle.adt_ctx=this;bundle.getprop=getLane;
    auto cal=pd_atc_calibration_callbacks(&bundle);
    if (ss) {
        laneData=OSDictionary::withCapacity(8);
        pd_atc_program program;
        int preflight=pd_atc_mode_plan(PD_ATC_MODE_USB3,flip,&program);
        if (!preflight) preflight=cal.validate(cal.ctx,&program);
        if (preflight) return preflight;
    }
    int ret=priorACIOOff();
    if (ret) return ret;
    ret=pd_pmgr_enable_node(atcPath,-1,0,true);
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
    // Adopt only the actual mux state after the controller stop check above.
    // Preserve a previously failed handoff rather than bypassing its retry.
    if (!pipeState.dummy_pending) {
        switch(pipe.r32(0xc)&0x3f) {
        case 8: pipeState.backend=PD_ATC_USB3_BACKEND;break;
        case 0x11: pipeState.backend=PD_ATC_USB4_BACKEND;break;
        case 0x22: pipeState.backend=PD_ATC_DUMMY;break;
        default: return EBUSY;
        }
    }
    ret=stopPHY();
    if (!ret && !domain) domain=new PDUSBDart;
    if (!ret) ret=domain?domain->prepare():ENOMEM;
    if (!ret) ret=ss?pd_atc_mode_start(&atcIO,PD_ATC_MODE_USB3,flip,&cal):pd_atc_usb2_start(&atcIO,flip);
    if (!ret) {
        pd_atc_usb2_release_reset(&atcIO);
        ret=pd_dwc3_host_init(&dwcIO);
        if (!ret && ss) ret=pd_atc_pipe_route(&atcIO,&pipeState,PD_ATC_USB3_BACKEND);
    }
    if (ret) {
        int cleanup=stopPHY();
        if (cleanup) quarantined=true;
        LOG("session initialization failed %d, PHY cleanup=%d",ret,cleanup);
        return ret;
    }
    pd_atc_usb2_select_host(&atcIO);
    nub=OSTypeAlloc(PDAppleUSBHostNub);
    if (!nub || !nub->init()) { OSSafeReleaseNULL(nub); if (stopPHY()) quarantined=true; return ENOMEM; }
    nub->domain=domain;
    nub->setName("pd,apple-dwc3-host");
    nub->setProperty("port-index",1,32);
    nub->setProperty("usb2-only",!ss);
    nub->setProperty("pd-usb-dma-live",false);
    IOMemoryDescriptor *md=IOMemoryDescriptor::withPhysicalAddress(dwc.phys,dwc.len,kIODirectionInOut);
    OSArray *mem=md?OSArray::withObjects((const OSObject**)&md,1):NULL;
    if (mem) nub->setDeviceMemory(mem);
    OSSafeReleaseNULL(mem); OSSafeReleaseNULL(md);
    if (!nub->getDeviceMemory() || !nub->attach(this)) {
        nub->release(); nub=NULL; if (stopPHY()) quarantined=true; return ENOMEM;
    }
    active=true; reversed=flip; superSpeed=ss; ++session;
    setProperty("usb2-only",!ss);
    setProperty("host-session",session,32);
    setProperty("host-active",true);
    nub->registerService();
    LOG("port 1 session %u %s host nub published (enumeration not established)",session,ss?"USB3":"USB2");
    return 0;
}

bool PDAppleUSB::end()
{
    if (!active) return !quarantined;
    auto clients=nub?nub->getClientIterator():nullptr;
    if (nub && !clients) return false;
    IOService *client=clients?OSDynamicCast(IOService,clients->getNextObject()):NULL;
    // Client iteration retains the child only until the iterator is released.
    if (client) client->retain();
    OSSafeReleaseNULL(clients);
    if (client) {
        IOReturn ret=client->callPlatformFunction("PDAppleUSBDrain",false,NULL,NULL,NULL,NULL);
        client->release();
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
    int ret=stopPHY();
    active=false;
    setProperty("host-active",false);
    if (ret) { quarantined=true; LOG("PHY stop failed %d; no new session",ret); return false; }
    LOG("port 1 session %u stopped",session);
    return true;
}

bool PDAppleUSB::readCable(pd_cd321x_cable_snapshot &state)
{
    if (!getProvider() || getProvider()->isInactive()) return false;
    OSObject *object=getProvider()->copyProperty("port-state");
    OSData *data=OSDynamicCast(OSData,object);
    bool valid=data && !pd_cd321x_snapshot_decode(data->getBytesNoCopy(),data->getLength(),&state);
    OSSafeReleaseNULL(object);
    return valid && !state.port.error && state.port.sample;
}
int PDAppleUSB::observeCable(const pd_cd321x_cable_snapshot &state)
{
    uint32_t smask=PD_TPS_STATUS_PLUG_PRESENT|PD_TPS_STATUS_UPSIDE_DOWN|PD_TPS_STATUS_DATAROLE;
    uint32_t dmask=PD_TPS_DATA_USB2|PD_TPS_DATA_USB3|PD_TPS_DATA_USB_DEVICE|PD_TPS_DATA_DP|PD_TPS_DATA_TBT|PD_CD321X_DATA_USB4|
        (7u<<25)|(3u<<28)|(1u<<18)|(1u<<20)|(1u<<22);
    uint64_t key=((uint64_t)(state.port.status&smask)<<32)|(state.port.data_status&dmask);
    if ((state.valid_flags&PD_CD321X_TBT_VALID) && (state.protocol_vdo&1u)) key|=UINT64_C(1)<<63;
    uint32_t eligible=0,d=state.port.data_status;
    if (state.port.status&PD_TPS_STATUS_PLUG_PRESENT) {
        if (d&PD_CD321X_DATA_USB4) eligible=PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_USB4);
        else if ((d&PD_TPS_DATA_TBT) && (state.valid_flags&PD_CD321X_TBT_VALID)) eligible=PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_TBT);
        // A composite USB3+DP cable needs a joint USB/DP session. Do not
        // silently steal its USB lanes for a four-lane DP-only claim.
        else if ((d&PD_TPS_DATA_DP) && !(d&PD_TPS_DATA_USB3)) eligible=PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_DP);
    }
    uint64_t before=lease.epoch;
    int ret=pd_usbc_phy_observe(&lease,key,eligible,!!(state.port.status&PD_TPS_STATUS_UPSIDE_DOWN));
    setProperty("phy-cable-epoch",lease.epoch,64);
    setProperty("phy-eligible-modes",lease.eligible,32);
    if (ret) quarantined=true;
    if (lease.phase==PD_USBC_PHY_REVOKED) queueRevoke();
    if (lease.epoch!=before) messageClients(kIOMessageServicePropertyChange);
    return ret;
}
void PDAppleUSB::sample()
{
    if (!__atomic_load_n(&acceptingSnapshots,__ATOMIC_ACQUIRE)) return;
    setProperty("snapshot-check-count",++snapshotChecks,32);
    pd_cd321x_cable_snapshot state={};
    if (stopping || !readCable(state) || state.port.sample==lastSample) return;
    lastSample=state.port.sample;
    if (observeCable(state)) return;
    if (lease.owner) {
        setProperty("alternate-mode-deferred",true);
        setProperty("phy-lease-revoked",lease.phase==PD_USBC_PHY_REVOKED);
        return; // Never drain/reset another client's PHY from this sampler.
    }
    bool host=pd_cd321x_usb_role(state.port.status,state.port.data_status)==PD_CD321X_USB_HOST;
    bool alt=state.port.data_status&(PD_TPS_DATA_DP|PD_TPS_DATA_TBT|PD_CD321X_DATA_USB4);
    bool flip=state.port.status&PD_TPS_STATUS_UPSIDE_DOWN;
    bool ss=state.port.data_status&PD_TPS_DATA_USB3;
    if (active && (!host || alt || flip!=reversed || ss!=superSpeed)) {
        if (!end()) { setProperty("removal-blocked",true); return; }
    }
    setProperty("removal-blocked",false);
    setProperty("alternate-mode-deferred",alt);
    if (host && !alt && !active && !quarantined) {
        int ret=begin(flip,ss);
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
    (void)source;
    self->sample(); // One-shot; Type-C producer schedules the next observation.
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
    setProperty("super-speed-capable",true);
    loop=IOWorkLoop::workLoop(); timer=IOTimerEventSource::timerEventSource(this,tick);
    gate=IOCommandGate::commandGate(this);
    revokeCall=thread_call_allocate(notifyRevoke,this);
    if (!loop || !timer || !gate || !revokeCall || loop->addEventSource(gate)!=kIOReturnSuccess ||
        loop->addEventSource(timer)!=kIOReturnSuccess) return false;
    setProperty("shared-phy-provider",true);
    setProperty("phy-lease-active",false);
    setProperty("snapshot-event-driven",true);
    snapshotChecks=0;
    setProperty("snapshot-check-count",(uint32_t)0,32);
    __atomic_store_n(&acceptingSnapshots,1,__ATOMIC_RELEASE);
    timer->setTimeoutMS(1); registerService();
    LOG("port 1 USB/shared PHY provider enabled; port 0 reserved");
    return true;
}
IOReturn PDAppleUSB::stopAction(OSObject *owner,void *,void *,void *,void *)
{
    auto self=OSDynamicCast(PDAppleUSB,owner);
    if (!self) return kIOReturnBadArgument;
    self->stopping=true;
    if (self->lease.owner) {
        self->lease.phase=PD_USBC_PHY_REVOKED;self->queueRevoke();
        self->retain();self->quarantined=true; // Keep provider/MMIO through asynchronous client drain.
    } else if (!self->end()) {
        self->retain();self->quarantined=true;LOG("stop retains failed-drain session");
    }
    return kIOReturnSuccess;
}
IOReturn PDAppleUSB::message(UInt32 type,IOService *provider,void *argument)
{
    if (type==kIOMessageServicePropertyChange && provider==getProvider()) {
        if (__atomic_load_n(&acceptingSnapshots,__ATOMIC_ACQUIRE) && timer)
            timer->setTimeoutMS(1);
        return kIOReturnSuccess;
    }
    return IOService::message(type,provider,argument);
}
void PDAppleUSB::stop(IOService *provider)
{
    __atomic_store_n(&acceptingSnapshots,0,__ATOMIC_RELEASE);
    if (timer) { timer->cancelTimeout(); timer->disable(); if(loop)loop->removeEventSource(timer); }
    if (gate) gate->runAction(stopAction);
    else stopAction(this,NULL,NULL,NULL,NULL);
    IOService::stop(provider);
}
void PDAppleUSB::free()
{
    if (active || quarantined || lease.owner) { LOG("quarantined session resources retained"); return; }
    if (revokeCall) { thread_call_cancel_wait(revokeCall);thread_call_free(revokeCall);revokeCall=NULL; }
    if (loop && gate) loop->removeEventSource(gate);
    OSSafeReleaseNULL(timer);OSSafeReleaseNULL(gate);OSSafeReleaseNULL(loop);
    for(unsigned i=0;i<10;++i) OSSafeReleaseNULL(common[i]);
    OSSafeReleaseNULL(laneData);
    core.unmap();usb2.unmap();pipe.unmap();lpdptx.unmap();axi.unmap();dwc.unmap();fuses.unmap();
    IOService::free();
}

int PDAppleUSB::beginAlt(pd_usbc_phy_mode mode,bool flip)
{
    setProperty("phy-start-stage","map-and-calibration");
    altTouched=false;auxTouched=false;
    if (quarantined || !map()) return ENODEV;
    pd_atc_mode target=mode==PD_USBC_PHY_DP?PD_ATC_MODE_DP:
        mode==PD_USBC_PHY_TBT?PD_ATC_MODE_TBT:PD_ATC_MODE_USB4;
    OSSafeReleaseNULL(laneData);laneData=OSDictionary::withCapacity(8);
    pd_atc_cal_bundle bundle={};bundle.io=&atcIO;bundle.adt_ctx=this;bundle.getprop=getLane;
    auto cal=pd_atc_calibration_callbacks(&bundle);
    pd_atc_program program;
    int ret=pd_atc_mode_plan(target,flip,&program);
    if (!ret) ret=cal.validate(cal.ctx,&program);
    if (ret) return ret; // All retained calibration is checked before power/writes.
    setProperty("phy-start-stage","acio-off-check");
    ret=priorACIOOff();
    if (ret) return ret;
    setProperty("phy-start-stage","power-enable");
    ret=pd_pmgr_enable_node(atcPath,-1,0,true);
    if (!ret) ret=pd_pmgr_enable_node(usbPath,-1,0,true);
    if (ret) return ret;
    setProperty("initial-pipe-clock",pipe.r32(0x1c),32);
    setProperty("initial-pipe-mux",pipe.r32(0xc),32);
    setProperty("phy-start-stage","stopped-dwc3-check");
    if (pipe.r32(0x1c)&1) {
        uint32_t cap=dwc.r32(0);unsigned op=cap&0xff;
        setProperty("initial-dwc3-cap",cap,32);
        setProperty("initial-dwc3-control",dwc.r32(0xc704),32);
        if ((dwc.r32(0xc704)&(1u<<31)) || cap==UINT32_MAX || op<0x20 || op>0x100 ||
            (dwc.r32(op)&1) || !(dwc.r32(op+4)&1)) return EBUSY;
    }
    if (!pipeState.dummy_pending) {
        switch(pipe.r32(0xc)&0x3f) {
        case 8:pipeState.backend=PD_ATC_USB3_BACKEND;break;
        case 0x11:pipeState.backend=PD_ATC_USB4_BACKEND;break;
        case 0x22:pipeState.backend=PD_ATC_DUMMY;break;
        default:return EBUSY;
        }
    }
    // Both prior backend checks have passed. No ACIO client has yet
    // received this lease, so initial failure cleanup has no NHI producers.
    altTouched=true;
    directDP={};directDP.io=&atcIO;directDP.lane_mask=program.dp_lanes;directDP.rate=-1;
    tunnel={};tunnel.io=&atcIO;
    setProperty("phy-start-stage","park-prior-phy");
    ret=stopPHY();
    if(!ret && target==PD_ATC_MODE_TBT){
        // priorACIOOff() proved this physical domain off before clocks rose.
        // The exclusive uncommitted lease still has no NHI producer, and
        // stopPHY() has completed the old DWC3/PIPE halt and reset.
        setProperty("phy-start-stage","pipe-bootstrap");
        setProperty("pipe-bootstrap-before-aon",pipe.r32(0x1c),32);
        setProperty("pipe-bootstrap-before-override",pipe.r32(0x20),32);
        ret=priorACIOOff(); // Fresh physical power proof immediately before writes.
        if(!ret)ret=pd_atc_pipe_bootstrap_dummy(&atcIO,&pipeState,1);
        setProperty("pipe-bootstrap-error",(uint32_t)ret,32);
        setProperty("pipe-bootstrap-after-aon",pipe.r32(0x1c),32);
        setProperty("pipe-bootstrap-after-override",pipe.r32(0x20),32);
        if(!ret){
            setProperty("pipe-aon-normalize-before",pipe.r32(0x1c),32);
            ret=priorACIOOff(); // Fresh proof before this separate baseline test.
            if(!ret)ret=pd_atc_pipe_bootstrap_aon(&atcIO,&pipeState,1);
            setProperty("pipe-aon-normalize-error",(uint32_t)ret,32);
            setProperty("pipe-aon-normalize-after",pipe.r32(0x1c),32);
        }
        if(!ret){
            setProperty("phy-start-stage","usb2-cold-baseline");
            setProperty("usb2-bootstrap-before",usb2.r32(0x1c),32);
            ret=priorACIOOff(); // Physical proof immediately before this write.
            if(!ret)ret=pd_atc_usb2_bootstrap_t8103(&atcIO,&pipeState,1);
            setProperty("usb2-bootstrap-error",(uint32_t)ret,32);
            setProperty("usb2-bootstrap-after",usb2.r32(0x1c),32);
        }
    }
    if (!ret) {setProperty("phy-start-stage","configure-mode");ret=pd_atc_mode_start(&atcIO,target,flip,&cal);}
    if (!ret && target==PD_ATC_MODE_USB4) ret=pd_atc_pipe_route(&atcIO,&pipeState,PD_ATC_USB4_BACKEND);
    if (!ret && target==PD_ATC_MODE_DP) {
        auxTouched=true; // Also unwind a partially failed AUX enable.
        ret=pd_atc_dp_aux_start(&directDP);
    }
    if (!ret) tunnel.configured=target==PD_ATC_MODE_TBT || target==PD_ATC_MODE_USB4;
    if (!ret) setProperty("phy-start-stage","configured");
    if(!ret && tunnel.configured){
        // Broker owns the calibrated PHY, prior ACIO was proved off, and
        // this lease has not yet been committed to an NHI producer.
        int diag=snapshotCalibration(true);
        setProperty("phy-pre-acio-calibration-error",(uint32_t)diag,32);
    }
    // DWC3 stays reset and unpublished in all three exclusive alt modes.
    return ret;
}
int PDAppleUSB::stopAlt()
{
    if (!altTouched) return 0;
    int ret=0;
    if (tunnel.clock_on) ret=pd_atc_tunnel_set_rate(&tunnel,0);
    if (!ret && auxTouched) {
        ret=pd_atc_dp_aux_stop(&directDP);
        if (!ret) auxTouched=false;
    }
    if (!ret) ret=stopPHY();
    if (ret) return ret<0?-ret:ret;
    altTouched=false;directDP={};tunnel={};return 0;
}
void PDAppleUSB::queueRevoke()
{
    if (!lease.owner || lease.phase!=PD_USBC_PHY_REVOKED || noticeToken==lease.token) return;
    noticeToken=lease.token;
    setProperty("phy-lease-revoked",true);
    if (revokeCall) thread_call_enter(revokeCall);
}
struct PDUSBCRevokeNotice { IOService *client;uint64_t token; };
IOReturn PDAppleUSB::noticeAction(OSObject *owner,void *arg,void *,void *,void *)
{
    auto self=OSDynamicCast(PDAppleUSB,owner);auto note=(PDUSBCRevokeNotice *)arg;
    if (!self || !note) return kIOReturnBadArgument;
    if (self->lease.owner && self->lease.phase==PD_USBC_PHY_REVOKED && self->altClient) {
        note->client=self->altClient;note->client->retain();note->token=self->lease.token;
    }
    return kIOReturnSuccess;
}
void PDAppleUSB::notifyRevoke(thread_call_param_t owner,thread_call_param_t)
{
    auto self=(PDAppleUSB *)owner;PDUSBCRevokeNotice note={};
    if (self->gate) self->gate->runAction(noticeAction,&note);
    if (note.client) {
        // Outside the physical gate: the client schedules its actual teardown.
        // The notification is never a drain ACK; stale tokens must be ignored.
        note.client->callPlatformFunction(PD_USBC_PHY_REVOKE_FUNCTION,false,&note.token,NULL,NULL,NULL);
        note.client->release();
    }
}
int PDAppleUSB::phyRequest(pd_usbc_phy_request *req)
{
    pd_cd321x_cable_snapshot state={};bool fresh=readCable(state);
    int ret=fresh?observeCable(state):ENODEV;
    if (req->command==PD_USBC_PHY_RELEASE ||
        ((req->command==PD_USBC_PHY_DP_RATE || req->command==PD_USBC_PHY_TUNNEL_RATE) && !req->rate)) ret=0;
    if (ret) goto out;
    if (stopping && req->command!=PD_USBC_PHY_RELEASE && req->command!=PD_USBC_PHY_QUERY && req->rate) { ret=ENODEV;goto out; }
    switch(req->command) {
    case PD_USBC_PHY_QUERY:break;
    case PD_USBC_PHY_ACQUIRE: {
        if (stopping || !__atomic_load_n(&acceptingSnapshots,__ATOMIC_ACQUIRE) || quarantined) { ret=EIO;break; }
        auto client=OSDynamicCast(IOService,(OSObject *)req->owner);
        if (!client || client==this) { ret=EINVAL;break; }
        uint64_t token=0;uint64_t epoch=req->epoch;
        ret=pd_usbc_phy_claim(&lease,client,epoch,(pd_usbc_phy_mode)req->mode,&token);
        if (ret) break;
        altClient=client;altClient->retain();noticeToken=0;
        // A changed cable never authorizes skipping an actual USB drain.
        if (active && !end()) ret=EBUSY;
        if (!ret && (quarantined || (domain && (!domain->healthy() || domain->faults())))) ret=EIO;
        if (!ret) ret=beginAlt(lease.mode,lease.reversed);
        // The HPM observer runs on another loop; recheck after calibration.
        if (!ret) ret=readCable(state)?observeCable(state):ENODEV;
        if (!ret) ret=pd_usbc_phy_commit(&lease,client,token,epoch);
        if (ret) {
            int cleanup=stopAlt();if (cleanup) quarantined=true;
            pd_usbc_phy_release(&lease,client,token,cleanup,cleanup!=0);
            if (!lease.owner) OSSafeReleaseNULL(altClient);
        }
        break;
    }
    case PD_USBC_PHY_RELEASE: {
        if (!pd_usbc_phy_identity(&lease,req->owner,req->token) || !altClient) { ret=ESTALE;break; }
        // This is a NONBLOCKING factual check after the caller has joined all
        // DMA/IRQ/controller/display users. Do not join a client under this gate.
        IOReturn drained=altClient->callPlatformFunction(PD_USBC_PHY_DRAIN_FUNCTION,false,&req->token,NULL,NULL,NULL);
        if (drained!=kIOReturnSuccess) {
            ret=pd_usbc_phy_release(&lease,req->owner,req->token,EBUSY,false);queueRevoke();break;
        }
        ret=stopAlt();if (ret) quarantined=true;
        ret=pd_usbc_phy_release(&lease,req->owner,req->token,ret,ret!=0);
        if (!ret) OSSafeReleaseNULL(altClient);
        break;
    }
    case PD_USBC_PHY_DP_RATE:case PD_USBC_PHY_TUNNEL_RATE: {
        if (!pd_usbc_phy_identity(&lease,req->owner,req->token)) { ret=ESTALE;break; }
        if (req->rate && (ret=pd_usbc_phy_live(&lease,req->owner,req->token))) break;
        if (req->command==PD_USBC_PHY_DP_RATE && lease.mode==PD_USBC_PHY_DP)
            ret=pd_atc_dp_set_rate(&directDP,req->rate);
        else if (req->command==PD_USBC_PHY_TUNNEL_RATE && (lease.mode==PD_USBC_PHY_TBT || lease.mode==PD_USBC_PHY_USB4) && req->rate<=UINT8_MAX)
            ret=pd_atc_tunnel_set_rate(&tunnel,(uint8_t)req->rate);
        else ret=EINVAL;
        if (ret<0) ret=-ret;
        if (ret && ret!=EINVAL) { lease.quarantined=true;quarantined=true;lease.phase=PD_USBC_PHY_REVOKED;queueRevoke(); }
        break;
    }
    case PD_USBC_PHY_SNAPSHOT: {
        if (stopping || !mapped || quarantined) { ret=EIO;break; }
        ret=pd_usbc_phy_live(&lease,req->owner,req->token);if(ret)break;
        uint32_t offsets[]={0x8,0x48,0x4c,0x1b0,0x20000,0x20004,0x20008,0x2a00,0x9010,0x10010,0x84,0x8c,0x90,0x9c,0xa8,0xac,0x1014,0x1028,0x17000,0x17004,0x17080,0x17088,0x1708c};
        uint32_t values[sizeof(offsets)/sizeof(offsets[0])];
        for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i)values[i]=core.r32(offsets[i]);
        uint32_t pipeOffsets[]={0xc,0x10,0x14,0x1c,0x20};
        uint32_t pipeValues[sizeof(pipeOffsets)/sizeof(pipeOffsets[0])];
        for(unsigned i=0;i<sizeof(pipeOffsets)/sizeof(pipeOffsets[0]);++i)pipeValues[i]=pipe.r32(pipeOffsets[i]);
        // These are ordinary T8103 control registers already read by the
        // shared power sequence, not status/clear-on-read registers. Capture
        // them only under the same live exclusive lease as the core/PIPE
        // snapshot; no AUX enable or USB2 role change is performed here.
        uint32_t auxOffsets[]={0,0x4000},auxValues[2];
        for(unsigned i=0;i<2;++i)auxValues[i]=lpdptx.r32(auxOffsets[i]);
        uint32_t usb2Offsets[]={0,4,8,0x1c},usb2Values[4];
        for(unsigned i=0;i<4;++i)usb2Values[i]=usb2.r32(usb2Offsets[i]);
        bool ok=setProperty("phy-snapshot-offsets",offsets,sizeof(offsets)) &&
            setProperty("phy-snapshot-values",values,sizeof(values)) &&
            setProperty("pipe-snapshot-offsets",pipeOffsets,sizeof(pipeOffsets)) &&
            setProperty("pipe-snapshot-values",pipeValues,sizeof(pipeValues)) &&
            setProperty("aux-snapshot-offsets",auxOffsets,sizeof(auxOffsets)) &&
            setProperty("aux-snapshot-values",auxValues,sizeof(auxValues)) &&
            setProperty("usb2-snapshot-offsets",usb2Offsets,sizeof(usb2Offsets)) &&
            setProperty("usb2-snapshot-values",usb2Values,sizeof(usb2Values));
        ret=ok?snapshotCalibration():ENOMEM;
        setProperty("phy-calibration-error",(uint32_t)ret,32);
        break;
    }
    default:ret=EINVAL;
    }
out:
    req->epoch=lease.epoch;req->eligible=lease.eligible;req->reversed=lease.reversed;
    req->phase=lease.phase;req->quarantined=quarantined||lease.quarantined;
    req->token=lease.owner==req->owner?lease.token:0;req->error=ret;
    setProperty("phy-lease-active",lease.owner!=NULL);
    setProperty("phy-lease-revoked",lease.phase==PD_USBC_PHY_REVOKED);
    setProperty("phy-mode",(uint32_t)lease.mode,32);
    setProperty("phy-quarantined",req->quarantined);
    return ret;
}
IOReturn PDAppleUSB::phyAction(OSObject *owner,void *arg,void *,void *,void *)
{
    auto self=OSDynamicCast(PDAppleUSB,owner);
    if (!self || !arg) return kIOReturnBadArgument;
    int ret=self->phyRequest((pd_usbc_phy_request *)arg);
    return ret?kIOReturnNotReady:kIOReturnSuccess;
}
IOReturn PDAppleUSB::callPlatformFunction(const OSSymbol *name,bool wait,void *a,void *b,void *c,void *d)
{
    if (name && name->isEqualTo(PD_USBC_PHY_FUNCTION)) {
        auto req=(pd_usbc_phy_request *)a;
        if (!req || req->version!=PD_USBC_PHY_ABI || req->size!=sizeof(*req) || req->port!=1) return kIOReturnBadArgument;
        if (!gate) { req->error=ENODEV;return kIOReturnNotReady; }
        return gate->runAction(phyAction,req);
    }
    return IOService::callPlatformFunction(name,wait,a,b,c,d);
}
