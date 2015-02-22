/*
 *  linux/arch/arm/boot/bootp/init.S
 *
 *  Copyright (C) 2000-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Header file for splitting kernel + initrd.  Note that we pass
 *  r0 through to r3 straight through.
 */
		.section .start,#alloc,#execinstr
		.type	_entry, #function
_entry:		adr	r10, initdata
		ldr	r11, initdata
		sub	r11, r10, r11		@ work out exec offset
		b	splitify
		.size	_entry,. - _entry

		.type	initdata, #object
initdata:	.word	initdata		@ compiled address of this
		.size	initdata,. - initdata

		.text
splitify:	adr	r13, data
		ldmia	r13!, {r4-r6}		@ move the kernel
		mov	r12, r5
		add	r4, r4, r11		@ correction
move:		ldmia	r4!, {r7 - r10}		@ move 32-bytes at a time
		stmia	r5!, {r7 - r10}
		ldmia	r4!, {r7 - r10}
		stmia	r5!, {r7 - r10}
		subs	r6, r6, #8 * 4
		bcs	move
		mov	pc, r12			@ call kernel

data:		.word	kernel_start
		.word	kernel_addr
		.word	kernel_len
		.type	kernel_start,#object
