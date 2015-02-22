/*
 * arch/arm/kernel/kgdb.c
 *
 * ARM KGDB support
 *
 * Copyright (c) 2002-2004 MontaVista Software, Inc
 *
 * Authors:  George Davis <davis_g@mvista.com>
 *           Deepak Saxena <dsaxena@plexity.net>
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/elf.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/kgdb.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/ptrace.h>
#include <asm/traps.h>

/* Make a local copy of the registers passed into the handler (bletch) */
void regs_to_gdb_regs(unsigned long *gdb_regs, struct pt_regs *kernel_regs)
{
	int regno;

	/* Initialize all to zero (??) */
	for (regno = 0; regno < GDB_MAX_REGS; regno++)
		gdb_regs[regno] = 0;

	gdb_regs[_R0] = kernel_regs->ARM_r0;
	gdb_regs[_R1] = kernel_regs->ARM_r1;
	gdb_regs[_R2] = kernel_regs->ARM_r2;
	gdb_regs[_R3] = kernel_regs->ARM_r3;
	gdb_regs[_R4] = kernel_regs->ARM_r4;
	gdb_regs[_R5] = kernel_regs->ARM_r5;
	gdb_regs[_R6] = kernel_regs->ARM_r6;
	gdb_regs[_R7] = kernel_regs->ARM_r7;
	gdb_regs[_R8] = kernel_regs->ARM_r8;
	gdb_regs[_R9] = kernel_regs->ARM_r9;
	gdb_regs[_R10] = kernel_regs->ARM_r10;
	gdb_regs[_FP] = kernel_regs->ARM_fp;
	gdb_regs[_IP] = kernel_regs->ARM_ip;
	gdb_regs[_SP] = kernel_regs->ARM_sp;
	gdb_regs[_LR] = kernel_regs->ARM_lr;
	gdb_regs[_PC] = kernel_regs->ARM_pc;
	gdb_regs[_CPSR] = kernel_regs->ARM_cpsr;
}

/* Copy local gdb registers back to kgdb regs, for later copy to kernel */
void gdb_regs_to_regs(unsigned long *gdb_regs, struct pt_regs *kernel_regs)
{
	kernel_regs->ARM_r0 = gdb_regs[_R0];
	kernel_regs->ARM_r1 = gdb_regs[_R1];
	kernel_regs->ARM_r2 = gdb_regs[_R2];
	kernel_regs->ARM_r3 = gdb_regs[_R3];
	kernel_regs->ARM_r4 = gdb_regs[_R4];
	kernel_regs->ARM_r5 = gdb_regs[_R5];
	kernel_regs->ARM_r6 = gdb_regs[_R6];
	kernel_regs->ARM_r7 = gdb_regs[_R7];
	kernel_regs->ARM_r8 = gdb_regs[_R8];
	kernel_regs->ARM_r9 = gdb_regs[_R9];
	kernel_regs->ARM_r10 = gdb_regs[_R10];
	kernel_regs->ARM_fp = gdb_regs[_FP];
	kernel_regs->ARM_ip = gdb_regs[_IP];
	kernel_regs->ARM_sp = gdb_regs[_SP];
	kernel_regs->ARM_lr = gdb_regs[_LR];
	kernel_regs->ARM_pc = gdb_regs[_PC];
	kernel_regs->ARM_cpsr = gdb_regs[GDB_MAX_REGS - 1];
}

static inline struct pt_regs *kgdb_get_user_regs(struct task_struct *task)
{
	return (struct pt_regs *)
	    ((unsigned long)task->thread_info + THREAD_SIZE -
	     8 - sizeof(struct pt_regs));
}

void sleeping_thread_to_gdb_regs(unsigned long *gdb_regs,
				 struct task_struct *task)
{
	int regno;
	struct pt_regs *thread_regs;

	/* Just making sure... */
	if (task == NULL)
		return;

	/* Initialize to zero */
	for (regno = 0; regno < GDB_MAX_REGS; regno++)
		gdb_regs[regno] = 0;

	/* Otherwise, we have only some registers from switch_to() */
	thread_regs = kgdb_get_user_regs(task);
	gdb_regs[_R0] = thread_regs->ARM_r0;	/* Not really valid? */
	gdb_regs[_R1] = thread_regs->ARM_r1;	/* "               " */
	gdb_regs[_R2] = thread_regs->ARM_r2;	/* "               " */
	gdb_regs[_R3] = thread_regs->ARM_r3;	/* "               " */
	gdb_regs[_R4] = thread_regs->ARM_r4;
	gdb_regs[_R5] = thread_regs->ARM_r5;
	gdb_regs[_R6] = thread_regs->ARM_r6;
	gdb_regs[_R7] = thread_regs->ARM_r7;
	gdb_regs[_R8] = thread_regs->ARM_r8;
	gdb_regs[_R9] = thread_regs->ARM_r9;
	gdb_regs[_R10] = thread_regs->ARM_r10;
	gdb_regs[_FP] = thread_regs->ARM_fp;
	gdb_regs[_IP] = thread_regs->ARM_ip;
	gdb_regs[_SP] = thread_regs->ARM_sp;
	gdb_regs[_LR] = thread_regs->ARM_lr;
	gdb_regs[_PC] = thread_regs->ARM_pc;
	gdb_regs[_CPSR] = thread_regs->ARM_cpsr;
}

static int compiled_break;

int kgdb_arch_handle_exception(int exception_vector, int signo,
			       int err_code, char *remcom_in_buffer,
			       char *remcom_out_buffer,
			       struct pt_regs *linux_regs)
{
	long addr;
	char *ptr;

	switch (remcom_in_buffer[0]) {
	case 'c':
		kgdb_contthread = NULL;

		/*
		 * Try to read optional parameter, pc unchanged if no parm.
		 * If this was a compiled breakpoint, we need to move
		 * to the next instruction or we will just breakpoint
		 * over and over again.
		 */
		ptr = &remcom_in_buffer[1];
		if (kgdb_hex2long(&ptr, &addr)) {
			linux_regs->ARM_pc = addr;
		} else if (compiled_break == 1) {
			linux_regs->ARM_pc += 4;
		}

		compiled_break = 0;

		return 0;
	}

	return -1;
}

static int kgdb_brk_fn(struct pt_regs *regs, unsigned int instr)
{
	kgdb_handle_exception(1, SIGTRAP, 0, regs);

	return 0;
}

static int kgdb_compiled_brk_fn(struct pt_regs *regs, unsigned int instr)
{
	compiled_break = 1;
	kgdb_handle_exception(1, SIGTRAP, 0, regs);

	return 0;
}

static struct undef_hook kgdb_brkpt_hook = {
	.instr_mask = 0xffffffff,
	.instr_val = KGDB_BREAKINST,
	.fn = kgdb_brk_fn
};

static struct undef_hook kgdb_compiled_brkpt_hook = {
	.instr_mask = 0xffffffff,
	.instr_val = KGDB_COMPILED_BREAK,
	.fn = kgdb_compiled_brk_fn
};

/*
 * Register our undef instruction hooks with ARM undef core.
 * We regsiter a hook specifically looking for the KGB break inst
 * and we handle the normal undef case within the do_undefinstr
 * handler.
 */
int kgdb_arch_init(void)
{
	register_undef_hook(&kgdb_brkpt_hook);
	register_undef_hook(&kgdb_compiled_brkpt_hook);

	return 0;
}

struct kgdb_arch arch_kgdb_ops = {
#ifndef __ARMEB__
	.gdb_bpt_instr = {0xfe, 0xde, 0xff, 0xe7}
#else
	.gdb_bpt_instr = {0xe7, 0xff, 0xde, 0xfe}
#endif
};
