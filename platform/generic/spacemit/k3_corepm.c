/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 SpacemiT
 *
 * SpacemiT K3 runtime power management.
 *
 * Ported from the vendor OpenSBI v1.6 tree
 * (platform/generic/spacemit/k3_corepm.c). Implements per-core / per-cluster
 * power voting through the PMU idle-config and APCR vote registers, plus the
 * HSM suspend/resume path with a full M/S/H/VS IMSIC interrupt-file
 * save-restore. These are driven from M-mode register pokes; no RPMI message
 * is exchanged with the RCPU for the actual power transition.
 *
 * The per-hart IMSIC save area lives in the scratch space; its offset is
 * allocated by the platform_override final_init (hart_imisc_save_offset).
 *
 * 1.6 -> master API adaptation: the per-hart scratch is looked up with
 * sbi_hartid_to_scratch() (which converts the physical hartid through the
 * hart-index table) instead of feeding a raw hartid to
 * sbi_hartindex_to_scratch().
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_types.h>
#include <spacemit/k3.h>
#include "k3_corepm.h"

/* IMSIC save area offset in scratch, allocated by the platform final_init */
extern unsigned long hart_imisc_save_offset;

void spacemit_mask_irq(uint32_t hartid)
{
	unsigned int value;

	switch (hartid) {
	case 0:
		value = readl((void *)PMU_CAP_CORE0_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE0_IDLE_CFG);
		break;
	case 1:
		value = readl((void *)PMU_CAP_CORE1_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE1_IDLE_CFG);
		break;
	case 2:
		value = readl((void *)PMU_CAP_CORE2_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE2_IDLE_CFG);
		break;
	case 3:
		value = readl((void *)PMU_CAP_CORE3_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE3_IDLE_CFG);
		break;
	case 4:
		value = readl((void *)PMU_CAP_CORE4_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE4_IDLE_CFG);
		break;
	case 5:
		value = readl((void *)PMU_CAP_CORE5_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE5_IDLE_CFG);
		break;
	case 6:
		value = readl((void *)PMU_CAP_CORE6_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE6_IDLE_CFG);
		break;
	case 7:
		value = readl((void *)PMU_CAP_CORE7_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE7_IDLE_CFG);
		break;
	case 8:
		value = readl((void *)PMU_CAP_CORE8_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE8_IDLE_CFG);
		break;
	case 9:
		value = readl((void *)PMU_CAP_CORE9_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE9_IDLE_CFG);
		break;
	case 10:
		value = readl((void *)PMU_CAP_CORE10_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE10_IDLE_CFG);
		break;
	case 11:
		value = readl((void *)PMU_CAP_CORE11_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE11_IDLE_CFG);
		break;
	case 12:
		value = readl((void *)PMU_CAP_CORE12_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE12_IDLE_CFG);
		break;
	case 13:
		value = readl((void *)PMU_CAP_CORE13_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE13_IDLE_CFG);
		break;
	case 14:
		value = readl((void *)PMU_CAP_CORE14_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE14_IDLE_CFG);
		break;
	case 15:
		value = readl((void *)PMU_CAP_CORE15_IDLE_CFG);
		value |= CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE15_IDLE_CFG);
		break;
	default:
		break;
	}
}

void spacemit_vote_core_apcr(uint32_t hartid)
{
	unsigned int value;

	switch (hartid) {
	case 0:
		value = readl((void *)APCR_CORE0_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE0_VETE_REG);
		break;
	case 1:
		value = readl((void *)APCR_CORE1_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE1_VETE_REG);
		break;
	case 2:
		value = readl((void *)APCR_CORE2_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE2_VETE_REG);
		break;
	case 3:
		value = readl((void *)APCR_CORE3_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE3_VETE_REG);
		break;
	case 4:
		value = readl((void *)APCR_CORE4_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE4_VETE_REG);
		break;
	case 5:
		value = readl((void *)APCR_CORE5_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE5_VETE_REG);
		break;
	case 6:
		value = readl((void *)APCR_CORE6_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE6_VETE_REG);
		break;
	case 7:
		value = readl((void *)APCR_CORE7_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE7_VETE_REG);
		break;
	case 8:
		value = readl((void *)APCR_CORE8_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE8_VETE_REG);
		break;
	case 9:
		value = readl((void *)APCR_CORE9_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE9_VETE_REG);
		break;
	case 10:
		value = readl((void *)APCR_CORE10_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE10_VETE_REG);
		break;
	case 11:
		value = readl((void *)APCR_CORE11_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE11_VETE_REG);
		break;
	case 12:
		value = readl((void *)APCR_CORE12_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE12_VETE_REG);
		break;
	case 13:
		value = readl((void *)APCR_CORE13_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE13_VETE_REG);
		break;
	case 14:
		value = readl((void *)APCR_CORE14_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE14_VETE_REG);
		break;
	case 15:
		value = readl((void *)APCR_CORE15_VETE_REG);
		value |= APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE15_VETE_REG);
		break;
	default:
		break;
	}
}

void spacemit_devote_core_apcr(uint32_t hartid)
{
	unsigned int value;

	switch (hartid) {
	case 0:
		value = readl((void *)APCR_CORE0_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE0_VETE_REG);
		break;
	case 1:
		value = readl((void *)APCR_CORE1_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE1_VETE_REG);
		break;
	case 2:
		value = readl((void *)APCR_CORE2_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE2_VETE_REG);
		break;
	case 3:
		value = readl((void *)APCR_CORE3_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE3_VETE_REG);
		break;
	case 4:
		value = readl((void *)APCR_CORE4_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE4_VETE_REG);
		break;
	case 5:
		value = readl((void *)APCR_CORE5_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE5_VETE_REG);
		break;
	case 6:
		value = readl((void *)APCR_CORE6_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE6_VETE_REG);
		break;
	case 7:
		value = readl((void *)APCR_CORE7_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE7_VETE_REG);
		break;
	case 8:
		value = readl((void *)APCR_CORE8_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE8_VETE_REG);
		break;
	case 9:
		value = readl((void *)APCR_CORE9_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE9_VETE_REG);
		break;
	case 10:
		value = readl((void *)APCR_CORE10_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE10_VETE_REG);
		break;
	case 11:
		value = readl((void *)APCR_CORE11_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE11_VETE_REG);
		break;
	case 12:
		value = readl((void *)APCR_CORE12_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE12_VETE_REG);
		break;
	case 13:
		value = readl((void *)APCR_CORE13_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE13_VETE_REG);
		break;
	case 14:
		value = readl((void *)APCR_CORE14_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE14_VETE_REG);
		break;
	case 15:
		value = readl((void *)APCR_CORE15_VETE_REG);
		value &= ~APCR_COREX_DEFAULT_VATE_VALUE;
		writel(value, (void *)APCR_CORE15_VETE_REG);
		break;
	default:
		break;
	}
}

void spacemit_unmask_irq(uint32_t hartid)
{
	unsigned int value;

	switch (hartid) {
	case 0:
		value = readl((void *)PMU_CAP_CORE0_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE0_IDLE_CFG);
		break;
	case 1:
		value = readl((void *)PMU_CAP_CORE1_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE1_IDLE_CFG);
		break;
	case 2:
		value = readl((void *)PMU_CAP_CORE2_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE2_IDLE_CFG);
		break;
	case 3:
		value = readl((void *)PMU_CAP_CORE3_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE3_IDLE_CFG);
		break;
	case 4:
		value = readl((void *)PMU_CAP_CORE4_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE4_IDLE_CFG);
		break;
	case 5:
		value = readl((void *)PMU_CAP_CORE5_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE5_IDLE_CFG);
		break;
	case 6:
		value = readl((void *)PMU_CAP_CORE6_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE6_IDLE_CFG);
		break;
	case 7:
		value = readl((void *)PMU_CAP_CORE7_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE7_IDLE_CFG);
		break;
	case 8:
		value = readl((void *)PMU_CAP_CORE8_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE8_IDLE_CFG);
		break;
	case 9:
		value = readl((void *)PMU_CAP_CORE9_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE9_IDLE_CFG);
		break;
	case 10:
		value = readl((void *)PMU_CAP_CORE10_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE10_IDLE_CFG);
		break;
	case 11:
		value = readl((void *)PMU_CAP_CORE11_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE11_IDLE_CFG);
		break;
	case 12:
		value = readl((void *)PMU_CAP_CORE12_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE12_IDLE_CFG);
		break;
	case 13:
		value = readl((void *)PMU_CAP_CORE13_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE13_IDLE_CFG);
		break;
	case 14:
		value = readl((void *)PMU_CAP_CORE14_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE14_IDLE_CFG);
		break;
	case 15:
		value = readl((void *)PMU_CAP_CORE15_IDLE_CFG);
		value &= ~CPU_MASK_FI_INTTERUPT;
		writel(value, (void *)PMU_CAP_CORE15_IDLE_CFG);
		break;
	default:
		break;
	}
}

void spacemit_vote_powrdown_core(uint32_t hartid)
{
	unsigned int value;

	switch (hartid) {
	case 0:
		value = readl((void *)PMU_CAP_CORE0_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE0_IDLE_CFG);
		break;
	case 1:
		value = readl((void *)PMU_CAP_CORE1_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE1_IDLE_CFG);
		break;
	case 2:
		value = readl((void *)PMU_CAP_CORE2_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE2_IDLE_CFG);
		break;
	case 3:
		value = readl((void *)PMU_CAP_CORE3_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE3_IDLE_CFG);
		break;
	case 4:
		value = readl((void *)PMU_CAP_CORE4_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE4_IDLE_CFG);
		break;
	case 5:
		value = readl((void *)PMU_CAP_CORE5_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE5_IDLE_CFG);
		break;
	case 6:
		value = readl((void *)PMU_CAP_CORE6_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE6_IDLE_CFG);
		break;
	case 7:
		value = readl((void *)PMU_CAP_CORE7_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE7_IDLE_CFG);
		break;
	case 8:
		value = readl((void *)PMU_CAP_CORE8_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE8_IDLE_CFG);
		break;
	case 9:
		value = readl((void *)PMU_CAP_CORE9_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE9_IDLE_CFG);
		break;
	case 10:
		value = readl((void *)PMU_CAP_CORE10_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE10_IDLE_CFG);
		break;
	case 11:
		value = readl((void *)PMU_CAP_CORE11_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE11_IDLE_CFG);
		break;
	case 12:
		value = readl((void *)PMU_CAP_CORE12_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE12_IDLE_CFG);
		break;
	case 13:
		value = readl((void *)PMU_CAP_CORE13_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE13_IDLE_CFG);
		break;
	case 14:
		value = readl((void *)PMU_CAP_CORE14_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE14_IDLE_CFG);
		break;
	case 15:
		value = readl((void *)PMU_CAP_CORE15_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE15_IDLE_CFG);
		break;
	default:
		break;
	}
}

void spacemit_vote_powrdown_cluster(uint32_t hartid)
{
	unsigned int value;

	switch (hartid) {
	case 0:
		value = readl((void *)PMU_CAP_CORE0_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE0_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG0);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG0);
		break;
	case 1:
		value = readl((void *)PMU_CAP_CORE1_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE1_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG1);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG1);
		break;
	case 2:
		value = readl((void *)PMU_CAP_CORE2_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE2_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG2);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG2);
		break;
	case 3:
		value = readl((void *)PMU_CAP_CORE3_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE3_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG3);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG3);
		break;
	case 4:
		value = readl((void *)PMU_CAP_CORE4_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE4_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG4);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG4);
		break;
	case 5:
		value = readl((void *)PMU_CAP_CORE5_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE5_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG5);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG5);
		break;
	case 6:
		value = readl((void *)PMU_CAP_CORE6_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE6_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG6);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG6);
		break;
	case 7:
		value = readl((void *)PMU_CAP_CORE7_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE7_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG7);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG7);
		break;
	case 8:
		value = readl((void *)PMU_CAP_CORE8_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE8_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG8);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG8);
		break;
	case 9:
		value = readl((void *)PMU_CAP_CORE9_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE9_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG9);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG9);
		break;
	case 10:
		value = readl((void *)PMU_CAP_CORE10_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE10_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG10);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG10);
		break;
	case 11:
		value = readl((void *)PMU_CAP_CORE11_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE11_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG11);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG11);
		break;
	case 12:
		value = readl((void *)PMU_CAP_CORE12_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE12_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG12);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG12);
		break;
	case 13:
		value = readl((void *)PMU_CAP_CORE13_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE13_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG13);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG13);
		break;
	case 14:
		value = readl((void *)PMU_CAP_CORE14_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE14_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG14);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG14);
		break;
	case 15:
		value = readl((void *)PMU_CAP_CORE15_IDLE_CFG);
		value |= CPU_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CAP_CORE15_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG15);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG15);
		break;
	default:
		break;
	}
}

void spacemit_devote_pwrdown_cluster(uint32_t hartid)
{
	unsigned int value;

	switch (hartid) {
	case 0:
		value = readl((void *)PMU_CAP_CORE0_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE0_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG0);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG0);
		break;
	case 1:
		value = readl((void *)PMU_CAP_CORE1_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE1_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG1);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG1);
		break;
	case 2:
		value = readl((void *)PMU_CAP_CORE2_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE2_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG2);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG2);
		break;
	case 3:
		value = readl((void *)PMU_CAP_CORE3_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE3_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG3);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG3);
		break;
	case 4:
		value = readl((void *)PMU_CAP_CORE4_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE4_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG4);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG4);
		break;
	case 5:
		value = readl((void *)PMU_CAP_CORE5_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE5_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG5);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG5);
		break;
	case 6:
		value = readl((void *)PMU_CAP_CORE6_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE6_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG6);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG6);
		break;
	case 7:
		value = readl((void *)PMU_CAP_CORE7_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE7_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG7);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG7);
		break;
	case 8:
		value = readl((void *)PMU_CAP_CORE8_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE8_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG8);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG8);
		break;
	case 9:
		value = readl((void *)PMU_CAP_CORE9_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE9_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG9);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG9);
		break;
	case 10:
		value = readl((void *)PMU_CAP_CORE10_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE10_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG10);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG10);
		break;
	case 11:
		value = readl((void *)PMU_CAP_CORE11_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE11_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG11);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG11);
		break;
	case 12:
		value = readl((void *)PMU_CAP_CORE12_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE12_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG12);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG12);
		break;
	case 13:
		value = readl((void *)PMU_CAP_CORE13_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE13_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG13);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG13);
		break;
	case 14:
		value = readl((void *)PMU_CAP_CORE14_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE14_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG14);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG14);
		break;
	case 15:
		value = readl((void *)PMU_CAP_CORE15_IDLE_CFG);
		value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
		writel(value, (void *)PMU_CAP_CORE15_IDLE_CFG);
		value = readl((void *)PMU_CX_CAPMP_IDLE_CFG15);
		value &= ~CLUSTER_PWR_DOWN_VALUE;
		writel(value, (void *)PMU_CX_CAPMP_IDLE_CFG15);
		break;
	default:
		break;
	}
}

int spacemit_wakeup_core(uint32_t hartid)
{
	switch (hartid) {
	case 0:
		writel((1 << hartid), (void *)PMU_CAP_CORE0_WAKEUP);
		break;
	case 1:
		writel((1 << hartid), (void *)PMU_CAP_CORE1_WAKEUP);
		break;
	case 2:
		writel((1 << hartid), (void *)PMU_CAP_CORE2_WAKEUP);
		break;
	case 3:
		writel((1 << hartid), (void *)PMU_CAP_CORE3_WAKEUP);
		break;
	case 4:
		writel((1 << hartid), (void *)PMU_CAP_CORE4_WAKEUP);
		break;
	case 5:
		writel((1 << hartid), (void *)PMU_CAP_CORE5_WAKEUP);
		break;
	case 6:
		writel((1 << hartid), (void *)PMU_CAP_CORE6_WAKEUP);
		break;
	case 7:
		writel((1 << hartid), (void *)PMU_CAP_CORE7_WAKEUP);
		break;
	case 8:
		writel((1 << hartid), (void *)PMU_CAP_CORE8_WAKEUP);
		break;
	case 9:
		writel((1 << hartid), (void *)PMU_CAP_CORE9_WAKEUP);
		break;
	case 10:
		writel((1 << hartid), (void *)PMU_CAP_CORE10_WAKEUP);
		break;
	case 11:
		writel((1 << hartid), (void *)PMU_CAP_CORE11_WAKEUP);
		break;
	case 12:
		writel((1 << hartid), (void *)PMU_CAP_CORE12_WAKEUP);
		break;
	case 13:
		writel((1 << hartid), (void *)PMU_CAP_CORE13_WAKEUP);
		break;
	case 14:
		writel((1 << hartid), (void *)PMU_CAP_CORE14_WAKEUP);
		break;
	case 15:
		writel((1 << hartid), (void *)PMU_CAP_CORE15_WAKEUP);
		break;
	default:
		break;
	}

	return 0;
}

int __rpmi_hsm_suspend_pre(void)
{
	int ret = 0;
	int retry_count = 5;
	unsigned long local_id;
	struct imsic_config *imsic;
	struct sbi_scratch *rscratch;

	rscratch = sbi_hartid_to_scratch(current_hartid());
	imsic = sbi_scratch_offset_ptr(rscratch, hart_imisc_save_offset);

	/* mask the irq */
	spacemit_mask_irq(current_hartid());

_retry:
	imsic->flags = 0;

	/* query s-mode irq pending */
	local_id = csr_read(CSR_STOPEI);
	local_id >>= TOPEI_ID_SHIFT;
	if (local_id) {
		ret = -1;
		imsic->flags = 1;
		goto exit;
	}

	/* query m-mode irq pending */
	local_id = csr_read(CSR_MTOPEI);
	local_id >>= TOPEI_ID_SHIFT;
	if (local_id) {
		ret = -1;
		imsic->flags = 1;
		goto exit;
	}

	/* a100: query the h-mode irq pending */
	if (current_hartid() < 8) {
		local_id = csr_read(CSR_HGEIP);
		if (local_id) {
			ret = -1;
			imsic->flags = 1;
			goto exit;
		}
	}

	if (--retry_count != 0)
		goto _retry;

exit:
	if (ret == -1) {
		/* keep the system out of low-power and skip the rcpu suspend */
		spacemit_unmask_irq(current_hartid());
	} else {
		/* vote core apcr */
		spacemit_vote_core_apcr(current_hartid());
	}

	return ret;
}

int __rpmi_hsm_suspend(u32 type)
{
	int i, j, k;
	int retry_count = 5;
	unsigned long local_id;
	struct imsic_config *imsic;
	struct sbi_scratch *rscratch;

	rscratch = sbi_hartid_to_scratch(current_hartid());
	imsic = sbi_scratch_offset_ptr(rscratch, hart_imisc_save_offset);

	/* mask the irq */
	spacemit_mask_irq(current_hartid());

_retry:
	imsic->flags = 0;

	/* query s-mode irq pending */
	local_id = csr_read(CSR_STOPEI);
	local_id >>= TOPEI_ID_SHIFT;
	if (local_id) {
		imsic->flags = 1;
		goto exit;
	}

	/* query m-mode irq pending */
	local_id = csr_read(CSR_MTOPEI);
	local_id >>= TOPEI_ID_SHIFT;
	if (local_id) {
		imsic->flags = 1;
		goto exit;
	}

	/* a100: query the h-mode irq pending */
	if (current_hartid() < 8) {
		local_id = csr_read(CSR_HGEIP);
		if (local_id) {
			imsic->flags = 1;
			goto exit;
		}
	}

	if (--retry_count != 0)
		goto _retry;

	/* disable all irq */
	csr_clear(CSR_MIE, MIP_SSIP | MIP_MSIP | MIP_STIP | MIP_MTIP |
			   MIP_SEIP | MIP_MEIP);

	/* no pending, save the interrupt files */
	/* 1. save m-mode */
	csr_write(CSR_MISELECT, IMSIC_EITHRESHOLD);
	imsic->meithreshold = csr_read(CSR_MIREG);

	csr_write(CSR_MISELECT, IMSIC_EIDELIVERY);
	imsic->meidelivery = csr_read(CSR_MIREG);

	for (i = 0, j = IMSIC_FIRST_EIE_REG; i < MAX_IMSIC_EIE_REGISTERS;
	     i += 2, j += 2) {
		csr_write(CSR_MISELECT, j);
		imsic->meie[i] = csr_read(CSR_MIREG);
	}

	/* 2. save s-mode */
	csr_write(CSR_SISELECT, IMSIC_EITHRESHOLD);
	imsic->seithreshold = csr_read(CSR_SIREG);

	csr_write(CSR_SISELECT, IMSIC_EIDELIVERY);
	imsic->seidelivery = csr_read(CSR_SIREG);

	for (i = 0, j = IMSIC_FIRST_EIE_REG; i < MAX_IMSIC_EIE_REGISTERS;
	     i += 2, j += 2) {
		csr_write(CSR_SISELECT, j);
		imsic->seie[i] = csr_read(CSR_SIREG);
	}

	/* 3. a100: save h-mode and the per-guest VS interrupt files */
	if (current_hartid() < 8) {
		imsic->hstatus = csr_read(CSR_HSTATUS);
		imsic->hedeleg = csr_read(CSR_HEDELEG);
		imsic->hideleg = csr_read(CSR_HIDELEG);
		imsic->hie = csr_read(CSR_HIE);
		imsic->hcounteren = csr_read(CSR_HCOUNTEREN);
		imsic->hgeie = csr_read(CSR_HGEIE);
		imsic->henvcfg = csr_read(CSR_HENVCFG);
		imsic->htval = csr_read(CSR_HTVAL);
		imsic->hgatp = csr_read(CSR_HGATP);
		imsic->htimedelta = csr_read(CSR_HTIMEDELTA);

		for (k = 1; k < IMSIC_MAX_VGEN; ++k) {
			/* select virtual guest k */
			unsigned long long s = imsic->hstatus & ~(0x3f << 12);

			s |= (k << 12);
			csr_write(CSR_HSTATUS, s);

			csr_write(CSR_VSISELECT, IMSIC_EITHRESHOLD);
			imsic->hc[k].heithreshold = csr_read(CSR_VSIREG);

			csr_write(CSR_VSISELECT, IMSIC_EIDELIVERY);
			imsic->hc[k].heidelivery = csr_read(CSR_VSIREG);

			for (i = 0, j = IMSIC_FIRST_EIE_REG;
			     i < MAX_IMSIC_EIE_REGISTERS; i += 2, j += 2) {
				csr_write(CSR_VSISELECT, j);
				imsic->hc[k].heie[i] = csr_read(CSR_VSIREG);
			}
		}
	}

	if (current_hartid() == 8) {
		spacemit_vote_powrdown_core(current_hartid());
	} else {
		if (type == (SBI_HSM_SUSP_NON_RET_BIT | SBI_HSM_SUSP_PLAT_BASE))
			spacemit_vote_powrdown_core(current_hartid());
		else
			spacemit_vote_powrdown_cluster(current_hartid());
	}

	/* disable prefetch */
	csi_disable_data_preftch();
	asm volatile ("fence iorw, iorw");
	/* flush dcache all */
	csi_flush_dcache_all();
	asm volatile ("fence iorw, iorw");
	/* disable d cache */
	csi_disable_cache();
	asm volatile ("fence iorw, iorw");
	/* disable core snoop */
	csr_clear(CSR_ML2SETUP,
		  1 << (current_hartid() % PLATFORM_MAX_CPUS_PER_CLUSTER));
	asm volatile ("fence iorw, iorw");

	/* wait for interrupt */
	while (1)
		wfi();

exit:
	spacemit_unmask_irq(current_hartid());

	return 0;
}

void __rpmi_hsm_resume(void)
{
	int i, j, k;
	struct imsic_config *imsic;
	struct sbi_scratch *rscratch;

	rscratch = sbi_hartid_to_scratch(current_hartid());
	imsic = sbi_scratch_offset_ptr(rscratch, hart_imisc_save_offset);

	if (imsic->flags)
		return;

	/* restore the imsic */
	/* 1. restore m-mode */
	csr_write(CSR_MISELECT, IMSIC_EITHRESHOLD);
	csr_write(CSR_MIREG, imsic->meithreshold);

	csr_write(CSR_MISELECT, IMSIC_EIDELIVERY);
	csr_write(CSR_MIREG, imsic->meidelivery);

	for (i = 0, j = IMSIC_FIRST_EIE_REG; i < MAX_IMSIC_EIE_REGISTERS;
	     i += 2, j += 2) {
		csr_write(CSR_MISELECT, j);
		csr_write(CSR_MIREG, imsic->meie[i]);
	}

	/* 2. restore s-mode */
	csr_write(CSR_SISELECT, IMSIC_EITHRESHOLD);
	csr_write(CSR_SIREG, imsic->seithreshold);

	csr_write(CSR_SISELECT, IMSIC_EIDELIVERY);
	csr_write(CSR_SIREG, imsic->seidelivery);

	for (i = 0, j = IMSIC_FIRST_EIE_REG; i < MAX_IMSIC_EIE_REGISTERS;
	     i += 2, j += 2) {
		csr_write(CSR_SISELECT, j);
		csr_write(CSR_SIREG, imsic->seie[i]);
	}

	/* 3. a100: restore h-mode and per-guest VS interrupt files */
	if (current_hartid() < 8) {
		csr_write(CSR_HEDELEG, imsic->hedeleg);
		csr_write(CSR_HIDELEG, imsic->hideleg);
		csr_write(CSR_HIE, imsic->hie);
		csr_write(CSR_HCOUNTEREN, imsic->hcounteren);
		csr_write(CSR_HGEIE, imsic->hgeie);
		csr_write(CSR_HENVCFG, imsic->henvcfg);
		csr_write(CSR_HTVAL, imsic->htval);
		csr_write(CSR_HGATP, imsic->hgatp);
		csr_write(CSR_HTIMEDELTA, imsic->htimedelta);

		for (k = 1; k < IMSIC_MAX_VGEN; ++k) {
			/* select virtual guest k */
			unsigned long long s = imsic->hstatus & ~(0x3f << 12);

			s |= (k << 12);
			csr_write(CSR_HSTATUS, s);

			csr_write(CSR_VSISELECT, IMSIC_EITHRESHOLD);
			csr_write(CSR_VSIREG, imsic->hc[k].heithreshold);

			csr_write(CSR_VSISELECT, IMSIC_EIDELIVERY);
			csr_write(CSR_VSIREG, imsic->hc[k].heidelivery);

			for (i = 0, j = IMSIC_FIRST_EIE_REG;
			     i < MAX_IMSIC_EIE_REGISTERS; i += 2, j += 2) {
				csr_write(CSR_VSISELECT, j);
				csr_write(CSR_VSIREG, imsic->hc[k].heie[i]);
			}
		}

		csr_write(CSR_HSTATUS, imsic->hstatus);
	}
}

void __rpmi_shutdown_process(void)
{
	/* mask the irq */
	spacemit_mask_irq(current_hartid());

	if (current_hartid() == 8)
		spacemit_vote_powrdown_core(current_hartid());
	else
		spacemit_vote_powrdown_cluster(current_hartid());

	/* vote core apcr */
	spacemit_vote_core_apcr(current_hartid());

	/* disable local timer */
	csr_write(CSR_STIMECMP, 0xffffffffffffffffUL);
	/* disable all irq */
	csr_clear(CSR_MIE, MIP_SSIP | MIP_MSIP | MIP_STIP | MIP_MTIP |
			   MIP_SEIP | MIP_MEIP);
	/* disable prefetch */
	csi_disable_data_preftch();
	asm volatile ("fence iorw, iorw");
	/* flush dcache all */
	csi_flush_dcache_all();
	asm volatile ("fence iorw, iorw");
	/* disable i/d cache */
	csi_disable_cache();
	asm volatile ("fence iorw, iorw");
	/* disable core snoop */
	csr_clear(CSR_ML2SETUP,
		  1 << (current_hartid() % PLATFORM_MAX_CPUS_PER_CLUSTER));
	asm volatile ("fence iorw, iorw");

	while (1)
		wfi();
}
