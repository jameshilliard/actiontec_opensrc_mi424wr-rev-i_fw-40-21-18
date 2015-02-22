/****************************************************************************
 *
 * rg/vendor/actiontec/mi424wr/modules/arch.c
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
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "arch.h"

extern void ixp425_map_io(void);
extern void ixp425_init_irq(void);

MACHINE_START(MI424WR, "MI424-WR")
	MAINTAINER("Actiontec") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END

MACHINE_START(RI408, "RI408")
	MAINTAINER("Actiontec") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END

MACHINE_START(VI414WG, "VI414-WG")
	MAINTAINER("Actiontec") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END

MACHINE_START(KI414WG, "KI414-WG")
	MAINTAINER("Actiontec") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END

int mi424wr_en2010_count;
int mi424wr_en2210_count;

mi424wr_rev_t mi424wr_rev_get(void)
{
    if (mi424wr_en2010_count == 0 && mi424wr_en2210_count == 2)
	return MI424WR_REVD;
    else if (mi424wr_en2010_count == 2 && mi424wr_en2210_count == 0)
	return MI424WR_REVC;
    else
	return MI424WR_UNKNOWN;
}

static int arch_init(void)
{
    if (machine_is_mi424wr())
	printk("Detected MI424WR Rev %c\n", MI424WR_IS_REVD() ? 'D' : 'C');

    if (machine_is_vi414wg())
    {
	*IXP425_EXP_CS3 = 0xbcd23c02;
	printk("vi414wg: initializing CS3 (VDSL CPE): %x\n", *IXP425_EXP_CS3);
    }
    return 0;
}

module_init(arch_init);
