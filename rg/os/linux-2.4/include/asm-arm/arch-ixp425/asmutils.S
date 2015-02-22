/* this file is included from IXP425 bootloader code */

#define MMU_Control_M  0x001    /* MMU enable */
#define MMU_Control_A  0x002    /* Alignment fault enable */
#define MMU_Control_C  0x004    /* D cache enable */
#define MMU_Control_B  0x080    /* Big-Endian enable */
#define MMU_Control_S  0x100    /* System protection enable */
#define MMU_Control_R  0x200    /* ROM protection enable */
#define MMU_Control_I  0x1000   /* I cache enable */
#define MMU_Control_X  0x2000   /* Exception vector relocation enable */

#ifdef CONFIG_ARCH_IXP425_IXDP425
	.macro DISPLAY_INIT reg0, reg1
	ldr	\reg0, =0xBFFF0002
	ldr     \reg1, =(IXP425_EXP_CFG_BASE_PHYS+IXP425_EXP_CS2_OFFSET)
	str     \reg0, [\reg1]
	.endm

	.macro DISPLAY value, reg0 , reg1
	ldr 	\reg0, =\value
	ldr 	\reg1, =IXP425_EXP_BUS_CS2_BASE_PHYS
	strh 	\reg0, [\reg1]
	.endm

	.macro DISPLAY_REG_LO reg0 , reg1
	ldr 	\reg1, =IXP425_EXP_BUS_CS2_BASE_PHYS
	strh 	\reg0, [\reg1]
	.endm

	.macro DISPLAY_REG_HI reg0 , reg1
	ldr 	\reg1, =IXP425_EXP_BUS_CS2_BASE_PHYS
	mov	\reg0, \reg0, ROR #16
	strh 	\reg0, [\reg1]
	mov	\reg0, \reg0, ROR #16
	.endm
#else
	.macro DISPLAY_INIT reg0, reg1
	.endm

	.macro DISPLAY value, reg0 , reg1
	.endm
#endif
	
	.macro DELAY cycles, reg
	ldr	\reg, =\cycles
	subs	\reg, \reg, #1
	subne	pc, pc, #0xc
	.endm

	.macro	CP_WAIT reg
	mrc	p15, 0, \reg, c2, c0, 0
        mov	\reg, \reg	
	sub	pc, pc, #4
	.endm	

	.macro	ALLOCATE reg
	mcr	p15, 0, \reg, c7, c2, 5
	.endm

	.macro	DRAIN
	mcr	p15, 0, r0, c7, c10, 4
	.endm	

	.macro DCACHE_LOCK reg
	mov	\reg, #1
	mcr	p15, 0, \reg, c9, c2, 0				
	CP_WAIT	\reg
	.endm

	.macro DCACHE_UNLOCK reg
	mov	\reg, #0
	mcr	p15, 0, \reg, c9, c2, 0				
	CP_WAIT	\reg
	.endm


 	/* base = section base address (bits 31:20 of physical addr)
         * x    = XScale Extended bit - changes c & b interpretation if 1
         * ap   = Access Permissions - 00 = depends on S & R bits in CP15, reg1
         *                             01 = priv. r/w, no user access
         *                             10 = priv. r/w, user read only
         *                             11 = priv. r/w, user r/w
         * p    = ECC Parity: 1 = ECC memory, 0 = no ECC memory
         * d    = Domain: indicates 1 of 16 possible domains for entry
         * c    = Cacheable
         * b    = Bufferable (Fill and Write)
         *         cb = 00: Uncached/Unbuffered (writes occur immediately)
         *         cb = 01: Uncached/Buffered   (writes coalesce - delay a bit)
         *         cb = 10: Cached/Buffered     (write thru w/ coalescing)
         *         cb = 11: Cached/Buffered     (write back w/ coalescing)
         * form a first-level section entry
	 */
        /* never set the X bit */
        .macro FL_SECTION_ENTRY base,x,ap,p,d,c,b
        .word (\base << 20) | (\ap << 10) | (\p << 9) |\
              (\d << 5) | (\c << 3) | (\b << 2) | 2
        .endm

