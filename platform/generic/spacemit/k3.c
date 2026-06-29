/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 SpacemiT
 *
 * SpacemiT K3 platform_override.
 *
 * Ported from the vendor OpenSBI v1.6 tree
 * (platform/generic/spacemit/spacemit_k3.c) onto the upstream
 * platform_override model, modeled on platform/generic/spacemit/k1.c.
 *
 * Covers the cold-boot SoC setup: CCI-550 snoop/DVM enable on every
 * slave interface (CPU clusters and IO-coherent masters), per-cluster
 * reset-vector (RVBADDR) and hardware L2-flush programming for all four
 * clusters, the cluster-2 dummy-core wakeup and DMASYS ungate so masters
 * can reach the full TCM range, and the resume-aware cold-boot CSR setup.
 *
 * Power management (k3_corepm), the RCPU memrange protection and the
 * IMSIC save/restore are ported separately.
 */

#include <libfdt.h>
#include <platform_override.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <spacemit/k3.h>
#include "k3_corepm.h"

/* Upstream warm-boot entry; an out-of-range hartid parks in _start_hang. */
extern void _start_warm(void);
extern void _start_warm_dummy(void);

/* IMSIC interrupt-file save area offset in per-hart scratch (k3_corepm) */
unsigned long hart_imisc_save_offset;

static void cci_enable_snoop_dvm_reqs(unsigned int slave_if_id)
{
	/*
	 * Enable Snoops and DVM messages, no need for Read/Modify/Write as
	 * rest of bits are write ignore
	 */
	writel(CCI_550_SNOOP_CTRL_ENABLE_DVMS | CCI_550_SNOOP_CTRL_ENABLE_SNOOPS,
	       (void *)(u64)CCI_550_PLATFORM_CCI_ADDR +
	       CCI_550_SLAVE_IFACE_OFFSET(slave_if_id) + CCI_550_SNOOP_CTRL);

	/*
	 * Wait for the completion of the write to the Snoop Control Register
	 * before testing the change_pending bit
	 */
	mb();

	/* Wait for the dust to settle down */
	while ((readl((void *)(u64)CCI_550_PLATFORM_CCI_ADDR + CCI_550_STATUS) &
	       CCI_550_STATUS_CHANGE_PENDING))
		;
}

static void spacemit_k3_pre_init(void)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	unsigned long warmboot = scratch->warmboot_addr;
	unsigned int i;

	/* program the reset vector of every cluster to the warm-boot entry */
	writel(warmboot & 0xffffffff, (void *)C0_RVBADDR_LO_ADDR);
	writel((u64)warmboot >> 32, (void *)C0_RVBADDR_HI_ADDR);
	writel(warmboot & 0xffffffff, (void *)C1_RVBADDR_LO_ADDR);
	writel((u64)warmboot >> 32, (void *)C1_RVBADDR_HI_ADDR);
	writel(warmboot & 0xffffffff, (void *)C2_RVBADDR_LO_ADDR);
	writel((u64)warmboot >> 32, (void *)C2_RVBADDR_HI_ADDR);
	writel(warmboot & 0xffffffff, (void *)C3_RVBADDR_LO_ADDR);
	writel((u64)warmboot >> 32, (void *)C3_RVBADDR_HI_ADDR);

	/* use hardware-driven L2 flush for every cluster */
	writel(PMU_L2_FLUSH_HW_EN | PMU_L2_FLUSH_HW_TYPE,
	       (void *)PMU_C0_L2_FLUSH_CTRL);
	writel(PMU_L2_FLUSH_HW_EN | PMU_L2_FLUSH_HW_TYPE,
	       (void *)PMU_C1_L2_FLUSH_CTRL);
	writel(PMU_L2_FLUSH_HW_EN | PMU_L2_FLUSH_HW_TYPE,
	       (void *)PMU_C2_L2_FLUSH_CTRL);
	writel(PMU_L2_FLUSH_HW_EN | PMU_L2_FLUSH_HW_TYPE,
	       (void *)PMU_C3_L2_FLUSH_CTRL);

	/*
	 * Enable CCI-550 snoop/DVM on the four CPU-cluster slave interfaces
	 * (0-3) ONLY -- the vendor's effective config. Slave interfaces 4-6 are
	 * the ACE-Lite IO-coherent masters (UFS/dwc3/PCIe): they have no
	 * cache/TLB and cannot answer a snoop or DVM Sync, so setting SNOOP/DVM
	 * on them enrolls them as coherency participants whose completion never
	 * arrives -- every coherent transaction behind a DVM barrier then stalls
	 * (the address-independent UFS write OCS=f stall, made worse once
	 * dwc3/fastboot adds a second active IO master). The vendor DT marks
	 * ufshc/dwc3 non-coherent (SW does the cache maintenance), so the CCI
	 * must NOT snoop these interfaces.
	 */
	/* Let the preceding L2 flush drain before and after reprogramming snoop. */
	{ volatile int kd; for (kd = 0; kd < 0x80000; kd++) ; }
	for (i = 0; i < PLATFORM_CLUSTER_COUNT; i++)
		cci_enable_snoop_dvm_reqs(i);
	{ volatile int kd; for (kd = 0; kd < 0x80000; kd++) ; }


	/*
	 * Clear the idle power-down vote of every cluster (0-3), mirroring the
	 * vendor early_init devote loop. Loop past PLATFORM_MAX_CPUS so the
	 * clusters 2/3 register banks (case 8..15) are covered, not just the
	 * enumerated clusters 0-1 -- a loop bounded by hart_count(8) never
	 * touches cluster 2, which is exactly the one core8 lives in.
	 */
	for (i = 0; i < 16; i++)
		spacemit_devote_pwrdown_cluster(i);

	/*
	 * Wake the cluster-2 dummy core (core8) to fold cluster 2 into the
	 * CCI-550 coherency domain for the IO-coherent masters, and point it at
	 * _start_warm_dummy, which flushes + disables its dcache and clears its
	 * snoop lane before parking. Without that quiesce the woken core8 stays
	 * a live snoop responder with an enabled cache but is wedged in
	 * _start_hang, so a coherent READ broadcast to the snoop domain (UFS
	 * write data-phase, dwc3) never gets its snoop response and every
	 * IO-coherent DMA hangs.
	 */
	writel((unsigned long)_start_warm_dummy & 0xffffffff,
	       (void *)C2_RVBADDR_LO_ADDR);
	writel((u64)(unsigned long)_start_warm_dummy >> 32,
	       (void *)C2_RVBADDR_HI_ADDR);
	writel(1 << 8, (void *)PMU_CAP_CORE8_WAKEUP);

	/* deassert DMASYS reset and ungate its clock for full TCM reach */
	writel(1, (void *)DMASYS_RESET);
	writel(1, (void *)DMASYS_CLK_EN);
}

/*
 * K3 HSM device. The K3 power transitions are driven entirely from M-mode
 * register pokes (PMU idle-config / APCR vote) plus an IMSIC save-restore;
 * no RPMI message is exchanged for the transition itself. We register this
 * device from early_init *before* generic_early_init() scans the FDT early
 * drivers, so it pre-empts the generic rpmi-hsm device (sbi_hsm_set_device()
 * keeps the first registration). This keeps the shared rpmi HSM/suspend
 * drivers free of any K3-specific #ifdef.
 */
static int spacemit_k3_hart_start(u32 hartid, ulong saddr)
{
	return spacemit_wakeup_core(hartid);
}

static int spacemit_k3_hart_stop(void)
{
	__rpmi_shutdown_process();

	return 0;
}

static int spacemit_k3_hart_suspend(u32 suspend_type, ulong mmode_resume_addr)
{
	return __rpmi_hsm_suspend(suspend_type);
}

static void spacemit_k3_hart_resume(void)
{
	__rpmi_hsm_resume();
}

static const struct sbi_hsm_device spacemit_k3_hsm = {
	.name		= "spacemit-k3-hsm",
	.hart_start	= spacemit_k3_hart_start,
	.hart_stop	= spacemit_k3_hart_stop,
	.hart_suspend	= spacemit_k3_hart_suspend,
	.hart_resume	= spacemit_k3_hart_resume,
};

/*
 * Lock the RCPU runtime firmware regions with PMP-enforced no-access
 * memranges (ENF_PERMISSIONS denies M-mode too), protecting the running
 * RCPU environment from corruption.
 *
 * This is only meaningful when the U-Boot/SPL stage actually loads RCPU
 * firmware at the fixed layout addresses. We key off the FDT: register
 * the ranges only if an enabled "spacemit,k3-rproc" node is present. With
 * no such node (e.g. the RCPUs are unused), nothing is locked and the boot
 * is unaffected.
 */
static int spacemit_k3_lock_rcpu_memranges(void)
{
	const void *fdt = fdt_get_address();
	bool rcpu_present = false;
	int noff = -1;
	int rc;

	while ((noff = fdt_node_offset_by_compatible(fdt, noff,
						     "spacemit,k3-rproc")) >= 0) {
		if (fdt_node_is_enabled(fdt, noff)) {
			rcpu_present = true;
			break;
		}
	}

	if (!rcpu_present)
		return 0;

	rc = sbi_domain_root_add_memrange(RCPU0_RUNTIME_SPACE_BASE_ADDR,
					  RCPU0_RUNTIME_SPACE_SIZE,
					  RCPU_PMP_REGION_ALIGN,
					  SBI_DOMAIN_MEMREGION_ENF_PERMISSIONS);
	if (rc)
		return rc;

	rc = sbi_domain_root_add_memrange(RCPU1_RUNTIME_SPACE_BASE_ADDR,
					  RCPU1_RUNTIME_SPACE_SIZE,
					  RCPU_PMP_REGION_ALIGN,
					  SBI_DOMAIN_MEMREGION_ENF_PERMISSIONS);
	if (rc)
		return rc;

	return sbi_domain_root_add_memrange(RCPU_DTB_SPACE_BASE_ADDR,
					    RCPU_DTB_SPACE_SIZE,
					    RCPU_PMP_REGION_ALIGN,
					    SBI_DOMAIN_MEMREGION_ENF_PERMISSIONS);
}

/*
 * Platform early initialization.
 */
static int spacemit_k3_early_init(bool cold_boot)
{
	int rc;

	/* win the HSM device over the generic rpmi-hsm driver */
	if (cold_boot)
		sbi_hsm_set_device(&spacemit_k3_hsm);

	rc = generic_early_init(cold_boot);
	if (rc)
		return rc;

	if (cold_boot) {
		spacemit_k3_pre_init();

		rc = spacemit_k3_lock_rcpu_memranges();
		if (rc)
			return rc;
	}

	return 0;
}

/*
 * Platform final initialization. Allocate the per-hart scratch slot that
 * holds the IMSIC interrupt-file save area used across HSM suspend/resume,
 * then chain into the generic final init for the FDT fixups.
 */
static int spacemit_k3_final_init(bool cold_boot)
{
	if (cold_boot) {
		hart_imisc_save_offset =
			sbi_scratch_alloc_offset(sizeof(struct imsic_config));
		/*
		 * Fail loudly rather than silently corrupting the scratch base:
		 * the IMSIC save area (~5.4 KiB) must fit in SBI_SCRATCH_SIZE, else
		 * alloc returns 0 and __rpmi_hsm_suspend writes the IMSIC state over
		 * hart_features (clobbering pmp_count -> "insufficient PMP entries").
		 */
		if (!hart_imisc_save_offset)
			return SBI_ENOMEM;
	}

	return generic_final_init(cold_boot);
}

static bool spacemit_k3_cold_boot_allowed(u32 hartid)
{
	/* enable core snoop, L2 instruction-fetch refill and TLB prefetch */
	csr_set(CSR_ML2SETUP,
		(1 << (hartid % PLATFORM_MAX_CPUS_PER_CLUSTER)) | IPRF | TPRF);

	/* clear this core's power-down + APCR retention votes on each
	 * cold_boot_allowed entry (re-entered on every warm boot), mirroring the
	 * vendor cold_boot_allowed (spacemit_k3.c:215-218): keeps a bare-WFI
	 * secondary powered, coherent and IMSIC-wakeable. */
	spacemit_devote_pwrdown_cluster(hartid);
	spacemit_devote_core_apcr(hartid);

	/*
	 * L2/prefetch tuning for the cluster-2/3 cores; the boot clusters
	 * get the same tuning from their warm-boot entry.
	 */
	if (hartid >= PLATFORM_MAX_CPUS) {
		csr_set(CSR_PERF_CTRL, VEC_L1BYPASS);
		csr_set(CSR_PREFETCH_CTRL, L2_PERF_DIST);
		csr_clear(CSR_ML2HINT, CIU_CHR2_DEPD_DIS);
		csr_set(CSR_ML2HINT, CIU_CHR2_MER_DIS);
	}

	/*
	 * On resume, hart0 must warm-boot rather than cold-boot: deny the
	 * cold path if it is parked in the SUSPENDED state.
	 */
	if (!hartid &&
	    __sbi_hsm_hart_get_state(sbi_hartid_to_hartindex(0)) ==
		    SBI_HSM_STATE_SUSPENDED)
		return false;

	return !hartid;
}

static int spacemit_k3_platform_init(const void *fdt, int nodeoff,
				     const struct fdt_match *match)
{
	generic_platform_ops.early_init = spacemit_k3_early_init;
	generic_platform_ops.final_init = spacemit_k3_final_init;
	generic_platform_ops.cold_boot_allowed = spacemit_k3_cold_boot_allowed;

	return 0;
}

static const struct fdt_match spacemit_k3_match[] = {
	{ .compatible = "spacemit,k3" },
	{ /* sentinel */ }
};

const struct fdt_driver spacemit_k3 = {
	.match_table = spacemit_k3_match,
	.init = spacemit_k3_platform_init,
};
