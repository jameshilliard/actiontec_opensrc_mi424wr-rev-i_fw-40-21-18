/*
 * ds1307.c
 *
 * Device driver for Dallas Semiconductor's Real Time Controller DS1307.
 *
 * Copyright (C) 2002 Intrinsyc Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/rtc.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>

#include "ds1307.h"

#define DEBUG 0

#if DEBUG
static unsigned int rtc_debug = DEBUG;
#else
#define rtc_debug 0	/* gcc will remove all the debug code for us */
#endif

static unsigned short slave_address = DS1307_I2C_SLAVE_ADDR;

struct i2c_driver ds1307_driver;
struct i2c_client *ds1307_i2c_client = 0;

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { DS1307_I2C_SLAVE_ADDR, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	normal_i2c:		normal_addr,
	normal_i2c_range:	ignore,
	probe:			ignore,
	probe_range:		ignore,
	ignore: 		ignore,
	ignore_range:		ignore,
	force:			ignore,
};

static int ds1307_rtc_ioctl( struct inode *, struct file *, unsigned int, unsigned long);
static int ds1307_rtc_open(struct inode *inode, struct file *file);
static int ds1307_rtc_release(struct inode *inode, struct file *file);

static struct file_operations rtc_fops = {
	owner:		THIS_MODULE,
	ioctl:		ds1307_rtc_ioctl,
	open:		ds1307_rtc_open,
	release:	ds1307_rtc_release,
};

static struct miscdevice ds1307_rtc_miscdev = {
	RTC_MINOR,
	"rtc",
	&rtc_fops
};

static int ds1307_probe(struct i2c_adapter *adap);
static int ds1307_detach(struct i2c_client *client);
static int ds1307_command(struct i2c_client *client, unsigned int cmd, void *arg);

struct i2c_driver ds1307_driver = {
	name:		"DS1307",
	id:		I2C_DRIVERID_DS1307,
	flags:		I2C_DF_NOTIFY,
	attach_adapter: ds1307_probe,
	detach_client:	ds1307_detach,
	command:	ds1307_command
};

static spinlock_t ds1307_rtc_lock = SPIN_LOCK_UNLOCKED;

#define DAT(x) ((unsigned int)((x)->data)) /* keep the control register info */

static int
ds1307_readram( char *buf, int len)
{
	unsigned long	flags;
	unsigned char ad[1] = { 0 };
	int ret;
	struct i2c_msg msgs[2] = {
		{ ds1307_i2c_client->addr  , 0,        1, ad  },
		{ ds1307_i2c_client->addr  , I2C_M_RD, len, buf } };

	spin_lock_irqsave(&ds1307_rtc_lock, flags);
	ret = i2c_transfer(ds1307_i2c_client->adapter, msgs, 2);
	spin_unlock_irqrestore(&ds1307_rtc_lock,flags);

	return ret;
}

static void
ds1307_dumpram( void)
{
	unsigned char buf[DS1307_RAM_SIZE];
	int ret;

	ret = ds1307_readram( buf, DS1307_RAM_SIZE);

	if( ret > 0)
	{
		int i;
		for( i=0; i<DS1307_RAM_SIZE; i++)
		{
			printk ("%02X ", buf[i]);
			if( (i%8) == 7) printk ("\n");
		}
		printk ("\n");
	}
}

static void
ds1307_enable_clock( int enable)
{
	unsigned char buf[2], ad[1] = { 0 };
	struct i2c_msg msgs[2] = {
		{ ds1307_i2c_client->addr	, 0,	    1, ad  },
		{ ds1307_i2c_client->addr	, I2C_M_RD, 1, buf }
	};
	unsigned char ctrl_info;
	int ret;

	if( enable)
		ctrl_info = SQW_ENABLE | RATE_32768HZ;
	else
		ctrl_info = SQW_DISABLE;
	ds1307_command(ds1307_i2c_client, DS1307_SETCTRL, &ctrl_info);

	/* read addr 0 (Clock-Halt bit and second counter */
	ret = i2c_transfer(ds1307_i2c_client->adapter, msgs, 2);

	if( enable)
		buf[1] = buf[0] & ~CLOCK_HALT; /* clear Clock-Halt bit */
	else
		buf[1] = buf[0] | CLOCK_HALT; /* set Clock-Halt bit */
	buf[0] = 0;	/* control register address on DS1307 */

	ret = i2c_master_send(ds1307_i2c_client, (char *)buf, 2);
}

static int
ds1307_attach(struct i2c_adapter *adap, int addr, unsigned short flags,int kind)
{
	struct i2c_client *c;
	unsigned char buf[1], ad[1] = { 7 };
	struct i2c_msg msgs[2] = {
		{ addr	, 0,	    1, ad  },
		{ addr	, I2C_M_RD, 1, buf }
	};
	int ret;

	c = (struct i2c_client *)kmalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return -ENOMEM;

	strcpy(c->name, "DS1307");
	c->id		= ds1307_driver.id;
	c->flags	= 0;
	c->addr 	= addr;
	c->adapter	= adap;
	c->driver	= &ds1307_driver;
	c->data 	= NULL;

	ret = i2c_transfer(c->adapter, msgs, 2);

	if ( ret == 2 )
	{
		DAT(c) = buf[0];
	}
	else
		printk ("ds1307_attach(): i2c_transfer() returned %d.\n",ret);

	ds1307_i2c_client = c;
	ds1307_enable_clock( 1);

	return i2c_attach_client(c);
}

static int
ds1307_probe(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, ds1307_attach);
}

static int
ds1307_detach(struct i2c_client *client)
{
	i2c_detach_client(client);
	ds1307_enable_clock( 0);

	return 0;
}

static void
ds1307_convert_to_time( struct rtc_time *dt, char *buf)
{
	dt->tm_sec = BCD_TO_BIN(buf[0]);
	dt->tm_min = BCD_TO_BIN(buf[1]);

	if ( TWELVE_HOUR_MODE(buf[2]) )
	{
		dt->tm_hour = HOURS_12(buf[2]);
		if (HOURS_AP(buf[2])) /* PM */
		{
			dt->tm_hour += 12;
		}
	}
	else /* 24-hour-mode */
	{
		dt->tm_hour = HOURS_24(buf[2]);
	}

	dt->tm_mday = BCD_TO_BIN(buf[4]);
	/* dt->tm_mon is zero-based */
	dt->tm_mon = BCD_TO_BIN(buf[5]) - 1;
	/* year is 1900 + dt->tm_year */
	dt->tm_year = BCD_TO_BIN(buf[6]) + 100;

	if( rtc_debug > 2)
	{
		printk("ds1307_get_datetime: year = %d\n", dt->tm_year);
		printk("ds1307_get_datetime: mon  = %d\n", dt->tm_mon);
		printk("ds1307_get_datetime: mday = %d\n", dt->tm_mday);
		printk("ds1307_get_datetime: hour = %d\n", dt->tm_hour);
		printk("ds1307_get_datetime: min  = %d\n", dt->tm_min);
		printk("ds1307_get_datetime: sec  = %d\n", dt->tm_sec);
	}
}

static int
ds1307_get_datetime(struct i2c_client *client, struct rtc_time *dt)
{
	unsigned char buf[7], addr[1] = { 0 };
	struct i2c_msg msgs[2] = {
		{ client->addr, 0,	  1, addr },
		{ client->addr, I2C_M_RD, 7, buf  }
	};
	int ret = -EIO;

	memset(buf, 0, sizeof(buf));

	ret = i2c_transfer(client->adapter, msgs, 2);

	if (ret == 2) {
		ds1307_convert_to_time( dt, buf);
		ret = 0;
	}
	else
		printk("ds1307_get_datetime(), i2c_transfer() returned %d\n",ret);

	return ret;
}

static int
ds1307_set_datetime(struct i2c_client *client, struct rtc_time *dt, int datetoo)
{
	unsigned char buf[8];
	int ret, len = 4;

	if( rtc_debug > 2)
	{
		printk("ds1307_set_datetime: tm_year = %d\n", dt->tm_year);
		printk("ds1307_set_datetime: tm_mon  = %d\n", dt->tm_mon);
		printk("ds1307_set_datetime: tm_mday = %d\n", dt->tm_mday);
		printk("ds1307_set_datetime: tm_hour = %d\n", dt->tm_hour);
		printk("ds1307_set_datetime: tm_min  = %d\n", dt->tm_min);
		printk("ds1307_set_datetime: tm_sec  = %d\n", dt->tm_sec);
	}

	buf[0] = 0;	/* register address on DS1307 */
	buf[1] = (BIN_TO_BCD(dt->tm_sec));
	buf[2] = (BIN_TO_BCD(dt->tm_min));
	buf[3] = (BIN_TO_BCD(dt->tm_hour));

	if (datetoo) {
		len = 8;
		/* we skip buf[4] as we don't use day-of-week. */
		buf[5] = (BIN_TO_BCD(dt->tm_mday));
		buf[6] = (BIN_TO_BCD(dt->tm_mon + 1));
		/* The year only ranges from 0-99, we are being passed an offset from 1900,
		 * and the chip calulates leap years based on 2000, thus we adjust by 100.
		 */
		buf[7] = (BIN_TO_BCD(dt->tm_year - 100));
	}
	ret = i2c_master_send(client, (char *)buf, len);
	if (ret == len)
		ret = 0;
	else
		printk("ds1307_set_datetime(), i2c_master_send() returned %d\n",ret);


	return ret;
}

static int
ds1307_get_ctrl(struct i2c_client *client, unsigned char *ctrl)
{
	*ctrl = DAT(client);

	return 0;
}

static int
ds1307_set_ctrl(struct i2c_client *client, unsigned char *cinfo)
{
	unsigned char buf[2];
	int ret;


	buf[0] = 7;	/* control register address on DS1307 */
	buf[1] = *cinfo;
	/* save the control reg info in the client data field so that get_ctrl
	 * function doesn't have to do an I2C transfer to get it.
	 */
	DAT(client) = buf[1];

	ret = i2c_master_send(client, (char *)buf, 2);

	return ret;
}

static int
ds1307_read_mem(struct i2c_client *client, struct rtc_mem *mem)
{
	unsigned char addr[1];
	struct i2c_msg msgs[2] = {
		{ client->addr, 0,	  1, addr },
		{ client->addr, I2C_M_RD, mem->nr, mem->data }
	};

	if ( (mem->loc < DS1307_RAM_ADDR_START) ||
	     ((mem->loc + mem->nr -1) > DS1307_RAM_ADDR_END) )
		return -EINVAL;

	addr[0] = mem->loc;

	return i2c_transfer(client->adapter, msgs, 2) == 2 ? 0 : -EIO;
}

static int
ds1307_write_mem(struct i2c_client *client, struct rtc_mem *mem)
{
	unsigned char addr[1];
	struct i2c_msg msgs[2] = {
		{ client->addr, 0, 1, addr },
		{ client->addr, 0, mem->nr, mem->data }
	};

	if ( (mem->loc < DS1307_RAM_ADDR_START) ||
	     ((mem->loc + mem->nr -1) > DS1307_RAM_ADDR_END) )
		return -EINVAL;

	addr[0] = mem->loc;

	return i2c_transfer(client->adapter, msgs, 2) == 2 ? 0 : -EIO;
}

static int
ds1307_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case DS1307_GETDATETIME:
		return ds1307_get_datetime(client, arg);

	case DS1307_SETTIME:
		return ds1307_set_datetime(client, arg, 0);

	case DS1307_SETDATETIME:
		return ds1307_set_datetime(client, arg, 1);

	case DS1307_GETCTRL:
		return ds1307_get_ctrl(client, arg);

	case DS1307_SETCTRL:
		return ds1307_set_ctrl(client, arg);

	case DS1307_MEM_READ:
		return ds1307_read_mem(client, arg);

	case DS1307_MEM_WRITE:
		return ds1307_write_mem(client, arg);

	default:
		return -EINVAL;
	}
}

static int
ds1307_rtc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int
ds1307_rtc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int
ds1307_rtc_ioctl( struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	unsigned long	flags;
	struct rtc_time wtime;
	int status = 0;

	switch (cmd) {
		default:
		case RTC_UIE_ON:
		case RTC_UIE_OFF:
		case RTC_PIE_ON:
		case RTC_PIE_OFF:
		case RTC_AIE_ON:
		case RTC_AIE_OFF:
		case RTC_ALM_SET:
		case RTC_ALM_READ:
		case RTC_IRQP_READ:
		case RTC_IRQP_SET:
		case RTC_EPOCH_READ:
		case RTC_EPOCH_SET:
		case RTC_WKALM_SET:
		case RTC_WKALM_RD:
			status = -EINVAL;
			break;

		case RTC_RD_TIME:
			spin_lock_irqsave(&ds1307_rtc_lock, flags);
			ds1307_command( ds1307_i2c_client, DS1307_GETDATETIME, &wtime);
			spin_unlock_irqrestore(&ds1307_rtc_lock,flags);

			if( copy_to_user((void *)arg, &wtime, sizeof (struct rtc_time)))
				status = -EFAULT;
			break;

		case RTC_SET_TIME:
			if (!capable(CAP_SYS_TIME))
			{
				status = -EACCES;
				break;
			}

			if (copy_from_user(&wtime, (struct rtc_time *)arg, sizeof(struct rtc_time)) )
			{
				status = -EFAULT;
				break;
			}

			spin_lock_irqsave(&ds1307_rtc_lock, flags);
			ds1307_command( ds1307_i2c_client, DS1307_SETDATETIME, &wtime);
			spin_unlock_irqrestore(&ds1307_rtc_lock,flags);
			break;
	}

	return status;
}

static char *
ds1307_mon2str( unsigned int mon)
{
	char *mon2str[12] = {
	  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	if( mon > 11) return "error";
	else return mon2str[ mon];
}

static int ds1307_rtc_proc_output( char *buf)
{
#define CHECK(ctrl,bit) ((ctrl & bit) ? "yes" : "no")
	unsigned char ram[DS1307_RAM_SIZE];
	int ret;

	char *p = buf;

	ret = ds1307_readram( ram, DS1307_RAM_SIZE);
	if( ret > 0)
	{
		int i;
		struct rtc_time dt;
		char text[9];

		p += sprintf(p, "DS1307 (64x8 Serial Real Time Clock)\n");

		ds1307_convert_to_time( &dt, ram);
		p += sprintf(p, "Date/Time	     : %02d-%s-%04d %02d:%02d:%02d\n",
			dt.tm_mday, ds1307_mon2str(dt.tm_mon), dt.tm_year + 1900,
			dt.tm_hour, dt.tm_min, dt.tm_sec);

		p += sprintf(p, "Clock halted	     : %s\n", CHECK(ram[0],0x80));
		p += sprintf(p, "24h mode	     : %s\n", CHECK(ram[2],0x40));
		p += sprintf(p, "Square wave enabled : %s\n", CHECK(ram[7],0x10));
		p += sprintf(p, "Freq		     : ");

		switch( ram[7] & 0x03)
		{
			case RATE_1HZ:
				p += sprintf(p, "1Hz\n");
				break;
			case RATE_4096HZ:
				p += sprintf(p, "4.096kHz\n");
				break;
			case RATE_8192HZ:
				p += sprintf(p, "8.192kHz\n");
				break;
			case RATE_32768HZ:
			default:
				p += sprintf(p, "32.768kHz\n");
				break;

		}

		p += sprintf(p, "RAM dump:\n");
		text[8]='\0';
		for( i=0; i<DS1307_RAM_SIZE; i++)
		{
			p += sprintf(p, "%02X ", ram[i]);

			if( (ram[i] < 32) || (ram[i]>126)) ram[i]='.';
			text[i%8] = ram[i];
			if( (i%8) == 7) p += sprintf(p, "%s\n",text);
		}
		p += sprintf(p, "\n");
	}
	else
	{
		p += sprintf(p, "Failed to read RTC memory!\n");
	}

	return	p - buf;
}

static int ds1307_rtc_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = ds1307_rtc_proc_output (page);
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static __init int ds1307_init(void)
{
	int retval=0;

	if( slave_address != 0xffff)
	{
		normal_addr[0] = slave_address;
	}

	if( normal_addr[0] == 0xffff)
	{
		printk(KERN_ERR"I2C: Invalid slave address for DS1307 RTC (%#x)\n",
			normal_addr[0]);
		return -EINVAL;
	}

	retval = i2c_add_driver(&ds1307_driver);

	if (retval==0)
	{
		misc_register (&ds1307_rtc_miscdev);
		create_proc_read_entry (PROC_DS1307_NAME, 0, 0, ds1307_rtc_read_proc, NULL);
		printk("I2C: DS1307 RTC driver successfully loaded\n");

		if( rtc_debug) ds1307_dumpram();
	}
	return retval;
}

static __exit void ds1307_exit(void)
{
	remove_proc_entry (PROC_DS1307_NAME, NULL);
	misc_deregister(&ds1307_rtc_miscdev);
	i2c_del_driver(&ds1307_driver);
}

module_init(ds1307_init);
module_exit(ds1307_exit);

MODULE_PARM (slave_address, "i");
MODULE_PARM_DESC (slave_address, "I2C slave address for DS1307 RTC.");

MODULE_AUTHOR ("Intrinsyc Software Inc.");
MODULE_LICENSE("GPL");
