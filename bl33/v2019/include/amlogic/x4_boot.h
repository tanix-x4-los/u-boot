/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __AMLOGIC_X4_BOOT_H
#define __AMLOGIC_X4_BOOT_H
#include <asm/io.h>
#include <asm/arch/secure_apb.h>
#include <asm/arch/romboot.h>
/* SC2 ROM boot source, as used by the vendor ADNL implementation. */
static inline bool x4_usb_ram_boot(void)
{
	return ((readl(SYSCTRL_SEC_STATUS_REG2) >> 4) & 0xf) == BOOT_ID_USB;
}
#endif
