/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/napa-pci.c
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

#define INTA_IRQ	IRQ_IXP425_GPIO12
#define INTB_IRQ	IRQ_IXP425_GPIO11
#define INTC_IRQ	IRQ_IXP425_GPIO10
#define INTD_IRQ	IRQ_IXP425_GPIO9

/* Slots configuration */
#define USB_SLOT	1
#define FIREWIRE_SLOT	2
#define MINI_PCI_SLOT	3

#ifdef CONFIG_PCI_RESET

#define INTA_GPIO	IXP425_GPIO_PIN_12
#define INTB_GPIO	IXP425_GPIO_PIN_11
#define INTC_GPIO	IXP425_GPIO_PIN_10
#define INTD_GPIO	IXP425_GPIO_PIN_9

/* PCI controller pin mappings */
#define RESET_GPIO	IXP425_GPIO_PIN_13
#define CLK_GPIO	IXP425_GPIO_CLK_0
#define CLK_ENABLE	IXP425_GPIO_CLK0_ENABLE
#define CLK_TC_LSH	IXP425_GPIO_CLK0TC_LSH
#define CLK_DC_LSH	IXP425_GPIO_CLK0DC_LSH

void __init napa_pci_hw_init(void)
{
	/* Disable PCI clock */
	*IXP425_GPIO_GPCLKR &= ~CLK_ENABLE;

	/* configure PCI-related GPIO */
	gpio_line_config(CLK_GPIO, IXP425_GPIO_OUT);
	gpio_line_config(RESET_GPIO, IXP425_GPIO_OUT);

	gpio_line_config(INTA_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTB_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTC_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTD_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

	gpio_line_isr_clear(INTA_GPIO);
	gpio_line_isr_clear(INTB_GPIO);
	gpio_line_isr_clear(INTC_GPIO);
	gpio_line_isr_clear(INTD_GPIO);

	/* Assert reset for PCI controller */
	gpio_line_set(RESET_GPIO, IXP425_GPIO_LOW);
	/* wait 1ms to satisfy "minimum reset assertion time"
	 * of the PCI spec.
	 */
	udelay(1000);
	/* Config PCI clock */
	*IXP425_GPIO_GPCLKR |= (0xf << CLK_TC_LSH) | (0xf << CLK_DC_LSH);
	/* Enable PCI clock */
	*IXP425_GPIO_GPCLKR |= CLK_ENABLE;
	/* wait 100us to satisfy "minimum reset assertion time from clock 
	 * stable". Requirement of the PCI spec.
	 */
	udelay(100);
	/* Deassert reset for PCI controller */
	gpio_line_set(RESET_GPIO, IXP425_GPIO_HIGH);

	/* wait a while to let other devices get ready after PCI reset */
	udelay(1000);
}

#endif

void __init napa_pci_init(void *sysdata)
{
#ifdef CONFIG_PCI_RESET
	if (ixp425_pci_is_host())
		napa_pci_hw_init();
#endif
	ixp425_pci_init(sysdata);
}

static int __init napa_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	switch (slot)
	{
	case USB_SLOT:
		/* USB (slot 1) is pfysicly connected to INTB */
		return INTB_IRQ;
	case FIREWIRE_SLOT:
		/* FireWire (slot 2) is pfysicly connected to INTA */
		return INTA_IRQ;
	case MINI_PCI_SLOT:
		/* Mini PCI connecter (slot 3) is pfysicly connected to INTC */
		return INTC_IRQ;
	default:
		return -1;
	}
}

struct hw_pci napa_pci __initdata = {
	init:		napa_pci_init,
	swizzle:	common_swizzle,
	map_irq:	napa_map_irq,
};

