/*
 * IRQ vector handles
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997, 2003 by Ralf Baechle
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include <asm/i8259.h>
#include <asm/irq_cpu.h>
#include <asm/gt64120.h>
#include <asm/ptrace.h>

#include <asm/mach-cobalt/cobalt.h>

extern void cobalt_handle_int(void);

/*
 * We have two types of interrupts that we handle, ones that come in through
 * the CPU interrupt lines, and ones that come in on the via chip. The CPU
 * mappings are:
 *
 *    16   - Software interrupt 0 (unused)	IE_SW0
 *    17   - Software interrupt 1 (unused)	IE_SW1
 *    18   - Galileo chip (timer)		IE_IRQ0
 *    19   - Tulip 0 + NCR SCSI			IE_IRQ1
 *    20   - Tulip 1				IE_IRQ2
 *    21   - 16550 UART				IE_IRQ3
 *    22   - VIA southbridge PIC		IE_IRQ4
 *    23   - unused				IE_IRQ5
 *
 * The VIA chip is a master/slave 8259 setup and has the following interrupts:
 *
 *     8  - RTC
 *     9  - PCI
 *    14  - IDE0
 *    15  - IDE1
 */

static inline void galileo_irq(struct pt_regs *regs)
{
	unsigned int mask, pending, devfn;

	mask = GALILEO_INL(GT_INTRMASK_OFS);
	pending = GALILEO_INL(GT_INTRCAUSE_OFS) & mask;

	if (pending & GALILEO_INTR_T0EXP) {

		GALILEO_OUTL(~GALILEO_INTR_T0EXP, GT_INTRCAUSE_OFS);
		do_IRQ(COBALT_GALILEO_IRQ, regs);

	} else if (pending & GALILEO_INTR_RETRY_CTR) {

		devfn = GALILEO_INL(GT_PCI0_CFGADDR_OFS) >> 8;
		GALILEO_OUTL(~GALILEO_INTR_RETRY_CTR, GT_INTRCAUSE_OFS);
		printk(KERN_WARNING "Galileo: PCI retry count exceeded (%02x.%u)\n",
			PCI_SLOT(devfn), PCI_FUNC(devfn));

	} else {

		GALILEO_OUTL(mask & ~pending, GT_INTRMASK_OFS);
		printk(KERN_WARNING "Galileo: masking unexpected interrupt %08x\n", pending);
	}
}

static inline void via_pic_irq(struct pt_regs *regs)
{
	int irq;

	irq = i8259_irq();
	if (irq >= 0)
		do_IRQ(irq, regs);
}

asmlinkage void cobalt_irq(struct pt_regs *regs)
{
	unsigned pending;

	pending = read_c0_status() & read_c0_cause();

	if (pending & CAUSEF_IP2)			/* COBALT_GALILEO_IRQ (18) */

		galileo_irq(regs);

	else if (pending & CAUSEF_IP6)			/* COBALT_VIA_IRQ (22) */

		via_pic_irq(regs);

	else if (pending & CAUSEF_IP3)			/* COBALT_ETH0_IRQ (19) */

		do_IRQ(COBALT_CPU_IRQ + 3, regs);

	else if (pending & CAUSEF_IP4)			/* COBALT_ETH1_IRQ (20) */

		do_IRQ(COBALT_CPU_IRQ + 4, regs);

	else if (pending & CAUSEF_IP5)			/* COBALT_SERIAL_IRQ (21) */

		do_IRQ(COBALT_CPU_IRQ + 5, regs);

	else if (pending & CAUSEF_IP7)			/* IRQ 23 */

		do_IRQ(COBALT_CPU_IRQ + 7, regs);
}

static struct irqaction irq_via = {
	no_action, 0, { { 0, } }, "cascade", NULL, NULL
};

void __init arch_init_irq(void)
{
	/*
	 * Mask all Galileo interrupts. The Galileo
	 * handler is set in cobalt_timer_setup()
	 */
	GALILEO_OUTL(0, GT_INTRMASK_OFS);

	set_except_vector(0, cobalt_handle_int);

	init_i8259_irqs();				/*  0 ... 15 */
	mips_cpu_irq_init(COBALT_CPU_IRQ);		/* 16 ... 23 */

	/*
	 * Mask all cpu interrupts
	 *  (except IE4, we already masked those at VIA level)
	 */
	change_c0_status(ST0_IM, IE_IRQ4);

	setup_irq(COBALT_VIA_IRQ, &irq_via);
}
