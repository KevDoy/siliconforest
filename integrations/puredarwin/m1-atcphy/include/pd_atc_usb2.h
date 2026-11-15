/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/* Copyright (C) The Asahi Linux Contributors
 * T8103 USB2 subset of drivers/phy/apple/atc.c, author Sven Peter.
 * MMIO is supplied by the kernel adapter or deterministic host tests.
 */
#ifndef PD_ATC_USB2_H
#define PD_ATC_USB2_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum pd_atc_window { PD_ATC_CORE, PD_ATC_USB2, PD_ATC_PIPE, PD_ATC_LPDPTX };
struct pd_atc_io {
    void *ctx;
    uint32_t (*read)(void *, enum pd_atc_window, uint32_t);
    void (*write)(void *, enum pd_atc_window, uint32_t, uint32_t);
    void (*delay_us)(void *, unsigned);
    int (*common_tunables)(void *);
};
/* Caller serializes all PHY access and has already drained any xHCI owner.
 * reset_assert leaves DWC3 clamped and USB2 isolated, without waking lanes.
 * start leaves DWC3 in reset; release_reset precedes DWC3 initialization.
 * select_host is deliberately AFTER core reset, matching Linux's xHCI path.
 * USB2 keeps every SuperSpeed lane OFF and PIPE on the dummy backend. */
int pd_atc_usb2_reset_assert(const struct pd_atc_io *io);
int pd_atc_usb2_start(const struct pd_atc_io *io, int reversed);
void pd_atc_usb2_release_reset(const struct pd_atc_io *io);
void pd_atc_usb2_select_host(const struct pd_atc_io *io);
int pd_atc_usb2_stop(const struct pd_atc_io *io);
#ifdef __cplusplus
}
#endif
#endif
