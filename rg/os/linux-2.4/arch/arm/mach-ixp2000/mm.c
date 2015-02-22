/*
 * linux/arch/arm/mach-ixp2000/mm.c
 *
 * Copyright (C) 2002 Intel Corp.
 * Copyright (C) 2003 MontaVista Software, Inc.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/mm.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mach/map.h>


/*
 * Chip specific mappings shared by all IXP2000 systems
 */
static struct map_desc ixp2000_io_desc[] __initdata = {
	{
		GLOBAL_REG_VIRT_BASE,
		GLOBAL_REG_PHYS_BASE,
		GLOBAL_REG_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		GPIO_VIRT_BASE,
		GPIO_PHYS_BASE,
		GPIO_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		TIMER_VIRT_BASE,
		TIMER_PHYS_BASE,
		TIMER_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		UART_VIRT_BASE,
		UART_PHYS_BASE,
		UART_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		SLOWPORT_CSR_VIRT_BASE,
		SLOWPORT_CSR_PHYS_BASE,
		SLOWPORT_CSR_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		INTCTL_VIRT_BASE,
		INTCTL_PHYS_BASE,
		INTCTL_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		PCI_CREG_VIRT_BASE,
		PCI_CREG_PHYS_BASE,
		PCI_CREG_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		PCI_CSR_VIRT_BASE,
		PCI_CSR_PHYS_BASE,
		PCI_CSR_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		PCI_CFG0_VIRT_BASE,
		PCI_CFG0_PHYS_BASE,
		PCI_CFG0_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		PCI_CFG1_VIRT_BASE,
		PCI_CFG1_PHYS_BASE,
		PCI_CFG1_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	{
		PCI_IO_VIRT_BASE,
		PCI_IO_PHYS_BASE,
		PCI_IO_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	LAST_DESC
};


/*
 * Board specific mappings
 */
#ifdef CONFIG_ARCH_IXDP2400
static struct map_desc ixdp2400_io_desc[] __initdata = {
	{ 
		IXDP2400_VIRT_CPLD_BASE, 
		IXDP2400_PHY_CPLD_BASE,
		IXDP2400_CPLD_SIZE,
		DOMAIN_IO, 0, 1, 0, 0 
	},
	LAST_DESC
};

void __init ixdp2400_map_io(void)
{
	iotable_init(ixp2000_io_desc);

	iotable_init(ixdp2400_io_desc);
}
#endif

#ifdef CONFIG_ARCH_IXDP2800
static struct map_desc ixdp2800_io_desc[] __initdata = {
	{
		IXDP2800_VIRT_CPLD_BASE,
		IXDP2800_PHY_CPLD_BASE,
		IXDP2800_CPLD_SIZE,
		DOMAIN_IO, 0, 1, 0, 0
	},
	LAST_DESC
};
void __init ixdp2800_map_io(void)
{
	iotable_init(ixp2000_io_desc);

	iotable_init(ixdp2800_io_desc);
}
#endif
