/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <libfdt.h>
#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_string.h>
#include <thead/c9xx_errata.h>
#include <zhihe/teesmc_opteed.h>

extern struct sbi_platform platform;

/* xuantie CSRS registers */
#define CSR_SMPEN			0x7f3
#define CSR_MTEE			0x7f4
#define CSR_MCOR			0x7c2
#define CSR_MHCR			0x7c1
#define CSR_MCCR2			0x7c3
#define CSR_MHINT			0x7c5
#define CSR_MCOUNTERWEN			0x7c9
#define CSR_MCOUNTEROF			0x7cb
#define CSR_MHINT2			0x7cc
#define CSR_MHINT3			0x7cd
#define CSR_MHINT4			0x7ce
#define CSR_MXSTATUS			0x7c0
#define THEAD_C9XX_CSR_MHPMEVENT0	0x7e0
#define THEAD_C9XX_CSR_MHPMEVENT2	0x7e1
#define CSR_MSMPR			0x7f3
#define CSR_MHPMEVENT0			0x7e0

#define MARCHID_CXXX_BASE 0x8000000000000000
#define MARCHID_C908 (MARCHID_CXXX_BASE + 0x9140d00)
#define MARCHID_C920 (MARCHID_CXXX_BASE + 0x90c0d00)

#define ABI_ENTRY_TYPE_FAST		1
#define ABI_ENTRY_TYPE_YIELD		0
#define FUNCID_TYPE_SHIFT		31
#define FUNCID_TYPE_MASK		0x1
#define GET_ABI_ENTRY_TYPE(id)		(((id) >> FUNCID_TYPE_SHIFT) & \
					 FUNCID_TYPE_MASK)

/* optee os vector table*/
static struct optee_vectors *optee_vector_table = NULL;
static struct sbi_domain *tee_domain = NULL, *ree_domain = NULL;

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

static void zhihe_a210_fw_init(const void *fdt, const struct fdt_match *match)
{
	platform.hart_stack_size = 32768;
}

static bool zhihe_a210_cold_boot_allowed(u32 hartid, const struct fdt_match *match)
{
	thead_register_tlb_flush_trap_handler();

	if (hartid == 0)
		return true;

	init_csrs();

	return false;
}

static int zhihe_a210_cpu_on_process(void)
{
	/* if no optee return SBI OK*/
	if (optee_vector_table == NULL)
		return SBI_OK;
	/* init ree domain hart context，data will be set in switch_to_next_domain_context */
	if (sbi_domain_hart_context_alloc(ree_domain))
		return SBI_ENOMEM;

	sbi_domain_init_hart_context(ree_domain);

	//init tee domain hart context and prepare context data
	if (sbi_domain_hart_context_alloc(tee_domain))
		return SBI_ENOMEM;

	sbi_domain_init_hart_context(tee_domain);
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	scratch->next_addr = (ulong)&optee_vector_table->cpu_on_entry;
	scratch->next_mode = tee_domain->next_mode;
	scratch->next_arg1 = 0;

	/* set current domain to tee domain */
	sbi_update_hartindex_to_domain(current_hartindex(), tee_domain);
	return SBI_OK;
}

static struct sbi_domain *__get_tdomain(void)
{
	struct sbi_domain *dom = NULL;
	sbi_domain_for_each(dom) {
		if (!sbi_strcmp(dom->name, "tee-domain"))
			return dom;
	}

	return NULL;
}

static struct sbi_domain *__get_udomain(void)
{
	struct sbi_domain *dom = NULL;
	sbi_domain_for_each(dom) {
		if (!sbi_strcmp(dom->name, "ree-domain"))
			return dom;
	}

	return NULL;
}

static int zhihe_a210_final_init(bool cold_boot, void *fdt,
				   const struct fdt_match *match)
{
	/* optee power on setup */
	if (!cold_boot)
		return zhihe_a210_cpu_on_process();
	else {
		if (!tee_domain)
			tee_domain = __get_tdomain();
		if (!ree_domain)
			ree_domain = __get_udomain();
	}

	return 0;
}

/*********************** pmu extention ***********************/
static void zhihe_a210_pmu_ctr_enable_irq(uint32_t idx)
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

static const struct sbi_pmu_device zhihe_a210_pmu_device = {
	.name = "thead,c908.c920v2-pmu",
	.hw_counter_enable_irq = zhihe_a210_pmu_ctr_enable_irq,
};


static int zhihe_a210_extensions_init(const struct fdt_match *match,
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

	sbi_pmu_set_device(&zhihe_a210_pmu_device);
	return 0;
}

static int sbi_ecall_tee_domain_enter(unsigned long entry_point)
{
	sbi_domain_context_set_mepc(tee_domain, entry_point);
	sbi_domain_context_enter(tee_domain);
	return 0;
}

static int sbi_ecall_tee_domain_exit(void)
{
	sbi_domain_context_exit();
	return 0;
}

static void zhhie_a210_hsm_finish(void)
{
	u32 hartindex = current_hartindex();
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	sbi_domain_restore_scratch();
	sbi_hart_switch_mode(hartindex, scratch->next_arg1, scratch->next_addr, scratch->next_mode, false);
}

static int zhi_a210_tee_smc_handler(long funcid, struct sbi_trap_regs *regs,
				  struct sbi_ecall_return *out, const struct fdt_match *match)
{
	int ret = -1;
	uint32_t funcid_type;
	struct sbi_domain *dom = sbi_domain_thishart_ptr();

	if (dom == ree_domain) {
		funcid_type = GET_ABI_ENTRY_TYPE((regs->a0));
		out->skip_regs_update = true;

		set_domain_regs(tee_domain, regs);
		sbi_ecall_tee_domain_enter((funcid_type == ABI_ENTRY_TYPE_FAST) ?
					(ulong)&optee_vector_table->fast_smc_entry :
					(ulong)&optee_vector_table->yield_smc_entry);

		return SBI_SUCCESS;
	}

	switch (funcid) {
		case TEESMC_OPTEED_RETURN_ENTRY_DONE:
			optee_vector_table =  (optee_vectors_t *)(regs->a1);
			sbi_domain_context_exit();
			ret = SBI_SUCCESS;
			break;
		/*
		* These function IDs is used only by OP-TEE to indicate it has
		* finished:
		* 1. turning itself on in response to an earlier psci
		*	cpu_on request
		* 2. resuming itself after an earlier psci cpu_suspend
		*	request.
		*/
		case TEESMC_OPTEED_RETURN_ON_DONE:
			out->skip_regs_update = true;
			sbi_ecall_tee_domain_exit();
			zhhie_a210_hsm_finish();
			ret = SBI_SUCCESS;
		case TEESMC_OPTEED_RETURN_RESUME_DONE:
		/*
		* These function IDs is used only by the SP to indicate it has
		* finished:
		* 1. suspending itself after an earlier psci cpu_suspend
		*	request.
		* 2. turning itself off in response to an earlier psci
		*	cpu_off request.
		*/
		case TEESMC_OPTEED_RETURN_OFF_DONE:
		case TEESMC_OPTEED_RETURN_SUSPEND_DONE:
		case TEESMC_OPTEED_RETURN_SYSTEM_OFF_DONE:
		case TEESMC_OPTEED_RETURN_SYSTEM_RESET_DONE:
			//TODO BACK to tee os
			break;
		/*
		* OPTEE is returning from a call or being preempted from a call, in
		* either case execution should resume in the normal world.
		*/
		case TEESMC_OPTEED_RETURN_CALL_DONE:
			//TODO goto to ree os
			out->skip_regs_update = true;
			set_domain_regs(ree_domain, regs);
			sbi_ecall_tee_domain_exit();
			regs->mepc += 4;
			ret = SBI_SUCCESS;

			break;
		/*
		* OPTEE has finished handling a S-EL1 FIQ interrupt. Execution
		* should resume in the normal world.
		*/
		case TEESMC_OPTEED_RETURN_FIQ_DONE:
			//TODO
			break;
		/*
		* OPTEE has finished handling a secure world S-EL1 FIQ interrupt. Execution
		* should resume in the secure world.
		*/
		case TEESMC_OPTEED_RETURN_FIQ2_DONE:
			//TODO
			break;
		default:
			break;
	}
	return ret;
}

static void fdt_restore_cpus_for_domain(void *fdt, struct sbi_domain *dom)
{
	int cpus_offset, cpu_off;
	u32 hartid;
	int rc;

	if (!fdt || !dom)
		return;

	cpus_offset = fdt_path_offset(fdt, "/cpus");
	if (cpus_offset < 0)
		return;

	/* ensure fdt has enough room if we will change properties */
	rc = fdt_open_into(fdt, fdt, fdt_totalsize(fdt) + 32);
	if (rc < 0)
		return;

	fdt_for_each_subnode(cpu_off, fdt, cpus_offset) {
		rc = fdt_parse_hart_id(fdt, cpu_off, &hartid);
		if (rc)
			continue;

		if (fdt_node_is_enabled(fdt, cpu_off))
			continue;

		/* if this hart belongs to the domain 'dom', ensure status is "okay" */
		{
			unsigned long hartindex = sbi_hartid_to_hartindex(hartid);
			if (sbi_hartmask_test_hartindex(hartindex,dom->possible_harts)) {
				/* set "status" = "okay" (create or replace) */
				fdt_setprop_string(fdt, cpu_off, "status", "okay");
			} else {
				/* Optionally leave others as-is (they may be disabled) */
			}
		}
	}
}

static int zhi_a210_fdt_fix_up(void *fdt, const struct fdt_match *match)
{
	/* restore cpu status*/
	fdt_restore_cpus_for_domain(fdt, ree_domain);

	return 0;
}

static const struct fdt_match zhihe_a210_match[] = {
	{ .compatible = "zhihe,a210" },
	{ },
};

const struct platform_override zhihe_a210 = {
	.match_table		= zhihe_a210_match,
	.fw_init		= zhihe_a210_fw_init,
	.cold_boot_allowed 	= zhihe_a210_cold_boot_allowed,
	.final_init		= zhihe_a210_final_init,
	.extensions_init	= zhihe_a210_extensions_init,
	.vendor_ext_provider	= zhi_a210_tee_smc_handler,
	.fdt_fixup		= zhi_a210_fdt_fix_up,
};
