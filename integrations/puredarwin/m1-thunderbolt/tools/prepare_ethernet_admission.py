#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Modify only the dedicated already-owned-DMA Ethernet staging tree.
import pathlib,sys
root=pathlib.Path(sys.argv[1])
def change(path,old,new):
 p=root/path;s=p.read_text()
 if s.count(old)!=1:raise SystemExit('Ethernet admission source shape changed: '+path)
 p.write_text(s.replace(old,new))
native='src/Kernel/fbsdkpi/xnu/PDFBSDEthernet.cpp'
change(native,'#include <IOKit/IOLib.h>','#include <IOKit/IOLib.h>\n#include <IOKit/IOMapper.h>\n#include <IOKit/IORegistryEntry.h>')
change(native,'\tfPCI->retain();','''
    // A real bound mapper is mandatory before FreeBSD can allocate DMA.
    IOService *dockHost=NULL;unsigned depth=0;
    for(IORegistryEntry *p=fPCI;p && depth++<64;p=p->getParentEntry(gIOServicePlane))
        if(p->metaCast("PDCIOPCIHost")){dockHost=OSDynamicCast(IOService,p);break;}
    auto binding=fPCI->copyProperty("iommu-parent");
    auto actual=OSDynamicCast(IOMapper,binding);
    IOMapper *resolved=actual?IOMapper::copyMapperForDevice(fPCI):NULL;
    bool owned=dockHost && actual && resolved==actual &&
        actual->metaCast("PDCIOPCIDartMapper") && actual->getProvider()==fPCI &&
        fPCI->configRead32(0)==0x15338086 && (fPCI->configRead32(8)>>8)==0x020000 &&
        actual->callPlatformFunction("PDCIOPCIDMAReady",false,fPCI,NULL,NULL,NULL)==kIOReturnSuccess;
    OSSafeReleaseNULL(resolved);OSSafeReleaseNULL(binding);
    if(!owned){fPCI=NULL;return false;}
    fPCI->retain();''')
change(native,'\tfPCI->setBusMasterEnable(true);','    // Bus mastering is admitted after actual FreeBSD IRQ setup below.')
change(native,'\tpd_fbsd_set_cookie(fIfp, this);','''
    // The MSI parent rearm is a real workloop action. Bounded attach-only
    // wait; no persistent poll or timer. Interface is not published yet.
    IOReturn admitted=kIOReturnNotReady;
    for(unsigned attempt=0;attempt<100;++attempt){
        admitted=dockHost->callPlatformFunction("PDCIOPCIEnableI210DMA",false,fPCI,NULL,NULL,NULL);
        if(admitted!=kIOReturnNotReady)break;
        IOSleep(10);
    }
    UInt16 command=fPCI->configRead16(4);
    if(admitted!=kIOReturnSuccess || command==UINT16_MAX || !(command&4)){
        IOLog("PDFBSDem: actual I210 DMA/MSI admission refused 0x%08x\\n",admitted);
        if(admitted==kIOReturnSuccess || admitted==kIOReturnIOError ||
           command==UINT16_MAX || (command&4)){
            setProperty("PDCIOI210StartupQuarantined",true);retain();
            return false; // Ambiguous BME: keep IRQ/context/DMA until reboot.
        }
        pd_fbsd_detach(fIfp);fIfp=NULL;fPCI->close(this);OSSafeReleaseNULL(fPCI);
        return false;
    }
    pd_fbsd_set_cookie(fIfp, this);''')
change(native,'\tif (!publishMedia())\n\t\treturn false;','\tif (!publishMedia()) { setProperty("PDCIOI210StartupQuarantined",true); retain(); return false; }')
change(native,'\t\tIOLog(PDFBSD_STR(PDFBSD_CLASS) ": attachInterface failed\\n");\n\t\treturn false;','\t\tIOLog(PDFBSD_STR(PDFBSD_CLASS) ": attachInterface failed\\n");\n\t\tsetProperty("PDCIOI210StartupQuarantined",true);retain();return false;')
change('src/Kernel/fbsdkpi/haiku/freebsd_iflib/iflib.c','\tpci_enable_busmaster(dev);\n\n\treturn (iflib_device_register(dev, NULL, sctx, &ctx));','\t/* Dedicated I210: native wrapper checks DMA/MSI after registration. */\n\treturn (iflib_device_register(dev, NULL, sctx, &ctx));')
change('src/Kernel/fbsdkpi/pd/pd_pci.c','command | PCI_command_memory | PCI_command_master','(command & ~PCI_command_master) | PCI_command_memory')

# Match only the actual I210; admission still requires a live native mapper.
import plistlib
plist=root/'src/Kernel/Extensions/PDFBSDem/Info.plist'
data=plistlib.loads(plist.read_bytes());personalities=data['IOKitPersonalities']
prototype=next(iter(personalities.values())).copy()
for key in list(prototype):
 if key.startswith('IOPCI'):prototype.pop(key)
prototype['IOProviderClass']='IOPCIDevice';prototype['IOPCIPrimaryMatch']='0x15338086'
data['IOKitPersonalities']={'T8103 dock I210':prototype}
plist.write_bytes(plistlib.dumps(data))
