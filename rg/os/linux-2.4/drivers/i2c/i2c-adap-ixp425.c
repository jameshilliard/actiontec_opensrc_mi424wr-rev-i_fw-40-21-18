/*
 * drivers/i2c/i2c-adap-ixp425.c
 *
 * Driver for gpio-based i2c adapter on IXP425 systems.
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Based on original Intel driver by Teodor Mihai <teodor.mihai@intel.com>
 *
 * TODO: fix GPIO interface so that _get just returns value instead
 *       of taking ptr
 *
 * Copyright 2003 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-id.h>

#include <asm/hardware.h>
#include <asm/mach-types.h>

static struct ixp425_i2c_data {
	unsigned int sda_line;
	unsigned int scl_line;
} gpio_data;

static inline int ixp425_scl_line(void *data)
{
	return ((struct ixp425_i2c_data*)data)->scl_line;
}

static inline int ixp425_sda_line(void *data)
{
	return ((struct ixp425_i2c_data*)data)->sda_line;
}

static void ixdp425_bit_setscl(void *data, int val)
{
        gpio_line_config(ixp425_scl_line(data), IXP425_GPIO_OUT);
	gpio_line_set(ixp425_scl_line(data), val ? 1 : 0);
}

static void ixdp425_bit_setsda(void *data, int val)
{
        gpio_line_config(ixp425_sda_line(data), IXP425_GPIO_OUT);
	gpio_line_set(ixp425_sda_line(data), val ? 1 : 0);
}

static int ixdp425_bit_getscl(void *data)
{
	int scl;

        gpio_line_config(ixp425_scl_line(data), IXP425_GPIO_IN);
	gpio_line_get(ixp425_scl_line(data), &scl);

	return scl;
}	

static int ixdp425_bit_getsda(void *data)
{
	int sda;

        gpio_line_config(ixp425_sda_line(data), IXP425_GPIO_IN);
	gpio_line_get(ixp425_sda_line(data), &sda);

	return sda;
}	

static void ixp425_i2c_inc_use(struct i2c_adapter *adap)
{
	MOD_INC_USE_COUNT;
}

static void ixp425_i2c_dec_use(struct i2c_adapter *adap)
{
	MOD_DEC_USE_COUNT;
}

struct i2c_algo_bit_data ixdp425_bit_data = {
	.data		= &gpio_data,
	.setsda		= ixdp425_bit_setsda,
	.setscl		= ixdp425_bit_setscl,
	.getsda		= ixdp425_bit_getsda,
	.getscl		= ixdp425_bit_getscl,
	.udelay		= 50,
	.mdelay		= 50,
	.timeout	= 500
};

struct i2c_adapter ixp425_i2c_adapter = {
	.name 		= "IXP425 I2C Adapter",
	.id		= I2C_HW_B_IXP425,
	.algo_data	= &ixdp425_bit_data,
	.inc_use	= ixp425_i2c_inc_use,
	.dec_use	= ixp425_i2c_dec_use,
};

int __init ixp425_i2c_init(void)
{
	int res;

	if(machine_is_ixdp425()) {
		gpio_data.scl_line = IXP425_GPIO_PIN_6;
		gpio_data.sda_line = IXP425_GPIO_PIN_7;

		gpio_line_config(IXP425_GPIO_PIN_6, IXP425_GPIO_OUT);
		gpio_line_config(IXP425_GPIO_PIN_7, IXP425_GPIO_OUT);
		gpio_line_set(IXP425_GPIO_PIN_6, 1);
		gpio_line_set(IXP425_GPIO_PIN_7, 1);
	} else if(machine_is_bruce()) {
		gpio_data.scl_line = IXP425_GPIO_PIN_13;
		gpio_data.sda_line = IXP425_GPIO_PIN_12;

		gpio_line_config(IXP425_GPIO_PIN_13, IXP425_GPIO_OUT);
		gpio_line_config(IXP425_GPIO_PIN_12, IXP425_GPIO_OUT);
		gpio_line_set(IXP425_GPIO_PIN_13, 1);
		gpio_line_set(IXP425_GPIO_PIN_12, 1);
	} else {
		printk(KERN_WARNING 
			"Unknown IXP425 platform: No I2C support available\n");
		return -EIO;
	}

	if ((res = i2c_bit_add_bus(&ixp425_i2c_adapter) != 0)) {
		printk(KERN_ERR "ERROR: Could not install IXP425 I2C adapter\n");
		return res;
	}
	
	return 0;
}

void __exit ixp425_i2c_exit(void)
{
	i2c_bit_del_bus(&ixp425_i2c_adapter);
}

module_init(ixp425_i2c_init);
module_exit(ixp425_i2c_exit);

MODULE_DESCRIPTION("GPIO-based I2C adapter for IXP425 systems");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Saxena<dsaxena@mvista.com>");

