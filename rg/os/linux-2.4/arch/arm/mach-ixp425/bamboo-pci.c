/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/bamboo-pci.c
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

#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/mach/pci.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pci.h>
#include <asm/arch/gpio.h>
#include <asm/arch/ixp425-pci.h>

#define INTA_IRQ	IRQ_IXP425_GPIO0
#define INTB_IRQ	IRQ_IXP425_GPIO1

/* Slots configuration */
#define USB_SLOT	20
#define CARDBUS_SLOT	21

#ifdef CONFIG_PCI_RESET

#define INTA_GPIO	IXP425_GPIO_PIN_0
#define INTB_GPIO	IXP425_GPIO_PIN_1

void __init bamboo_pci_hw_init(void)
{
	/* configure PCI-related GPIO */
	gpio_line_config(INTA_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTB_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

	gpio_line_isr_clear(INTA_GPIO);
	gpio_line_isr_clear(INTB_GPIO);
}

#endif

extern void __init cs6_init(void);

void __init bamboo_pci_init(void *sysdata)
{
    	cs6_init();
#ifdef CONFIG_PCI_RESET
	if (ixp425_pci_is_host())
		bamboo_pci_hw_init();
#endif
	ixp425_pci_init(sysdata);
}

static int __init bamboo_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	switch (slot)
	{
	case USB_SLOT: 
	    	return INTB_IRQ;
	
	case CARDBUS_SLOT:
		return INTA_IRQ;

	default:
		return -1;
	}
}

struct hw_pci bamboo_pci __initdata = {
	init:		bamboo_pci_init,
	swizzle:	common_swizzle,
	map_irq:	bamboo_map_irq,
};
