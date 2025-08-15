/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_system.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/reset/fdt_reset.h>

static int nr_dies = 1;

__attribute__((naked, noreturn)) static void core_start_warm(void)
{
	__asm__ __volatile__ (				   \
			" li      t1, 0x70013 \n"	   \
			" csrw    0x7c2, t1 \n"            \
			" fence rw,rw \n"            	   \
			" j _start_warm \n"		   \
			:                   		   \
			:                                  \
			: "memory");
}

static ulong core_entry = (ulong) core_start_warm;

static int imp_system_reset_check(u32 type, u32 reason)
{
	return 0;
}

static void imp_system_reset(u32 type, u32 reason)
{
	ebreak();
}

static struct sbi_system_reset_device imp_reset = {
	.name = "reset",
	.system_reset_check = imp_system_reset_check,
	.system_reset = imp_system_reset
};

#define NCORE_BASE			0x6f800000
#define DIE1_OFFSET			0x2000000000ul

#define ACE1_OFFSET			0x1000
#define ACE3_OFFSET			0x3000
#define XAIUTCR				0x40
#define XAIUTAR				0x44

#define NCORE_OFFSET_ACE1		NCORE_BASE + ACE1_OFFSET
#define NCORE_OFFSET_ACE1_XAIUTCR	NCORE_OFFSET_ACE1 + XAIUTCR
#define NCORE_OFFSET_ACE1_XAIUTAR	NCORE_OFFSET_ACE1 + XAIUTAR
#define NCORE_OFFSET_ACE3		NCORE_BASE + ACE3_OFFSET
#define NCORE_OFFSET_ACE3_XAIUTCR	NCORE_OFFSET_ACE3 + XAIUTCR
#define NCORE_OFFSET_ACE3_XAIUTAR	NCORE_OFFSET_ACE3 + XAIUTAR

#define DIE1_NCORE_OFFSET_ACE1_XAIUTCR	DIE1_OFFSET + NCORE_OFFSET_ACE1 + XAIUTCR
#define DIE1_NCORE_OFFSET_ACE1_XAIUTAR	DIE1_OFFSET + NCORE_OFFSET_ACE1 + XAIUTAR
#define DIE1_NCORE_OFFSET_ACE3_XAIUTCR	DIE1_OFFSET + NCORE_OFFSET_ACE3 + XAIUTCR
#define DIE1_NCORE_OFFSET_ACE3_XAIUTAR	DIE1_OFFSET + NCORE_OFFSET_ACE3 + XAIUTAR

#define MEM32(addr) *((volatile unsigned int *)(addr))
static void wr(u64 addr, u32 data)
{
    MEM32(addr) = data;
}

static u32 rd(u64 addr)
{
	u32 data;
    data = MEM32(addr);
    return data;
}
static void wait_cluster1_co_attach(void)
{
    wr(NCORE_OFFSET_ACE1_XAIUTCR,0x1<<9);
    while(!(rd(NCORE_OFFSET_ACE1_XAIUTAR)&(0x1<<5))){;}

    if (nr_dies >= 2) {
	    wr(DIE1_NCORE_OFFSET_ACE3_XAIUTCR,0x1<<9);
	    while(!(rd(DIE1_NCORE_OFFSET_ACE3_XAIUTAR)&(0x1<<5))){;}
    }
}

static void wait_cluster3_co_attach(void)
{
    wr(DIE1_NCORE_OFFSET_ACE1_XAIUTCR,0x1<<9);
    while(!(rd(DIE1_NCORE_OFFSET_ACE1_XAIUTAR)&(0x1<<5))){;}

    wr(NCORE_OFFSET_ACE3_XAIUTCR,0x1<<9);
    while(!(rd(NCORE_OFFSET_ACE3_XAIUTAR)&(0x1<<5))){;}
}

/* only reset DIE1 first c908 */
static void reset_die1_boot_hart(void *control_reg_addr)
{
	unsigned int val;

	val = readl(control_reg_addr);
	/* assert */
	val &= ~(1 << 1);
	val |= 0x1;
	writel(val, control_reg_addr);

	/* de-assert */
	val |= (1 << 1);
	writel(val, control_reg_addr);

	return;
}

static void parse_nr_dies(const void *fdt, int nodeoff)
{
	int d2d_offset, len;
	const fdt32_t *val;
	u32 phandle;

	val = fdt_getprop(fdt, nodeoff, "d2d-info", &len);
	if (!val || len < sizeof(fdt32_t)) {
		return;
	}

	phandle = fdt32_to_cpu(*val);
	d2d_offset = fdt_node_offset_by_phandle(fdt, phandle);
	if (d2d_offset < 0) {
		return;
	}

	val = fdt_getprop(fdt, d2d_offset, "nr_dies", &len);
	if (len > 0 && val)
		nr_dies = fdt32_to_cpu(*val);
}

static int imp_reset_init(const void *fdt, int nodeoff,
			  const struct fdt_match *match)
{
	void *p;
	const fdt64_t *val, *val_entry_reg, *val_ctrl_reg;
	const fdt32_t *val_w_entry_cnt, *val_w_ctrl_val;
	int len, len2, i, cnt = 0;
	u32 t, tmp = 0;
	int cluster_cnt = 0;

	parse_nr_dies(fdt, nodeoff);

	/* Delegate plic enable regs for S-mode */
	val = fdt_getprop(fdt, nodeoff, "plic-delegate", &len);
	if (len > 0 && val) {
		p = (void *)(ulong)fdt64_to_cpu(*val);
		writel(BIT(0), p);
	}

	val = fdt_getprop(fdt, nodeoff, "plic1-delegate", &len);
	if (len > 0 && val) {
		p = (void *)(ulong)fdt64_to_cpu(*val);
		writel(BIT(0), p);
	}

	/* Custom reset method for secondary harts */
	/* Cluster reset register list check
	 * entry-reg = <0x00 0x10148040 0x00 0x10148060>;
	 * control-reg = <0x00 0x10144004 0x00 0x10144008>;
	 */
	val_entry_reg = fdt_getprop(fdt, nodeoff, "entry-reg", &len);
	val_ctrl_reg = fdt_getprop(fdt, nodeoff, "control-reg", &len2);

	if (len <= 0 || len2 <= 0 || len != len2 || val_entry_reg == NULL || val_ctrl_reg == NULL ) {
		goto check_err;
	}
	cluster_cnt = len / sizeof(fdt64_t);

	/* Cluster init val list check
	 * entry-cnt = <4 2>;
	 * control-val = <0x1c 0x1c>;
	 */
	val_w_entry_cnt = fdt_getprop(fdt, nodeoff, "entry-cnt", &len);
	val_w_ctrl_val = fdt_getprop(fdt, nodeoff, "control-val", &len2);
	if (len <= 0 || len2 <= 0 || len != len2 || val_w_entry_cnt == NULL || val_w_ctrl_val == NULL ) {
		goto check_err;
	}
	cnt = len / sizeof(fdt32_t);
	if (cluster_cnt != cnt) {
		goto check_err;
	}

	/* Set the reset address of each CPU in each cluster */
	for (cnt = 0; cnt < cluster_cnt; cnt++) {
		p = (void *)(ulong)fdt64_to_cpu(val_entry_reg[cnt]);
		tmp = fdt32_to_cpu(val_w_entry_cnt[cnt]);
		for (i = 0; i < tmp; i++) {
			t = (u32) (core_entry & 0xFFFFFFFF);
			writel(t, p + (8 * i));
			t = (u32)(core_entry >> 32);
			writel(t, p + (8 * i) + 4);
		}
	}

	/* Reset each CPU in each cluster */
	for (cnt = 0; cnt < cluster_cnt; cnt++) {
		/* if tmp == 0, disabled all core */
		tmp = fdt32_to_cpu(val_w_ctrl_val[cnt]);
		if (cnt == 1 && tmp > 1) {
			wait_cluster1_co_attach();
		}

		if (cnt == 3 && tmp > 1) {
			wait_cluster3_co_attach();
		}

		p = (void *)(ulong)fdt64_to_cpu(val_ctrl_reg[cnt]);
		writel(tmp, p);
	}

	/*  Reset die1 HART0 */
	if (cluster_cnt > 2)
		reset_die1_boot_hart((void*)(ulong)(fdt64_to_cpu(val_ctrl_reg[2])));

check_err:
	sbi_system_reset_add_device(&imp_reset);

	return 0;
}

static const struct fdt_match imp_reset_match[] = {
	{ .compatible = "zhihe,reset-sample" },
	{ .compatible = "thead,reset-sample" },
	{ },
};

const struct fdt_driver fdt_reset_zhihe = {
	.match_table = imp_reset_match,
	.init = imp_reset_init
};
