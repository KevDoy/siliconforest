/* SPDX-License-Identifier: GPL-2.0 */
/* PDRtkitProbe: what its IOKit class (PDRtkitProbe.cpp) calls on the LinuxKPI side. */
#ifndef PD_RTKITPROBE_H
#define PD_RTKITPROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/* glue.c: pd_fbsd_init(). 0 when the layer is up. */
int pd_rtkitprobe_layer_up(void);

/*
 * probe.c, mode 2 after the ASC CPU was started: apple_rtkit_init() and
 * apple_rtkit_boot() on /arm-io/gfx-asc as the Asahi driver does them, every
 * step logged. Returns what apple_rtkit_boot() returned (0: IOP and AP power
 * states ON), or the error of an earlier step. The RTKit instance is left
 * alive so that later firmware messages are still acknowledged and a crash is
 * reported.
 */
int pd_rtkitprobe_rtkit_boot(void);

#ifdef __cplusplus
}
#endif

#endif
