/*
 * linux/arch/arm/mach-ixp2000/arch.c
 *
 * Copyright (C) 2002 Intel Corp.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>



static void __init
fixup_ixp2000(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{

	if(machine_is_ixdp2400() || machine_is_ixdp2800())
	{
		mi->bank[0].start = PHYS_SDRAM_BASE; 
		mi->bank[0].size  = PHYS_SDRAM_SIZE;
		mi->bank[0].node  = 0;
		mi->nr_banks      = 1;
	} 

#ifdef CONFIG_BLK_DEV_INITRD
	setup_ramdisk( 1, 0, 0, 16384 );
	setup_initrd(__phys_to_virt(INITRD_LOCATION), INITRD_SIZE );
	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
#endif
}

#ifdef CONFIG_ARCH_IXDP2400
extern void ixdp2400_map_io(void);
extern void ixdp2400_init_irq(void);

MACHINE_START(IXDP2400, "Intel IXDP2400 Development Platform")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(PHYS_SDRAM_BASE, 0xc0030000, IXP2000_UART_BASE)/*pram,pio,vio*/
	FIXUP(fixup_ixp2000)
	MAPIO(ixdp2400_map_io)
	INITIRQ(ixdp2400_init_irq)
MACHINE_END
#elif defined (CONFIG_ARCH_IXDP2800)
extern void ixdp2800_map_io(void);
extern void ixdp2800_init_irq(void);

MACHINE_START(IXDP2800, "Intel IXDP2800 Development Platform")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(PHYS_SDRAM_BASE, 0xc0030000, IXP2000_UART_BASE)/*pram,pio,vio*/
	FIXUP(fixup_ixp2000)
	MAPIO(ixdp2800_map_io)
	INITIRQ(ixdp2800_init_irq)
MACHINE_END
#else
#error No board defined!
#endif
