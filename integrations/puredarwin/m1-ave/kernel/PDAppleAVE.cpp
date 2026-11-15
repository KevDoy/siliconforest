// SPDX-License-Identifier: GPL-2.0-only
/* PDAppleAVD-pattern service/device plumbing. Diagnostic phase only: reads
 * live ADT and bounded preloaded TEXT identity, no encoder/MMIO power or DMA.
 * Copyright (c) 2026 SiliconForest contributors. See ../NOTICE.
 */
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <pexpert/pexpert.h>
#include <pexpert/arm64/boot.h>
#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include "pdave/pdave_ioctl.h"
extern "C" {
#include "pdave/pdave_identity.h"
}
#define LOG(fmt,...) IOLog("PDAppleAVE: " fmt "\n",##__VA_ARGS__)
class PDAppleAVE:public IOService {
 OSDeclareDefaultStructors(PDAppleAVE);
 public:bool start(IOService *) APPLE_KEXT_OVERRIDE;
};
OSDefineMetaClassAndStructors(PDAppleAVE,IOService);
struct Segment {uint64_t physical,iova,remap;uint32_t size,flags;};
static_assert(sizeof(Segment)==32,"ADT segment");
static_assert(sizeof(pdave_info)==192,"INFO ABI");
static pdave_info gInfo;
static IOLock *gInfoLock;
static bool gInventoried;
static int inventory();
static int openNode(dev_t,int,int,proc_t){return 0;}
static int closeNode(dev_t,int,int,proc_t){return 0;}
static int ioctlNode(dev_t,u_long command,caddr_t data,int,proc_t) {
 if(command!=PDAVE_IOC_INFO)return ENOTTY;
 IOLockLock(gInfoLock);
 if(!gInventoried){gInfo.error=inventory();gInventoried=true;}
 memcpy(data,&gInfo,sizeof(gInfo));
 IOLockUnlock(gInfoLock);return 0;
}
static struct cdevsw methods={.d_open=openNode,.d_close=closeNode,.d_read=eno_rdwrt,
 .d_write=eno_rdwrt,.d_ioctl=ioctlNode,.d_stop=eno_stop,.d_reset=eno_reset,
 .d_ttys=nullptr,.d_select=eno_select,.d_mmap=eno_mmap,.d_strategy=eno_strat,
 .d_reserved_1=eno_getc,.d_reserved_2=eno_putc,.d_type=0};
static const OSData *property(IORegistryEntry *node,const char *key) {
 return node?OSDynamicCast(OSData,node->getProperty(key)):nullptr;
}
static bool number(IORegistryEntry *node,const char *key,void *out,size_t size) {
 const OSData *d=property(node,key);if(!d||d->getLength()!=size)return false;
 memcpy(out,d->getBytesNoCopy(),size);return true;
}
static bool text(IORegistryEntry *node,const char *key,char *out,size_t size) {
 const OSData *d=property(node,key);if(!d||!d->getLength()||d->getLength()>size)return false;
 const char *s=(const char *)d->getBytesNoCopy();size_t n=strnlen(s,d->getLength());
 if(n==d->getLength())return false;
 for(size_t i=0;i<n;i++)if((unsigned char)s[i]<32||(unsigned char)s[i]>126)return false;
 memcpy(out,s,n);out[n]=0;return true;
}
static int inventory() {
 IORegistryEntry *chosen=IORegistryEntry::fromPath("/chosen",gIODTPlane);
 IORegistryEntry *ave=IORegistryEntry::fromPath("/arm-io/ave",gIODTPlane);
 uint64_t dram=0;uint32_t preloaded=0;Segment segment[2];
 bool valid=number(chosen,"chip-id",&gInfo.soc,4)&&gInfo.soc==0x8103 &&
  text(chosen,"firmware-version",gInfo.iboot,sizeof(gInfo.iboot))&&
  number(chosen,"dram-base",&dram,8)&&number(ave,"pre-loaded",&preloaded,4)&&preloaded==1&&
  number(ave,"segment-ranges",segment,sizeof(segment));
 OSSafeReleaseNULL(chosen);OSSafeReleaseNULL(ave);
 if(!valid)return ENODEV;
 gInfo.flags|=PDAVE_INFO_PRELOADED;gInfo.segments=2;
 const boot_args *boot=(const boot_args *)PE_state.bootArgs;
 if(!boot||!dram||dram>=boot->physBase)return EINVAL;
 for(unsigned i=0;i<2;i++) {
  const Segment &s=segment[i];gInfo.segment_size[i]=s.size;gInfo.segment_flags[i]=s.flags;
  if(s.size!=(i?0x128000u:0xcc000u)||s.flags!=(i?0u:3u)||s.iova!=(i?0xcc000u:0u)||
     (s.physical&0x3fff)||s.physical<dram||s.physical>=boot->physBase||
     s.size>boot->physBase-s.physical)return EINVAL;
 }
 if(segment[0].physical<segment[1].physical+segment[1].size&&
    segment[1].physical<segment[0].physical+segment[0].size)return EINVAL;
 gInfo.flags|=PDAVE_INFO_OUTSIDE_VM;
 // This proves exclusion from XNU's allocator, NOT that stage1/2 left DATA cold.
 // DATA is neither mapped nor copied in this diagnostic. No cold-reserved flag.
 IOMemoryDescriptor *memory=IOMemoryDescriptor::withPhysicalAddress(segment[0].physical,segment[0].size,kIODirectionIn);
 if(!memory)return ENOMEM;
 IOMemoryMap *map=memory->map(kIOMapCopybackCache|kIOMapReadOnly);
 if(!map){memory->release();return ENOMEM;}
 const void *base=(const void *)map->getVirtualAddress();
 int fw=pdave_find_identity(base,segment[0].size,"AppleAVE2FW-",gInfo.firmware,sizeof(gInfo.firmware));
 int rt=pdave_find_identity(base,segment[0].size,"RTKit-",gInfo.rtkit,sizeof(gInfo.rtkit));
 map->release();memory->release();
 if(fw||rt)return fw?fw:rt;
 if(strcmp(gInfo.iboot,"iBoot-8422.141.2")||strcmp(gInfo.firmware,"AppleAVE2FW-6070.11.1")||
    strcmp(gInfo.rtkit,"RTKit-2062.141.1"))return ENOTSUP;
 gInfo.flags|=PDAVE_INFO_TEXT_IDENTITY;
 return 0;
}
bool PDAppleAVE::start(IOService *provider) {
 if(!IOService::start(provider))return false;
 uint32_t mode=0;PE_parse_boot_argn("pdave",&mode,sizeof(mode));
 if(mode>1)return false; // no hardware mode implemented
 gInfoLock=IOLockAlloc();if(!gInfoLock)return false;
 bzero(&gInfo,sizeof(gInfo));gInfo.abi=PDAVE_ABI_VERSION;gInfo.size=sizeof(gInfo);
 // Default publishes INFO only. Explicit root ioctl triggers the bounded read.
 // Optional pdave=1 prefetches it; no boot-argument bytes are required.
 if(mode==1){gInfo.error=inventory();gInventoried=true;}
 int major=cdevsw_add(-1,&methods);if(major<0){IOLockFree(gInfoLock);gInfoLock=nullptr;return false;}
 void *node=devfs_make_node(makedev(major,0),DEVFS_CHAR,0,0,0600,"pdave");
 if(!node){cdevsw_remove(major,&methods);IOLockFree(gInfoLock);gInfoLock=nullptr;return false;}
 LOG("diagnostic INFO only (lazy=%u); codec caps=0; no DATA/power/DMA access",!gInventoried);
 registerService();return true;
}
