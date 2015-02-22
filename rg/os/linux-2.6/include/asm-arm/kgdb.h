/*
 * include/asm-arm/kgdb.h
 *
 * ARM KGDB support
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2002 MontaVista Software Inc.
 *
 */

#ifndef __ASM_KGDB_H__
#define __ASM_KGDB_H__

#include <linux/config.h>
#include <asm/ptrace.h>


/*
 * GDB assumes that we're a user process being debugged, so
 * it will send us an SWI command to write into memory as the
 * debug trap. When an SWI occurs, the next instruction addr is
 * placed into R14_svc before jumping to the vector trap.
 * This doesn't work for kernel debugging as we are already in SVC
 * we would loose the kernel's LR, which is a bad thing. This
 * is  bad thing.
 *
 * By doing this as an undefined instruction trap, we force a mode
 * switch from SVC to UND mode, allowing us to save full kernel state.
 *
 * We also define a KGDB_COMPILED_BREAK which can be used to compile
 * in breakpoints. This is important for things like sysrq-G and for
 * the initial breakpoint from trap_init().
 *
 * Note to ARM HW designers: Add real trap support like SH && PPC to
 * make our lives much much simpler. :)
 */
#define	BREAK_INSTR_SIZE		4
#define GDB_BREAKINST                   0xef9f0001
#define KGDB_BREAKINST                  0xe7ffdefe
#define KGDB_COMPILED_BREAK             0xe7ffdeff
#define CACHE_FLUSH_IS_SAFE		1

#ifndef	__ASSEMBLY__

#define	BREAKPOINT()			asm(".word 	0xe7ffdeff")


extern void kgdb_handle_bus_error(void);
extern int kgdb_fault_expected;

/*
 * From Amit S. Kale:
 *
 * In the register packet, words 0-15 are R0 to R10, FP, IP, SP, LR, PC. But
 * Register 16 isn't cpsr. GDB passes CPSR in word 25. There are 9 words in
 * between which are unused. Passing only 26 words to gdb is sufficient.
 * GDB can figure out that floating point registers are not passed.
 * GDB_MAX_REGS should be 26.
 */
#define	GDB_MAX_REGS		(26)

#define	KGDB_MAX_NO_CPUS	1
#define	BUFMAX			400
#define	NUMREGBYTES		(GDB_MAX_REGS << 2)
#define	NUMCRITREGBYTES		(32 << 2)
#define	CHECK_EXCEPTION_STACK()	1

#define	_R0		0
#define	_R1		1
#define	_R2		2
#define	_R3		3
#define	_R4		4
#define	_R5		5
#define	_R6		6
#define	_R7		7
#define	_R8		8
#define	_R9		9
#define	_R10		10
#define	_FP		11
#define	_IP		12
#define	_SP		13
#define	_LR		14
#define	_PC		15
#define	_CPSR		(GDB_MAX_REGS - 1)

#define	CHECK_EXCEPTION_STACK()	1

#endif // !__ASSEMBLY__
#endif // __ASM_KGDB_H__
