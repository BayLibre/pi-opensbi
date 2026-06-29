/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 SpacemiT
 *
 * SpacemiT K3 platform definitions.
 *
 * Ported from the vendor OpenSBI tree (platform/generic/spacemit/spacemit_k3.c
 * + include/spacemit/k3/). The 8-core Pico-ITX boots clusters 0 and 1;
 * clusters 2/3 and the dummy core8 are brought to a known state so that
 * IO-coherent masters and DMASYS can reach the full TCM range. The
 * power-management (k3_corepm) paths are ported separately.
 */

#ifndef __RISCV_SPACEMIT_K3_H__
#define __RISCV_SPACEMIT_K3_H__

#include <sbi/riscv_asm.h>
#include <sbi/sbi_bitops.h>

/* SpacemiT custom CSRs (from vendor k3/core_common.h) */
#define CSR_MSETUP				0x7c0
#define CSR_MHCR				0x7c1
#define CSR_MHINT				0x7c5
#define CSR_ML2SETUP				0x7f0
#define CSR_ML2HINT				0x7f7
#define CSR_PERF_CTRL				0x7d0
#define CSR_PREFETCH_CTRL			0x7d1
#define CSR_PMACFG0				0x7de

/* CSR_ML2SETUP bits */
/* L2 instruction-fetch refill on miss */
#define IPRF					BIT(16)
/* L2 TLB prefetch enable */
#define TPRF					BIT(18)

/* CSR_ML2HINT bits */
/* disable read/prefetch transaction merge */
#define CIU_CHR2_MER_DIS			BIT(2)
/* disable full address dependency check */
#define CIU_CHR2_DEPD_DIS			BIT(3)

/* CSR_PERF_CTRL bits */
/* vector loads bypass L1, cached in L2 only */
#define VEC_L1BYPASS				BIT(32)

/* CSR_PREFETCH_CTRL bits */
/* L2 prefetch distance = 56 entries */
#define L2_PERF_DIST				(_UL(3) << 10)

/* Per-cluster reset vector base address registers (from vendor k3/k3.h) */
#define C0_RVBADDR_LO_ADDR			0xd4282db0
#define C0_RVBADDR_HI_ADDR			0xd4282db4
#define C1_RVBADDR_LO_ADDR			(0xd4282c00 + 0x2b0)
#define C1_RVBADDR_HI_ADDR			(0xd4282c00 + 0x2b4)
#define C2_RVBADDR_LO_ADDR			(0xd4282c00 + 0x3e8)
#define C2_RVBADDR_HI_ADDR			(0xd4282c00 + 0x3ec)
#define C3_RVBADDR_LO_ADDR			(0xd4282c00 + 0x260)
#define C3_RVBADDR_HI_ADDR			(0xd4282c00 + 0x264)

/* Wakeup latch for the cluster-2 dummy core (core8) */
#define PMU_CAP_CORE8_WAKEUP			(0xd4282800 + 0x360)

/* DMASYS reset/clock: ungated so masters can reach the full TCM range */
#define DMASYS_RESET				(0xd8440000 + 0x22c)
#define DMASYS_CLK_EN				(0xd8440000 + 0x234)

/* PMU hardware L2-flush control, per cluster */
#define PMU_C0_L2_FLUSH_CTRL			(0xd8440000 + 0x1b0)
#define PMU_C1_L2_FLUSH_CTRL			(0xd8440000 + 0x1b4)
#define PMU_C2_L2_FLUSH_CTRL			(0xd8440000 + 0x1c4)
#define PMU_C3_L2_FLUSH_CTRL			(0xd8440000 + 0x1ec)
#define PMU_L2_FLUSH_HW_TYPE			BIT(0)
#define PMU_L2_FLUSH_HW_EN			BIT(2)

/* CCI-550 (from vendor k3/k3.h + modeled on upstream k1.h) */
#define CCI_550_PLATFORM_CCI_ADDR		0xd8500000

/* relative to cci base */
#define CCI_550_STATUS				0x000c
/* status register bits */
#define CCI_550_STATUS_CHANGE_PENDING		BIT(0)

/* slave interface registers */
#define CCI_550_SLAVE_IFACE0_OFFSET		0x1000
#define CCI_550_SLAVE_IFACE_OFFSET(idx)		(CCI_550_SLAVE_IFACE0_OFFSET + ((0x1000) * (idx)))

/* relative to slave interface base */
#define CCI_550_SNOOP_CTRL			0x0000
/* snoop control register bits */
#define CCI_550_SNOOP_CTRL_ENABLE_SNOOPS	BIT(0)
#define CCI_550_SNOOP_CTRL_ENABLE_DVMS		BIT(1)

/* CCI slave-interface index per cluster */
#define PLAT_CCI_CLUSTER0_IFACE_IX		0
#define PLAT_CCI_CLUSTER1_IFACE_IX		1
#define PLAT_CCI_CLUSTER2_IFACE_IX		2
#define PLAT_CCI_CLUSTER3_IFACE_IX		3

/*
 * Total CCI-550 slave interfaces. 0-3 are the CPU clusters, 4-6 are
 * IO-coherent masters; snoop/DVM must be enabled on all of them.
 */
#define CCI_550_SLAVE_IFACE_COUNT		7

/*
 * RCPU runtime firmware layout (from vendor k3/k3.h). When the U-Boot/SPL
 * boot stage loads RCPU firmware it places the two RCPU images and the
 * shared RCPU DTB at these fixed physical addresses. OpenSBI locks them
 * down with PMP-enforced no-access memranges so neither M/S/U mode can
 * corrupt the running RCPU environment. Registered conditionally: only
 * when an enabled "spacemit,k3-rproc" FDT node confirms the layout is used.
 */
#define RCPU0_RUNTIME_SPACE_BASE_ADDR		0x100200000UL
#define RCPU0_RUNTIME_SPACE_SIZE		0x500000UL
#define RCPU1_RUNTIME_SPACE_BASE_ADDR		0x100800000UL
#define RCPU1_RUNTIME_SPACE_SIZE		0x500000UL
#define RCPU_DTB_SPACE_BASE_ADDR		0x100f00000UL
#define RCPU_DTB_SPACE_SIZE			0x300000UL

/* PMP region alignment for the RCPU lock ranges */
#define RCPU_PMP_REGION_ALIGN			0x100000UL

/* clusters and CPU mapping (4 clusters of 4; Pico-ITX boots clusters 0-1) */
#define PLATFORM_CLUSTER_COUNT			4
#define PLATFORM_MAX_CPUS			8
#define PLATFORM_MAX_CPUS_PER_CLUSTER		4
#define CPU_TO_CLUSTER(cpu)			((cpu) / PLATFORM_MAX_CPUS_PER_CLUSTER)

/*
 * Runtime power-management register map (k3_corepm). The PMU wakeup/idle
 * latches and the APCR vote registers are per-core; the cluster idle
 * configuration (PMU_CX_CAPMP) is per-cluster. Addresses come from the
 * vendor k3/k3.h; the 16-core layout is non-linear by design.
 */
#define PMU_CAP_CORE0_WAKEUP			(0xd4282800 + 0x12c)
#define PMU_CAP_CORE1_WAKEUP			(0xd4282800 + 0x130)
#define PMU_CAP_CORE2_WAKEUP			(0xd4282800 + 0x134)
#define PMU_CAP_CORE3_WAKEUP			(0xd4282800 + 0x138)
#define PMU_CAP_CORE4_WAKEUP			(0xd4282800 + 0x324)
#define PMU_CAP_CORE5_WAKEUP			(0xd4282800 + 0x328)
#define PMU_CAP_CORE6_WAKEUP			(0xd4282800 + 0x32c)
#define PMU_CAP_CORE7_WAKEUP			(0xd4282800 + 0x330)
/* PMU_CAP_CORE8_WAKEUP is declared above (cluster-2 dummy core) */
#define PMU_CAP_CORE9_WAKEUP			(0xd4282800 + 0x364)
#define PMU_CAP_CORE10_WAKEUP			(0xd4282800 + 0x368)
#define PMU_CAP_CORE11_WAKEUP			(0xd4282800 + 0x36c)
#define PMU_CAP_CORE12_WAKEUP			(0xd4282800 + 0x22c)
#define PMU_CAP_CORE13_WAKEUP			(0xd4282800 + 0x230)
#define PMU_CAP_CORE14_WAKEUP			(0xd4282800 + 0x234)
#define PMU_CAP_CORE15_WAKEUP			(0xd4282800 + 0x238)

#define PMU_CAP_CORE0_IDLE_CFG			(0xd4282800 + 0x124)
#define PMU_CAP_CORE1_IDLE_CFG			(0xd4282800 + 0x128)
#define PMU_CAP_CORE2_IDLE_CFG			(0xd4282800 + 0x160)
#define PMU_CAP_CORE3_IDLE_CFG			(0xd4282800 + 0x164)
#define PMU_CAP_CORE4_IDLE_CFG			(0xd4282800 + 0x304)
#define PMU_CAP_CORE5_IDLE_CFG			(0xd4282800 + 0x308)
#define PMU_CAP_CORE6_IDLE_CFG			(0xd4282800 + 0x30c)
#define PMU_CAP_CORE7_IDLE_CFG			(0xd4282800 + 0x310)
#define PMU_CAP_CORE8_IDLE_CFG			(0xd4282800 + 0x340)
#define PMU_CAP_CORE9_IDLE_CFG			(0xd4282800 + 0x344)
#define PMU_CAP_CORE10_IDLE_CFG			(0xd4282800 + 0x348)
#define PMU_CAP_CORE11_IDLE_CFG			(0xd4282800 + 0x34c)
#define PMU_CAP_CORE12_IDLE_CFG			(0xd4282800 + 0x20c)
#define PMU_CAP_CORE13_IDLE_CFG			(0xd4282800 + 0x210)
#define PMU_CAP_CORE14_IDLE_CFG			(0xd4282800 + 0x214)
#define PMU_CAP_CORE15_IDLE_CFG			(0xd4282800 + 0x218)

#define PMU_CX_CAPMP_IDLE_CFG0			(0xd4282800 + 0x120)
#define PMU_CX_CAPMP_IDLE_CFG1			(0xd4282800 + 0xe4)
#define PMU_CX_CAPMP_IDLE_CFG2			(0xd4282800 + 0x150)
#define PMU_CX_CAPMP_IDLE_CFG3			(0xd4282800 + 0x154)
#define PMU_CX_CAPMP_IDLE_CFG4			(0xd4282800 + 0x314)
#define PMU_CX_CAPMP_IDLE_CFG5			(0xd4282800 + 0x318)
#define PMU_CX_CAPMP_IDLE_CFG6			(0xd4282800 + 0x31c)
#define PMU_CX_CAPMP_IDLE_CFG7			(0xd4282800 + 0x320)
#define PMU_CX_CAPMP_IDLE_CFG8			(0xd4282800 + 0x350)
#define PMU_CX_CAPMP_IDLE_CFG9			(0xd4282800 + 0x354)
#define PMU_CX_CAPMP_IDLE_CFG10			(0xd4282800 + 0x358)
#define PMU_CX_CAPMP_IDLE_CFG11			(0xd4282800 + 0x35c)
#define PMU_CX_CAPMP_IDLE_CFG12			(0xd4282800 + 0x21c)
#define PMU_CX_CAPMP_IDLE_CFG13			(0xd4282800 + 0x220)
#define PMU_CX_CAPMP_IDLE_CFG14			(0xd4282800 + 0x224)
#define PMU_CX_CAPMP_IDLE_CFG15			(0xd4282800 + 0x228)

#define APCR_CORE0_VETE_REG			(0xd4050000 + 0x10c0)
#define APCR_CORE1_VETE_REG			(0xd4050000 + 0x10c4)
#define APCR_CORE2_VETE_REG			(0xd4050000 + 0x10c8)
#define APCR_CORE3_VETE_REG			(0xd4050000 + 0x10cc)
#define APCR_CORE4_VETE_REG			(0xd4050000 + 0x10d0)
#define APCR_CORE5_VETE_REG			(0xd4050000 + 0x10d4)
#define APCR_CORE6_VETE_REG			(0xd4050000 + 0x10d8)
#define APCR_CORE7_VETE_REG			(0xd4050000 + 0x10dc)
#define APCR_CORE8_VETE_REG			(0xd4050000 + 0x10e0)
#define APCR_CORE9_VETE_REG			(0xd4050000 + 0x10e4)
#define APCR_CORE10_VETE_REG			(0xd4050000 + 0x10e8)
#define APCR_CORE11_VETE_REG			(0xd4050000 + 0x10ec)
#define APCR_CORE12_VETE_REG			(0xd4050000 + 0x10f0)
#define APCR_CORE13_VETE_REG			(0xd4050000 + 0x10f4)
#define APCR_CORE14_VETE_REG			(0xd4050000 + 0x10f8)
#define APCR_CORE15_VETE_REG			(0xd4050000 + 0x10fc)

#define APCR_COREX_DEFAULT_VATE_VALUE		(BIT(3) | BIT(13) | BIT(14) | \
						 BIT(19) | BIT(25) | BIT(26) | \
						 BIT(27) | BIT(29) | BIT(31))

/* PMU idle-config / cluster-config field values */
#define CPU_MASK_FI_INTTERUPT			(BIT(3) | BIT(4))
#define CPU_PWR_DOWN_VALUE			0x1f
#define CLUSTER_PWR_DOWN_VALUE			0x8f

/* M-mode IMSIC interrupt-file save area, used across HSM suspend/resume */
#define MAX_IMSIC_EIE_REGISTERS			64
#define IMSIC_FIRST_EIE_REG			0xc0
#define IMSIC_EIDELIVERY			0x70
#define IMSIC_EITHRESHOLD			0x72
#define IMSIC_MAX_VGEN				0x8
/* xtopei reports the pending interrupt id in bits [26:16] */
#define TOPEI_ID_SHIFT				16

struct himsic_config {
	/* h-mode (per virtual guest) */
	unsigned long long heidelivery;
	unsigned long long heithreshold;
	unsigned long long heie[MAX_IMSIC_EIE_REGISTERS];
};

struct imsic_config {
	unsigned int flags;
	/* m-mode */
	unsigned long long meidelivery;
	unsigned long long meithreshold;
	unsigned long long meie[MAX_IMSIC_EIE_REGISTERS];
	/* s-mode */
	unsigned long long seidelivery;
	unsigned long long seithreshold;
	unsigned long long seie[MAX_IMSIC_EIE_REGISTERS];
	/* h-mode */
	unsigned long long hstatus;
	unsigned long long hedeleg;
	unsigned long long hideleg;
	unsigned long long hie;
	unsigned long long hcounteren;
	unsigned long long hgeie;
	unsigned long long henvcfg;
	unsigned long long henvcfgh;
	unsigned long long htval;
	unsigned long long hgatp;
	unsigned long long htimedelta;
	unsigned long long htimedeltah;
	struct himsic_config hc[IMSIC_MAX_VGEN];
};

/*
 * SpacemiT CSR-based cache maintenance helpers. These are not the upstream
 * CMO (cbo.*) primitives in <sbi_utils/cache/cache.h>; they drive the
 * vendor's custom M-mode setup/flush CSRs and are only valid on the K3 core.
 */
static inline void csi_disable_data_preftch(void)
{
	csr_clear(CSR_MSETUP, 32);
}

static inline void csi_flush_dcache_all(void)
{
	csr_set(0x7c2, 0x3);
}

static inline void csi_disable_cache(void)
{
	csr_clear(CSR_MSETUP, 3);
}

#endif
