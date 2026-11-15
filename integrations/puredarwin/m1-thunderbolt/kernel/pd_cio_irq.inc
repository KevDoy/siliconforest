/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Appended only to PDAppleCIO's private PDRtkitPlatform.cpp copy. Reuse the
 * existing masked-AIC/software-workloop engine, with real T8103 NHI sources.
 * Register/IRQ ownership follows Linux apple.c, Copyright Sven Peter and
 * The Asahi Linux Contributors. No IRQ is registered by resolution alone.
 */
extern "C" int pd_cio_nhi_irqs_resolve(int out[25])
{
    if (!out || __atomic_load_n(&gInitState,__ATOMIC_ACQUIRE)!=2)return -EINVAL;
    if (gIrqMode!=kIrqModeReregister)return -EOPNOTSUPP;
    static bool resolved;
    static int handles[25];
    IOLockLock(gLock);
    if(resolved){memcpy(out,handles,sizeof(handles));IOLockUnlock(gLock);return 0;}
    auto node=IORegistryEntry::fromPath("/arm-io/acio1",gIODTPlane);
    auto dart=IORegistryEntry::fromPath("/arm-io/dart-acio1",gIODTPlane);
    auto ints=node?OSDynamicCast(OSData,node->getProperty("interrupts")):NULL;
    auto dartInts=dart?OSDynamicCast(OSData,dart->getProperty("interrupts")):NULL;
    uint32_t vectors[25]={};
    int ret=0;unsigned allocated=0;
    IOService *nub=NULL;
    OSArray *ca=NULL,*sa=NULL;
    do {
        if(!ints || ints->getLength()!=24*4 || !dartInts || dartInts->getLength()!=4){ret=-EINVAL;break;}
        for(unsigned i=0;i<24;++i){
            vectors[i]=OSReadLittleInt32(ints->getBytesNoCopy(),i*4);
            // ADT lists RX first, TX second. NHI enable bits use TX first.
            if(vectors[i]!=(i<12?875+i:863+i-12)){ret=-EINVAL;break;}
        }
        vectors[24]=OSReadLittleInt32(dartInts->getBytesNoCopy(),0);
        if(ret || vectors[24]!=890){ret=-EINVAL;break;}
        nub=new IOService;ca=OSArray::withCapacity(25);sa=OSArray::withCapacity(25);
        if(!nub || !nub->init() || !ca || !sa){ret=-ENOMEM;break;}
        for(unsigned i=0;i<25;++i){
            IOInterruptVectorNumber vector=vectors[i];
            auto spec=OSData::withBytes(&vector,sizeof(vector));
            bool ok=spec && ca->setObject(gIODTDefaultInterruptController) && sa->setObject(spec);
            OSSafeReleaseNULL(spec);
            if(!ok){ret=-ENOMEM;break;}
        }
        if(ret)break;
        if(!nub->setProperty(gIOInterruptControllersKey,ca) ||
           !nub->setProperty(gIOInterruptSpecifiersKey,sa) || !nub->attach(gHost)){ret=-ENXIO;break;}
        nub->setName("PDAppleCIO-NHI-IRQs");
        for(unsigned i=0;i<25;++i){
            handles[i]=allocIrq(nub,i,vectors[i],false,i==24?"acio1-dart":"acio1-nhi");
            if(handles[i]<0){ret=-ENOSPC;break;}
            ++allocated;
        }
        if(ret)break;
        // Handles/nub persist across cable cycles; irq_free masks and joins
        // actual handlers before a subsequent request can reuse the handle.
        resolved=true;memcpy(out,handles,sizeof(handles));
    }while(false);
    if(ret){
        for(unsigned i=0;i<allocated;++i)gIrqs[handles[i]].used=false;
        if(nub){if(nub->getProvider())nub->detach(gHost);nub->release();}
    }
    OSSafeReleaseNULL(ca);OSSafeReleaseNULL(sa);OSSafeReleaseNULL(node);OSSafeReleaseNULL(dart);
    IOLockUnlock(gLock);return ret;
}
