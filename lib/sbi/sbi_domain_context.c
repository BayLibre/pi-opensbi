/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#include <sbi/sbi_error.h>
#include <sbi/riscv_locks.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_heap.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_domain_context.h>
#include <sbi/sbi_trap.h>
#include <sbi_utils/fdt/fdt_helper.h>

/** Context representation for a hart within a domain */
struct hart_context {
	/** Trap-related states such as GPRs, mepc, and mstatus */
	struct sbi_trap_context trap_ctx;

	/** Supervisor status register */
	unsigned long sstatus;
	/** Supervisor interrupt enable register */
	unsigned long sie;
	/** Supervisor trap vector base address register */
	unsigned long stvec;
	/** Supervisor scratch register for temporary storage */
	unsigned long sscratch;
	/** Supervisor exception program counter register */
	unsigned long sepc;
	/** Supervisor cause register */
	unsigned long scause;
	/** Supervisor trap value register */
	unsigned long stval;
	/** Supervisor interrupt pending register */
	unsigned long sip;
	/** Supervisor address translation and protection register */
	unsigned long satp;
	/** Counter-enable register */
	unsigned long scounteren;
	/** Supervisor environment configuration register */
	unsigned long senvcfg;

	/** Reference to the owning domain */
	struct sbi_domain *dom;
	/** Previous context (caller) to jump to during context exits */
	struct hart_context *prev_ctx;
	/** Is context initialized and runnable */
	bool initialized;
};

struct domain_context_priv {
	/** Contexts for possible HARTs indexed by hartindex */
	struct hart_context *hartindex_to_context_table[SBI_HARTMASK_MAX_BITS];
};

static struct sbi_domain_data dcpriv = {
	.data_size = sizeof(struct domain_context_priv),
};

static inline struct hart_context *hart_context_get(struct sbi_domain *dom,
						    u32 hartindex)
{
	struct domain_context_priv *dcp = sbi_domain_data_ptr(dom, &dcpriv);

	return (dcp && hartindex < SBI_HARTMASK_MAX_BITS) ?
		dcp->hartindex_to_context_table[hartindex] : NULL;
}

static void hart_context_set(struct sbi_domain *dom, u32 hartindex,
			     struct hart_context *hc)
{
	struct domain_context_priv *dcp = sbi_domain_data_ptr(dom, &dcpriv);

	if (dcp && hartindex < SBI_HARTMASK_MAX_BITS)
		dcp->hartindex_to_context_table[hartindex] = hc;
}

/** Macro to obtain the current hart's context pointer */
#define hart_context_thishart_get()					\
	hart_context_get(sbi_domain_thishart_ptr(),			\
			 current_hartindex())


static void debug_hart_context(const struct hart_context *ctx, bool target_flag)
{
#if 0
	u32 hartindex = current_hartindex();
	struct sbi_domain *dom = ctx->dom;
	if (target_flag)
		sbi_printf("%s: target domain: %s, hart:%x context:\n", __func__, dom->name, hartindex);
	else
		sbi_printf("%s: current domain: %s, hart:%x context:\n", __func__, dom->name, hartindex);
	sbi_printf("S mode context: \n");

	sbi_printf("sstatus: %lx\n", ctx->sstatus);
	sbi_printf("sie: %lx\n", ctx->sie);
	sbi_printf("stvec: %lx\n", ctx->stvec);
	sbi_printf("sscratch: %lx\n", ctx->sscratch);
	sbi_printf("sepc: %lx\n", ctx->sepc);
	sbi_printf("scause: %lx\n", ctx->scause);
	sbi_printf("stval: %lx\n", ctx->stval);
	sbi_printf("sip: %lx\n", ctx->sip);
	sbi_printf("statp: %lx\n", ctx->satp);

	sbi_printf("trap context: \n");
	sbi_printf("a0-a4: %lx,%lx,%lx,%lx\n", ctx->trap_ctx.regs.a0, ctx->trap_ctx.regs.a1, ctx->trap_ctx.regs.a2, ctx->trap_ctx.regs.a3);
	sbi_printf("tp:%lx, gp:%lx, sp:%lx, ra:%lx\n", ctx->trap_ctx.regs.tp, ctx->trap_ctx.regs.gp, ctx->trap_ctx.regs.sp, ctx->trap_ctx.regs.ra);
	sbi_printf("mepc: %lx\n", ctx->trap_ctx.regs.mepc);
	sbi_printf("msstatus: %lx,\n", ctx->trap_ctx.regs.mstatus);
#endif
}
/**
 * Switches the HART context from the current domain to the target domain.
 * This includes changing domain assignments and reconfiguring PMP, as well
 * as saving and restoring CSRs and trap states.
 *
 * @param ctx pointer to the current HART context
 * @param dom_ctx pointer to the target domain context
 */
static void switch_to_next_domain_context(struct hart_context *ctx,
					  struct hart_context *dom_ctx)
{
	u32 hartindex = current_hartindex();
	struct sbi_trap_context *trap_ctx;
	struct sbi_domain *current_dom = ctx->dom;
	struct sbi_domain *target_dom = dom_ctx->dom;
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	unsigned int pmp_count = sbi_hart_pmp_count(scratch);

	/* Assign current hart to target domain */
	spin_lock(&current_dom->assigned_harts_lock);
	sbi_hartmask_clear_hartindex(hartindex, &current_dom->assigned_harts);
	spin_unlock(&current_dom->assigned_harts_lock);

	sbi_update_hartindex_to_domain(hartindex, target_dom);

	spin_lock(&target_dom->assigned_harts_lock);
	sbi_hartmask_set_hartindex(hartindex, &target_dom->assigned_harts);
	spin_unlock(&target_dom->assigned_harts_lock);

	/* Reconfigure PMP settings for the new domain */
	for (int i = 0; i < pmp_count; i++) {
		pmp_disable(i);
	}
	sbi_hart_pmp_configure(scratch);

	/* Save current CSR context and restore target domain's CSR context */
	ctx->sstatus	= csr_swap(CSR_SSTATUS, dom_ctx->sstatus);
	ctx->sie	= csr_swap(CSR_SIE, dom_ctx->sie);
	ctx->stvec	= csr_swap(CSR_STVEC, dom_ctx->stvec);
	ctx->sscratch	= csr_swap(CSR_SSCRATCH, dom_ctx->sscratch);
	ctx->sepc	= csr_swap(CSR_SEPC, dom_ctx->sepc);
	ctx->scause	= csr_swap(CSR_SCAUSE, dom_ctx->scause);
	ctx->stval	= csr_swap(CSR_STVAL, dom_ctx->stval);
	ctx->sip	= csr_swap(CSR_SIP, dom_ctx->sip);
	ctx->satp	= csr_swap(CSR_SATP, dom_ctx->satp);
	if (sbi_hart_priv_version(scratch) >= SBI_HART_PRIV_VER_1_10)
		ctx->scounteren = csr_swap(CSR_SCOUNTEREN, dom_ctx->scounteren);
	if (sbi_hart_priv_version(scratch) >= SBI_HART_PRIV_VER_1_12)
		ctx->senvcfg	= csr_swap(CSR_SENVCFG, dom_ctx->senvcfg);

	/* Save current trap state and restore target domain's trap state */
	trap_ctx = sbi_trap_get_context(scratch);
	sbi_memcpy(&ctx->trap_ctx, trap_ctx, sizeof(*trap_ctx));
	sbi_memcpy(trap_ctx, &dom_ctx->trap_ctx, sizeof(*trap_ctx));

	debug_hart_context(ctx, false);
	debug_hart_context(dom_ctx, true);
	/* Mark current context structure initialized because context saved */
	ctx->initialized = true;

	/* If target domain context is not initialized or runnable */
	if (!dom_ctx->initialized) {
		/* Startup boot HART of target domain */
		if (current_hartid() == target_dom->boot_hartid)
			sbi_hart_switch_mode(target_dom->boot_hartid,
					     target_dom->next_arg1,
					     target_dom->next_addr,
					     target_dom->next_mode,
					     false);
		else
			sbi_hsm_hart_stop(scratch, true);
	}
}

int sbi_domain_context_set_mepc(struct sbi_domain *dom, unsigned long entry_point)
{
	struct hart_context *dom_ctx = hart_context_get(dom, current_hartindex());

	/* Validate the domain context existence */
	if (!dom_ctx)
		return SBI_EINVAL;

	dom_ctx->trap_ctx.regs.mepc = entry_point;

	return SBI_OK;
}

int sbi_domain_context_enter(struct sbi_domain *dom)
{
	struct hart_context *ctx = hart_context_thishart_get();
	struct hart_context *dom_ctx = hart_context_get(dom, current_hartindex());

	/* Validate the domain context existence */
	if (!dom_ctx)
		return SBI_EINVAL;

	/* Update target context's previous context to indicate the caller */
	dom_ctx->prev_ctx = ctx;

	switch_to_next_domain_context(ctx, dom_ctx);

	return 0;
}

int set_domain_regs(struct sbi_domain *domain, struct sbi_trap_regs *regs)
{
	struct hart_context *dom_ctx = hart_context_get(domain, current_hartindex());
	/* Validate the domain context existence */
	if (!dom_ctx)
		return SBI_EINVAL;

	dom_ctx->trap_ctx.regs.a0 = regs->a0;
	dom_ctx->trap_ctx.regs.a1 = regs->a1;
	dom_ctx->trap_ctx.regs.a2 = regs->a2;
	dom_ctx->trap_ctx.regs.a3 = regs->a3;
	dom_ctx->trap_ctx.regs.a4 = regs->a4;
	dom_ctx->trap_ctx.regs.a5 = regs->a5;
	dom_ctx->trap_ctx.regs.a6 = regs->a6;
	dom_ctx->trap_ctx.regs.a7 = regs->a7;

	return SBI_OK;
}

int sbi_domain_context_exit(void)
{
	u32 hartindex = current_hartindex();
	struct sbi_domain *dom;
	struct hart_context *ctx = hart_context_thishart_get();
	struct hart_context *dom_ctx, *tmp;

	/*
	 * If it's first time to call `exit` on the current hart, no
	 * context allocated before. Loop through each domain to allocate
	 * its context on the current hart if valid.
	 */
	if (!ctx) {
		sbi_domain_for_each(dom) {
			if (!sbi_hartmask_test_hartindex(hartindex,
							 dom->possible_harts))
				continue;

			dom_ctx = sbi_zalloc(sizeof(struct hart_context));
			if (!dom_ctx)
				return SBI_ENOMEM;

			/* Bind context and domain */
			dom_ctx->dom = dom;
			hart_context_set(dom, hartindex, dom_ctx);
		}

		ctx = hart_context_thishart_get();
	}

	dom_ctx = ctx->prev_ctx;

	/* If no previous caller context */
	if (!dom_ctx) {
		/* Try to find next uninitialized user-defined domain's context */
		sbi_domain_for_each(dom) {
			if (dom == &root || dom == sbi_domain_thishart_ptr())
				continue;

			tmp = hart_context_get(dom, hartindex);
			if (tmp && !tmp->initialized) {
				dom_ctx = tmp;
				break;
			}
		}
	}

	/* Take the root domain context if fail to find */
	if (!dom_ctx)
		dom_ctx = hart_context_get(&root, hartindex);

	switch_to_next_domain_context(ctx, dom_ctx);

	return 0;
}

int sbi_domain_context_init(void)
{
	return sbi_domain_register_data(&dcpriv);
}

void sbi_domain_context_deinit(void)
{
	sbi_domain_unregister_data(&dcpriv);
}

int sbi_domain_hart_context_alloc(struct sbi_domain *dom)
{
	if(!dom)
		return SBI_EINVAL;

	u32 hartindex = current_hartindex();
	struct hart_context *dom_ctx = hart_context_get(dom, hartindex);

	if (!dom_ctx && sbi_hartmask_test_hartindex(hartindex, dom->possible_harts)) {
		dom_ctx = sbi_zalloc(sizeof(struct hart_context));
		if (!dom_ctx)
			return SBI_ENOMEM;
		dom_ctx->dom = dom;
		hart_context_set(dom, hartindex, dom_ctx);
	}

	return SBI_OK;
}

int sbi_domain_init_hart_context(struct sbi_domain *dom)
{
	if(!dom)
		return SBI_EINVAL;

	u32 hartindex = current_hartindex();
	struct hart_context *dom_ctx = hart_context_get(dom, hartindex);
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	struct hart_context *current_ctx = hart_context_thishart_get();
	struct sbi_trap_context *trap_ctx;

	//if dom is current domain(ree domain), read regs from current hart.
	if (dom_ctx == current_ctx) {
		dom_ctx->sstatus	= csr_read(CSR_SSTATUS);
		dom_ctx->sie	= csr_read(CSR_SIE);
		dom_ctx->stvec	= csr_read(CSR_STVEC);
		dom_ctx->sscratch	= csr_read(CSR_SSCRATCH);
		dom_ctx->sepc	= csr_read(CSR_SEPC);
		dom_ctx->scause	= csr_read(CSR_SCAUSE);
		dom_ctx->stval	= csr_read(CSR_STVAL);
		dom_ctx->sip	= csr_read(CSR_SIP);
		if (sbi_hart_priv_version(scratch) >= SBI_HART_PRIV_VER_1_10)
			dom_ctx->scounteren = csr_swap(CSR_SCOUNTEREN, dom_ctx->scounteren);
		if (sbi_hart_priv_version(scratch) >= SBI_HART_PRIV_VER_1_12)
			dom_ctx->senvcfg	= csr_swap(CSR_SENVCFG, dom_ctx->senvcfg);

		/* update to mepc and next_arg1 from scrash, this value is update by hsm start process*/
		trap_ctx = sbi_trap_get_context(scratch);
		dom_ctx->trap_ctx.regs.mepc = scratch->next_addr;
		dom_ctx->trap_ctx.regs.a0 = hartindex;
		dom_ctx->trap_ctx.regs.a1 = scratch->next_arg1;

		/* Save current trap state */
		dom_ctx->trap_ctx.prev_context = NULL;
		sbi_trap_set_context(scratch, &dom_ctx->trap_ctx);
		dom_ctx->initialized = true;
	}
	else {
		dom_ctx->sstatus = 0;
		sbi_memset(&dom_ctx->trap_ctx, 0, sizeof(*trap_ctx));

		dom_ctx->sstatus = 0;
		dom_ctx->sie = 0;
		dom_ctx->stvec = 0;
		dom_ctx->sscratch = 0;
		dom_ctx->sepc = 0;
		dom_ctx->scause = 0;
		dom_ctx->stval = 0;
		dom_ctx->sip = 0;
		dom_ctx->satp = 0;

		//set context initialized to true
		dom_ctx->initialized = true;

		dom_ctx->prev_ctx = current_ctx;
		dom_ctx->trap_ctx.prev_context = &dom_ctx->trap_ctx;
	}

	return SBI_OK;
}

void sbi_domain_restore_scratch()
{
	struct hart_context *ctx = hart_context_thishart_get();
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	scratch->next_addr = ctx->trap_ctx.regs.mepc;
	scratch->next_arg1 = ctx->trap_ctx.regs.a1;
}
