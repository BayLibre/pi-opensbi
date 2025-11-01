/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) IPADS@SJTU 2023. All rights reserved.
 */

#ifndef __SBI_DOMAIN_CONTEXT_H__
#define __SBI_DOMAIN_CONTEXT_H__

#include <sbi/sbi_types.h>

struct sbi_domain;
struct sbi_trap_regs;

/**
 * Enter a specific domain context synchronously
 * @param dom pointer to domain
 *
 * @return 0 on success and negative error code on failure
 */
int sbi_domain_context_enter(struct sbi_domain *dom);

/**
 * Exit the current domain context, and then return to the caller
 * of sbi_domain_context_enter or attempt to start the next domain
 * context to be initialized
 *
 * @return 0 on success and negative error code on failure
 */
int sbi_domain_context_exit(void);

/**
 * Initialize domain context support
 *
 * @return 0 on success and negative error code on failure
 */
int sbi_domain_context_init(void);

/* Deinitialize domain context support */
void sbi_domain_context_deinit(void);

int sbi_domain_context_set_mepc(struct sbi_domain *dom, unsigned long entry_point);
int set_domain_regs(struct sbi_domain *domain, struct sbi_trap_regs *regs);
int sbi_domain_hart_context_alloc(struct sbi_domain *dom);
int sbi_domain_init_hart_context(struct sbi_domain *dom);
void sbi_domain_restore_scratch();

#endif // __SBI_DOMAIN_CONTEXT_H__
