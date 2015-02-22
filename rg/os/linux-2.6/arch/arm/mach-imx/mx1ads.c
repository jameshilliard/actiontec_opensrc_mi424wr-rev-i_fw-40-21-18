/*
 * arch/arm/mach-imx/mx1ads.c
 *
 * Initially based on:
 *	linux-2.6.7-imx/arch/arm/mach-imx/scb9328.c
 *	Copyright (c) 2004 Sascha Hauer <sascha@saschahauer.de>
 *
 * 2004 (c) MontaVista Software, Inc.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/page.h>

#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <linux/interrupt.h>
#include "generic.h"

static struct resource cs89x0_resources[] = {
	[0] = {
		.start	= IMX_CS4_PHYS + 0x300,
		.end	= IMX_CS4_PHYS + 0x300 + 16,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_GPIOC(17),
		.end	= IRQ_GPIOC(17),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device cs89x0_device = {
	.name		= "cirrus-cs89x0",
	.num_resources	= ARRAY_SIZE(cs89x0_resources),
	.resource	= cs89x0_resources,
};

static struct platform_device *devices[] __initdata = {
	&cs89x0_device,
};

static void __init
mx1ads_init(void)
{
#ifdef CONFIG_LEDS
	imx_gpio_mode(GPIO_PORTA | GPIO_OUT | 2);
#endif
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

static void __init
mx1ads_map_io(void)
{
	imx_map_io();
}

MACHINE_START(MX1ADS, "Motorola MX1ADS")
	/* Maintainer: Sascha Hauer, Pengutronix */
	.phys_io	= 0x00200000,
	.io_pg_offst	= ((0xe0200000) >> 18) & 0xfffc,
	.boot_params	= 0x08000100,
	.map_io		= mx1ads_map_io,
	.init_irq	= imx_init_irq,
	.timer		= &imx_timer,
	.init_machine	= mx1ads_init,
MACHINE_END
