/* SPDX-License-Identifier: GPL-2.0 */
/* T8103 PCIe-C sequence adapted from Linux pcie-apple.c.
 * Copyright (C) 2021 Alyssa Rosenzweig <alyssa@rosenzweig.io>
 * Copyright (C) 2021 Google LLC
 * Copyright (C) 2021 Corellium LLC
 * Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 */
#ifndef PD_PCIEC_PORT_H
#define PD_PCIEC_PORT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum pd_pciec_window { PD_PCIEC_CONFIG, PD_PCIEC_PORT, PD_PCIEC_DEBUG,
                       PD_PCIEC_FABRIC, PD_PCIEC_OE };
struct pd_pciec_io {
    void *ctx;
    /* Native adapter must establish the ACIO/ATC PCIe power/reset lease,
     * reject access after revocation and use ordered nonposted root MMIO.
     * Endpoint BARs require posted mappings on T8103. Caller serializes.
     * I/O and tune callbacks return 0 or a negative errno. */
    int (*read)(void *, enum pd_pciec_window, uint32_t offset, uint32_t *);
    int (*write)(void *, enum pd_pciec_window, uint32_t offset, uint32_t);
    int (*tune)(void *, enum pd_pciec_window);
    void (*delay_us)(void *, unsigned);
};
/* Cold init ends before tunnel reset release and LTSSM start. No bridge,
 * PCI device, DMA mapper or IRQ is published by these functions. A failure
 * requires the native owner to retain power until hardware drain is proved. */
int pd_pciec_cold_init(const struct pd_pciec_io *);
/* Called only after the real connection manager activated its PCIe tunnel;
 * native bridge/DART/MSI setup must precede endpoint-driver admission. */
int pd_pciec_link_start(const struct pd_pciec_io *);
/* Published T8103 Linux SoC constants, not runtime/private calibration.
 * Caller validates real windows/power and serializes. No ADT bytes accepted.
 * This function supplies the tune callback; it never calls io->tune. */
int pd_pciec_t8103_tune(const struct pd_pciec_io *,enum pd_pciec_window);
#ifdef __cplusplus
}
#endif
#endif
