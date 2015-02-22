/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/wagp100g-pci.c
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

#define CN3_SLOT 1
#define CN6_SLOT 2

#define CN3_IRQ  IRQ_IXP425_GPIO2
#define CN6_IRQ  IRQ_IXP425_GPIO3

#ifdef CONFIG_PCI_RESET

#define CN3_GPIO IXP425_GPIO_PIN_2
#define CN6_GPIO IXP425_GPIO_PIN_3

#define IXP425_PCI_CLK_PIN	IXP425_GPIO_CLK_1
#define IXP425_PCI_CLK_ENABLE	IXP425_GPIO_CLK1_ENABLE
#define IXP425_PCI_CLK_TC_LSH	IXP425_GPIO_CLK1TC_LSH
#define IXP425_PCI_CLK_DC_LSH	IXP425_GPIO_CLK1DC_LSH

static void __init wagp100g_pci_clock_init(void)
{
    *IXP425_GPIO_GPCLKR &= ~IXP425_PCI_CLK_ENABLE;
    gpio_line_config(IXP425_PCI_CLK_PIN, IXP425_GPIO_OUT);
    *IXP425_GPIO_GPCLKR |= (0xf << IXP425_PCI_CLK_TC_LSH) | 
	(0xf << IXP425_PCI_CLK_DC_LSH);
    *IXP425_GPIO_GPCLKR |= IXP425_PCI_CLK_ENABLE;
}

static void __init wagp100g_pci_reset(void)
{
    void *rst = ioremap(IXP425_EXP_BUS_CS5_BASE_PHYS, 512);
    if (rst)
    {
	*IXP425_EXP_CS5 = 0xbfff0003;
	*(char *)(rst) = 0xFF;
	iounmap(rst);
	udelay(1000);
    }
}

static void __init wag100p_pci_hw_init(void)
{
    wagp100g_pci_clock_init();
    wagp100g_pci_reset();
    gpio_line_config(CN3_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
    gpio_line_isr_clear(CN3_GPIO);  
    gpio_line_config(CN6_GPIO, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
    gpio_line_isr_clear(CN6_GPIO);
}

#endif

static void __init wagp100g_pci_init(void *sysdata)
{
#ifdef CONFIG_PCI_RESET
    if (ixp425_pci_is_host())
	wag100p_pci_hw_init();
#endif

     ixp425_pci_init(sysdata);
}

static int __init wagp100g_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
    if (pin < 1 || pin > 4)
	return -1;

    switch (slot)
    {
    case CN3_SLOT:
	return CN3_IRQ;

    case CN6_SLOT:
	return CN6_IRQ;

    default:
	return -1;
    }
}

struct hw_pci wagp100g_pci __initdata = {
	init:		wagp100g_pci_init,
	swizzle:	common_swizzle,
	map_irq:	wagp100g_map_irq,
};

