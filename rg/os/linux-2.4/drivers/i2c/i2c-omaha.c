/* ------------------------------------------------------------------------- *
    Copyright ARM Limited 2002. All rights reserved.
 
    i2c driver for Omaha					     
 
    Notes:Based on i2c-elv.c
 
    The S3C2400X01 has better support for I2C, but bit oriented operations 
    are directly supported by the other I2C layers, so we use that method
    of performing I2C operations.

    Copyright (C) 1995-2000 Simon G. Vogl

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include <asm/io.h>
#include <asm/hardware.h>

/* ----- global defines ----------------------------------------------- */
#define DEB(x) if (i2c_debug>=1) x;
#define DEB2(x) if (i2c_debug>=2) x;
#define DEB3(x) if (i2c_debug>=3) x
#define DEBE(x)	x	// error messages
#define DEBSTAT(x) if (i2c_debug>=3) x; /* print several statistical values*/
#define DEBPROTO(x) if (i2c_debug>=9) { x; }
 	/* debug the protocol by showing transferred bits */

/* Register and bitdefs for Omaha */

// Port G control registers
static volatile unsigned int pgcon = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_PGCON);
static volatile unsigned int pgdat = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_PGDAT);

static volatile unsigned int opencr = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_OPENCR);

static int base = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_PGCON);

// Open drain control registers
#define OPC_CMD		BIT2
#define OPC_DAT		BIT3

// data bits in GPIO Port G data register
#define OMAHA_SDA	BIT5
#define OMAHA_SCL	BIT6
#define IIC_WP		BIT3	// Write Protect for EEPROM

// input/out select bits in GPIO G control register
#define IIC_BITS	(BIT12|BIT10|BIT6);


/* ----- local functions ----------------------------------------------	*/


static void bit_omaha_setscl(void *data, int state)
{
	unsigned int tmp;
	
	if (state)
	{
		tmp = __raw_readl(pgdat);
		tmp |= OMAHA_SCL;
		__raw_writel(tmp,pgdat);
	}
	else
	{
		tmp = __raw_readl(pgdat);
		tmp &= ~OMAHA_SCL;
		__raw_writel(tmp,pgdat);
	}
}

static void bit_omaha_setsda(void *data, int state)
{
	unsigned int tmp;
	
	// ensure that sda is an output at the moment
	tmp = __raw_readl(pgcon);
	tmp = tmp | BIT10;
	__raw_writel(tmp,pgcon);
		
	if (state)
	{
		tmp = __raw_readl(pgdat);
		tmp |= OMAHA_SDA;
		__raw_writel(tmp,pgdat);
	}
	else
	{
		tmp = __raw_readl(pgdat);
		tmp &= ~OMAHA_SDA;
		__raw_writel(tmp,pgdat);
	}
} 

static int bit_omaha_getscl(void *data)
{
	if (__raw_readl(pgdat) & OMAHA_SCL)
		return 1;
	else
		return 0;
}

static int bit_omaha_getsda(void *data)
{
	unsigned int tmp;
	
	// ensure that sda is an output at the moment
	tmp = __raw_readl(pgcon);
	tmp = tmp & ~BIT10;
	__raw_writel(tmp,pgcon);
	
	if (__raw_readl(pgdat) & OMAHA_SDA)
		return 1;
	else
		return 0;
}

static int bit_omaha_init(void)
{
	// Have we got some mmapped space?
	if (request_region(base, 0x100, "i2c (omaha bus adapter)") < 0 )
	{
		printk("i2c-omaha.o: requested I/O region (0x%08x) is in use.\n", base);
		return -ENODEV;
	}

	return 0;
}


static int bit_omaha_reg(struct i2c_client *client)
{
	return 0;
}


static int bit_omaha_unreg(struct i2c_client *client)
{
	return 0;
}

static void bit_omaha_inc_use(struct i2c_adapter *adap)
{
	MOD_INC_USE_COUNT;
}

static void bit_omaha_dec_use(struct i2c_adapter *adap)
{
	MOD_DEC_USE_COUNT;
}



/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
static struct i2c_algo_bit_data bit_omaha_data = {
	NULL,
	bit_omaha_setsda,
	bit_omaha_setscl,
	bit_omaha_getsda,
	bit_omaha_getscl,
	10, 10, 20,		/*	waits, timeout */
};

static struct i2c_adapter bit_omaha_ops = {
	"BIT-Type Omaha I2C adapter",
	I2C_HW_B_OMAHA,
	NULL,
	&bit_omaha_data,
	bit_omaha_inc_use,
	bit_omaha_dec_use,
	bit_omaha_reg,
	bit_omaha_unreg,
};     

static int __init i2c_omaha_init (void)
{
	unsigned int tmp;

	printk("i2c-omaha.o: i2c omaha adapter module\n");
	
	if (bit_omaha_init() == 0) {
		if(i2c_bit_add_bus(&bit_omaha_ops) < 0)
		{
			printk("Could not add bus!\n");
			return -ENODEV;
		}
	} else {
		printk("Could not pcf_omaha_init\n");
		return -ENODEV;
	}	
	
	// Program Port G bits to output function
	tmp = __raw_readl(pgcon);
	tmp |= IIC_BITS;	   	
	__raw_writel(tmp,pgcon);
	
	// Ensure SDA and SCL are open-drain
	tmp = __raw_readl(opencr);
	tmp = tmp | OPC_CMD | OPC_DAT;
	__raw_writel(tmp,opencr);

	bit_omaha_setsda((void*)base,1);
	bit_omaha_setscl((void*)base,1);

	// Disable WP
	tmp = __raw_readl(pgdat);
	tmp = tmp & ~IIC_WP;
	__raw_writel(tmp,pgdat);
			       		
	return 0;
}

static void bit_omaha_exit(void)
{
	release_region(base , 2);
}

static void i2c_omaha_exit(void)
{

	i2c_bit_del_bus(&bit_omaha_ops);

	bit_omaha_exit();

}

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("ARM Limited <support@arm.com>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for Omaha");
MODULE_LICENSE("GPL");
			
MODULE_PARM(base, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(clock, "i");
MODULE_PARM(own, "i");
MODULE_PARM(mmapped, "i");
MODULE_PARM(i2c_debug, "i");


module_init(i2c_omaha_init);
module_exit(i2c_omaha_exit);


