/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* ACIO RTKit bridge, no Linux or XNU types across the ABI. */
#ifndef PD_CIO_RTKIT_H
#define PD_CIO_RTKIT_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct pd_cio_rtkit;
/* SRAM remains mapped and owned until successful stop. No firmware is loaded
 * or copied: use the boot firmware's validated __DATA SRAM segment. */
struct pd_cio_rtkit *pd_cio_rtkit_create(void *dt_entry,void *sram,uint64_t phys,size_t span);
int pd_cio_rtkit_boot(struct pd_cio_rtkit *);
int pd_cio_rtkit_stop(struct pd_cio_rtkit *);
int pd_cio_layer_up(void);
#ifdef __cplusplus
}
#endif
#endif
