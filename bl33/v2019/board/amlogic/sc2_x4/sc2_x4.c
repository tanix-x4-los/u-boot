// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <common.h>
#include <command.h>
#include <amlogic/x4_boot.h>
#include <mmc.h>
#include <part.h>
#include <u-boot/crc.h>
#include <asm/io.h>
#include <malloc.h>
#include <memalign.h>
#include <partition_table.h>
#include <emmc_partitions.h>
#include <errno.h>
#include <environment.h>
#include <fdt_support.h>
#include <linux/libfdt.h>
#include <amlogic/cpu_id.h>
#include <asm/arch/secure_apb.h>
#include <asm/arch/pinctrl_init.h>
#include <linux/sizes.h>
#include <asm-generic/gpio.h>
#include <dm.h>
#include <asm/armv8/mmu.h>
#include <amlogic/aml_v3_burning.h>
#include <amlogic/aml_v2_burning.h>
#include <linux/mtd/partitions.h>
#include <asm/arch/bl31_apis.h>
#ifdef CONFIG_AML_VPU
#include <amlogic/media/vpu/vpu.h>
#endif
#ifdef CONFIG_AML_VPP
#include <amlogic/media/vpp/vpp.h>
#endif
#ifdef CONFIG_AML_HDMITX20
#include <amlogic/media/vout/hdmitx/hdmitx_ext.h>
#endif
#ifdef CONFIG_AML_CVBS
#include <amlogic/media/vout/aml_cvbs.h>
#endif

#include "avb2_kpub.c"

DECLARE_GLOBAL_DATA_PTR;

void sys_led_init(void)
{
}

int serial_set_pin_port(unsigned long port_base)
{
    return 0;
}

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
	return 0;
}

/* secondary_boot_func
 * this function should be write with asm, here, is is only for compiling pass
 * */
void secondary_boot_func(void)
{
}

int board_eth_init(bd_t *bis)
{
	return 0;
}

int active_clk(void)
{
	struct udevice *clk = NULL;
	int err;

	err = uclass_get_device_by_name(UCLASS_CLK,
			"xtal-clk", &clk);
	if (err) {
		pr_err("Can't find xtal-clk clock (%d)\n", err);
		return err;
	}
	err = uclass_get_device_by_name(UCLASS_CLK,
			"clock-controller@0", &clk);
	if (err) {
		pr_err("Can't find clock-controller@0 clock (%d)\n", err);
		return err;
	}

	return 0;
}

#ifdef CONFIG_AML_HDMITX20
static void hdmitx_set_hdmi_5v(void)
{
	/*Power on VCC_5V for HDMI_5V*/
}
#endif

static const char ddr_type_info[6][8] =
{
	"DDR3\0",       //CONFIG_DDR_TYPE_DDR3			//0
	"DDR4\0",       //CONFIG_DDR_TYPE_DDR4			//1
	"LPDDR4\0",     //CONFIG_DDR_TYPE_LPDDR4		//2
	"LPDDR3\0",     //CONFIG_DDR_TYPE_LPDDR3		//3
	"LPDDR2\0",     //CONFIG_DDR_TYPE_LPDDR2		//4
	"LPDDR4X\0",    //CONFIG_DDR_TYPE_LPDDR4X		//5
};

void board_init_mem(void) {
	#if 1
	/* config bootm low size, make sure whole dram/psram space can be used */
	phys_size_t ram_size;
	unsigned int ddr_type;
	char *env_tmp;
	env_tmp = env_get("bootm_size");
	if (!env_tmp) {
		ram_size = (((readl(SYSCTRL_SEC_STATUS_REG4)) & 0xFFF80000) << 4);
		env_set_hex("bootm_low", 0);
		env_set_hex("bootm_size", ram_size);
	}
	env_tmp = env_get("boot_ddr_type");
	if (!env_tmp) {
		ddr_type = (((readl(SYSCTRL_SEC_STATUS_REG4)) & 0x00070000) >> 16);
		env_set("boot_ddr_type", 0);
		env_set("boot_ddr_type", ddr_type_info[ddr_type]);
	}
	#endif
}

int board_init(void)
{
	printf("board init\n");

	/* The non-secure watchdog is enabled in BL2 TEE, disable it */
	run_command("watchdog off", 0);
	printf("watchdog disable\n");

	aml_set_bootsequence(0);

	#if 0
	active_clk();
	#endif
	pinctrl_devices_active(PIN_CONTROLLER_NUM);
	run_command("gpio clr GPIOH_8", 0);
#ifdef CONFIG_AML_HDMITX20
	hdmitx_set_hdmi_5v();
	hdmitx_init();
#endif

	//wifi reset
	run_command("gpio c GPIOX_6", 0);
	printf("wifi chip en down\n");
	mdelay(100);
	run_command("gpio s GPIOX_6", 0);
	printf("wifi chip en up\n");

	return 0;
}

/*
 * The Amlogic map has no per-partition UUID, so a GPT write stops at
 * "invalid guid". Fill stable ids and publish a primary table that
 * matches the live eMMC map. The user-area bootloader starts at LBA 0,
 * which is the GPT header, so that one entry is left out. The ROM
 * boots from the hardware boot areas, not from this partition.
 * No backup header: that would overwrite the tail of userdata.
 */
static void x4_part_uuid(const char *name, char *uuid, int len)
{
	u32 h = 2166136261u;
	const unsigned char *p;

	for (p = (const unsigned char *)name; *p; p++)
		h = (h ^ *p) * 16777619u;
	snprintf(uuid, len, "6f%06x-a4b0-4c2d-8e1f-%012x",
		 h & 0xffffff, h);
}

static int x4_write_primary_gpt(struct blk_desc *dev_desc,
				     disk_partition_t *parts, int n)
{
	gpt_header *gpt_h = NULL;
	gpt_entry *gpt_e = NULL;
	int ret = -1;
	int hsize, esize;
	lbaint_t pte_blks;
	char disk_guid[] = "a0453c8e-6c14-4b6e-9c1a-0000c0a4a4a4";
	ALLOC_CACHE_ALIGN_BUFFER_PAD(legacy_mbr, p_mbr, 1, dev_desc->blksz);

	hsize = PAD_TO_BLOCKSIZE(sizeof(*gpt_h), dev_desc);
	gpt_h = malloc_cache_aligned(hsize);
	esize = PAD_TO_BLOCKSIZE(GPT_ENTRY_NUMBERS * sizeof(*gpt_e), dev_desc);
	gpt_e = malloc_cache_aligned(esize);
	if (!gpt_h || !gpt_e)
		goto out;
	memset(gpt_h, 0, hsize);
	memset(gpt_e, 0, esize);

	if (gpt_fill_header(dev_desc, gpt_h, disk_guid, n))
		goto out;
	/* Cover the whole user area. There is no backup header at the end. */
	gpt_h->last_usable_lba = cpu_to_le64(dev_desc->lba - 1);
	if (gpt_fill_pte(dev_desc, gpt_h, gpt_e, parts, n))
		goto out;

	if (blk_dread(dev_desc, 0, 1, p_mbr) != 1)
		goto out;
	memset((char *)p_mbr + MSDOS_MBR_BOOT_CODE_SIZE, 0,
	       sizeof(*p_mbr) - MSDOS_MBR_BOOT_CODE_SIZE);
	p_mbr->signature = MSDOS_MBR_SIGNATURE;
	p_mbr->partition_record[0].sys_ind = EFI_PMBR_OSTYPE_EFI_GPT;
	p_mbr->partition_record[0].start_sect = 1;
	p_mbr->partition_record[0].nr_sects = (u32)dev_desc->lba - 1;

	gpt_h->partition_entry_array_crc32 = cpu_to_le32(
		crc32(0, (unsigned char *)gpt_e,
		      le32_to_cpu(gpt_h->num_partition_entries) *
		      le32_to_cpu(gpt_h->sizeof_partition_entry)));
	gpt_h->header_crc32 = 0;
	gpt_h->header_crc32 = cpu_to_le32(
		crc32(0, (unsigned char *)gpt_h,
		      le32_to_cpu(gpt_h->header_size)));

	pte_blks = BLOCK_CNT(le32_to_cpu(gpt_h->num_partition_entries) *
			     sizeof(gpt_entry), dev_desc);
	if (blk_dwrite(dev_desc, 0, 1, p_mbr) != 1 ||
	    blk_dwrite(dev_desc, 1, 1, gpt_h) != 1 ||
	    blk_dwrite(dev_desc,
		       (lbaint_t)le64_to_cpu(gpt_h->partition_entry_lba),
		       pte_blks, gpt_e) != pte_blks)
		goto out;
	ret = 0;
out:
	free(gpt_e);
	free(gpt_h);
	return ret;
}

static int x4_publish_gpt(void)
{
	struct blk_desc *dev_desc;
	struct partitions *pt;
	disk_partition_t *parts;
	gpt_entry *pte = NULL;
	struct mmc *mmc;
	int i, j, n, count, ret = CMD_RET_FAILURE;
	lbaint_t last;

	ALLOC_CACHE_ALIGN_BUFFER_PAD(gpt_header, gpt_head, 1, 512);

	mmc = find_mmc_device(CONFIG_FASTBOOT_FLASH_MMC_DEV);
	if (!mmc || mmc_init(mmc))
		return CMD_RET_FAILURE;
	dev_desc = mmc_get_blk_desc(mmc);
	if (!dev_desc || dev_desc->blksz != 512 || dev_desc->hwpart != 0 ||
	    dev_desc->lba < 0x100000 || dev_desc->lba > 0xffffffffULL)
		return CMD_RET_FAILURE;

	if (is_gpt_valid(dev_desc, GPT_PRIMARY_PARTITION_TABLE_LBA,
			 gpt_head, &pte) == 1) {
		free(pte);
		puts("GPT already valid; leaving it unchanged\n");
		return CMD_RET_SUCCESS;
	}

	pt = aml_ept_table(&count);
	if (!pt || count < 1 || count > GPT_ENTRY_NUMBERS)
		return CMD_RET_FAILURE;

	parts = calloc(count, sizeof(*parts));
	if (!parts)
		return CMD_RET_FAILURE;

	last = dev_desc->lba - 1;
	for (i = 0, n = 0; i < count; i++) {
		lbaint_t start = pt[i].offset / dev_desc->blksz;
		lbaint_t size;

		/* Disabled zero-length entries (stock cache) have no GPT extent. */
		if (!pt[i].name[0] || !pt[i].size)
			continue;
		if (!memchr(pt[i].name, 0, sizeof(pt[i].name)))
			goto out_parts;
		/* LBA 0-33 belong to the protective MBR, header and entries. */
		if (start < 34) {
			if (strcmp(pt[i].name, "bootloader"))
				goto out_parts;
			printf("gpt: skip %s at LBA %lu\n", pt[i].name,
			       (unsigned long)start);
			continue;
		}
		if (pt[i].offset % dev_desc->blksz || start > last ||
		    !memchr(pt[i].name, 0, sizeof(pt[i].name)))
			goto out_parts;
		if (pt[i].size == (uint64_t)-1)
			size = last + 1 - start;
		else {
			if (pt[i].size % dev_desc->blksz)
				goto out_parts;
			size = pt[i].size / dev_desc->blksz;
		}
		if (!size || size > last + 1 - start)
			goto out_parts;
		for (j = 0; j < n; j++) {
			if (!strcmp(pt[i].name, (char *)parts[j].name) ||
			    (start < parts[j].start + parts[j].size &&
			     parts[j].start < start + size))
				goto out_parts;
		}

		parts[n].start = start;
		parts[n].size = size;
		parts[n].blksz = dev_desc->blksz;
		strncpy((char *)parts[n].name, pt[i].name,
			sizeof(parts[n].name) - 1);
#if CONFIG_IS_ENABLED(PARTITION_UUIDS)
		x4_part_uuid(pt[i].name, parts[n].uuid,
				 sizeof(parts[n].uuid));
#endif
		n++;
	}

	if (n > 0 && !x4_write_primary_gpt(dev_desc, parts, n)) {
		printf("gpt: published %d entries\n", n);
		ret = CMD_RET_SUCCESS;
	}
	else
		printf("gpt: not published (%d entries)\n", n);
out_parts:
	free(parts);
	return ret;
}

/* Explicit provisioning only: primary GPT destroys the user-area BL2. */
static int do_x4_gpt(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	u32 status = readl(SYSCTRL_SEC_STATUS_REG2);
	u32 source = (status >> 4) & 0xf;
	u32 copy = status & 0xf;

	if (argc != 2 || strcmp(argv[1], "publish"))
		return CMD_RET_USAGE;
	if (source != BOOT_ID_EMMC || (copy != 1 && copy != 2)) {
		puts("GPT requires a verified eMMC hardware-partition boot\n");
		return CMD_RET_FAILURE;
	}
	return x4_publish_gpt();
}

U_BOOT_CMD(x4_gpt, 2, 0, do_x4_gpt,
	   "publish primary GPT from the existing Amlogic partition map",
	   "publish - requires hardware boot copy 1 or 2; preserves partition data");

int board_late_init(void)
{
	/* Boot scripts must come from this build, not a saved factory environment.
	 * Display preferences and identifiers remain available from storage.
	 * This changes RAM environment only; never save it during startup.
	 */
	char *boot_vars[] = {
		"board", "bootcmd", "bootdelay", "preboot", "initargs", "storeboot",
		"usb_burning",
		"bcb_cmd", "bcb_cmd_base", "storeargs", "storeargs_base",
		"switch_bootmode", "update", "update_base", "enter_fastboot",
		"common_dtb_load", "loadaddr_kernel", "dtb_mem_addr",
		"storage_param", "storage_param_base", "cmdline_keys",
		"cmdline_keys_base", "upgrade_check", "init_display",
		"init_display_base", "recovery_from_flash", "recovery_from_flash_base",
		"recovery_from_sdcard", "recovery_from_sdcard_base",
		"recovery_from_udisk", "recovery_from_udisk_base",
		"recovery_from_fat_dev", "recovery_from_fat_dev_base"
	};
	unsigned char chipid[16];
	char chipid_str[25];
	int i;

	set_default_vars(ARRAY_SIZE(boot_vars), boot_vars, 0);
	board_init_mem();
	env_set("board", "x4");
	env_set("reboot_status", "reboot_init");
	env_set("write_boot", "0");
	env_set("update_env", "0");
	env_set("lock", "10100000");
	env_set("recovery_mode", "false");
	env_set("vout_init", "disable");

	run_command("get_avb_mode", 0);
	if (run_command("get_valid_slot", 0)) {
		puts("X4: slot selection failed; use fastboot recovery\n");
		env_set("bootcmd", "run enter_fastboot");
	}
	if (env_get("active_slot") && strcmp(env_get("active_slot"), "normal"))
		env_set("common_dtb_load", "imgread dtb ${boot_part} ${dtb_mem_addr}");
	printf("X4: slot %s, boot=%s, vendor_boot=%s\n",
	       env_get("active_slot"), env_get("boot_part"),
	       env_get("vendor_boot_part"));

	if (get_chip_id(chipid, sizeof(chipid)) != -1) {
		for (i = 0; i < 12; i++)
			sprintf(chipid_str + 2 * i, "%02x", chipid[15 - i]);
		env_set("cpu_id", chipid_str);
	}

#ifndef CONFIG_SYSTEM_RTOS
	if (run_command("run common_dtb_load", 0))
		puts("X4: no OS DTB loaded; U-Boot recovery remains available\n");
#endif
#ifdef CONFIG_AML_VPU
	vpu_probe();
#endif
#ifdef CONFIG_AML_VPP
	vpp_init();
#endif
#ifdef CONFIG_AML_CVBS
	cvbs_init();
#endif
	/* The same production binary can be validated via USB without starting
	 * an OS or changing persistent data. eMMC boots retain normal autoboot.
	 */
	if (x4_usb_ram_boot()) {
		env_set("boot_source", "usb");
		env_set("bootdelay", "-1");
		env_set("preboot", "echo X4 USB RAM validation: MMC writes blocked, autoboot stopped");
	} else {
		env_set("boot_source", "storage");
	}
	return 0;
}

unsigned int get_ddr_memsize(void)
{
	unsigned int ddr_size;
#if 0
	/*if soc don't support automatic get ddr size,
	  then it get ddr size with software method*/
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		ddr_size += gd->bd->bi_dram[i].size;
	}
#if defined(CONFIG_SYS_MEM_TOP_HIDE)
	ddr_size += CONFIG_SYS_MEM_TOP_HIDE;
#endif
#else
	/*auto get ddr size from hardware method*/
	ddr_size = ((readl(SYSCTRL_SEC_STATUS_REG4)) & 0xFFF80000) << 4;
#endif
	return ddr_size;
}

phys_size_t get_effective_memsize(void)
{
	// >>16 -> MB, <<20 -> real size, so >>16<<20 = <<4
#if defined(CONFIG_SYS_MEM_TOP_HIDE)
	return (((readl(SYSCTRL_SEC_STATUS_REG4)) & 0xFFF80000) << 4) - CONFIG_SYS_MEM_TOP_HIDE;
#else
	return (((readl(SYSCTRL_SEC_STATUS_REG4)) & 0xFFF80000) << 4);
#endif /* CONFIG_SYS_MEM_TOP_HIDE */

}

static struct mm_region bd_mem_map[] = {
	{
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		.virt = 0xf1000000UL,
		.phys = 0xf1000000UL,
		.size = 0x0f000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = bd_mem_map;

int mach_cpu_init(void) {
	unsigned int nddrSize = ((readl(SYSCTRL_SEC_STATUS_REG4)) & 0xFFF80000) << 4;

	/*
	 * Stock BL2E reports 3856MiB (0xf1000000). Default map is only
	 * 2GiB RAM + device above that; reloc lands at ~0xf0659000 and
	 * aborts if this is not updated before caches/MMU come on.
	 */
	if (nddrSize < CONFIG_1G_SIZE || nddrSize > CONFIG_DDR_MAX_SIZE)
		nddrSize = CONFIG_DDR_MAX_SIZE;
	bd_mem_map[0].size = nddrSize;

	/* BL2 TEE arms the non-secure WDT; kill it before reloc. */
	clrbits_le32(RESETCTRL_WATCHDOG_CTRL0, BIT(18));

	return 0;
}

int ft_board_setup(void *blob, bd_t *bd)
{
	/* eg: bl31/32 rsv */
	return 0;
}

/* partition table for spinor flash */
#ifdef CONFIG_SPI_FLASH
static const struct mtd_partition spiflash_partitions[] = {
	{
		.name = "env",
		.offset = 0,
		.size = 1 * SZ_256K,
	},
	{
		.name = "dtb",
		.offset = 0,
		.size = 1 * SZ_256K,
	},
	{
		.name = "boot",
		.offset = 0,
		.size = 1 * SZ_1M,
	},
	/* last partition get the rest capacity */
	{
		.name = "user",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
	}
};

const struct mtd_partition *get_spiflash_partition_table(int *partitions)
{
	*partitions = ARRAY_SIZE(spiflash_partitions);
	return spiflash_partitions;
}

uint64_t spiflash_bootloader_size(void)
{
	return 3 * SZ_1M;
}
#endif /* CONFIG_SPI_FLASH */

#ifdef CONFIG_MESON_NFC
static struct mtd_partition normal_partition_info[] = {
{
	.name = BOOT_BL2E,
	.offset = 0,
	.size = 0,
},
{
	.name = BOOT_BL2X,
	.offset = 0,
	.size = 0,
},
{
	.name = BOOT_DDRFIP,
	.offset = 0,
	.size = 0,
},
{
	.name = BOOT_DEVFIP,
	.offset = 0,
	.size = 0,
},
{
	.name = "logo",
	.offset = 0,
	.size = 2*SZ_1M,
},
{
	.name = "recovery",
	.offset = 0,
	.size = 16*SZ_1M,
},
{
	.name = "boot",
	.offset = 0,
	.size = 16*SZ_1M,
},
{
	.name = "system",
	.offset = 0,
	.size = 64*SZ_1M,
},
/* last partition get the rest capacity */
{
	.name = "data",
	.offset = MTDPART_OFS_APPEND,
	.size = MTDPART_SIZ_FULL,
},

struct mtd_partition *get_aml_mtd_partition(void)
{
        return normal_partition_info;
}

int get_aml_partition_count(void)
{
        return ARRAY_SIZE(normal_partition_info);
}

#endif

/* partition table */
/* partition table for spinand flash */
#if (defined(CONFIG_SPI_NAND) || defined(CONFIG_MTD_SPI_NAND))
static const struct mtd_partition spinand_partitions[] = {
	{
		.name = "logo",
		.offset = 0,
		.size = 2 * SZ_1M,
	},
	{
		.name = "recovery",
		.offset = 0,
		.size = 16 * SZ_1M,
	},
	{
		.name = "boot",
		.offset = 0,
		.size = 16 * SZ_1M,
	},
	{
		.name = "system",
		.offset = 0,
		.size = 64 * SZ_1M,
	},
	/* last partition get the rest capacity */
	{
		.name = "data",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
	}
};
const struct mtd_partition *get_spinand_partition_table(int *partitions)
{
	*partitions = ARRAY_SIZE(spinand_partitions);
	return spinand_partitions;
}
#endif /* CONFIG_SPI_NAND */

#ifdef CONFIG_MULTI_DTB
int checkhw(char * name)
{
#ifdef CONFIG_AUTO_ADAPT_DDR_DTB
	unsigned int ddr_size = 0;
	char loc_name[64] = {0};
	char *mem_size = env_get("mem_size");

	ddr_size = get_ddr_memsize();

	printf("%s:%d ddr_size:0x%x\r\n",__func__,__LINE__,ddr_size);
	/* Factory RSV multi-DTB splits aml_dt on '_' as soc/plat/vari
	 * (sc2 / ah212 / 2g|4g). A 4-token name never matches.
	 */
	switch (ddr_size) {
		case CONFIG_2G_SIZE:
			strcpy(loc_name, "sc2_ah212_2g");

			/* if limit memory size */
			if (mem_size && !strcmp(mem_size, "1g")) {
				strcpy(loc_name, "sc2_ah212_1g");
			}
			break;
		case CONFIG_1G_SIZE:
			strcpy(loc_name, "sc2_ah212_1g");
			break;
		case CONFIG_3G_SIZE:
			strcpy(loc_name, "sc2_ah212_3g");
			break;
		case CONFIG_DDR_MAX_SIZE:
			strcpy(loc_name, "sc2_ah212_4g");
			break;
		default:
			printf("DDR size: 0x%x, multi-dt doesn't support, using 4g\n", ddr_size);
			strcpy(loc_name, "sc2_ah212_4g");
			break;
	}

	strcpy(name, loc_name);
	env_set("aml_dt", loc_name);
	return 0;
#else
	strcpy(name, "sc2_ah212_4g");
	env_set("aml_dt", "sc2_ah212_4g");
	return 0;
#endif
}
#endif

const char * const _env_args_reserve_[] =
{
	"upgrade_step",
	"bootloader_version",
	"lock",

	NULL//Keep NULL be last to tell END
};

int __attribute__((weak)) mmc_initialize(bd_t *bis){ return 0;}

int __attribute__((weak)) do_bootm(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]){ return 0;}

void __attribute__((weak)) set_working_fdt_addr(ulong addr) {}

int __attribute__((weak)) ofnode_read_u32_default(ofnode node, const char *propname, u32 def) {return 0;}

void __attribute__((weak)) md5_wd (unsigned char *input, int len, unsigned char output[16],	unsigned int chunk_sz){}
