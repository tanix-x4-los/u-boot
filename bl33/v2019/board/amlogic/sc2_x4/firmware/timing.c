// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <asm/arch/secure_apb.h>
#include <asm/arch/timing.h>
#include <asm/arch/ddr_define.h>

/* board clk defines */
#define CPU_CLK					1512
#define DSU_CLK					1200

/* ddr config support multiple configs for boards which use same bootloader:
 * config steps:
 * 1. add a new data struct in __ddr_setting[]
 * 2. config correct board_id, ddr_type, freq, etc..
 */


/* CAUTION!! */
/* Confirm ddr configs with hardware designer,
 * if you don't know how to config, then don't edit it
 */

/* Key configs */
/*
 * board_id: check hardware adc config
 * dram_rank_config:
 *            #define CONFIG_DDR_CHL_AUTO					0xF
 *            #define CONFIG_DDR0_16BIT_CH0					0x1
 *            #define CONFIG_DDR0_16BIT_RANK01_CH0			0x4
 *            #define CONFIG_DDR0_32BIT_RANK0_CH0			0x2
 *            #define CONFIG_DDR0_32BIT_RANK01_CH01			0x3
 *            #define CONFIG_DDR0_32BIT_16BIT_RANK0_CH0		0x5
 *            #define CONFIG_DDR0_32BIT_16BIT_RANK01_CH0	0x6
 * DramType:
 *            #define CONFIG_DDR_TYPE_DDR3					0
 *            #define CONFIG_DDR_TYPE_DDR4					1
 *            #define CONFIG_DDR_TYPE_LPDDR4				2
 *            #define CONFIG_DDR_TYPE_LPDDR3				3
 * DRAMFreq:
 *            {pstate0, pstate1, pstate2, pstate3} //more than one pstate means use dynamic freq
 *
 */

/* Exact packed DDR table bytes from the local stock bootloader backup.
 * Six entries, 1176 bytes each; DDR3 entry 0 uses rank0 at 672 MHz.
 * Preserve all reserved fields and vendor training data without translating.
 */
extern ddr_set_t __ddr_setting[6];
typedef char stock_ddr_table_abi_check[(sizeof(ddr_set_t) == 1176) ? 1 : -1];
__asm__(
    ".pushsection .ddr_param,\"a\"\n"
    ".balign 4\n"
    ".global __ddr_setting\n"
    ".type __ddr_setting, %object\n"
    "__ddr_setting:\n"
#include "stock-ddr-tables.inc"
    ".size __ddr_setting, 7056\n"
    ".popsection\n"
);

board_clk_set_t __board_clk_setting
__attribute__ ((section(".clk_param"))) = {
	/* clock settings for bl2 */
	.cpu_clk				= CPU_CLK / 24 * 24,
	.dsu_clk				= DSU_CLK / 24 * 24,
#ifdef CONFIG_PXP_EMULATOR
	.pxp					= 1,
#else
	.pxp					= 0,
#endif
	.high_console_baud = CONFIG_HIGH_CONSOLE_BAUD,
};


#define VCCK_VAL				AML_VCCK_INIT_VOLTAGE
#define VDDEE_VAL				AML_VDDEE_INIT_VOLTAGE
/* VCCK PWM table */
#if   (VCCK_VAL == 1039)
	#define VCCK_VAL_REG	0x00000022
#elif (VCCK_VAL == 1029)
	#define VCCK_VAL_REG	0x00010021
#elif (VCCK_VAL == 1019)
	#define VCCK_VAL_REG	0x00020020
#elif (VCCK_VAL == 1009)
	#define VCCK_VAL_REG	0x0003001f
#elif (VCCK_VAL == 999)
	#define VCCK_VAL_REG	0x0004001e
#elif (VCCK_VAL == 989)
	#define VCCK_VAL_REG	0x0005001d
#elif (VCCK_VAL == 979)
	#define VCCK_VAL_REG	0x0006001c
#elif (VCCK_VAL == 969)
	#define VCCK_VAL_REG	0x0007001b
#elif (VCCK_VAL == 959)
	#define VCCK_VAL_REG	0x0008001a
#elif (VCCK_VAL == 949)
	#define VCCK_VAL_REG	0x00090019
#elif (VCCK_VAL == 939)
	#define VCCK_VAL_REG	0x000a0018
#elif (VCCK_VAL == 929)
	#define VCCK_VAL_REG	0x000b0017
#elif (VCCK_VAL == 919)
	#define VCCK_VAL_REG	0x000c0016
#elif (VCCK_VAL == 909)
	#define VCCK_VAL_REG	0x000d0015
#elif (VCCK_VAL == 899)
	#define VCCK_VAL_REG	0x000e0014
#elif (VCCK_VAL == 889)
	#define VCCK_VAL_REG	0x000f0013
#elif (VCCK_VAL == 879)
	#define VCCK_VAL_REG	0x00100012
#elif (VCCK_VAL == 869)
	#define VCCK_VAL_REG	0x00110011
#elif (VCCK_VAL == 859)
	#define VCCK_VAL_REG	0x00120010
#elif (VCCK_VAL == 849)
	#define VCCK_VAL_REG	0x0013000f
#elif (VCCK_VAL == 839)
	#define VCCK_VAL_REG	0x0014000e
#elif (VCCK_VAL == 829)
	#define VCCK_VAL_REG	0x0015000d
#elif (VCCK_VAL == 819)
	#define VCCK_VAL_REG	0x0016000c
#elif (VCCK_VAL == 809)
	#define VCCK_VAL_REG	0x0017000b
#elif (VCCK_VAL == 799)
	#define VCCK_VAL_REG	0x0018000a
#elif (VCCK_VAL == 789)
	#define VCCK_VAL_REG	0x00190009
#elif (VCCK_VAL == 779)
	#define VCCK_VAL_REG	0x001a0008
#elif (VCCK_VAL == 769)
	#define VCCK_VAL_REG	0x001b0007
#elif (VCCK_VAL == 759)
	#define VCCK_VAL_REG	0x001c0006
#elif (VCCK_VAL == 749)
	#define VCCK_VAL_REG	0x001d0005
#elif (VCCK_VAL == 739)
	#define VCCK_VAL_REG	0x001e0004
#elif (VCCK_VAL == 729)
	#define VCCK_VAL_REG	0x001f0003
#elif (VCCK_VAL == 719)
	#define VCCK_VAL_REG	0x00200002
#elif (VCCK_VAL == 709)
	#define VCCK_VAL_REG	0x00210001
#elif (VCCK_VAL == 699)
	#define VCCK_VAL_REG	0x00220000
#else
	#error "VCCK val out of range\n"
#endif

/* VDDEE_VAL_REG */
#if    (VDDEE_VAL == 700)
	#define VDDEE_VAL_REG	0x120000
#elif (VDDEE_VAL == 710)
	#define VDDEE_VAL_REG	0x110001
#elif (VDDEE_VAL == 720)
	#define VDDEE_VAL_REG	0x100002
#elif (VDDEE_VAL == 730)
	#define VDDEE_VAL_REG	0xf0003
#elif (VDDEE_VAL == 740)
	#define VDDEE_VAL_REG	0xe0004
#elif (VDDEE_VAL == 750)
	#define VDDEE_VAL_REG	0xd0005
#elif (VDDEE_VAL == 760)
	#define VDDEE_VAL_REG	0xc0006
#elif (VDDEE_VAL == 770)
	#define VDDEE_VAL_REG	0xb0007
#elif (VDDEE_VAL == 780)
	#define VDDEE_VAL_REG	0xa0008
#elif (VDDEE_VAL == 790)
	#define VDDEE_VAL_REG	0x90009
#elif (VDDEE_VAL == 800)
	#define VDDEE_VAL_REG	0x8000a
#elif (VDDEE_VAL == 810)
	#define VDDEE_VAL_REG	0x7000b
#elif (VDDEE_VAL == 820)
	#define VDDEE_VAL_REG	0x6000c
#elif (VDDEE_VAL == 830)
	#define VDDEE_VAL_REG	0x5000d
#elif (VDDEE_VAL == 840)
	#define VDDEE_VAL_REG	0x4000e
#elif (VDDEE_VAL == 850)
	#define VDDEE_VAL_REG	0x3000f
#elif (VDDEE_VAL == 860)
	#define VDDEE_VAL_REG	0x20010
#elif (VDDEE_VAL == 870)
	#define VDDEE_VAL_REG	0x10011
#elif (VDDEE_VAL == 880)
	#define VDDEE_VAL_REG	0x12
#else
	#error "VDDEE val out of range\n"
#endif

bl2_reg_t __bl2_reg[] __attribute__ ((section(".generic_param"))) = {
	//need fine tune
	{0,			0,            		0xffffffff,   0, 0, 0},
};

/* gpio/pinmux/pwm init */
register_ops_t __bl2_ops_reg[MAX_REG_OPS_ENTRIES]
__attribute__ ((section(".misc_param"))) = {
	/* demo, user defined override register */
	{PWMGH_PWM_B,		VDDEE_VAL_REG,  	0xffffffff,	0, 0, 0},
	{PWMIJ_PWM_B,		VCCK_VAL_REG,  		0xffffffff,	0, 0, 0},
	{PWMGH_MISC_REG_AB,	(0x1 << 1), 		(0x1 << 1), 0, 0, 0},
	{PWMIJ_MISC_REG_AB,	(0x1 << 1), 		(0x1 << 1), 0, 0, 0},
	/* enable vddcpu dc-dc, set TEST_N to high */
	{PADCTRL_TESTN_O,	(0x1 << 0), 		(0x1 << 0),	0, 0, 0},
	{PADCTRL_TESTN_OEN,	(0x0 << 0), 		(0x1 << 0), 0, 0, 0},
	/* set pwm h and pwm j clock rate to 24M, enable them */
	{CLKCTRL_PWM_CLK_GH_CTRL,	(1 << 8 | 1 << 24) , 	0xffffffff, 	0, 0, 0},
	{CLKCTRL_PWM_CLK_IJ_CTRL,	(1 << 8 | 1 << 24) , 	0xffffffff, 	0, 0, 0},
	/* set GPIOE_0 GPIOE_1 drive strength to 3 */
	{PADCTRL_GPIOE_DS,	0xf, 	0xf,		0, 0, 0},
	/* set GPIOE_0 GPIOE_1 mux to pwmh pwmj */
	{PADCTRL_PIN_MUX_REGI,	(0x3 << 0),		(0xf << 0),	0, 0, 0},
	{PADCTRL_PIN_MUX_REGI,	(0x3 << 4),		(0xf << 4),	0, 0, 0},
};

#define DEV_FIP_SIZE 0x300000
#define DDR_FIP_SIZE 0x40000
/* for all the storage parameter */
storage_parameter_t __store_para __attribute__ ((section(".store_param"))) = {
	.common				= {
		.version = 0x01,
		.device_fip_container_size = DEV_FIP_SIZE,
		.device_fip_container_copies = 4,
		.ddr_fip_container_size = DDR_FIP_SIZE,
	},
	.nand				= {
		.version = 0x01,
		.bbt_pages = 0x01,
		.bbt_start_block = 20,
		.discrete_mode = 1,
		.setup_data.nand_setup_data = (2 << 20) |               \
					(0 << 19) |                     \
					(1 << 17) |                     \
					(1 << 14) |                     \
					(0 << 13) |                     \
					(64 << 6) |                     \
					(4 << 0),
		.reserved_area_blk_cnt = 48,
		.page_per_block = 64,
		.use_param_page_list = 0,
	},
};
