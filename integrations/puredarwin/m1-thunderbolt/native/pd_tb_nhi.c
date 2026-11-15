// SPDX-License-Identifier: GPL-2.0
/* Apple ACIO1 -> pinned Linux NHI ring engine/software connection manager.
 * Ring/interrupt/cable sequence from apple.c (linux-aurora 6a08a08e),
 * Copyright (c) Sven Peter <sven@kernel.org>, The Asahi Linux Contributors.
 * Uses neither Intel NHI reset registers nor the LinuxKPI IRQ success stubs.
 * Native resource/DMA/PM/publication installation must precede this bridge.
 */
#include "pd_tb_nhi.h"
#include <linux/of.h>
#include "pd_tb_dma.h"
#include "driver/tb.h"
#include "driver/nhi.h"
#include "driver/nhi_regs.h"
#define IRQ_STATUS 0xd0000
#define IRQ_ENABLE 0xd0010
#define IRQ_THROTTLE 0xd004c
#define STRIDE 0x4000
#define TX_BASE 0x10000
#define RX_BASE 0x80000
struct pd_tb_nhi {
 struct tb_nhi nhi;
 struct pd_tb_nhi_config config;
 struct tb_ring *tx[12],*rx[12];
 struct tb *domain;
 bool started,failed;
};
static struct pd_tb_nhi *owner(struct tb_nhi *nhi)
{ return container_of(nhi,struct pd_tb_nhi,nhi); }
static unsigned index_of(struct tb_ring *r){return r->hop+(r->is_tx?0:12);}
static bool present(struct tb_nhi *nhi)
{struct pd_tb_nhi *s=owner(nhi);return !__atomic_load_n(&s->failed,__ATOMIC_ACQUIRE) && s->config.powered(s->config.ctx);}
static void fail(struct pd_tb_nhi *s)
{__atomic_store_n(&s->failed,true,__ATOMIC_RELEASE);pd_tb_dma_quarantine();pd_tb_native_fault(-EIO);}
static void active(struct tb_ring *r,bool yes)
{
 struct pd_tb_nhi *s=owner(r->nhi);unsigned idx=index_of(r);
 if(yes && !present(r->nhi)){fail(s);return;}
 if(yes && r->interval_nsec){
  u32 interval=min_t(u32,r->interval_nsec,65535u*256u);
  writel(DIV_ROUND_UP(interval,256),s->config.nhi_base+IRQ_THROTTLE+4*idx);
 }
 u32 v=readl(s->config.nhi_base+IRQ_ENABLE);
 if(v==U32_MAX){fail(s);return;}
 writel(yes?v|BIT(idx):v&~BIT(idx),s->config.nhi_base+IRQ_ENABLE);
 if(!!(readl(s->config.nhi_base+IRQ_ENABLE)&BIT(idx))!=yes)fail(s);
}
static void mask(struct tb_ring *r,bool yes){active(r,!yes);}
static int interrupt(void *arg)
{
 struct tb_ring *r=arg;struct pd_tb_nhi *s=owner(r->nhi);
 spin_lock(&r->nhi->lock);spin_lock(&r->lock);
 writel(BIT(index_of(r)),s->config.nhi_base+IRQ_STATUS);
 if(!pd_tb_native_ready()){
  active(r,false);fail(s);
 }
 if(r->running && !__atomic_load_n(&s->failed,__ATOMIC_ACQUIRE)){
  if(r->start_poll){mask(r,true);r->start_poll(r->poll_data);}
  else schedule_work(&r->work);
 }
 spin_unlock(&r->lock);spin_unlock(&r->nhi->lock);return 1;
}
static int request_ring_irq(struct tb_ring *r,bool no_suspend)
{
 (void)no_suspend;struct pd_tb_nhi *s=owner(r->nhi);
 if(r->hop<0 || r->hop>=12 || !present(r->nhi))return -ENODEV;
 int irq=r->is_tx?s->config.tx_irq[r->hop]:s->config.rx_irq[r->hop];
 int ret=s->config.irq_request(irq,interrupt,r,r->is_tx?"acio1-tx":"acio1-rx");
 if(ret)return ret;
 r->irq=irq;s->config.irq_enable(irq);return 0;
}
static void release_irq(struct tb_ring *r)
{
 struct pd_tb_nhi *s=owner(r->nhi);
 if(r->running)fail(s);
 if(r->irq<=0)return;
 // Native AIC masking and join precede any descriptor retirement.
 s->config.irq_free(r->irq);r->irq=0;
}
static void configure(struct tb_ring *r,u32 flags,u32 e2e)
{
 struct pd_tb_nhi *s=owner(r->nhi);
 if(!present(r->nhi)){fail(s);return;}
 void __iomem *options=s->config.nhi_base+(r->is_tx?TX_BASE:RX_BASE)+r->hop*STRIDE+0x10;
 if(r->is_tx)writel(r->hop==0?2:r->hop<=5?40:5,options+4);
 else{
  u32 pdf=r->sof_mask<<16|r->eof_mask;
  writel(pdf,options+4);writel(pdf,s->config.pdf_base+r->hop*STRIDE);
 }
 writel(flags|e2e,options); // VALID and E2E sampled together by firmware.
 if(!r->hop)
  pr_info("PDAppleCIO: control ring %s%u dma=%#llx size=%u flags=%#x readback=%#x\n",
          r->is_tx?"TX":"RX",r->hop,(unsigned long long)r->descriptors_dma,
          r->size,flags|e2e,readl(options));
 if(readl(options)!=(flags|e2e))fail(s);
}
static int no_pcie(struct tb_nhi *nhi){(void)nhi;return -EOPNOTSUPP;}
static int no_dp(struct tb_nhi *nhi,struct tb_port *in,struct tb_port *out)
{(void)nhi;(void)in;(void)out;return -EOPNOTSUPP;}
static const struct tb_nhi_ops operations={
 .request_ring_irq=request_ring_irq,.release_ring_irq=release_irq,
 .ring_interrupt_active=active,.ring_interrupt_mask=mask,.ring_configure=configure,
 .is_present=present,.pci_tunnel_pre_activate=no_pcie,.dp_tunnel_pre_activate=no_dp,
};
static const struct tb_nhi_ring_layout layout={TX_BASE,RX_BASE,STRIDE,TX_BASE+0x10,RX_BASE+0x10,STRIDE};
int pd_tb_nhi_check_stopped(void *opaque)
{
 struct tb_ring *r=opaque;
 if(!r || r->nhi->ops!=&operations)return -EOPNOTSUPP;
 struct pd_tb_nhi *s=owner(r->nhi);
 if(!s->config.powered(s->config.ctx) ||
    (readl(s->config.nhi_base+(r->is_tx?TX_BASE:RX_BASE)+r->hop*STRIDE+0x10)&RING_FLAG_ENABLE) ||
    (readl(s->config.nhi_base+IRQ_ENABLE)&BIT(index_of(r)))){
  fail(s);return -EIO;
 }
 return 0;
}
int pd_tb_nhi_create(const struct pd_tb_nhi_config *c,void **out)
{
 if(!c || !out || !c->device || !c->nhi_base || !c->pdf_base || !c->powered ||
    !c->irq_request || !c->irq_enable || !c->irq_free)return -EINVAL;
 if(!pd_tb_native_ready() || !c->powered(c->ctx))return -ENODEV;
 if((readl(c->nhi_base)&0x3ff)!=12 || readl(c->nhi_base+IRQ_ENABLE))return -EBUSY;
 for(unsigned i=0;i<12;++i){
  if(c->rx_irq[i]<=0 || c->tx_irq[i]<=0)return -EINVAL;
  for(unsigned j=0;j<i;++j)if(c->rx_irq[i]==c->rx_irq[j] || c->tx_irq[i]==c->tx_irq[j])return -EINVAL;
  for(unsigned j=0;j<12;++j)if(c->rx_irq[i]==c->tx_irq[j])return -EINVAL;
  if(readl(c->nhi_base+TX_BASE+i*STRIDE+0x10)&RING_FLAG_ENABLE ||
     readl(c->nhi_base+RX_BASE+i*STRIDE+0x10)&RING_FLAG_ENABLE)return -EBUSY;
 }
 int ret=pd_tb_dma_drain_status();if(ret)return ret;
 struct pd_tb_nhi *s=kzalloc(sizeof(*s),GFP_KERNEL);if(!s)return -ENOMEM;
 s->config=*c;s->nhi.dev=c->device;s->nhi.iobase=c->nhi_base;s->nhi.hop_count=12;
 s->nhi.ops=&operations;s->nhi.ring_layout=&layout;s->nhi.tx_rings=s->tx;s->nhi.rx_rings=s->rx;
 s->nhi.quirks=QUIRK_NO_DMA_PORT|QUIRK_NO_USB3_BW_ALLOC;
 spin_lock_init(&s->nhi.lock);init_completion(&s->nhi.domain_released);
 *out=s;return 0;
}
int pd_tb_nhi_start(void *ctx,uint32_t cable)
{
 struct pd_tb_nhi *s=ctx;if(!s || !cable || s->domain || !present(&s->nhi))return -EINVAL;
 pd_tb_dma_admit(true);
 s->domain=tb_probe(&s->nhi);if(!s->domain)return -ENODEV;
 int ret=tb_domain_add(s->domain,false);
 if(ret){tb_domain_put(s->domain);wait_for_completion(&s->nhi.domain_released);s->domain=NULL;return ret;}
 s->started=true;
 mutex_lock(&s->domain->lock);
 struct tb_switch *root=s->domain->root_switch;
 if(!root || !root->drom){ret=-EINVAL;goto done;}
 int cap=tb_switch_find_vse_cap(root,0x00); // TB_VSE_CAP_APPLE from pinned apple.c.
 if(cap<0){ret=cap;goto done;}
 struct tb_port *port;
 tb_switch_for_each_port(root,port)if(tb_port_is_null(port)){
  ret=tb_port_unlock(port);if(ret)goto done;
 }
 ret=tb_sw_write(root,&cable,TB_CFG_SWITCH,cap+1,1);
 done:mutex_unlock(&s->domain->lock);return ret;
}
int pd_tb_nhi_stop(void *ctx)
{
 struct pd_tb_nhi *s=ctx;if(!s)return -EINVAL;
 if(s->domain){
  if(s->started)tb_domain_remove(s->domain);else tb_domain_put(s->domain);
  s->started=false;wait_for_completion(&s->nhi.domain_released);s->domain=NULL;
 }
 pd_tb_dma_admit(false);
 if(__atomic_load_n(&s->failed,__ATOMIC_ACQUIRE) || !s->config.powered(s->config.ctx))return -EIO;
 if(readl(s->config.nhi_base+IRQ_ENABLE))return -EBUSY;
 for(unsigned i=0;i<12;++i)if(s->tx[i] || s->rx[i])return -EBUSY;
 return pd_tb_dma_drain_status();
}

/* The parent device is a lifetime/reference anchor for the actual native
 * owner. Its IORegistry node is that owner, not a second synthetic host. */
struct device_node *pd_of_node_for_entry(void *entry);
static void parent_release(struct device *dev){kfree(dev);}
int pd_tb_nhi_device_create(void *entry,const void *drom,size_t bytes,void **out)
{
 if(!entry || !drom || bytes!=76 || !out || !pd_tb_native_ready())return -EINVAL;
 struct device *dev=kzalloc(sizeof(*dev),GFP_KERNEL);if(!dev)return -ENOMEM;
 device_initialize(dev);dev->release=parent_release;
 struct device_node *original=pd_of_node_for_entry(entry);
 struct device_node *node=original?kzalloc(sizeof(*node),GFP_KERNEL):NULL;
 struct property *raw=node?kzalloc(sizeof(*raw)+bytes,GFP_KERNEL):NULL;
 if(node && raw){
  // Private node/cache: preserve raw ADT byte order. LinuxKPI's generic
  // to_fdt otherwise swaps every non-string property divisible by four.
  *node=*original;node->pd_next=NULL;node->properties=raw;
  raw->name=kstrdup("apple,thunderbolt-drom",GFP_KERNEL);
  raw->length=bytes;raw->value=raw+1;memcpy(raw->value,drom,bytes);
  if(raw->name)dev->of_node=node;
 }
 int ret=dev->of_node?dev_set_name(dev,"acio1"):-ENODEV;
 if(ret){put_device(dev);return ret;}
 *out=dev;return 0;
}
int pd_tb_nhi_bus_init(void){return tb_domain_init();}
