/*
 * arch/arm/mach-ixp425/ixp425-irq.c 
 *
 * Generic IRQ routines for IXP425 based systems
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

#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
 
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/bitops.h>
 
#include <asm/mach/irq.h>           

#include <asm/hardware.h>

static void ixp425_irq_mask(unsigned int irq)
{
	*IXP425_ICMR &= ~BIT(irq);
}

static void ixp425_irq_gpio_ack(unsigned int irq)
{
	int line = gpio_irq_to_line(irq);

	if (line >= 0)
		gpio_line_isr_clear(line);
}
	
static void ixp425_irq_mask_ack(unsigned int irq)
{
        ixp425_irq_gpio_ack(irq);
	ixp425_irq_mask(irq);
}

static void ixp425_irq_unmask(unsigned int irq)
{
	*IXP425_ICMR |= BIT(irq);
}

void __init ixp425_init_irq(void)
{
	int i = 0;

	/* Route all sources to IRQ instead of FIQ */
	*IXP425_ICLR = 0x0;
	/* Disable all interrupt */
	*IXP425_ICMR = 0x0; 

	for(i = 0; i < NR_IRQS; i++)
	{
		irq_desc[i].valid	= 1;
		irq_desc[i].probe_ok	= 0;
		irq_desc[i].mask_ack	= ixp425_irq_mask_ack;
		irq_desc[i].mask	= ixp425_irq_mask;
		irq_desc[i].unmask	= ixp425_irq_unmask;
		irq_desc[i].ack		= ixp425_irq_gpio_ack;
	}
}
