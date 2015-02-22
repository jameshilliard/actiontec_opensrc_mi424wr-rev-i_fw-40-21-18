/*
 * drivers/char/pcf8594c-nvram.c
 *
 * I2C based NVRAM driver for the PCF8594C-2 EEPROM on the IXDP425
 * development platform.  
 *
 * Original Author: Teodor Mihai <teodor.mihai@intel.com>
 *      Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * The NVRAM is used to store MAC addresses for the onboard NPEs and 
 * other HW specific info on the IXDP425, so we want a much nicer/cleaner 
 * way to access the data then having to use the /proc interface from 
 * lm-sensors. This driver can easilly be adapted to other platforms 
 * by changing the I2C bus address we connect to. Note though that it
 * is not meant to be used as a general purpose I2C EEPROM driver.
 *
 *
 *  Copyright (C) 2002-2003 INTEL CORPORATION
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     
 */

#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/init.h> 
#include <asm/uaccess.h> 
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/slab.h>

#ifdef DEBUG
#define DBG printk
#else
#define DBG if (0) printk
#endif /* DEBUG */

#define PCF8594_EEPROM_ADDR (0x50)
#define PCF8594_EEPROM_SIZE (256)	/* 256byte EEPROM */

#define PCF8594_EEPROM_MAX_WRITE   (8)	/* 8 bytes page mode */
#define PCF8594_EEPROM_WRITE_DELAY (100)/* E/W time delay = 100ms */

/* forward declarations */
static int pcf8594_attach_adapter(struct i2c_adapter *adapter);
static int pcf8594_detach_client(struct i2c_client *client);
static void pcf8594_inc_use(struct i2c_client *client);
static void pcf8594_dec_use(struct i2c_client *client);

static struct i2c_driver pcf8594_driver = {
	name:           "PCF8594-2 EEPROM Driver",
	id:             I2C_DRIVERID_EXP0,
	flags:          I2C_DF_NOTIFY,
	attach_adapter: pcf8594_attach_adapter,
	detach_client:  pcf8594_detach_client,
	command:        NULL,
	inc_use:        pcf8594_inc_use,
	dec_use:        pcf8594_dec_use
};

static struct i2c_client *pcf8594_client;

/* Global lock to r/w acces to the device */
static spinlock_t nvram_lock;

/* mode [O_EXCL/FMODE_WRITE] for /dev/nvram */
static int open_mode;

static int pcf8594_write_transfer(int addr, u8 *buf, u32 num, u8 offset)
{
	struct i2c_msg msg[2];
	
	if (pcf8594_client) {
		msg[0].addr  = addr;
		msg[0].flags = 0;
		msg[0].len   = 1;
		msg[0].buf   = &offset;
		
		msg[1].addr  = addr;
		msg[1].flags = 0 | I2C_M_NOSTART;
		msg[1].len   = num;
		msg[1].buf   = buf;
		
		return i2c_transfer(pcf8594_client->adapter, msg, 2);
	}
	else {
		return -ENODEV;
	}
}

static int pcf8594_read_transfer(int addr, u8 *buf, u32 num, u8 offset)
{
	struct i2c_msg msg[2];
	
	if (pcf8594_client) {
		msg[0].addr  = addr;
		msg[0].flags = 0;
		msg[0].len   = 1;
		msg[0].buf   = &offset;
		
		msg[1].addr  = addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len   = num;
		msg[1].buf   = buf;
		
		return i2c_transfer(pcf8594_client->adapter, msg, 2);
	}
	else {
		return -ENODEV;
	}
}

/*
 * Kernel exported APIs.  We provide a set of APIs to the kernel itself
 * as there are drivers for IXP425 systems that require reading data
 * out of the EEPROM.
 */

/**
 *	pcf8594_eeprom_read - Read data from PCF8594C-2 EEPROM device
 *	@buf: Pointer to source buffer
 *	@num: Number of bytes to read
 *	@offset: Offset into EEPROM device at which to start reading
 */
int pcf8594_eeprom_read(u8 *buf, u32 num, u8 offset)
{
	if ((num + offset) > PCF8594_EEPROM_SIZE || buf == NULL) return (-1);

	return pcf8594_read_transfer(PCF8594_EEPROM_ADDR, buf, num, offset);
}


/**
 *	pcf8594_eeprom_write - Write data to PCF8594C-2 EEPROM
 *	@buf: Pointer to destination buffer
 *	@num: Number of bytes to write
 *	@offset: Offset at which to start writing
 */
int pcf8594_eeprom_write(u8 *buf, u32 num, u8 offset)
{
	int temp_count     = 0;
	int current_number = num;

	if ((num + offset) > PCF8594_EEPROM_SIZE || buf == NULL)
		return (-1);

	while (current_number > 0) {
		temp_count = PCF8594_EEPROM_MAX_WRITE > current_number ? current_number : PCF8594_EEPROM_MAX_WRITE;

		pcf8594_write_transfer(PCF8594_EEPROM_ADDR, buf, temp_count, offset);
		
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout((PCF8594_EEPROM_WRITE_DELAY * HZ) / 1000);

		current_number -= temp_count;
		buf            += temp_count;
		offset         += temp_count;
	}

	return(num);
}


static int pcf8594_attach_adapter(struct i2c_adapter *adapter)
{
	DBG("pcf8594c-nvram: attaching to adapter %s\n", adapter->name);

	pcf8594_client = kmalloc(sizeof(*pcf8594_client), GFP_KERNEL);

	if (!pcf8594_client) {
		return -ENOMEM;
	}

	strcpy(pcf8594_client->name, "PCF8594C-2 NVRAM Driver");
	pcf8594_client->id		  = pcf8594_driver.id;
	pcf8594_client->flags	      = I2C_CLIENT_ALLOW_USE;
	pcf8594_client->addr	      = PCF8594_EEPROM_ADDR;
	pcf8594_client->adapter	  = adapter;
	pcf8594_client->driver	  = &pcf8594_driver;
	pcf8594_client->data	      = NULL;
	pcf8594_client->usage_count = 0;

	DBG("pcf8594c-nvram.0: client created, inserting\n");

	return i2c_attach_client(pcf8594_client);
}

static int pcf8594_detach_client(struct i2c_client *client) {

	int result = i2c_detach_client(client);

	if (result == 0) {
		kfree(client);
	
		return 0;
	}
	else {
		return result;
	}

}

static void pcf8594_inc_use(struct i2c_client *client)
{
	MOD_INC_USE_COUNT;
}

static void pcf8594_dec_use(struct i2c_client *client)
{
	MOD_DEC_USE_COUNT;
}

static long long pcf8594c_nvram_llseek(struct file *file, loff_t offset, int from)
{
	if (!pcf8594_client) return -ENODEV;

	switch (from) {
		case 0:
			break;
		case 1:
			offset += file->f_pos;
			break;
		case 2:
			offset += PCF8594_EEPROM_SIZE;
			break;
		default:
			return -EINVAL;
	}

	if (offset < 0  || offset >= PCF8594_EEPROM_SIZE)
		return -EINVAL;

	return file->f_pos = offset;
}

static ssize_t pcf8594c_nvram_read(struct file *file,
		char *buf, size_t count, loff_t *offset)
{
	char buffer[PCF8594_EEPROM_SIZE];

	if (!pcf8594_client) return -ENODEV;

	if (*offset < 0 || *offset >= PCF8594_EEPROM_SIZE)
		return 0;

	if (*offset + count >= PCF8594_EEPROM_SIZE)
		count = PCF8594_EEPROM_SIZE - *offset;

	DBG("nvram: file_read into buffer 0x%08x, offset %d, len %d\n", (int) buf, (int) *offset, count);

	pcf8594_eeprom_read(buffer, count, *offset);

	if (copy_to_user(buf, buffer, count))
		return -EFAULT;

	*offset += count;

	DBG("nvram: read %d bytes\n", count);

	return count;
}

static ssize_t pcf8594c_nvram_write(struct file *file,
		const char *buf, size_t count, loff_t *offset)
{
	char buffer[PCF8594_EEPROM_SIZE];

	if (!pcf8594_client) return -ENODEV;

	if (*offset < 0 || *offset >= PCF8594_EEPROM_SIZE)
		return 0;

	if (*offset + count >= PCF8594_EEPROM_SIZE)
		count = PCF8594_EEPROM_SIZE - *offset;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	DBG("nvram: file_write from buffer 0x%08x, offset %d, len %d\n", (int) buf, (int) *offset, count);

	pcf8594_eeprom_write(buffer, count, *offset);

	*offset += count;

	DBG("nvram: wrote %d bytes\n", count);

	return count;
}

static int pcf8594c_nvram_open( struct inode *inode, struct file *file )
{
	int res = -EBUSY;

	if (!pcf8594_client) return -ENODEV;

	spin_lock(nvram_lock);

	if ((MOD_IN_USE && (file->f_flags & O_EXCL)) ||
		(open_mode & O_EXCL)) {
		goto exit;
	}

	if (file->f_flags & O_EXCL)
		open_mode |= O_EXCL;

	pcf8594_inc_use(pcf8594_client);

	res = 0;

exit:
	spin_unlock(nvram_lock);

	return res;
}

static int pcf8594c_nvram_release(struct inode *inode, struct file *file)
{
	if (!pcf8594_client) return -ENODEV;

	spin_lock_irq(nvram_lock);

	pcf8594_dec_use(pcf8594_client);

	if (file->f_flags & O_EXCL)
		open_mode &= ~O_EXCL;

	spin_unlock_irq(nvram_lock);

	return 0;
}

/*
 * Since we're not a real PC-style "NVRAM", we just return -EINVAL
 * on all IOCTL calls.
 */
static int pcf8594c_nvram_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}

static struct file_operations dev_fops = {
	owner:		THIS_MODULE,

	read:		pcf8594c_nvram_read,
	write:		pcf8594c_nvram_write,

	open:		pcf8594c_nvram_open,
	release:	pcf8594c_nvram_release,

	llseek:		pcf8594c_nvram_llseek,

	ioctl:		pcf8594c_nvram_ioctl
};

static struct miscdevice pcf8594c_nvram_dev = {
	NVRAM_MINOR,
	"PCF8594C2 I2C NVRAM",
	&dev_fops
};


static int __init pcf8594c_nvram_init(void)
{
	int ret;

	if ((ret = i2c_add_driver(&pcf8594_driver)) == 0) {
		ret = misc_register(&pcf8594c_nvram_dev);
				
		if (ret) {
			printk(KERN_ERR  "misc_register failed for PCF8594C2 NVRAM device\n");
			return ret;
		}

		printk(KERN_INFO "Loaded PCF8594C2 I2C EEPROM NVRAM driver\n");
		return 0;
	} else {
		printk(KERN_ERR "failed to register PCF8594C2 I2C EEPROM driver\n");

		return ret;
	}
}

static void __exit pcf8594c_nvram_exit (void)
{
	i2c_del_driver(&pcf8594_driver);
	misc_deregister(&pcf8594c_nvram_dev);
}

module_init(pcf8594c_nvram_init);
module_exit(pcf8594c_nvram_exit);

EXPORT_SYMBOL(pcf8594_eeprom_read);
EXPORT_SYMBOL(pcf8594_eeprom_write);

MODULE_DESCRIPTION("PCF8594C-2 EEPROM /dev/nvram driver");
MODULE_AUTHOR("Teodor Mihai <teodor.mihai@intel.com>");
MODULE_LICENSE("GPL");

