/*
 *  linux/drivers/misc/mcp-core.c
 *
 *  Copyright (C) 2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 *  Generic MCP (Multimedia Communications Port) layer.  All MCP locking
 *  is solely held within this file.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/smp.h>

#include <asm/dma.h>
#include <asm/system.h>

#include "mcp.h"

/**
 *	mcp_set_telecom_divisor - set the telecom divisor
 *	@mcp: MCP interface structure
 *	@div: SIB clock divisor
 *
 *	Set the telecom divisor on the MCP interface.  The resulting
 *	sample rate is SIBCLOCK/div.
 */
void mcp_set_telecom_divisor(struct mcp *mcp, unsigned int div)
{
	spin_lock_irq(&mcp->lock);
	mcp->set_telecom_divisor(mcp, div);
	spin_unlock_irq(&mcp->lock);
}

/**
 *	mcp_set_audio_divisor - set the audio divisor
 *	@mcp: MCP interface structure
 *	@div: SIB clock divisor
 *
 *	Set the audio divisor on the MCP interface.
 */
void mcp_set_audio_divisor(struct mcp *mcp, unsigned int div)
{
	spin_lock_irq(&mcp->lock);
	mcp->set_audio_divisor(mcp, div);
	spin_unlock_irq(&mcp->lock);
}

/**
 *	mcp_reg_write - write a device register
 *	@mcp: MCP interface structure
 *	@reg: 4-bit register index
 *	@val: 16-bit data value
 *
 *	Write a device register.  The MCP interface must be enabled
 *	to prevent this function hanging.
 */
void mcp_reg_write(struct mcp *mcp, unsigned int reg, unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&mcp->lock, flags);
	mcp->reg_write(mcp, reg, val);
	spin_unlock_irqrestore(&mcp->lock, flags);
}

/**
 *	mcp_reg_read - read a device register
 *	@mcp: MCP interface structure
 *	@reg: 4-bit register index
 *
 *	Read a device register and return its value.  The MCP interface
 *	must be enabled to prevent this function hanging.
 */
unsigned int mcp_reg_read(struct mcp *mcp, unsigned int reg)
{
	unsigned long flags;
	unsigned int val;

	spin_lock_irqsave(&mcp->lock, flags);
	val = mcp->reg_read(mcp, reg);
	spin_unlock_irqrestore(&mcp->lock, flags);

	return val;
}

/**
 *	mcp_enable - enable the MCP interface
 *	@mcp: MCP interface to enable
 *
 *	Enable the MCP interface.  Each call to mcp_enable will need
 *	a corresponding call to mcp_disable to disable the interface.
 */
void mcp_enable(struct mcp *mcp)
{
	spin_lock_irq(&mcp->lock);
	if (mcp->use_count++ == 0)
		mcp->enable(mcp);
	spin_unlock_irq(&mcp->lock);
}

/**
 *	mcp_disable - disable the MCP interface
 *	@mcp: MCP interface to disable
 *
 *	Disable the MCP interface.  The MCP interface will only be
 *	disabled once the number of calls to mcp_enable matches the
 *	number of calls to mcp_disable.
 */
void mcp_disable(struct mcp *mcp)
{
	unsigned long flags;

	spin_lock_irqsave(&mcp->lock, flags);
	if (--mcp->use_count == 0)
		mcp->disable(mcp);
	spin_unlock_irqrestore(&mcp->lock, flags);
}


/*
 * This needs re-working
 */
static struct mcp *mcp_if;

struct mcp *mcp_get(void)
{
	return mcp_if;
}

int mcp_register(struct mcp *mcp)
{
	if (mcp_if)
		return -EBUSY;
	if (mcp->owner)
		__MOD_INC_USE_COUNT(mcp->owner);
	mcp_if = mcp;
	return 0;
}

EXPORT_SYMBOL(mcp_set_telecom_divisor);
EXPORT_SYMBOL(mcp_set_audio_divisor);
EXPORT_SYMBOL(mcp_reg_write);
EXPORT_SYMBOL(mcp_reg_read);
EXPORT_SYMBOL(mcp_enable);
EXPORT_SYMBOL(mcp_disable);
EXPORT_SYMBOL(mcp_get);
EXPORT_SYMBOL(mcp_register);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("Core multimedia communications port driver");
MODULE_LICENSE("GPL");
