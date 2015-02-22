/*
 * arch/mips/kernel/kgdb-jmp.c
 *
 * Save and restore system registers so that within a limited frame we
 * may have a fault and "jump back" to a known safe location.
 *
 * Copyright (C) 2005 by MontaVista Software.
 * Author: Manish Lachwani (mlachwani@mvista.com)
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program as licensed "as is" without any warranty of
 * any kind, whether express or implied.
 */

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

ENTRY (kgdb_fault_setjmp)
	move    a1, sp
	move	a2, fp
#ifdef CONFIG_MIPS64
	nop
#endif
	j	kgdb_fault_setjmp_aux
	.end	kgdb_fault_setjmp
