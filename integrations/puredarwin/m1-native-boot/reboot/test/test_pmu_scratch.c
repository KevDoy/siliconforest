// SPDX-License-Identifier: GPL-2.0
/*
 * Host test of ../pd_pmu_scratch.c against a model of the generation 1 SPMI controller
 * (register behaviour as Linux spmi-apple-controller.c and m1n1 src/spmi.c use it) with a
 * PMU register file behind it.
 *   cc -Wall -Werror -I.. -o t test_pmu_scratch.c ../pd_pmu_scratch.c && ./t
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pd_pmu_scratch.h"

struct model {
	uint8_t reg[0x10000];
	uint32_t rx[32];
	unsigned rx_count, rx_at;
	/* a command being assembled */
	int pending;
	uint32_t cmd;
	uint8_t data[8];
	unsigned data_have, data_want;
	/* log */
	uint32_t commands[64];
	unsigned ncommands;
	struct { uint16_t addr; uint8_t value; } writes[64];
	unsigned nwrites;
	unsigned delayed_us;
	/* faults */
	int silent, nack, bad_parity, wrong_slave;
	uint8_t answer_sid;
};

static void
model_execute(struct model *m)
{
	uint8_t opc = m->cmd & 0xff, sid = (m->cmd >> 8) & 0xf;
	uint16_t addr = m->cmd >> 16;
	unsigned len = (opc & 7) + 1, i;
	uint32_t reply;

	assert(m->cmd & 0x8000);
	m->commands[m->ncommands++] = m->cmd;
	if (m->silent || sid != m->answer_sid)
		return;
	reply = opc | (uint32_t)(m->wrong_slave ? 0x3 : sid) << 8;
	if ((opc & 0xf8) == 0x38) {
		reply |= (m->bad_parity ? 0u : ((1u << len) - 1)) << 16;
		m->rx[m->rx_count++] = reply;
		for (i = 0; i < len; i += 4) {
			uint32_t word = 0;
			for (unsigned j = 0; j < 4 && i + j < len; j++)
				word |= (uint32_t)m->reg[(uint16_t)(addr + i + j)] << (8 * j);
			m->rx[m->rx_count++] = word;
		}
	} else if ((opc & 0xf8) == 0x30) {
		if (!m->nack) {
			reply |= 0x8000;
			for (i = 0; i < len; i++) {
				m->reg[(uint16_t)(addr + i)] = m->data[i];
				m->writes[m->nwrites].addr = (uint16_t)(addr + i);
				m->writes[m->nwrites++].value = m->data[i];
			}
		}
		m->rx[m->rx_count++] = reply;
	} else {
		assert(!"an opcode this driver must not send");
	}
}

static uint32_t
model_read32(void *ctx, unsigned offset)
{
	struct model *m = ctx;

	if (offset == 0x00)
		return (m->rx_at == m->rx_count ? 1u << 24 : 0) | 1u << 8;
	assert(offset == 0x08 && m->rx_at < m->rx_count);
	return m->rx[m->rx_at++];
}

static void
model_write32(void *ctx, unsigned offset, uint32_t value)
{
	struct model *m = ctx;

	assert(offset == 0x04);         /* only the command register is ever written */
	if (!m->pending) {
		uint8_t opc = value & 0xff;
		m->cmd = value;
		m->data_have = 0;
		m->data_want = (opc & 0xf8) == 0x30 ? (opc & 7) + 1 : 0;
		m->pending = 1;
	} else {
		for (unsigned j = 0; j < 4 && m->data_have < m->data_want; j++)
			m->data[m->data_have++] = (uint8_t)(value >> (8 * j));
	}
	if (m->data_have == m->data_want) {
		m->pending = 0;
		if (m->rx_at == m->rx_count)
			m->rx_at = m->rx_count = 0;
		model_execute(m);
	}
}

static void
model_delay(void *ctx, unsigned us)
{
	((struct model *)ctx)->delayed_us += us;
}

static struct model M;
static const struct pd_spmi_io IO = { &M, model_read32, model_write32, model_delay };
static const struct pd_pmu PMU = { &IO, PD_T8103_PMU_SID, PD_T8103_LEG_SCRPAD, 0xd001 };

static void
fresh(void)
{
	memset(&M, 0, sizeof(M));
	memset(M.reg, 0xa5, sizeof(M.reg));
	M.answer_sid = 0xf;
	M.reg[0x9f01] = 0x2f;
	M.reg[0x9f02] = 0x21;           /* two panics, one boot error */
	M.reg[0x9f03] = 0x30;
	M.reg[0x9f0f] = 0xf7;           /* shutdown flag clear, every other bit set */
	M.reg[0xd001] = 0x05;
}

int
main(void)
{
	struct pd_pmu_state s;
	uint8_t buf[8];

	/* reading: values, command words, no write */
	fresh();
	assert(pd_pmu_read_state(&PMU, &s) == PD_SPMI_OK);
	assert(s.boot_stage == 0x2f && s.boot_error_count == 1 && s.panic_count == 2 && s.boot_error_stage == 0x30);
	assert(s.shutdown_flag == 0 && s.have_pm_setting && s.pm_setting == 0x05);
	assert(M.nwrites == 0 && M.ncommands == 5);
	/* extended register read long, one byte, slave 0xf: opcode 0x38, bit 15, address in 31..16 */
	assert(M.commands[0] == 0x9f018f38 && M.commands[1] == 0x9f028f38 && M.commands[2] == 0x9f038f38);
	assert(M.commands[3] == 0x9f0f8f38 && M.commands[4] == 0xd0018f38);

	/* Linux probe: boot stage 0x30, then the panic count, then the boot error count */
	assert(pd_pmu_kernel_started(&PMU, &s) == PD_SPMI_OK);
	assert(M.nwrites == 3);
	assert(M.writes[0].addr == 0x9f01 && M.writes[0].value == 0x30);
	assert(M.writes[1].addr == 0x9f02 && M.writes[1].value == 0x01);
	assert(M.writes[2].addr == 0x9f02 && M.writes[2].value == 0x00);
	assert(M.reg[0x9f03] == 0x30 && M.reg[0x9f0f] == 0xf7 && M.reg[0x9f00] == 0xa5 && M.reg[0x9f04] == 0xa5);
	/* extended register write long, one byte: opcode 0x30 */
	assert(M.commands[5] == 0x9f018f30);

	/* nothing to clear: the boot stage alone */
	fresh();
	M.reg[0x9f02] = 0;
	assert(pd_pmu_read_state(&PMU, &s) == PD_SPMI_OK && s.boot_error_count == 0 && s.panic_count == 0);
	assert(pd_pmu_kernel_started(&PMU, &s) == PD_SPMI_OK);
	assert(M.nwrites == 1 && M.writes[0].addr == 0x9f01 && M.writes[0].value == 0x30);

	/* restart: clean boot stage, shutdown flag cleared, its neighbours kept */
	fresh();
	M.reg[0x9f0f] = 0xff;
	assert(pd_pmu_prepare_halt(&PMU, 0) == PD_SPMI_OK);
	assert(M.nwrites == 2 && M.writes[0].addr == 0x9f01 && M.writes[0].value == 0x00);
	assert(M.writes[1].addr == 0x9f0f && M.writes[1].value == 0xf7);
	assert(M.reg[0x9f02] == 0x21);  /* the counts are not this path's */
	/* power-off: flag set */
	fresh();
	assert(pd_pmu_prepare_halt(&PMU, 1) == PD_SPMI_OK);
	assert(M.nwrites == 2 && M.writes[1].addr == 0x9f0f && M.writes[1].value == 0xff && M.reg[0x9f01] == 0);

	/* multi-byte transfers are little endian words */
	fresh();
	for (unsigned i = 0; i < 6; i++)
		M.reg[0xd100 + i] = (uint8_t)(0x11 * (i + 1));
	assert(pd_spmi_ext_readl(&IO, 0xf, 0xd100, buf, 6) == PD_SPMI_OK);
	assert(memcmp(buf, "\x11\x22\x33\x44\x55\x66", 6) == 0 && M.commands[0] == 0xd1008f3d);
	assert(pd_spmi_ext_writel(&IO, 0xf, 0x1234, (const uint8_t *)"\1\2\3\4\5", 5) == PD_SPMI_OK);
	assert(memcmp(&M.reg[0x1234], "\1\2\3\4\5", 5) == 0 && M.reg[0x1239] == 0xa5);

	/* faults */
	fresh();
	M.silent = 1;
	assert(pd_spmi_ext_readl(&IO, 0xf, 0x9f02, buf, 1) == PD_SPMI_TIMEOUT && M.delayed_us == 50000);
	assert(pd_pmu_read_state(&PMU, &s) == PD_SPMI_TIMEOUT);
	fresh();
	M.nack = 1;
	assert(pd_pmu_prepare_halt(&PMU, 0) == PD_SPMI_NOT_ACKED && M.nwrites == 0);
	fresh();
	M.bad_parity = 1;
	assert(pd_spmi_ext_readl(&IO, 0xf, 0x9f02, buf, 1) == PD_SPMI_PARITY);
	fresh();
	M.wrong_slave = 1;
	assert(pd_spmi_ext_readl(&IO, 0xf, 0x9f02, buf, 1) == PD_SPMI_WRONG_REPLY);
	/* a failed read of a narrow cell never writes */
	fresh();
	M.bad_parity = 1;
	assert(pd_pmu_prepare_halt(&PMU, 1) == PD_SPMI_PARITY);
	assert(M.nwrites == 1 && M.writes[0].addr == 0x9f01);
	/* words left in the reply FIFO by someone else are dropped, not taken as the reply */
	fresh();
	M.rx[0] = 0xdeadbeef;
	M.rx[1] = 0x12345678;
	M.rx_count = 2;
	assert(pd_spmi_ext_readl(&IO, 0xf, 0x9f01, buf, 1) == PD_SPMI_OK && buf[0] == 0x2f);
	/* arguments */
	assert(pd_spmi_ext_readl(&IO, 0xf, 0, buf, 0) == PD_SPMI_BAD_ARGUMENT);
	assert(pd_spmi_ext_readl(&IO, 0xf, 0, buf, 9) == PD_SPMI_BAD_ARGUMENT);
	assert(pd_spmi_ext_readl(&IO, 0x10, 0, buf, 1) == PD_SPMI_BAD_ARGUMENT);

	printf("pd_pmu_scratch: all checks passed\n");
	return 0;
}
