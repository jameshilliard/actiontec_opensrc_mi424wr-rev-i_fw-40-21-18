/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/wav54g-fixup.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <asm/setup.h>
#include <asm/pgtable.h>
#include <asm/mach/arch.h>
#include <asm/hardware.h>

#define DMT_CLK_PIN      IXP425_GPIO_CLK_0
#define DMT_CLK_ENABLE   IXP425_GPIO_CLK0_ENABLE
#define DMT_CLK_TC_LSH   IXP425_GPIO_CLK0TC_LSH
#define DMT_CLK_DC_LSH   IXP425_GPIO_CLK0DC_LSH

static void __init map_peripheral_regs(void)
{
	unsigned long phys = IXP425_PERIPHERAL_BASE_PHYS;
	unsigned long virt = IXP425_PERIPHERAL_BASE_VIRT;
	int prot = PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_DOMAIN(DOMAIN_IO);
	pmd_t pmd;
	pmd_val(pmd) = phys | prot;
	set_pmd(pmd_offset(pgd_offset_k(virt), virt), pmd);
}

void __init wav54g_fixup(struct machine_desc *mdesc, 
	struct param_struct *param, char **p, struct meminfo *minfo)
{
    	map_peripheral_regs();
	
	/* Disable DMT clock */
	*IXP425_GPIO_GPCLKR &= ~DMT_CLK_ENABLE;
	gpio_line_config(DMT_CLK_PIN, IXP425_GPIO_OUT);

	/* Config DMT clock */
	*IXP425_GPIO_GPCLKR |= (0xf << DMT_CLK_TC_LSH) |
		(0xf << DMT_CLK_DC_LSH);
	
	/* Enable DMT clock */
	*IXP425_GPIO_GPCLKR |= DMT_CLK_ENABLE;
}

