/* linux/arch/arm/mach-s3c2410/gpio.c
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 GPIO support
 *
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
 *
 * Changelog
 *	13-Sep-2004  BJD  Implemented change of MISCCR
 *	14-Sep-2004  BJD  Added getpin call
 *	14-Sep-2004  BJD  Fixed bug in setpin() call
 *	30-Sep-2004  BJD  Fixed cfgpin() mask bug
 *	01-Oct-2004  BJD  Added getcfg() to get pin configuration
 *	01-Oct-2004  BJD  Fixed mask bug in pullup() call
 *	01-Oct-2004  BJD  Added getirq() to turn pin into irqno
 *	04-Oct-2004  BJD  Added irq filter controls for GPIO
 *	05-Nov-2004  BJD  EXPORT_SYMBOL() added for all code
 *	13-Mar-2005  BJD  Updates for __iomem
 *	26-Oct-2005  BJD  Added generic configuration types
 *	15-Jan-2006  LCVR Added support for the S3C2400
 */


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/arch/regs-gpio.h>

void s3c2410_gpio_cfgpin(unsigned int pin, unsigned int function)
{
	void __iomem *base = S3C24XX_GPIO_BASE(pin);
	unsigned long mask;
	unsigned long con;
	unsigned long flags;

	if (pin < S3C2410_GPIO_BANKB) {
		mask = 1 << S3C2410_GPIO_OFFSET(pin);
	} else {
		mask = 3 << S3C2410_GPIO_OFFSET(pin)*2;
	}

	switch (function) {
	case S3C2410_GPIO_LEAVE:
		mask = 0;
		function = 0;
		break;

	case S3C2410_GPIO_INPUT:
	case S3C2410_GPIO_OUTPUT:
	case S3C2410_GPIO_SFN2:
	case S3C2410_GPIO_SFN3:
		if (pin < S3C2410_GPIO_BANKB) {
			function &= 1;
			function <<= S3C2410_GPIO_OFFSET(pin);
		} else {
			function &= 3;
			function <<= S3C2410_GPIO_OFFSET(pin)*2;
		}
	}

	/* modify the specified register wwith IRQs off */

	local_irq_save(flags);

	con  = __raw_readl(base + 0x00);
	con &= ~mask;
	con |= function;

	__raw_writel(con, base + 0x00);

	local_irq_restore(flags);
}

EXPORT_SYMBOL(s3c2410_gpio_cfgpin);

unsigned int s3c2410_gpio_getcfg(unsigned int pin)
{
	void __iomem *base = S3C24XX_GPIO_BASE(pin);
	unsigned long mask;

	if (pin < S3C2410_GPIO_BANKB) {
		mask = 1 << S3C2410_GPIO_OFFSET(pin);
	} else {
		mask = 3 << S3C2410_GPIO_OFFSET(pin)*2;
	}

	return __raw_readl(base) & mask;
}

EXPORT_SYMBOL(s3c2410_gpio_getcfg);

void s3c2410_gpio_pullup(unsigned int pin, unsigned int to)
{
	void __iomem *base = S3C24XX_GPIO_BASE(pin);
	unsigned long offs = S3C2410_GPIO_OFFSET(pin);
	unsigned long flags;
	unsigned long up;

	if (pin < S3C2410_GPIO_BANKB)
		return;

	local_irq_save(flags);

	up = __raw_readl(base + 0x08);
	up &= ~(1L << offs);
	up |= to << offs;
	__raw_writel(up, base + 0x08);

	local_irq_restore(flags);
}

EXPORT_SYMBOL(s3c2410_gpio_pullup);

void s3c2410_gpio_setpin(unsigned int pin, unsigned int to)
{
	void __iomem *base = S3C24XX_GPIO_BASE(pin);
	unsigned long offs = S3C2410_GPIO_OFFSET(pin);
	unsigned long flags;
	unsigned long dat;

	local_irq_save(flags);

	dat = __raw_readl(base + 0x04);
	dat &= ~(1 << offs);
	dat |= to << offs;
	__raw_writel(dat, base + 0x04);

	local_irq_restore(flags);
}

EXPORT_SYMBOL(s3c2410_gpio_setpin);

unsigned int s3c2410_gpio_getpin(unsigned int pin)
{
	void __iomem *base = S3C24XX_GPIO_BASE(pin);
	unsigned long offs = S3C2410_GPIO_OFFSET(pin);

	return __raw_readl(base + 0x04) & (1<< offs);
}

EXPORT_SYMBOL(s3c2410_gpio_getpin);

unsigned int s3c2410_modify_misccr(unsigned int clear, unsigned int change)
{
	unsigned long flags;
	unsigned long misccr;

	local_irq_save(flags);
	misccr = __raw_readl(S3C24XX_MISCCR);
	misccr &= ~clear;
	misccr ^= change;
	__raw_writel(misccr, S3C24XX_MISCCR);
	local_irq_restore(flags);

	return misccr;
}

EXPORT_SYMBOL(s3c2410_modify_misccr);
