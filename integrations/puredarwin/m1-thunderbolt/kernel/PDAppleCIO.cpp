// SPDX-License-Identifier: GPL-2.0
/* T8103 port-1 ACIO owner. Startup order/registers from Linux apple.c and
 * reset-apple-cio.c, Copyright (c) Sven Peter <sven@kernel.org> and
 * Copyright The Asahi Linux Contributors. No firmware or calibration shipped.
 * Firmware-ready is distinct from NHI/domain/PCI readiness.
 */
#include "pd_soc.h"
#ifdef PD_CIO_CM
#include "pd_cio_cm.h"
#include "pd_cd321x.h"
#endif
#include "pd_cio_rtkit.h"
#include "pd_cio_tunables.h"
#include "pd_usbc_phy_lease.h"
#include "pd_rtkit_platform.h"
#include "pd_fbsd_os.h"
#include "pd_fbsd_xnu.h"
#include <IOKit/IOService.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMessage.h>
#include <pexpert/pexpert.h>
int pd_cio_power_off();
int pd_cio_pcie_reset();
#define LOG(fmt,...) IOLog("PDAppleCIO: " fmt "\n",##__VA_ARGS__)
class PDAppleCIO: public IOService {
 OSDeclareDefaultStructors(PDAppleCIO)
 IOWorkLoop *loop;
 IOCommandGate *gate;
 IOTimerEventSource *timer;
 PDMmio rc,nhi,pdf,adapter,sram,reset;
 OSDictionary *tunableData;
 IORegistryEntry *cpuEntry;
 pd_cio_tunables tunables;
 pd_cio_tunable_io tuning;
 pd_cio_rtkit *rtkit;
 uint64_t leaseToken,attemptedEpoch;
 uint64_t drainToken,revokeToken;
 uint32_t accepting,drained;
 bool power,rtkitTouched,firmwareReady,quarantined,stopping;
#ifdef PD_CIO_CM
 PDCIOCM *cm;
 uint32_t cmLive;
 static bool cmPowered(void *);
 int startCM();
#endif
 int mapResources();
 int preparePCIe();
 int deassertReset();
 int startFirmware(pd_usbc_phy_request &);
 int stopFirmware();
 void step();
 void report(int);
 static void tick(OSObject *,IOTimerEventSource *);
 static IOReturn stopAction(OSObject *,void *,void *,void *,void *);
 static const void *getprop(void *,const char *,const char *,size_t *);
 static uint32_t read(void *,pd_cio_window,uint32_t);
 static void write(void *,pd_cio_window,uint32_t,uint32_t);
 PDMmio &window(pd_cio_window w){return w==PD_CIO_RC?rc:w==PD_CIO_NHI?nhi:adapter;}
 pd_usbc_phy_request request(unsigned command){
  pd_usbc_phy_request r={};r.version=PD_USBC_PHY_ABI;r.size=sizeof(r);
  r.command=command;r.port=1;r.owner=this;r.token=leaseToken;return r;
 }
 int phy(pd_usbc_phy_request &r){
  IOReturn ret=getProvider()->callPlatformFunction(PD_USBC_PHY_FUNCTION,false,&r,NULL,NULL,NULL);
  return r.error?r.error:(ret==kIOReturnSuccess?0:EIO);
 }
public:
 IOService *probe(IOService *,SInt32 *) override;
 bool start(IOService *) override;
 void stop(IOService *) override;
 void free() override;
 IOReturn message(UInt32,IOService *,void *) override;
 IOReturn callPlatformFunction(const OSSymbol *,bool,void *,void *,void *,void *) override;
};
OSDefineMetaClassAndStructors(PDAppleCIO,IOService)
const void *PDAppleCIO::getprop(void *ctx,const char *path,const char *name,size_t *len)
{
 auto s=(PDAppleCIO *)ctx;*len=0;
 if(strcmp(path,"/arm-io/acio1") || !s->tunableData)return NULL;
 auto data=OSDynamicCast(OSData,s->tunableData->getObject(name));
 if(!data){
  auto entry=IORegistryEntry::fromPath(path,gIODTPlane);
  OSObject *v=entry?entry->copyProperty(name):NULL;
  auto source=OSDynamicCast(OSData,v);
  auto copy=source?OSData::withBytes(source->getBytesNoCopy(),source->getLength()):NULL;
  bool ok=copy && s->tunableData->setObject(name,copy);
  OSSafeReleaseNULL(copy);OSSafeReleaseNULL(v);OSSafeReleaseNULL(entry);
  if(!ok)return NULL;
  data=OSDynamicCast(OSData,s->tunableData->getObject(name));
 }
 *len=data->getLength();return data->getBytesNoCopy();
}
uint32_t PDAppleCIO::read(void *ctx,pd_cio_window w,uint32_t off){return ((PDAppleCIO *)ctx)->window(w).r32(off);}
void PDAppleCIO::write(void *ctx,pd_cio_window w,uint32_t off,uint32_t v){((PDAppleCIO *)ctx)->window(w).w32(off,v);}
int PDAppleCIO::mapResources()
{
 if(rtkit)return 0; // Immutable validated resources/context retained for this boot.
 setProperty("resource-stage","acio-reg-layout");
 OSSafeReleaseNULL(cpuEntry);OSSafeReleaseNULL(tunableData);
 PDMmio rc1,rc2;
 if(!nhi.mapNode("/arm-io/acio1",0) || nhi.phys!=0x501f00000ULL || nhi.len<0x100000 ||
    !pdf.mapNode("/arm-io/acio1",1) || pdf.phys!=0x501db0000ULL || pdf.len<0x30004 ||
    !rc.mapNode("/arm-io/acio1",2) || rc.phys!=0x501ac0000ULL || rc.len!=0x4000 ||
    !rc1.mapNode("/arm-io/acio1",3) || rc1.phys!=rc.phys+0x4000 || rc1.len!=0x4000 ||
    !rc2.mapNode("/arm-io/acio1",4) || rc2.phys!=rc.phys+0x8000 || rc2.len!=0x4000 ||
    !adapter.mapNode("/arm-io/acio1",5) || adapter.phys!=0x501e44000ULL || adapter.len<0x4000){
  rc1.unmap();rc2.unmap();return ENODEV;
 }
 rc1.unmap();rc2.unmap();rc.unmap();
 if(!rc.mapPhys(0x501ac0000ULL,0xc000))return ENOMEM;
 setProperty("resource-stage","m3-compatible");
 cpuEntry=IORegistryEntry::fromPath("/arm-io/acio-cpu1",gIODTPlane);
 auto compatible=OSString::withCString("iop,m3wrap-v2-acio");
 bool cpuMatch=cpuEntry && compatible && IODTCompareNubName(cpuEntry,compatible,NULL);
 OSSafeReleaseNULL(compatible);
 if(!cpuMatch)return ENODEV;
 setProperty("resource-stage","sram-segment");
 auto nub=IORegistryEntry::fromPath("/arm-io/acio-cpu1/iop-acio1-nub",gIODTPlane);
 auto segments=nub?OSDynamicCast(OSData,nub->getProperty("segment-ranges")):NULL;
 auto names=nub?OSDynamicCast(OSData,nub->getProperty("segment-names")):NULL;
 bool valid=false;
 // m1n1 ADT: phys, iova, remap, u32 size, u32 flags. Require the exact
 // qualified T8103 SRAM segment and exclude firmware text. Never read text.
 if(segments && segments->getLength()==64 && names && names->getLength()==14 &&
    !memcmp(names->getBytesNoCopy(),"__TEXT;__DATA",14)){
  const uint8_t *d=(const uint8_t *)segments->getBytesNoCopy()+32;
  uint64_t pa=OSReadLittleInt64(d,0),iova=OSReadLittleInt64(d,8),remap=OSReadLittleInt64(d,16);
  uint32_t span=OSReadLittleInt32(d,24),flags=OSReadLittleInt32(d,28);
  valid=pa==0x501080000ULL && iova==0x10000000 && remap==iova && span==0x20000 && !flags && sram.mapPhys(pa,span);
 }
 OSSafeReleaseNULL(nub);
 if(!valid)return EINVAL;
 setProperty("resource-stage","pmgr-reset-window");
 if(!reset.mapNode("/arm-io/pmgr",0) || reset.phys!=0x23b700000ULL || reset.len<0x88004)return ENODEV;
 setProperty("resource-stage","tunables");
 tunableData=OSDictionary::withCapacity(11);
 if(!tunableData)return ENOMEM;
 tuning={this,getprop,read,write};
 int ret=pd_cio_tunables_load(&tunables,&tuning);
 if(ret)return ret;
 // Never introduce the RTKit compatibility layer's 2 ms polling mode.
 if(pd_rtkit_platform_bootarg("pdrtkit_irq",2)!=2)return EOPNOTSUPP;
 setProperty("resource-stage","mailbox-platform");
 ret=pd_rtkit_platform_init(this);
 if(ret)return ret<0?-ret:ret;
 pd_rtkit_platform_set_log_prefix("PDAppleCIO: rtkit: ");
 rtkit=pd_cio_rtkit_create(cpuEntry,(void *)sram.va,sram.phys,sram.len);
 if(rtkit)setProperty("resource-stage","complete");
 return rtkit?0:ENOMEM;
}
int PDAppleCIO::preparePCIe()
{
 auto node=IORegistryEntry::fromPath("/arm-io/apciec1",gIODTPlane);
 auto dart=IORegistryEntry::fromPath("/arm-io/dart-apciec1",gIODTPlane);
 if(!node || !dart){OSSafeReleaseNULL(node);OSSafeReleaseNULL(dart);return ENODEV;}
 auto clients=node->getChildIterator(gIOServicePlane);
 auto dmaClients=dart->getChildIterator(gIOServicePlane);
 bool bound=(clients && clients->getNextObject()) || (dmaClients && dmaClients->getNextObject());
 OSSafeReleaseNULL(clients);OSSafeReleaseNULL(dmaClients);OSSafeReleaseNULL(node);OSSafeReleaseNULL(dart);
 if(bound)return EBUSY;
 int ret=pd_pmgr_enable_node("/arm-io/apciec1",-1,0,true);
 return ret?ret:pd_cio_pcie_reset();
}
int PDAppleCIO::deassertReset()
{
 // Port 1 only, Linux reset-apple-cio.c: bounded pre-poll is advisory;
 // write INIT_REQ then require exactly INIT_DONE within 100 ms.
 for(unsigned i=0;i<1000 && (reset.r32(0x88000)&2);++i)IODelay(100);
 reset.w32(0x88000,1);
 for(unsigned i=0;i<1000;++i){if(reset.r32(0x88000)==4)return 0;IODelay(100);}
 return ETIMEDOUT;
}
int PDAppleCIO::startFirmware(pd_usbc_phy_request &q)
{
 int ret=mapResources();if(ret)return ret;
 q.command=PD_USBC_PHY_ACQUIRE;q.mode=(q.eligible&PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_TBT))?PD_USBC_PHY_TBT:PD_USBC_PHY_USB4;
 ret=phy(q);
 // A failed PHY setup can still retain this client's lease when cleanup
 // failed. Keep that token and drain identity even on an error response.
 leaseToken=q.token;
 if(!ret && !leaseToken)ret=EPROTO;
 __atomic_store_n(&drainToken,leaseToken,__ATOMIC_RELEASE);
 __atomic_store_n(&drained,0,__ATOMIC_RELEASE);
 if(ret)return ret;
 ret=preparePCIe();if(ret)return ret;
 power=true; // Also unwind a partially failed enable.
 ret=pd_pmgr_enable_node("/arm-io/acio1",-1,0,true);if(ret)return ret;
 ret=deassertReset();if(ret)return ret;
 rc.mask32(0x0c,0,2); // preserve undocumented iBoot bits; request M3 startup
 rtkitTouched=true;
 ret=pd_cio_rtkit_boot(rtkit);if(ret)return ret<0?-ret:ret;
 for(unsigned i=0;i<5000;++i){
  uint32_t state=rc.r32(0xa8);
  if(state!=UINT32_MAX && (state&0x7f000000)){
   firmwareReady=true;setProperty("m3-firmware-state",state,32);
   ret=pd_cio_tunables_apply(&tunables,&tuning,PD_CIO_RC);
   if(!ret)ret=pd_cio_tunables_apply(&tunables,&tuning,PD_CIO_PCIE_ADAPTER);
   if(ret)return ret;
   setProperty("nhi-hop-count",nhi.r32(0)&0x3ff,32);
   setProperty("nhi-interrupt-enable",nhi.r32(0xd0010),32);
   setProperty("rtkit-ready",true);
   // NHI/domain attachment is a separate, still absent stage. No ring/DART
   // is programmed and no PCI device is published by this firmware stage.
   setProperty("nhi-ready",false);
#ifdef PD_CIO_CM
   if(pd_rtkit_platform_bootarg("pdcio_cm",0)==1){
    __atomic_store_n(&cmLive,1,__ATOMIC_RELEASE);
    ret=startCM();if(ret)return ret;
   }
#endif
   return 0;
  }
  IODelay(100);
 }
 return ETIMEDOUT;
}
int PDAppleCIO::stopFirmware()
{
#ifdef PD_CIO_CM
 if(cm){int ret=pd_cio_cm_stop(cm);if(ret)return ret;}
 if(power && firmwareReady){
  setProperty("nhi-interrupt-enable",nhi.r32(0xd0010),32);
  setProperty("nhi-control-tx-options",nhi.r32(0x10010),32);
  setProperty("nhi-control-rx-options",nhi.r32(0x80010),32);
 }
 __atomic_store_n(&cmLive,0,__ATOMIC_RELEASE);setProperty("nhi-ready",false);
#endif
 firmwareReady=false;setProperty("rtkit-ready",false);
 if(rtkitTouched){
  int ret=pd_cio_rtkit_stop(rtkit);if(ret)return ret<0?-ret:ret;
  rtkitTouched=false;
 }
 if(power){int ret=pd_cio_power_off();if(ret)return ret;power=false;}
 __atomic_store_n(&drained,1,__ATOMIC_RELEASE);
 if(leaseToken){
  auto q=request(PD_USBC_PHY_RELEASE);int ret=phy(q);if(ret)return ret;
  leaseToken=0;__atomic_store_n(&drainToken,0,__ATOMIC_RELEASE);
 }
 return 0;
}
void PDAppleCIO::report(int ret){setProperty("last-error",(uint32_t)ret,32);setProperty("phy-owned",leaseToken!=0);setProperty("quarantined",quarantined);if(ret)LOG("port1 stage error=%d, PHY retained=%d",ret,leaseToken!=0);}
void PDAppleCIO::step()
{
 if(stopping || quarantined)return;
 auto q=request(PD_USBC_PHY_QUERY);int ret=phy(q);
 if(leaseToken){
  if(q.phase==PD_USBC_PHY_REVOKED || __atomic_load_n(&revokeToken,__ATOMIC_ACQUIRE)==leaseToken){
   ret=stopFirmware();if(ret)quarantined=true;report(ret);
   if(!ret)timer->setTimeoutMS(1);
  }
  return;
 }
 if(ret){report(ret);return;}
 if(q.epoch==attemptedEpoch || !(q.eligible&(PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_TBT)|PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_USB4))))return;
 attemptedEpoch=q.epoch;
 ret=startFirmware(q);
 if(ret){int cleanup=stopFirmware();if(cleanup)quarantined=true;}
 report(ret);
}
void PDAppleCIO::tick(OSObject *owner,IOTimerEventSource *){auto s=OSDynamicCast(PDAppleCIO,owner);if(s)s->step();}
IOService *PDAppleCIO::probe(IOService *p,SInt32 *score)
{
 int enabled=0;uint32_t chip=0;
 if(!PE_parse_boot_argn("pdcio",&enabled,sizeof(enabled)) || enabled!=1 ||
    !pd_adt_u32("/chosen","chip-id",0,&chip) || chip!=0x8103)return NULL;
 return IOService::probe(p,score);
}
bool PDAppleCIO::start(IOService *p)
{
 if(!IOService::start(p))return false;
 pdos_xnu_init();if(pd_cio_layer_up())return false;
 loop=IOWorkLoop::workLoop();gate=IOCommandGate::commandGate(this);timer=IOTimerEventSource::timerEventSource(this,tick);
 if(!loop || !gate || !timer || loop->addEventSource(gate)!=kIOReturnSuccess || loop->addEventSource(timer)!=kIOReturnSuccess)return false;
 setProperty("port-index",1,32);setProperty("rtkit-ready",false);setProperty("nhi-ready",false);
 __atomic_store_n(&accepting,1,__ATOMIC_RELEASE);__atomic_store_n(&drained,1,__ATOMIC_RELEASE);
 registerService();timer->setTimeoutMS(1);return true;
}
IOReturn PDAppleCIO::message(UInt32 type,IOService *p,void *a)
{
 if(type==kIOMessageServicePropertyChange && p==getProvider()){
  if(__atomic_load_n(&accepting,__ATOMIC_ACQUIRE) && timer)timer->setTimeoutMS(1);
  return kIOReturnSuccess;
 }
 return IOService::message(type,p,a);
}
IOReturn PDAppleCIO::callPlatformFunction(const OSSymbol *name,bool wait,void *a,void *b,void *c,void *d)
{
 if(name && name->isEqualTo(PD_USBC_PHY_DRAIN_FUNCTION)){
  return a && __atomic_load_n(&drained,__ATOMIC_ACQUIRE) && *(uint64_t *)a==__atomic_load_n(&drainToken,__ATOMIC_ACQUIRE)?kIOReturnSuccess:kIOReturnBusy;
 }
 if(name && name->isEqualTo(PD_USBC_PHY_REVOKE_FUNCTION)){
  if(!a)return kIOReturnBadArgument;
  __atomic_store_n(&revokeToken,*(uint64_t *)a,__ATOMIC_RELEASE);
  if(__atomic_load_n(&accepting,__ATOMIC_ACQUIRE) && timer)timer->setTimeoutMS(1);
  return kIOReturnSuccess;
 }
 return IOService::callPlatformFunction(name,wait,a,b,c,d);
}
IOReturn PDAppleCIO::stopAction(OSObject *owner,void *,void *,void *,void *)
{
 auto s=OSDynamicCast(PDAppleCIO,owner);if(!s)return kIOReturnBadArgument;
 s->stopping=true;int ret=s->stopFirmware();
 if(ret){s->quarantined=true;s->retain();}s->report(ret);
 return ret?kIOReturnBusy:kIOReturnSuccess;
}
void PDAppleCIO::stop(IOService *p)
{
 __atomic_store_n(&accepting,0,__ATOMIC_RELEASE);
 if(timer){timer->cancelTimeout();timer->disable();if(loop)loop->removeEventSource(timer);}
 if(gate)gate->runAction(stopAction);else stopAction(this,NULL,NULL,NULL,NULL);
 IOService::stop(p);
}
void PDAppleCIO::free()
{
 if(leaseToken || power || rtkitTouched || quarantined){LOG("retaining failed-drain resources");return;}
 if(loop && gate)loop->removeEventSource(gate);
 OSSafeReleaseNULL(timer);OSSafeReleaseNULL(gate);OSSafeReleaseNULL(loop);
 OSSafeReleaseNULL(tunableData);OSSafeReleaseNULL(cpuEntry);
 rc.unmap();nhi.unmap();pdf.unmap();adapter.unmap();sram.unmap();reset.unmap();IOService::free();
}

#ifdef PD_CIO_CM
bool PDAppleCIO::cmPowered(void *ctx)
{
 auto s=(PDAppleCIO *)ctx;
 if(!__atomic_load_n(&s->cmLive,__ATOMIC_ACQUIRE))return false;
 // Actual ATC-CIO, AON-CIO and AUSB-CIO power states, plus NHI accessibility.
 const unsigned gates[]={0x458,0x460,0x468};
 for(unsigned off:gates)if((s->reset.r32(off)&0xf0)!=0xf0)return false;
 return (s->nhi.r32(0)&0x3ff)==12;
}
int PDAppleCIO::startCM()
{
 auto typec=getProvider()?getProvider()->getProvider():NULL;
 OSObject *value=typec?typec->copyProperty("port-state"):NULL;
 auto data=OSDynamicCast(OSData,value);pd_cd321x_cable_snapshot snapshot={};uint32_t cable=0;
 int ret=data?pd_cd321x_snapshot_decode(data->getBytesNoCopy(),data->getLength(),&snapshot):ENODEV;
 if(!ret)ret=pd_cd321x_tbt_cable_info(&snapshot,&cable);
 OSSafeReleaseNULL(value);if(ret)return ret;
 // USB4 EUDO support is a separate stage. Never guess a TBT cable word.
 setProperty("cm-atc-cio-power",reset.r32(0x458),32);
 setProperty("cm-aon-cio-power",reset.r32(0x460),32);
 setProperty("cm-ausb-cio-power",reset.r32(0x468),32);
 if(!cm)cm=pd_cio_cm_create(this,this,cmPowered);
 if(!cm)return EIO;
 ret=pd_cio_cm_start(cm,(void *)nhi.va,(void *)pdf.va,cable);
 setProperty("nhi-cable-info",cable,32);setProperty("nhi-ready",ret==0);
 setProperty("nhi-interrupt-enable",nhi.r32(0xd0010),32);
 setProperty("nhi-control-tx-options",nhi.r32(0x10010),32);
 setProperty("nhi-control-rx-options",nhi.r32(0x80010),32);
 return ret;
}
#endif
