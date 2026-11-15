/* SPDX-License-Identifier: MIT */
/* Copyright The Asahi Linux Contributors. See NOTICE. */
#ifndef PD_CIO_TUNABLES_H
#define PD_CIO_TUNABLES_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
enum pd_cio_window { PD_CIO_RC,PD_CIO_NHI,PD_CIO_PCIE_ADAPTER };
struct pd_cio_tunable_entry { const void *raw;size_t bytes; };
struct pd_cio_tunables { struct pd_cio_tunable_entry entries[11];bool checked; };
struct pd_cio_tunable_io {
    void *ctx;
    const void *(*getprop)(void *,const char *path,const char *name,size_t *);
    uint32_t (*read)(void *,enum pd_cio_window,uint32_t);
    void (*write)(void *,enum pd_cio_window,uint32_t,uint32_t);
};
/* Runtime ADT 24-byte local records, not ATC's 12-byte compact format.
 * Native getter must retain immutable bytes through all applications.
 * T8103/J293 port1 only. All eleven groups are preflighted before any write.
 * Return 0 or positive errno; external ACIO/PHY serialization required. */
int pd_cio_tunables_load(struct pd_cio_tunables *,const struct pd_cio_tunable_io *);
int pd_cio_tunables_apply(const struct pd_cio_tunables *,const struct pd_cio_tunable_io *,enum pd_cio_window);
#ifdef __cplusplus
}
#endif
#endif
