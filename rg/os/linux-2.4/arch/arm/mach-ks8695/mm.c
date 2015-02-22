/*
 *  linux/arch/arm/mach-ks8695/mm.c
 *
 *  Extra MM routines for the ARM Integrator board
 *
 *  Copyright (C) 1999,2000 Arm Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
 
#include <asm/mach/map.h>

/*
 * Logical      Physical
 * f0000000	03FF0000	IO registers
 */
 
static struct map_desc ks8695_io_desc[] __initdata = {
#ifdef CONFIG_PCI_KS8695P
  { IO_ADDRESS(KS8695_IO_BASE),   KS8695_IO_BASE,         SZ_64K,   DOMAIN_IO, 0, 1},	 
#else  
  { IO_ADDRESS(KS8695_PCMCIA_IO_BASE), KS8695_PCMCIA_IO_BASE,  SZ_768K, DOMAIN_IO, 0, 1},
  { IO_ADDRESS(KS8695_IO_BASE),        KS8695_IO_BASE,         SZ_64K,  DOMAIN_IO, 0, 1},
#endif	
   LAST_DESC
};

void __init ks8695_map_io(void)
{
	iotable_init(ks8695_io_desc);
}
