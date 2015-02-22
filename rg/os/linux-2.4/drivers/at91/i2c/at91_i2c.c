/*
    i2c Support for Atmel's AT91RM9200 Two-Wire Interface

    (c) Rick Bronson

    Borrowed heavily from original work by:
    Copyright (c) 2000 Philip Edelbrock <phil@stimpy.netroedge.com>

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>

#include <asm/arch/AT91RM9200_TWI.h>
#include <asm/arch/pio.h>
#include "at91_i2c.h"

#define DBG(x...) do {\
	if (debug > 0) \
		printk(KERN_DEBUG "i2c:" x); \
	} while(0)

int debug = 0;

static struct at91_i2c_local *at91_i2c_device;

/*
 * SMBUS-type transfer entrypoint
 */
static s32 at91_smbus_xfer(struct i2c_adapter *adap,
		u16 addr,
		unsigned short flags,
		char read_write,
		u8 command, int size, union i2c_smbus_data *data)
{
#if 0
	struct at91_i2c_local *device = (struct at91_i2c_local *) adap->data;
	int len;
	u8 *buffer;
	u16 cur_word;
	int rc = 0;

	/* Prepare datas & select mode */
	switch (size) {
	case I2C_SMBUS_QUICK:
		len = 0;
		buffer = NULL;
		break;
	case I2C_SMBUS_BYTE:
		len = 1;
		buffer = &data->byte;
		break;
	case I2C_SMBUS_BYTE_DATA:
		len = 1;
		buffer = &data->byte;
		break;
	case I2C_SMBUS_WORD_DATA:
		len = 2;
		cur_word = cpu_to_le16(data->word);
		buffer = (u8 *) & cur_word;
		break;
	case I2C_SMBUS_BLOCK_DATA:
		len = data->block[0];
		buffer = &data->block[1];
		break;
	default:
		return -1;
	}

	/* Original driver had this limitation */
	if (len > 32)
		len = 32;

	DBG("chan: %d, addr: 0x%x, transfer len: %d, read: %d\n",
	    device->chan_no, addr, len, read_write == I2C_SMBUS_READ);

	/* Set up address and r/w bit */
	write_reg(reg_addr, (addr << 1) | ((read_write == I2C_SMBUS_READ) ? 0x01 : 0x00));

	/* Set up the sub address */

	/* Start sending address & enable interrupt */
	write_reg(reg_control, read_reg(reg_control) | KW_I2C_CTL_XADDR);
	write_reg(reg_ier, KW_I2C_IRQ_MASK);

	DBG("transfer done\n");

	if (size == I2C_SMBUS_WORD_DATA && read_write == I2C_SMBUS_READ)
		data->word = le16_to_cpu(cur_word);
#endif
	return 0;
}

/*
 * Generic i2c master transfer entrypoint
 */
static int at91_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	struct at91_i2c_local *device = (struct at91_i2c_local *) adap->data;
	AT91PS_TWI twi = (AT91PS_TWI) device->base_addr;

	struct i2c_msg *pmsg;
	int completed = 0, length;
	unsigned char *buf;

	pmsg = msgs;		/* look at 1st message, it contains the address/command */
	if (num == 2 && pmsg->len >= 1 && pmsg->len <= 3) {
		DBG("xfer: doing %s %d bytes to 0x%02x - %d messages\n",
		    pmsg->flags & I2C_M_RD ? "read" : "write",
		    pmsg->len, pmsg->buf[0], num);

		/* Set the TWI Master Mode Register */
		twi->TWI_MMR = (pmsg->addr << 16) | (pmsg->len << 8)
			| ((pmsg + 1)->flags & I2C_M_RD ? AT91C_TWI_MREAD : 0);

		/* Set TWI Internal Address Register with first messages data field */
		if (pmsg->len == 1)
			twi->TWI_IADR = pmsg->buf[0];
		else if (pmsg->len == 2)
			twi->TWI_IADR = pmsg->buf[0] << 8 | pmsg->buf[1];
		else			/* must be 3 */
			twi->TWI_IADR =  pmsg->buf[0] << 16 | pmsg->buf[1] << 8 | pmsg->buf[2];

		pmsg++;		/* go to real message */
		length = pmsg->len;
		buf = pmsg->buf;
		if (length && buf) {	/* sanity check */
			if (pmsg->flags & I2C_M_RD) {
				twi->TWI_CR = AT91C_TWI_START;
				while (length--) {
					if (!length)
						twi->TWI_CR = AT91C_TWI_STOP;
					/* Wait until transfer is finished */
					do
						udelay(10);
					while (!(twi->TWI_SR & AT91C_TWI_RXRDY));
					*buf++ = twi->TWI_RHR;
				}
				do {
					udelay(10);
				} while (!(twi->TWI_SR & AT91C_TWI_TXCOMP));
			} else {
				twi->TWI_CR = AT91C_TWI_START;
				while (length--) {
					twi->TWI_THR = *buf++;
					if (!length)
						twi->TWI_CR = AT91C_TWI_STOP;
					do {
						udelay(10);
					} while (!(twi->TWI_SR & AT91C_TWI_TXRDY));
				}
				/* Wait until transfer is finished */
				do {
					udelay(10);
				} while (!(twi->TWI_SR & AT91C_TWI_TXCOMP));
			}
		}
	}
	DBG("transfer done\n");
	return completed++;
}

/*
 * Return list of supported functionality
 */
static u32 at91_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE
		| I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA
		| I2C_FUNC_SMBUS_BLOCK_DATA;
}

/*
 * Open
 */
static void at91_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
	AT91_SYS->PMC_PCER = 1 << AT91C_ID_TWI;	/* enable peripheral clock */
}

/*
 * Close
 */
static void at91_dec(struct i2c_adapter *adapter)
{
	AT91_SYS->PMC_PCDR = 1 << AT91C_ID_TWI;	/* disable peripheral clock */
	MOD_DEC_USE_COUNT;
}

/* For now, we only handle combined mode (smbus) */
static struct i2c_algorithm at91_algorithm = {
	name:"at91 i2c",
	id:I2C_ALGO_SMBUS,
	smbus_xfer:at91_smbus_xfer,
	master_xfer:at91_xfer,
	functionality:at91_func,
};

/*
 * Main initialization routine
 */
static int __init i2c_at91_init(void)
{
	AT91PS_TWI twi = (AT91PS_TWI) AT91C_VA_BASE_TWI;
	struct at91_i2c_local *device;
	int rc;

	AT91_CfgPIO_TWI();

	twi->TWI_IDR = 0x3ff;				/* Disable all interrupts */
	twi->TWI_CR = AT91C_TWI_SWRST;			/* Reset peripheral */
	twi->TWI_CR = AT91C_TWI_MSEN | AT91C_TWI_SVDIS;	/* Set Master mode */

	/* Here, CKDIV = 1 and CHDIV=CLDIV  ==> CLDIV = CHDIV = 1/4*((Fmclk/FTWI) -6) */
	twi->TWI_CWGR = AT91C_TWI_CKDIV1 | AT91C_TWI_CLDIV3 | (AT91C_TWI_CLDIV3 << 8);

	device = (struct at91_i2c_local *) kmalloc(sizeof(struct at91_i2c_local), GFP_KERNEL);
	if (device == NULL) {
		printk(KERN_ERR "at91_i2c: can't allocate inteface!\n");
		return -ENOMEM;
	}
	memset(device, 0, sizeof(struct at91_i2c_local));
	at91_i2c_device = device;

	sprintf(device->adapter.name, "AT91RM9200");
	device->adapter.data = (void *) device;
	device->adapter.id = I2C_ALGO_SMBUS;
	device->adapter.algo = &at91_algorithm;
	device->adapter.algo_data = NULL;
	device->adapter.inc_use = at91_inc;
	device->adapter.dec_use = at91_dec;
	device->adapter.client_register = NULL;
	device->adapter.client_unregister = NULL;
	device->base_addr = AT91C_VA_BASE_TWI;

	rc = i2c_add_adapter(&device->adapter);
	if (rc) {
		printk(KERN_ERR "at91_i2c: Adapter %s registration failed\n", device->adapter.name);
		device->adapter.data = NULL;
		kfree(device);
	}
	else
		printk(KERN_INFO "Found AT91 i2c\n");
	return rc;
}

/*
 * Clean up routine
 */
static void __exit i2c_at91_cleanup(void)
{
	struct at91_i2c_local *device = at91_i2c_device;
	int rc;

	rc = i2c_del_adapter(&device->adapter);
	device->adapter.data = NULL;
	kfree(device);

	/* We aren't that prepared to deal with this... */
	if (rc)
		printk(KERN_ERR "at91_i2c: i2c_del_adapter failed (%i), that's bad!\n", rc);
}

module_init(i2c_at91_init);
module_exit(i2c_at91_cleanup);

MODULE_AUTHOR("Rick Bronson");
MODULE_DESCRIPTION("I2C driver for Atmel AT91RM9200");
MODULE_LICENSE("GPL");
MODULE_PARM(debug, "i");

EXPORT_NO_SYMBOLS;
