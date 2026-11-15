// SPDX-License-Identifier: MIT
/* Actual IORegistry, reference-counted runtime policy and DART callbacks for
 * the private Linux connection manager. The cable lease pins controller power;
 * autosuspend hints are recorded, never reported as a hardware power-off.
 * All objects/resources live until reboot; failed drains retain ownership.
 */
#include "pd_cio_cm.h"
#include "pd_cio_dart.h"
#include "pd_tb_native_abi.h"
#include "pd_tb_pm.h"
#include "pd_tb_dma.h"
#include "pd_tb_nhi.h"
#include "pd_rtkit_platform.h"
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLocks.h>
#include <libkern/libkern.h>
#include <mach/mach_time.h>
#include <sys/errno.h>
extern "C" {
// XNU libkern/sysctl.h private KPI; present in the exact installed kernel.
int kernel_sysctlbyname(const char *,void *,size_t *,void *,size_t);
extern char hostname[]; // actual XNU export; no compatibility-layer homonym
int pd_cio_nhi_irqs_resolve(int [25]);
void pd_tb_native_fault(int);
}
struct PDCIOCM {
 IOService *owner;
 IORegistryEntry *entry;
 void *powerCtx,*device,*nhi;
 bool (*powered)(void *);
 IOLock *registryLock,*stateLock,*dmaLock;
 PDCIODart dart;
 int irqs[25];
 bool dmaLive,irqLive,busReady;
 uint32_t failed;
 struct Slot {
  void *device;IOService *node;
  pd_tb_pm_state pm;
 } slots[128];
 Slot *slot(void *dev,bool create){
  for(auto &s:slots)if(s.device==dev)return &s;
  if(create)for(auto &s:slots)if(!s.device){s={};s.device=dev;pd_tb_pm_init(&s.pm);return &s;}
  return NULL;
 }
 bool live(){return pd_tb_native_ready() && !__atomic_load_n(&failed,__ATOMIC_ACQUIRE) && powered(powerCtx);}
 void fault(){__atomic_store_n(&failed,1,__ATOMIC_RELEASE);pd_tb_dma_quarantine();pd_tb_native_fault(-EIO);}
 static void lock(void *p){IOLockLock(((PDCIOCM *)p)->registryLock);}
 static void unlock(void *p){IOLockUnlock(((PDCIOCM *)p)->registryLock);}
 static void dma_lock(void *p){IOLockLock(((PDCIOCM *)p)->dmaLock);}
 static void dma_unlock(void *p){IOLockUnlock(((PDCIOCM *)p)->dmaLock);}
 static bool power(void *p){auto s=(PDCIOCM *)p;return s->live() && s->dart.healthy();}
 static int alloc(void *p,size_t bytes,size_t align,void **cpu,uint64_t *iova,void **token){
  auto s=(PDCIOCM *)p;
  if(!power(p))return -EIO;
  if(align<PD_DART_PAGE)align=PD_DART_PAGE;
  auto mem=PDCIODMABuffer::allocate(&s->dart,bytes,align);
  if(!mem)return s->dart.healthy()?-ENOMEM:-EIO;
  *cpu=mem->getBytesNoCopy();*iova=mem->dmaAddress();*token=mem;return 0;
 }
 static int retire(void *,void *token){
  auto mem=(PDCIODMABuffer *)token;int ret=mem->retire();
  if(ret)return -ret;
  mem->release();return 0;
 }
 static int osInfo(void *,void *out,size_t size){
  struct Uts {char fields[6][65];};
  if(!out || size!=sizeof(Uts))return -EINVAL;
  Uts v={};const char *names[]={"kern.ostype","kern.hostname","kern.osrelease","kern.osversion"};
  for(unsigned i=0;i<4;++i){
   // kern.hostname deliberately lacks CTLFLAG_KERN in XNU. A kernel sysctl
   // call returns EPERM even though userspace can read it. Use the actual
   // exported hostname, as Linux drivers read their live UTS string; reject
   // truncation. The other three MIBs explicitly allow kernel callers.
   if(i==1){
    size_t n=strnlen(hostname,sizeof(v.fields[i]));if(n==sizeof(v.fields[i]))return -EOVERFLOW;
    memcpy(v.fields[i],hostname,n+1);continue;
   }
   size_t n=sizeof(v.fields[i]);int ret=kernel_sysctlbyname(names[i],v.fields[i],&n,NULL,0);
   if(ret)return ret>0?-ret:ret;
   if(!n || n>sizeof(v.fields[i]) || !memchr(v.fields[i],0,n))return -EOVERFLOW;
  }
  // Hardware is qualified T8103/ARM64; no NIS domain is supplied by this CM.
  memcpy(v.fields[4],"arm64",6);
  memcpy(out,&v,sizeof(v));return 0;
 }
 static int publish(void *p,void *dev,void *parent,const char *name,const char *type,void **token){
  auto s=(PDCIOCM *)p;
  if(!dev || !token || !name || !*name || strnlen(name,128)==128 || !s->live())return -EINVAL;
  IOLockLock(s->stateLock);
  auto slot=s->slot(dev,true);auto par=s->slot(parent,false);
  IOService *provider=parent==s->device?s->owner:(par?par->node:NULL);
  bool ok=slot && !slot->node && provider;
  if(ok)provider->retain();IOLockUnlock(s->stateLock);
  if(!ok)return -ENODEV;
  auto node=new IOService;
  ok=node && node->init();
  if(ok){
   node->setName(name);
   ok=node->setProperty("thunderbolt-device-type",type?type:"device") &&
      node->setProperty("runtime-policy","cable-power-held") && node->attach(provider);
  }
  provider->release();
  if(!ok){OSSafeReleaseNULL(node);return -ENOMEM;}
  IOLockLock(s->stateLock);slot->node=node;IOLockUnlock(s->stateLock);
  node->registerService();*token=slot;return 0;
 }
 static void unpublish(void *p,void *token){
  auto s=(PDCIOCM *)p;auto slot=(Slot *)token;
  IOLockLock(s->stateLock);auto node=slot?slot->node:NULL;
  if(node)node->retain();IOLockUnlock(s->stateLock);
  if(!node){s->fault();return;}
  if(!node->terminate(kIOServiceRequired|kIOServiceSynchronous)){node->release();s->fault();return;}
  IOLockLock(s->stateLock);slot->node=NULL;IOLockUnlock(s->stateLock);
  if(node->getProvider())node->detach(node->getProvider());
  node->release();node->release();
 }
 static void forget(void *p,void *dev){
  auto s=(PDCIOCM *)p;IOLockLock(s->stateLock);auto slot=s->slot(dev,false);
  bool pending=slot && slot->node;
  if(slot && !pending)*slot={};
  IOLockUnlock(s->stateLock);if(pending)s->fault();
 }
 static int event(void *p,void *token,int action,const char *const *env,size_t count){
  auto s=(PDCIOCM *)p;auto slot=(Slot *)token;
  if(!slot || action<0 || action>2 || count>32)return -EINVAL;
  auto values=OSArray::withCapacity(count?count:1);if(!values)return -ENOMEM;
  bool ok=true;
  for(size_t i=0;i<count && ok;++i){
   if(!env || !env[i] || strnlen(env[i],2048)==2048){ok=false;break;}
   auto value=OSString::withCString(env[i]);ok=value && values->setObject(value);OSSafeReleaseNULL(value);
  }
  IOLockLock(s->stateLock);auto node=slot->node;if(node)node->retain();IOLockUnlock(s->stateLock);
  if(ok && node)ok=node->setProperty("thunderbolt-event",action,32) && node->setProperty("thunderbolt-event-values",values);
  else ok=false;
  OSSafeReleaseNULL(values);OSSafeReleaseNULL(node);return ok?0:-EIO;
 }
 static int runtime(void *p,void *dev,pd_tb_pm_op op,int arg){
  auto s=(PDCIOCM *)p;if(!dev || !s->live())return -EIO;
  IOLockLock(s->stateLock);auto v=s->slot(dev,true);int ret=0;
  if(!v){IOLockUnlock(s->stateLock);return -ENOMEM;}
  ret=pd_tb_pm_update(&v->pm,op,arg,v->node!=NULL,mach_absolute_time());
  IOLockUnlock(s->stateLock);return ret;
 }
 static int dartInterrupt(void *p){
  auto s=(PDCIOCM *)p;
  pd_rtkit_platform_irq_disable_nosync(s->irqs[24]);
  uint32_t raw=s->dart.faults();s->owner->setProperty("nhi-dart-fault",raw,32);
  s->fault();return 1; // retain backing and power for diagnosis, no retry loop
 }
};
PDCIOCM *pd_cio_cm_create(IOService *owner,void *ctx,bool (*powered)(void *))
{
 if(!owner)return NULL;
 auto failure=[owner](const char *stage,int error)->PDCIOCM *{
  owner->setProperty("cm-create-stage",stage);owner->setProperty("cm-create-error",(uint32_t)(error<0?-error:error),32);
  IOLog("PDAppleCIO: CM create %s failed errno=%d\n",stage,error);return NULL;
 };
 owner->setProperty("cm-create-stage","power");
 if(!powered || !powered(ctx))return failure("power",EIO);
 auto s=new PDCIOCM{};if(!s)return failure("context",ENOMEM);
 s->owner=owner;owner->retain();s->powerCtx=ctx;s->powered=powered;
 s->registryLock=IOLockAlloc();s->stateLock=IOLockAlloc();s->dmaLock=IOLockAlloc();
 s->entry=IORegistryEntry::fromPath("/arm-io/acio1",gIODTPlane);
 // Keep any partially installed callback context pinned until reboot.
 if(!s->registryLock || !s->stateLock || !s->dmaLock || !s->entry)return failure("resources",ENOMEM);
 pd_tb_native_ops ops={s,PDCIOCM::lock,PDCIOCM::unlock,PDCIOCM::publish,PDCIOCM::unpublish,PDCIOCM::event,PDCIOCM::runtime,PDCIOCM::osInfo,PDCIOCM::forget};
 OSObject *raw=s->entry->copyProperty("thunderbolt-drom");
 auto drom=OSDynamicCast(OSData,raw);
 int ret=drom && drom->getLength()==76?0:EINVAL;
 if(ret){OSSafeReleaseNULL(raw);return failure("drom",ret);}
 owner->setProperty("cm-create-stage","native-install");
 ret=pd_tb_native_install(&ops);
 if(ret){OSSafeReleaseNULL(raw);return failure("native-install",ret);}
 ret=pd_tb_nhi_device_create(s->entry,drom->getBytesNoCopy(),drom->getLength(),&s->device);
 OSSafeReleaseNULL(raw);if(ret)return failure("parent-device",ret);
 pd_tb_dma_ops dma={s,s->device,PDCIOCM::dma_lock,PDCIOCM::dma_unlock,PDCIOCM::power,PDCIOCM::alloc,PDCIOCM::retire};
 ret=pd_tb_dma_install(&dma);if(ret)return failure("dma-install",ret);
 ret=pd_tb_nhi_bus_init();if(ret)return failure("bus-init",ret);
 s->busReady=true;owner->setProperty("cm-create-stage","complete");owner->setProperty("cm-create-error",(uint32_t)0,32);return s;
}
int pd_cio_cm_start(PDCIOCM *s,void *nhi,void *pdf,uint32_t cable)
{
 if(!s || !s->busReady || s->nhi || !s->live())return EINVAL;
 // Require pristine actual hardware before attaching/resetting its DART.
 auto base=(volatile uint32_t *)nhi;
 if(!base || !pdf || (base[0]&0x3ff)!=12 || base[0xd0010/4])return EBUSY;
 for(unsigned i=0;i<12;++i)if(base[(0x10010+i*0x4000)/4]&(1u<<31) || base[(0x80010+i*0x4000)/4]&(1u<<31))return EBUSY;
 s->owner->setProperty("cm-start-stage","dart-prepare");
 // prepare may change hardware before failing. Its poisoned/reset state
 // must participate in stop, even when no NHI descriptor was allocated.
 s->dmaLive=true;
 int ret=s->dart.prepare();if(ret){s->owner->setProperty("cm-start-error",(uint32_t)ret,32);return ret;}
 s->owner->setProperty("cm-start-stage","irq-resolve");
 ret=pd_cio_nhi_irqs_resolve(s->irqs);if(ret)return -ret;
 ret=pd_rtkit_platform_irq_request(s->irqs[24],PDCIOCM::dartInterrupt,s,"acio1-dart");if(ret)return -ret;
 s->irqLive=true;pd_rtkit_platform_irq_enable(s->irqs[24]);
 s->owner->setProperty("cm-start-stage","nhi-create");
 pd_tb_nhi_config config={};config.ctx=s;config.device=s->device;config.nhi_base=nhi;config.pdf_base=pdf;config.powered=PDCIOCM::power;
 config.irq_request=pd_rtkit_platform_irq_request;config.irq_enable=pd_rtkit_platform_irq_enable;config.irq_free=pd_rtkit_platform_irq_free;
 for(unsigned i=0;i<12;++i){config.rx_irq[i]=s->irqs[i];config.tx_irq[i]=s->irqs[12+i];}
 ret=pd_tb_nhi_create(&config,&s->nhi);
 if(!ret){s->owner->setProperty("cm-start-stage","domain-start");ret=pd_tb_nhi_start(s->nhi,cable);}
 s->owner->setProperty("cm-start-error",(uint32_t)(ret<0?-ret:ret),32);
 if(!ret)s->owner->setProperty("cm-start-stage","complete");
 return ret<0?-ret:ret;
}
int pd_cio_cm_stop(PDCIOCM *s)
{
 if(!s)return EINVAL;
 if(s->nhi){int ret=pd_tb_nhi_stop(s->nhi);if(ret)return ret<0?-ret:ret;s->nhi=NULL;}
 if(s->irqLive){pd_rtkit_platform_irq_free(s->irqs[24]);s->irqLive=false;}
 if(s->dmaLive){int ret=s->dart.suspend();if(ret)return ret;s->dmaLive=false;}
 return __atomic_load_n(&s->failed,__ATOMIC_ACQUIRE)?EIO:0;
}
int pd_cio_cm_snapshot(PDCIOCM *s)
{
 if(!s || !s->nhi || !s->live())return ENODEV;
 pd_tb_nhi_status status={};int ret=pd_tb_nhi_snapshot(s->nhi,&status);
 s->owner->setProperty("link-query-error",(uint64_t)(ret<0?-ret:ret),32);
 s->owner->setProperty("host-router-vendor",(uint64_t)status.root_vendor,32);
 s->owner->setProperty("host-router-device",(uint64_t)status.root_device,32);
 s->owner->setProperty("host-router-max-port",(uint64_t)status.max_port,32);
 auto ports=OSArray::withCapacity(status.port_count);
 if(!ports)return ENOMEM;
 bool ok=true;
 for(unsigned i=0;i<status.port_count && ok;++i){
  const auto &p=status.ports[i];auto item=OSDictionary::withCapacity(6);
  if(!item){ok=false;break;}
  const char *keys[]={"port","state","error","remote","remote-vendor","remote-device"};
  const uint32_t values[]={p.port,p.state,p.error,p.remote,p.remote_vendor,p.remote_device};
  for(unsigned n=0;n<6 && ok;++n){auto v=OSNumber::withNumber((uint64_t)values[n],32);ok=v && item->setObject(keys[n],v);OSSafeReleaseNULL(v);}
  if(ok)ok=ports->setObject(item);item->release();
 }
 if(ok)ok=s->owner->setProperty("physical-port-state",ports);
 ports->release();return !ok?ENOMEM:ret<0?-ret:ret;
}
