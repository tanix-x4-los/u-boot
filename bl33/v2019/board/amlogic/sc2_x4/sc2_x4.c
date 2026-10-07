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

	if (x4_migrate_gpt()) {
		puts("X4: GPT migration failed; entering fastboot\n");
		env_set("bootcmd", "run enter_fastboot");
	}

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
