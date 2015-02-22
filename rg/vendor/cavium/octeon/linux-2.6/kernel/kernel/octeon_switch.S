/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995, 1996, 1998, 1999, 2002, 2003 Ralf Baechle
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 * Copyright (C) 1994, 1995, 1996, by Andreas Busse
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) 2000 MIPS Technologies, Inc.
 *    written by Carsten Langgaard, carstenl@mips.com
 */
#include <linux/config.h>
#include <asm/asm.h>
#include <asm/cachectl.h>
#include <asm/fpregdef.h>
#include <asm/mipsregs.h>
#include <asm/asm-offsets.h>
#include <asm/page.h>
#include <asm/pgtable-bits.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/thread_info.h>

#include <asm/asmmacro.h>

/*
 * Offset to the current process status flags, the first 32 bytes of the
 * stack are not used.
 */
#define ST_OFF (_THREAD_SIZE - 32 - PT_SIZE + PT_STATUS)

/*
 * task_struct *resume(task_struct *prev, task_struct *next,
 *                     struct thread_info *next_ti)
 */
	.align	7
	LEAF(resume)
#ifndef CONFIG_CPU_HAS_LLSC
	sw	zero, ll_bit
#endif
	mfc0	t1, CP0_STATUS
	LONG_S	t1, THREAD_STATUS(a0)
	cpu_save_nonscratch a0
	LONG_S	ra, THREAD_REG31(a0)

	/* check if we need to save COP2 registers */
	mfc0	t0, CP0_STATUS
	bbit0	t0, 30, 1f
	
	/* Store the COP2 state. I don't need to save ra since it is overwritten
	    later in cpu_restore_nonscratch */
	jal octeon_cop2_save

	/* clear saved user stack CU2 bit and disable COP2 */
	mfc0	t0, CP0_STATUS
	li	t1, ~ST0_CU2
	and	t0, t0, t1
	mtc0	t0, CP0_STATUS
	LONG_S	t0, THREAD_STATUS(a0)
	PTR_L	t2, TASK_THREAD_INFO(a0)
	LONG_L	t0, ST_OFF(t2)
	and	t0, t0, t1
	LONG_S	t0, ST_OFF(t2)

1:
	/* Check if we need to store CVMSEG state */
	mfc0	t0, $11,7 	/* CvmMemCtl */
	bbit0	t0, 6, 3f	/* Is user access enabled? */
	
	/* Store the CVMSEG state */
	andi	t0, 0x3f	/* Extract the size of CVMSEG */
	sll	t0, 3		/* Multiply * 8 (cache line size/16) */
	dli	t1, 0xFFFFFFFFFFFF8000 	/* Base address of CVMSEG */
	daddi	t2, a0, THREAD_CVMSEG	/* Where to store CVMSEG to */
	synciobdma
2:	
	LONG_L	t8, 0(t1)	/* Load from CVMSEG */
	subu	t0, 1		/* Decrement loop var */
	LONG_L	t9, 8(t1)	/* Load from CVMSEG */
	daddu	t1, 16		/* Increment loc in CVMSEG */
	LONG_S	t8, 0(t2)	/* Store CVMSEG to thread storage */
	LONG_S	t9, 8(t2)	/* Store CVMSEG to thread storage */
	daddu	t2, 16		/* Increment loc in thread storage */
	bnez	t0, 2b		/* Loop until we've copied it all */
	
	/* Disable access to CVMSEG */
	mfc0	t0, $11,7 	/* CvmMemCtl */
	xori	t0, t0, 0x40	/* Bit 6 is CVMSEG user enable */
	mtc0	t0, $11,7 	/* CvmMemCtl */

3:
	/*
	 * The order of restoring the registers takes care of the race
	 * updating $28, $29 and kernelsp without disabling ints.
	 */
	move	$28, a2
	cpu_restore_nonscratch a1

	PTR_ADDIU	t0, $28, _THREAD_SIZE - 32
	set_saved_sp	t0, t1, t2

#ifdef CONFIG_FAST_ACCESS_TO_THREAD_POINTER
	/* We need to put the thread pointer in CVMMEM immediately. The 
		kernel will use this value during TLB exceptions even
		though userspace hasn't accessed CVMMEM */
	LONG_L	t1, TI_TP_VALUE($28)
	LONG_S	t1, FAST_ACCESS_THREAD_OFFSET($0)
#endif				

	mfc0	t1, CP0_STATUS		/* Do we really need this? */
	li	a3, 0xff01
	and	t1, a3
	LONG_L	a2, THREAD_STATUS(a1)
	nor	a3, $0, a3
	and	a2, a3
	or	a2, t1
	mtc0	a2, CP0_STATUS
	move	v0, a0
	jr	ra
	END(resume)

/*
 * void octeon_cop2_save(struct task_struct *current)
 */
	.align	7
	LEAF(octeon_cop2_save)

	dmfc0	t9, $9,7	/* CvmCtl register. */

        /* Save the COP2 CRC state */
	dmfc2	t0, 0x0201
	dmfc2	t1, 0x0202
	dmfc2	t2, 0x0200
	LONG_S	t0, THREAD_CP2_CRC_IV(a0)
	LONG_S	t1, THREAD_CP2_CRC_LENGTH(a0)
	LONG_S	t2, THREAD_CP2_CRC_POLY(a0)
	
	bbit1	t9, 28, 1f	/* Skip next instructions if CvmCtl[NODFA_CP2] set */

	/* Save the LLM state */
	dmfc2	t0, 0x0402
	dmfc2	t1, 0x040A
	LONG_S	t0, THREAD_CP2_LLM_DAT(a0)
	LONG_S	t1, THREAD_CP2_LLM_DAT+8(a0)
	
1:      bbit1	t9, 26, 3f	/* done if CvmCtl[NOCRYPTO] set */

	/* Save the COP2 crypto state */
        /* this part is mostly common to both pass 1 and later revisions */
	dmfc2 	t0, 0x0084		
	dmfc2 	t1, 0x0080
	dmfc2 	t2, 0x0081
	dmfc2 	t3, 0x0082
	LONG_S	t0, THREAD_CP2_3DES_IV(a0)
	dmfc2 	t0, 0x0088
	LONG_S	t1, THREAD_CP2_3DES_KEY(a0)
	dmfc2 	t1, 0x0111                      /* only necessary for pass 1 */
	LONG_S	t2, THREAD_CP2_3DES_KEY+8(a0)
	dmfc2 	t2, 0x0102
	LONG_S	t3, THREAD_CP2_3DES_KEY+16(a0)
	dmfc2 	t3, 0x0103
	LONG_S	t0, THREAD_CP2_3DES_RESULT(a0)
	dmfc2 	t0, 0x0104
	LONG_S	t1, THREAD_CP2_AES_INP0(a0)     /* only necessary for pass 1 */
	dmfc2 	t1, 0x0105
	LONG_S	t2, THREAD_CP2_AES_IV(a0)
	dmfc2	t2, 0x0106
	LONG_S	t3, THREAD_CP2_AES_IV+8(a0)
	dmfc2 	t3, 0x0107
	LONG_S	t0, THREAD_CP2_AES_KEY(a0)
	dmfc2	t0, 0x0110
	LONG_S	t1, THREAD_CP2_AES_KEY+8(a0)
	dmfc2	t1, 0x0100
	LONG_S	t2, THREAD_CP2_AES_KEY+16(a0)
	dmfc2	t2, 0x0101
	LONG_S	t3, THREAD_CP2_AES_KEY+24(a0)
	mfc0	t3, $15,0 	/* Get the processor ID register */
	LONG_S	t0, THREAD_CP2_AES_KEYLEN(a0)
	li	t0, 0x000d0000	/* This is the processor ID of Octeon Pass1 */
	LONG_S	t1, THREAD_CP2_AES_RESULT(a0)
	LONG_S	t2, THREAD_CP2_AES_RESULT+8(a0)

	beq	t3, t0, 2f	/* Skip to the Pass1 version of the remainder of the COP2 state */
	
        /* the non-pass1 state when !CvmCtl[NOCRYPTO] */
	dmfc2	t1, 0x0240
	dmfc2	t2, 0x0241
	dmfc2	t3, 0x0242
	dmfc2	t0, 0x0243
	LONG_S	t1, THREAD_CP2_HSH_DATW(a0)
	dmfc2	t1, 0x0244
	LONG_S	t2, THREAD_CP2_HSH_DATW+8(a0)
	dmfc2	t2, 0x0245
	LONG_S	t3, THREAD_CP2_HSH_DATW+16(a0)
	dmfc2	t3, 0x0246
	LONG_S	t0, THREAD_CP2_HSH_DATW+24(a0)
	dmfc2	t0, 0x0247
	LONG_S	t1, THREAD_CP2_HSH_DATW+32(a0)
	dmfc2	t1, 0x0248
	LONG_S	t2, THREAD_CP2_HSH_DATW+40(a0)
	dmfc2	t2, 0x0249
	LONG_S	t3, THREAD_CP2_HSH_DATW+48(a0)
	dmfc2	t3, 0x024A
	LONG_S	t0, THREAD_CP2_HSH_DATW+56(a0)
	dmfc2	t0, 0x024B
	LONG_S	t1, THREAD_CP2_HSH_DATW+64(a0)
	dmfc2	t1, 0x024C
	LONG_S	t2, THREAD_CP2_HSH_DATW+72(a0)
	dmfc2	t2, 0x024D
	LONG_S	t3, THREAD_CP2_HSH_DATW+80(a0)
	dmfc2 	t3, 0x024E
	LONG_S	t0, THREAD_CP2_HSH_DATW+88(a0)
	dmfc2	t0, 0x0250
	LONG_S	t1, THREAD_CP2_HSH_DATW+96(a0)
	dmfc2	t1, 0x0251
	LONG_S	t2, THREAD_CP2_HSH_DATW+104(a0)
	dmfc2	t2, 0x0252
	LONG_S	t3, THREAD_CP2_HSH_DATW+112(a0)
	dmfc2	t3, 0x0253
	LONG_S	t0, THREAD_CP2_HSH_IVW(a0)
	dmfc2	t0, 0x0254
	LONG_S	t1, THREAD_CP2_HSH_IVW+8(a0)
	dmfc2	t1, 0x0255
	LONG_S	t2, THREAD_CP2_HSH_IVW+16(a0)
	dmfc2	t2, 0x0256
	LONG_S	t3, THREAD_CP2_HSH_IVW+24(a0)
	dmfc2	t3, 0x0257
	LONG_S	t0, THREAD_CP2_HSH_IVW+32(a0)
	dmfc2 	t0, 0x0258
	LONG_S	t1, THREAD_CP2_HSH_IVW+40(a0)
	dmfc2 	t1, 0x0259
	LONG_S	t2, THREAD_CP2_HSH_IVW+48(a0)
	dmfc2	t2, 0x025E
	LONG_S	t3, THREAD_CP2_HSH_IVW+56(a0)
	dmfc2	t3, 0x025A
	LONG_S	t0, THREAD_CP2_GFM_MULT(a0)
	dmfc2	t0, 0x025B
	LONG_S	t1, THREAD_CP2_GFM_MULT+8(a0)
	LONG_S	t2, THREAD_CP2_GFM_POLY(a0)
	LONG_S	t3, THREAD_CP2_GFM_RESULT(a0)
	LONG_S	t0, THREAD_CP2_GFM_RESULT+8(a0)
	jr	ra
	
2:      /* pass 1 special stuff when !CvmCtl[NOCRYPTO] */
	dmfc2	t3, 0x0040
	dmfc2	t0, 0x0041
	dmfc2	t1, 0x0042
	dmfc2	t2, 0x0043
	LONG_S	t3, THREAD_CP2_HSH_DATW(a0)
	dmfc2	t3, 0x0044
	LONG_S	t0, THREAD_CP2_HSH_DATW+8(a0)
	dmfc2	t0, 0x0045
	LONG_S	t1, THREAD_CP2_HSH_DATW+16(a0)
	dmfc2	t1, 0x0046
	LONG_S	t2, THREAD_CP2_HSH_DATW+24(a0)
	dmfc2	t2, 0x0048
	LONG_S	t3, THREAD_CP2_HSH_DATW+32(a0)
	dmfc2	t3, 0x0049
	LONG_S	t0, THREAD_CP2_HSH_DATW+40(a0)
	dmfc2	t0, 0x004A
	LONG_S	t1, THREAD_CP2_HSH_DATW+48(a0)
	LONG_S	t2, THREAD_CP2_HSH_IVW(a0)
	LONG_S	t3, THREAD_CP2_HSH_IVW+8(a0)
	LONG_S	t0, THREAD_CP2_HSH_IVW+16(a0)
	
3:      /* pass 1 or CvmCtl[NOCRYPTO] set */
	jr	ra
	END(octeon_cop2_save)
	
/*
 * void octeon_cop2_restore(struct task_struct *current)
 */
	.align	7
	.set push
	.set noreorder
	LEAF(octeon_cop2_restore)
        /* First cache line was prefetched before the call */
        pref    4,  THREAD_CP2_CRC_IV+128(a0)
	dmfc0	t9, $9,7	/* CvmCtl register. */
	
	/* Enable COP2 accesses so we can restore the state */
	mfc0	t2, CP0_STATUS
        pref    4,  THREAD_CP2_CRC_IV+256(a0)
	LONG_L	t0, THREAD_CP2_CRC_IV(a0)
        pref    4,  THREAD_CP2_CRC_IV+384(a0)
	li	t3, ST0_CU2
	LONG_L	t1, THREAD_CP2_CRC_LENGTH(a0)
	or	t3, t2, t3
	LONG_L	t2, THREAD_CP2_CRC_POLY(a0)
	mtc0	t3, CP0_STATUS
	
	/* Restore the COP2 CRC state */		
	dmtc2	t0, 0x0201
	dmtc2 	t1, 0x1202
	bbit1	t9, 28, 2f	/* Skip LLM if CvmCtl[NODFA_CP2] is set */
	 dmtc2	t2, 0x4200

	/* Restore the LLM state */
	LONG_L	t0, THREAD_CP2_LLM_DAT(a0)
	LONG_L	t1, THREAD_CP2_LLM_DAT+8(a0)
	dmtc2	t0, 0x0402
	dmtc2	t1, 0x040A
	
2:
	bbit1	t9, 26, done_restore	/* done if CvmCtl[NOCRYPTO] set */
	 nop

	/* Restore the COP2 crypto state common to pass 1 and pass 2 */		
	LONG_L	t0, THREAD_CP2_3DES_IV(a0)
	LONG_L	t1, THREAD_CP2_3DES_KEY(a0)
	LONG_L	t2, THREAD_CP2_3DES_KEY+8(a0)
	dmtc2 	t0, 0x0084
	LONG_L	t0, THREAD_CP2_3DES_KEY+16(a0)
	dmtc2 	t1, 0x0080
	LONG_L	t1, THREAD_CP2_3DES_RESULT(a0)
	dmtc2 	t2, 0x0081
	LONG_L	t2, THREAD_CP2_AES_INP0(a0)       /* only really needed for pass 1 */
	dmtc2	t0, 0x0082
	LONG_L	t0, THREAD_CP2_AES_IV(a0)
	dmtc2 	t1, 0x0098
	LONG_L	t1, THREAD_CP2_AES_IV+8(a0)
	dmtc2 	t2, 0x010A                        /* only really needed for pass 1 */
	LONG_L	t2, THREAD_CP2_AES_KEY(a0)
	dmtc2 	t0, 0x0102
	LONG_L	t0, THREAD_CP2_AES_KEY+8(a0)
	dmtc2	t1, 0x0103
	LONG_L	t1, THREAD_CP2_AES_KEY+16(a0)
	dmtc2	t2, 0x0104
	LONG_L	t2, THREAD_CP2_AES_KEY+24(a0)
	dmtc2	t0, 0x0105
	LONG_L	t0, THREAD_CP2_AES_KEYLEN(a0)
	dmtc2	t1, 0x0106
	LONG_L	t1, THREAD_CP2_AES_RESULT(a0)
	dmtc2	t2, 0x0107
	LONG_L	t2, THREAD_CP2_AES_RESULT+8(a0)
	mfc0	t3, $15,0 	/* Get the processor ID register */
	dmtc2	t0, 0x0110
	li	t0, 0x000d0000	/* This is the processor ID of Octeon Pass1 */
	dmtc2	t1, 0x0100
	bne	t0, t3, 3f	/* Skip the next stuff for non-pass1 */
	 dmtc2	t2, 0x0101

        /* this code is specific for pass 1 */
	LONG_L	t0, THREAD_CP2_HSH_DATW(a0)
	LONG_L	t1, THREAD_CP2_HSH_DATW+8(a0)
	LONG_L	t2, THREAD_CP2_HSH_DATW+16(a0)
	dmtc2	t0, 0x0040
	LONG_L	t0, THREAD_CP2_HSH_DATW+24(a0)
	dmtc2	t1, 0x0041
	LONG_L	t1, THREAD_CP2_HSH_DATW+32(a0)
	dmtc2	t2, 0x0042
	LONG_L	t2, THREAD_CP2_HSH_DATW+40(a0)
	dmtc2	t0, 0x0043
	LONG_L	t0, THREAD_CP2_HSH_DATW+48(a0)
	dmtc2	t1, 0x0044
	LONG_L	t1, THREAD_CP2_HSH_IVW(a0)
	dmtc2	t2, 0x0045
	LONG_L	t2, THREAD_CP2_HSH_IVW+8(a0)
	dmtc2	t0, 0x0046
	LONG_L	t0, THREAD_CP2_HSH_IVW+16(a0)
	dmtc2	t1, 0x0048
	dmtc2	t2, 0x0049
        b done_restore   /* unconditional branch */
	 dmtc2	t0, 0x004A
	
3:      /* this is post-pass1 code */
	LONG_L	t2, THREAD_CP2_HSH_DATW(a0)
	LONG_L	t0, THREAD_CP2_HSH_DATW+8(a0)
	LONG_L	t1, THREAD_CP2_HSH_DATW+16(a0)
	dmtc2	t2, 0x0240
	LONG_L	t2, THREAD_CP2_HSH_DATW+24(a0)
	dmtc2	t0, 0x0241
	LONG_L	t0, THREAD_CP2_HSH_DATW+32(a0)
	dmtc2	t1, 0x0242
	LONG_L	t1, THREAD_CP2_HSH_DATW+40(a0)
	dmtc2	t2, 0x0243
	LONG_L	t2, THREAD_CP2_HSH_DATW+48(a0)
	dmtc2	t0, 0x0244
	LONG_L	t0, THREAD_CP2_HSH_DATW+56(a0)
	dmtc2	t1, 0x0245
	LONG_L	t1, THREAD_CP2_HSH_DATW+64(a0)
	dmtc2	t2, 0x0246
	LONG_L	t2, THREAD_CP2_HSH_DATW+72(a0)
	dmtc2	t0, 0x0247
	LONG_L	t0, THREAD_CP2_HSH_DATW+80(a0)
	dmtc2	t1, 0x0248
	LONG_L	t1, THREAD_CP2_HSH_DATW+88(a0)
	dmtc2	t2, 0x0249
	LONG_L	t2, THREAD_CP2_HSH_DATW+96(a0)
	dmtc2	t0, 0x024A
	LONG_L	t0, THREAD_CP2_HSH_DATW+104(a0)
	dmtc2	t1, 0x024B
	LONG_L	t1, THREAD_CP2_HSH_DATW+112(a0)
	dmtc2	t2, 0x024C
	LONG_L	t2, THREAD_CP2_HSH_IVW(a0)
	dmtc2	t0, 0x024D
	LONG_L	t0, THREAD_CP2_HSH_IVW+8(a0)
	dmtc2	t1, 0x024E
	LONG_L	t1, THREAD_CP2_HSH_IVW+16(a0)
	dmtc2	t2, 0x0250
	LONG_L	t2, THREAD_CP2_HSH_IVW+24(a0)
	dmtc2	t0, 0x0251
	LONG_L	t0, THREAD_CP2_HSH_IVW+32(a0)
	dmtc2	t1, 0x0252
	LONG_L	t1, THREAD_CP2_HSH_IVW+40(a0)
	dmtc2	t2, 0x0253
	LONG_L	t2, THREAD_CP2_HSH_IVW+48(a0)
	dmtc2	t0, 0x0254
	LONG_L	t0, THREAD_CP2_HSH_IVW+56(a0)
	dmtc2	t1, 0x0255
	LONG_L	t1, THREAD_CP2_GFM_MULT(a0)
	dmtc2	t2, 0x0256
	LONG_L	t2, THREAD_CP2_GFM_MULT+8(a0)
	dmtc2	t0, 0x0257
	LONG_L	t0, THREAD_CP2_GFM_POLY(a0)
	dmtc2	t1, 0x0258
	LONG_L	t1, THREAD_CP2_GFM_RESULT(a0)
	dmtc2	t2, 0x0259
	LONG_L	t2, THREAD_CP2_GFM_RESULT+8(a0)
	dmtc2	t0, 0x025E
	dmtc2	t1, 0x025A
	dmtc2	t2, 0x025B
	
done_restore:
	/* Disable COP2 accesses so nobody can use it. It will be enabled as
	   soon as the task is scheduled. ST0_CU2 was enabled in the saved
	   status register than will be resotred later */
	mfc0	t0, CP0_STATUS
	li	t1, ~ST0_CU2
	and	t0, t0, t1
	jr	ra
	 mtc0	t0, CP0_STATUS
	END(octeon_cop2_restore)
	.set pop
	
/*
 * void octeon_mult_save()
 * sp is assumed to point to a struct pt_regs
 *
 * NOTE: This is called in SAVE_SOME in stackframe.h. It can only
 *       safely modify k0 and k1.
 */
	.align	7
	.set push
	.set noreorder
	LEAF(octeon_mult_save)
	dmfc0	k0, $9,7	/* CvmCtl register. */
	bbit1	k0, 27, 1f	/* Skip CvmCtl[NOMUL] */
	 nop
	
	/* Save the multiplier state */
	v3mulu	k0, $0, $0
	v3mulu	k1, $0, $0
	sd	k0, PT_MTP(sp)        /* PT_MTP    has P0 */
	v3mulu	k0, $0, $0
	sd	k1, PT_MTP+8(sp)      /* PT_MTP+8  has P1 */
	ori	k1, $0, 1
	v3mulu	k1, k1, $0
	sd	k0, PT_MTP+16(sp)     /* PT_MTP+16 has P2 */
	v3mulu	k0, $0, $0
	sd	k1, PT_MPL(sp)        /* PT_MPL    has MPL0 */
	v3mulu	k1, $0, $0
	sd	k0, PT_MPL+8(sp)      /* PT_MPL+8  has MPL1 */
	jr	ra
	 sd	k1, PT_MPL+16(sp)     /* PT_MPL+16 has MPL2 */
	
1:	/* Resume here if CvmCtl[NOMUL] */	
	jr	ra
	END(octeon_mult_save)
	.set pop
	
/*
 * void octeon_mult_restore()
 * sp is assumed to point to a struct pt_regs
 *
 * NOTE: This is called in RESTORE_SOME in stackframe.h.
 */
	.align	7
	.set push
	.set noreorder
	LEAF(octeon_mult_restore)
	dmfc0	k1, $9,7		/* CvmCtl register. */
	ld	v0, PT_MPL(sp)        	/* MPL0 */
	ld	v1, PT_MPL+8(sp)      	/* MPL1 */
	ld	k0, PT_MPL+16(sp)     	/* MPL2 */
	bbit1	k1, 27, 1f		/* Skip CvmCtl[NOMUL] */
	 nop				/* Normally falls through, so no time wasted here */
	
	/* Restore the multiplier state */
	ld	k1, PT_MTP+16(sp)     	/* P2 */
	MTM0	v0			/* MPL0 */
	ld	v0, PT_MTP+8(sp)	/* P1 */
	MTM1	v1			/* MPL1 */
	ld	v1, PT_MTP(sp)   	/* P0 */
	MTM2	k0			/* MPL2 */
	MTP2	k1			/* P2 */
	MTP1	v0			/* P1 */
	jr	ra
	 MTP0	v1			/* P0 */
	
1:	/* Resume here if CvmCtl[NOMUL] */	
	jr	ra
	 nop
	END(octeon_mult_restore)
	.set pop

