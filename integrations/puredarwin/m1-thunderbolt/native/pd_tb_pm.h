/* SPDX-License-Identifier: MIT */
#ifndef PD_TB_PM_H
#define PD_TB_PM_H
#include "pd_tb_native_abi.h"
#include <stdint.h>
/* Logical device state while the native owner holds the actual cable power
 * lease. Caller serializes, checks real power first, and supplies publication
 * state/time. This never powers hardware or acknowledges a physical suspend. */
struct pd_tb_pm_state {
 uint32_t usage,disabled;
 int delay;uint64_t lastBusy;
 bool active,allowed,noCallbacks,autosuspend,wakeCapable,wakeEnabled;
};
#ifdef __cplusplus
extern "C" {
#endif
void pd_tb_pm_init(struct pd_tb_pm_state *);
int pd_tb_pm_update(struct pd_tb_pm_state *,enum pd_tb_pm_op,int,bool published,uint64_t now);
#ifdef __cplusplus
}
#endif
#endif
