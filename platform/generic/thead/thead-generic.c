/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Authors:
 *   Inochi Amaoto <inochiama@outlook.com>
 *
 */

#include <platform_override.h>
#include <thead/c9xx_errata.h>
#include <thead/c9xx_pmu.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <sbi_utils/fdt/fdt_helper.h>

/* xuantie CSRS registers */
#define CSR_SMPEN			0x7f3
#define CSR_MTEE			0x7f4
#define CSR_MCOR			0x7c2
#define CSR_MHCR			0x7c1
#define CSR_MCCR2			0x7c3
#define CSR_MHINT			0x7c5
#define CSR_MHINT2			0x7cc
#define CSR_MHINT3			0x7cd
#define CSR_MHINT4			0x7ce
#define CSR_MXSTATUS			0x7c0
#define CSR_MSMPR			0x7f3


struct thead_generic_quirks {
	u64	errata;
};

#define NR_CSR 16
static unsigned long csr_val[NR_CSR];

/*
 * cold_boot(core 0) read/save the CSRs
 * warm_boot(other cores0 write the CSRs 
 */
static void init_csrs(bool cold_boot) 
{
	int i = 0;
	if (cold_boot) {
		csr_val[i++] = csr_read(CSR_SMPEN);
		csr_val[i++] = csr_read(CSR_MXSTATUS);
		csr_val[i++] = csr_read(CSR_MHCR);
		csr_val[i++] = csr_read(CSR_MCOR);
		csr_val[i++] = csr_read(CSR_MCCR2);
		csr_val[i++] = csr_read(CSR_MHINT);
		csr_val[i++] = csr_read(CSR_MHINT2);
		csr_val[i++] = csr_read(CSR_MHINT4);
	} else {
		csr_write(CSR_SMPEN, csr_val[i++]);
		csr_write(CSR_MXSTATUS, csr_val[i++]);
		csr_write(CSR_MHCR, csr_val[i++]);
		csr_write(CSR_MCOR, csr_val[i++]);
		csr_write(CSR_MCCR2, csr_val[i++]);
		csr_write(CSR_MHINT, csr_val[i++]);
		csr_write(CSR_MHINT2, csr_val[i++]);
		csr_write(CSR_MHINT4, csr_val[i++]);
	}
}

static bool thead_generic_cold_boot_allowed(u32 hartid, const struct fdt_match *match)
{
	struct thead_generic_quirks *quirks = (void *)match->data;
	bool cold_boot;

	if (quirks->errata & THEAD_QUIRK_ERRATA_TLB_FLUSH)
		thead_register_tlb_flush_trap_handler();

	cold_boot = (hartid == 0);
	init_csrs(cold_boot);

	return cold_boot;
}

static int thead_generic_extensions_init(const struct fdt_match *match,
					 struct sbi_hart_features *hfeatures)
{
	struct thead_generic_quirks *quirks = (void *)match->data;

	if (quirks->errata & THEAD_QUIRK_ERRATA_THEAD_PMU)
		thead_c9xx_register_pmu_device();

	return 0;
}

static struct thead_generic_quirks thead_th1520_quirks = {
	.errata = THEAD_QUIRK_ERRATA_TLB_FLUSH | THEAD_QUIRK_ERRATA_THEAD_PMU,
};

static struct thead_generic_quirks canaan_k230_quirks = {
	.errata = THEAD_QUIRK_ERRATA_THEAD_PMU,
};

static struct thead_generic_quirks sophgo_cv1800_quirks = {
	.errata = THEAD_QUIRK_ERRATA_THEAD_PMU,
};

static const struct fdt_match thead_generic_match[] = {
	{ .compatible = "sophgo,cv1800b", .data = &sophgo_cv1800_quirks },
	{ .compatible = "sophgo,cv1812h", .data = &sophgo_cv1800_quirks },
	{ .compatible = "sophgo,sg2000", .data = &sophgo_cv1800_quirks },
	{ .compatible = "sophgo,sg2002", .data = &sophgo_cv1800_quirks },
	{ .compatible = "thead,th1520", .data = &thead_th1520_quirks },
	{ .compatible = "canaan,kendryte-k230", .data = &canaan_k230_quirks },
	{ },
};

const struct platform_override thead_generic = {
	.match_table		= thead_generic_match,
	.cold_boot_allowed 	= thead_generic_cold_boot_allowed,
	.extensions_init	= thead_generic_extensions_init,
};
