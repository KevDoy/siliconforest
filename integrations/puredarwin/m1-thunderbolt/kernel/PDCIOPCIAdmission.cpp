// SPDX-License-Identifier: GPL-2.0
// Actual I210/ancestor admission, separate from the inventory-only profile.
#include "pd_cio_pcie_host.h"
#include <IOKit/IOMapper.h>
#include <IOKit/IORegistryEntry.h>
#include <machine/machine_routines.h>
#include <libkern/OSByteOrder.h>
#include <sys/errno.h>

int PDCIOPCIHost::bindI210Access(IOPCIDevice *device)
{
    if(!device || ml_at_interrupt_context() || !interruptPower(this) ||
       device->configRead32(0)!=0x15338086 || (device->configRead32(8)>>8)!=0x020000)return ENODEV;
    IORegistryEntry *p=device;unsigned depth=0;
    while(p && p!=this && depth++<64)p=p->getParentEntry(gIOServicePlane);
    if(p!=this || networkEndpoint)return EBUSY;
    device->retain();__atomic_store_n(&networkEndpoint,device,__ATOMIC_RELEASE);
    return 0;
}

bool PDCIOPCIHost::busMasterAllowed(IOPCIAddressSpace space)
{
    // accessLock is held. Never call a mapper/IRQ query here: they read config.
    if(!bmeActive)return false;
    for(UInt32 n=0;n<grantCount;++n){
        auto &g=grants[n];
        if(g.bus!=space.s.busNum || g.slot!=space.s.deviceNum ||
           g.function!=space.s.functionNum)continue;
        auto id=address(space,0,4),code=address(space,8,4);
        return id && code && OSReadLittleInt32((volatile void *)id,0)==g.id &&
            (OSReadLittleInt32((volatile void *)code,0)>>8)==g.code;
    }
    return false;
}

IOReturn PDCIOPCIHost::admitI210(IOPCIDevice *device)
{
    if(!device || ml_at_interrupt_context() || !msi || !interruptPower(this) ||
       device->configRead32(0)!=0x15338086 ||
       (device->configRead32(8)>>8)!=0x020000)return kIOReturnNotReady;
    auto mapper=IOMapper::copyMapperForDevice(device);
    bool ready=mapper && mapper->metaCast("PDCIOPCIDartMapper") &&
        mapper->getProvider()==device &&
        mapper->callPlatformFunction("PDCIOPCIDMAReady",false,device,NULL,NULL,NULL)==kIOReturnSuccess;
    OSSafeReleaseNULL(mapper);
    // The actual interrupt workloop may not have armed the newly enabled
    // parent yet. Caller may retry this factual query during bounded attach.
    if(!ready || !msi->routeReady(device))return kIOReturnNotReady;
    Grant chain[8]={};UInt32 count=0;unsigned depth=0;
    IORegistryEntry *p=device;
    while(p && p!=this && depth++<64){
        auto pci=OSDynamicCast(IOPCIDevice,p);
        if(pci){
            if(count==8)return kIOReturnUnsupported;
            UInt32 id=pci->configRead32(0),code=pci->configRead32(8)>>8;
            if(id==UINT32_MAX || (!count ? pci!=device || code!=0x020000 : code!=0x060400))
                return kIOReturnUnsupported;
            chain[count++]={pci,id,code,pci->getBusNumber(),pci->getDeviceNumber(),pci->getFunctionNumber()};
        }
        p=p->getParentEntry(gIOServicePlane);
    }
    if(p!=this || count<2)return kIOReturnUnsupported;
    // Fresh full inventory: never enable another endpoint, including USB.
    auto it=IORegistryIterator::iterateOver(this,gIOServicePlane,kIORegistryIterateRecursively);
    if(!it)return kIOReturnNoMemory;
    unsigned seen=0,xhci=0,i210=0;bool valid=true;
    while(auto object=it->getNextObject()){
        if(++seen>256){valid=false;break;}
        auto pci=OSDynamicCast(IOPCIDevice,object);if(!pci)continue;
        UInt32 id=pci->configRead32(0),code=pci->configRead32(8)>>8;
        if(id==UINT32_MAX || (pci->configRead16(4)&4)){valid=false;break;}
        if(code==0x0c0330)++xhci;
        else if(id==0x15338086 && code==0x020000 && pci==device)++i210;
        else if(code!=0x060400){valid=false;break;}
    }
    if(!it->isValid())valid=false;it->release();
    if(!valid || xhci!=4 || i210!=1)return kIOReturnNotPermitted;
    // No config lock is held across either real ownership query.
    if(!interruptPower(this) || !msi->routeReady(device))return kIOReturnNotReady;
    IOLockLock(accessLock);
    if(networkAttempted || !view.admitted){IOLockUnlock(accessLock);return kIOReturnNotPermitted;}
    networkAttempted=true;
    for(UInt32 n=0;n<count;++n){grants[n]=chain[n];grants[n].device->retain();}
    grantCount=count;bmeActive=1;
    retain(); // Ambiguous writes/partial DMA remain retained until reboot.
    IOLockUnlock(accessLock);
    // Forwarding bridges first (root to leaf), endpoint last. Every write
    // still passes fresh native physical power/link and identity checks.
    bool ok=true;
    for(UInt32 n=count;n>0;--n){
        auto pci=chain[n-1].device;
        if(!interruptPower(this)){ok=false;break;}
        pci->setBusMasterEnable(true);
        UInt16 command=pci->configRead16(4);
        if(command==UINT16_MAX || !(command&4)){ok=false;break;}
    }
    if(!ok){
        for(UInt32 n=0;n<count;++n){
            auto pci=chain[n].device;UInt16 command=pci->configRead16(4);
            if(command!=UINT16_MAX)pci->setBusMasterEnable(false);
        }
        leaseOwner->setProperty("pcie-i210-BME-error",(UInt32)EIO,32);
        blockConfiguration(); // Gate/pin only; never claim a DMA drain.
        return kIOReturnIOError;
    }
    setProperty("PDCIOPCIDMAAdmitted",true);
    setProperty("PDCIOI210Only",true);
    device->setProperty("PDCIOI210BusMasterAdmitted",true);
    leaseOwner->setProperty("pcie-i210-BME-error",(UInt32)0,32);
    leaseOwner->setProperty("pcie-i210-BME-ancestor-count",count-1,32);
    leaseOwner->setProperty("pcie-endpoint-DMA-admitted",true);
    return kIOReturnSuccess;
}
