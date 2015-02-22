/*
 *  linux/drivers/misc/ucb1x00-core.c
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 *  The UCB1x00 core driver provides basic services for handling IO,
 *  the ADC, interrupts, and accessing registers.  It is designed
 *  such that everything goes through this layer, thereby providing
 *  a consistent locking methodology, as well as allowing the drivers
 *  to be used on other non-MCP-enabled hardware platforms.
 *
 *  Note that all locks are private to this file.  Nothing else may
 *  touch them.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/pm.h>

#include <asm/dma.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/arch/shannon.h>

#include "ucb1x00.h"

/**
 *	ucb1x00_io_set_dir - set IO direction
 *	@ucb: UCB1x00 structure describing chip
 *	@in:  bitfield of IO pins to be set as inputs
 *	@out: bitfield of IO pins to be set as outputs
 *
 *	Set the IO direction of the ten general purpose IO pins on
 *	the UCB1x00 chip.  The @in bitfield has priority over the
 *	@out bitfield, in that if you specify a pin as both input
 *	and output, it will end up as an input.
 *
 *	ucb1x00_enable must have been called to enable the comms
 *	before using this function.
 *
 *	This function takes a spinlock, disabling interrupts.
 */
void ucb1x00_io_set_dir(struct ucb1x00 *ucb, unsigned int in, unsigned int out)
{
	unsigned long flags;

	spin_lock_irqsave(&ucb->io_lock, flags);
	ucb->io_dir |= out;
	ucb->io_dir &= ~in;

	ucb1x00_reg_write(ucb, UCB_IO_DIR, ucb->io_dir);
	spin_unlock_irqrestore(&ucb->io_lock, flags);
}

/**
 *	ucb1x00_io_write - set or clear IO outputs
 *	@ucb:   UCB1x00 structure describing chip
 *	@set:   bitfield of IO pins to set to logic '1'
 *	@clear: bitfield of IO pins to set to logic '0'
 *
 *	Set the IO output state of the specified IO pins.  The value
 *	is retained if the pins are subsequently configured as inputs.
 *	The @clear bitfield has priority over the @set bitfield -
 *	outputs will be cleared.
 *
 *	ucb1x00_enable must have been called to enable the comms
 *	before using this function.
 *
 *	This function takes a spinlock, disabling interrupts.
 */
void ucb1x00_io_write(struct ucb1x00 *ucb, unsigned int set, unsigned int clear)
{
	unsigned long flags;

	spin_lock_irqsave(&ucb->io_lock, flags);
	ucb->io_out |= set;
	ucb->io_out &= ~clear;

	ucb1x00_reg_write(ucb, UCB_IO_DATA, ucb->io_out);
	spin_unlock_irqrestore(&ucb->io_lock, flags);
}

/**
 *	ucb1x00_io_read - read the current state of the IO pins
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Return a bitfield describing the logic state of the ten
 *	general purpose IO pins.
 *
 *	ucb1x00_enable must have been called to enable the comms
 *	before using this function.
 *
 *	This function does not take any semaphores or spinlocks.
 */
unsigned int ucb1x00_io_read(struct ucb1x00 *ucb)
{
	return ucb1x00_reg_read(ucb, UCB_IO_DATA);
}

/*
 * UCB1300 data sheet says we must:
 *  1. enable ADC	=> 5us (including reference startup time)
 *  2. select input	=> 51*tsibclk  => 4.3us
 *  3. start conversion	=> 102*tsibclk => 8.5us
 * (tsibclk = 1/11981000)
 * Period between SIB 128-bit frames = 10.7us
 */

/**
 *	ucb1x00_adc_enable - enable the ADC converter
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Enable the ucb1x00 and ADC converter on the UCB1x00 for use.
 *	Any code wishing to use the ADC converter must call this
 *	function prior to using it.
 *
 *	This function takes the ADC semaphore to prevent two or more
 *	concurrent uses, and therefore may sleep.  As a result, it
 *	can only be called from process context, not interrupt
 *	context.
 *
 *	You should release the ADC as soon as possible using
 *	ucb1x00_adc_disable.
 */
void ucb1x00_adc_enable(struct ucb1x00 *ucb)
{
	down(&ucb->adc_sem);

	ucb->adc_cr |= UCB_ADC_ENA;

	ucb1x00_enable(ucb);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr);
}

/**
 *	ucb1x00_adc_read - read the specified ADC channel
 *	@ucb: UCB1x00 structure describing chip
 *	@adc_channel: ADC channel mask
 *	@sync: wait for syncronisation pulse.
 *
 *	Start an ADC conversion and wait for the result.  Note that
 *	synchronised ADC conversions (via the ADCSYNC pin) must wait
 *	until the trigger is asserted and the conversion is finished.
 *
 *	This function currently spins waiting for the conversion to
 *	complete (2 frames max without sync).
 *
 *	If called for a synchronised ADC conversion, it may sleep
 *	with the ADC semaphore held.
 */
unsigned int ucb1x00_adc_read(struct ucb1x00 *ucb, int adc_channel, int sync)
{
	unsigned int val;

	if (sync)
		adc_channel |= UCB_ADC_SYNC_ENA;

	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr | adc_channel);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr | adc_channel | UCB_ADC_START);

	for (;;) {
		val = ucb1x00_reg_read(ucb, UCB_ADC_DATA);
		if (val & UCB_ADC_DAT_VAL)
			break;
		/* yield to other processes */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
	}

	return UCB_ADC_DAT(val);
}

/**
 *	ucb1x00_adc_disable - disable the ADC converter
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Disable the ADC converter and release the ADC semaphore.
 */
void ucb1x00_adc_disable(struct ucb1x00 *ucb)
{
	ucb->adc_cr &= ~UCB_ADC_ENA;
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr);
	ucb1x00_disable(ucb);

	up(&ucb->adc_sem);
}

#ifdef CONFIG_PM
static int ucb1x00_pm (struct pm_dev *dev, pm_request_t rqst, void *data)
{
	struct ucb1x00 *ucb = (struct ucb1x00 *)dev->data;
	unsigned int isr;

	if (rqst == PM_RESUME) {
		ucb1x00_enable(ucb);
		isr = ucb1x00_reg_read(ucb, UCB_IE_STATUS);
		ucb1x00_reg_write(ucb, UCB_IE_CLEAR, isr);
		ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);
		ucb1x00_disable(ucb);
	}

	return 0;
}
#endif

/*
 * UCB1x00 Interrupt handling.
 *
 * The UCB1x00 can generate interrupts when the SIBCLK is stopped.
 * Since we need to read an internal register, we must re-enable
 * SIBCLK to talk to the chip.  We leave the clock running until
 * we have finished processing all interrupts from the chip.
 */
static void ucb1x00_irq(int irqnr, void *devid, struct pt_regs *regs)
{
	struct ucb1x00 *ucb = devid;
	struct ucb1x00_irq *irq;
	unsigned int isr, i;

	ucb1x00_enable(ucb);
	isr = ucb1x00_reg_read(ucb, UCB_IE_STATUS);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, isr);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);

	for (i = 0, irq = ucb->irq_handler; i < 16 && isr; i++, isr >>= 1, irq++)
		if (isr & 1 && irq->fn)
			irq->fn(i, irq->devid);
	ucb1x00_disable(ucb);
}

/**
 *	ucb1x00_hook_irq - hook a UCB1x00 interrupt
 *	@ucb:   UCB1x00 structure describing chip
 *	@idx:   interrupt index
 *	@fn:    function to call when interrupt is triggered
 *	@devid: device id to pass to interrupt handler
 *
 *	Hook the specified interrupt.  You can only register one handler
 *	for each interrupt source.  The interrupt source is not enabled
 *	by this function; use ucb1x00_enable_irq instead.
 *
 *	Interrupt handlers will be called with other interrupts enabled.
 *
 *	Returns zero on success, or one of the following errors:
 *	 -EINVAL if the interrupt index is invalid
 *	 -EBUSY if the interrupt has already been hooked
 */
int ucb1x00_hook_irq(struct ucb1x00 *ucb, unsigned int idx, void (*fn)(int, void *), void *devid)
{
	struct ucb1x00_irq *irq;
	int ret = -EINVAL;

	if (idx < 16) {
		irq = ucb->irq_handler + idx;
		ret = -EBUSY;

		spin_lock_irq(&ucb->lock);
		if (irq->fn == NULL) {
			irq->devid = devid;
			irq->fn = fn;
			ret = 0;
		}
		spin_unlock_irq(&ucb->lock);
	}
	return ret;
}

/**
 *	ucb1x00_enable_irq - enable an UCB1x00 interrupt source
 *	@ucb: UCB1x00 structure describing chip
 *	@idx: interrupt index
 *	@edges: interrupt edges to enable
 *
 *	Enable the specified interrupt to trigger on %UCB_RISING,
 *	%UCB_FALLING or both edges.  The interrupt should have been
 *	hooked by ucb1x00_hook_irq.
 */
void ucb1x00_enable_irq(struct ucb1x00 *ucb, unsigned int idx, int edges)
{
	unsigned long flags;

	if (idx < 16) {
		spin_lock_irqsave(&ucb->lock, flags);

		ucb1x00_enable(ucb);
		if (edges & UCB_RISING) {
			ucb->irq_ris_enbl |= 1 << idx;
			ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl);
		}
		if (edges & UCB_FALLING) {
			ucb->irq_fal_enbl |= 1 << idx;
			ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl);
		}
		ucb1x00_disable(ucb);
		spin_unlock_irqrestore(&ucb->lock, flags);
	}
}

/**
 *	ucb1x00_disable_irq - disable an UCB1x00 interrupt source
 *	@ucb: UCB1x00 structure describing chip
 *	@edges: interrupt edges to disable
 *
 *	Disable the specified interrupt triggering on the specified
 *	(%UCB_RISING, %UCB_FALLING or both) edges.
 */
void ucb1x00_disable_irq(struct ucb1x00 *ucb, unsigned int idx, int edges)
{
	unsigned long flags;

	if (idx < 16) {
		spin_lock_irqsave(&ucb->lock, flags);

		ucb1x00_enable(ucb);
		if (edges & UCB_RISING) {
			ucb->irq_ris_enbl &= ~(1 << idx);
			ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl);
		}
		if (edges & UCB_FALLING) {
			ucb->irq_fal_enbl &= ~(1 << idx);
			ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl);
		}
		ucb1x00_disable(ucb);
		spin_unlock_irqrestore(&ucb->lock, flags);
	}
}

/**
 *	ucb1x00_free_irq - disable and free the specified UCB1x00 interrupt
 *	@ucb: UCB1x00 structure describing chip
 *	@idx: interrupt index
 *	@devid: device id.
 *
 *	Disable the interrupt source and remove the handler.  devid must
 *	match the devid passed when hooking the interrupt.
 *
 *	Returns zero on success, or one of the following errors:
 *	 -EINVAL if the interrupt index is invalid
 *	 -ENOENT if devid does not match
 */
int ucb1x00_free_irq(struct ucb1x00 *ucb, unsigned int idx, void *devid)
{
	struct ucb1x00_irq *irq;
	int ret;

	if (idx >= 16)
		goto bad;

	irq = ucb->irq_handler + idx;
	ret = -ENOENT;

	spin_lock_irq(&ucb->lock);
	if (irq->devid == devid) {
		ucb->irq_ris_enbl &= ~(1 << idx);
		ucb->irq_fal_enbl &= ~(1 << idx);

		ucb1x00_enable(ucb);
		ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl);
		ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl);
		ucb1x00_disable(ucb);

		irq->fn = NULL;
		irq->devid = NULL;
		ret = 0;
	}
	spin_unlock_irq(&ucb->lock);
	return ret;

bad:
	printk(KERN_ERR "%s: freeing bad irq %d\n", __FUNCTION__, idx);
	return -EINVAL;
}

/*
 * Try to probe our interrupt, rather than relying on lots of
 * hard-coded machine dependencies.  For reference, the expected
 * IRQ mappings are:
 *
 *  	Machine		Default IRQ
 *	adsbitsy	IRQ_GPCIN4
 *	cerf		IRQ_GPIO_UCB1200_IRQ
 *	flexanet	IRQ_GPIO_GUI
 *	freebird	IRQ_GPIO_FREEBIRD_UCB1300_IRQ
 *	graphicsclient	ADS_EXT_IRQ(8)
 *	graphicsmaster	ADS_EXT_IRQ(8)
 *	lart		LART_IRQ_UCB1200
 *	omnimeter	IRQ_GPIO23
 *	pfs168		IRQ_GPIO_UCB1300_IRQ
 *	simpad		IRQ_GPIO_UCB1300_IRQ
 *	shannon		SHANNON_IRQ_GPIO_IRQ_CODEC
 *	yopy		IRQ_GPIO_UCB1200_IRQ
 */
static int __init ucb1x00_detect_irq(struct ucb1x00 *ucb)
{
	unsigned long mask;

	mask = probe_irq_on();
	if (!mask)
		return NO_IRQ;

	/*
	 * Enable the ADC interrupt.
	 */
	ucb1x00_reg_write(ucb, UCB_IE_RIS, UCB_IE_ADC);
	ucb1x00_reg_write(ucb, UCB_IE_FAL, UCB_IE_ADC);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0xffff);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);

	/*
	 * Cause an ADC interrupt.
	 */
	ucb1x00_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, UCB_ADC_ENA | UCB_ADC_START);

	/*
	 * Wait for the conversion to complete.
	 */
	while ((ucb1x00_reg_read(ucb, UCB_ADC_DATA) & UCB_ADC_DAT_VAL) == 0);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, 0);

	/*
	 * Disable and clear interrupt.
	 */
	ucb1x00_reg_write(ucb, UCB_IE_RIS, 0);
	ucb1x00_reg_write(ucb, UCB_IE_FAL, 0);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0xffff);
	ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);

	/*
	 * Read triggered interrupt.
	 */
	return probe_irq_off(mask);
}

/*
 * This configures the UCB1x00 layer depending on the machine type
 * we're running on.  The UCB1x00 drivers should not contain any
 * machine dependencies.
 *
 * We can get rid of some of these dependencies by using existing
 * facilities provided by the kernel - namely IRQ probing.  The
 * machine specific files are expected to setup the IRQ levels on
 * initialisation.  With any luck, we'll get rid of all the
 * machine dependencies here.
 */
static int __init ucb1x00_configure(struct ucb1x00 *ucb)
{
	unsigned int irq_gpio_pin = 0;
	int irq, default_irq = NO_IRQ;

	if (machine_is_adsbitsy())
		default_irq = IRQ_GPCIN4;

//	if (machine_is_assabet())
//		default_irq = IRQ_GPIO23;

#ifdef CONFIG_SA1100_CERF
	if (machine_is_cerf())
		default_irq = IRQ_GPIO_UCB1200_IRQ;
#endif
#ifdef CONFIG_SA1100_FREEBIRD
	if (machine_is_freebird())
		default_irq = IRQ_GPIO_FREEBIRD_UCB1300_IRQ;
#endif
#if defined(CONFIG_SA1100_GRAPHICSCLIENT) || defined(CONFIG_SA1100_GRAPICSMASTER)
	if (machine_is_graphicsclient() || machine_is_graphicsmaster())
		default_irq = ADS_EXT_IRQ(8);
#endif
#ifdef CONFIG_SA1100_LART
	if (machine_is_lart()) {
		default_irq = LART_IRQ_UCB1200;
		irq_gpio_pin = LART_GPIO_UCB1200;
	}
#endif
	if (machine_is_omnimeter())
		default_irq = IRQ_GPIO23;

#ifdef CONFIG_SA1100_PFS168
	if (machine_is_pfs168())
		default_irq = IRQ_GPIO_UCB1300_IRQ;
#endif
#ifdef CONFIG_SA1100_SIMPAD
	if (machine_is_simpad())
		default_irq = IRQ_GPIO_UCB1300_IRQ;
#endif
#ifdef CONFIG_SA1100_SIMPUTER
	if (machine_is_simputer()) {
		default_irq = IRQ_GPIO_UCB1300_IRQ;
		irq_gpio_pin = GPIO_UCB1300_IRQ;
    }
#endif
	if (machine_is_shannon())
		default_irq = SHANNON_IRQ_GPIO_IRQ_CODEC;
#ifdef CONFIG_SA1100_YOPY
	if (machine_is_yopy())
		default_irq = IRQ_GPIO_UCB1200_IRQ;
#endif
#ifdef CONFIG_SA1100_ACCELENT
	if (machine_is_accelent_sa()) {
		ucb->irq = IRQ_GPIO_UCB1200_IRQ;
		irq_gpio_pin = GPIO_UCB1200_IRQ;
	}
#endif

	/*
	 * Eventually, this will disappear.
	 */
	if (irq_gpio_pin)
		set_GPIO_IRQ_edge(irq_gpio_pin, GPIO_RISING_EDGE);

	irq = ucb1x00_detect_irq(ucb);
	if (irq != NO_IRQ) {
		if (default_irq != NO_IRQ && irq != default_irq)
			printk(KERN_ERR "UCB1x00: probed IRQ%d != default IRQ%d\n",
				irq, default_irq);
		if (irq == default_irq)
			printk(KERN_ERR "UCB1x00: probed IRQ%d correctly. "
				"Please remove machine dependencies from "
				"ucb1x00-core.c\n", irq);
		ucb->irq = irq;
	} else {
		printk(KERN_ERR "UCB1x00: IRQ probe failed, using IRQ%d\n",
			default_irq);
		ucb->irq = default_irq;
	}

	return ucb->irq == NO_IRQ ? -ENODEV : 0;
}

struct ucb1x00 *my_ucb;

/**
 *	ucb1x00_get - get the UCB1x00 structure describing a chip
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Return the UCB1x00 structure describing a chip.
 *
 *	FIXME: Currently very noddy indeed, which currently doesn't
 *	matter since we only support one chip.
 */
struct ucb1x00 *ucb1x00_get(void)
{
	return my_ucb;
}

static int __init ucb1x00_init(void)
{
	struct mcp *mcp;
	unsigned int id;
	int ret = -ENODEV;

	mcp = mcp_get();
	if (!mcp)
		goto no_mcp;

	mcp_enable(mcp);
	id = mcp_reg_read(mcp, UCB_ID);

	if (id != UCB_ID_1200 && id != UCB_ID_1300) {
		printk(KERN_WARNING "UCB1x00 ID not found: %04x\n", id);
		goto out;
	}

	my_ucb = kmalloc(sizeof(struct ucb1x00), GFP_KERNEL);
	ret = -ENOMEM;
	if (!my_ucb)
		goto out;

	if (machine_is_shannon()) {
		/* reset the codec */
		GPDR |= SHANNON_GPIO_CODEC_RESET;
		GPCR = SHANNON_GPIO_CODEC_RESET;
		GPSR = SHANNON_GPIO_CODEC_RESET;

	}

	memset(my_ucb, 0, sizeof(struct ucb1x00));

	spin_lock_init(&my_ucb->lock);
	spin_lock_init(&my_ucb->io_lock);
	sema_init(&my_ucb->adc_sem, 1);

	my_ucb->id  = id;
	my_ucb->mcp = mcp;

	ret = ucb1x00_configure(my_ucb);
	if (ret)
		goto out;

	ret = request_irq(my_ucb->irq, ucb1x00_irq, 0, "UCB1x00", my_ucb);
	if (ret) {
		printk(KERN_ERR "ucb1x00: unable to grab irq%d: %d\n",
			my_ucb->irq, ret);
		kfree(my_ucb);
		my_ucb = NULL;
		goto out;
	}

#ifdef CONFIG_PM
	my_ucb->pmdev = pm_register(PM_SYS_DEV, PM_SYS_UNKNOWN, ucb1x00_pm);
	if (my_ucb->pmdev == NULL)
		printk("ucb1x00: unable to register in PM.\n");
	else
		my_ucb->pmdev->data = my_ucb;
#endif

out:
	mcp_disable(mcp);
no_mcp:
	return ret;
}

static void __exit ucb1x00_exit(void)
{
	free_irq(my_ucb->irq, my_ucb);
	kfree(my_ucb);
}

module_init(ucb1x00_init);
module_exit(ucb1x00_exit);

EXPORT_SYMBOL(ucb1x00_get);

EXPORT_SYMBOL(ucb1x00_io_set_dir);
EXPORT_SYMBOL(ucb1x00_io_write);
EXPORT_SYMBOL(ucb1x00_io_read);

EXPORT_SYMBOL(ucb1x00_adc_enable);
EXPORT_SYMBOL(ucb1x00_adc_read);
EXPORT_SYMBOL(ucb1x00_adc_disable);

EXPORT_SYMBOL(ucb1x00_hook_irq);
EXPORT_SYMBOL(ucb1x00_free_irq);
EXPORT_SYMBOL(ucb1x00_enable_irq);
EXPORT_SYMBOL(ucb1x00_disable_irq);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("UCB1x00 core driver");
MODULE_LICENSE("GPL");
