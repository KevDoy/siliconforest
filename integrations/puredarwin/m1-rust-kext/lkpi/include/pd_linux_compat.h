/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Linux names the kernel crate reads through `bindings::` that LinuxKPI does not define and
 * that have one obvious value on this platform. Anything needing real code is a helper
 * (lkpi/helpers), anything without an honest equivalent is left missing and the Rust that
 * uses it is compiled out.
 */
#ifndef PD_LINUX_COMPAT_H
#define PD_LINUX_COMPAT_H

#include <linux/types.h>

/* No freezer in LinuxKPI: a freezable sleep is an ordinary one. */
#ifndef TASK_FREEZABLE
#define TASK_FREEZABLE 0
#endif

/* One memory node. */
#ifndef MAX_NUMNODES
#define MAX_NUMNODES 1
#endif
#ifndef NUMA_NO_NODE
#define NUMA_NO_NODE (-1)
#endif

/* include/linux/delay.h: the longest udelay() before mdelay() must be used. */
#ifndef MAX_UDELAY_MS
#define MAX_UDELAY_MS 5
#endif

/*
 * Clock ids. CLOCK_MONOTONIC and CLOCK_REALTIME come from Haiku's time.h (as casts, which
 * bindgen cannot evaluate, hence the constants in pd_bindings_extra.h). The other two do not
 * exist there; the ids are only labels on the Rust side (kernel::time::ClockSource::ID).
 */
#ifndef CLOCK_BOOTTIME
#define CLOCK_BOOTTIME 7
#endif
#ifndef CLOCK_TAI
#define CLOCK_TAI 11
#endif

/* lib/vsprintf.c on Linux declares this for %pA; defined by rust/kernel/print.rs. */
char *rust_fmt_argument(char *buf, char *end, const void *ptr);

#endif
