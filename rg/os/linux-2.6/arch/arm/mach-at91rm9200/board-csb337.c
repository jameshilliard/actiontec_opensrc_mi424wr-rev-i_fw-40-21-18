/*
 * linux/arch/arm/mach-at91rm9200/board-csb337.c
 *
 *  Copyright (C) 2005 SAN People
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/hardware.h>
#include <asm/mach/serial_at91rm9200.h>
#include <asm/arch/board.h>

#include "generic.h"

static void __init csb337_init_irq(void)
{
	/* Initialize AIC controller */
	at91rm9200_init_irq(NULL);

	/* Set up the GPIO interrupts */
	at91_gpio_irq_setup(BGA_GPIO_BANKS);
}

/*
 * Serial port configuration.
 *    0 .. 3 = USART0 .. USART3
 *    4      = DBGU
 */
#define CSB337_UART_MAP		{ 4, 1, -1, -1, -1 }	/* ttyS0, ..., ttyS4 */
#define CSB337_SERIAL_CONSOLE	0			/* ttyS0 */

static void __init csb337_map_io(void)
{
	int serial[AT91_NR_UART] = CSB337_UART_MAP;
	int i;

	at91rm9200_map_io();

	/* Initialize clocks: 3.6864 MHz crystal */
	at91_clock_init(3686400);

#ifdef CONFIG_SERIAL_AT91
	at91_console_port = CSB337_SERIAL_CONSOLE;
	memcpy(at91_serial_map, serial, sizeof(serial));

	/* Register UARTs */
	for (i = 0; i < AT91_NR_UART; i++) {
		if (serial[i] >= 0)
			at91_register_uart(i, serial[i]);
	}
#endif
}

static struct at91_eth_data __initdata csb337_eth_data = {
	.phy_irq_pin	= AT91_PIN_PC2,
	.is_rmii	= 0,
};

static struct at91_usbh_data __initdata csb337_usbh_data = {
	.ports		= 2,
};

static struct at91_udc_data __initdata csb337_udc_data = {
	// this has no VBUS sensing pin
	.pullup_pin	= AT91_PIN_PA24,
};

static struct at91_cf_data __initdata csb337_cf_data = {
	/*
	 * connector P4 on the CSB 337 mates to
	 * connector P8 on the CSB 300CF
	 */

	/* CSB337 specific */
	.det_pin	= AT91_PIN_PC3,

	/* CSB300CF specific */
	.irq_pin	= AT91_PIN_PA19,
	.vcc_pin	= AT91_PIN_PD0,
	.rst_pin	= AT91_PIN_PD2,
};

static struct at91_mmc_data __initdata csb337_mmc_data = {
	.det_pin	= AT91_PIN_PD5,
	.is_b		= 0,
	.wire4		= 1,
	.wp_pin		= AT91_PIN_PD6,
};

static void __init csb337_board_init(void)
{
	/* Ethernet */
	at91_add_device_eth(&csb337_eth_data);
	/* USB Host */
	at91_add_device_usbh(&csb337_usbh_data);
	/* USB Device */
	at91_add_device_udc(&csb337_udc_data);
	/* Compact Flash */
	at91_set_gpio_input(AT91_PIN_PB22, 1);		/* IOIS16 */
	at91_add_device_cf(&csb337_cf_data);
	/* MMC */
	at91_add_device_mmc(&csb337_mmc_data);
}

MACHINE_START(CSB337, "Cogent CSB337")
	/* Maintainer: Bill Gatliff */
	.phys_io	= AT91_BASE_SYS,
	.io_pg_offst	= (AT91_VA_BASE_SYS >> 18) & 0xfffc,
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91rm9200_timer,
	.map_io		= csb337_map_io,
	.init_irq	= csb337_init_irq,
	.init_machine	= csb337_board_init,
MACHINE_END
