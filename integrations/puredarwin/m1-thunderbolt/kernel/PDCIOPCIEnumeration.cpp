// SPDX-License-Identifier: MIT
#include "pd_cio_pci_enumeration.h"
#include <IOKit/IOCatalogue.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <libkern/c++/OSOrderedSet.h>
#include <sys/errno.h>
OSDefineMetaClassAndStructors(PDCIOPCIEnumeration,OSObject)
bool PDCIOPCIEnumeration::initWithOwner(IOService *service,void *ctx,bool (*proof)(void *))
{
 owner=nub=NULL;context=NULL;powerProof=NULL;lock=NULL;controller=NULL;host=NULL;
 posted=committed=attempted=revoked=0;preparing=false;
 if(!service || !ctx || !proof || !OSObject::init())return false;
 lock=IOLockAlloc();if(!lock)return false;
 service->retain();owner=service;context=ctx;powerProof=proof;return true;
}
bool PDCIOPCIEnumeration::power(void *p)
{
 auto s=(PDCIOPCIEnumeration *)p;
 return !__atomic_load_n(&s->revoked,__ATOMIC_ACQUIRE) && s->powerProof(s->context);
}
bool PDCIOPCIEnumeration::tunnel(void *p)
{
 auto s=(PDCIOPCIEnumeration *)p;
 return power(p) && __atomic_load_n(&s->posted,__ATOMIC_ACQUIRE) &&
        __atomic_load_n(&s->committed,__ATOMIC_ACQUIRE);
}
int PDCIOPCIEnumeration::catalogueSafe()
{
 if(!gIOCatalogue)return ENODEV;
 auto matching=OSDictionary::withCapacity(1);
 auto provider=OSString::withCString("IOPCIDevice");
 bool ok=matching && provider && matching->setObject("IOProviderClass",provider);
 OSSafeReleaseNULL(provider);if(!ok){OSSafeReleaseNULL(matching);return ENOMEM;}
 SInt32 generation=0;auto drivers=gIOCatalogue->findDrivers(matching,&generation);
 matching->release();
#if !defined(IOKIT_ENABLE_SHARED_PTR)
 struct ReleaseList { OSOrderedSet *value;~ReleaseList(){if(value)value->release();} } releaseList={drivers};
#endif
 if(!drivers || !drivers->getCount() || drivers->getCount()>64)return E2BIG;
 unsigned bridges=0,wifi=0;int ret=0;
 for(unsigned i=0;i<drivers->getCount();++i){
  auto dict=OSDynamicCast(OSDictionary,drivers->getObject(i));
  auto cls=dict?OSDynamicCast(OSString,dict->getObject("IOClass")):NULL;
  auto bundle=dict?OSDynamicCast(OSString,dict->getObject("CFBundleIdentifier")):NULL;
  if(cls && cls->isEqualTo("IOPCI2PCIBridge") && bundle && bundle->isEqualTo("com.apple.iokit.IOPCIFamily")){++bridges;continue;}
  auto id=dict?OSDynamicCast(OSString,dict->getObject("IOPCIPrimaryMatch")):NULL;
  if(cls && cls->isEqualTo("PDBwfm") && bundle && bundle->isEqualTo("org.puredarwin.driver.PDBwfm") && id && id->isEqualTo("0x442514e4") &&
     !dict->getObject("IOPCIMatch") && !dict->getObject("IOPCIClassMatch")){++wifi;continue;}
  ret=EOPNOTSUPP;break; // No generic endpoint personality, including PCI xHCI.
 }
 if(!ret && (!bridges || !wifi))ret=ENODEV;
 owner->setProperty("pcie-catalogue-error",(uint32_t)ret,32);
 owner->setProperty("pcie-catalogue-count",drivers->getCount(),32);
 owner->setProperty("pcie-catalogue-generation",(uint32_t)generation,32);
 return ret;
}
int PDCIOPCIEnumeration::prepare()
{
 if(!power(this))return ENODEV;
 int ret=catalogueSafe();if(ret)return ret;
 IOLockLock(lock);
 if(preparing || controller || host || __atomic_load_n(&attempted,__ATOMIC_ACQUIRE)){
  IOLockUnlock(lock);return EBUSY;
 }
 preparing=true;IOLockUnlock(lock);
 auto ctl=OSTypeAlloc(PDCIOPCIController);
 bool ok=ctl && ctl->initWithOwner(owner,this,power,tunnel);
 IOLockLock(lock);preparing=false;if(ok)controller=ctl;IOLockUnlock(lock);
 if(!ok){OSSafeReleaseNULL(ctl);return ENODEV;}
 owner->setProperty("pcie-enumeration-stage","reserved-no-MMIO");return 0;
}
int PDCIOPCIEnumeration::checkProfile()
{return catalogueSafe();}
int PDCIOPCIEnumeration::activated()
{
 if(!power(this) || !controller || __atomic_load_n(&attempted,__ATOMIC_ACQUIRE))return ENODEV;
 // Real CM post-activation only. No CPU PCIe access or queued worker here.
 __atomic_store_n(&posted,1,__ATOMIC_RELEASE);
 owner->setProperty("pcie-enumeration-stage","post-callback-seen");return 0;
}
int PDCIOPCIEnumeration::fail(int error)
{
 if(host)host->blockConfiguration();
 owner->setProperty("pcie-enumeration-error",(uint32_t)error,32);
 owner->setProperty("pcie-enumeration-quarantined",true);
 owner->setProperty("pcie-enumeration-stage","quarantined-until-reboot");return error;
}
int PDCIOPCIEnumeration::publishAfterApproval()
{
 // The caller has returned from pd_tb_nhi_authorize_ts3(): both real
 // tb_domain_approve_switch and the post hook returned, domain lock released.
 if(!power(this) || !controller || !__atomic_load_n(&posted,__ATOMIC_ACQUIRE))return ENODEV;
 int ret=catalogueSafe();if(ret)return ret;
 uint32_t expected=0;
 if(!__atomic_compare_exchange_n(&attempted,&expected,1,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return EBUSY;
 __atomic_store_n(&committed,1,__ATOMIC_RELEASE);
 owner->setProperty("pcie-enumeration-stage","cold-init");
 ret=controller->coldInit();if(ret)return fail(ret);
 owner->setProperty("pcie-enumeration-stage","link-start");
 ret=controller->linkStart();if(ret)return fail(ret);
 owner->setProperty("pcie-enumeration-stage","block-all-endpoint-DMA");
 ret=controller->blockEndpointDMA();if(ret)return fail(ret);
 ret=catalogueSafe();if(ret)return fail(ret);
 if(!tunnel(this))return fail(ENODEV);
 auto entry=IORegistryEntry::fromPath("/arm-io/apciec1",gIODTPlane);
 if(!entry)return fail(ENODEV);
 auto service=OSDynamicCast(IOService,entry);
 if(service){service->retain();nub=service;}
 else{
  auto made=OSTypeAlloc(IOPlatformDevice);
  bool ok=made && made->init(entry,gIODTPlane);
  if(ok)ok=made->attach(IOService::getPlatform());
  if(!ok){OSSafeReleaseNULL(made);entry->release();return fail(ENODEV);}
  nub=made;
 }
 entry->release();
 if(!nub->inPlane(gIOServicePlane) && !nub->attach(IOService::getPlatform()))return fail(ENODEV);
 if(!nub->setProperty("PDCIOPCIEnumerationOnly",true))return fail(ENOMEM);
 auto bridge=OSTypeAlloc(PDCIOPCIHost);
 if(!bridge || !bridge->initForEnumeration(owner,this,tunnel)){
  OSSafeReleaseNULL(bridge);return fail(ENOMEM);
 }
 // Publish the initialized pointer before attach/start, so concurrent CM
 // teardown can block configuration. Retain even partial start until reboot.
 IOLockLock(lock);host=bridge;IOLockUnlock(lock);
 owner->setProperty("pcie-enumeration-stage","bridge-start");
 if(!tunnel(this) || !host->attach(nub) || !host->start(nub))return fail(ENODEV);
 if(!tunnel(this))return fail(ENODEV);
 owner->setProperty("pcie-host-published",true);
 owner->setProperty("pcie-endpoint-DMA-admitted",false);
 owner->setProperty("pcie-enumeration-error",(uint32_t)0,32);
 owner->setProperty("pcie-enumeration-stage","host-published-inventory-required");return 0;
}
int PDCIOPCIEnumeration::deactivate()
{
 __atomic_store_n(&revoked,1,__ATOMIC_RELEASE);
 __atomic_store_n(&committed,0,__ATOMIC_RELEASE);
 __atomic_store_n(&posted,0,__ATOMIC_RELEASE);
 IOLockLock(lock);auto bridge=host;auto ctl=controller;IOLockUnlock(lock);
 if(bridge)bridge->blockConfiguration();
 int ret=ctl?ctl->revoke():0;
 // No synchronous child termination while the CM domain lock is held.
 // A programmed first-enumeration profile pins all ownership until reboot.
 if(bridge && !ret)ret=EBUSY;
 if(ret)return fail(ret);
 owner->setProperty("pcie-enumeration-stage","revoked-before-programming");return 0;
}
bool PDCIOPCIEnumeration::blocksStop() const
{return __atomic_load_n(&attempted,__ATOMIC_ACQUIRE)!=0;}
void PDCIOPCIEnumeration::free()
{
 if(controller || host || nub)return; // pinned experimental context, no drain claim
 if(lock)IOLockFree(lock);OSSafeReleaseNULL(owner);OSObject::free();
}
