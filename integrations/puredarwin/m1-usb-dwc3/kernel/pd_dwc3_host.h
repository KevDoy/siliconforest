/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) The Asahi Linux Contributors
 * Copyright (C) 2010-2011 Texas Instruments Incorporated
 * Based on dwc3-apple.c: Copyright (c) 2018 The Linux Foundation;
 * Copyright (c) 2015 Texas Instruments Incorporated.
 * T8103 host-only subset; no gadget driver or device event DMA is started.
 */
#ifndef PD_DWC3_HOST_H
#define PD_DWC3_HOST_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct pd_dwc3_io {
    void *ctx;
    uint32_t (*read)(void *, uint32_t);
    void (*write)(void *, uint32_t, uint32_t);
    void (*delay_us)(void *, unsigned);
};
/* PHY running, PIPE dummy, external reset released, xHCI stopped. */
int pd_dwc3_host_init(const struct pd_dwc3_io *io);
#ifdef __cplusplus
}
#endif
#endif
