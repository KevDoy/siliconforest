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

/*
 * probe.c, DCP mode (pdrtkitprobe=3), after the caller has seen CPU_CONTROL.RUN set on
 * /arm-io/dcp: apple_rtkit_init() and apple_rtkit_wake() as Linux's dcp_comp_bind() does
 * them, with every received mailbox message traced. No endpoint >= 0x20 is started, no
 * buffer request is answered, nothing is shut down or put to sleep. Returns what
 * apple_rtkit_wake() returned, or the error of an earlier step.
 */
int pd_rtkitprobe_dcp_wake(void);

#ifdef __cplusplus
}
#endif

#endif
