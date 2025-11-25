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

#define DIE0_OFFSET			0x0
#define DIE1_OFFSET			0x2000000000ul
#define NCORE_BASE			0x6f800000
#define ACE1_OFFSET			0x1000
#define ACE3_OFFSET			0x3000
#define XAIUTCR				0x40
#define XAIUTAR				0x44

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

#define CLUSTER_ATTACH(ace, die) do { \
	wr(NCORE_BASE + XAIUTCR + ace + die, 0x1<<9); \
	while(!(rd(NCORE_BASE + XAIUTAR + ace + die) & (0x1 << 5))){;} \
} while(0)

static int get_die_count(const void *fdt, int nodeoff)
{
	int d2d_offset, len;
	const fdt32_t *val;
	u32 phandle;
	int nr_dies = 1;

	val = fdt_getprop(fdt, nodeoff, "d2d-info", &len);
	if (!val || len < sizeof(fdt32_t))
		return nr_dies;

	phandle = fdt32_to_cpu(*val);
	d2d_offset = fdt_node_offset_by_phandle(fdt, phandle);
	if (d2d_offset < 0)
		return nr_dies;

	val = fdt_getprop(fdt, d2d_offset, "nr_dies", &len);
	if (len > 0 && val)
		nr_dies = fdt32_to_cpu(*val);

	return nr_dies;
}

static int imp_reset_init(const void *fdt, int nodeoff,
			  const struct fdt_match *match)
{
	void *p;
	const fdt64_t *val, *entry_reg, *ctrl_reg;
	const fdt32_t *val_w_entry_cnt, *val_w_ctrl_val;
	int len, len2, i, cnt = 0;
	u32 t, entry_cnt, ctrl_val;
	int cluster_cnt = 0;
	int nr_dies;

	nr_dies = get_die_count(fdt, nodeoff);

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
	entry_reg = fdt_getprop(fdt, nodeoff, "entry-reg", &len);
	ctrl_reg = fdt_getprop(fdt, nodeoff, "control-reg", &len2);

	if (len <= 0 || len2 <= 0 || len != len2 || entry_reg == NULL || ctrl_reg == NULL ) {
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
		p = (void *)(ulong)fdt64_to_cpu(entry_reg[cnt]);
		entry_cnt = fdt32_to_cpu(val_w_entry_cnt[cnt]);
		for (i = 0; i < entry_cnt; i++) {
			t = (u32) (core_entry & 0xFFFFFFFF);
			writel(t, p + (8 * i));
			t = (u32)(core_entry >> 32);
			writel(t, p + (8 * i) + 4);
		}
	}

	/* Reset each CPU in each cluster */
	for (cnt = cluster_cnt - 1; cnt >= 0; cnt--) {
		ctrl_val = fdt32_to_cpu(val_w_ctrl_val[cnt]);
		p = (void *)(ulong)fdt64_to_cpu(ctrl_reg[cnt]);

		if (cnt > 0 && ctrl_val > 1) {
			/* deassert cluster firstly */
			writel(0x1, p);
			if (cnt == 1) {
				CLUSTER_ATTACH(ACE1_OFFSET, DIE0_OFFSET);
				if (nr_dies > 1)
					CLUSTER_ATTACH(ACE3_OFFSET, DIE1_OFFSET);
			} else if (cnt == 3) {
				CLUSTER_ATTACH(ACE1_OFFSET, DIE1_OFFSET);
				CLUSTER_ATTACH(ACE3_OFFSET, DIE0_OFFSET);
			}
		}

		writel(ctrl_val, p);
	}

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
