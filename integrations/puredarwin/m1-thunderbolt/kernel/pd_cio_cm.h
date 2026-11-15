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
int pd_cio_cm_snapshot(PDCIOCM *); // one read-only actual PHY/link query
int pd_cio_cm_authorize_ts3(PDCIOCM *); // explicit private enum profile only
#ifdef PD_CIO_ENDPOINT_MAPPER
int pd_cio_cm_prepare_i210_dma(PDCIOCM *); // Actual mapper; BME remains stripped.
#endif
int pd_cio_cm_can_retrain(PDCIOCM *); // Actual two unplugged lanes, pristine PCI owner.
#endif
