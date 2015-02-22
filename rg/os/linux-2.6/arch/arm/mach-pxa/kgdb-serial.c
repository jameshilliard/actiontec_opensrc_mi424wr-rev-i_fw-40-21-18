/*
 * linux/arch/arm/mach-pxa/kgdb-serial.c
 *
 * Provides low level kgdb serial support hooks for PXA2xx boards
 *
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2002-2005 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/serial_reg.h>
#include <linux/kgdb.h>
#include <asm/processor.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>

#if   defined(CONFIG_KGDB_PXA_FFUART)

#define UART		FFUART
#define CKEN_UART	CKEN6_FFUART
#define GPIO_RX_MD	GPIO34_FFRXD_MD
#define GPIO_TX_MD	GPIO39_FFTXD_MD

#elif defined(CONFIG_KGDB_PXA_BTUART)

#define UART		BTUART
#define CKEN_UART	CKEN7_BTUART
#define GPIO_RX_MD	GPIO42_BTRXD_MD
#define GPIO_TX_MD	GPIO43_BTTXD_MD

#elif defined(CONFIG_KGDB_PXA_STUART)

#define UART		STUART
#define CKEN_UART	CKEN5_STUART
#define GPIO_RX_MD	GPIO46_STRXD_MD
#define GPIO_TX_MD	GPIO47_STTXD_MD

#endif

#define UART_BAUDRATE	(CONFIG_KGDB_PXA_BAUDRATE)

static volatile unsigned long *port = (unsigned long *)&UART;

static int kgdb_serial_init(void)
{
	pxa_set_cken(CKEN_UART, 1);
	pxa_gpio_mode(GPIO_RX_MD);
	pxa_gpio_mode(GPIO_TX_MD);

	port[UART_IER] = 0;
	port[UART_LCR] = LCR_DLAB;
	port[UART_DLL] = ((921600 / UART_BAUDRATE) & 0xff);
	port[UART_DLM] = ((921600 / UART_BAUDRATE) >> 8);
	port[UART_LCR] = LCR_WLS1 | LCR_WLS0;
	port[UART_MCR] = 0;
	port[UART_IER] = IER_UUE;
	port[UART_FCR] = FCR_ITL_16;

	return 0;
}

static void kgdb_serial_putchar(int c)
{
	if (!(CKEN & CKEN_UART) || port[UART_IER] != IER_UUE)
		kgdb_serial_init();
	while (!(port[UART_LSR] & LSR_TDRQ))
		cpu_relax();
	port[UART_TX] = c;
}

static void kgdb_serial_flush(void)
{
	if ((CKEN & CKEN_UART) && (port[UART_IER] & IER_UUE))
		while (!(port[UART_LSR] & LSR_TEMT))
			cpu_relax();
}

static int kgdb_serial_getchar(void)
{
	unsigned char c;
	if (!(CKEN & CKEN_UART) || port[UART_IER] != IER_UUE)
		kgdb_serial_init();
	while (!(port[UART_LSR] & UART_LSR_DR))
		cpu_relax();
	c = port[UART_RX];
	return c;
}

struct kgdb_io kgdb_io_ops = {
	.init = kgdb_serial_init,
	.write_char = kgdb_serial_putchar,
	.flush = kgdb_serial_flush,
	.read_char = kgdb_serial_getchar,
};
