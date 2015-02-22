/*
 * linux/arch/arm/mach-iop3xx/mm.c
 *
 * Low level memory intialization for IOP321 based systems
 *
 * Author: Rory Bolt <rorybolt@pacbell.net>
 * Copyright (C) 2002 Rory Bolt
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/mm.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
 
#include <asm/mach/map.h>
#include <asm/mach-types.h>


/*
 * Standard IO mapping for all IOP321 based systems
 */
static struct map_desc iop80321_std_desc[] __initdata = {
 /* virtual     physical      length      domain     r  w  c  b */ 

 /* mem mapped registers */ 
 { 0xfff00000,  0xffffe000,   0x00002000,  DOMAIN_IO, 0, 1, 0, 0},  

 /* PCI IO space */
 { 0xfe000000,  0x90000000,   0x00020000,  DOMAIN_IO, 0, 1, 0, 0}, 
 LAST_DESC
};

void __init iop321_map_io(void)
{
	iotable_init(iop80321_std_desc);
}

/*
 * IQ80321 & IQ31244 specific IO mappings
 *
 * We use RedBoot's setup for the onboard devices.
 */
#if defined(CONFIG_ARCH_IQ80321) || defined(CONFIG_ARCH_IQ31244)
static struct map_desc iq80321_io_desc[] __initdata = {
 /* virtual     physical      length        domain     r  w  c  b */ 

 /* on-board devices */
 { 0xfe800000,  0xfe800000,   0x00100000,   DOMAIN_IO, 0, 1, 0, 0}, 
 LAST_DESC
};

void __init iq80321_map_io(void)
{
	iop321_map_io();

	iotable_init(iq80321_io_desc);
}
#endif // CONFIG_ARCH_IQ80321 || CONFIG_ARCH_IQ31244

