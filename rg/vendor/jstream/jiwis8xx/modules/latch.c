/****************************************************************************
 *
 * rg/vendor/jstream/jiwis8xx/modules/latch.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#include "jiwis8xx.h"

#define JIWIS8XX_LATCH_PHYS		IXP425_EXP_BUS_CS3_BASE_PHYS
#define JIWIS8XX_LATCH_CFG		IXP425_EXP_CS3
#define JIWIS8XX_LATCH_DEFAULT		0x00ff
static volatile u16 *jiwis8xx_latch_base;
static u16 jiwis8xx_latch_value;

#define JIWIS8XX_LATCH2_PHYS		IXP425_EXP_BUS_CS4_BASE_PHYS
#define JIWIS8XX_LATCH2_CFG		IXP425_EXP_CS4
#define JIWIS8XX_LATCH2_DEFAULT		0x0f00
static volatile u16 *jiwis8xx_latch2_base;
static u16 jiwis8xx_latch2_value;

/* initialize CS3 to default timings, Intel style, 16bit bus */
#define JIWIS8XX_LATCH_CS_CONFIG	0x80003c42

int jiwis8xx_latch_set(u8 latch, u8 line, u8 value)
{
    volatile u16 *base = latch ? jiwis8xx_latch2_base : jiwis8xx_latch_base;
    u16 *val = latch ? &jiwis8xx_latch2_value : &jiwis8xx_latch_value;

    if (!base)
	return -1;

    if (value)
	*val |= BIT(line);
    else
	*val &= ~BIT(line);
    
    *base = *val;

    return 0;
}
EXPORT_SYMBOL(jiwis8xx_latch_set);

static u16 *jiwis8xx_latch_init_one(u32 phys, volatile u32 *cfg, u16 init,
    char *name)
{
    u16 *base;
    
    *cfg = JIWIS8XX_LATCH_CS_CONFIG;
    
    if (!(base = ioremap(phys, 2)))
    {
	printk("%s: faled to ioremap phys: %#x\n", name, phys);
	return NULL;
    }

    *base = init;

    printk("%s: phys: %#x, virt: 0x%p\n", name, phys, base);

    return base;
}

int jiwis8xx_latch_init(void)
{
    int err = 0;

    jiwis8xx_latch_base = jiwis8xx_latch_init_one(JIWIS8XX_LATCH_PHYS,
	JIWIS8XX_LATCH_CFG, JIWIS8XX_LATCH_DEFAULT, "latch");
    jiwis8xx_latch_value = JIWIS8XX_LATCH_DEFAULT;
    if (!jiwis8xx_latch_base)
	err = -1;

    if (machine_is_jiwis800())
    {
	jiwis8xx_latch2_base = jiwis8xx_latch_init_one(JIWIS8XX_LATCH2_PHYS,
	    JIWIS8XX_LATCH2_CFG, JIWIS8XX_LATCH2_DEFAULT, "latch2");
	jiwis8xx_latch2_value = JIWIS8XX_LATCH_DEFAULT;
	if (!jiwis8xx_latch2_base)
	    err = -1;
    }

    return err;
}

void jiwis8xx_latch_exit(void)
{
    if (jiwis8xx_latch_base)
	iounmap((void *)jiwis8xx_latch_base);
    if (jiwis8xx_latch2_base)
	iounmap((void *)jiwis8xx_latch2_base);
}

module_init(jiwis8xx_latch_init);
module_exit(jiwis8xx_latch_exit);

