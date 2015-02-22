/*
 * drivers/i2c/i2c-ixdp2400.c
 *
 * I2C adapter for IXP2000 systems with IXDP2400 style I2C bus
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 * Based on code by: Naeem M. Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright 2003 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 * I2C adapter driver for IXDP2400 reference platform. This could
 * be a generic IXP2000 driver, but the IXDP2400 uses the HW GPIO
 * in a very board specific manner to create an I2C bus. If you copy
 * the exact same setup, you can use this driver for your board.
 *
 * GPIO2 is pulled high and connected to both I2C lines.
 *
 * GPIO6 and GPIO7 are used for SDA and SCL respectively and
 * are both set to 0.
 *
 * To drive a line high on the bus, that that GPIO is set to an output.
 * This causes the GPIO pin to float and so the bus sees a logic 1.
 *
 * To drive a line low on the bus, that GPIO is set to an input
 * and we pull down the line.
 *
 * (This is my understanding of it.  I'm not a HW person and don't have
 *  the schematics in front of me, but this seems to make sense from
 *  the original Intel driver).
 */

#include <linux/version.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>

/*
 * In case other boards use the same setup, this will make it
 * easier to support multiple boards without tons of 
 * #ifdef's or machine_is_xxx() calls. Just fill in the ixdp2400_gpio
 * with the appropriate GPIO information.
 */
static struct ixdp2400_data {
	unsigned short sda_gpio;
	unsigned short scl_gpio;
} ixdp2400_gpio;

static void ixdp2400_bit_setscl(void *data, int val)
{
	struct ixdp2400_data *gpio = (struct ixdp2400_data*)data; 
	int i = 5000;

	if(val) {
		gpio_line_config(gpio->scl_gpio, GPIO_IN);
		while(!gpio_line_get(gpio->scl_gpio) && i--);
	} else {
		gpio_line_config(gpio->scl_gpio, GPIO_OUT);
	}
}

static void ixdp2400_bit_setsda(void *data, int val)
{
	struct ixdp2400_data *gpio = (struct ixdp2400_data*)data;

	if(val) {
		gpio_line_config(gpio->sda_gpio, GPIO_IN);
	} else {
		gpio_line_config(gpio->sda_gpio, GPIO_OUT);
	}

//	printk("SDA %d PDPR = %#010x\n", val, *IXP2000_GPIO_PDPR);
}

static int ixdp2400_bit_getscl(void *data)
{
	struct ixdp2400_data *gpio = (struct ixdp2400_data*)data;
	int ret;

	ret = gpio_line_get(gpio->scl_gpio);

	return ret;
}

static int ixdp2400_bit_getsda(void *data)
{
	struct ixdp2400_data *gpio = (struct ixdp2400_data*)data;
	int ret;

	ret = gpio_line_get(gpio->sda_gpio);

	return ret;
}

void ixdp2400_i2c_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

void ixdp2400_i2c_dec(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

/*
 * If you board provides multiple I2C channels, just
 * make copies of this and change the gpio information.
 */
static struct i2c_algo_bit_data ixdp2400_bit_data = {
	.data =		&ixdp2400_gpio,
	.setsda =	ixdp2400_bit_setsda,
	.setscl =	ixdp2400_bit_setscl,
	.getsda =	ixdp2400_bit_getsda,
	.getscl =	ixdp2400_bit_getscl,
	.udelay = 	6,
	.mdelay =	6,
	.timeout = 	100
};

static struct i2c_adapter ixdp2400_i2c_adapter = {
	.name =		"IXDP2400-style GPIO I2C Adapter",
	.id =		I2C_HW_B_IXDP2400,
	.algo =		NULL,
	.algo_data =	&ixdp2400_bit_data,
	.inc_use =	ixdp2400_i2c_inc,
	.dec_use = 	ixdp2400_i2c_dec
};

static int __init ixdp2400_i2c_init(void)
{
	if(machine_is_ixdp2400()) {
		int i = 0;

		/*
		 * GPIO2 needs to be high for SDA to work on this board
		 */
		gpio_line_set(IXDP2400_GPIO_HIGH, 1);
		gpio_line_config(IXDP2400_GPIO_HIGH, GPIO_OUT);

		ixdp2400_gpio.sda_gpio = IXDP2400_GPIO_SCL;
		ixdp2400_gpio.scl_gpio = IXDP2400_GPIO_SDA;

		gpio_line_config(ixdp2400_gpio.sda_gpio, GPIO_OUT);
		gpio_line_config(ixdp2400_gpio.scl_gpio, GPIO_OUT);

		gpio_line_set(ixdp2400_gpio.scl_gpio, 0);
		gpio_line_set(ixdp2400_gpio.sda_gpio, 0);

		gpio_line_config(ixdp2400_gpio.scl_gpio, GPIO_IN);

		for(i = 0; i < 10; i++);

		gpio_line_config(ixdp2400_gpio.sda_gpio, GPIO_IN);
	}

	if (i2c_bit_add_bus(&ixdp2400_i2c_adapter)) {
		printk("i2c-ixdp2400: I2C adapter registration failed\n");
		return -EIO;
	} else printk("i2c-ixdp2400: I2C bus initialized\n");

	return 0;
}

static void __exit ixdp2400_i2c_exit(void)
{
	i2c_bit_del_bus(&ixdp2400_i2c_adapter);
}

EXPORT_NO_SYMBOLS;
MODULE_AUTHOR ("Deepak Saxena <dsaxena@mvista.com>");
MODULE_DESCRIPTION("IXDP2400 I2C bus driver");
MODULE_LICENSE("GPL");

module_init(ixdp2400_i2c_init);
module_exit(ixdp2400_i2c_exit);
