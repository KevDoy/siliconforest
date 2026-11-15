// SPDX-License-Identifier: GPL-2.0
/* ACIO M3 RTKit startup and SRAM mapping from Linux drivers/thunderbolt/apple.c.
 * Copyright (c) Sven Peter <sven@kernel.org>
 * RTKit port: Copyright The Asahi Linux Contributors, GPL-2.0-only OR MIT.
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/soc/apple/rtkit.h>
#include "pd_rtkit_compat.h"
#include "pd_cio_rtkit.h"
#include "pd_apple_nhi.h"
struct pd_cio_rtkit {
    struct device *dev;
    struct apple_rtkit *rtk;
    void *sram;
    uint64_t phys;
    size_t span;
};
static int setup(void *cookie,struct apple_rtkit_shmem *bfr)
{
    struct pd_cio_rtkit *s=cookie;
    uint64_t off;
    int ret=pd_apple_cio_sram_offset(bfr->iova,bfr->size,s->phys,s->span,&off);
    if(ret)return ret;
    bfr->iomem=(uint8_t *)s->sram+off;
    bfr->is_mapped=true;
    return 0;
}
static const struct apple_rtkit_ops ops={.shmem_setup=setup};
struct pd_cio_rtkit *pd_cio_rtkit_create(void *entry,void *sram,uint64_t phys,size_t span)
{
    if(!entry || !sram || !phys || !span || phys>UINT64_MAX-span)return NULL;
    pd_rtkit_enter();
    struct pd_cio_rtkit *s=kzalloc(sizeof(*s),GFP_KERNEL);
    if(!s)return NULL;
    s->sram=sram;s->phys=phys;s->span=span;
    s->dev=pd_rtkit_device_create("acio1",entry);
    if(!s->dev){kfree(s);return NULL;}
    /* One stable device/context per boot. A failed boot keeps its mailbox and
     * SRAM context until a successful stop; never recycle a live cookie. */
    return s;
}
int pd_cio_rtkit_boot(struct pd_cio_rtkit *s)
{
    if(!s)return -EINVAL;
    pd_rtkit_enter();
    if(s->rtk)return -EBUSY;
    int ret=pd_apple_mbox_bind(s->dev,0,"/arm-io/acio-cpu1");
    if(ret)return ret;
    s->rtk=apple_rtkit_init(s->dev,s,NULL,0,&ops);
    if(IS_ERR(s->rtk)){ret=PTR_ERR(s->rtk);s->rtk=NULL;return ret;}
    /* Keep the handle even after failure: the caller must drain it before
     * changing power/PHY. Boot does not imply NHI/router readiness. */
    return apple_rtkit_boot(s->rtk);
}
int pd_cio_rtkit_stop(struct pd_cio_rtkit *s)
{
    if(!s)return -EINVAL;
    pd_rtkit_enter();
    if(!s->rtk)return 0;
    int ret=apple_rtkit_poweroff(s->rtk);
    if(ret)return ret;
    apple_rtkit_free(s->rtk); // masks/joins mailbox receive work before freeing
    s->rtk=NULL;
    return 0;
}
