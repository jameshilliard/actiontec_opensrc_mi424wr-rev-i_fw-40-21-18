/*
 * arch/arm/mach-feroceon-kw/sleep.S
 *
 * Lower level support for Marvell Orion System On Chip
 *
 * Maintainer: Tawfik Bayouk <tawfik@marvell.com>
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/domain.h>
#include <asm/pgtable-hwdef.h>
#include <asm/system.h>
#include "config/mvSysHwConfig.h"


	.text

#define	FUNC(x) ENTRY(x)

#define	FUNC_END(x)

#define CHANGE_MODE(x) \
	mov	r1, r0		; \
	bic	r1, r1, #0x1f	; \
	orr	r1, r1, #0x##x	; \
	msr	cpsr_c, r1

# r0 - The address to map 0xF1000000 to.
FUNC(kw2_cpu_map_regs)
	stmfd	sp!, {lr}
	mrc 	p15, 0, r1, c2, c0, 0		@ Get TTBR
	ldr	r2, =INTER_REGS_BASE
	lsr	r2, r2, #18
	orr	r2, r2, #0xC0000000		@ Add kernel virtual Base.
	orr	r1, r1, r2			@ Add TTBR base.
	ldr	r2, [r1]
	ldr	r3, =0xFFF
	and	r2, r2, r3
	orr	r0, r0, r2
	str	r0, [r1]
	# Flush Caches So that all is writen back to DRAM
	bl	feroceon_flush_kern_cache_all
#if defined(CONFIG_L2_CACHE_ENABLE) || defined(CONFIG_CACHE_FEROCEON_L2)
	mov	r0, #0
	mcr	p15, 1, r0, c15, c9, 0		@ clean entire L2 cache
	mcr	p15, 0, r0, c7, c10, 4		@ Drain write buffer
#endif
#ifdef CONFIG_MMU
	mcr	p15, 0, r0, c8, c7, 0		@ invalidate I,D TLBs on v4
#endif

	ldmfd	sp!, {pc}
FUNC_END(kw2_cpu_suspend)

/*
 * Suspend execution and enter STANDBY or SLEEP mode
 */
FUNC(kw2_cpu_suspend)
	stmfd	sp!, {r4-r12, lr}		@ save general regs
	mrc 	p15, 0, r2, c1, c0, 0		@ save CP15 - CR
	mrc 	p15, 0, r3, c3, c0, 0		@ save CP15 - DACR
	mrc 	p15, 0, r4, c13, c0, 0		@ save CP15 - FCSE
	mrc 	p15, 0, r5, c2, c0, 0		@ save CP15 - TTBR
	mov	r6, r13				@ save resume SP
	mrc	p15, 1, r7, c15, c1, 0		@ save Marvell extra features
	ldr	r0, =(INTER_REGS_BASE + 0x20128)@ save L2 Config Reg
	ldr	r8, [r0]
	stmfd	sp!, {r2-r8}

	/*
	 * Save the physical address of the resume SP
	 */
	mov	r0, sp
	bl	suspend_phys_addr
	ldr	r1, =suspend_saved_sp
	str	r0, [r1]

	/*
	 * Flush Caches So that all is writen back to DRAM
	 */
	bl	feroceon_flush_kern_cache_all

#if defined(CONFIG_L2_CACHE_ENABLE) || defined(CONFIG_CACHE_FEROCEON_L2)
	mov	r0, #0
	mcr	p15, 1, r0, c15, c9, 0		@ clean entire L2 cache
	mcr	p15, 0, r0, c7, c10, 4		@ Drain write buffer
#endif

	ldr	r1, =(INTER_REGS_BASE + 0x200F0)
	ldr	r2, =(0xD0000000)
	str	r2, [r1]

#	b	1f				@ cache alligned

#	.align 5
#	str	r2, [r3]			@ Self refresh command
1:	mcr	p15, 0, r0, c7, c10, 4		@ Drain write buffer
	mov	r0, #0x0
	mcr	p15, 0, r0, c7, c0, 4		@ Wait for interrupt

	# We returned immediately from suspend, restore internal regs base &
	# stack pointer.
	ldr	r0, =0xD0000000
	bl	kw2_cpu_map_regs		@ Map 0xF1000000 --> 0xD0000000 in MMU.

	ldr	r0, =(INTER_REGS_BASE + 0x200F0)
	ldr	r1, =INTER_REGS_BASE
	str	r1, [r0]			@ restore regs base addr

	ldr	r0, =INTER_REGS_BASE
	bl	kw2_cpu_map_regs		@ Map 0xF1000000 --> 0xF1000000 in MMU.

##	ldr	r0, =(INTER_REGS_BASE + 0x12000)
##	mov	r1, #'F'
##	str	r1, [r0]

	mov	r0, #1
	ldmfd	sp!, {r2-r8}
	ldmfd	sp!, {r4-r12, pc}

FUNC_END(kw2_cpu_suspend)


/*
 * Resume from SLEEP or STANDBY suspend
 */
	.data
	.align 5
FUNC(kw2_cpu_resume)
	msr	cpsr_c, #PSR_F_BIT | PSR_I_BIT | SVC_MODE
##	ldr	r0, =(0xD0000000 + 0x12000)
##	mov	r1, #'O'
##	str	r1, [r0]

	ldr	r0, =0xD00200F0
	ldr	r1, =INTER_REGS_BASE
	str	r1, [r0]			@ restore regs base addr

	mov	r0, #0
	mcr	p15, 0, r0, c7, c7, 0		@ invalidate I,D caches on v4
	mcr	p15, 0, r0, c7, c10, 4		@ drain write buffer on v4
#ifdef CONFIG_MMU
	mcr	p15, 0, r0, c8, c7, 0		@ invalidate I,D TLBs on v4
#endif
#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
	mov	r0, #4				@ disable write-back on caches explicitly
	mcr	p15, 7, r0, c15, c0, 0
#endif 

	ldr	r0, suspend_saved_sp		@ stack phys addr
	ldmfd	r0, {r2-r8}
	ldr	r0, =(INTER_REGS_BASE + 0x20128)
	str	r8, [r0]			@ restorem L2 Config Reg
	mcr	p15, 1, r7, c15, c1, 0		@ restore Marvell Extra Features
	mcr 	p15, 0, r5, c2, c0, 0		@ restore CP15 - TTBR
	mcr 	p15, 0, r4, c13, c0, 0		@ restore CP15 - FCSE
	mcr 	p15, 0, r3, c3, c0, 0		@ restore CP15 - DACR
	ldr	r3, =resume_after_mmu		@ virtual address jump
	b	resume_turn_on_mmu		@ cache align

	.align 5
resume_turn_on_mmu:
	mcr 	p15, 0, r2, c1, c0, 0		@ restore CP15 - CR
	nop
	mov	pc, r3				@ jump to virtual addr
	nop
	nop
	nop

suspend_saved_sp:
	.word	0				@ physical SP saved here

	.text
resume_after_mmu:
	mov	r13, r6				@ restore virtual SP
	mov	r0, #0x0
	ldmfd	sp!, {r4-r12, pc}		@ restore SVC registers
FUNC_END(kw2_cpu_resume)

	.end
