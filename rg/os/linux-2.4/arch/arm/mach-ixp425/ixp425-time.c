/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/ixp425-time.c
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

/*
 * arch/arm/mach-ixp425/ixp425-time.c
 *
 * Author:  Peter Barry
 * Copyright:   (C) 2001 Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>
#include <asm/hardware.h>

/* Ticks per second are picked up from include/asm-arm/arch-ixp425/timex.h
 * LATCH is defined with CLOCK_TICK_RATE and HZ.
 */

extern int setup_arm_irq(int, struct irqaction *);

/* IRQs are disabled before entering here from do_gettimeofday() */

#define WRAP_THRESHOLD 2 /* two micro-seconds threshold */

static unsigned long ixp425_gettimeoffset(void)
{
	u32 curr, reload, usec;
	u64 elapsed; /* use u64 to prevent overflow */
	int unhandled_irq;

	/* We need elapsed timer ticks since last interrupt
	 * 
	 * Read the CCNT value.  The returned value is 
	 * between -LATCH and 0, 0 corresponding to a full jiffy 
	 */

	reload = *IXP425_OSRT1 & ~IXP425_OST_RELOAD_MASK;
	curr = *IXP425_OST1;
	
	/* sample wrap around interrupt status */
	unhandled_irq = *IXP425_OSST & IXP425_OSST_TIMER_1_PEND;
	elapsed = reload - curr;

	/* convert to micro seconds */
	usec = (unsigned long)(elapsed * tick / LATCH); 

	if (unhandled_irq && usec < (tick - WRAP_THRESHOLD))
	{
	    /* Hack, solves the following: 
	     * wrap around of IXP425_OST1 occured, interrupt handler did not 
	     * yet run (jiffies counter is not updated) and then IXP425_OST1 
	     * is sampled. In such case a full tick time should be added to 
	     * 'usec'.
	     * Notes:
	     * - If IXP425_OST1 value was sampled and then wrap around occured,
	     * 'usec' should not be increased. This case is handled by the 
	     * threshold condition. 
	     * - We've encountred a ~50 usec deviation once in a Million 
	     * executions (why? probably because LATCH isn't exact, see bug
	     * B10541).
	     */
	    usec += tick; 
	}

	return usec;
}

static void ixp425_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	u32 flags;

	/* Clear Pending Interrupt by writing '1' to it */
	*IXP425_OSST = IXP425_OSST_TIMER_1_PEND;

	save_flags_cli(flags);
	do_timer(regs);
	restore_flags(flags);
}

extern unsigned long (*gettimeoffset)(void);

static struct irqaction timer_irq = {
	name: "timer",
	handler: ixp425_timer_interrupt,
};

void __init setup_timer(void)
{
	gettimeoffset = ixp425_gettimeoffset;

	/* Clear Pending Interrupt by writing '1' to it */
	*IXP425_OSST = IXP425_OSST_TIMER_1_PEND;

	/* Setup the Timer counter value */
	*IXP425_OSRT1 = (LATCH & ~IXP425_OST_RELOAD_MASK) | IXP425_OST_ENABLE;

	/* Connect the interrupt handler and enable the interrupt */
	setup_arm_irq(IRQ_IXP425_TIMER1, &timer_irq);

	printk("Using IXP425 Timer 0 as timer source\n");
}

