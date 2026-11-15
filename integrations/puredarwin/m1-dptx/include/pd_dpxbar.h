/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (C) Asahi Linux Contributors; Author Sven Peter. */
#ifndef PD_DPXBAR_H
#define PD_DPXBAR_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum pd_dpxbar_output { PD_DPXBAR_PHY,PD_DPXBAR_DPIN0,PD_DPXBAR_DPIN1 };
#define PD_DPXBAR_DISCONNECT (-2)
struct pd_dpxbar_io {
    void *ctx;
    uint32_t (*read)(void *,uint32_t);
    void (*write)(void *,uint32_t,uint32_t);
    void (*delay_us)(void *,unsigned);
    void (*gates_warn)(void *,uint32_t wr,uint32_t rd,uint32_t out);
};
struct pd_dpxbar {
    struct pd_dpxbar_io io;
    int selected_dispext[3];
    uintptr_t regs; // offset origin for the direct register-program adapter
    int lock; // upstream lock site marker; native caller owns serialization
    bool initialized;
};
/* Zero-initialize the state before its first init. Native callers manage
 * mux leases/refcounts; select is the hardware .set program.
 * T8103, physical port1's xbar at 0x50304c000. Caller validates/powers the
 * resource, drains any old streams, and serializes with ATC/rate/DP-IN
 * ownership. Init does not establish that hardware is idle. Return negative
 * errno. Programming success is not link/clock/HPD/training evidence. */
int pd_dpxbar_init(struct pd_dpxbar *,const struct pd_dpxbar_io *);
int pd_dpxbar_select(struct pd_dpxbar *,enum pd_dpxbar_output,int input);
int pd_dpxbar_preselect(struct pd_dpxbar *,enum pd_dpxbar_output,int input);
int pd_dpxbar_link(struct pd_dpxbar *,enum pd_dpxbar_output,bool up);
#ifdef __cplusplus
}
#endif
#endif
