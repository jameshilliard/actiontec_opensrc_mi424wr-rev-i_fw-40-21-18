/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ks8695/kgdb.c
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
#include <linux/console.h>
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
#include <linux/serial_core.h>

#define UART_NR	1

#define UART_GET_CHAR(p)	  ((*(volatile u_int *) \
    ((p)->membase + KS8695_UART_RX_BUFFER)) & 0xFF)
#define UART_GET_IER(p)	          (*(volatile u_int *) \
    ((p)->membase + KS8695_INT_ENABLE))
#define UART_GET_LSR(p)	          (*(volatile u_int *) \
    ((p)->membase + KS8695_UART_LINE_STATUS))
#define UART_PUT_CHAR(p, c)       (*(u_int *) \
    ((p)->membase + KS8695_UART_TX_HOLDING) = (c))
#define UART_RX_DATA(s)           (((s) & KS8695_UART_LINES_RXFE) != 0)
#define UART_TX_READY(s)	  (((s) & KS8695_UART_LINES_TXFE) != 0)

extern struct console amba_console;
extern struct uart_port amba_ports[UART_NR];
  	 
void kgdb_serial_init(void)
{
    return;
}

void kgdb_serial_putchar(unsigned char ch)
{
    struct uart_port *port = amba_ports + amba_console.index;

    while (UART_GET_IER(port) & KS8695_INT_ENABLE_TX);
    while (!UART_TX_READY(UART_GET_LSR(port)));

    UART_PUT_CHAR(port, (u_int)ch);
}

unsigned char kgdb_serial_getchar(void)
{
    struct uart_port *port = amba_ports;

    /* Wait for incoming char */
    while (!UART_RX_DATA(UART_GET_LSR(port)));

    return (unsigned char)UART_GET_CHAR(port);
}

