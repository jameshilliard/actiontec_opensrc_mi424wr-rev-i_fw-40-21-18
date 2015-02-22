/************************************************************************************\
Copyright	: Copyright (C) 1995-2000 Simon G. Vogl
		  Copyright 2002 IDERs Incorporated
File Name	: i2c-guide.c
Description	: this i2c driver uses the GPIO port B pin 0 and pin 1 on the cs89712.
Notes		: To change the bit rate, change the structure i2c_algo_bit_data
                : to 10 10 100
Contact		: tsong@iders.ca
License		: This source code is free software; you can redistribute it and/or
		  modify it under the terms of the GNU General Public License
		  version 2 as published by the Free Software Foundation.
\************************************************************************************/

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>  /* for 2.0 kernels to get NULL   */
#include <asm/errno.h>     /* for 2.0 kernels to get ENODEV */
#include <asm/io.h>

#include <asm/hardware/cs89712.h>   // io operation ep_writel()
#include <asm/hardware/clps7111.h>   // io operation clps_writel()
#include <asm/arch-clps711x/hardware.h>   // io operation clps_writel()

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

/* ----- global defines -----------------------------------------------	*/

#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEB2(x) 	/* low level debugging - very slow 		*/
#define DEBE(x)	x	/* error messages 				*/
					/*  Pin Port  Inverted	name	*/
#define I2C_SDA		0x08		/*  port B ctrl pin 3  	(inv)	*/
#define I2C_SCL		0x04		/*  port B ctrl pin 2 	(inv)	*/

#define I2C_SDAIN	0x08		/*  use the same pin with output	*/
#define I2C_SCLIN	0x04		/*  use the same pin with output        */

#define I2C_DMASK	0xf7            /* inverse of I2C_SDA  */
#define I2C_CMASK	0xfb            /* inverse of I2c_SCL  */

#define PORTB_PIN0_SDA_OUTPUT 0x08               /* pin 3 direction  of port B output */
#define PORTB_PIN0_SDA_INPUT  0xf7               /* pin 3 direction of port B input  */

#define PORTB_PIN1_SCL_OUTPUT 0x04               /* pin 2 direction of port B output  */
#define PORTB_PIN1_SCL_INPUT  0xfb               /* pin 2 direction of port B input  */

int base = 0;
#define DEFAULT_BASE PBDR

/* ----- local functions --------------------------------------------------- */

static void bit_guide_setscl(void* data, int state)
{
	if (state) {
		// set port B pin2 input
		clps_writeb((clps_readb(PBDDR)) & PORTB_PIN1_SCL_INPUT, PBDDR);
	}
	else {
		// clear
		clps_writeb((clps_readb(PBDR)) & I2C_CMASK,  PBDR);
		// set port B pin2 output
		clps_writeb((clps_readb(PBDDR)) | PORTB_PIN1_SCL_OUTPUT, PBDDR);
	}
}

static void bit_guide_setsda(void* data, int state)
{
	if (state) {
		clps_writeb((clps_readb(PBDDR)) & PORTB_PIN0_SDA_INPUT, PBDDR);
		// float pin 0 (actually  drive high by pull up resistor)
		// clps_writeb((clps_readb(PBDR)) | I2C_SDA, PBDR);   // set Jan4 ori: eff
		// printk("set sda high, state=%i\n",state);
	}
	else {
		// clear
		clps_writeb((clps_readb(PBDR)) & I2C_DMASK,  PBDR);
		// set port B pin 0 output
		clps_writeb((clps_readb(PBDDR)) | PORTB_PIN0_SDA_OUTPUT, PBDDR);
	}
}

static int bit_guide_getscl(void *data)
{
	return ( 0 != ( (clps_readb(PBDR)) & I2C_SCLIN  ) );
}

static int bit_guide_getsda(void *data)
{
	// set port B pin 0 input Jan4 ori eff
	clps_writeb((clps_readb(PBDDR)) & PORTB_PIN0_SDA_INPUT, PBDDR);
	return ( 0 != ( (clps_readb(PBDR) ) & I2C_SDAIN ) );
}

static int bit_guide_init(void)
{
	bit_guide_setsda((void*)base,1);
	bit_guide_setscl((void*)base,1);
	return 0;
}

static int bit_guide_reg(struct i2c_client *client)
{
	return 0;
}

static int bit_guide_unreg(struct i2c_client *client)
{
	return 0;
}

static void bit_guide_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void bit_guide_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */

/* last line (us, ms, timout)
 * us dominates the bit rate: 10us  means: 100Kbit/sec(25 means 40kbps)
 *                            10ms  not known
 *                            100ms timeout
 */
static struct i2c_algo_bit_data bit_guide_data = {
	NULL,
	bit_guide_setsda,
	bit_guide_setscl,
	bit_guide_getsda,
	bit_guide_getscl,
	50, 10, 100,	/* orginal (non-guide) value 10, 10, 100  */
};

static struct i2c_adapter bit_guide_ops = {
	"Guide Port B: PIN2-SCL/PIN3-SDA",
	I2C_HW_B_GUIDE,
	NULL,
	&bit_guide_data,
	bit_guide_inc_use,
	bit_guide_dec_use,
	bit_guide_reg,
	bit_guide_unreg,
};

static int __init  i2c_bitguide_init(void)
{
	printk("i2c-guide.o: Guide i2c port B adapter module.\n");
	clps_writeb((clps_readb(PBDDR)) & 0xfd, PBDDR);  //  set service reuest pb1 as input
	if (base==0) {
		/* probe some values */
		base=DEFAULT_BASE;
		bit_guide_data.data=(void*)DEFAULT_BASE;
		if (bit_guide_init()==0) {
			if(i2c_bit_add_bus(&bit_guide_ops) < 0)
				return -ENODEV;
		} else {
			return -ENODEV;
		}
	} else {
		bit_guide_data.data=(void*)base;
		if (bit_guide_init()==0) {
			if(i2c_bit_add_bus(&bit_guide_ops) < 0)
				return -ENODEV;
		} else {
			return -ENODEV;
		}
	}
	printk("i2c-guide.o: found device at %#x.\n",base);
	return 0;
}

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("T. C. Song  <tsong@iders.ca>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for Guide (cs89712) GPIO port B");
MODULE_LICENSE("GPL");

MODULE_PARM(base, "i");

module_init(i2c_bitguide_init);
/* for completeness, we should have a module_exit() function, but the
   GUIDE requires this to always be loaded.  If it is unloaded, the
   operation of the GUIDE is undefined.
   Nobody has written the i2c_bitguide_exit() routine yet, so it is not included.
module_exit(i2c_bitguide_exit);
*/
