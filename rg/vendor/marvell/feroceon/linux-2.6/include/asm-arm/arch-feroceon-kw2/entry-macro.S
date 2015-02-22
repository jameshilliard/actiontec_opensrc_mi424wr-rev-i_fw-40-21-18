/*
 * include/asm-arm/arch-feroceon-kw/entry-macro.S
 *
 * Low-level IRQ helper macros for Integrator platforms
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <asm/hardware.h>
#include <asm/arch/irqs.h>


        .macro  disable_fiq
        .endm
        
        .macro  get_irqnr_preamble, base, tmp
        .endm

         .macro  arch_ret_to_user, tmp1, tmp2
         .endm

#if defined(CONFIG_CPU_BIG_ENDIAN)
        	.macro	HTOLL sr, tt		@ sr   = A  ,B  ,C  ,D    
        	eor \tt, \sr, \sr, ror #16 ;    @ temp = A^C,B^	  
        	bic \tt, \tt, #0xff0000 ;       @ temp = A^C,0  ,C^A,D^B
        	mov \sr, \sr, ror #8 ;          @ sr   = D  ,A  ,B  ,C
        	eor \sr, \sr, \tt, lsr #8       @ sr   = D  ,C  ,B  ,A
        	.endm
#else		
        	.macro	HTOLL sr, tt	
        	.endm
#endif

                /*
                 * This macro should return the follows:
                 *      irqnr - the number of the received irq
                 *      eq flag - not set if succeed and set if fail.
                 */

                .macro  get_irqnr_and_base, irqnr, irqstat, base, tmp
                ldr     \base, =MV_ASM_IRQ_MASK_LOW_REG
                ldr     \tmp, [\base]    			        @ get low mask
                ldr	\base, =MV_ASM_IRQ_CAUSE_LOW_REG
                ldr     \irqstat, [\base]                		@ get low cause
                ands    \irqstat, \irqstat, \tmp                        @ and mask and cause
		beq     1001f
		HTOLL	\irqstat, \tmp
		clz     \irqnr, \irqstat
		mov     \base, #31
		subs    \irqnr, \base, \irqnr
                beq     1001f                                           @ bit0 is set - jump to 1001
		teq     \irqnr, #32                                     @ if irq isn't 32 unset the flag
		b 	1006f
1001:
		ldr     \base, =MV_ASM_IRQ_MASK_HIGH_REG
                ldr     \tmp, [\base]    			        @ get mask
                ldr	\base, =MV_ASM_IRQ_CAUSE_HIGH_REG
                ldr     \irqstat, [\base]                		@ get cause
                ands    \irqstat, \irqstat, \tmp                        @ and mask and cause
		beq	1002f						@ no bit is set - jump to 1002
		HTOLL	\irqstat, \tmp
		clz     \irqnr, \irqstat
		mov     \base, #31
		subs    \irqnr, \base, \irqnr
		cmp	\irqnr, #4					@ GPP LOW 0-7
		beq	1003f
		cmp	\irqnr, #5					@ GPP LOW 8-15
		beq	1003f
		cmp	\irqnr, #6					@ GPP LOW 16-23
		beq	1003f
		cmp	\irqnr, #7					@ GPP LOW 24-31
		beq	1003f
		cmp	\irqnr, #8					@ GPP MID 0-7
		beq	1004f
		cmp	\irqnr, #9					@ GPP MID 8-15
		beq	1004f
		cmp	\irqnr, #10					@ GPP MID 16-23
		beq	1004f
		cmp	\irqnr, #11					@ GPP MID 24-31
		beq	1004f
		cmp	\irqnr, #12					@ GPP HIGH 0-7
		beq	1005f
		cmp	\irqnr, #13					@ GPP HIGH 8-15
		beq	1005f
		cmp	\irqnr, #14					@ GPP HIGH 16-23
		beq	1005f
		cmp	\irqnr, #15					@ GPP HIGH 24
		beq	1005f
		add	\irqnr,\irqnr,#32				@ add 32 so (32 <= irqnr < 64)
		teq     \irqnr, #64                                     @ if irq isn't 64 unset the flag
		b	1006f						@ update irqnr and return
1002:
		ldr     \base, =MV_ASM_IRQ_MASK_ERROR_REG
                ldr     \tmp, [\base]    				@ get error mask
                ldr	\base, =MV_ASM_IRQ_CAUSE_ERROR_REG
                ldr     \irqstat, [\base]                		@ get error cause
                ands    \irqstat, \irqstat, \tmp                        @ and mask and cause
		beq	1006f						@ if eq to 0 jump to 1006
		HTOLL	\irqstat, \tmp
		clz     \irqnr, \irqstat
		mov     \base, #31
		subs    \irqnr, \base, \irqnr
		add	\irqnr,\irqnr, #64
		teq     \irqnr, #96                                     @ if irq isn't 96 unset the flag
		b	1006f						@ update irqnr and return
1003:
		ldr     \base, =MV_ASM_GPP_IRQ_MASK_REG
                ldr     \tmp, [\base]    				@ get gpp mask
                ldr	\base, =MV_ASM_GPP_IRQ_CAUSE_REG
                ldr     \irqstat, [\base]                		@ get gpp data in (cause)
                ands    \irqstat, \irqstat, \tmp                        @ and mask and cause
		beq	1006f						@ if eq to 0 jump to 1006
		HTOLL	\irqstat, \tmp
		clz     \irqnr, \irqstat
		mov     \base, #31
		subs    \irqnr, \base, \irqnr
		add	\irqnr,\irqnr, #96
		teq     \irqnr, #128                                    @ if irq isn't 128 unset the flag
		b	1006f						@ update irqnr and return 
1004:
		ldr     \base, =MV_ASM_GPP_IRQ_MID_MASK_REG
                ldr     \tmp, [\base]    				@ get gpp mask
                ldr	\base, =MV_ASM_GPP_IRQ_MID_CAUSE_REG
                ldr     \irqstat, [\base]                		@ get gpp data in (cause)
                ands    \irqstat, \irqstat, \tmp                        @ and mask and cause
		beq	1006f						@ if eq to 0 jump to 1006
		HTOLL	\irqstat, \tmp
		clz     \irqnr, \irqstat
		mov     \base, #31
		subs    \irqnr, \base, \irqnr
		add	\irqnr,\irqnr, #128
		teq     \irqnr, #160					@ if irq isn't 160 unset the flag
		b	1006f						@ update irqnr and return	
1005:
		ldr     \base, =MV_ASM_GPP_IRQ_HIGH_MASK_REG
                ldr     \tmp, [\base]    				@ get gpp mask
                ldr	\base, =MV_ASM_GPP_IRQ_HIGH_CAUSE_REG
                ldr     \irqstat, [\base]                		@ get gpp data in (cause)
                ands    \irqstat, \irqstat, \tmp                        @ and mask and cause
		beq	1006f						@ if eq to 0 jump to 1006
		HTOLL	\irqstat, \tmp
		clz     \irqnr, \irqstat
		mov     \base, #31
		subs    \irqnr, \base, \irqnr
		add	\irqnr,\irqnr, #160
		teq     \irqnr, #192					@ if irq isn't 192 unset the flag
		b	1006f						@ update irqnr and return	
1006:
                .endm
