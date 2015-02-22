/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysdev.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>

#include "ctrlEnv/mvCtrlEnvLib.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "gpp/mvGpp.h"
#include "mvOs.h"


unsigned int  irq_int_type[NR_IRQS];

static void mv_mask_irq(unsigned int irq)
{
	if(irq < 32) {
		MV_REG_BIT_RESET(MV_IRQ_MASK_LOW_REG, (1 << irq) );
	} else if(irq < 64) {		/* (irq > 32 && irq < 64) */
		MV_REG_BIT_RESET(MV_IRQ_MASK_HIGH_REG, (1 << (irq - 32)) );
	} else if(irq < 96) { /* (irq > 64 && irq < 96) */
		MV_REG_BIT_RESET(MV_IRQ_MASK_ERROR_REG, (1 << (irq - 64)) );
	} else if(irq < NR_IRQS) {	/* (irq > 96 && irq < NR_IRQS) */
		MV_U32 bitmask = 1 << (irq & 0x1F);
		MV_U32 reg = (irq - IRQ_GPP_START) >> 5;
		MV_REG_BIT_RESET(MV_GPP_IRQ_MASK_REG(reg), bitmask);
	} else
		printk("mv_mask_irq: ERR, irq-%u out of scope\n",irq);

	return;
}


static void mv_unmask_irq(unsigned int irq)
{
	if(irq < 32) {
		MV_REG_BIT_SET(MV_IRQ_MASK_LOW_REG, (1 << irq) );
	} else if(irq < 64) {	/* (irq > 32 && irq < 64) */
		MV_REG_BIT_SET(MV_IRQ_MASK_HIGH_REG, (1 << (irq - 32)) );
	} else if(irq < 96) { /* (irq > 64 && irq < 96) */
		MV_REG_BIT_SET(MV_IRQ_MASK_ERROR_REG, (1 << (irq - 64)) );
	} else if(irq < NR_IRQS) {
		MV_U32 bitmask = 1 << (irq & 0x1F);
		MV_U32 reg = (irq - IRQ_GPP_START) >> 5;
		MV_REG_BIT_SET(MV_GPP_IRQ_MASK_REG(reg), bitmask);
		if (irq_int_type[irq] == GPP_IRQ_TYPE_CHANGE_LEVEL) {
			(MV_REG_READ(MV_GPP_IRQ_POLARITY(reg)) & bitmask)?
				MV_REG_BIT_RESET(MV_GPP_IRQ_POLARITY(reg), bitmask):
				MV_REG_BIT_SET(MV_GPP_IRQ_POLARITY(reg), bitmask);
		}
	} else
		printk("mv_unmask_irq: ERR, irq-%u out of scope\n",irq);

	return;
}

struct irqchip mv_chip = {
	.ack	= mv_mask_irq,
	.mask	= mv_mask_irq,
	.unmask = mv_unmask_irq,
};

void __init mv_init_irq(void)
{
	u32 i;
	MV_U32 gppMask;

	/* Disable all interrupts initially. */
	MV_REG_WRITE(MV_IRQ_MASK_LOW_REG, 0x0);
	MV_REG_WRITE(MV_IRQ_MASK_HIGH_REG, 0x0);
	MV_REG_WRITE(MV_IRQ_MASK_ERROR_REG, 0x0);

	for(i = 0; i < 3; i++) {
		MV_REG_WRITE(MV_GPP_IRQ_MASK_REG(i), 0x0);
		MV_REG_WRITE(MV_GPP_IRQ_EDGE_REG(i), 0x0);
	}

	/* Set gpp interrupts as needed */
	for(i = 0; i < MV_GPP_MAX_GROUP; i++) {
		gppMask = mvBoardGpioIntMaskGet(i);
		mvGppTypeSet(i, gppMask , (MV_GPP_IN & gppMask));
		mvGppPolaritySet(i, gppMask , (MV_GPP_IN_INVERT & gppMask));
	}

	/* enable GPP in the main mask high */
	MV_REG_BIT_SET(MV_IRQ_MASK_HIGH_REG,
			MV_BIT_MASK(GPP_LOW_0_7_IRQ_NUM - 32)    |
			MV_BIT_MASK(GPP_LOW_8_15_IRQ_NUM - 32)   |
			MV_BIT_MASK(GPP_LOW_16_23_IRQ_NUM - 32)  |
			MV_BIT_MASK(GPP_LOW_24_31_IRQ_NUM - 32)  |
			MV_BIT_MASK(GPP_MID_0_7_IRQ_NUM	- 32)    |
			MV_BIT_MASK(GPP_MID_8_15_IRQ_NUM - 32)   |
			MV_BIT_MASK(GPP_MID_16_23_IRQ_NUM - 32)  |
			MV_BIT_MASK(GPP_MID_24_31_IRQ_NUM - 32)  |
			MV_BIT_MASK(GPP_HIGH_0_7_IRQ_NUM - 32)   |
			MV_BIT_MASK(GPP_HIGH_8_15_IRQ_NUM - 32)  |
			MV_BIT_MASK(GPP_HIGH_16_23_IRQ_NUM - 32) |
			MV_BIT_MASK(GPP_HIGH_24_IRQ_NUM - 32));

	/* clear all int */
	MV_REG_WRITE(MV_IRQ_CAUSE_LOW_REG, 0x0);
	MV_REG_WRITE(MV_IRQ_CAUSE_HIGH_REG, 0x0);
	MV_REG_WRITE(MV_IRQ_CAUSE_ERROR_REG, 0x0);

	for(i = 0; i < 3; i++)
		MV_REG_WRITE(MV_GPP_IRQ_CAUSE_REG(i), 0x0);

	/* Do the core module ones */
	for (i = 0; i < NR_IRQS; i++) {
		set_irq_chip(i, &mv_chip);
		set_irq_handler(i, do_level_IRQ);
		set_irq_flags(i, IRQF_VALID | IRQF_PROBE);
	}

	/* init GPP IRQs in default level mode*/
	for (i = 0; i < NR_IRQS; i++)
		irq_int_type[i] = GPP_IRQ_TYPE_LEVEL;

	/* TBD. Add support for error interrupts */

	return;
}

