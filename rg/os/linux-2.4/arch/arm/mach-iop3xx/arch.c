/*
 * linux/arch/arm/mach-iop3xx/arch.c
 *
 * Author: Nicolas Pitre <nico@cam.org>
 * Copyright (C) 2001-2003 MontaVista Software, Inc.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <asm/types.h>
#include <asm/setup.h>
#include <asm/system.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#ifdef CONFIG_ARCH_IQ80310
extern void iq80310_map_io(void);
extern void iq80310_init_irq(void);
#endif

#if defined(CONFIG_ARCH_IQ80321) || defined(CONFIG_ARCH_IQ31244)
extern void iq80321_map_io(void);
extern void iop321_init_irq(void);
#endif

#ifdef CONFIG_ARCH_IQ80310
MACHINE_START(IQ80310, "Cyclone IQ80310")
	MAINTAINER("MontaVista Software Inc.")
	BOOT_MEM(0xa0000000, 0xfe000000, 0xfe000000)
	BOOT_PARAMS(0xa0000100)
	MAPIO(iq80310_map_io)
	INITIRQ(iq80310_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IQ80321
MACHINE_START(IQ80321, "Intel IQ80321")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(0xa0000000, 0xfe800000, 0xfe800000)
	BOOT_PARAMS(0xa0000100)
	MAPIO(iq80321_map_io)
	INITIRQ(iop321_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IQ31244
MACHINE_START(IQ31244, "Intel IQ31244")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(0xa0000000, 0xfe800000, 0xfe800000)
	BOOT_PARAMS(0xa0000100)
	MAPIO(iq80321_map_io)
	INITIRQ(iop321_init_irq)
MACHINE_END
#endif
