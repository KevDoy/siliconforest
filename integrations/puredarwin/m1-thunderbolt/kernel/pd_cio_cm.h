/* SPDX-License-Identifier: MIT */
#ifndef PD_CIO_CM_H
#define PD_CIO_CM_H
#include <stdint.h>
#include <IOKit/IOService.h>
/* Native IORegistry/PM/DART owner for the private Linux CM. One pinned object
 * per controller per boot. Stop failure retains every resource and PHY lease.
 * This API accepts only already-validated mapped ACIO1 resources. */
struct PDCIOCM;
PDCIOCM *pd_cio_cm_create(IOService *owner,void *power_ctx,bool (*powered)(void *));
int pd_cio_cm_start(PDCIOCM *,void *nhi,void *pdf,uint32_t cable);
int pd_cio_cm_stop(PDCIOCM *);
#endif
