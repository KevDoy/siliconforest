// SPDX-License-Identifier: GPL-2.0
/*
 * Polled access to the Apple SPMI controller (generation 1 register layout, T8103
 * /arm-io/nub-spmi) and the boot-state bytes of the PMU behind it.
 *
 * Ported from Linux (Omacom linux-aurora integration/aurora 6a08a08e):
 *   drivers/spmi/spmi-apple-controller.c (GPL-2.0, Copyright The Asahi Linux Contributors;
 *     apple_spmi_pack_cmd, spmi_raw_cmd without the interrupt path, the gen1 offsets),
 *   drivers/base/regmap/regmap-spmi.c (regmap_spmi_ext_read / _gather_write: registers
 *     above 0xff go by "extended register read/write long", at most 8 bytes),
 *   drivers/power/reset/macsmc-reboot.c (GPL-2.0-only OR MIT; what is written when),
 *   arch/arm64/boot/dts/apple/t8103.dtsi (pmic1 nvmem-layout: which byte and bits),
 * with the stale-FIFO and reply-identity checks of m1n1 src/spmi.c raw_command() (MIT).
 */
#include "pd_pmu_scratch.h"

#define SPMI_STATUS_REG         0x00
#define SPMI_CMD_REG            0x04
#define SPMI_RSP_REG            0x08

#define SPMI_STATUS_RX_EMPTY    (1u << 24)

#define SPMI_CMD_ACTIVE         (1u << 15)

#define SPMI_REPLY_PARITY(r)    ((r) >> 16)
#define SPMI_REPLY_ACK          (1u << 15)
#define SPMI_REPLY_SLAVE(r)     (((r) >> 8) & 0x7f)
#define SPMI_REPLY_CMD(r)       ((r) & 0xff)

#define SPMI_CMD_EXT_WRITEL     0x30
#define SPMI_CMD_EXT_READL      0x38

#define POLL_US                 10
#define TIMEOUT_US              50000           /* Linux REG_POLL_TIMEOUT_US */

static int
rx_empty(const struct pd_spmi_io *io)
{
	return (io->read32(io->ctx, SPMI_STATUS_REG) & SPMI_STATUS_RX_EMPTY) != 0;
}

static int
raw_cmd(const struct pd_spmi_io *io, uint8_t opc, uint8_t sid, uint16_t param,
    const uint8_t *wr, unsigned len_wr, uint8_t *rd, unsigned len_rd)
{
	uint32_t reply, word;
	unsigned i, j, waited;

	if (io == 0 || sid > 0xf || len_wr > 8 || len_rd > 8)
		return PD_SPMI_BAD_ARGUMENT;

	/* m1n1: never take a word that was already in the reply FIFO for this command's reply */
	for (i = 0; !rx_empty(io); i++) {
		if (i >= 64)
			return PD_SPMI_RX_STALE;
		(void)io->read32(io->ctx, SPMI_RSP_REG);
	}

	/* apple_spmi_pack_cmd() */
	io->write32(io->ctx, SPMI_CMD_REG, (uint32_t)opc | (uint32_t)sid << 8 | (uint32_t)param << 16 | SPMI_CMD_ACTIVE);
	for (i = 0; i < len_wr;) {
		word = 0;
		for (j = 0; j < 4 && i < len_wr; j++)
			word |= (uint32_t)wr[i++] << (8 * j);
		io->write32(io->ctx, SPMI_CMD_REG, word);
	}

	for (waited = 0; rx_empty(io); waited += POLL_US) {
		if (waited >= TIMEOUT_US)
			return PD_SPMI_TIMEOUT;
		io->delay_us(io->ctx, POLL_US);
	}
	reply = io->read32(io->ctx, SPMI_RSP_REG);
	if (SPMI_REPLY_CMD(reply) != opc || SPMI_REPLY_SLAVE(reply) != sid)
		return PD_SPMI_WRONG_REPLY;

	for (i = 0; i < len_rd;) {
		if (rx_empty(io))
			return PD_SPMI_SHORT_REPLY;
		word = io->read32(io->ctx, SPMI_RSP_REG);
		for (j = 0; j < 4 && i < len_rd; j++)
			rd[i++] = (uint8_t)(word >> (8 * j));
	}

	if (len_rd == 0 && !(reply & SPMI_REPLY_ACK))
		return PD_SPMI_NOT_ACKED;
	if (~SPMI_REPLY_PARITY(reply) & ((1u << len_rd) - 1))
		return PD_SPMI_PARITY;
	return PD_SPMI_OK;
}

int
pd_spmi_ext_readl(const struct pd_spmi_io *io, uint8_t sid, uint16_t addr, uint8_t *buf, unsigned len)
{
	if (buf == 0 || len < 1 || len > 8)
		return PD_SPMI_BAD_ARGUMENT;
	return raw_cmd(io, (uint8_t)(SPMI_CMD_EXT_READL | (len - 1)), sid, addr, 0, 0, buf, len);
}

int
pd_spmi_ext_writel(const struct pd_spmi_io *io, uint8_t sid, uint16_t addr, const uint8_t *buf, unsigned len)
{
	if (buf == 0 || len < 1 || len > 8)
		return PD_SPMI_BAD_ARGUMENT;
	return raw_cmd(io, (uint8_t)(SPMI_CMD_EXT_WRITEL | (len - 1)), sid, addr, buf, len, 0, 0);
}

static int
read8(const struct pd_pmu *pmu, uint16_t addr, uint8_t *value)
{
	return pd_spmi_ext_readl(pmu->io, pmu->sid, addr, value, 1);
}

static int
write8(const struct pd_pmu *pmu, uint16_t addr, uint8_t value)
{
	return pd_spmi_ext_writel(pmu->io, pmu->sid, addr, &value, 1);
}

/* An NVMEM cell narrower than its byte: the other bits are read back and kept. */
static int
update8(const struct pd_pmu *pmu, uint16_t addr, uint8_t mask, uint8_t value)
{
	uint8_t old;
	int ret = read8(pmu, addr, &old);

	if (ret != PD_SPMI_OK)
		return ret;
	return write8(pmu, addr, (uint8_t)((old & ~mask) | (value & mask)));
}

int
pd_pmu_read_state(const struct pd_pmu *pmu, struct pd_pmu_state *state)
{
	uint8_t counts = 0, flag = 0;
	int ret;

	if (pmu == 0 || state == 0)
		return PD_SPMI_BAD_ARGUMENT;
	state->have_pm_setting = 0;
	state->pm_setting = 0;
	if ((ret = read8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_BOOT_STAGE), &state->boot_stage)) != PD_SPMI_OK ||
	    (ret = read8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_ERROR_COUNTS), &counts)) != PD_SPMI_OK ||
	    (ret = read8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_BOOT_ERROR_STAGE), &state->boot_error_stage)) != PD_SPMI_OK ||
	    (ret = read8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_SHUTDOWN_FLAG), &flag)) != PD_SPMI_OK)
		return ret;
	state->boot_error_count = counts & 0xf;
	state->panic_count = counts >> 4;
	state->shutdown_flag = (flag & PD_PMU_SHUTDOWN_BIT) != 0;
	if (pmu->pm_setting != 0 && read8(pmu, pmu->pm_setting, &state->pm_setting) == PD_SPMI_OK)
		state->have_pm_setting = 1;
	return PD_SPMI_OK;
}

int
pd_pmu_kernel_started(const struct pd_pmu *pmu, const struct pd_pmu_state *state)
{
	int first, ret;

	if (pmu == 0 || state == 0)
		return PD_SPMI_BAD_ARGUMENT;
	first = write8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_BOOT_STAGE), PD_BOOT_STAGE_KERNEL_STARTED);
	if (state->boot_error_count == 0 && state->panic_count == 0)
		return first;
	/* macsmc_power_init_error_counts(): the panic count first, then the boot errors */
	ret = update8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_ERROR_COUNTS), 0xf0, 0);
	if (first == PD_SPMI_OK)
		first = ret;
	ret = update8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_ERROR_COUNTS), 0x0f, 0);
	if (first == PD_SPMI_OK)
		first = ret;
	return first;
}

int
pd_pmu_prepare_halt(const struct pd_pmu *pmu, int poweroff)
{
	int first, ret;

	if (pmu == 0)
		return PD_SPMI_BAD_ARGUMENT;
	first = write8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_BOOT_STAGE), PD_BOOT_STAGE_SHUTDOWN);
	ret = update8(pmu, (uint16_t)(pmu->scrpad + PD_PMU_SHUTDOWN_FLAG), PD_PMU_SHUTDOWN_BIT,
	    poweroff ? PD_PMU_SHUTDOWN_BIT : 0);
	if (first == PD_SPMI_OK)
		first = ret;
	return first;
}
