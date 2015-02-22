/*
 * arch/arm/mach-ixp2000/ixp2000-gpio.c
 *
 * GPIO code for IXP2000 board
 *
 * Author: Jeff Daly <jeffrey.daly@intel.com>
 *	Using mach-ixp425/ixp425-gpio.c as a model
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/module.h>
#include <linux/sched.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/arch/hardware.h>

static int GPIO_IRQ_rising_edge;
static int GPIO_IRQ_falling_edge;
static int GPIO_IRQ_level_low;
static int GPIO_IRQ_level_high;

#if 0
void set_GPIO_IRQ_edge(int gpio_nr, int edge)
{
	long flags;

	local_irq_save(flags);
	*IXP2000_GPIO_PDCR = BIT(gpio_nr);

	if (edge & GPIO_FALLING_EDGE)
		GPIO_IRQ_falling_edge |= BIT(gpio_nr);
	else
		GPIO_IRQ_falling_edge &= ~BIT(gpio_nr);
	if (edge & GPIO_RISING_EDGE)
		GPIO_IRQ_rising_edge |= BIT(gpio_nr);
	else
		GPIO_IRQ_rising_edge &= ~BIT(gpio_nr);
	GPIO_IRQ_level_high &= ~(GPIO_IRQ_falling_edge | GPIO_IRQ_rising_edge);
	GPIO_IRQ_level_low &= ~(GPIO_IRQ_falling_edge | GPIO_IRQ_rising_edge);

	*IXP2000_GPIO_FEDR = GPIO_IRQ_falling_edge;
	*IXP2000_GPIO_REDR = GPIO_IRQ_rising_edge;
	*IXP2000_GPIO_LSHR = GPIO_IRQ_level_high;
	*IXP2000_GPIO_LSLR = GPIO_IRQ_level_low;

	irq_desc[gpio_nr+IRQ_IXP2000_GPIO0].valid = 1;

	local_irq_restore(flags);
}

void set_GPIO_IRQ_level(int gpio_nr, int level)
{
	long flags;

	local_irq_save(flags);
	*IXP2000_GPIO_PDCR = BIT(gpio_nr);

	if (level & GPIO_LEVEL_LOW) {
		GPIO_IRQ_level_low |= BIT(gpio_nr);
		GPIO_IRQ_level_high &= ~BIT(gpio_nr);
	}
	else {
		GPIO_IRQ_level_low &= ~BIT(gpio_nr);
		GPIO_IRQ_level_high |= BIT(gpio_nr);
	}
	GPIO_IRQ_rising_edge &= ~(GPIO_IRQ_level_low | GPIO_IRQ_level_high);
	GPIO_IRQ_falling_edge &= ~(GPIO_IRQ_level_low | GPIO_IRQ_level_high);

	*IXP2000_GPIO_FEDR = GPIO_IRQ_falling_edge;
	*IXP2000_GPIO_REDR = GPIO_IRQ_rising_edge;
	*IXP2000_GPIO_LSHR = GPIO_IRQ_level_high;
	*IXP2000_GPIO_LSLR = GPIO_IRQ_level_low;

	irq_desc[gpio_nr+IRQ_IXP2000_GPIO0].valid = 1;

	local_irq_restore(flags);
}
#endif


void gpio_line_config(int line, int style)
{
	int flags;

	local_irq_save(flags);

	if(style == GPIO_OUT) {
		/* if it's an output, it ain't an interrupt anymore */
		*IXP2000_GPIO_PDSR = BIT(line);
		GPIO_IRQ_falling_edge &= ~BIT(line);
		GPIO_IRQ_rising_edge &= ~BIT(line);
		GPIO_IRQ_level_low &= ~BIT(line);
		GPIO_IRQ_level_high &= ~BIT(line);
		*IXP2000_GPIO_FEDR = GPIO_IRQ_falling_edge;
		*IXP2000_GPIO_REDR = GPIO_IRQ_rising_edge;
		*IXP2000_GPIO_LSHR = GPIO_IRQ_level_high;
		*IXP2000_GPIO_LSLR = GPIO_IRQ_level_low;
		irq_desc[line+IRQ_IXP2000_GPIO0].valid = 0;
	} else if(style == GPIO_IN) {
		*IXP2000_GPIO_PDCR = BIT(line);
	}
		
	local_irq_restore(flags);
}	

#if 0
EXPORT_SYMBOL(set_GPIO_IRQ_edge);
EXPORT_SYMBOL(set_GPIO_IRQ_level);
#endif
EXPORT_SYMBOL(gpio_line_config);


