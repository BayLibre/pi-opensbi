/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Aurelien Jarno
 * Authors:
 *   Aurelien Jarno <aurelien@aurel32.net>
 */

#include <libfdt.h>
#include <platform_override.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_system.h>
#include <sbi_utils/fdt/fdt_driver.h>
#include <sbi_utils/i2c/fdt_i2c.h>

/* SpacemiT P1 Power Control Register 2 */
#define PWR_CTRL2		0x7e
#define PWR_CTRL2_SHUTDOWN	BIT(2)	/* Shutdown request */
#define PWR_CTRL2_RST		BIT(1)	/* Reset request */

/*
 * On the spm8821 PWR_CTRL2_RST only fires once the PMIC watchdog reset path is
 * fully armed (the vendor does this at boot via SYSRESET_WATCHDOG_AUTO). OpenSBI
 * has no boot-time watchdog start, so the reset path arms everything inline.
 */
#define WDT_CTRL		0x44
#define WDT_CLEAR_STATUS	BIT(0)
#define WDT_TIMEOUT_MASK	(0x3 << 1)	/* timeout field; 0 == 1s */
#define WDT_ENABLE		BIT(3)
#define PWR_CTRL0		0x7c
#define PWR_CTRL0_WDT_RST_EN	BIT(7)	/* enable watchdog reset output */
#define ALIVE_REG		0xab
#define ALIVE_REBOOT_FLAG	BIT(2)	/* reboot, not shutdown, after reset */

static struct i2c_adapter *p1_adapter = NULL;
static uint32_t p1_reg = 0;

static int p1_system_reset_check(uint32_t type, uint32_t reason)
{
	switch (type) {
	case SBI_SRST_RESET_TYPE_SHUTDOWN:
		return 1;
	case SBI_SRST_RESET_TYPE_COLD_REBOOT:
	case SBI_SRST_RESET_TYPE_WARM_REBOOT:
		return 255;
	}

	return 0;
}

static void p1_update_reg(uint32_t reg, uint8_t clr, uint8_t set)
{
	uint8_t byte;
	int rc;

	rc = i2c_adapter_reg_read(p1_adapter, p1_reg, reg, &byte);
	if (rc) {
		sbi_printf("%s: cannot read P1 reg 0x%x\n", __func__, reg);
		return;
	}

	byte = (byte & ~clr) | set;
	rc = i2c_adapter_reg_write(p1_adapter, p1_reg, reg, byte);
	if (rc)
		sbi_printf("%s: cannot write P1 reg 0x%x\n", __func__, reg);
}

static void p1_ops(uint32_t type)
{
	if (type == SBI_SRST_RESET_TYPE_SHUTDOWN) {
		p1_update_reg(PWR_CTRL2, 0, PWR_CTRL2_SHUTDOWN);
		return;
	}

	/*
	 * Reboot: arm the watchdog reset path the way the vendor does at boot,
	 * then trigger. Order matters -- PWR_CTRL2_RST is a no-op until the
	 * watchdog is enabled and its reset output is armed.
	 */
	p1_update_reg(WDT_CTRL, WDT_TIMEOUT_MASK, WDT_CLEAR_STATUS | WDT_ENABLE);
	p1_update_reg(PWR_CTRL0, 0, PWR_CTRL0_WDT_RST_EN);
	p1_update_reg(ALIVE_REG, 0, ALIVE_REBOOT_FLAG);
	p1_update_reg(PWR_CTRL2, 0, PWR_CTRL2_RST);
}

static void p1_system_reset(uint32_t type, uint32_t reason)
{
	switch (type) {
	case SBI_SRST_RESET_TYPE_SHUTDOWN:
	case SBI_SRST_RESET_TYPE_COLD_REBOOT:
	case SBI_SRST_RESET_TYPE_WARM_REBOOT:
		p1_ops(type);
		break;
	}
}

static struct sbi_system_reset_device p1_reset = {
	.name = "spacemit-p1-reset",
	.system_reset_check = p1_system_reset_check,
	.system_reset = p1_system_reset
};

static int p1_reset_init(const void *fdt, int nodeoff,
			 const struct fdt_match *match)
{
	int rc, i2c_bus;
	uint64_t addr;

	/* we are spacemit,p1 node */
	rc = fdt_get_node_addr_size(fdt, nodeoff, 0, &addr, NULL);
	if (rc)
		return rc;

	p1_reg = addr;

	/* find i2c bus parent node */
	i2c_bus = fdt_parent_offset(fdt, nodeoff);
	if (i2c_bus < 0)
		return i2c_bus;

	/* i2c adapter get */
	rc = fdt_i2c_adapter_get(fdt, i2c_bus, &p1_adapter);
	if (rc)
		return rc;

	sbi_system_reset_add_device(&p1_reset);

	return 0;
}

static const struct fdt_match p1_reset_match[] = {
	{ .compatible = "spacemit,p1" },
	{ },
};

const struct fdt_driver fdt_reset_spacemit_p1 = {
	.match_table = p1_reset_match,
	.init = p1_reset_init,
};
