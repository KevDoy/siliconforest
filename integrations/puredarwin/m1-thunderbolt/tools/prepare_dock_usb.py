#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prepare only the dedicated native PCI USB staging tree; fail on source drift."""
from pathlib import Path
import plistlib
import sys

root = Path(sys.argv[1]) / 'src/Kernel/Extensions/RavynXHCIPort'
composition=plistlib.loads((root/'Info.plist').read_bytes())
preserved={name:value for name,value in composition['IOKitPersonalities'].items()
           if value.get('IOClass')=='PDAppleUSB' or value.get('IONameMatch')=='pd,apple-dwc3-host'}
assert sum(v.get('IOClass')=='PDAppleUSB' and v.get('IOProviderClass')=='PDAppleTypeC' for v in preserved.values())==1, 'shared PHY broker missing'
assert sum(v.get('IONameMatch')=='pd,apple-dwc3-host' for v in preserved.values())==1, 'prepared direct host missing'
assert (root/'PDAppleUSB.cpp').is_file() and 'PDAppleUSB.cpp' in (root/'CMakeLists.txt').read_text(), 'broker code missing'
def replace(text, old, new, count=1):
    if text.count(old) != count:
        raise SystemExit(f'dock USB source shape changed ({text.count(old)} != {count}): {old[:80]}')
    return text.replace(old, new)

p = root / 'RavynXHCIPort.h'
s = p.read_text()
s = replace(s, '    PDUSBDart *fAppleDart;', '''    PDUSBDart *fAppleDart;
    IOService *fDockHost;
    const OSSymbol *fDockAccessKey;
    OSArray *fDockBuffers;
    IOLock *fDockBufferLock;
    mutable UInt32 fDockMMIOFailed;
    bool fDockRetiring;
    void dockFail() const;
    bool dockAccess() const;
    UInt32 dockRead32(volatile UInt8 *,UInt32) const;
    void dockWrite32(volatile UInt8 *,UInt32,UInt32) const;
    bool retireDockBuffers();''')
for base, prefix in [('fCapRegs', 'cap'), ('fOpRegs', 'op'), ('fRTRegs', 'rt')]:
    s = replace(s, f'{{ return *(volatile UInt32 *)({base} + off); }}',
                f'{{ return dockRead32({base},off); }}')
for base in ['fOpRegs', 'fRTRegs']:
    s = replace(s, f'{{ *(volatile UInt32 *)({base} + off) = v; }}',
                f'{{ dockWrite32({base},off,v); }}')
s = replace(s, '            *(volatile UInt32 *)(fDBRegs + slot * 4) = target;',
            '            dockWrite32(fDBRegs,slot * 4,target);')
p.write_text(s)

p = root / 'RavynXHCIPort.cpp'
s = p.read_text()
s = replace(s, '#include "pd_usb_dart.h"', '#include "pd_usb_dart.h"\n#include "pd_pci_usb_dma.h"\n#include <IOKit/IORegistryEntry.h>')
s = replace(s, '    PDUSBDMABuffer *apple = OSDynamicCast(PDUSBDMABuffer, mem);', '''    if (auto dock = OSDynamicCast(PDPCIUSBDMABuffer,mem)) return dock->dmaAddress();
    PDUSBDMABuffer *apple = OSDynamicCast(PDUSBDMABuffer, mem);''')
s = replace(s, '    if (!fAppleDart) return IOBufferMemoryDescriptor::inTaskWithPhysicalMask(task, options, bytes, mask);', '''    if (fPCI) {
        if (!fDockBufferLock || !fDockBuffers || !dockAccess()) return NULL;
        IOLockLock(fDockBufferLock);
        PDPCIUSBDMABuffer *memory = NULL;
        // All ring/payload releases drop only their client reference while
        // this ledger retains every mapping until whole-controller halt/reset.
        // No live buffer recycling or retain-count inference for native DMA.
        if (!fDockRetiring && fDockBuffers->getCount() < 4096) {
            UInt64 alignment = mask & (~mask + 1);
            if (alignment < 0x4000) alignment = 0x4000;
            memory = PDPCIUSBDMABuffer::allocate(fPCI,bytes,alignment);
            if (memory && !fDockBuffers->setObject(memory)) {
                // Prepared mapping: keep its native ownership cycle pinned.
                memory = NULL;
                dockFail();
            }
        }
        IOLockUnlock(fDockBufferLock);
        return memory;
    }
    if (!fAppleDart) return NULL; // No physical-address DMA fallback in this profile.''')
anchor='bool RavynXHCIPort::startApple(IOService *provider)'
s = replace(s, anchor, '''void RavynXHCIPort::dockFail() const
{
    auto self=const_cast<RavynXHCIPort *>(this);
    __atomic_store_n(&fDockMMIOFailed,1,__ATOMIC_RELEASE);
    pd_usb_lifetime_close(&self->fLifetime);
    self->fHotplugRunning=false;self->fBulkStop=true;
}
bool RavynXHCIPort::dockAccess() const
{
    if (!fPCI) return true;
    if (__atomic_load_n(&fDockMMIOFailed,__ATOMIC_ACQUIRE)) return false;
    bool ok = fDockHost && fDockAccessKey &&
        fDockHost->callPlatformFunction(fDockAccessKey,false,fPCI,NULL,NULL,NULL) == kIOReturnSuccess;
    if (!ok) dockFail();
    return ok;
}
UInt32 RavynXHCIPort::dockRead32(volatile UInt8 *base,UInt32 off) const
{
    if (!dockAccess()) return UINT32_MAX;
    if (fPCI && (!base || !fCapRegs || (uintptr_t)base < (uintptr_t)fCapRegs ||
        (UInt64)((uintptr_t)base-(uintptr_t)fCapRegs)+off+4 > fBARLength)) {
        dockFail();return UINT32_MAX;
    }
    __sync_synchronize();UInt32 value=*(volatile UInt32 *)(base+off);__sync_synchronize();
    if (fPCI && value==UINT32_MAX) dockFail();
    return value;
}
void RavynXHCIPort::dockWrite32(volatile UInt8 *base,UInt32 off,UInt32 value) const
{
    if (!dockAccess()) return;
    if (fPCI && (!base || !fCapRegs || (uintptr_t)base < (uintptr_t)fCapRegs ||
        (UInt64)((uintptr_t)base-(uintptr_t)fCapRegs)+off+4 > fBARLength)) {
        dockFail();return;
    }
    __sync_synchronize();*(volatile UInt32 *)(base+off)=value;__sync_synchronize();
}
bool RavynXHCIPort::retireDockBuffers()
{
    if (!fPCI) return true;
    if (!dockAccess() || !fDockBufferLock || !fDockBuffers) return false;
    // Caller has closed admission, joined operations/workers, and observed
    // real HCH plus completed reset. No failed MMIO can qualify that proof.
    IOLockLock(fDockBufferLock);fDockRetiring=true;
    for (unsigned n=0;n<fDockBuffers->getCount();++n) {
        auto memory=OSDynamicCast(PDPCIUSBDMABuffer,fDockBuffers->getObject(n));
        if (!memory || memory->retire()!=kIOReturnSuccess) {
            IOLockUnlock(fDockBufferLock);return false;
        }
    }
    fDockBuffers->flushCollection();IOLockUnlock(fDockBufferLock);return true;
}

''' + anchor)
s = replace(s, '    if (!super::start(provider)) return false;', '''    if (fPCI) {
    if (fPCI->configRead32(0)!=0x11001b73 ||
        (fPCI->configRead32(8)>>8)!=0x0c0330 || (fPCI->configRead16(4)&4)) return false;
    fDockMMIOFailed=0;fDockRetiring=false;
    fDockAccessKey=OSSymbol::withCString("PDCIOPCIEndpointAccess");
    unsigned depth=0;
    for (IORegistryEntry *p=fPCI; p && depth++<64; p=p->getParentEntry(gIOServicePlane)) {
        if (p->metaCast("PDCIOPCIHost")) {fDockHost=OSDynamicCast(IOService,p);break;}
    }
    if (!fDockHost || !fDockAccessKey || !dockAccess()) {
        OSSafeReleaseNULL(fDockAccessKey);fDockHost=NULL;return false;
    }
    fDockHost->retain();retain(); // IRQ/partial DMA startup remains pinned until reboot.
    fDockBuffers=OSArray::withCapacity(64);fDockBufferLock=IOLockAlloc();
    if (!fDockBuffers || !fDockBufferLock) return false;
    }
    if (!super::start(provider)) return false;''')
s = replace(s, '        fPCI->setBusMasterEnable(true);', '        // Native host admits DMA only after registered and armed MSI below.')
s = replace(s, '''        fBARMap = mapUsableBAR(fPCI, kIOPCIConfigBaseAddress0);
        if (!fBARMap) mapBARFromAssignedAddresses(fPCI, &fBARDesc, &fBARMap);
        if (!fBARMap) mapBARFromConfig(fPCI, &fBARDesc, &fBARMap);''', '''        auto resource=fPCI->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
        if (!resource || resource->getLength()<0x1000 || !dockAccess()) return false;
        fBARMap=fPCI->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0,kIOMapPostedWrite);
        // Native resources only: no BAR sizing, guessed address or fallback.''')
s = replace(s, '    // CONFIG.MaxSlotsEn', '''    if (fPCI) {
    if (!fInterruptsEnabled || !(irqType & kIOInterruptTypePCIMessaged)) return false;
    IOReturn admitted=kIOReturnNotReady;
    for (unsigned attempt=0;attempt<100;++attempt) {
        admitted=fDockHost->callPlatformFunction("PDCIOPCIEnableDockUSBDMA",false,fPCI,NULL,NULL,NULL);
        if (admitted!=kIOReturnNotReady) break;
        IOSleep(10);
    }
    if (admitted!=kIOReturnSuccess || !dockAccess() || !(fPCI->configRead16(4)&4)) return false;
    }

    // CONFIG.MaxSlotsEn''')
s = replace(s, '        if (wait == 2000) { IOLockUnlock(fRemovalLock); return; }',
            '        if (wait == 2000 || !dockAccess()) { IOLockUnlock(fRemovalLock); return; }',2)
s = replace(s, '    fDmaLive = false;', '''    if (!retireDockBuffers()) { IOLockUnlock(fRemovalLock); return; }
    fDmaLive = false;''')
s = replace(s, '        if (!(opRead32(XHCI_USBCMD) & XHCI_USBCMD_HCRST) &&',
            '        if (dockAccess() && !(opRead32(XHCI_USBCMD) & XHCI_USBCMD_HCRST) &&')
s = replace(s, '    if (leaked)\n        xferMem->retain();', '''    if (auto dock=OSDynamicCast(PDPCIUSBDMABuffer,xferMem)) {
        if (leaked || !ok || !dockAccess()) dock->markUnsafeForReuse();
    } else if (leaked) xferMem->retain();''')
s = replace(s, '        if (mem->getRetainCount() <= 1) return;', '''        auto dock=OSDynamicCast(PDPCIUSBDMABuffer,mem);
        if (dock ? !dock->unsafeForReuse() : mem->getRetainCount() <= 1) return;''')
# These upstream extended-capability functions bypass the inline accessors.
# Guard their reads and BIOS handoff writes as well as ordinary registers.
s = replace(s, 'UInt32 cap = *(volatile UInt32 *)(fCapRegs + off);',
            'UInt32 cap = capRead32(off);')
s = replace(s, '''            volatile UInt32 *legSup = (volatile UInt32 *)(fCapRegs + off);
            volatile UInt32 *legCtl = (volatile UInt32 *)(fCapRegs + off + XHCI_LEGACY_CONTROL_STATUS);
            UInt32 before = *legSup;''', '''            UInt32 before = capRead32(off);''')
s = replace(s, 'off, before, *legCtl);', 'off, before, capRead32(off + XHCI_LEGACY_CONTROL_STATUS));')
s = replace(s, '            *legSup = before | XHCI_LEGACY_OS_OWNED;',
            '            dockWrite32(fCapRegs,off,before | XHCI_LEGACY_OS_OWNED);')
s = replace(s, '                UInt32 now = *legSup;',
            '                if (!dockAccess()) return;\n                UInt32 now = capRead32(off);')
s = replace(s, '            *legCtl = 0;',
            '            dockWrite32(fCapRegs,off + XHCI_LEGACY_CONTROL_STATUS,0);')
s = replace(s, '"Legacy Support cap after=%08x ctl=%08x", *legSup, *legCtl);',
            '"Legacy Support cap after=%08x ctl=%08x", capRead32(off), capRead32(off + XHCI_LEGACY_CONTROL_STATUS));')
s = replace(s, 'UInt32 dw0 = *(volatile UInt32 *)(fCapRegs + off);', 'UInt32 dw0 = capRead32(off);')
s = replace(s, 'UInt32 dw2 = *(volatile UInt32 *)(fCapRegs + off + 8);', 'UInt32 dw2 = capRead32(off + 8);')
# Client pages never bypass the owned mapper bounces in this profile.
start=s.index('bool RavynXHCIPort::mapClientSegs(');body=s.index('{',start)
s=s[:body+1]+'\n    if (fPCI) return false;\n'+s[body+1:]
p.write_text(s)

p=root/'Info.plist';data=plistlib.loads(p.read_bytes());prototype=next(v for v in data['IOKitPersonalities'].values() if v.get('IOProviderClass')=='IOPCIDevice').copy()
for key in list(prototype):
    if key.startswith('IOPCI'):prototype.pop(key)
prototype['IOProviderClass']='IOPCIDevice';prototype['IOPCIPrimaryMatch']='0x11001b73';prototype['IOPCITunnelCompatible']=True
# Mapper/access checks still select one actual device; matching alone admits nothing.
data['IOKitPersonalities']={**preserved,'T8103 dock FL1100':prototype};p.write_bytes(plistlib.dumps(data))
