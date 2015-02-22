/****************************************************************************
 *
 * rg/vendor/jstream/jiwis8xx/modules/pci.c
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

#define IXP425_PCI_INTA_GPIO IXP425_GPIO_PIN_6
#define IXP425_PCI_INTB_GPIO IXP425_GPIO_PIN_7
#define IXP425_PCI_INTC_GPIO IXP425_GPIO_PIN_0

#ifdef CONFIG_PCI_RESET

/* PCI controller pin mappings */
#define IXP425_PCI_RESET_GPIO	IXP425_GPIO_PIN_8
#define IXP425_PCI_CLK_PIN      IXP425_GPIO_PIN_14
#define IXP425_PCI_CLK_ENABLE   IXP425_GPIO_CLK0_ENABLE
#define IXP425_PCI_CLK_TC_LSH   IXP425_GPIO_CLK0TC_LSH
#define IXP425_PCI_CLK_DC_LSH   IXP425_GPIO_CLK0DC_LSH

static void __init jiwis8xx_pci_hw_init(void)
{
    /* configure PCI-related GPIO */
#ifdef IXP425_PCI_INTA_GPIO
    gpio_line_config(IXP425_PCI_INTA_GPIO,
	IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
    gpio_line_isr_clear(IXP425_PCI_INTA_GPIO);
#endif

#ifdef IXP425_PCI_INTB_GPIO
    gpio_line_config(IXP425_PCI_INTB_GPIO,
	IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
    gpio_line_isr_clear(IXP425_PCI_INTB_GPIO);
#endif

#ifdef IXP425_PCI_INTC_GPIO
    gpio_line_config(IXP425_PCI_INTC_GPIO,
	IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
    gpio_line_isr_clear(IXP425_PCI_INTC_GPIO);
#endif

#ifdef IXP425_PCI_INTD_GPIO
    gpio_line_config(IXP425_PCI_INTD_GPIO,
	IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
    gpio_line_isr_clear(IXP425_PCI_INTD_GPIO);
#endif

    gpio_line_config(IXP425_PCI_CLK_PIN, IXP425_GPIO_OUT);
    gpio_line_config(IXP425_PCI_RESET_GPIO, IXP425_GPIO_OUT);

    /* Disable PCI clock */
    *IXP425_GPIO_GPCLKR &= ~IXP425_PCI_CLK_ENABLE;

    /* Assert reset for PCI controller */
    gpio_line_set(IXP425_PCI_RESET_GPIO, IXP425_GPIO_LOW);

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
    gpio_line_set(IXP425_PCI_RESET_GPIO, IXP425_GPIO_HIGH);

    /* wait a while to let other devices get ready after PCI reset */
    mdelay(1000);
}

#endif

static void __init jiwis8xx_pci_init(void *sysdata)
{
#ifdef CONFIG_PCI_RESET
    if (ixp425_pci_is_host())
	jiwis8xx_pci_hw_init();
#endif

    ixp425_pci_init(sysdata);
}

#define JIWIS8XX_PCI_IDE_MINIPCI3_SLOT 11
#define JIWIS8XX_PCI_MINIPCI1_SLOT 12
#define JIWIS8XX_PCI_MINIPCI2_SLOT 13
#define JIWIS8XX_PCI_USB_SLOT 14

static int __init jiwis8xx_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
    int irq;
    u8 gpio = (u8)-1;

    switch (slot)
    {
    case JIWIS8XX_PCI_MINIPCI1_SLOT:
    case JIWIS8XX_PCI_MINIPCI2_SLOT:
	gpio = IXP425_PCI_INTB_GPIO;
	break;
    case JIWIS8XX_PCI_IDE_MINIPCI3_SLOT:
	gpio = IXP425_PCI_INTC_GPIO;
	break;
    case JIWIS8XX_PCI_USB_SLOT:
	gpio = IXP425_PCI_INTA_GPIO;
	break;
    }
    
    irq = gpio_line_to_irq(gpio);

    if (irq > 0)
    {
	printk("PCI: %d.%d: %s got IRQ%d, GPIO%d\n",
	    slot, pin, dev->name, irq, gpio);
    }
    else
	printk("PCI: %d.%d: %s unexpected request IRQ\n", slot, pin, dev->name);
    
    return irq;
}

struct hw_pci jiwis8xx_pci __initdata = {
    .init = jiwis8xx_pci_init,
    .swizzle = common_swizzle,
    .map_irq = jiwis8xx_map_irq,
};

