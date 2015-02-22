/*
 * f71805f.c - driver for the Fintek F71805F/FG Super-I/O chip integrated
 *             hardware monitoring features
 * Copyright (C) 2005  Jean Delvare <khali@linux-fr.org>
 *
 * The F71805F/FG is a LPC Super-I/O chip made by Fintek. It integrates
 * complete hardware monitoring features: voltage, fan and temperature
 * sensors, and manual and automatic fan speed control.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <asm/io.h>

static struct platform_device *pdev;

#define DRVNAME "f71805f"

/*
 * Super-I/O constants and functions
 */

#define F71805F_LD_HWM		0x04

#define SIO_REG_LDSEL		0x07	/* Logical device select */
#define SIO_REG_DEVID		0x20	/* Device ID (2 bytes) */
#define SIO_REG_DEVREV		0x22	/* Device revision */
#define SIO_REG_MANID		0x23	/* Fintek ID (2 bytes) */
#define SIO_REG_ENABLE		0x30	/* Logical device enable */
#define SIO_REG_ADDR		0x60	/* Logical device address (2 bytes) */

#define SIO_FINTEK_ID		0x1934
#define SIO_F71805F_ID		0x0406

static inline int
superio_inb(int base, int reg)
{
	outb(reg, base);
	return inb(base + 1);
}

static int
superio_inw(int base, int reg)
{
	int val;
	outb(reg++, base);
	val = inb(base + 1) << 8;
	outb(reg, base);
	val |= inb(base + 1);
	return val;
}

static inline void
superio_select(int base, int ld)
{
	outb(SIO_REG_LDSEL, base);
	outb(ld, base + 1);
}

static inline void
superio_enter(int base)
{
	outb(0x87, base);
	outb(0x87, base);
}

static inline void
superio_exit(int base)
{
	outb(0xaa, base);
}

/*
 * ISA constants
 */

#define REGION_LENGTH		2
#define ADDR_REG_OFFSET		0
#define DATA_REG_OFFSET		1

static struct resource f71805f_resource __initdata = {
	.flags	= IORESOURCE_IO,
};

/*
 * Registers
 */

/* in nr from 0 to 8 (8-bit values) */
#define F71805F_REG_IN(nr)		(0x10 + (nr))
#define F71805F_REG_IN_HIGH(nr)		(0x40 + 2 * (nr))
#define F71805F_REG_IN_LOW(nr)		(0x41 + 2 * (nr))
/* fan nr from 0 to 2 (12-bit values, two registers) */
#define F71805F_REG_FAN(nr)		(0x20 + 2 * (nr))
#define F71805F_REG_FAN_LOW(nr)		(0x28 + 2 * (nr))
#define F71805F_REG_FAN_CTRL(nr)	(0x60 + 16 * (nr))
/* temp nr from 0 to 2 (8-bit values) */
#define F71805F_REG_TEMP(nr)		(0x1B + (nr))
#define F71805F_REG_TEMP_HIGH(nr)	(0x54 + 2 * (nr))
#define F71805F_REG_TEMP_HYST(nr)	(0x55 + 2 * (nr))
#define F71805F_REG_TEMP_MODE		0x01

#define F71805F_REG_START		0x00
/* status nr from 0 to 2 */
#define F71805F_REG_STATUS(nr)		(0x36 + (nr))

/*
 * Data structures and manipulation thereof
 */

struct f71805f_data {
	unsigned short addr;
	const char *name;
	struct semaphore lock;
	struct class_device *class_dev;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */
	unsigned long last_limits;	/* In jiffies */

	/* Register values */
	u8 in[9];
	u8 in_high[9];
	u8 in_low[9];
	u16 fan[3];
	u16 fan_low[3];
	u8 fan_enabled;		/* Read once at init time */
	u8 temp[3];
	u8 temp_high[3];
	u8 temp_hyst[3];
	u8 temp_mode;
	u8 alarms[3];
};

static inline long in_from_reg(u8 reg)
{
	return (reg * 8);
}

/* The 2 least significant bits are not used */
static inline u8 in_to_reg(long val)
{
	if (val <= 0)
		return 0;
	if (val >= 2016)
		return 0xfc;
	return (((val + 16) / 32) << 2);
}

/* in0 is downscaled by a factor 2 internally */
static inline long in0_from_reg(u8 reg)
{
	return (reg * 16);
}

static inline u8 in0_to_reg(long val)
{
	if (val <= 0)
		return 0;
	if (val >= 4032)
		return 0xfc;
	return (((val + 32) / 64) << 2);
}

/* The 4 most significant bits are not used */
static inline long fan_from_reg(u16 reg)
{
	reg &= 0xfff;
	if (!reg || reg == 0xfff)
		return 0;
	return (1500000 / reg);
}

static inline u16 fan_to_reg(long rpm)
{
	/* If the low limit is set below what the chip can measure,
	   store the largest possible 12-bit value in the registers,
	   so that no alarm will ever trigger. */
	if (rpm < 367)
		return 0xfff;
	return (1500000 / rpm);
}

static inline long temp_from_reg(u8 reg)
{
	return (reg * 1000);
}

static inline u8 temp_to_reg(long val)
{
	if (val < 0)
		val = 0;
	else if (val > 1000 * 0xff)
		val = 0xff;
	return ((val + 500) / 1000);
}

/*
 * Device I/O access
 */

static u8 f71805f_read8(struct f71805f_data *data, u8 reg)
{
	u8 val;

	down(&data->lock);
	outb(reg, data->addr + ADDR_REG_OFFSET);
	val = inb(data->addr + DATA_REG_OFFSET);
	up(&data->lock);

	return val;
}

static void f71805f_write8(struct f71805f_data *data, u8 reg, u8 val)
{
	down(&data->lock);
	outb(reg, data->addr + ADDR_REG_OFFSET);
	outb(val, data->addr + DATA_REG_OFFSET);
	up(&data->lock);
}

/* It is important to read the MSB first, because doing so latches the
   value of the LSB, so we are sure both bytes belong to the same value. */
static u16 f71805f_read16(struct f71805f_data *data, u8 reg)
{
	u16 val;

	down(&data->lock);
	outb(reg, data->addr + ADDR_REG_OFFSET);
	val = inb(data->addr + DATA_REG_OFFSET) << 8;
	outb(++reg, data->addr + ADDR_REG_OFFSET);
	val |= inb(data->addr + DATA_REG_OFFSET);
	up(&data->lock);

	return val;
}

static void f71805f_write16(struct f71805f_data *data, u8 reg, u16 val)
{
	down(&data->lock);
	outb(reg, data->addr + ADDR_REG_OFFSET);
	outb(val >> 8, data->addr + DATA_REG_OFFSET);
	outb(++reg, data->addr + ADDR_REG_OFFSET);
	outb(val & 0xff, data->addr + DATA_REG_OFFSET);
	up(&data->lock);
}

static struct f71805f_data *f71805f_update_device(struct device *dev)
{
	struct f71805f_data *data = dev_get_drvdata(dev);
	int nr;

	down(&data->update_lock);

	/* Limit registers cache is refreshed after 60 seconds */
	if (time_after(jiffies, data->last_updated + 60 * HZ)
	 || !data->valid) {
		for (nr = 0; nr < 9; nr++) {
			data->in_high[nr] = f71805f_read8(data,
					    F71805F_REG_IN_HIGH(nr));
			data->in_low[nr] = f71805f_read8(data,
					   F71805F_REG_IN_LOW(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			if (data->fan_enabled & (1 << nr))
				data->fan_low[nr] = f71805f_read16(data,
						    F71805F_REG_FAN_LOW(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			data->temp_high[nr] = f71805f_read8(data,
					      F71805F_REG_TEMP_HIGH(nr));
			data->temp_hyst[nr] = f71805f_read8(data,
					      F71805F_REG_TEMP_HYST(nr));
		}
		data->temp_mode = f71805f_read8(data, F71805F_REG_TEMP_MODE);

		data->last_limits = jiffies;
	}

	/* Measurement registers cache is refreshed after 1 second */
	if (time_after(jiffies, data->last_updated + HZ)
	 || !data->valid) {
		for (nr = 0; nr < 9; nr++) {
			data->in[nr] = f71805f_read8(data,
				       F71805F_REG_IN(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			if (data->fan_enabled & (1 << nr))
				data->fan[nr] = f71805f_read16(data,
						F71805F_REG_FAN(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			data->temp[nr] = f71805f_read8(data,
					 F71805F_REG_TEMP(nr));
		}
		for (nr = 0; nr < 3; nr++) {
			data->alarms[nr] = f71805f_read8(data,
					   F71805F_REG_STATUS(nr));
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);

	return data;
}

/*
 * Sysfs interface
 */

static ssize_t show_in0(struct device *dev, struct device_attribute *devattr,
			char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);

	return sprintf(buf, "%ld\n", in0_from_reg(data->in[0]));
}

static ssize_t show_in0_max(struct device *dev, struct device_attribute
			    *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);

	return sprintf(buf, "%ld\n", in0_from_reg(data->in_high[0]));
}

static ssize_t show_in0_min(struct device *dev, struct device_attribute
			    *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);

	return sprintf(buf, "%ld\n", in0_from_reg(data->in_low[0]));
}

static ssize_t set_in0_max(struct device *dev, struct device_attribute
			   *devattr, const char *buf, size_t count)
{
	struct f71805f_data *data = dev_get_drvdata(dev);
	long val = simple_strtol(buf, NULL, 10);

	down(&data->update_lock);
	data->in_high[0] = in0_to_reg(val);
	f71805f_write8(data, F71805F_REG_IN_HIGH(0), data->in_high[0]);
	up(&data->update_lock);

	return count;
}

static ssize_t set_in0_min(struct device *dev, struct device_attribute
			   *devattr, const char *buf, size_t count)
{
	struct f71805f_data *data = dev_get_drvdata(dev);
	long val = simple_strtol(buf, NULL, 10);

	down(&data->update_lock);
	data->in_low[0] = in0_to_reg(val);
	f71805f_write8(data, F71805F_REG_IN_LOW(0), data->in_low[0]);
	up(&data->update_lock);

	return count;
}

static DEVICE_ATTR(in0_input, S_IRUGO, show_in0, NULL);
static DEVICE_ATTR(in0_max, S_IRUGO| S_IWUSR, show_in0_max, set_in0_max);
static DEVICE_ATTR(in0_min, S_IRUGO| S_IWUSR, show_in0_min, set_in0_min);

static ssize_t show_in(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	return sprintf(buf, "%ld\n", in_from_reg(data->in[nr]));
}

static ssize_t show_in_max(struct device *dev, struct device_attribute
			   *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	return sprintf(buf, "%ld\n", in_from_reg(data->in_high[nr]));
}

static ssize_t show_in_min(struct device *dev, struct device_attribute
			   *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	return sprintf(buf, "%ld\n", in_from_reg(data->in_low[nr]));
}

static ssize_t set_in_max(struct device *dev, struct device_attribute
			  *devattr, const char *buf, size_t count)
{
	struct f71805f_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;
	long val = simple_strtol(buf, NULL, 10);

	down(&data->update_lock);
	data->in_high[nr] = in_to_reg(val);
	f71805f_write8(data, F71805F_REG_IN_HIGH(nr), data->in_high[nr]);
	up(&data->update_lock);

	return count;
}

static ssize_t set_in_min(struct device *dev, struct device_attribute
			  *devattr, const char *buf, size_t count)
{
	struct f71805f_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;
	long val = simple_strtol(buf, NULL, 10);

	down(&data->update_lock);
	data->in_low[nr] = in_to_reg(val);
	f71805f_write8(data, F71805F_REG_IN_LOW(nr), data->in_low[nr]);
	up(&data->update_lock);

	return count;
}

#define sysfs_in(offset)					\
static SENSOR_DEVICE_ATTR(in##offset##_input, S_IRUGO,		\
		show_in, NULL, offset);				\
static SENSOR_DEVICE_ATTR(in##offset##_max, S_IRUGO | S_IWUSR,	\
		show_in_max, set_in_max, offset);		\
static SENSOR_DEVICE_ATTR(in##offset##_min, S_IRUGO | S_IWUSR,	\
		show_in_min, set_in_min, offset)

sysfs_in(1);
sysfs_in(2);
sysfs_in(3);
sysfs_in(4);
sysfs_in(5);
sysfs_in(6);
sysfs_in(7);
sysfs_in(8);

static ssize_t show_fan(struct device *dev, struct device_attribute *devattr,
			char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	return sprintf(buf, "%ld\n", fan_from_reg(data->fan[nr]));
}

static ssize_t show_fan_min(struct device *dev, struct device_attribute
			    *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	return sprintf(buf, "%ld\n", fan_from_reg(data->fan_low[nr]));
}

static ssize_t set_fan_min(struct device *dev, struct device_attribute
			   *devattr, const char *buf, size_t count)
{
	struct f71805f_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;
	long val = simple_strtol(buf, NULL, 10);

	down(&data->update_lock);
	data->fan_low[nr] = fan_to_reg(val);
	f71805f_write16(data, F71805F_REG_FAN_LOW(nr), data->fan_low[nr]);
	up(&data->update_lock);

	return count;
}

#define sysfs_fan(offset)					\
static SENSOR_DEVICE_ATTR(fan##offset##_input, S_IRUGO,		\
		show_fan, NULL, offset - 1);			\
static SENSOR_DEVICE_ATTR(fan##offset##_min, S_IRUGO | S_IWUSR,	\
		show_fan_min, set_fan_min, offset - 1)

sysfs_fan(1);
sysfs_fan(2);
sysfs_fan(3);

static ssize_t show_temp(struct device *dev, struct device_attribute *devattr,
			 char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	return sprintf(buf, "%ld\n", temp_from_reg(data->temp[nr]));
}

static ssize_t show_temp_max(struct device *dev, struct device_attribute
			     *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	return sprintf(buf, "%ld\n", temp_from_reg(data->temp_high[nr]));
}

static ssize_t show_temp_hyst(struct device *dev, struct device_attribute
			      *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	return sprintf(buf, "%ld\n", temp_from_reg(data->temp_hyst[nr]));
}

static ssize_t show_temp_type(struct device *dev, struct device_attribute
			      *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;

	/* 3 is diode, 4 is thermistor */
	return sprintf(buf, "%u\n", (data->temp_mode & (1 << nr)) ? 3 : 4);
}

static ssize_t set_temp_max(struct device *dev, struct device_attribute
			    *devattr, const char *buf, size_t count)
{
	struct f71805f_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;
	long val = simple_strtol(buf, NULL, 10);

	down(&data->update_lock);
	data->temp_high[nr] = temp_to_reg(val);
	f71805f_write8(data, F71805F_REG_TEMP_HIGH(nr), data->temp_high[nr]);
	up(&data->update_lock);

	return count;
}

static ssize_t set_temp_hyst(struct device *dev, struct device_attribute
			     *devattr, const char *buf, size_t count)
{
	struct f71805f_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int nr = attr->index;
	long val = simple_strtol(buf, NULL, 10);

	down(&data->update_lock);
	data->temp_hyst[nr] = temp_to_reg(val);
	f71805f_write8(data, F71805F_REG_TEMP_HYST(nr), data->temp_hyst[nr]);
	up(&data->update_lock);

	return count;
}

#define sysfs_temp(offset)						\
static SENSOR_DEVICE_ATTR(temp##offset##_input, S_IRUGO,		\
		show_temp, NULL, offset - 1);				\
static SENSOR_DEVICE_ATTR(temp##offset##_max, S_IRUGO | S_IWUSR,	\
		show_temp_max, set_temp_max, offset - 1);		\
static SENSOR_DEVICE_ATTR(temp##offset##_max_hyst, S_IRUGO | S_IWUSR,	\
		show_temp_hyst, set_temp_hyst, offset - 1);		\
static SENSOR_DEVICE_ATTR(temp##offset##_type, S_IRUGO,			\
		show_temp_type, NULL, offset - 1)

sysfs_temp(1);
sysfs_temp(2);
sysfs_temp(3);

static ssize_t show_alarms_in(struct device *dev, struct device_attribute
			      *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);

	return sprintf(buf, "%d\n", data->alarms[0] |
				    ((data->alarms[1] & 0x01) << 8));
}

static ssize_t show_alarms_fan(struct device *dev, struct device_attribute
			       *devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);

	return sprintf(buf, "%d\n", data->alarms[2] & 0x07);
}

static ssize_t show_alarms_temp(struct device *dev, struct device_attribute
				*devattr, char *buf)
{
	struct f71805f_data *data = f71805f_update_device(dev);

	return sprintf(buf, "%d\n", (data->alarms[1] >> 3) & 0x07);
}

static DEVICE_ATTR(alarms_in, S_IRUGO, show_alarms_in, NULL);
static DEVICE_ATTR(alarms_fan, S_IRUGO, show_alarms_fan, NULL);
static DEVICE_ATTR(alarms_temp, S_IRUGO, show_alarms_temp, NULL);

static ssize_t show_name(struct device *dev, struct device_attribute
			 *devattr, char *buf)
{
	struct f71805f_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", data->name);
}

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

/*
 * Device registration and initialization
 */

static void __devinit f71805f_init_device(struct f71805f_data *data)
{
	u8 reg;
	int i;

	reg = f71805f_read8(data, F71805F_REG_START);
	if ((reg & 0x41) != 0x01) {
		printk(KERN_DEBUG DRVNAME ": Starting monitoring "
		       "operations\n");
		f71805f_write8(data, F71805F_REG_START, (reg | 0x01) & ~0x40);
	}

	/* Fan monitoring can be disabled. If it is, we won't be polling
	   the register values, and won't create the related sysfs files. */
	for (i = 0; i < 3; i++) {
		reg = f71805f_read8(data, F71805F_REG_FAN_CTRL(i));
		if (!(reg & 0x80))
			data->fan_enabled |= (1 << i);
	}
}

static int __devinit f71805f_probe(struct platform_device *pdev)
{
	struct f71805f_data *data;
	struct resource *res;
	int err;

	if (!(data = kzalloc(sizeof(struct f71805f_data), GFP_KERNEL))) {
		err = -ENOMEM;
		printk(KERN_ERR DRVNAME ": Out of memory\n");
		goto exit;
	}

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	data->addr = res->start;
	init_MUTEX(&data->lock);
	data->name = "f71805f";
	init_MUTEX(&data->update_lock);

	platform_set_drvdata(pdev, data);

	data->class_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->class_dev)) {
		err = PTR_ERR(data->class_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n", err);
		goto exit_free;
	}

	/* Initialize the F71805F chip */
	f71805f_init_device(data);

	/* Register sysfs interface files */
	device_create_file(&pdev->dev, &dev_attr_in0_input);
	device_create_file(&pdev->dev, &dev_attr_in0_max);
	device_create_file(&pdev->dev, &dev_attr_in0_min);
	device_create_file(&pdev->dev, &sensor_dev_attr_in1_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in2_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in3_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in4_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in5_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in6_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in7_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in8_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in1_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in2_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in3_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in4_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in5_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in6_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in7_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in8_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in1_min.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in2_min.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in3_min.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in4_min.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in5_min.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in6_min.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in7_min.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_in8_min.dev_attr);
	if (data->fan_enabled & (1 << 0)) {
		device_create_file(&pdev->dev,
				   &sensor_dev_attr_fan1_input.dev_attr);
		device_create_file(&pdev->dev,
				   &sensor_dev_attr_fan1_min.dev_attr);
	}
	if (data->fan_enabled & (1 << 1)) {
		device_create_file(&pdev->dev,
				   &sensor_dev_attr_fan2_input.dev_attr);
		device_create_file(&pdev->dev,
				   &sensor_dev_attr_fan2_min.dev_attr);
	}
	if (data->fan_enabled & (1 << 2)) {
		device_create_file(&pdev->dev,
				   &sensor_dev_attr_fan3_input.dev_attr);
		device_create_file(&pdev->dev,
				   &sensor_dev_attr_fan3_min.dev_attr);
	}
	device_create_file(&pdev->dev,
			   &sensor_dev_attr_temp1_input.dev_attr);
	device_create_file(&pdev->dev,
			   &sensor_dev_attr_temp2_input.dev_attr);
	device_create_file(&pdev->dev,
			   &sensor_dev_attr_temp3_input.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_temp1_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_temp2_max.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_temp3_max.dev_attr);
	device_create_file(&pdev->dev,
			   &sensor_dev_attr_temp1_max_hyst.dev_attr);
	device_create_file(&pdev->dev,
			   &sensor_dev_attr_temp2_max_hyst.dev_attr);
	device_create_file(&pdev->dev,
			   &sensor_dev_attr_temp3_max_hyst.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_temp1_type.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_temp2_type.dev_attr);
	device_create_file(&pdev->dev, &sensor_dev_attr_temp3_type.dev_attr);
	device_create_file(&pdev->dev, &dev_attr_alarms_in);
	device_create_file(&pdev->dev, &dev_attr_alarms_fan);
	device_create_file(&pdev->dev, &dev_attr_alarms_temp);
	device_create_file(&pdev->dev, &dev_attr_name);

	return 0;

exit_free:
	kfree(data);
exit:
	return err;
}

static int __devexit f71805f_remove(struct platform_device *pdev)
{
	struct f71805f_data *data = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	hwmon_device_unregister(data->class_dev);
	kfree(data);

	return 0;
}

static struct platform_driver f71805f_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= DRVNAME,
	},
	.probe		= f71805f_probe,
	.remove		= __devexit_p(f71805f_remove),
};

static int __init f71805f_device_add(unsigned short address)
{
	int err;

	pdev = platform_device_alloc(DRVNAME, address);
	if (!pdev) {
		err = -ENOMEM;
		printk(KERN_ERR DRVNAME ": Device allocation failed\n");
		goto exit;
	}

	f71805f_resource.start = address;
	f71805f_resource.end = address + REGION_LENGTH - 1;
	f71805f_resource.name = pdev->name;
	err = platform_device_add_resources(pdev, &f71805f_resource, 1);
	if (err) {
		printk(KERN_ERR DRVNAME ": Device resource addition failed "
		       "(%d)\n", err);
		goto exit_device_put;
	}

	err = platform_device_add(pdev);
	if (err) {
		printk(KERN_ERR DRVNAME ": Device addition failed (%d)\n",
		       err);
		goto exit_device_put;
	}

	return 0;

exit_device_put:
	platform_device_put(pdev);
exit:
	return err;
}

static int __init f71805f_find(int sioaddr, unsigned short *address)
{
	int err = -ENODEV;
	u16 devid;

	superio_enter(sioaddr);

	devid = superio_inw(sioaddr, SIO_REG_MANID);
	if (devid != SIO_FINTEK_ID)
		goto exit;

	devid = superio_inw(sioaddr, SIO_REG_DEVID);
	if (devid != SIO_F71805F_ID) {
		printk(KERN_INFO DRVNAME ": Unsupported Fintek device, "
		       "skipping\n");
		goto exit;
	}

	superio_select(sioaddr, F71805F_LD_HWM);
	if (!(superio_inb(sioaddr, SIO_REG_ENABLE) & 0x01)) {
		printk(KERN_WARNING DRVNAME ": Device not activated, "
		       "skipping\n");
		goto exit;
	}

	*address = superio_inw(sioaddr, SIO_REG_ADDR);
	if (*address == 0) {
		printk(KERN_WARNING DRVNAME ": Base address not set, "
		       "skipping\n");
		goto exit;
	}

	err = 0;
	printk(KERN_INFO DRVNAME ": Found F71805F chip at %#x, revision %u\n",
	       *address, superio_inb(sioaddr, SIO_REG_DEVREV));

exit:
	superio_exit(sioaddr);
	return err;
}

static int __init f71805f_init(void)
{
	int err;
	unsigned short address;

	if (f71805f_find(0x2e, &address)
	 && f71805f_find(0x4e, &address))
		return -ENODEV;

	err = platform_driver_register(&f71805f_driver);
	if (err)
		goto exit;

	/* Sets global pdev as a side effect */
	err = f71805f_device_add(address);
	if (err)
		goto exit_driver;

	return 0;

exit_driver:
	platform_driver_unregister(&f71805f_driver);
exit:
	return err;
}

static void __exit f71805f_exit(void)
{
	platform_device_unregister(pdev);
	platform_driver_unregister(&f71805f_driver);
}

MODULE_AUTHOR("Jean Delvare <khali@linux-fr>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("F71805F hardware monitoring driver");

module_init(f71805f_init);
module_exit(f71805f_exit);
