/*
 * arch/arm/mach-ixp2000/ixp2000-irq.c
 *
 * Interrupt code for IXDP2400 board
 *
 * Original Author: Naeem Afzal <naeem.m.afzal@intel.com>
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (c) 2002 Intel Corp.
 * Copyright (c) 2003 MontaVista Software, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
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

extern void do_IRQ(int, struct pt_regs *);
extern int setup_arm_irq(int, struct irqaction *);

static void ixp_irq_mask(unsigned int irq)
{
	*(IXP2000_IRQ_ENABLE_CLR) = (1 << irq);
}

static void ixp_irq_unmask(unsigned int irq)
{
	*(IXP2000_IRQ_ENABLE_SET) = (1 << irq);
}

/*
 * Install handler for GPIO interrupts
 */
static void ixp_GPIO_demux(int irq, void *dev_id, struct pt_regs *regs)
{                               
	int i;
		        
	while ((irq = (*IXP2000_GPIO_INST & 0xff))) {
		for (i = 0; i<=7;i++) {
			if (irq & (1<<i)) {
				do_IRQ(IRQ_IXP2000_GPIO0 + i,regs);
			}
		}
	}
}

static struct irqaction GPIO_irq = {
	name:           "GPIOs",
	handler:        ixp_GPIO_demux,
	flags:          SA_INTERRUPT
};

static void ixp_GPIO_irq_mask(unsigned int irq)
{
	*IXP2000_GPIO_INCR = (1 << (irq - IRQ_IXP2000_GPIO0));
}

static void ixp_GPIO_irq_unmask(unsigned int irq)
{
	*IXP2000_GPIO_INSR = (1 << (irq - IRQ_IXP2000_GPIO0));
}

static void ixp_PCI_irq_mask(unsigned int irq)
{
	if (irq == IRQ_IXP2000_PCIA)
		*IXP2000_PCI_XSCALE_INT_ENABLE &= ~(1 << 26);
	else if (irq == IRQ_IXP2000_PCIB)
		*IXP2000_PCI_XSCALE_INT_ENABLE &= ~(1 << 27);
}

static void ixp_PCI_irq_unmask(unsigned int irq)
{
	if (irq == IRQ_IXP2000_PCIA)
		*IXP2000_PCI_XSCALE_INT_ENABLE |= (1 << 26);
	else if (irq == IRQ_IXP2000_PCIB)
		*IXP2000_PCI_XSCALE_INT_ENABLE |= (1 << 27);
}

/*
 * Error interrupts, this should be generic ixp2000 code
 */
static void ixp_err_demux(int irq, void *dev_id, struct pt_regs *regs)
{
	int i;
	while ((irq = (*IXP2000_IRQ_ERR_STATUS))) {
		for (i=0; i<=26; i++) {
			if (irq & (1 << i)) {
				do_IRQ(IRQ_IXP2000_DRAM0_MIN_ERR + i, regs);
			}
		}
	}
}

struct irqaction ERR_irq = {
	name: "Error IRQs",
	handler: ixp_err_demux,
	flags: SA_INTERRUPT
};

static void ixp_ERR_irq_mask(unsigned int irq)
{
	*IXP2000_IRQ_ERR_ENABLE_CLR = (1 << (irq - IRQ_IXP2000_DRAM0_MIN_ERR));
}

static void ixp_ERR_irq_unmask(unsigned int irq)
{
	*IXP2000_IRQ_ERR_ENABLE_SET = (1 << (irq - IRQ_IXP2000_DRAM0_MIN_ERR));
}


void __init ixp2000_init_irq(void)
{
	int irq;

	/*
	 * Mask all sources
	 */
	*(IXP2000_IRQ_ENABLE_CLR) = 0xffffffff;
	*(IXP2000_FIQ_ENABLE_CLR) = 0xffffffff;

	/* clear all GPIO edge/level detects */
	*IXP2000_GPIO_REDR = 0;
	*IXP2000_GPIO_FEDR = 0;
	*IXP2000_GPIO_LSHR = 0;
	*IXP2000_GPIO_LSLR = 0;
	*IXP2000_GPIO_INCR = -1;

	/* clear PCI interrupt sources */
	*IXP2000_PCI_XSCALE_INT_ENABLE = 0;

	/*
	 * Certain bits in the IRQ status register of the 
	 * IXP2000 are reserved. Instead of trying to map
	 * things non 1:1 from bit position to IRQ number,
	 * we mark the reserved IRQs as invalid. This makes
	 * our mask/unmask code much simpler.
	 */
	for (irq = IRQ_IXP2000_SWI; irq <= IRQ_IXP2000_THDB3; irq++) {
		if((1 << irq) & IXP2000_VALID_IRQ_MASK) {
			irq_desc[irq].valid     = 1;
			irq_desc[irq].probe_ok  = 0;
			irq_desc[irq].mask_ack  = ixp_irq_mask;
			irq_desc[irq].mask      = ixp_irq_mask;
			irq_desc[irq].unmask    = ixp_irq_unmask;
		} else irq_desc[irq].valid = 0;
	}
	
	/*
	 * GPIO IRQs are invalid until someone sets the interrupt mode
	 * by calling gpio_line_set();
	 */
	for (irq = IRQ_IXP2000_GPIO0; irq <= IRQ_IXP2000_GPIO7; irq++) {
		irq_desc[irq].valid     = 0;
		irq_desc[irq].probe_ok  = 1;
		irq_desc[irq].mask_ack  = ixp_GPIO_irq_mask;
		irq_desc[irq].mask      = ixp_GPIO_irq_mask;
		irq_desc[irq].unmask    = ixp_GPIO_irq_unmask;
	}
	setup_arm_irq(IRQ_IXP2000_GPIO, &GPIO_irq);

	/*
	 * Enable PCI irq
	 */
	*(IXP2000_IRQ_ENABLE_SET) = (1 << IRQ_IXP2000_PCI);
	for (irq = IRQ_IXP2000_PCIA; irq <= IRQ_IXP2000_PCIB; irq++) {
		irq_desc[irq].valid     = 1;
		irq_desc[irq].probe_ok  = 0;
		irq_desc[irq].mask_ack  = ixp_PCI_irq_mask;
		irq_desc[irq].mask      = ixp_PCI_irq_mask;
		irq_desc[irq].unmask    = ixp_PCI_irq_unmask;
	}

	for (irq = IRQ_IXP2000_DRAM0_MIN_ERR; irq <= IRQ_IXP2000_SP_INT; irq++) {
		irq_desc[irq].valid     = 1;
		irq_desc[irq].probe_ok  = 0;
		irq_desc[irq].mask_ack  = ixp_ERR_irq_mask;
		irq_desc[irq].mask      = ixp_ERR_irq_mask;
		irq_desc[irq].unmask    = ixp_ERR_irq_unmask;
	}       
	setup_arm_irq(IRQ_IXP2000_ERRSUM, &ERR_irq);

}
