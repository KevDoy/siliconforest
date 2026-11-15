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
change(native,'#include <IOKit/IOLib.h>','#include <IOKit/IOLib.h>\n#include <IOKit/IOMapper.h>\n#include <IOKit/IORegistryEntry.h>\nextern \"C\" int pd_i210_bind_access(void *,void *);')
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
    auto bar0=fPCI->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    IOByteCount span=0;UInt64 physical=bar0?bar0->getPhysicalSegment(0,&span,kIOMemoryMapperNone):0;
    UInt64 bytes=bar0?bar0->getLength():0;
    fPCI->setProperty("PDCIOI210BAR0Physical",physical,64);
    fPCI->setProperty("PDCIOI210BAR0Bytes",bytes,64);
    // Linux apciec1 non-prefetchable32 window: PCI00100000..3fffffff
    // translates to CPU600100000..63fffffff. Never touch an untranslated BAR.
    if(!bar0 || bytes<0x20000 || span<bytes || physical<0x600100000ULL ||
       physical>=0x640000000ULL || bytes>0x640000000ULL-physical){
        fPCI->setProperty("PDCIOI210StartError","BAR0-resource-refused");fPCI=NULL;return false;
    }
    if(pd_i210_bind_access(dockHost,fPCI)){
        fPCI->setProperty("PDCIOI210StartError","live-access-binding-refused");fPCI=NULL;return false;
    }
    fPCI->setProperty("PDCIOI210StartError","resources-and-access-ready");
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
prototype['IOProviderClass']='IOPCIDevice';prototype['IOPCIPrimaryMatch']='0x15338086';prototype['IOPCITunnelCompatible']=True
data['IOKitPersonalities']={'T8103 dock I210':prototype}
plist.write_bytes(plistlib.dumps(data))

# Keep real driver/bootstrap linker-set entries while eliminating unreachable
# private compatibility functions from this capacity-constrained collection.
change('src/Kernel/fbsdkpi/haiku/freebsd_network/compat/sys/linker_set.h',
 '__section("__DATA," __PDSET_##set) __used',
 '__section("__DATA," __PDSET_##set ",regular,no_dead_strip") __used')
cm=root/'src/Kernel/Extensions/PDFBSDem/CMakeLists.txt'
with cm.open('a') as f:f.write('\ntarget_link_options(PDFBSDem.kext PRIVATE "LINKER:-dead_strip")\n')

# The native guard and personality admit only I210. Keep its exact upstream
# initializer and reject every other MAC type; dead stripping can then drop
# unused adapter-family initializers instead of carrying x86-era devices.
api=root/'src/Kernel/Extensions/PDFBSDem/dev/e1000/e1000_api.c'
a=api.read_text();fn=a.index('s32 e1000_setup_init_funcs(')
start=a.index('\tswitch (hw->mac.type) {',fn)
end=a.index('\n\t}\n\n\t/*\n\t * Initialize the rest',start)+len('\n\t}')
old=a[start:end]
assert old.count('e1000_init_function_pointers_i210(hw);')==1
a=a[:start]+'\tswitch (hw->mac.type) {\n\tcase e1000_i210:\n\t\te1000_init_function_pointers_i210(hw);\n\t\tbreak;\n\tdefault:\n\t\tret_val = -E1000_ERR_CONFIG;\n\t\tbreak;\n\t}'+a[end:]
api.write_text(a)

# Optimize only this new private driver bundle for collection capacity.
with cm.open('a') as f:f.write('\nforeach(t PDFBSDem_core PDFBSDem_iflib PDFBSDem_driver PDFBSDem.kext)\n target_compile_options(${t} PRIVATE -Oz)\nendforeach()\n')

# Guard every scalar/bulk FreeBSD BAR access in this private bundle. Bulk
# accessors use these scalar primitives. No shared bus header is modified.
bus='src/Kernel/fbsdkpi/haiku/freebsd_network/compat/machine/generic/bus.h'
bp=root/bus;b=bp.read_text();needle='#include <machine/_bus.h>';assert b.count(needle)==1
b=b.replace(needle,needle+'\n#ifdef __cplusplus\nextern "C" int pd_i210_mmio_access(void);\n#else\nint pd_i210_mmio_access(void);\n#endif')
assert b.count('if (tag != BUS_SPACE_TAG_MEM)')==8
b=b.replace('if (tag != BUS_SPACE_TAG_MEM)','if (tag != BUS_SPACE_TAG_MEM || !pd_i210_mmio_access())');bp.write_text(b)
os=root/'src/Kernel/fbsdkpi/xnu/pd_fbsd_os.cpp'
with os.open('a') as f:f.write('''
// Real retained native endpoint guard, published before FreeBSD threads/IRQs.
static IOService *pd_i210_host;
static IOService *pd_i210_device;
static const OSSymbol *pd_i210_access_key;
extern "C" int pd_i210_bind_access(void *host,void *device)
{
 if(!host || !device || pd_i210_host)return -1;
 auto key=OSSymbol::withCString("PDCIOPCIEndpointAccess");if(!key)return -1;
 auto h=(IOService *)host;auto d=(IOService *)device;
 if(h->callPlatformFunction(key,false,d,NULL,NULL,NULL)!=kIOReturnSuccess){key->release();return -1;}
 h->retain();d->retain();pd_i210_access_key=key;pd_i210_device=d;pd_i210_host=h;return 0;
}
extern "C" int pd_i210_mmio_access(void)
{
 return pd_i210_host && pd_i210_access_key &&
  pd_i210_host->callPlatformFunction(pd_i210_access_key,false,pd_i210_device,NULL,NULL,NULL)==kIOReturnSuccess;
}
''')
