/* SPDX-License-Identifier: MIT */
/* Copyright The Asahi Linux Contributors
 * Lane property maps follow m1n1 src/kboot_atc.c; see NOTICE.
 */
#ifndef PD_ATC_CALIBRATION_H
#define PD_ATC_CALIBRATION_H
#include "pd_atc_modes.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct pd_atc_cal_entry { const void *raw;size_t bytes;uint32_t base; };
struct pd_atc_cal_bundle {
    const struct pd_atc_io *io;
    void *adt_ctx;
    /* Returned bytes remain immutable and alive through apply. The native
     * adapter retains their OSData or borrows permanent original ADT data. */
    const void *(*getprop)(void *,const char *path,const char *name,size_t *bytes);
    struct pd_atc_program program;
    struct pd_atc_cal_entry entries[8];
    unsigned count;
    bool checked;
};
struct pd_atc_calibration pd_atc_calibration_callbacks(struct pd_atc_cal_bundle *);
#ifdef __cplusplus
}
#endif
#endif
