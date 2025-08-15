/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Zhihe Computing Corporation or its affiliates.
 *
 * Authors:
 *   Xuliang Lin <linxuliang@zhcomputing.com>
 */

#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_version.h>
#include <sbi/riscv_asm.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <libfdt.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_bitmap.h>
#include <sbi/riscv_io.h>

#define RWX_PERMISSION       "rwx"
#define RO_PERMISSION        "ro"
#define NO_ACCESS_PERMISSION "no-access"

#define TOR_MODE             "TOR"
#define NAPOT_MODE           "NAPOT"

#define RWX_PERMISSION_VALUE       0x07
#define RO_PERMISSION_VALUE        0x01
#define NO_ACCESS_PERMISSION_VALUE 0x0

#define TOR_MODE_VALUE             0x08
#define NAPOT_MODE_VALUE           0x18

#define MAX_IOPMP_PARAM_SIZE       16
#define DOMAIN_IOPMP_MAX_DFMU      10
#define MAX_DOMAIN_DEVICES_SIZE    128

#define IOPMP_ENTRY_RESOURCE_MAX   (64 * 4)

#define IOPMP_DEV_IOMMU_START      0x40
#define NO_ENTRY                   -1

enum sbi_ext_config_iopmp_fid {
	SBI_EXT_CONFIG_IOPMP_ADD_RULE = 0,
	SBI_EXT_CONFIG_IOPMP_REMOVE_RULE,
};

struct iopmp_region_t {
	uint64_t addr_start;
	uint64_t addr_range;
	uint8_t permission;
	uint8_t mode;
	uint8_t entries[2];
	uint8_t entry_count;
};

struct device_iopmps_t {
	uint32_t global;
	uint8_t device_id;
	bool is_configured;
	struct iopmp_dfmu_t *dfmu;
	uint8_t region_count;
	struct iopmp_region_t regions[MAX_IOPMP_PARAM_SIZE];
};

struct iopmp_dfmu_t {
	const char *reg_name;
	uint64_t reg_start;
	uint64_t size;
	DECLARE_BITMAP(entry_bitmap, IOPMP_ENTRY_RESOURCE_MAX);
};

struct iopmp_list_t {
	struct device_iopmps_t device_iopmps[MAX_DOMAIN_DEVICES_SIZE];
	struct iopmp_dfmu_t dfmus[DOMAIN_IOPMP_MAX_DFMU];
};

static struct iopmp_list_t iopmp_list = {0};

//#define IOMPM_CONFIG_DEBUG
#ifdef IOMPM_CONFIG_DEBUG
static void iopmp_debug()
{
	sbi_printf("%s: iopmp_list: \n", __func__);
	for (uint8_t device_index=0; device_index < MAX_DOMAIN_DEVICES_SIZE; device_index++) {
		struct device_iopmps_t *device_iopmps = iopmp_list.device_iopmps + device_index;
		if (device_iopmps->region_count > 0) {
			sbi_printf("   device id: %d\n", device_iopmps->device_id);
			sbi_printf("   dfmu reg name : %s\n", device_iopmps->dfmu->reg_name);
			sbi_printf("   dfmu reg addr : 0x%lx\n", device_iopmps->dfmu->reg_start);
			sbi_printf("   is_configured : %d\n", device_iopmps->is_configured);
			sbi_printf("   region_count: %d\n", device_iopmps->region_count);
			for (uint8_t iopmp_index = 0; iopmp_index < device_iopmps->region_count; iopmp_index++) {
				struct iopmp_region_t *iopmp_region = device_iopmps->regions + iopmp_index;
				sbi_printf("	  iopmp region address:	0x%lx\n", iopmp_region->addr_start);
				sbi_printf("	  iopmp region range:	  0x%lx\n", iopmp_region->addr_range);
				sbi_printf("	  iopmp region permission: 0x%x\n", iopmp_region->permission);
				sbi_printf("	  iopmp region mode:	   0x%x\n", iopmp_region->mode);
				sbi_printf("	  entry count: %d\n", iopmp_region->entry_count);
				for(uint8_t entry_index = 0; entry_index < iopmp_region->entry_count; entry_index++)
					sbi_printf("		entry index: %d\n", iopmp_region->entries[entry_index]);
			}
		}
	}
}
#endif

static int iopmp_permession_convert(const char* permission, struct iopmp_region_t *regions)
{
	if (strcmp(permission, RWX_PERMISSION) == 0)
		regions->permission = RWX_PERMISSION_VALUE;
	else if (strcmp(permission, RO_PERMISSION) == 0)
		regions->permission = RO_PERMISSION_VALUE;
	else if (strcmp(permission, NO_ACCESS_PERMISSION) == 0)
		regions->permission = NO_ACCESS_PERMISSION_VALUE;
	else
		return -1;

	return 0;
}

static int iopmp_mode_convert(const char* mode, struct iopmp_region_t *regions)
{
	if (strcmp(mode, TOR_MODE) == 0)
		regions->mode = TOR_MODE_VALUE;
	else if (strcmp(mode, NAPOT_MODE) == 0)
		regions->mode = NAPOT_MODE_VALUE;
	else
		return -1;

	return 0;
}

static int iopmp_entry_alloc(struct iopmp_dfmu_t *dfmu)
{
	for (int i = 0; i < IOPMP_ENTRY_RESOURCE_MAX; i++) {
		if (!bitmap_test(dfmu->entry_bitmap, i)) {
			bitmap_set(dfmu->entry_bitmap, i, 1);
			return i;
		}
	}
	return -1;
}

static void iopmp_entry_free(struct iopmp_dfmu_t *dfmu, int entry_index)
{
	if (entry_index >= 0)
		bitmap_clear(dfmu->entry_bitmap, entry_index, 1);
}

static void iopmp_entry_set(struct device_iopmps_t *device_iopmp, uint32_t entry_index, uint32_t cfg_value, uint64_t u64_addr)
{
	uint32_t channel = entry_index >> 2;
	uint32_t cfg_index = entry_index & 0x3;
	struct iopmp_dfmu_t *dfmu = device_iopmp->dfmu;
	uint32_t iopmp_cfg;
	uint32_t high_addr = u64_addr >> 32;
	uint32_t low_addr = u64_addr & 0x00000000ffffffff;

	//udpate iopmpcfg
	iopmp_cfg = readl((void *)(dfmu->reg_start + channel * 4));
	iopmp_cfg = iopmp_cfg & ~(0xff << (cfg_index * 8));
	iopmp_cfg = iopmp_cfg | (cfg_value << (cfg_index * 8));
	writel(iopmp_cfg, (void *)(dfmu->reg_start + channel * 4));

	//set iopmpaddr
	writel(low_addr, (void *)(dfmu->reg_start+ 0x800 + channel * 0x20 + cfg_index * 0x8));
	writel(high_addr | (device_iopmp->global << 31) | (device_iopmp->device_id << 14), (void *)(dfmu->reg_start + 0x804 + channel * 0x20 + cfg_index * 0x8));
}

static void iopmp_entry_reset(struct device_iopmps_t *device_iopmp, uint32_t entry_index)
{
	uint32_t channel = entry_index >> 2;
	uint32_t cfg_index = entry_index & 0x3;
	struct iopmp_dfmu_t *dfmu = device_iopmp->dfmu;
	uint32_t iopmp_cfg;

	//clear iopmpcfg
	iopmp_cfg = readl((void *)(dfmu->reg_start + channel * 4));
	iopmp_cfg = iopmp_cfg & ~(0xff << (cfg_index * 8));
	writel(0x0, (void *)(dfmu->reg_start + channel * 4));

	//set iopmpaddr
	writel(0x0, (void *)(dfmu->reg_start+ 0x800 + channel * 0x20 + cfg_index * 0x8));
	writel(0x0, (void *)(dfmu->reg_start + 0x804 + channel * 0x20 + cfg_index * 0x8));
}

static int iopmp_region_enable(struct device_iopmps_t *device_iopmp, struct iopmp_region_t *region)
{
	struct iopmp_dfmu_t *dfmu = device_iopmp->dfmu;
	uint32_t cfg_value = 0;
	uint64_t u64_addr;

	if (region->mode == TOR_MODE_VALUE) {
		// allocate two entries
		region->entries[0] = iopmp_entry_alloc(dfmu);
		region->entries[1] = iopmp_entry_alloc(dfmu);
		if (region->entries[0] == -1 || region->entries[1] == -1) {
			iopmp_entry_free(dfmu, region->entries[0]);
			iopmp_entry_free(dfmu, region->entries[1]);
			return NO_ENTRY;
		}
		region->entry_count = 2;

		cfg_value = RWX_PERMISSION_VALUE | region->mode;
		u64_addr = (region->addr_start >> 2);
		iopmp_entry_set(device_iopmp, region->entries[0], cfg_value, u64_addr);

		cfg_value = region->permission | region->mode;
		u64_addr = (region->addr_start + region->addr_range) >> 2;
		iopmp_entry_set(device_iopmp, region->entries[1], cfg_value, u64_addr);
	}
	else if (region->mode == NAPOT_MODE_VALUE) {
		// allocate one entry
		region->entries[0] = iopmp_entry_alloc(dfmu);
		if (region->entries[0] == -1) {
			return NO_ENTRY;
		}
		region->entry_count = 1;

		cfg_value = region->permission | region->mode;
		u64_addr = ((region->addr_start >> 2) | ((region->addr_range - 1) >> 3));
		iopmp_entry_set(device_iopmp, region->entries[0], cfg_value, u64_addr);
	}

	return 0;
}

static void iopmp_region_disable(struct device_iopmps_t *device_iopmp, struct iopmp_region_t *region)
{
	struct iopmp_dfmu_t *dfmu = device_iopmp->dfmu;
	for (int i = 0; i < region->entry_count; i++) {
		iopmp_entry_reset(device_iopmp, region->entries[i]);
		iopmp_entry_free(dfmu, region->entries[i]);
	}
	region->entry_count = 0;
}

static int iopmp_device_disable(unsigned long device_id)
{
	struct device_iopmps_t *device_iopmp = iopmp_list.device_iopmps + device_id;

	if (device_iopmp->is_configured == false)
		return 0;

	for (int i = 0; i < device_iopmp->region_count; i++) {
		struct iopmp_region_t *region = device_iopmp->regions + i;
		iopmp_region_disable(device_iopmp, region);
	}

	device_iopmp->is_configured = false;
	return 0;
}

static int iopmp_device_enable(unsigned long device_id)
{
	int ret = 0;
	struct device_iopmps_t *device_iopmp = iopmp_list.device_iopmps + device_id;

	if (device_iopmp->is_configured == true)
		return 0;

	for (int i = 0; i < device_iopmp->region_count; i++) {
		struct iopmp_region_t *region = device_iopmp->regions + i;
		ret = iopmp_region_enable(device_iopmp, region);
		if (ret != 0) {
			goto failed;
		}
	}
	device_iopmp->is_configured = true;
	return 0;

failed:
	iopmp_device_disable(device_id);
	return ret;
}

static int iopmp_region_parse(const void *fdt, int offset, struct iopmp_region_t *regions)
{
	int len;

	// get permission
	const char *perm = fdt_getprop(fdt, offset, "permission", &len);
	if (perm == NULL || iopmp_permession_convert(perm, regions)) {
		sbi_printf("%s: parse mode: failed\n", __func__);
		return -1;
	}

	// get mode
	const char *mode = fdt_getprop(fdt, offset, "mode", &len);
	if (mode == NULL || iopmp_mode_convert(mode, regions)) {
		sbi_printf("%s: parse mode: failed\n", __func__);
		return -1;
	}

	//get reg
	int ret = fdt_get_node_addr_size(fdt, offset, 0, &regions->addr_start, &regions->addr_range);
	if (ret) {
		sbi_printf("%s: reg property not found, ret:%d\n",__func__, ret);
		return -1;
	}

	return 0;
}

static int iopmp_device_parse(const void *fdt, int offset, struct device_iopmps_t *device_iopmps)
{
	int len;
	const fdt32_t *prop;

	//parse iopmp-regions
	prop = fdt_getprop(fdt, offset, "iopmp-regions", &len);
	if (!prop) {
		sbi_printf("%s: No iopmp-regions property found\n", __func__);
		return -1;
	}

	if (len % sizeof(uint32_t) != 0) {
		sbi_printf("Invalid formed iopmp-regions property (length %d)\n", len);
		return -1;
	}

	int num_regions = len / sizeof(uint32_t);

	for (int i = 0; i < num_regions; i++) {
		uint32_t phandle = fdt32_to_cpu(prop[i]);
		int region_node = fdt_node_offset_by_phandle(fdt, phandle);
		if (region_node >= 0) {
			// 解析每个region的具体属性
			if(iopmp_region_parse(fdt, region_node, &(device_iopmps->regions[i])) == 0)
				device_iopmps->region_count++;
			else
				return -1;
		}
		else {
			sbi_printf("%s: [%d] invalid phandle (0x%x): %s\n", __func__, i, phandle, fdt_strerror(region_node));
			return -1;
		}
	}
	return 0;
}

static struct iopmp_dfmu_t *dfmu_data_get(const char* reg_name)
{
	for (int i = 0; i < DOMAIN_IOPMP_MAX_DFMU; i++) {
		if (strcmp(reg_name, iopmp_list.dfmus[i].reg_name) == 0)
			return &iopmp_list.dfmus[i];
	}

	return NULL;
}

static struct iopmp_dfmu_t *dfmu_data_parse(const void *fdt, int offset, const char* reg_name)
{
	for (int i=0; i < DOMAIN_IOPMP_MAX_DFMU; i++) {
		struct iopmp_dfmu_t *dfmu = &iopmp_list.dfmus[i];
		if (dfmu->reg_name == NULL) {
			int ret = fdt_get_node_addr_size_by_name(fdt, offset, reg_name, &dfmu->reg_start, &dfmu->size);
			if (ret != 0) {
				sbi_printf("%s: get %s addr and size failed\n", __func__, reg_name);
				return NULL;
			}
			dfmu->reg_name = reg_name;
			return dfmu;
		}
	}

	sbi_printf("%s: have no valid dfmu resource to use\n", __func__);
	return NULL;
}

static int iopmp_devices_parse(struct iopmp_list_t *list)
{
	const void *fdt;
	int offset, contrl_offset;

	fdt = fdt_get_address();
	if (fdt == NULL) {
		sbi_printf("%s: fdt get address failed\n",__func__);
		return -1;
	}

	offset = fdt_path_offset(fdt, "/soc/iopmp_devices");

	if (offset < 0) {
		sbi_printf("%s: /soc/iopmp_devices not found\n", __func__);
		return -1;
	}

	contrl_offset = fdt_path_offset(fdt, "/soc/iopmp-controller");
	if (contrl_offset < 0) {
		sbi_printf("%s: not find /soc/iopmp-controller node\n", __func__);
		return -1;
	}

	int dfmu_node;
	fdt_for_each_subnode(dfmu_node, fdt, offset) {
		//get iopmp-name
		int len;
		const char *iopmp_name = fdt_getprop(fdt, dfmu_node, "iopmp-name", &len);
		if (iopmp_name == NULL) {
			sbi_printf("%s: parse iopmp-name: failed\n", __func__);
			return -1;
		}

		//get device id
		const fdt32_t *device_prob = fdt_getprop(fdt, dfmu_node, "device-id", &len);
		if (!device_prob || len != 4) {
			sbi_printf("%s,Error getting device id property\n", __func__);
			return -1;
		}
		uint32_t device_id = fdt32_to_cpu(*device_prob);
		if (device_id >= MAX_DOMAIN_DEVICES_SIZE) {
			sbi_printf("%s: device id %d is invalid.\n", __func__, device_id);
			return -1;
		}

		if (iopmp_device_parse(fdt, dfmu_node, &(list->device_iopmps[device_id]))) {
			sbi_printf("%s: parse device %d regions failed.\n", __func__, device_id);
			return -1;
		}

		//set dfmu index
		struct iopmp_dfmu_t *dfmu = dfmu_data_get(iopmp_name);
		if (dfmu == NULL){
			//not found, create new dfmu
			dfmu = dfmu_data_parse(fdt, contrl_offset, iopmp_name);
			if (dfmu == NULL) {
				sbi_printf("%s: iopmp-name %s parse dfmu data failed.\n", __func__, iopmp_name);
				return -1;
			}
		}

		list->device_iopmps[device_id].dfmu = dfmu;

		//set device id
		if (device_id >= IOPMP_DEV_IOMMU_START)
			//iommu devcie
			list->device_iopmps[device_id].device_id = 0;
		else
			list->device_iopmps[device_id].device_id = device_id;

		//global default 0
		const fdt32_t *global_prop = fdt_getprop(fdt, offset, "global", &len);
		if (global_prop && len == 4) {
			list->device_iopmps[device_id].global = fdt32_to_cpu(*global_prop);
		}
	}

	return 0;
}

static int sbi_iopmp_handler(unsigned long extid,
							 unsigned long funcid,
							 struct sbi_trap_regs *regs,
							 struct sbi_ecall_return *out)
{
	unsigned long count = regs->a0;

	count = MIN(count, 5);

	unsigned long *devices_ids =&regs->a1;
	int (*iopmp_device_config_func)(unsigned long device_id);

	if (funcid == SBI_EXT_CONFIG_IOPMP_ADD_RULE)
		iopmp_device_config_func = iopmp_device_enable;
	else if (funcid == SBI_EXT_CONFIG_IOPMP_REMOVE_RULE)
		iopmp_device_config_func = iopmp_device_disable;
	else {
		return SBI_ERR_INVALID_PARAM;
	}

	for (int i = 0; i < count; i++) {
		int ret = iopmp_device_config_func(devices_ids[i]);
		if (ret != 0) {
			sbi_printf("%s: funcid = %ld device = %ld ret = %d .\n", __func__, funcid, devices_ids[i], ret);
			return SBI_ERR_FAILED;
		}
	}

#ifdef IOMPM_CONFIG_DEBUG
	sbi_printf("%s: debug infor after configured\n", __func__);
	iopmp_debug();
#endif
	return SBI_SUCCESS;
}

struct sbi_ecall_extension ecall_zh_iopmp;

static int sbi_ecall_iopmp_register_extensions(void)
{
	if (iopmp_devices_parse(&iopmp_list)) {
		return 0;
	}

	return sbi_ecall_register_extension(&ecall_zh_iopmp);
}

struct sbi_ecall_extension ecall_zh_iopmp = {
	.extid_start = SBI_EXT_CONFIG_IOPMP,
	.extid_end = SBI_EXT_CONFIG_IOPMP,
	.register_extensions = sbi_ecall_iopmp_register_extensions,
	.handle = sbi_iopmp_handler,
};
