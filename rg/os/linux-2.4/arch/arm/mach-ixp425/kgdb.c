/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/kgdb.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include <asm/kgdb.h>
#include <asm/serial.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm-arm/arch/ixp425.h>
#include <linux/serial_reg.h>

#if CONFIG_IXDP425_KGDB_UART == 0
#define UART_BASE ((volatile u32*)IXP425_UART1_BASE_VIRT)
#else
#if CONFIG_IXDP425_KGDB_UART == 1
#define UART_BASE ((volatile u32*)IXP425_UART2_BASE_VIRT)
#else
#error INVALID CONFIG_IXDP425_KGDB_UART
#endif
#endif

#define TX_DONE	(UART_LSR_TEMT|UART_LSR_THRE)

void kgdb_serial_init(void)
{
	/* Enable access to DLL/DLM */
	UART_BASE[UART_LCR] = 0x80;
	/* Set baud rate devisors */
	UART_BASE[UART_DLL] = IXP425_DEF_UART_BAUD_DLL;
	UART_BASE[UART_DLM] = IXP425_DEF_UART_BAUD_DLM;
	/* 8N1 */
	UART_BASE[UART_LCR] = 0x3;
	/* DMAE=0 (no DMA), UUE(Unit enble)=1 */
	UART_BASE[UART_IER] = 0x40;
	/* RESESTTF | RESESTRF | TRFIFOE 
	 * - reset Tx&Rx and enable tx/rx FIFO
	 */
	UART_BASE[UART_FCR] = 0x7;
	/* Enable interrupt */
	UART_BASE[UART_MCR] = 0x8;
	return;
}

void kgdb_serial_putchar(unsigned char ch)
{
	/* Check THRE and  TEMT bits before we transmit the character */
	while ((UART_BASE[UART_LSR] & TX_DONE) != TX_DONE);
	
	*UART_BASE = ch;
}

unsigned char kgdb_serial_getchar(void)
{
	/* Wait for incomming char */
	while (!(UART_BASE[UART_LSR] & UART_LSR_DR));
	
	return (u8)*UART_BASE;
}
