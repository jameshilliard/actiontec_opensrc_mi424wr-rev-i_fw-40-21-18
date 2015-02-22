/*
 * arch/arm/mach-ixp2000/ixdp2800-irq.c
 *
 * Interrupt code for IXDP2800 board
 *
 * Author: Jeff Daly <jeffrey.daly@intel.com>
 * Copyright 2002 Intel Corp.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
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
#include <asm/mach/irq.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>

extern void do_IRQ(int, struct pt_regs *);
extern int setup_arm_irq(int, struct irqaction *);
extern void  ixp2000_init_irq(void);

static void ext_irq_mask(unsigned int irq)
{
	*IXDP2800_CPLD_INT_MASK &= ~(1 << (irq - NR_IXP2000_IRQS));
}

static void ext_irq_unmask(unsigned int irq)
{
	*IXDP2800_CPLD_INT_MASK |= (1 << (irq - NR_IXP2000_IRQS));
}

static void ext_irq_demux(int irq, void *dev_id, struct pt_regs *regs)
{
        volatile u32 ex_interrupt = 0;
    	int irqno = 0, i = 0;

        ex_interrupt = *(IXDP2800_CPLD_INT);

	for(i = 0; i < 6; i++) {
		if(ex_interrupt & (1 << i))
			do_IRQ(IRQ_IXDP2800(i), regs);
	}

#if 0
   	if (ex_interrupt & IXDP2800_MASK_EGRESS_NIC)
                irqno = IRQ_IXDP2800_EGRESS_NIC;
        else if(ex_interrupt & IXDP2800_MASK_INGRESS)
		irqno = IRQ_IXDP2800_INGRESS_NPU;
	else if(ex_interrupt & IXDP2800_MASK_PMC)
                irqno = IRQ_IXDP2800_PMC_PCI;
        else if(ex_interrupt & IXDP2800_MASK_FABRIC_PCI)
		irqno = IRQ_IXDP2800_FABRIC_PCI;
        else if (ex_interrupt & IXDP2800_MASK_FABRIC_SP)
                irqno = IRQ_IXDP2800_FABRIC;
	else if (ex_interrupt & IXDP2800_MASK_MEDIA)
		irqno = IRQ_IXDP2800_MEDIA;

   	do_IRQ(irqno, regs);
#endif
}

static struct irqaction ext_irq = {
	name:	"IXDP2800 CPLD",
	handler: ext_irq_demux,
	flags: SA_INTERRUPT
};

void __init ixdp2800_init_irq(void)
{
	int i = 0;
	
	*IXDP2800_CPLD_INT_MASK = 0;		/* turn off interrupts */

	/* initialize chip specific interrupts */
	ixp2000_init_irq();

	/*
	 * Slave only has NIC routed to it, so we don't init everything
	 */
	if (npu_is_master()) {
		for(i = NR_IXP2000_IRQS; i < NR_IXDP2800_IRQS; i++) {
			irq_desc[i].valid 	= 1;
			irq_desc[i].probe_ok	= 0;
			irq_desc[i].mask_ack	= ext_irq_mask;
			irq_desc[i].mask	= ext_irq_mask;
			irq_desc[i].unmask	= ext_irq_unmask;
		}

		/* init PCI interrupts */
		setup_arm_irq(IRQ_IXP2000_PCIB, &ext_irq);
	}
}
