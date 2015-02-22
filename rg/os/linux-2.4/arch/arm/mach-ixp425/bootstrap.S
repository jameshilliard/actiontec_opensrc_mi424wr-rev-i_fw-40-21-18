#include <asm/assembler.h>
#include <asm/arch/hardware.h>

#include <asm/arch/asmutils.S>

	.section ".bootstrap",#alloc,#execinstr
_ixp425_start:
	mov	r12, r0
	mov	r0, #F_BIT | I_BIT | MODE_SVC	@ make sure svc mode
	msr	cpsr_c, r0			@ and all irqs disabled

/* Disable MMU */
	mov	r0, #(0xF<<3)
	orr	r0, r0, #MMU_Control_B
	mcr	p15, 0, r0, c1, c0, 0
	CP_WAIT	r0

/* Set process ID register to zero */
	mov	r0, #0x0
	mcr	p15, 0, r0, c13, c0, 0
	CP_WAIT r0

/* Invalidate I-cache, D-cache and BTB */
	mcr	p15, 0, r0, c7, c7, 0
	CP_WAIT	r0

/* Drain write & fill buffer */
	DRAIN
	CP_WAIT r0

/* Enable coprocessors (CP0) */
        mov     r0, #0x1
        mcr     p15, 0, r0, c15, c1, 0
        mcr     p15, 0, r0, c7, c10, 4
        CP_WAIT r0

/* The initialization procedure is taken from IXP425 Developer's manual,
 * 3.5.1.2 Initializing the SDRAM.
 */
	ldr	r1, =IXP425_SDRAM_DEF_CFG
        ldr	r2, =IXP425_SDRAM_CFG_PHYS
	str	r1, [r2]

/* Disable refresh cycles */
	ldr	r1, =IXP425_SDRAM_REFRESH_DISABLE
	ldr	r3, =IXP425_SDRAM_REFRESH_PHYS
	str	r1, [r3]

/* Send a NOP command */
	ldr	r1, =IXP425_SDRAM_IR_NOP_CMD
	ldr	r4, =IXP425_SDRAM_INSTRUCTION_PHYS
	str	r1, [r4]
	DELAY	0x4000, r0

/* Set SDRAM internal refresh value */
	ldr	r1, =IXP425_SDRAM_REFRESH_CNT
	str	r1, [r3]
	DELAY	0x4000, r0

/* Send a Precharge-All command to close all open banks */
	ldr	r1, =IXP425_SDRAM_IR_PRECHARGE_ALL_CMD
	str	r1, [r4]
	DELAY	0x4000, r0

/* Provide 8 Auto-Refresh cycles */
	ldr	r1, =IXP425_SDRAM_IR_AUTOREFRESH_CMD
	mov	r5, #8
111:	str	r1, [r4]
	DELAY	0x100, r0
	subs	r5, r5, #1
	bne	111b

/* Send CAS latency 3 */
	ldr	r1, =IXP425_SDRAM_IR_MODE_SET_CAS3_CMD
	str	r1, [r4]
	DELAY	0x4000, r0

/* Send Normal operation command */
	ldr	r1, =IXP425_SDRAM_IR_NORMAL_OPERATION_CMD
	str	r1, [r4]
	DELAY	0x4000, r0

/* Drain write & fill buffer */
	DRAIN
	CP_WAIT	r0

/* Set the CS0 to optimum timings */
	ldr	r1, =IXP425_FLASH_DEFAULT
	ldr	r2, =(IXP425_EXP_CFG_BASE_PHYS+IXP425_EXP_CS0_OFFSET)
	str	r1, [r2]

/* Continue execution in flash (high) */
	orr	pc, pc, #0x50000000
	
/* Invalidate I cache & BTB */
	mcr 	p15, 0, r0, c7, c5, 0
	CP_WAIT	r0
	
/* Initialize Expansion Bus Config Reg 0 */
	ldr	r2, =(IXP425_EXP_CFG_BASE_PHYS+IXP425_EXP_CFG0_OFFSET)
	ldr     r1, =IXP425_EXP_CFG0_INIT
	str	r1, [r2]

#ifdef CONFIG_IXP425_POST
	bl	_postStart
#endif

/* jump over to proceed with boot */
	b 	after_init

/* dump accumulated literal pool into the current section */
	.pool
after_init:
