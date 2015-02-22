/*
 *  linux/drivers/misc/mcp-sa1100.c
 *
 *  Copyright (C) 2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 *  SA1100 MCP (Multimedia Communications Port) driver.
 *
 *  MCP read/write timeouts from Jordi Colomer, rehacked by rmk.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include <asm/dma.h>
#include <asm/hardware.h>
#include <asm/system.h>

#include "mcp.h"

static void
mcp_sa1100_set_telecom_divisor(struct mcp *mcp, unsigned int divisor)
{
	unsigned int mccr0;

	divisor /= 32;

	mccr0 = Ser4MCCR0 & ~0x00007f00;
	mccr0 |= divisor << 8;
	Ser4MCCR0 = mccr0;
}

static void
mcp_sa1100_set_audio_divisor(struct mcp *mcp, unsigned int divisor)
{
	unsigned int mccr0;

	divisor /= 32;

	mccr0 = Ser4MCCR0 & ~0x0000007f;
	mccr0 |= divisor;
	Ser4MCCR0 = mccr0;
}

/*
 * Write data to the device.  The bit should be set after 3 subframe
 * times (each frame is 64 clocks).  We wait a maximum of 6 subframes.
 * We really should try doing something more productive while we
 * wait.
 */
static void
mcp_sa1100_write(struct mcp *mcp, unsigned int reg, unsigned int val)
{
	int ret = -ETIME;
	int i;

	Ser4MCDR2 = reg << 17 | MCDR2_Wr | (val & 0xffff);

	for (i = 0; i < 2; i++) {
		udelay(mcp->rw_timeout);
		if (Ser4MCSR & MCSR_CWC) {
			ret = 0;
			break;
		}
	}

	if (ret < 0)
		printk(KERN_WARNING "mcp: write timed out\n");
}

/*
 * Read data from the device.  The bit should be set after 3 subframe
 * times (each frame is 64 clocks).  We wait a maximum of 6 subframes.
 * We really should try doing something more productive while we
 * wait.
 */
static unsigned int
mcp_sa1100_read(struct mcp *mcp, unsigned int reg)
{
	int ret = -ETIME;
	int i;

	Ser4MCDR2 = reg << 17 | MCDR2_Rd;

	for (i = 0; i < 2; i++) {
		udelay(mcp->rw_timeout);
		if (Ser4MCSR & MCSR_CRC) {
			ret = Ser4MCDR2 & 0xffff;
			break;
		}
	}

	if (ret < 0)
		printk(KERN_WARNING "mcp: read timed out\n");

	return ret;
}

static void mcp_sa1100_enable(struct mcp *mcp)
{
	Ser4MCSR = -1;
	Ser4MCCR0 |= MCCR0_MCE;
}

static void mcp_sa1100_disable(struct mcp *mcp)
{
	Ser4MCCR0 &= ~MCCR0_MCE;
}

struct mcp mcp_sa1100 = {
	owner:			THIS_MODULE,
	lock:			SPIN_LOCK_UNLOCKED,
	sclk_rate:		11981000,
	dma_audio_rd:		DMA_Ser4MCP0Rd,
	dma_audio_wr:		DMA_Ser4MCP0Wr,
	dma_telco_rd:		DMA_Ser4MCP1Rd,
	dma_telco_wr:		DMA_Ser4MCP1Wr,
	set_telecom_divisor:	mcp_sa1100_set_telecom_divisor,
	set_audio_divisor:	mcp_sa1100_set_audio_divisor,
	reg_write:		mcp_sa1100_write,
	reg_read:		mcp_sa1100_read,
	enable:			mcp_sa1100_enable,
	disable:		mcp_sa1100_disable,
};

/*
 * This needs re-working
 */
static int mcp_sa1100_init(void)
{
	struct mcp *mcp = &mcp_sa1100;
	int ret = -ENODEV;

	if (machine_is_accelent_sa()    ||
	    machine_is_adsbitsy()       || machine_is_assabet()        ||
	    machine_is_cerf()           || machine_is_flexanet()       ||
	    machine_is_freebird()       || machine_is_graphicsclient() ||
	    machine_is_graphicsmaster() || machine_is_lart()           ||
	    machine_is_omnimeter()      || machine_is_pfs168()         ||
	    machine_is_shannon()        || machine_is_simpad()         ||
	    machine_is_simputer()       || machine_is_yopy()) {
		/*
		 * Setup the PPC unit correctly.
		 */
		PPDR &= ~PPC_RXD4;
		PPDR |= PPC_TXD4 | PPC_SCLK | PPC_SFRM;
		PSDR |= PPC_RXD4;
		PSDR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);
		PPSR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);

		Ser4MCSR = -1;
		Ser4MCCR1 = 0;
		Ser4MCCR0 = 0x00007f7f | MCCR0_ADM;

		/*
		 * Calculate the read/write timeout (us) from the bit clock
		 * rate.  This is the period for 3 64-bit frames.  Always
		 * round this time up.
		 */
		mcp->rw_timeout = (64 * 3 * 1000000 + mcp->sclk_rate - 1) /
				  mcp->sclk_rate;

		ret = mcp_register(mcp);
	}

	return ret;
}

module_init(mcp_sa1100_init);
EXPORT_SYMBOL(mcp_sa1100_init);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("SA11x0 multimedia communications port driver");
MODULE_LICENSE("GPL");
