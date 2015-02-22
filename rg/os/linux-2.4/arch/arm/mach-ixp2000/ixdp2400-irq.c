/*
 * arch/arm/mach-ixp2000/ixdp2400-irq.c
 *
 * Interrupt code for IXDP2400 board
 *
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
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

/*
 * Slowport configuration for accessing CPLD registers
 */
static struct slowport_cfg slowport_cpld_cfg = {
	CCR:	SLOWPORT_CCR_DIV_2,
	WTC:	0x00000070,
	RTC:	0x00000070,
	PCR:	SLOWPORT_MODE_FLASH,
	ADC:	SLOWPORT_ADDR_WIDTH_24 | SLOWPORT_DATA_WIDTH_8
};

static void ext_irq_mask(unsigned int irq)
{
	acquire_slowport(&slowport_cpld_cfg);
	*IXDP2400_CPLD_INT_MASK |= (1 << (irq - NR_IXP2000_IRQS));
	release_slowport();
}

static void ext_irq_unmask(unsigned int irq)
{
	acquire_slowport(&slowport_cpld_cfg);
	*IXDP2400_CPLD_INT_MASK &= ~(1 << (irq - NR_IXP2000_IRQS));
	release_slowport();
}

static void ext_irq_demux(int irq, void *dev_id, struct pt_regs *regs)
{
        volatile u32 ex_interrupt = 0;
	int i;

	acquire_slowport(&slowport_cpld_cfg);
        ex_interrupt = *(IXDP2400_CPLD_INT) & 0xff;
	release_slowport();

	if(!ex_interrupt) {
		printk(KERN_ERR "Spurious IXDP2400 CPLD interrupt!\n");
		return;
	}

	for(i = 0; i < 8; i++) {
		if(ex_interrupt & (1 << i)) 
			do_IRQ(IRQ_IXDP2400(0) + i, regs);
	}
}

static struct irqaction ext_irq = {
	name:	"IXDP2400 CPLD",
	handler: ext_irq_demux,
	flags: SA_INTERRUPT
};

/*
 * We only do anything if we are the master NPU on the board.
 * The slave NPU only has the ethernet chip going directly to
 * the PCIB interrupt input.
 */
void __init ixdp2400_init_irq(void)
{
	int i = 0;

	/* initialize chip specific interrupts */
	ixp2000_init_irq();

	if (npu_is_master()) {

		/* Disable all CPLD interrupts */
		acquire_slowport(&slowport_cpld_cfg);
		*IXDP2400_CPLD_INT_MASK = 0xff;
		release_slowport();

		for(i = NR_IXP2000_IRQS; i < NR_IXDP2400_IRQS; i++) {
			irq_desc[i].valid 	= 1;
			irq_desc[i].probe_ok	= 0;
			irq_desc[i].mask_ack	= ext_irq_mask;
			irq_desc[i].mask	= ext_irq_mask;
			irq_desc[i].unmask	= ext_irq_unmask;
		}

		/* Hook into PCI interrupts */
		setup_arm_irq(IRQ_IXP2000_PCIB, &ext_irq);
	}
}
