/* SPDX-License-Identifier: MIT */
#ifndef PD_TB_CM_COMPAT_H
#define PD_TB_CM_COMPAT_H
#include <linux/types.h>
#include <linux/export.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/pci_ids.h>
#include "pd_tb_wire.h"
#define crc32c pd_tb_crc32c
#define cpu_to_be32_array pd_tb_to_be32_array
#define be32_to_cpu_array pd_tb_from_be32_array
#ifndef EXPORT_SYMBOL_FOR_MODULES
#define EXPORT_SYMBOL_FOR_MODULES(symbol,modules) EXPORT_SYMBOL_GPL(symbol)
#endif
#ifndef DECLARE_COMPLETION_ONSTACK
/* LinuxKPI completion contains only its zero-initialized done counter. */
#define DECLARE_COMPLETION_ONSTACK(name) struct completion name = { .done=0 }
#endif
#ifndef MEGA
#define MEGA 1000000UL
#endif
/* Required native binding, intentionally DECLARED, not a success fixture.
 * The core object target is not a linked/attached driver. The bus/power
 * adapter must implement this from actual per-device wake policy. */
bool device_may_wakeup(struct device *dev);
#endif
