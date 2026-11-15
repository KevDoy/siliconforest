/* SPDX-License-Identifier: GPL-2.0 */
#ifndef PD_TB_NHI_H
#define PD_TB_NHI_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* Native owner supplies mapped, powered ACIO1 resources and resolved AIC
 * handles. This interface does not map registers or acquire a PHY lease.
 * All resources/context must remain valid until reboot, including on errors.
 * irq_free must mask and JOIN its actual native handler before returning. */
struct pd_tb_nhi_config {
 void *ctx,*device,*nhi_base,*pdf_base;
 int rx_irq[12],tx_irq[12];
 bool (*powered)(void *);
 int (*irq_request)(int,int (*)(void *),void *,const char *);
 void (*irq_enable)(int);
 void (*irq_free)(int);
};
#ifdef __cplusplus
extern "C" {
#endif
int pd_tb_nhi_device_create(void *dt_entry,const void *raw_drom,size_t length,void **device);
int pd_tb_nhi_bus_init(void);
int pd_tb_nhi_create(const struct pd_tb_nhi_config *,void **);
int pd_tb_nhi_start(void *,uint32_t cable_info);
/* Bounded read-only diagnostic, never an authorization or link-state cache.
 * Public model IDs only; no UID, DROM bytes or DMA/physical addresses. */
struct pd_tb_nhi_port_status {
 uint32_t port,state,error,remote,remote_vendor,remote_device;
};
struct pd_tb_nhi_status {
 uint32_t root_vendor,root_device,max_port,port_count,cable_info;
 struct pd_tb_nhi_port_status ports[16];
};
int pd_tb_nhi_snapshot(void *,struct pd_tb_nhi_status *);
/* Failure retains context, DMA and controller ownership. No timeout-free
 * retirement is implied: Linux domain teardown joins its actual workers. */
int pd_tb_nhi_stop(void *);
/* Called by the private Linux ring engine with its NHI/ring locks held,
 * after disabling IRQ/VALID and BEFORE clearing descriptor addresses. */
int pd_tb_nhi_check_stopped(void *ring);
#ifdef __cplusplus
}
#endif
#endif
