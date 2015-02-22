/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Setting up the clock on the MIPS boards.
 */

#include <linux/types.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/mc146818rtc.h>

#include <asm/mipsregs.h>
#include <asm/ptrace.h>
#include <asm/hardirq.h>
#include <asm/irq.h>
#include <asm/div64.h>
#include <asm/cpu.h>
#include <asm/time.h>
#include <asm/mc146818-time.h>
#include <asm/msc01_ic.h>

#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/prom.h>
#include <asm/mips-boards/maltaint.h>
#include <asm/mc146818-time.h>

unsigned long cpu_khz;

#if defined(CONFIG_MIPS_ATLAS)
static char display_string[] = "        LINUX ON ATLAS       ";
#endif
#if defined(CONFIG_MIPS_MALTA)
static char display_string[] = "        LINUX ON MALTA       ";
#endif
#if defined(CONFIG_MIPS_SEAD)
static char display_string[] = "        LINUX ON SEAD       ";
#endif
static unsigned int display_count = 0;
#define MAX_DISPLAY_COUNT (sizeof(display_string) - 8)

static unsigned int timer_tick_count=0;
static int mips_cpu_timer_irq;

static inline void scroll_display_message(void)
{
	if ((timer_tick_count++ % HZ) == 0) {
		mips_display_message(&display_string[display_count++]);
		if (display_count == MAX_DISPLAY_COUNT)
			display_count = 0;
	}
}

static void mips_timer_dispatch (struct pt_regs *regs)
{
	do_IRQ (mips_cpu_timer_irq, regs);
}

extern int null_perf_irq(struct pt_regs *regs);

extern int (*perf_irq)(struct pt_regs *regs);

irqreturn_t mips_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int r2 = cpu_has_mips_r2;
	int cpu = smp_processor_id();

	if (cpu == 0) {
		/*
		 * CPU 0 handles the global timer interrupt job and process
		 * accounting resets count/compare registers to trigger next
		 * timer int.
		 */
		if (!r2 || (read_c0_cause() & (1 << 26)))
			if (perf_irq(regs))
				goto out;

		/* we keep interrupt disabled all the time */
		if (!r2 || (read_c0_cause() & (1 << 30)))
			timer_interrupt(irq, NULL, regs);

		scroll_display_message();
	} else {
		/* Everyone else needs to reset the timer int here as
		   ll_local_timer_interrupt doesn't */
		/*
		 * FIXME: need to cope with counter underflow.
		 * More support needs to be added to kernel/time for
		 * counter/timer interrupts on multiple CPU's
		 */
		write_c0_compare (read_c0_count() + (mips_hpt_frequency/HZ));
		/*
		 * other CPUs should do profiling and process accounting
		 */
		local_timer_interrupt (irq, dev_id, regs);
	}

out:
	return IRQ_HANDLED;
}

/*
 * Estimate CPU frequency.  Sets mips_counter_frequency as a side-effect
 */
static unsigned int __init estimate_cpu_frequency(void)
{
	unsigned int prid = read_c0_prid() & 0xffff00;
	unsigned int count;

#ifdef CONFIG_MIPS_SEAD
	/*
	 * The SEAD board doesn't have a real time clock, so we can't
	 * really calculate the timer frequency
	 * For now we hardwire the SEAD board frequency to 12MHz.
	 */

	if ((prid == (PRID_COMP_MIPS | PRID_IMP_20KC)) ||
	    (prid == (PRID_COMP_MIPS | PRID_IMP_25KF)))
		count = 12000000;
	else
		count = 6000000;
#endif
#if defined(CONFIG_MIPS_ATLAS) || defined(CONFIG_MIPS_MALTA)
	unsigned int flags;

	local_irq_save(flags);

	/* Start counter exactly on falling edge of update flag */
	while (CMOS_READ(RTC_REG_A) & RTC_UIP);
	while (!(CMOS_READ(RTC_REG_A) & RTC_UIP));

	/* Start r4k counter. */
	write_c0_count(0);

	/* Read counter exactly on falling edge of update flag */
	while (CMOS_READ(RTC_REG_A) & RTC_UIP);
	while (!(CMOS_READ(RTC_REG_A) & RTC_UIP));

	count = read_c0_count();

	/* restore interrupts */
	local_irq_restore(flags);
#endif

	mips_hpt_frequency = count;
	if ((prid != (PRID_COMP_MIPS | PRID_IMP_20KC)) &&
	    (prid != (PRID_COMP_MIPS | PRID_IMP_25KF)))
		count *= 2;

	count += 5000;    /* round */
	count -= count%10000;

	return count;
}

unsigned long __init mips_rtc_get_time(void)
{
	return mc146818_get_cmos_time();
}

void __init mips_time_init(void)
{
	unsigned int est_freq, flags;

	local_irq_save(flags);

        /* Set Data mode - binary. */
        CMOS_WRITE(CMOS_READ(RTC_CONTROL) | RTC_DM_BINARY, RTC_CONTROL);

	est_freq = estimate_cpu_frequency ();

	printk("CPU frequency %d.%02d MHz\n", est_freq/1000000,
	       (est_freq%1000000)*100/1000000);

        cpu_khz = est_freq / 1000;

	local_irq_restore(flags);
}

void __init mips_timer_setup(struct irqaction *irq)
{
	if (cpu_has_veic) {
		set_vi_handler (MSC01E_INT_CPUCTR, mips_timer_dispatch);
		mips_cpu_timer_irq = MSC01E_INT_BASE + MSC01E_INT_CPUCTR;
	}
	else {
		if (cpu_has_vint)
			set_vi_handler (MIPSCPU_INT_CPUCTR, mips_timer_dispatch);
		mips_cpu_timer_irq = MIPSCPU_INT_BASE + MIPSCPU_INT_CPUCTR;
	}


	/* we are using the cpu counter for timer interrupts */
	irq->handler = mips_timer_interrupt;	/* we use our own handler */
	setup_irq(mips_cpu_timer_irq, irq);

#ifdef CONFIG_SMP
	/* irq_desc(riptor) is a global resource, when the interrupt overlaps
	   on seperate cpu's the first one tries to handle the second interrupt.
	   The effect is that the int remains disabled on the second cpu.
	   Mark the interrupt with IRQ_PER_CPU to avoid any confusion */
	irq_desc[mips_cpu_timer_irq].status |= IRQ_PER_CPU;
#endif

        /* to generate the first timer interrupt */
	write_c0_compare (read_c0_count() + mips_hpt_frequency/HZ);
}
