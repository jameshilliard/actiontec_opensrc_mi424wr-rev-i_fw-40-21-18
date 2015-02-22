/*
 * arch/mips/kernel/kgdb-jmp.c
 *
 * Save and restore system registers so that within a limited frame we
 * may have a fault and "jump back" to a known safe location.
 *
 * Author: Tom Rini <trini@kernel.crashing.org>
 * Author: Manish Lachwani <mlachwani@mvista.com>
 *
 * Cribbed from glibc, which carries the following:
 * Copyright (C) 1996, 1997, 2000, 2002, 2003 Free Software Foundation, Inc.
 * Copyright (C) 2005 by MontaVista Software.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program as licensed "as is" without any warranty of
 * any kind, whether express or implied.
 */

#include <linux/kgdb.h>
#include <asm/interrupt.h>

#ifdef CONFIG_MIPS64
/*
 * MIPS 64-bit
 */

int kgdb_fault_setjmp_aux(unsigned long *curr_context, int sp, int fp)
{
	__asm__ __volatile__ ("sd $gp, %0" : : "m" (curr_context[0]));
	__asm__ __volatile__ ("sd $16, %0" : : "m" (curr_context[1]));
	__asm__ __volatile__ ("sd $17, %0" : : "m" (curr_context[2]));
	__asm__ __volatile__ ("sd $18, %0" : : "m" (curr_context[3]));
	__asm__ __volatile__ ("sd $19, %0" : : "m" (curr_context[4]));
	__asm__ __volatile__ ("sd $20, %0" : : "m" (curr_context[5]));
	__asm__ __volatile__ ("sd $21, %0" : : "m" (curr_context[6]));
	__asm__ __volatile__ ("sd $22, %0" : : "m" (curr_context[7]));
	__asm__ __volatile__ ("sd $23, %0" : : "m" (curr_context[8]));
	__asm__ __volatile__ ("sd $31, %0" : : "m" (curr_context[9]));
	curr_context[10] = (long *)sp;
	curr_context[11] = (long *)fp;

	return 0;
}

void kgdb_fault_longjmp(unsigned long *curr_context)
{
	unsigned long sp_val, fp_val;

	__asm__ __volatile__ ("ld $gp, %0" : : "m" (curr_context[0]));
	__asm__ __volatile__ ("ld $16, %0" : : "m" (curr_context[1]));
	__asm__ __volatile__ ("ld $17, %0" : : "m" (curr_context[2]));
	__asm__ __volatile__ ("ld $18, %0" : : "m" (curr_context[3]));
	__asm__ __volatile__ ("ld $19, %0" : : "m" (curr_context[4]));
	__asm__ __volatile__ ("ld $20, %0" : : "m" (curr_context[5]));
	__asm__ __volatile__ ("ld $21, %0" : : "m" (curr_context[6]));
	__asm__ __volatile__ ("ld $22, %0" : : "m" (curr_context[7]));
	__asm__ __volatile__ ("ld $23, %0" : : "m" (curr_context[8]));
	__asm__ __volatile__ ("ld $25, %0" : : "m" (curr_context[9]));
	sp_val = curr_context[10];
	fp_val = curr_context[11];
	__asm__ __volatile__ ("ld $29, %0\n\t"
			      "ld $30, %1\n\t" : : "m" (sp_val), "m" (fp_val));

	__asm__ __volatile__ ("dli $2, 1");
	__asm__ __volatile__ ("j $25");

	for (;;);
}
#else
/*
 * MIPS 32-bit
 */

int kgdb_fault_setjmp_aux(unsigned long *curr_context, int sp, int fp)
{
	__asm__ __volatile__("sw $gp, %0" : : "m" (curr_context[0]));
	__asm__ __volatile__("sw $16, %0" : : "m" (curr_context[1]));
	__asm__ __volatile__("sw $17, %0" : : "m" (curr_context[2]));
	__asm__ __volatile__("sw $18, %0" : : "m" (curr_context[3]));
	__asm__ __volatile__("sw $19, %0" : : "m" (curr_context[4]));
	__asm__ __volatile__("sw $20, %0" : : "m" (curr_context[5]));
	__asm__ __volatile__("sw $21, %0" : : "m" (curr_context[6]));
	__asm__ __volatile__("sw $22, %0" : : "m" (curr_context[7]));
	__asm__ __volatile__("sw $23, %0" : : "m" (curr_context[8]));
	__asm__ __volatile__("sw $31, %0" : : "m" (curr_context[9]));
	curr_context[10] = (long *)sp;
	curr_context[11] = (long *)fp;

	return 0;
}

void kgdb_fault_longjmp(unsigned long *curr_context)
{
	unsigned long sp_val, fp_val;

	__asm__ __volatile__("lw $gp, %0" : : "m" (curr_context[0]));
	__asm__ __volatile__("lw $16, %0" : : "m" (curr_context[1]));
	__asm__ __volatile__("lw $17, %0" : : "m" (curr_context[2]));
	__asm__ __volatile__("lw $18, %0" : : "m" (curr_context[3]));
	__asm__ __volatile__("lw $19, %0" : : "m" (curr_context[4]));
	__asm__ __volatile__("lw $20, %0" : : "m" (curr_context[5]));
	__asm__ __volatile__("lw $21, %0" : : "m" (curr_context[6]));
	__asm__ __volatile__("lw $22, %0" : : "m" (curr_context[7]));
	__asm__ __volatile__("lw $23, %0" : : "m" (curr_context[8]));
	__asm__ __volatile__("lw $25, %0" : : "m" (curr_context[9]));
	sp_val = curr_context[10];
	fp_val = curr_context[11];
	__asm__ __volatile__("lw $29, %0\n\t"
			      "lw $30, %1\n\t" : : "m" (sp_val), "m" (fp_val));

	__asm__ __volatile__("li $2, 1");
	__asm__ __volatile__("jr $25");

	for (;;);
}
#endif
