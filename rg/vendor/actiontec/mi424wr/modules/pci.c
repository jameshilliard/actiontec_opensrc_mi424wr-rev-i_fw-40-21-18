/****************************************************************************
 *
 * rg/vendor/actiontec/mi424wr/modules/pci.c
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

#include "latch.h"
#include "arch.h"

#define IXP425_PCI_INTA_IRQ IRQ_IXP425_GPIO6
#define IXP425_PCI_INTB_IRQ IRQ_IXP425_GPIO7
#define IXP425_PCI_INTC_IRQ IRQ_IXP425_GPIO8

#ifdef CONFIG_PCI_RESET

#define IXP425_PCI_INTA_GPIO	IXP425_GPIO_PIN_6
#define IXP425_PCI_INTB_GPIO	IXP425_GPIO_PIN_7
#define IXP425_PCI_INTC_GPIO	IXP425_GPIO_PIN_8

/* PCI controller pin mappings */
#define IXP425_PCI_CLK_PIN      IXP425_GPIO_PIN_14
#define IXP425_PCI_CLK_ENABLE   IXP425_GPIO_CLK0_ENABLE
#define IXP425_PCI_CLK_TC_LSH   IXP425_GPIO_CLK0TC_LSH
#define IXP425_PCI_CLK_DC_LSH   IXP425_GPIO_CLK0DC_LSH

void __init mi424wr_pci_hw_init(void)
{
    /* Disable PCI clock */
    *IXP425_GPIO_GPCLKR &= ~IXP425_PCI_CLK_ENABLE;

    /* configure PCI-related GPIO */
    gpio_line_config(IXP425_PCI_CLK_PIN, IXP425_GPIO_OUT);

    gpio_line_config(IXP425_PCI_INTA_GPIO,
	IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
    gpio_line_config(IXP425_PCI_INTB_GPIO,
	IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
    gpio_line_config(IXP425_PCI_INTC_GPIO,
	IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

    gpio_line_isr_clear(IXP425_PCI_INTA_GPIO);
    gpio_line_isr_clear(IXP425_PCI_INTB_GPIO);
    gpio_line_isr_clear(IXP425_PCI_INTC_GPIO);

    /* Assert reset for PCI controller */
    mi424wr_latch_set(MI424WR_LATCH_PCI_RESET, 0);

    /* Wait 1ms to satisfy "minimum reset assertion time" of the PCI spec. */
    udelay(1000);

    /* Configure PCI clock */
    *IXP425_GPIO_GPCLKR |= (0xf << IXP425_PCI_CLK_TC_LSH) | 
	(0xf << IXP425_PCI_CLK_DC_LSH);

    /* Enable PCI clock */
    *IXP425_GPIO_GPCLKR |= IXP425_PCI_CLK_ENABLE;

    /* wait 100us to satisfy "minimum reset assertion time from clock stable"
     * requirement of the PCI spec. */
    udelay(100);

    /* Deassert reset for PCI controller */
    mi424wr_latch_set(MI424WR_LATCH_PCI_RESET, 1);

    /* wait a while to let other devices get ready after PCI reset */
    mdelay(1000);
}

#endif

void __init mi424wr_pci_init(void *sysdata)
{
#ifdef CONFIG_PCI_RESET
    if (ixp425_pci_is_host())
	mi424wr_pci_hw_init();
#endif

    ixp425_pci_init(sysdata);
}

static int __init mi424wr_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
    static int irqs[3][2] = {
	{IXP425_PCI_INTC_IRQ, IXP425_PCI_INTA_IRQ}, /* MoCA WAN */
	{IXP425_PCI_INTB_IRQ, IXP425_PCI_INTC_IRQ}, /* Mini-PCI */
	{IXP425_PCI_INTA_IRQ, IXP425_PCI_INTB_IRQ}, /* MoCA LAN */
    };

    if (slot < 13 || slot > 15 || pin > 2)
	return -1;

    int irq = irqs[slot - 13][pin - 1];

    if (dev->vendor == 0x17e6)
    {
	if (dev->device == 0x0021)
	    mi424wr_en2210_count++;
	else if (dev->device == 0x0010)
	    mi424wr_en2010_count++;
    }

    printk("PCI: %d.%d: %s got irq %d\n", slot, pin, dev->name, irq);
    return irq;
}

struct hw_pci aei_ixp4xx_pci __initdata = {
    .init = mi424wr_pci_init,
    .swizzle = common_swizzle,
    .map_irq = mi424wr_map_irq,
};

