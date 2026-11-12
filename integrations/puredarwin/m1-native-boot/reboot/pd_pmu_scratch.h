/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The boot-state bytes of the Apple SPMI PMU ("sera" on T8103) and the polled SPMI
 * controller access they need. Plain C with no kernel dependency: the kext supplies the
 * register accessors, the host test supplies a model of the controller.
 * Provenance: see pd_pmu_scratch.c.
 */
#ifndef _PD_PMU_SCRATCH_H
#define _PD_PMU_SCRATCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Linux t8103.dtsi, nub_spmi / pmic1, and this machine's device tree
 * (/arm-io/nub-spmi reg 0, spmi-pmu reg and info-leg_scrpad). */
#define PD_T8103_NUB_SPMI_PHYS  0x23d0d9300ULL
#define PD_T8103_PMU_SID        0xf
#define PD_T8103_LEG_SCRPAD     0x9f00

/* offsets from the legacy scratchpad (Linux nvmem-layout of pmic1; m1n1 pmu.py: +2) */
#define PD_PMU_BOOT_STAGE       0x01    /* boot_stage: one byte */
#define PD_PMU_ERROR_COUNTS     0x02    /* boot_error_count: bits 0..3; panic_count: bits 4..7 */
#define PD_PMU_BOOT_ERROR_STAGE 0x03    /* boot_error_stage: one byte (named in the dtsi, unused by Linux) */
#define PD_PMU_SHUTDOWN_FLAG    0x0f    /* shutdown_flag: bit 3 */
#define PD_PMU_SHUTDOWN_BIT     0x08

/* Linux macsmc-reboot.c enum boot_stage */
#define PD_BOOT_STAGE_SHUTDOWN          0x00    /* clean shutdown */
#define PD_BOOT_STAGE_IBOOT_DONE        0x2f    /* last stage of the boot loader */
#define PD_BOOT_STAGE_KERNEL_STARTED    0x30    /* normal OS booting */

struct pd_spmi_io {
	void *ctx;
	uint32_t (*read32)(void *ctx, unsigned offset);
	void (*write32)(void *ctx, unsigned offset, uint32_t value);
	void (*delay_us)(void *ctx, unsigned us);
};

enum {
	PD_SPMI_OK = 0,
	PD_SPMI_BAD_ARGUMENT = -1,
	PD_SPMI_RX_STALE = -3,          /* the reply FIFO would not drain */
	PD_SPMI_TIMEOUT = -4,           /* no reply within 50 ms */
	PD_SPMI_WRONG_REPLY = -5,       /* a reply to another command or slave */
	PD_SPMI_SHORT_REPLY = -6,
	PD_SPMI_NOT_ACKED = -7,
	PD_SPMI_PARITY = -8,
};

/* Extended register read/write long: 16-bit register address, 1..8 bytes. */
int pd_spmi_ext_readl(const struct pd_spmi_io *io, uint8_t sid, uint16_t addr, uint8_t *buf, unsigned len);
int pd_spmi_ext_writel(const struct pd_spmi_io *io, uint8_t sid, uint16_t addr, const uint8_t *buf, unsigned len);

struct pd_pmu {
	const struct pd_spmi_io *io;
	uint8_t sid;
	uint16_t scrpad;
	uint16_t pm_setting;            /* register address, 0 when unknown: read for the log only */
};

struct pd_pmu_state {
	uint8_t boot_stage;
	uint8_t boot_error_count;
	uint8_t panic_count;
	uint8_t boot_error_stage;
	uint8_t shutdown_flag;
	uint8_t pm_setting;
	uint8_t have_pm_setting;
};

/* Reads only. */
int pd_pmu_read_state(const struct pd_pmu *pmu, struct pd_pmu_state *state);

/*
 * What Linux macsmc_reboot_probe() writes: boot_stage = 0x30, then, only when one of the
 * two counts is not zero, panic_count = 0 and boot_error_count = 0 (each a
 * read-modify-write of its four bits, as the NVMEM cell writes are). `state` is what
 * pd_pmu_read_state() returned. Returns the first SPMI error, having attempted all writes.
 */
int pd_pmu_kernel_started(const struct pd_pmu *pmu, const struct pd_pmu_state *state);

/*
 * What Linux macsmc_reboot_notify() writes before a restart or a power-off:
 * boot_stage = 0 and shutdown_flag = 0 (restart) or 1 (power-off).
 */
int pd_pmu_prepare_halt(const struct pd_pmu *pmu, int poweroff);

#ifdef __cplusplus
}
#endif

#endif
