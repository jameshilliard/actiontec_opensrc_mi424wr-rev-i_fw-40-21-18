/*
 *  cadence_ipsec2_reverse.S
 *
 *  Copyright (C) Mindspeed Technologies
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/linkage.h>
#include <asm/assembler.h>

		.text

#define ENTER	\
		mov	ip,sp	;\
		stmfd	sp!,{r0,r4-r9,fp,ip,lr,pc}	;\
		sub	fp,ip,#4

#define EXIT	\
		LOADREGS(ea, fp, {r0, r4 - r9, fp, sp, pc})

#define EXITEQ	\
		LOADREGS(eqea, fp, {r0, r4 - r9, fp, sp, pc})

/*
 * Prototype: void memswap64(void *to,const void *from,unsigned long n);
 */
ENTRY(memswap64)
		ENTER
		ands 	r3, r2, #7	@ length multiple of 64 bits
		bne	7f
		subs	r2, r2, #8	@ do we have at least 8 bytes (64 bits)	
		blt	7f
		ands	ip, r0, #3	@ dst aligned on 32 bits ?
		bne	7f				@ exit
		ands	ip, r1, #3	@ src aligned on 32 bits ?
		bne	7f				@ exit

		mov 	ip,#0xff
		orr		ip,ip,#0xff0000	@ ip = 0x00ff00ff
		
1:		subs	r2, r2,	#8  @ at least 2 blocks ?
		blt	5f				@ 1 block
		subs	r2, r2, #8  @at least  3 blocks
		blt	4f				@ 2 blocks
		
2:	
		ldmia	r1!, {r3 - r8}	@ load 24 bytes -> 3 blocks
								@ swap each registers (r3, r4, r5, r6, r7, r8)
									@ rx = A B C D
		and		r9, ip, r3			@ r9 = 0 B 0 D
		and		r3, ip, r3, ror #24 @ rx = 0 C 0 A
		orr 	lr, r3, r9, ror #8	@ rx = D C B A
		and		r9, ip, r4			@ r9 = 0 B 0 D
		and		r4, ip, r4, ror #24 @ rx = 0 C 0 A
		orr 	r3, r4, r9, ror #8	@ rx = D C B A
		mov 	r4,	lr
		and		r9, ip, r5			@ r9 = 0 B 0 D
		and		r5, ip, r5, ror #24 @ rx = 0 C 0 A
		orr 	lr, r5, r9, ror #8	@ rx = D C B A
		and		r9, ip, r6			@ r9 = 0 B 0 D
		and		r6, ip, r6, ror #24 @ rx = 0 C 0 A
		orr 	r5, r6, r9, ror #8	@ rx = D C B A
		mov		r6, lr
		and		r9, ip, r7			@ r9 = 0 B 0 D
		and		r7, ip, r7, ror #24 @ rx = 0 C 0 A
		orr 	lr, r7, r9, ror #8	@ rx = D C B A
		and		r9, ip, r8			@ r9 = 0 B 0 D
		and		r8, ip, r8, ror #24 @ rx = 0 C 0 A
		orr 	r7, r8, r9, ror #8	@ rx = D C B A	
		mov		r8, lr
		stmia	r0!, {r3 - r8}
/*		str	r4, [r0],#4
*		str	r3, [r0],#4
*		str	r6, [r0],#4
*		str	r5, [r0],#4
*		str	r8, [r0],#4
*		str	r7, [r0],#4
*/
		subs	r2, r2, #24
		bge	2b
		
		adds	r2,r2,	#24
		beq		7f					@finished
		cmp		r2, #8
		beq		5f					@ 1 block remaining

4:		ldmia	r1!, {r3 - r6}		@ 2 block remainings
									@ rx = A B C D
		and		r9, ip, r3			@ r9 = 0 B 0 D
		and		r3, ip, r3, ror #24 @ rx = 0 C 0 A
		orr 	lr, r3, r9, ror #8	@ rx = D C B A
		and		r9, ip, r4			@ r9 = 0 B 0 D
		and		r4, ip, r4, ror #24 @ rx = 0 C 0 A
		orr	 	r3, r4, r9, ror #8	@ rx = D C B A
		mov 	r4, lr
		and		r9, ip, r5			@ r9 = 0 B 0 D
		and		r5, ip, r5, ror #24 @ rx = 0 C 0 A
		orr 	lr, r5, r9, ror #8	@ rx = D C B A
		and		r9, ip, r6			@ r9 = 0 B 0 D
		and		r6, ip, r6, ror #24 @ rx = 0 C 0 A
		orr	 	r5, r6, r9, ror #8	@ rx = D C B A
		mov 	r6, lr
		stmia	r0!, {r3 - r6}
		EXIT

		
5:		
		ldmia	r1!, {r3 - r4}
									@ rx = A B C D
		and		r9, ip, r3			@ r9 = 0 B 0 D
		and		r3, ip, r3, ror #24 @ rx = 0 C 0 A
		orr 	lr, r3, r9, ror #8	@ rx = D C B A
		and		r9, ip, r4			@ r9 = 0 B 0 D
		and		r4, ip, r4, ror #24 @ rx = 0 C 0 A
		orr 	r3, r4, r9, ror #8	@ rx = D C B A
		mov		r4, lr
		stmia	r0!, {r3 - r4}
7:		EXIT




/*
 * Prototype: void memswap128(void *to,const void *from,unsigned long n);
 */
ENTRY(memswap128)
		ENTER
		ands 	r3, r2, #15 @ length multiple of 128 bits
		bne	70f
		subs	r2, r2, #16	@ do we have at least 16 bytes (128 bits)	
		blt	70f
		ands	ip, r0, #3	@ dst aligned on 32 bits ?
		bne	70f				@ exit
		ands	ip, r1, #3	@ src aligned on 32 bits ?
		bne	70f				@ exit

		mov 	ip,#0xff
		orr		ip,ip,#0xff0000	@ ip = 0x00ff00ff
		
20:	
		ldmia	r1!, {r3 - r6}	@ load 16 bytes -> 1 blocks
								@ swap each registers (r3, r4, r5, r6)
									@ rx = A B C D
		and		r9, ip, r3			@ r9 = 0 B 0 D
		and		r3, ip, r3, ror #24 @ rx = 0 C 0 A
		orr 	lr, r3, r9, ror #8	@ rx = D C B A
		and		r9, ip, r6			@ r9 = 0 B 0 D
		and		r6, ip, r6, ror #24 @ rx = 0 C 0 A
		orr 	r3, r6, r9, ror #8	@ rx = D C B A
		mov		r6, lr
		and		r9, ip, r5			@ r9 = 0 B 0 D
		and		r5, ip, r5, ror #24 @ rx = 0 C 0 A
		orr 	lr, r5, r9, ror #8	@ rx = D C B A
		and		r9, ip, r4			@ r9 = 0 B 0 D
		and		r4, ip, r4, ror #24 @ rx = 0 C 0 A
		orr 	r5, r4, r9, ror #8	@ rx = D C B A
		mov 	r4,	lr
		stmia	r0!, {r3 - r6}
		subs	r2, r2, #16
		bge	20b

70:		EXIT


