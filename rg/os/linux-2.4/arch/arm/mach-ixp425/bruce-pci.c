/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/bruce-pci.c
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

#define USB_IRQ			IRQ_IXP425_GPIO0
#define PATA_IRQ		IRQ_IXP425_GPIO1
#define SATA_IRQ		IRQ_IXP425_GPIO2
#define MINIPCI_IRQ		IRQ_IXP425_GPIO3

/* Slots configuration */
#define USB_SLOT		15
#define PATA_SLOT		14
#define SATA_SLOT		13
#define MINIPCI_SLOT		12

#define USB_GPIO		IXP425_GPIO_PIN_0
#define PATA_GPIO		IXP425_GPIO_PIN_1
#define SATA_GPIO		IXP425_GPIO_PIN_2
#define MINIPCI_GPIO		IXP425_GPIO_PIN_3

void __init bruce_pci_gpio_init(void)
{
	/* configure PCI-related GPIO */

	gpio_line_config(USB_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(PATA_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(SATA_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(MINIPCI_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

	gpio_line_isr_clear(USB_GPIO);
	gpio_line_isr_clear(PATA_GPIO);
	gpio_line_isr_clear(SATA_GPIO);
	gpio_line_isr_clear(MINIPCI_GPIO);
}

void __init bruce_pci_init(void *sysdata)
{
	bruce_pci_gpio_init();
	ixp425_pci_init(sysdata);
}

static int __init bruce_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
    switch(slot)
    {
    case USB_SLOT: return USB_IRQ;
    case PATA_SLOT: return PATA_IRQ;
    case SATA_SLOT: return SATA_IRQ;
    case MINIPCI_SLOT: return MINIPCI_IRQ;
    default: return -1;
    }
}

struct hw_pci bruce_pci __initdata = {
	init:		bruce_pci_init,
	swizzle:	common_swizzle,
	map_irq:	bruce_map_irq,
};

