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
    setProperty("PDCIOPCIEnumerationOnly",true);setProperty("PDCIOPCIDMAAdmitted",false);
    startAttempted=true;stopped=false;
    bool ok=IOPCIBridge::start(provider);
    if(!ok)blockConfiguration(); // Native owner must still drain partial children.
    return ok;
}
IOReturn PDCIOPCIHost::blockConfiguration()
{
    if(ml_at_interrupt_context())return kIOReturnNotPermitted;
    if(!accessLock)return kIOReturnNotReady;
    // Joins actual in-flight config accesses. Later calls cannot reach either
    // the borrowed power context or the controller mappings after revocation.
    IOLockLock(accessLock);view.admitted=false;IOLockUnlock(accessLock);return kIOReturnSuccess;
}
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
    if(p && !pd_pciec_config_write_filter(reg,width,value,false,&filtered)){
        if(width==4)OSWriteLittleInt32((volatile void *)p,0,filtered);
        else if(width==2)OSWriteLittleInt16((volatile void *)p,0,(UInt16)filtered);
        else *p=(UInt8)filtered;
        OSSynchronizeIO();
    }
    IOLockUnlock(accessLock);
}
bool PDCIOPCIHost::configure(IOService *provider)
{
    if(!addBridgeMemoryRange(0x00100000ULL,0x3ff00000ULL,true) ||
       !addBridgeMemoryRange(0x40000000ULL,0x40000000ULL,true) ||
       !addBridgeMemoryRange(0x580000000ULL,0x80000000ULL,true))return false;
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
    if(accessLock){IOLockFree(accessLock);accessLock=NULL;}
    OSSafeReleaseNULL(leaseOwner);IOPCIBridge::free();
}
