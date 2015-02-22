/*
 * arch/mips/kernel/kgdb.c
 *
 *  Originally written by Glenn Engel, Lake Stevens Instrument Division
 *
 *  Contributed by HP Systems
 *
 *  Modified for SPARC by Stu Grossman, Cygnus Support.
 *
 *  Modified for Linux/MIPS (and MIPS in general) by Andreas Busse
 *  Send complaints, suggestions etc. to <andy@waldorf-gmbh.de>
 *
 *  Copyright (C) 1995 Andreas Busse
 *
 *  Copyright (C) 2003 MontaVista Software Inc.
 *  Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 *  Copyright (C) 2004-2005 MontaVista Software Inc.
 *  Author: Manish Lachwani, mlachwani@mvista.com or manish@koffee-break.com
 *
 *  This file is licensed under the terms of the GNU General Public License
 *  version 2. This program is licensed "as is" without any warranty of any
 *  kind, whether express or implied.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <asm/system.h>
#include <asm/ptrace.h>		/* for linux pt_regs struct */
#include <linux/kgdb.h>
#include <linux/init.h>
#include <asm/inst.h>
#include <asm/gdb-stub.h>
#include <asm/cacheflush.h>
#include <asm/kdebug.h>

static struct hard_trap_info {
	unsigned char tt;	/* Trap type code for MIPS R3xxx and R4xxx */
	unsigned char signo;	/* Signal that we map this trap into */
} hard_trap_info[] = {
	{ 6, SIGBUS },		/* instruction bus error */
	{ 7, SIGBUS },		/* data bus error */
	{ 9, SIGTRAP },		/* break */
/*	{ 11, SIGILL },	*/	/* CPU unusable */
	{ 12, SIGFPE },		/* overflow */
	{ 13, SIGTRAP },	/* trap */
	{ 14, SIGSEGV },	/* virtual instruction cache coherency */
	{ 15, SIGFPE },		/* floating point exception */
	{ 23, SIGSEGV },	/* watch */
	{ 31, SIGSEGV },	/* virtual data cache coherency */
	{ 0, 0}			/* Must be last */
};

/* Save the normal trap handlers for user-mode traps. */
void *saved_vectors[32];

extern void trap_low(void);
extern void breakinst(void);
extern void init_IRQ(void);

void kgdb_call_nmi_hook(void *ignored)
{
	kgdb_nmihook(smp_processor_id(), (void *)0);
}

void kgdb_roundup_cpus(unsigned long flags)
{
	local_irq_restore(flags);
	smp_call_function(kgdb_call_nmi_hook, 0, 0, 0);
	local_irq_save(flags);
}

static int compute_signal(int tt)
{
	struct hard_trap_info *ht;

	for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
		if (ht->tt == tt)
			return ht->signo;

	return SIGHUP;		/* default for things we don't know about */
}

/*
 * Set up exception handlers for tracing and breakpoints
 */
void handle_exception(struct pt_regs *regs)
{
	int trap = (regs->cp0_cause & 0x7c) >> 2;

	if (fixup_exception(regs)) {
		return;
	}

	if (atomic_read(&debugger_active))
		kgdb_nmihook(smp_processor_id(), regs);

	if (atomic_read(&kgdb_setting_breakpoint))
		if ((trap == 9) && (regs->cp0_epc == (unsigned long)breakinst))
			regs->cp0_epc += 4;

	kgdb_handle_exception(0, compute_signal(trap), 0, regs);

	/* In SMP mode, __flush_cache_all does IPI */
	__flush_cache_all();
}

void set_debug_traps(void)
{
	struct hard_trap_info *ht;
	unsigned long flags;

	local_irq_save(flags);

	for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
		saved_vectors[ht->tt] = set_except_vector(ht->tt, trap_low);

	local_irq_restore(flags);
}

#if 0
/* This should be called before we exit kgdb_handle_exception() I believe.
 * -- Tom
 */
void restore_debug_traps(void)
{
	struct hard_trap_info *ht;
	unsigned long flags;

	local_irq_save(flags);
	for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
		set_except_vector(ht->tt, saved_vectors[ht->tt]);
	local_irq_restore(flags);
}
#endif

void regs_to_gdb_regs(unsigned long *gdb_regs, struct pt_regs *regs)
{
	int reg = 0;
	unsigned long *ptr = gdb_regs;

	for (reg = 0; reg < 32; reg++)
		*(ptr++) = regs->regs[reg];

	*(ptr++) = regs->cp0_status;
	*(ptr++) = regs->lo;
	*(ptr++) = regs->hi;
	*(ptr++) = regs->cp0_badvaddr;
	*(ptr++) = regs->cp0_cause;
	*(ptr++) = regs->cp0_epc;

	return;
}

void gdb_regs_to_regs(unsigned long *gdb_regs, struct pt_regs *regs)
{
	int reg = 0;
	unsigned long *ptr = gdb_regs;

	for (reg = 0; reg < 32; reg++)
		regs->regs[reg] = *(ptr++);

	regs->cp0_status = *(ptr++);
	regs->lo = *(ptr++);
	regs->hi = *(ptr++);
	regs->cp0_badvaddr = *(ptr++);
	regs->cp0_cause = *(ptr++);
	regs->cp0_epc = *(ptr++);

	return;
}

/*
 * Similar to regs_to_gdb_regs() except that process is sleeping and so
 * we may not be able to get all the info.
 */
void sleeping_thread_to_gdb_regs(unsigned long *gdb_regs, struct task_struct *p)
{
	int reg = 0;
	struct thread_info *ti = p->thread_info;
	unsigned long ksp = (unsigned long)ti + THREAD_SIZE - 32;
	struct pt_regs *regs = (struct pt_regs *)ksp - 1;
	unsigned long *ptr = gdb_regs;

	for (reg = 0; reg < 16; reg++)
		*(ptr++) = regs->regs[reg];

	/* S0 - S7 */
	for (reg = 16; reg < 24; reg++)
		*(ptr++) = regs->regs[reg];

	for (reg = 24; reg < 28; reg++)
		*(ptr++) = 0;

	/* GP, SP, FP, RA */
	for (reg = 28; reg < 32; reg++)
		*(ptr++) = regs->regs[reg];

	*(ptr++) = regs->cp0_status;
	*(ptr++) = regs->lo;
	*(ptr++) = regs->hi;
	*(ptr++) = regs->cp0_badvaddr;
	*(ptr++) = regs->cp0_cause;
	*(ptr++) = regs->cp0_epc;

	return;
}

/*
 * Calls linux_debug_hook before the kernel dies. If KGDB is enabled,
 * then try to fall into the debugger
 */
static int kgdb_mips_notify(struct notifier_block *self, unsigned long cmd,
			    void *ptr)
{
	struct die_args *args = (struct die_args *)ptr;
	struct pt_regs *regs = args->regs;
	int trap = (regs->cp0_cause & 0x7c) >> 2;

	/* See if KGDB is interested. */
	if (user_mode(regs))
		/* Userpace events, ignore. */
		return NOTIFY_DONE;

	kgdb_handle_exception(trap, compute_signal(trap), 0, regs);
	return NOTIFY_OK;
}

static struct notifier_block kgdb_notifier = {
	.notifier_call = kgdb_mips_notify,
};

/*
 * Handle the 's' and 'c' commands
 */
int kgdb_arch_handle_exception(int vector, int signo, int err_code,
			       char *remcom_in_buffer, char *remcom_out_buffer,
			       struct pt_regs *regs)
{
	char *ptr;
	unsigned long address;
	int cpu = smp_processor_id();

	switch (remcom_in_buffer[0]) {
	case 's':
	case 'c':
		/* handle the optional parameter */
		ptr = &remcom_in_buffer[1];
		if (kgdb_hex2long(&ptr, &address))
			regs->cp0_epc = address;

		atomic_set(&cpu_doing_single_step, -1);
		if (remcom_in_buffer[0] == 's')
			if (kgdb_contthread)
				atomic_set(&cpu_doing_single_step, cpu);

		return 0;
	}

	return -1;
}

struct kgdb_arch arch_kgdb_ops = {
#ifdef CONFIG_CPU_LITTLE_ENDIAN
	.gdb_bpt_instr = {0xd},
#else
	.gdb_bpt_instr = {0x00, 0x00, 0x00, 0x0d},
#endif
};

/*
 * We use kgdb_early_setup so that functions we need to call now don't
 * cause trouble when called again later.
 */
int kgdb_arch_init(void)
{
	/* Board-specifics. */
	/* Force some calls to happen earlier. */
	if (kgdb_early_setup == 0) {
		trap_init();
		init_IRQ();
		kgdb_early_setup = 1;
	}

	/* Set our traps. */
	/* This needs to be done more finely grained again, paired in
	 * a before/after in kgdb_handle_exception(...) -- Tom */
	set_debug_traps();
	notifier_chain_register(&mips_die_chain, &kgdb_notifier);

	return 0;
}
