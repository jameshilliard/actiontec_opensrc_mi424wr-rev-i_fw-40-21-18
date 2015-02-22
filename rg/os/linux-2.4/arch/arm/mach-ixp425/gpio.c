/*
 * arch/arm/mach-ixp425/ixp425-gpio.c 
 *
 * GPIO configuration APIs for IXP425 based systems
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <asm/hardware.h>
#include <asm/arch/irqs.h>

#define IXP425_GPIO_INTSTYLE_MASK	0x7C  /* Bits [6:2] define interrupt style */

/* Interrupt styles, these refer to actual values used in reg */
#define IXP425_GPIO_STYLE_ACTIVE_HIGH	0x0
#define IXP425_GPIO_STYLE_ACTIVE_LOW	0x1
#define IXP425_GPIO_STYLE_RISING_EDGE	0x2
#define IXP425_GPIO_STYLE_FALLING_EDGE	0x3
#define IXP425_GPIO_STYLE_TRANSITIONAL	0x4

/* Mask used to clear interrupt styles */
#define IXP425_GPIO_STYLE_CLEAR		0x7
#define IXP425_GPIO_STYLE_SIZE		3

void gpio_line_isr_clear(u8 line)
{
	*IXP425_GPIO_GPISR = BIT(line);
}

void gpio_line_get(u8 line, int *value)
{
	*value = (*IXP425_GPIO_GPINR >> line) & 0x1;
}

void gpio_line_set(u8 line, int value)
{
	if (value == IXP425_GPIO_HIGH)
	    *IXP425_GPIO_GPOUTR |= BIT(line);
	else if (value == IXP425_GPIO_LOW)
	    *IXP425_GPIO_GPOUTR &= ~BIT(line);
}

void gpio_line_config(u8 line, u32 style)
{
	u32 enable;
	volatile u32 *int_reg;
	u32 int_style;

	enable = *IXP425_GPIO_GPOER;

	if (style & IXP425_GPIO_OUT) {
		enable &= ~BIT(line);
	} else if (style & IXP425_GPIO_IN) {
		enable |= BIT(line);

		switch (style & IXP425_GPIO_INTSTYLE_MASK)
		{
		case (IXP425_GPIO_ACTIVE_HIGH):
			int_style = IXP425_GPIO_STYLE_ACTIVE_HIGH;
			break;
		case (IXP425_GPIO_ACTIVE_LOW):
			int_style = IXP425_GPIO_STYLE_ACTIVE_LOW;
			break;
		case (IXP425_GPIO_RISING_EDGE):
			int_style = IXP425_GPIO_STYLE_RISING_EDGE;
			break;
		case (IXP425_GPIO_FALLING_EDGE):
			int_style = IXP425_GPIO_STYLE_FALLING_EDGE;
			break;
		case (IXP425_GPIO_TRANSITIONAL):
			int_style = IXP425_GPIO_STYLE_TRANSITIONAL;
			break;
		default:
			int_style = IXP425_GPIO_STYLE_ACTIVE_HIGH;
			break;
		}

		if (line >= 8) {	/* pins 8-15 */ 
			line -= 8;
			int_reg = IXP425_GPIO_GPIT2R;
		}
		else {			/* pins 0-7 */
			int_reg = IXP425_GPIO_GPIT1R;
		}

		/* Clear the style for the appropriate pin */
		*int_reg &= ~(IXP425_GPIO_STYLE_CLEAR << 
		    		(line * IXP425_GPIO_STYLE_SIZE));

		/* Set the new style */
		*int_reg |= (int_style << (line * IXP425_GPIO_STYLE_SIZE));
	}

	*IXP425_GPIO_GPOER = enable;
}

int gpio_line_to_irq(u8 line)
{
	static int gpio2irq[] = {
		IRQ_IXP425_GPIO0,
		IRQ_IXP425_GPIO1,
		IRQ_IXP425_GPIO2,
		IRQ_IXP425_GPIO3,
		IRQ_IXP425_GPIO4,
		IRQ_IXP425_GPIO5,
		IRQ_IXP425_GPIO6,
		IRQ_IXP425_GPIO7,
		IRQ_IXP425_GPIO8,
		IRQ_IXP425_GPIO9,
		IRQ_IXP425_GPIO10,
		IRQ_IXP425_GPIO11,
		IRQ_IXP425_GPIO12,
	};

	if (line < 0 || line > 12)
		return -1;

	return gpio2irq[line];
}

u8 gpio_irq_to_line(int irq)
{
	static u8 irq2gpio[NR_IRQS] = {
		-1, -1, -1, -1, -1, -1,  0,  1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1,  2,  3,  4,  5,  6,
		 7,  8,  9, 10, 11, 12, -1, -1,
	};

	if (irq < 0 || irq > NR_IRQS)
	    return -1;

	return irq2gpio[irq];
}

EXPORT_SYMBOL(gpio_line_config);
EXPORT_SYMBOL(gpio_line_set);
EXPORT_SYMBOL(gpio_line_get);
EXPORT_SYMBOL(gpio_line_isr_clear);
EXPORT_SYMBOL(gpio_line_to_irq);
EXPORT_SYMBOL(gpio_irq_to_line);

