// SPDX-License-Identifier: GPL-2.0
/* Native PCIe-C host adapter, following PDApcieT8103Host / pcie-apple.c.
 * Copyright (C) 2021 Alyssa Rosenzweig <alyssa@rosenzweig.io>
 * Copyright (C) 2021 Google LLC
 * Copyright (C) 2021 Corellium LLC
 * Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 */
#include "pd_cio_pcie_host.h"
#include <IOKit/IODeviceTreeSupport.h>
#include <machine/machine_routines.h>
#include <libkern/OSByteOrder.h>
OSDefineMetaClassAndStructors(PDCIOPCIHost,IOPCIBridge)
bool PDCIOPCIHost::initForEnumeration(IOService *owner,void *ctx,bool (*proof)(void *))
{
    leaseOwner=NULL;powerContext=NULL;powerProof=NULL;accessLock=NULL;
#if PD_CIO_PCI_MSI
    msi=NULL;interruptAdmitted=0;
#endif
#if PD_CIO_I210_NETWORK
    bzero(grants,sizeof(grants));grantCount=bmeActive=0;networkAttempted=false;networkEndpoint=NULL;
    usbAttempted=false;usbEndpoint=NULL;
#endif
    view={0x10000000,0,127,false};startAttempted=false;stopped=true;
    if(!owner || !ctx || !proof || !IOPCIBridge::init())return false;
    accessLock=IOLockAlloc();if(!accessLock)return false;
    owner->retain();leaseOwner=owner;powerContext=ctx;powerProof=proof;return true;
}
bool PDCIOPCIHost::start(IOService *provider)
{
    uint32_t chip=0;
    if(!accessLock || !leaseOwner || !powerProof || !provider ||
       provider->getProperty("PDCIOPCIEnumerationOnly")!=kOSBooleanTrue ||
       !pd_adt_u32("/chosen","chip-id",0,&chip) || chip!=0x8103 ||
       !powerProof(powerContext))return false;
    // ACIO can remain powered while PCIe-C loses its own domain. Never
    // infer this separate gate from the CM's cable/PHY lifetime alone.
    if(!powerGate.mapPhys(0x23b700450ULL,4) || !physicalPower())return false;
    // CPU mappings only; no PCIe-C DART parameter/register access here.
    // PDMmio supplies nonposted Device-nGnRnE root/config accesses.
    if(!config.mapNode("/arm-io/apciec1",0) || config.phys!=0x530000000ULL || config.len<0x8000000 ||
       !port.mapNode("/arm-io/apciec1",2) || port.phys!=0x521000000ULL || port.len<0x8000)return false;
    UInt32 run=port.r32(0x804),link=port.r32(0x208);
    if(run==UINT32_MAX || link==UINT32_MAX || !(run&1) || !(link&1))return false;
    IOLockLock(accessLock);view.bytes=config.len;view.admitted=true;IOLockUnlock(accessLock);
    IOPCIAddressSpace root={};UInt32 vendor=configRead32(root,0),classCode=configRead32(root,8);
    if(vendor==UINT32_MAX || !(vendor&0xffff) || (vendor&0xffff)==0xffff ||
       ((classCode>>16)&0xffff)!=0x0604){blockConfiguration();return false;}
#if PD_CIO_PCI_MSI
    // The pinned T8103 sequence programs 32 per-port MSI vectors, base0.
    // Derive and verify the actual hardware configuration before exposing
    // any PCI child or resolving an endpoint's interrupt properties.
    UInt32 dtBase=0;
    if(!pd_adt_u32("/arm-io/apciec1","msi-vector-offset",0,&dtBase) || dtBase!=818){
        setProperty("msi-setup-stage","ADT-vector-base-refused");blockConfiguration();return false;
    }
    // Cold init intentionally cleared these registers. Restore the pinned
    // Linux sequence before constructing the native interrupt controller.
    pd_pciec_msi_setup_io setup={this,msiRead,msiWrite};
    pd_pciec_msi_setup_report report={};
    int msiError=pd_pciec_msi_setup(&setup,&report);
    setProperty("msi-setup-error",(UInt32)msiError,32);
    setProperty("msi-before-count",report.before_count,32);
    setProperty("msi-after-count",report.after_count,32);
    setProperty("msi-before-registers",report.before,report.before_count*sizeof(UInt32));
    setProperty("msi-after-registers",report.after,report.after_count*sizeof(UInt32));
    if(msiError){setProperty("msi-setup-stage","register-setup-refused");blockConfiguration();return false;}
    setProperty("msi-setup-stage","native-parent-setup");
    __atomic_store_n(&interruptAdmitted,1,__ATOMIC_RELEASE);
    msi=OSTypeAlloc(PDCIOPCIMSIController);
    if(!msi || !msi->initForPort(this,this,interruptPower,dtBase,32,0xfffff000)){
        blockConfiguration();return false;
    }
#endif
#if PD_CIO_PCI_MSI
    setProperty("msi-setup-stage","configured-parents-masked");
#endif
    setProperty("PDCIOPCIEnumerationOnly",true);setProperty("PDCIOPCIDMAAdmitted",false);
    startAttempted=true;stopped=false;
    bool ok=IOPCIBridge::start(provider);
    if(!ok)blockConfiguration(); // Native owner must still drain partial children.
    return ok;
}
IOReturn PDCIOPCIHost::blockConfiguration()
{
#if PD_CIO_PCI_MSI
    __atomic_store_n(&interruptAdmitted,0,__ATOMIC_RELEASE);
    if(msi)msi->revoke(); // Gate only, never a parent-IRQ drain ACK.
#endif
    if(ml_at_interrupt_context())return kIOReturnNotPermitted;
    if(!accessLock)return kIOReturnNotReady;
    // Joins actual in-flight config accesses. Later calls cannot reach either
    // the borrowed power context or the controller mappings after revocation.
    IOLockLock(accessLock);view.admitted=false;
#if PD_CIO_I210_NETWORK
    bmeActive=0;
#endif
    IOLockUnlock(accessLock);
#if PD_CIO_I210_NETWORK
    setProperty("PDCIOPCIDMAAdmitted",false); // Admission revoked, not DMA drained.
#endif
    return kIOReturnSuccess;
}
#if PD_CIO_PCI_MSI
bool PDCIOPCIHost::msiPhysicalAccess()
{
    if(!physicalPower() || !powerProof || !powerProof(powerContext))return false;
    UInt32 run=port.r32(0x804),link=port.r32(0x208);
    return run!=UINT32_MAX && link!=UINT32_MAX && (run&1) && (link&1);
}
int PDCIOPCIHost::msiRead(void *context,uint32_t off,uint32_t *value)
{
    auto s=(PDCIOPCIHost *)context;
    if(!value || (off!=0x124 && off!=0x128 && off!=0x168))return EINVAL;
    if(!s->msiPhysicalAccess())return ENODEV;
    __asm__ volatile("dsb sy; isb" ::: "memory");*value=s->port.r32(off);
    __asm__ volatile("dsb sy; isb" ::: "memory");return 0;
}
int PDCIOPCIHost::msiWrite(void *context,uint32_t off,uint32_t value)
{
    auto s=(PDCIOPCIHost *)context;
    if(off!=0x104 && off!=0x100 && off!=0x210 && off!=0x168 && off!=0x128 && off!=0x124)return EINVAL;
    if(!s->msiPhysicalAccess())return ENODEV;
    __asm__ volatile("dsb sy; isb" ::: "memory");s->port.w32(off,value);
    __asm__ volatile("dsb sy; isb" ::: "memory");return 0;
}
bool PDCIOPCIHost::interruptPower(void *context)
{
    auto s=(PDCIOPCIHost *)context;
    // IRQ-safe predicates only; the retained owner/context cannot be freed
    // while any registered AIC parent can call us. No PCI config or locks.
    if(!__atomic_load_n(&s->interruptAdmitted,__ATOMIC_ACQUIRE) ||
       !s->physicalPower() || !s->powerProof || !s->powerProof(s->powerContext))return false;
    // Cable loss can precede the CM revocation callback. Never dispatch an
    // endpoint's ISR through a link which the actual root already lost.
    UInt32 run=s->port.r32(0x804),link=s->port.r32(0x208);
    return run!=UINT32_MAX && link!=UINT32_MAX && (run&1) && (link&1);
}
IOReturn PDCIOPCIHost::callPlatformFunction(const OSSymbol *name,bool wait,
 void *p1,void *p2,void *p3,void *p4)
{
#if PD_CIO_I210_NETWORK
    if(name && name->isEqualTo("PDCIOPCIEndpointAccess")){
        if(!p1 || (p1!=__atomic_load_n(&networkEndpoint,__ATOMIC_ACQUIRE) &&
           p1!=__atomic_load_n(&usbEndpoint,__ATOMIC_ACQUIRE)) || p2 || p3 || p4)return kIOReturnBadArgument;
        return interruptPower(this)?kIOReturnSuccess:kIOReturnNotReady;
    }
    if(name && name->isEqualTo("PDCIOPCIEnableI210DMA")){
        if(p2 || p3 || p4 || ml_at_interrupt_context())return kIOReturnBadArgument;
        return admitI210(OSDynamicCast(IOPCIDevice,(IOService *)p1));
    }
    if(name && name->isEqualTo("PDCIOPCIEnableDockUSBDMA")){
        if(p2 || p3 || p4 || ml_at_interrupt_context())return kIOReturnBadArgument;
        return admitDockUSB(OSDynamicCast(IOPCIDevice,(IOService *)p1));
    }
#endif
    if(name==gIOPlatformGetMessagedInterruptControllerKey){
        if(p2)*(IOPCIMessagedInterruptController **)p2=NULL;
        if(!p2 || !msi || !msi->ready())return kIOReturnNotReady;
        *(IOPCIMessagedInterruptController **)p2=msi;return kIOReturnSuccess;
    }
    if(name==gIOPlatformGetMessagedInterruptAddressKey){
        if(p2 || !msi || (uintptr_t)p3>UINT32_MAX)return kIOReturnUnsupported;
        return msi->messageForDevice((IOService *)p1,(UInt32)(uintptr_t)p3,(UInt32 *)p4);
    }
    if(name && name->isEqualTo("PDCIOPCIMSIRouteReady")){
        if(p2 || p3 || p4 || !msi || ml_at_interrupt_context())return kIOReturnBadArgument;
        return msi->routeReady(OSDynamicCast(IOPCIDevice,(IOService *)p1))?
            kIOReturnSuccess:kIOReturnNotReady;
    }
    return IOPCIBridge::callPlatformFunction(name,wait,p1,p2,p3,p4);
}
#endif
bool PDCIOPCIHost::physicalPower()
{
    if(!powerGate.va)return false;
    __asm__ volatile("dsb sy; isb" ::: "memory");
    UInt32 state=powerGate.r32(0);
    __asm__ volatile("dsb sy; isb" ::: "memory");
    return state!=UINT32_MAX && (state&0xf0)==0xf0 && !(state&((1u<<31)|(1u<<10)));
}
volatile uint8_t *PDCIOPCIHost::address(IOPCIAddressSpace space,UInt8 offset,unsigned width)
{
    uint64_t off;
    unsigned reg=((unsigned)space.es.registerNumExtended<<8)|offset;
    if(pd_pciec_config_offset(&view,space.s.busNum,space.s.deviceNum,
                             space.s.functionNum,reg,width,&off))return NULL;
    if(!powerProof || !powerProof(powerContext) || !physicalPower())return NULL;
    UInt32 run=port.r32(0x804),link=port.r32(0x208);
    if(run==UINT32_MAX || link==UINT32_MAX || !(run&1) || !(link&1))return NULL;
    return config.va+off;
}
UInt32 PDCIOPCIHost::readConfig(IOPCIAddressSpace space,UInt8 offset,unsigned width)
{
    if(!accessLock || ml_at_interrupt_context())return UINT32_MAX;
    IOLockLock(accessLock);auto p=address(space,offset,width);UInt32 value=UINT32_MAX;
    if(p){
        if(width==4)value=OSReadLittleInt32((volatile void *)p,0);
        else if(width==2)value=OSReadLittleInt16((volatile void *)p,0);
        else value=*p;
        OSSynchronizeIO();
    }
    IOLockUnlock(accessLock);return value;
}
void PDCIOPCIHost::writeConfig(IOPCIAddressSpace space,UInt8 offset,unsigned width,UInt32 value)
{
    if(!accessLock || ml_at_interrupt_context())return;
    IOLockLock(accessLock);auto p=address(space,offset,width);UInt32 filtered;
    unsigned reg=((unsigned)space.es.registerNumExtended<<8)|offset;
    bool bme=false;
#if PD_CIO_I210_NETWORK
    if(p)bme=busMasterAllowed(space);
#endif
    if(p && !pd_pciec_config_write_filter(reg,width,value,bme,&filtered)){
        if(width==4)OSWriteLittleInt32((volatile void *)p,0,filtered);
        else if(width==2)OSWriteLittleInt16((volatile void *)p,0,(UInt16)filtered);
        else *p=(UInt8)filtered;
        OSSynchronizeIO();
    }
    IOLockUnlock(accessLock);
}
bool PDCIOPCIHost::configure(IOService *provider)
{
    // Preserve Linux apciec1 range types. With only a MEM list, native TB
    // prefetch maximization falls back to it and starves ordinary BARs.
    if(!addBridgeMemoryRange(0x00100000ULL,0x3ff00000ULL,true) ||
       !addBridgePrefetchableMemoryRange(0x40000000ULL,0x40000000ULL) ||
       !addBridgePrefetchableMemoryRange(0x580000000ULL,0x80000000ULL))return false;
    return IOPCIBridge::configure(provider);
}
IODeviceMemory *PDCIOPCIHost::ioDeviceMemory(){return NULL;}
UInt8 PDCIOPCIHost::firstBusNum(){return 0;}
UInt8 PDCIOPCIHost::lastBusNum(){return 127;}
IOPCIAddressSpace PDCIOPCIHost::getBridgeSpace(){IOPCIAddressSpace s={};return s;}
UInt32 PDCIOPCIHost::configRead32(IOPCIAddressSpace s,UInt8 o){return readConfig(s,o&~3u,4);}
UInt16 PDCIOPCIHost::configRead16(IOPCIAddressSpace s,UInt8 o){return (UInt16)readConfig(s,o&~1u,2);}
UInt8 PDCIOPCIHost::configRead8(IOPCIAddressSpace s,UInt8 o){return (UInt8)readConfig(s,o,1);}
void PDCIOPCIHost::configWrite32(IOPCIAddressSpace s,UInt8 o,UInt32 v){writeConfig(s,o&~3u,4,v);}
void PDCIOPCIHost::configWrite16(IOPCIAddressSpace s,UInt8 o,UInt16 v){writeConfig(s,o&~1u,2,v);}
void PDCIOPCIHost::configWrite8(IOPCIAddressSpace s,UInt8 o,UInt8 v){writeConfig(s,o,1,v);}
void PDCIOPCIHost::stop(IOService *provider)
{
    if(blockConfiguration()!=kIOReturnSuccess){retain();return;}
    IOPCIBridge::stop(provider);stopped=true;
}
void PDCIOPCIHost::free()
{
    if(view.admitted || (startAttempted && !stopped)){
        IOLog("PDCIOPCIHost: retaining controller mappings after incomplete stop\n");return;
    }
    config.unmap();port.unmap();powerGate.unmap();
#if PD_CIO_PCI_MSI
    OSSafeReleaseNULL(msi);
#endif
    if(accessLock){IOLockFree(accessLock);accessLock=NULL;}
    OSSafeReleaseNULL(leaseOwner);IOPCIBridge::free();
}
