/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/jeeves-pci.c
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

#define WIRELESS_INTA_IRQ	IRQ_IXP425_GPIO7
#define VT6307_INTA_IRQ		IRQ_IXP425_GPIO8
#define VT6202_INTC_IRQ		IRQ_IXP425_GPIO9
#define VT6202_INTB_IRQ		IRQ_IXP425_GPIO10
#define VT6202_INTA_IRQ		IRQ_IXP425_GPIO11

/* Slots configuration */
#define VT6202_SLOT		16
#define VT6307_SLOT		15
#define WIRELESS_SLOT		14

#ifdef CONFIG_PCI_RESET

#define WIRELESS_INTA_GPIO	IXP425_GPIO_PIN_7
#define VT6307_INTA_GPIO	IXP425_GPIO_PIN_8
#define VT6202_INTC_GPIO	IXP425_GPIO_PIN_9
#define VT6202_INTB_GPIO	IXP425_GPIO_PIN_10
#define VT6202_INTA_GPIO	IXP425_GPIO_PIN_11

/* PCI controller pin mappings */
#define IXP425_PCI_RESET_GPIO	IXP425_GPIO_PIN_13
#define IXP425_PCI_CLK_PIN	IXP425_GPIO_CLK_0
#define IXP425_PCI_CLK_ENABLE	IXP425_GPIO_CLK0_ENABLE
#define IXP425_PCI_CLK_TC_LSH	IXP425_GPIO_CLK0TC_LSH
#define IXP425_PCI_CLK_DC_LSH	IXP425_GPIO_CLK0DC_LSH

void __init jeeves_pci_hw_init(void)
{
	/* Disable PCI clock */
	*IXP425_GPIO_GPCLKR &= ~IXP425_PCI_CLK_ENABLE;

	/* configure PCI-related GPIO */
	gpio_line_config(IXP425_PCI_CLK_PIN, IXP425_GPIO_OUT);
	gpio_line_config(IXP425_PCI_RESET_GPIO, IXP425_GPIO_OUT);

	gpio_line_config(WIRELESS_INTA_GPIO,
		IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(VT6307_INTA_GPIO,
		IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(VT6202_INTC_GPIO,
		IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(VT6202_INTB_GPIO,
		IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(VT6202_INTA_GPIO,
		IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

	gpio_line_isr_clear(WIRELESS_INTA_GPIO);
	gpio_line_isr_clear(VT6307_INTA_GPIO);
	gpio_line_isr_clear(VT6202_INTC_GPIO);
	gpio_line_isr_clear(VT6202_INTB_GPIO);
	gpio_line_isr_clear(VT6202_INTA_GPIO);

	/* Assert reset for PCI controller */
	gpio_line_set(IXP425_PCI_RESET_GPIO, IXP425_GPIO_LOW);
	/* wait 1ms to satisfy "minimum reset assertion time" of the PCI spec.
	 */
	udelay(1000);
	/* Config PCI clock */
	*IXP425_GPIO_GPCLKR |= (0xf << IXP425_PCI_CLK_TC_LSH) | 
		(0xf << IXP425_PCI_CLK_DC_LSH);
	/* Enable PCI clock */
	*IXP425_GPIO_GPCLKR |= IXP425_PCI_CLK_ENABLE;
	/* wait 100us to satisfy "minimum reset assertion time from clock
	 * stable" requirement of the PCI spec. */
	udelay(100);
	/* Deassert reset for PCI controller */
	gpio_line_set(IXP425_PCI_RESET_GPIO, IXP425_GPIO_HIGH);

	/* wait a while to let other devices get ready after PCI reset */
	udelay(1000);
}

#endif

void __init jeeves_pci_init(void *sysdata)
{
#ifdef CONFIG_PCI_RESET
	if (ixp425_pci_is_host())
		jeeves_pci_hw_init();
#endif
	ixp425_pci_init(sysdata);
}

static int __init jeeves_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	switch (slot)
	{
	case VT6202_SLOT:
		if (pin == 1) return VT6202_INTA_IRQ;
		if (pin == 2) return VT6202_INTB_IRQ;
		if (pin == 3) return VT6202_INTC_IRQ;
		return -1;
	case VT6307_SLOT:
		if (pin == 1) return VT6307_INTA_IRQ;
		return -1;
	case WIRELESS_SLOT:
		if (pin == 1) return WIRELESS_INTA_IRQ;
		return -1;
	default:
		return -1;
	}
}

struct hw_pci jeeves_pci __initdata = {
	init:		jeeves_pci_init,
	swizzle:	common_swizzle,
	map_irq:	jeeves_map_irq,
};

