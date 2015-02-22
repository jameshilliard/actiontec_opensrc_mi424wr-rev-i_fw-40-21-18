/*
 * drivers/i2c/i2c-adap-ixp2000.c
 *
 * I2C adapter for IXP2000 systems using GPIOs for I2C bus
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 * Based on code by: Naeem M. Afzal <naeem.m.afzal@intel.com>
 * Made generic by: Jeff Daly <jeffrey.daly@intel.com>
 *
 * Copyright (c) 2003 MontaVista Software Inc.
 *
 * This file is licensed under  the terms of the GNU General Public 
 * License version 2. This program is licensed "as is" without any 
 * warranty of any kind, whether express or implied.
 *
 * I2C adapter driver for IXP2000 platform. This should 
 * be a generic IXP2000 driver, if you use the HW GPIO in the same manner.
 * Basically, SDA and SCL GPIOs have external pullups.  Setting the respective
 * GPIO to an input will make the signal a '1' via the pullup.  Setting them
 * to outputs will pull them down.
 *
 * The GPIOs are open drain signals and are used as configuration strap inputs
 * during power-up so there's generally a buffer on the board that needs to be 
 * 'enabled' to drive the GPIOs.
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
 * Init code fills this in at run time with board-specific data.
 */
static struct ixp2000_data {
	unsigned short sda_gpio;
	unsigned short scl_gpio;
} ixp2000_gpio;

static void ixp2000_bit_setscl(void *data, int val)
{
	struct ixp2000_data *gpio = (struct ixp2000_data*)data; 
	int i = 5000;

	if(val) {
		gpio_line_config(gpio->scl_gpio, GPIO_IN);
		while(!gpio_line_get(gpio->scl_gpio) && i--);
	} else {
		gpio_line_config(gpio->scl_gpio, GPIO_OUT);
	}
}

static void ixp2000_bit_setsda(void *data, int val)
{
	struct ixp2000_data *gpio = (struct ixp2000_data*)data;

	if(val) {
		gpio_line_config(gpio->sda_gpio, GPIO_IN);
	} else {
		gpio_line_config(gpio->sda_gpio, GPIO_OUT);
	}
}

static int ixp2000_bit_getscl(void *data)
{
	struct ixp2000_data *gpio = (struct ixp2000_data*)data;
	int ret;

	ret = gpio_line_get(gpio->scl_gpio);

	return ret;
}

static int ixp2000_bit_getsda(void *data)
{
	struct ixp2000_data *gpio = (struct ixp2000_data*)data;
	int ret;

	ret = gpio_line_get(gpio->sda_gpio);

	return ret;
}

void ixp2000_i2c_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

void ixp2000_i2c_dec(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

/*
 * If you board provides multiple I2C channels, just
 * make copies of this and change the gpio information.
 */
static struct i2c_algo_bit_data ixp2000_bit_data = {
	.data =		&ixp2000_gpio,
	.setsda =	ixp2000_bit_setsda,
	.setscl =	ixp2000_bit_setscl,
	.getsda =	ixp2000_bit_getsda,
	.getscl =	ixp2000_bit_getscl,
	.udelay = 	6,
	.mdelay =	6,
	.timeout = 	100
};

static struct i2c_adapter ixp2000_i2c_adapter = {
	.name =		"IXP2000-style GPIO I2C Adapter",
	.id =		I2C_HW_B_IXP2000,
	.algo =		NULL,
	.algo_data =	&ixp2000_bit_data,
	.inc_use =	ixp2000_i2c_inc,
	.dec_use = 	ixp2000_i2c_dec
};

static int __init ixp2000_i2c_init(void)
{
	int i = 0;

#ifdef CONFIG_ARCH_IXDP2400
	if(machine_is_ixdp2400()) {

		gpio_line_set(IXDP2400_GPIO_I2C_ENABLE, 1);
		gpio_line_config(IXDP2400_GPIO_I2C_ENABLE, GPIO_OUT);

		ixp2000_gpio.sda_gpio = IXDP2400_GPIO_SDA;
		ixp2000_gpio.scl_gpio = IXDP2400_GPIO_SCL;
	}
#endif
#ifdef CONFIG_ARCH_IXDP2800
	if(machine_is_ixdp2800()) {

		gpio_line_set(IXDP2800_GPIO_I2C_ENABLE, 1);
		gpio_line_config(IXDP2800_GPIO_I2C_ENABLE, GPIO_OUT);

		ixp2000_gpio.sda_gpio = IXDP2800_GPIO_SDA;
		ixp2000_gpio.scl_gpio = IXDP2800_GPIO_SCL;
	}
#endif

	gpio_line_config(ixp2000_gpio.sda_gpio, GPIO_OUT);
	gpio_line_config(ixp2000_gpio.scl_gpio, GPIO_OUT);

	gpio_line_set(ixp2000_gpio.scl_gpio, 0);
	gpio_line_set(ixp2000_gpio.sda_gpio, 0);

	gpio_line_config(ixp2000_gpio.scl_gpio, GPIO_IN);

	for(i = 0; i < 10; i++);

	gpio_line_config(ixp2000_gpio.sda_gpio, GPIO_IN);

	if (i2c_bit_add_bus(&ixp2000_i2c_adapter)) {
		printk("i2c-adap-ixp2000: I2C adapter registration failed\n");
		return -EIO;
	} else printk("i2c-adap-ixp2000: I2C bus initialized\n");

	return 0;
}

static void __exit ixp2000_i2c_exit(void)
{
	i2c_bit_del_bus(&ixp2000_i2c_adapter);
}

EXPORT_NO_SYMBOLS;
MODULE_AUTHOR ("Deepak Saxena <dsaxena@mvista.com>");
MODULE_DESCRIPTION("IXP2000 GPIO-based I2C bus driver");
MODULE_LICENSE("GPL");

module_init(ixp2000_i2c_init);
module_exit(ixp2000_i2c_exit);
