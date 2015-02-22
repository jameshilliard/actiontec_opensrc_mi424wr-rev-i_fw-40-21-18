/*
 * arch/arm/mach-iop3xxx/aau->c
 *
 * Support functions for the Intel 803xx AAU channels. The AAU is
 * a HW XOR unit that is specifically designed for use with RAID5
 * applications.  This driver provides an interface that is used by
 * the Linux RAID stack.
 *
 * Original Author: Dave Jiang <dave.jiang@intel.com>
 *
 * Contributors: David A. Griego <david.a.griego@intel.com>
 *               Deepak Saxena <dsaxena@mvista.com>
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2003 Intel Corporation
 * Copyright (C) 2003 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * TODO:	Move to 2.5 kernel
 * 		Replace list interface with a cache of objects?
 * 		Move interrupt handling to tasklet?
 * 		Better error handling (ERR IRQs and runtime errors)
 *
 * History:	(02/25/2003, DJ) Initial Creation
 *		(04/23/2003, DS) Cleanups, add support for iop310
 *
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/iobuf.h>
#include <linux/mm.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/uaccess.h>
#include <asm/proc/cache.h>
#include <asm/hardware.h>
#include <asm/arch/xor.h>

/*
 * pick up local definitions
 */
#include "aau.h"

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#undef DEBUG
#ifdef DEBUG
#define DPRINTK(s, args...) printk("IOP3xx AAU: " s "\n", ## args)
#define DENTER() DPRINTK("Entered...\n");
#define DEXIT() DPRINTK("Exited...\n");
#else
#define DPRINTK(s, args...)
#define DENTER()
#define DEXIT()
#endif

/* globals */
iop3xx_aau_t *aau;		/* AAU context */

/* static prototypes */
static int iop3xx_aau_chaininit(iop3xx_aau_t * aa);
static void iop3xx_aau_isr(int, void *, struct pt_regs *);

static inline void aau_queue_descriptor(sw_aau_t *sw_desc)
{
	unsigned long flags;

	// Set last NDAR to NULL
	sw_desc->next = NULL;
	sw_desc->status = AAU_ACTIVE;

	spin_lock_irqsave(&aau->process_lock, flags);
	list_add_tail(&sw_desc->link, &aau->process_q);

	aau->last_desc->aau_desc->NDAR = sw_desc->aau_phys;
	aau->last_desc = sw_desc;
	*(aau->regs.ACR) = AAU_ACR_AAU_ENABLE | AAU_ACR_CHAIN_RESUME;
	spin_unlock_irqrestore(&aau->process_lock, flags);

	DPRINTK("Going to sleep");
	wait_event_interruptible(sw_desc->wait,
				(sw_desc->status & (AAU_COMPLETE | AAU_ERROR)));
	DPRINTK("Woken up!");

	spin_lock_irqsave(&aau->free_lock, flags);
	list_add_tail(&sw_desc->link, &aau->free_q);
	spin_unlock_irqrestore(&aau->free_lock, flags);
}

void
xor_iop3xxaau_2(unsigned long bytes, unsigned long *p1, unsigned long *p2)
{
	sw_aau_t *sw_desc = NULL;
	unsigned long flags;

	DENTER();

	spin_lock_irqsave(&aau->free_lock, flags);
	if (!list_empty(&aau->free_q)) {
		sw_desc = list_entry(aau->free_q.next, sw_aau_t, link);
		if (sw_desc->aau_phys != *aau->regs.ADAR) {
			list_del(&sw_desc->link);
		} else {
			// This should NEVER happen. We need to increase the amount
			// of descriptors if that is the case
			// TODO: put in something nice later to handle this!

			DPRINTK(KERN_ERR "out of AAU descriptors! Increase!\n");
			spin_unlock_irqrestore(&aau->free_lock, flags);
			BUG();
			return;
		}
	} else {
		// This should NEVER happen. We need to increase the amount
		// of descriptors if that is the case
		// TODO: put in something nice later to handle this!

		DPRINTK(KERN_ERR "out of AAU descriptors! Increase!\n");
		spin_unlock_irqrestore(&aau->free_lock, flags);
		BUG();
		return;
	}
	spin_unlock_irqrestore(&aau->free_lock, flags);

	// flush the cache to memory before AAU touches them
	cpu_dcache_clean_range((unsigned long) p1, (unsigned long) p1 + bytes);
	cpu_dcache_clean_range((unsigned long) p2, (unsigned long) p2 + bytes);

	// setting up AAU descriptors
	sw_desc->aau_desc->DAR = virt_to_phys(p1);
	sw_desc->aau_desc->SAR = virt_to_phys(p1);
	sw_desc->aau_desc->SAR2 = virt_to_phys(p2);
	sw_desc->aau_desc->BC = bytes;

	// write enable, direct fill block 1, xor the rest
	sw_desc->aau_desc->DC = AAU_ADCR_WREN | AAU_ADCR_XOR_2 | AAU_ADCR_IE;
	sw_desc->aau_desc->NDAR = 0;

	aau_queue_descriptor(sw_desc);

	if (sw_desc->status & AAU_ERROR) {
		printk(KERN_ERR "%s: IOP321 AAU operation failed!\n", __func__);
		BUG();
	}

	// invalidate the cache region to destination
	cpu_dcache_invalidate_range((unsigned long) p1,
				    (unsigned long) p1 + bytes);
	DEXIT();
}

void
xor_iop3xxaau_3(unsigned long bytes, unsigned long *p1, unsigned long *p2,
		unsigned long *p3)
{
	sw_aau_t *sw_desc = NULL;
	unsigned long flags;

	spin_lock_irqsave(&aau->free_lock, flags);
	if (!list_empty(&aau->free_q)) {
		sw_desc = list_entry(aau->free_q.next, sw_aau_t, link);
		if (sw_desc->aau_phys != *aau->regs.ADAR) {
			list_del(&sw_desc->link);
		} else {
			// This should NEVER happen. We need to increase the amount
			// of descriptors if that is the case
			// TODO: put in something nice later to handle this!
			DPRINTK(KERN_ERR "out of AAU descriptors! Increase!\n");
			spin_unlock_irqrestore(&aau->free_lock, flags);
			BUG();
			return;
		}
	} else {
		// This should NEVER happen. We need to increase the amount
		// of descriptors if that is the case
		// TODO: put in something nice later to handle this!
		DPRINTK(KERN_ERR "out of AAU descriptors! Increase!\n");
		spin_unlock_irqrestore(&aau->free_lock, flags);
		BUG();
		return;
	}
	spin_unlock_irqrestore(&aau->free_lock, flags);

	// flush the cache to memory before AAU touches them
	cpu_dcache_clean_range((unsigned long) p1, (unsigned long) p1 + bytes);
	cpu_dcache_clean_range((unsigned long) p2, (unsigned long) p2 + bytes);
	cpu_dcache_clean_range((unsigned long) p3, (unsigned long) p3 + bytes);

	// setting up AAU descriptors
	sw_desc->aau_desc->DAR = virt_to_phys(p1);
	sw_desc->aau_desc->SAR = virt_to_phys(p1);
	sw_desc->aau_desc->SAR2 = virt_to_phys(p2);
	sw_desc->aau_desc->SAR3 = virt_to_phys(p3);
	sw_desc->aau_desc->BC = bytes;
	
	// write enable, direct fill block 1, xor the rest
	sw_desc->aau_desc->DC = AAU_ADCR_WREN | AAU_ADCR_XOR_3 | AAU_ADCR_IE;
	sw_desc->aau_desc->NDAR = 0;

	aau_queue_descriptor(sw_desc);

	// check to see if AAU failed
	if (sw_desc->status & AAU_ERROR) {
		printk(KERN_ERR "%s: IOP321 AAU operation failed!\n", __func__);
		BUG();
	}
	// invalidate the cache region to destination
	cpu_dcache_invalidate_range((unsigned long) p1,
				    (unsigned long) p1 + bytes);
}

void
xor_iop3xxaau_4(unsigned long bytes, unsigned long *p1, unsigned long *p2,
		unsigned long *p3, unsigned long *p4)
{
	sw_aau_t *sw_desc = NULL;
	unsigned long flags;

	spin_lock_irqsave(&aau->free_lock, flags);
	if (!list_empty(&aau->free_q)) {
		sw_desc = list_entry(aau->free_q.next, sw_aau_t, link);
		if (sw_desc->aau_phys != *aau->regs.ADAR) {
			list_del(&sw_desc->link);
		} else {
			// This should NEVER happen. We need to increase the amount
			// of descriptors if that is the case
			// TODO: put in something nice later to handle this!

			DPRINTK(KERN_ERR "out of AAU descriptors! Increase!\n");
			spin_unlock_irqrestore(&aau->free_lock, flags);
			BUG();
			return;
		}
	} else {
		// This should NEVER happen. We need to increase the amount
		// of descriptors if that is the case
		// TODO: put in something nice later to handle this!

		DPRINTK(KERN_ERR "out of AAU descriptors! Increase!\n");
		spin_unlock_irqrestore(&aau->free_lock, flags);
		BUG();
		return;
	}
	spin_unlock_irqrestore(&aau->free_lock, flags);

	// flush the cache to memory before AAU touches them
	cpu_dcache_clean_range((unsigned long) p1, (unsigned long) p1 + bytes);
	cpu_dcache_clean_range((unsigned long) p2, (unsigned long) p2 + bytes);
	cpu_dcache_clean_range((unsigned long) p3, (unsigned long) p3 + bytes);
	cpu_dcache_clean_range((unsigned long) p4, (unsigned long) p4 + bytes);

	// setting up AAU descriptors
	sw_desc->aau_desc->DAR = virt_to_phys(p1);
	sw_desc->aau_desc->SAR = virt_to_phys(p1);
	sw_desc->aau_desc->SAR2 = virt_to_phys(p2);
	sw_desc->aau_desc->SAR3 = virt_to_phys(p3);
	sw_desc->aau_desc->SAR4 = virt_to_phys(p4);

	sw_desc->aau_desc->BC = bytes;
	// write enable, direct fill block 1, xor the rest
	sw_desc->aau_desc->DC = AAU_ADCR_WREN | AAU_ADCR_XOR_4 | AAU_ADCR_IE;
	sw_desc->aau_desc->NDAR = 0;

	aau_queue_descriptor(sw_desc);

	// check to see if AAU failed
	if (sw_desc->status & AAU_ERROR) {
		printk(KERN_ERR "%s: IOP321 AAU operation failed!\n", __func__);
		BUG();
	}

	// invalidate the cache region to destination
	cpu_dcache_invalidate_range((unsigned long) p1,
				    (unsigned long) p1 + bytes);
}

void
xor_iop3xxaau_5(unsigned long bytes, unsigned long *p1, unsigned long *p2,
		unsigned long *p3, unsigned long *p4, unsigned long *p5)
{
	sw_aau_t *sw_desc = NULL;
	unsigned long flags;

	if (!list_empty(&aau->free_q)) {
		sw_desc = list_entry(aau->free_q.next, sw_aau_t, link);
		list_del(&sw_desc->link);
	} else {
		// This should NEVER happen. We need to increase the amount
		// of descriptors if that is the case
		// TODO: put in something nice later to handle this!

		DPRINTK(KERN_ERR "out of AAU descriptors! Increase!\n");
		spin_unlock_irqrestore(&aau->free_lock, flags);
		BUG();
		return;
	}

	// flush the cache to memory before AAU touches them
	cpu_dcache_clean_range((unsigned long) p1, (unsigned long) p1 + bytes);
	cpu_dcache_clean_range((unsigned long) p2, (unsigned long) p2 + bytes);
	cpu_dcache_clean_range((unsigned long) p3, (unsigned long) p3 + bytes);
	cpu_dcache_clean_range((unsigned long) p4, (unsigned long) p4 + bytes);
	cpu_dcache_clean_range((unsigned long) p5, (unsigned long) p5 + bytes);

	// setting up AAU descriptors
	sw_desc->aau_desc->DAR = virt_to_phys(p1);
	sw_desc->aau_desc->SAR = virt_to_phys(p1);
	sw_desc->aau_desc->SAR2 = virt_to_phys(p2);
	sw_desc->aau_desc->SAR3 = virt_to_phys(p3);
	sw_desc->aau_desc->SAR4 = virt_to_phys(p4);
	sw_desc->aau_desc->SAR5 = virt_to_phys(p5);
	sw_desc->aau_desc->BC = bytes;

	// write enable, direct fill block 1, xor the rest
	sw_desc->aau_desc->DC = AAU_ADCR_WREN | AAU_ADCR_XOR_5 | AAU_ADCR_IE;
	sw_desc->aau_desc->NDAR = 0;

	aau_queue_descriptor(sw_desc);

	// check to see if AAU failed
	if (sw_desc->status & AAU_ERROR) {
		printk(KERN_ERR "%s: IOP321 AAU operation failed!\n", __func__);
		BUG();
	}

	// invalidate the cache region to destination
	cpu_dcache_invalidate_range((unsigned long) p1,
				    (unsigned long) p1 + bytes);
}

static void
iop3xx_aau_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	iop3xx_aau_t *aa = (iop3xx_aau_t *) dev_id;
	u32 csr = 0;
	sw_aau_t *sw_desc = NULL;
	int irq_thresh = AAU_IRQ_THRESH;

	DENTER();

	csr = *(aa->regs.ASR);

	// make sure the interrupt is for us
	if (!(csr & (AAU_ASR_DONE_MASK | AAU_ASR_ERR_MASK))) {
		return;
	}

	// Does not support more than AAU chaining of multiple processes yet */
	while (csr && irq_thresh--) {
		// clear interrupts
		*(aa->regs.ASR) |= AAU_ASR_DONE_MASK | AAU_ASR_ERR_MASK;

		spin_lock(&aa->process_lock);
		while (!list_empty(&aa->process_q)) {
			sw_desc = list_entry(aa->process_q.next, sw_aau_t, link);
			if (sw_desc->aau_phys != *aa->regs.ADAR) {
				list_del(&sw_desc->link);
				if (!(csr & AAU_ASR_ERR_MASK)) {
					sw_desc->status = AAU_COMPLETE;
				} else {
					sw_desc->status = AAU_ERROR;
				}
				wake_up_interruptible(&sw_desc->wait);
			} else {
				if (!(*aa->regs.ASR & AAU_ASR_ACTIVE)) {
					list_del(&sw_desc->link);
					if (!(csr & AAU_ASR_ERR_MASK)) {
						sw_desc->status = AAU_COMPLETE;
					} else {
						sw_desc->status = AAU_ERROR;
					}
					wake_up_interruptible(&sw_desc->wait);
				} else {
					spin_unlock(&aa->process_lock);
					return;
				}
			}
		}
		spin_unlock(&aa->process_lock);

		// re-read CSR to check for more interrupts
		csr = *(aa->regs.ASR);
	}			// end of while(csr)

	DEXIT();
}

int
iop3xx_aau_init(void)
{
	int i;
	sw_aau_t *sw_desc = NULL;
	void *desc = NULL;
	int err = 0;

	aau = kmalloc(sizeof(*aau), GFP_KERNEL);

	printk("Intel IOP3xx AAU RAID Copyright(c) 2003 Intel Corporation\n");

	/* init free stack */
	INIT_LIST_HEAD(&aau->free_q);
	INIT_LIST_HEAD(&aau->process_q);

	/* init free stack spinlock */
	spin_lock_init(&aau->free_lock);
	spin_lock_init(&aau->process_lock);

	aau->last_desc = NULL;

	/* pre-alloc AAU descriptors */
	for (i = 0; i < MAX_AAU_DESC; i++) {
		/* 
		 * we keep track of original address before alignment 
		 * adjust so we can free it later 
		 */
		sw_desc = kmalloc(sizeof (sw_aau_t), GFP_KERNEL);
		sw_desc->aau_virt = desc =
		    kmalloc((sizeof (aau_desc_t) + 0x20), GFP_KERNEL);
		memset(desc, 0, sizeof (aau_desc_t) + 0x20);

		/* 
		 * hardware descriptors must be aligned on an 
		 * 8-word boundary 
		 */
		desc = (aau_desc_t *) (((u32) desc & 0xffffffe0) + 0x20);
		// get the physical address
		sw_desc->aau_phys = (u32) virt_to_phys(desc);
		// remap it to non-cached
		sw_desc->aau_desc =
		    (aau_desc_t *) ioremap(sw_desc->aau_phys,
					   sizeof (aau_desc_t));

		sw_desc->status = AAU_FREE;
		init_waitqueue_head(&sw_desc->wait);

		/* put the descriptors on the free stack */
		list_add(&sw_desc->link, &aau->free_q);
	}

	/*
	 * Fill out AAU descriptor based on IOP type
	 */
#ifdef CONFIG_ARCH_IOP321
	if(iop_is_321()) {
		aau->regs.ACR = IOP321_AAU_ACR;
		aau->regs.ASR = IOP321_AAU_ASR;
		aau->regs.ADAR = IOP321_AAU_ADAR;
		aau->regs.ANDAR = IOP321_AAU_ANDAR;
		aau->regs.SAR = IOP321_AAU_SAR1;
		aau->regs.DAR = IOP321_AAU_DAR;
		aau->regs.BCR = IOP321_AAU_ABCR;
		aau->regs.DCR = IOP321_AAU_ADCR;

		aau->irq.EOT = IRQ_IOP321_AA_EOT;
		aau->irq.EOC = IRQ_IOP321_AA_EOC;
		aau->irq.ERR = IRQ_IOP321_AA_ERR;

		aau->device_id = "IOP321 HW XOR";
	} 
#endif
#ifdef CONFIG_ARCH_IOP310
	if(iop_is_310()) {
		aau->regs.ACR = IOP310_AAU_ACR;
		aau->regs.ASR = IOP310_AAU_ASR;
		aau->regs.ADAR = IOP310_AAU_ADAR;
		aau->regs.ANDAR = IOP310_AAU_ANDAR;
		aau->regs.SAR = IOP310_AAU_SAR1;
		aau->regs.DAR = IOP310_AAU_DAR;
		aau->regs.BCR = IOP310_AAU_ABCR;
		aau->regs.DCR = IOP310_AAU_ADCR;

		aau->irq.EOT = -1;
		aau->irq.EOC = IRQ_IOP310_AAU;
		aau->irq.ERR = -1;

		aau->device_id = "IOP310 HW XOR";
	}
#endif
	/* clear AAU control register */

	*(aau->regs.ACR) = AAU_ACR_CLEAR;
	*(aau->regs.ASR) |= (AAU_ASR_DONE_MASK | AAU_ASR_ERR_MASK);
	*(aau->regs.ANDAR) = 0;

	if(aau->irq.EOT >= 0)
		err = request_irq(aau->irq.EOT, iop3xx_aau_isr, SA_INTERRUPT,
				  aau->device_id, (void *)aau);
	if (err < 0) {
		printk(KERN_ERR "%s: unable to request IRQ %d for "
		       "AAU %d: %d\n", aau->device_id, aau->irq.EOT, 0, err);
		return -EBUSY;
	}

	if(aau->irq.EOC >= 0)
		err = request_irq(aau->irq.EOC, iop3xx_aau_isr, SA_INTERRUPT,
				  aau->device_id, (void *)aau);
	if (err < 0) {
		printk(KERN_ERR "%s: unable to request IRQ %d for "
		       "AAU %d: %d\n", aau->device_id, aau->irq.EOC, 0, err);
		return -EBUSY;
	}

	if(aau->irq.ERR >= 0)
		err = request_irq(aau->irq.ERR, iop3xx_aau_isr, SA_INTERRUPT,
				  aau->device_id, (void *)aau);
	if (err < 0) {
		printk(KERN_ERR "%s: unable to request IRQ %d for "
		       "AAU %d: %d\n", aau->device_id, aau->irq.ERR, 0, err);
		return -EBUSY;
	}

	err = iop3xx_aau_chaininit(aau);
	if (err < 0) {
		printk(KERN_ERR "unable to setup chaining\n");
		return -EIO;
	}

	DPRINTK("AAU init Done!\n");
	return 0;
}				/* end of iop3xx_aau_init() */

void
iop3xx_aau_exit(void)
{
	sw_aau_t *sw_desc = NULL;

	while (!list_empty(&aau->free_q)) {
		sw_desc = list_entry(aau->free_q.next, sw_aau_t, link);
		list_del(&sw_desc->link);
		iounmap(sw_desc->aau_desc);
		kfree(sw_desc->aau_virt);
		kfree((void *) sw_desc);
	}

	while (!list_empty(&aau->process_q)) {
		sw_desc = list_entry(aau->process_q.next, sw_aau_t, link);
		list_del(&sw_desc->link);
		iounmap(sw_desc->aau_desc);
		kfree(sw_desc->aau_virt);
		kfree((void *) sw_desc);
	}

	// get rid of the last dangle descriptor
	sw_desc = aau->last_desc;
	iounmap(sw_desc->aau_desc);
	kfree(sw_desc->aau_virt);
	kfree((void *) sw_desc);

	free_irq(aau->irq.EOC, (void *) &aau);
	free_irq(aau->irq.EOT, (void *) &aau);
	free_irq(aau->irq.ERR, (void *) &aau);
}

static int
iop3xx_aau_chaininit(iop3xx_aau_t * aa)
{
	int flags = 0;
	sw_aau_t *sw_desc = NULL;
	u32 csr = 0;

	spin_lock_irqsave(&aau->free_lock, flags);

	if (!list_empty(&aau->free_q)) {
		sw_desc = list_entry(aau->free_q.next, sw_aau_t, link);
		list_del(&sw_desc->link);
	} else {
		/* 
		 * This should NEVER happen. We need to increase the amount
		 * of descriptors if that is the case
		 *
		 * TODO: put in something nice later to handle this!
		 */
		spin_unlock_irqrestore(&aau->free_lock, flags);
		DPRINTK(KERN_ERR "out of AAU descriptors! Increase!\n");
		BUG();
		return -ENOMEM;
	}
	spin_unlock_irqrestore(&aau->free_lock, flags);

	DPRINTK("soft AAU desc acquired");

	/*
	 * Initialize HW chaining
	 */
	sw_desc->aau_desc->BC = 0;
	sw_desc->aau_desc->DC = 0;	// NULL command
	sw_desc->aau_desc->NDAR = 0;
	sw_desc->next = NULL;

	spin_lock_irqsave(&aau->process_lock, flags);
	list_add(&sw_desc->link, &aau->process_q);
	spin_unlock_irqrestore(&aau->process_lock, flags);

	// Do AAU stuff
	*(aau->regs.ASR) |= (AAU_ASR_DONE_MASK | AAU_ASR_ERR_MASK);
	*(aau->regs.ANDAR) = sw_desc->aau_phys;
	*(aau->regs.ACR) = AAU_ACR_AAU_ENABLE;

	DPRINTK("we are polling");
	csr = *(aau->regs.ASR);
	while (csr & AAU_ASR_ACTIVE) {
		csr = *(aau->regs.ASR);
	}

	DPRINTK("removing from processing queue");
	spin_lock_irqsave(&aau->process_lock, flags);
	list_del(&sw_desc->link);
	spin_unlock_irqrestore(&aau->process_lock, flags);

	sw_desc->status = AAU_FREE;
	spin_lock_irqsave(&aau->free_lock, flags);
	list_add_tail(&sw_desc->link, &aau->free_q);
	spin_unlock_irqrestore(&aau->free_lock, flags);

	// check to see if AAU failed
	if (!(csr & AAU_ASR_ERR_MASK)) {
		aau->last_desc = sw_desc;
	} else {
		aau->last_desc = NULL;
		return -EIO;
	}

	return 0;
}				/* end of iop3xx_aau_chaininit() */

module_init(iop3xx_aau_init);
module_exit(iop3xx_aau_exit);
MODULE_LICENSE(GPL);

