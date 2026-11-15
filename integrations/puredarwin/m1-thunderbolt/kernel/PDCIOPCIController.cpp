// SPDX-License-Identifier: GPL-2.0
/* Native adapter for pd_pciec_port, following pinned pcie-apple.c.
 * Copyright (C) 2021 Alyssa Rosenzweig <alyssa@rosenzweig.io>
 * Copyright (C) 2021 Google LLC
 * Copyright (C) 2021 Corellium LLC
 * Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 */
#include "pd_cio_pcie_controller.h"
#include <IOKit/IODeviceTreeSupport.h>
#include <machine/machine_routines.h>
#include <sys/errno.h>
OSDefineMetaClassAndStructors(PDCIOPCIController,OSObject)
static void ordered(){__asm__ volatile("dsb sy; isb" ::: "memory");}
bool PDCIOPCIController::initWithOwner(IOService *lease,void *ctx,
 bool (*power)(void *),bool (*tunnel)(void *))
{
 owner=NULL;context=NULL;powerProof=NULL;tunnelProof=NULL;lock=NULL;
 admitted=touched=poisoned=cold=linked=false;
 if(!lease || !ctx || !power || !tunnel || !OSObject::init())return false;
 uint32_t chip=0;if(!pd_adt_u32("/chosen","chip-id",0,&chip) || chip!=0x8103)return false;
 // Actual registry children, not a caller-supplied "unbound" flag.
 const char *paths[]={"/arm-io/apciec1","/arm-io/dart-apciec1"};
 for(auto path:paths){
  auto node=IORegistryEntry::fromPath(path,gIODTPlane);if(!node)return false;
  auto children=node->getChildIterator(gIOServicePlane);
  bool bound=children && children->getNextObject();
  OSSafeReleaseNULL(children);node->release();if(bound)return false;
 }
 lock=IOLockAlloc();if(!lock)return false;
 lease->retain();owner=lease;context=ctx;powerProof=power;tunnelProof=tunnel;
 static const unsigned indices[]={0,2,4,3};
 static const uint64_t bases[]={0x530000000ULL,0x521000000ULL,0x520200000ULL,0x520004000ULL};
 static const uint64_t lengths[]={0x10000000,0x8000,0x4000,0x4000};
 for(unsigned n=0;n<4;++n)
  if(!windows[n].mapNode(paths[0],indices[n]) || windows[n].phys!=bases[n] || windows[n].len<lengths[n])return false;
 // T8103's published oe-fabric region has no separate native ADT reg entry.
 if(!windows[PD_PCIEC_OE].mapPhys(0x520008000ULL,0x4000) ||
    !powerGate.mapPhys(0x23b700450ULL,4))return false;
 io={this,read,write,tune,delay};admitted=true;return true;
}
bool PDCIOPCIController::live()
{
 if(!admitted || poisoned || !powerProof || !powerProof(context))return false;
 ordered();uint32_t gate=powerGate.r32(0);ordered();
 return gate!=UINT32_MAX && (gate&0xf0)==0xf0 && !(gate&((1u<<31)|(1u<<10)));
}
int PDCIOPCIController::read(void *ctx,pd_pciec_window win,uint32_t off,uint32_t *out)
{
 auto s=(PDCIOPCIController *)ctx;
 if(!out || (unsigned)win>PD_PCIEC_OE || (off&3) ||
    off>s->windows[win].len || s->windows[win].len-off<4)return -EINVAL;
 if(!s->live())return -ENODEV;
 ordered();uint32_t value=s->windows[win].r32(off);ordered();*out=value;return 0;
}
int PDCIOPCIController::write(void *ctx,pd_pciec_window win,uint32_t off,uint32_t value)
{
 auto s=(PDCIOPCIController *)ctx;
 if((unsigned)win>PD_PCIEC_OE || (off&3) ||
    off>s->windows[win].len || s->windows[win].len-off<4)return -EINVAL;
 if(!s->live())return -ENODEV;
 ordered();s->windows[win].w32(off,value);ordered();return 0;
}
int PDCIOPCIController::tune(void *ctx,pd_pciec_window win)
{auto s=(PDCIOPCIController *)ctx;return pd_pciec_t8103_tune(&s->io,win);}
void PDCIOPCIController::delay(void *,unsigned us)
{if(us>=1000)IOSleep(us/1000);else IODelay(us);}
int PDCIOPCIController::coldInit()
{
 if(!lock || ml_at_interrupt_context())return EINVAL;
 IOLockLock(lock);int ret;
 if(!live() || !tunnelProof(context))ret=ENODEV;
 else if(touched)ret=EBUSY;
 else{
  touched=true;ret=pd_pciec_cold_init(&io);if(ret)poisoned=true;else cold=true;
 }
 IOLockUnlock(lock);return ret<0?-ret:ret;
}
int PDCIOPCIController::linkStart()
{
 if(!lock || ml_at_interrupt_context())return EINVAL;
 IOLockLock(lock);int ret;
 if(!live() || !cold || !tunnelProof(context))ret=ENODEV;
 else if(linked)ret=EALREADY;
 else{ret=pd_pciec_link_start(&io);if(ret)poisoned=true;else linked=true;}
 IOLockUnlock(lock);return ret<0?-ret:ret;
}
int PDCIOPCIController::revoke()
{
 if(!lock || ml_at_interrupt_context())return EINVAL;
 IOLockLock(lock);admitted=false;int ret=touched?EBUSY:0;IOLockUnlock(lock);return ret;
}
void PDCIOPCIController::free()
{
 if(touched){IOLog("PDCIOPCIController: retaining programmed controller ownership until reboot\n");return;}
 for(auto &window:windows)window.unmap();powerGate.unmap();
 if(lock){IOLockFree(lock);lock=NULL;}
 OSSafeReleaseNULL(owner);OSObject::free();
}
