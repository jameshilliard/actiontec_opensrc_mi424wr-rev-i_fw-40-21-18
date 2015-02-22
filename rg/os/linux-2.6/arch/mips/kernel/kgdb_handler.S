/*
 * arch/mips/kernel/kgdb_handler.S
 *
 * Copyright (C) 2004-2005 MontaVista Software Inc.
 * Author: Manish Lachwani, mlachwani@mvista.com or manish@koffee-break.com
 *
 * This file is licensed under the terms of the GNU General Public
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/*
 * Trap Handler for the new KGDB framework. The main KGDB handler is
 * handle_exception that will be called from here
 *
 */

#include <linux/config.h>
#include <linux/sys.h>

#include <asm/asm.h>
#include <asm/errno.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	.align  5
	NESTED(trap_low, PT_SIZE, sp)
		.set    noat
		.set	noreorder

		/*
		 * Check for privileged instructions in user mode. For
		 * this, check the cu0 bit in the CPU status register.
		 */
		mfc0	k0, CP0_STATUS
		sll	k0, 3
		bltz	k0, 1f
		move	k1, sp

		/*
		 * GDB userland from within KGDB. If a user mode address
		 * then jump to the saved exception handler
		 */
		mfc0	k1, CP0_CAUSE
		andi	k1, k1, 0x7c
		PTR_L	k0, saved_vectors(k1)
		jr	k0
		nop
1:
		SAVE_ALL
		.set    at
		.set	reorder
		move    a0, sp
		jal     handle_exception
		j       ret_from_exception
	END(trap_low)
