/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_pmu.h>
/* xuantie CSRS registers */
#define CSR_SMPEN			0x7f3
#define CSR_MTEE			0x7f4
#define CSR_MCOR			0x7c2
#define CSR_MHCR			0x7c1
#define CSR_MCCR2			0x7c3
#define CSR_MHINT			0x7c5
#define CSR_MCOUNTERWEN 	0x7c9
#define CSR_MCOUNTEROF		0x7cb
#define CSR_MHINT2			0x7cc
#define CSR_MHINT3			0x7cd
#define CSR_MHINT4			0x7ce
#define CSR_MXSTATUS		0x7c0
#define THEAD_C9XX_CSR_MHPMEVENT0   0x7e0
#define THEAD_C9XX_CSR_MHPMEVENT2   0x7e1
#define CSR_MSMPR			0x7f3
#define CSR_MHPMEVENT0		0x7e0

#define MARCHID_CXXX_BASE 0x8000000000000000
#define MARCHID_C908 (MARCHID_CXXX_BASE + 0x9140d00)
#define MARCHID_C920 (MARCHID_CXXX_BASE + 0x90c0d00)

static void init_csrs(void)
{
	unsigned long marchid = csr_read(CSR_MARCHID);

	if (marchid == MARCHID_C908) {
		csr_write(CSR_MHINT, 0x212A10C);// Enabled the broadcast configuration
		csr_set(CSR_MHINT2, 1ul<<33); //bit 33,When the CPU hangs, it is possible to obtain the CPU's internal context register through jtag
		csr_write(CSR_MCOR, 0x70013); //Clear L1 cache & BTB & BHT ...
		csr_write(CSR_MCCR2, 0xA2490008);
		csr_write(CSR_SMPEN, 0x1);
		csr_write(CSR_MHCR, 0x10011FF);   //MHCR must be set after MHINT MCCR2 SMPEN
		csr_write(CSR_MXSTATUS, 0x438000);
		csr_set(CSR_MHINT4, 1<<13); // enabled L2 cache free write to reduce L2 miss latency

		//bit7,28 bit enable cpu wirte-evict function o enable cpu to write clean data to LLC(L3)
		//csr_set(CSR_MHINT4, 1<<7);
		//csr_set(CSR_MHINT4, 1<<28);
	} else if (marchid == MARCHID_C920) {
		csr_write(CSR_MHINT, 0x316A32C);
		csr_write(CSR_MHINT2, 0x180 | (1ul<<44)); //bit44,When the CPU hangs, it is possible to obtain the CPU's internal context register through jtag
		csr_write(CSR_MCOR, 0x70013);  //Clear L1 cache & BTB & BHT ...
		csr_write(CSR_MCCR2, 0xE2490009);
		csr_write(CSR_SMPEN, 0x1);
		csr_write(CSR_MHCR, 0x11FF);   //MHCR must be set after MHINT MCCR2 SMPEN
		csr_write(CSR_MXSTATUS, 0x438000);
		csr_set(CSR_MHINT4, 1<<13); // enabled L2 cache free write to reduce L2 miss latency

		//bit7,28 bit enable cpu wirte-evict function o enable cpu to write clean data to LLC(L3)
		//csr_set(CSR_MHINT4, 1<<7);
		//csr_set(CSR_MHINT4, 1<<28);
	}
}

static bool zhihe_p100_cold_boot_allowed(u32 hartid, const struct fdt_match *match)
{
	if (hartid == 0)
		return true;

	init_csrs();

	return false;
}

/*
 * if use S-mode register directly instead of stimecmp(CSR),
 * expect bit ENVCFG_STCE is 0
 */
static int zhihe_p100_final_init(bool cold_boot, void *fdt,
				   const struct fdt_match *match)
{
	uint64_t menvcfg_val = csr_read(CSR_MENVCFG);
	menvcfg_val &= ~(ENVCFG_STCE);
	csr_write(CSR_MENVCFG, menvcfg_val);

	return 0;
}

/*********************** pmu extention ***********************/
static void zhihe_p100_pmu_ctr_enable_irq(uint32_t idx)
{
	#define CASE_C_OVF(idx) case idx:                 \
							csr_clear(mhpmevent##idx, 0x8000000000000000);  \
							break;
	switch(idx) {
	case 0:
		csr_clear(THEAD_C9XX_CSR_MHPMEVENT0, 0x8000000000000000);
		break;
	case 2:
		csr_clear(THEAD_C9XX_CSR_MHPMEVENT2, 0x8000000000000000);
		break;
	CASE_C_OVF(3)
	CASE_C_OVF(4)
	CASE_C_OVF(5)
	CASE_C_OVF(6)
	CASE_C_OVF(7)
	CASE_C_OVF(8)
	CASE_C_OVF(9)
	CASE_C_OVF(10)
	CASE_C_OVF(11)
	CASE_C_OVF(12)
	CASE_C_OVF(13)
	CASE_C_OVF(14)
	CASE_C_OVF(15)
	CASE_C_OVF(16)
	CASE_C_OVF(17)
	CASE_C_OVF(18)
	default:
		break;
	}
}

static const struct sbi_pmu_device zhihe_p100_pmu_device = {
	.name = "thead,c908.c920v2-pmu",
	.hw_counter_enable_irq = zhihe_p100_pmu_ctr_enable_irq,
};


static int zhihe_p100_extensions_init(const struct fdt_match *match,
					 struct sbi_hart_features *hfeatures)
{
	// all cpu should initial PMU
	// delegate PMU interrupt into S mode
	csr_set(CSR_MIDELEG, 1 << 13);
	// enable PMU interrupt
	csr_set(CSR_SIE, 1 << 13);
	// enable cycle, insn overflow interrupt
	csr_set(CSR_MXSTATUS, 1 << 8);
	// set CSR_MCOUNTEREN to enable scountovf
	csr_write(CSR_MCOUNTERWEN, 0xffffffff);

	sbi_pmu_set_device(&zhihe_p100_pmu_device);
	return 0;
}

static const struct fdt_match zhihe_p100_match[] = {
	{ .compatible = "zhihe,a210" },
	{ .compatible = "zhihe,p100" },
	{ },
};

const struct platform_override zhihe_p100 = {
	.match_table = zhihe_p100_match,
	.cold_boot_allowed = zhihe_p100_cold_boot_allowed,
	.final_init = zhihe_p100_final_init,
	.extensions_init	= zhihe_p100_extensions_init,
};
