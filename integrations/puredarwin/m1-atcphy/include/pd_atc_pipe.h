/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/* Copyright (C) The Asahi Linux Contributors; author Sven Peter. */
#ifndef PD_ATC_PIPE_H
#define PD_ATC_PIPE_H
#include "pd_atc_modes.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Externally serialized with mode/rate programming. Initialize only after
 * native reset_assert has established dummy, never from an assumed idle. */
struct pd_atc_pipe_state { enum pd_atc_pipe backend; bool dummy_pending; };
/* T8103's qualified USB3 workaround skips host BIST (atc.c). Caller has
 * already powered/calibrated the mode and initialized DWC3's host role. */
int pd_atc_pipe_route(const struct pd_atc_io *,struct pd_atc_pipe_state *,enum pd_atc_pipe);
/* stopped=true requires actual controller halt/reset and joined producers.
 * A stopped PIPE may lack a lock clock. A live USB3 handoff must ACK; if
 * completion fails after moving the mux, retain ownership and retry it. */
int pd_atc_pipe_park(const struct pd_atc_io *,struct pd_atc_pipe_state *,bool stopped);
/* Enable the dummy-PHY bit from Linux's m1n1-established baseline,
 * preserving all other PIPE fields, without releasing DWC3
 * reset. acio_off must be actual native power-state proof, held through
 * this exclusive broker transition; old DWC3 producers must be joined.
 * The function separately verifies actual dummy/ACK/reset/clamp state.
 * No direct host or NHI client may be published before it succeeds. */
int pd_atc_pipe_bootstrap_dummy(const struct pd_atc_io *,const struct pd_atc_pipe_state *,int acio_off);
#ifdef __cplusplus
}
#endif
#endif
