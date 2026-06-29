/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 SpacemiT
 *
 * SpacemiT K3 runtime power management interface.
 */

#ifndef __SPACEMIT_K3_COREPM_H__
#define __SPACEMIT_K3_COREPM_H__

#include <sbi/sbi_types.h>

void spacemit_mask_irq(uint32_t hartid);
void spacemit_unmask_irq(uint32_t hartid);
void spacemit_vote_core_apcr(uint32_t hartid);
void spacemit_devote_core_apcr(uint32_t hartid);
void spacemit_vote_powrdown_core(uint32_t hartid);
void spacemit_vote_powrdown_cluster(uint32_t hartid);
void spacemit_devote_pwrdown_cluster(uint32_t hartid);
int spacemit_wakeup_core(uint32_t hartid);

int __rpmi_hsm_suspend_pre(void);
int __rpmi_hsm_suspend(u32 type);
void __rpmi_hsm_resume(void);
void __rpmi_shutdown_process(void);

#endif /* __SPACEMIT_K3_COREPM_H__ */
