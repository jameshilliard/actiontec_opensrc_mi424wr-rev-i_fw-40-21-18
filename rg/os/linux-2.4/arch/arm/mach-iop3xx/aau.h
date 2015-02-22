/*
 * Private Definitions for IOP3XX AAU 
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2003 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _AAU_PRIVATE_H_
#define _AAU_PRIVATE_H_

#define AAU_IRQ_THRESH		10

#define	MAX_AAU_DESC		64
#define AAU_DESC_SIZE		32

/*
 * AAU Control Register
 */
#define AAU_ACR_CLEAR		0x00000000
#define AAU_ACR_AAU_ENABLE	0x00000001
#define AAU_ACR_CHAIN_RESUME	0x00000002
#define AAU_ACR_512B_EN		0x00000004

/*
 * AAU Status Register
 */
#define AAU_ASR_INT_MST_ABORT	0x00000020
#define AAU_ASR_EOC_INT		0x00000100
#define AAU_ASR_EOT_INT		0x00000200
#define AAU_ASR_ACTIVE		0x00000400
#define AAU_ASR_DONE_MASK	(AAU_ASR_EOC_INT | AAU_ASR_EOT_INT)
#define AAU_ASR_ERR_MASK	(AAU_ASR_INT_MST_ABORT)

/*
 * AAU Descriptor Control Register
 */
#define AAU_ADCR_IE		0x00000001	/* Interrupt Enable */
#define AAU_ADCR_DIRFILL	0x0000000E	/* memory to memory transfer */
#define AAU_ADCR_MEMFILL	0x00000004	/* memory fill */
#define AAU_ADCR_WREN		0x80000000	/* dest write enable */

#define	AAU_ADCR_XOR_2		0x0000001e
#define AAU_ADCR_XOR_3		0x0000009e
#define	AAU_ADCR_XOR_4		0x0000049e
#define	AAU_ADCR_XOR_5		0x0000249e

#define AAU_FREE         0x0
#define AAU_ACTIVE       0x1
#define AAU_COMPLETE     0x2
#define AAU_ERROR	 0x4


/*
 * AAU Descriptor
 *
 * This descriptor can be extended to support up 32 buffers at
 * once, but we're not supporting that atm as the Linux RAID
 * stack only support 5 blocks at once. :(
 */
typedef struct _aau_desc {
	u32 NDAR;		/* next descriptor adress */
	u32 SAR;		/* source addr 1 or data */
	u32 SAR2;		/* source addr 2 */
	u32 SAR3;		/* source addr 3 */
	u32 SAR4;		/* source addr 4 */
	u32 DAR;		/* dest addr */
	u32 BC;			/* byte count */
	u32 DC;			/* descriptor control */
	u32 SAR5;
	u32 SAR6;
	u32 SAR7;
	u32 SAR8;
} aau_desc_t __attribute__((__aligned__(L1_CACHE_BYTES * 2)));

/*
 * AAU Software Descriptor
 */
typedef struct _sw_aau sw_aau_t;

struct _sw_aau {
	struct list_head link;
	aau_desc_t *aau_desc;	/* AAU descriptor pointer */
	void *aau_virt;		/* unaligned virtual aau addr */
	u32 aau_phys;		/* aligned aau physical */
	u32 status;
	void *virt_src;		/* virt source */
	void *virt_dest;	/* virt destination */
	u32 phys_src;		/* physical/bus address of src */
	u32 phys_dest;		/* Physical/bus address of dest */
	u32 buf_size;
	sw_aau_t *next;
	wait_queue_head_t wait;
};

/*
 * AAU control register structure
 */
typedef struct _aau_regs {
	volatile u32 *ACR;	/* channel control register */
	volatile u32 *ASR;	/* channel status register */
	volatile u32 *ADAR;	/* descriptor address register */
	volatile u32 *ANDAR;	/* next descriptor address register */
	volatile u32 *SAR;	/* SAR 1 */
	/* SAR2...SAR32 */	/* not supported yet */
	volatile u32 *DAR;	/* local address register */
	volatile u32 *BCR;	/* byte count register */
	volatile u32 *DCR;	/* descriptor control register */
	volatile u32 *EDCR0;
	volatile u32 *EDCR1;
	volatile u32 *EDCR2;
} aau_regs_t __attribute__((__aligned__(L1_CACHE_BYTES)));

/*
 * AAU structure.
 */
typedef struct _aau_ctrl_t {
	spinlock_t aau_lock;		/* lock for accessing AAU registers */
	struct list_head process_q;
	struct list_head free_q;
	spinlock_t process_lock;	/* process queue lock */
	spinlock_t free_lock;		/* hold queue lock */
	struct {
		int EOT;
		int EOC;
		int ERR;
	} irq;
	const char *device_id;	/* Device name */
	aau_regs_t regs;
	sw_aau_t *last_desc;
} iop3xx_aau_t;

#define SW_ENTRY(list) list_entry((list.next), sw_aau_t, link)

#endif

