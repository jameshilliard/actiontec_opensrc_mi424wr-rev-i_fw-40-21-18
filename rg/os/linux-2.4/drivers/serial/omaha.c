/*
 *  linux/drivers/char/omaha.c
 *
 *  Driver for Omaha serial port
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Copyright 1999-2002 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
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
 *
 *  $Id: omaha.c,v 1.1.1.1 2007/05/07 23:29:50 jungo Exp $
 *
 * This is a generic driver for ARM AMBA-type serial ports.  They
 * have a lot of 16550-like features, but are not register compatable.
 * Note that although they do have CTS, DCD and DSR inputs, they do
 * not have an RI input, nor do they have DTR or RTS outputs.  If
 * required, these have to be supplied via some other means (eg, GPIO)
 * and hooked into this driver.
 */
  
#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/circ_buf.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

#if defined(CONFIG_SERIAL_OMAHA_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>

#include <asm/hardware/serial_omaha.h>

#define UART_NR		1

#define SERIAL_OMAHA_MAJOR	204
#define SERIAL_OMAHA_MINOR	32
#define SERIAL_OMAHA_NR		UART_NR

#define CALLOUT_OMAHA_NAME	"cuaom"
#define CALLOUT_OMAHA_MAJOR	205
#define CALLOUT_OMAHA_MINOR	32
#define CALLOUT_OMAHA_NR	UART_NR

static struct tty_driver normal, callout;
static struct tty_struct *omaha_table[UART_NR];
static struct termios *omaha_termios[UART_NR], *omaha_termios_locked[UART_NR];
#ifdef SUPPORT_SYSRQ
static struct console omaha_console;
#endif

#define OMAHA_ISR_PASS_LIMIT	256

/*
 * Access macros for the Omaha UARTs
 */

#define UART_GET_FR(p)		readb((p)->membase + OMAHA_UTRSTAT)
#define UART_GET_CHAR(p)    	readb((p)->membase + OMAHA_URXH)
#define UART_PUT_CHAR(p, c)	writel((c), (p)->membase + OMAHA_UTXH)
#define UART_GET_RSR(p)		readb((p)->membase + OMAHA_UERSTAT)
#define UART_FIFO_STATUS(p)	(readl((p)->membase + OMAHA_UFSTAT))
#define UART_RX_DATA(s)		(((s) & OMAHA_RXFF_CNT) != 0)
#define UART_TX_DATA(s)		(!((s) & OMAHA_TXFF))
#define UART_TX_READY(s)	(((s) & OMAHA_UTX_EMPTY))
#define UART_TX_EMPTY(p)	((UART_GET_FR(p) & OMAHA_UTXEMPTY) != 0)
				      
#define UART_DUMMY_RSR_RX	256
#define UART_PORT_SIZE		64

#define RX_IRQ(port)		((port)->irq)
#define TX_IRQ(port)		((port)->irq + 5)

/*
 * Our private driver data mappings.
 */
#define drv_old_status	driver_priv

static void omahauart_stop_tx(struct uart_port *port, u_int from_tty)
{
	disable_irq(TX_IRQ(port));
}

static void omahauart_start_tx(struct uart_port *port, u_int nonempty, u_int from_tty)
{
	if (nonempty)
		enable_irq(TX_IRQ(port));
}

static void omahauart_stop_rx(struct uart_port *port)
{
	disable_irq(RX_IRQ(port));
}

static void omahauart_enable_ms(struct uart_port *port)
{
	// Do nothing...
}

static void
#ifdef SUPPORT_SYSRQ
omahauart_rx_chars(struct uart_info *info, struct pt_regs *regs)
#else
omahauart_rx_chars(struct uart_info *info)
#endif
{
	struct tty_struct *tty = info->tty;
	volatile unsigned int status, data, ch, rsr, max_count = 256;
	struct uart_port *port = info->port;

	status = UART_FIFO_STATUS(port);
	while (UART_RX_DATA(status) && max_count--) {
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			tty->flip.tqueue.routine((void *)tty);
			if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
				printk(KERN_WARNING "TTY_DONT_FLIP set\n");
				return;
			}
		}

		ch = UART_GET_CHAR(port);

		*tty->flip.char_buf_ptr = ch;
		*tty->flip.flag_buf_ptr = TTY_NORMAL;
		port->icount.rx++;

		/*
		 * Note that the error handling code is
		 * out of the main execution path
		 */
		rsr = UART_GET_RSR(port) | UART_DUMMY_RSR_RX;
		if (rsr & 0xf) {
			if (rsr & OMAHA_UART_BREAK) {
				rsr &= ~(OMAHA_UART_FRAME | OMAHA_UART_PARITY);
				port->icount.brk++;
				if (uart_handle_break(info, &omaha_console))
					goto ignore_char;
			} else if (rsr & OMAHA_UART_PARITY)
				port->icount.parity++;
			else if (rsr & OMAHA_UART_FRAME)
				port->icount.frame++;
			if (rsr & OMAHA_UART_OVERRUN)
				port->icount.overrun++;

			rsr &= port->read_status_mask;

			if (rsr & OMAHA_UART_BREAK)
				*tty->flip.flag_buf_ptr = TTY_BREAK;
			else if (rsr & OMAHA_UART_PARITY)
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			else if (rsr & OMAHA_UART_FRAME)
				*tty->flip.flag_buf_ptr = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(info, ch, regs))
			goto ignore_char;

		if ((rsr & port->ignore_status_mask) == 0) {
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if ((rsr & OMAHA_UART_OVERRUN) &&
		    tty->flip.count < TTY_FLIPBUF_SIZE) {
			/*
			 * Overrun is special, since it's reported
			 * immediately, and doesn't affect the current
			 * character
			 */
			*tty->flip.char_buf_ptr++ = 0;
			*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
			tty->flip.count++;
		}
	ignore_char:
		status = UART_FIFO_STATUS(port);
	}
	tty_flip_buffer_push(tty);
	return;
}

static void omahauart_tx_chars(struct uart_info *info)
{
	struct uart_port *port = info->port;
	volatile unsigned int status;

	if (port->x_char) {
		UART_PUT_CHAR(port, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
		omahauart_stop_tx(port, 0);
		return;
	}

	status = UART_FIFO_STATUS(info->port);
	
	// FIll FIFO as far as possible
	while(UART_TX_DATA(UART_FIFO_STATUS(info->port)))
	{
		UART_PUT_CHAR(port, info->xmit.buf[info->xmit.tail]);
		info->xmit.tail = (info->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	}

	if (CIRC_CNT(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE) <
			WAKEUP_CHARS)
		uart_event(info, EVT_WRITE_WAKEUP);

	if (info->xmit.head == info->xmit.tail)
		omahauart_stop_tx(info->port, 0);
}

static void omahauart_int_tx(int irq, void *dev_id, struct pt_regs *regs)
{
	struct uart_info *info = dev_id;
	volatile unsigned int status, pass_counter = OMAHA_ISR_PASS_LIMIT;

	status = UART_FIFO_STATUS(info->port);
	do {
		// TX if FIFO not full
		if (UART_TX_DATA(status))
			omahauart_tx_chars(info);
		
		if (pass_counter-- == 0)
			break;

		status = UART_FIFO_STATUS(info->port);
	} while (UART_TX_DATA(status));
}

static void omahauart_int_rx(int irq, void *dev_id, struct pt_regs *regs)
{
	struct uart_info *info = dev_id;
	volatile unsigned int status, pass_counter = OMAHA_ISR_PASS_LIMIT;

	status = UART_FIFO_STATUS(info->port);
	do {
		if (UART_RX_DATA(status))
#ifdef SUPPORT_SYSRQ
			omahauart_rx_chars(info, regs);
#else
			omahauart_rx_chars(info);
#endif
		
		if (pass_counter-- == 0)
			break;

		status = UART_FIFO_STATUS(info->port);
	} while (UART_RX_DATA(status));
}

static u_int omahauart_tx_empty(struct uart_port *port)
{
	return UART_FIFO_STATUS(port) ? 0 : TIOCSER_TEMT;
}

static int omahauart_get_mctrl(struct uart_port *port)
{
	// Report no errors.

	return 0;
}

static void omahauart_set_mctrl(struct uart_port *port, u_int mctrl)
{
	// Do nothing.
}

static void omahauart_break_ctl(struct uart_port *port, int break_state)
{
	// Do nothing.
}

static int omahauart_startup(struct uart_port *port, struct uart_info *info)
{
	unsigned int tmp;
	int retval;

	/*
	 * Allocate the IRQs
	 */
	retval = request_irq(TX_IRQ(port), omahauart_int_tx, 0, "omaha_uart_tx", info);
	if (retval)
		return retval;

	retval = request_irq(RX_IRQ(port), omahauart_int_rx, 0, "omaha_uart_rx", info);
	
	if (retval)
	{
		free_irq(TX_IRQ(port), info);
		return retval;
	}
	
	/*
	 * initialise the old status of the modem signals
	 */
	info->drv_old_status = 0;

	// Clear all errors
	writel(0, port->membase + OMAHA_UERSTAT);
	
	// Enable FIFO, 16-byte watermark, also do reset (auto-clearing)
	writel(0xF7, port->membase + OMAHA_UFCON);

	// Level driven TX/RX ints, with rx timeout enabled
	tmp = readl(port->membase + OMAHA_UCON);
	tmp |= 0x280; // rx is pulse driven...
	writel(tmp, port->membase + OMAHA_UCON);

	return 0;
}

static void omahauart_shutdown(struct uart_port *port, struct uart_info *info)
{
	/*
	 * Free the interrupt
	 */
	free_irq(TX_IRQ(port), info);	/* TX interrupt */
	free_irq(RX_IRQ(port), info);	/* RX interrupt */

}

static void omahauart_change_speed(struct uart_port *port, u_int cflag, u_int iflag, u_int quot)
{
	// Do nothing.
}

static const char *omahauart_type(struct uart_port *port)
{
	return port->type == PORT_OMAHA ? "OMAHA" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'
 */
static void omahauart_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, UART_PORT_SIZE);
}

/*
 * Request the memory region(s) being used by 'port'
 */
static int omahauart_request_port(struct uart_port *port)
{
	return request_mem_region(port->mapbase, UART_PORT_SIZE, "serial_omaha")
			!= NULL ? 0 : -EBUSY;
}

/*
 * Configure/autoconfigure the port.
 */
static void omahauart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_OMAHA;
		omahauart_request_port(port);
	}
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int omahauart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	int ret = 0;
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_OMAHA)
		ret = -EINVAL;
	if (ser->irq < 0 || ser->irq >= NR_IRQS)
		ret = -EINVAL;
	if (ser->baud_base < 9600)
		ret = -EINVAL;
	return ret;
}

static struct uart_ops omaha_pops = {
	.tx_empty	= omahauart_tx_empty,
	.set_mctrl	= omahauart_set_mctrl,
	.get_mctrl	= omahauart_get_mctrl,
	.stop_tx	= omahauart_stop_tx,
	.start_tx	= omahauart_start_tx,
	.stop_rx	= omahauart_stop_rx,
	.enable_ms	= omahauart_enable_ms,
	.break_ctl	= omahauart_break_ctl,
	.startup	= omahauart_startup,
	.shutdown	= omahauart_shutdown,
	.change_speed	= omahauart_change_speed,
	.type		= omahauart_type,
	.release_port	= omahauart_release_port,
	.request_port	= omahauart_request_port,
	.config_port	= omahauart_config_port,
	.verify_port	= omahauart_verify_port,
};

static struct uart_port omaha_ports[UART_NR] = {
	{
		.membase	= (void *)IO_ADDRESS(OMAHA_UART0_BASE),
		.mapbase	= OMAHA_UART0_BASE,
		.iotype		= SERIAL_IO_MEM,
		.irq		= OMAHA_INT_URXD0,
		.uartclk	= 10000000,
		.fifosize	= 8,
		.unused		= { 4, 5 }, /*Udriver_priv: PORT_CTRLS(5, 4), */
		.ops		= &omaha_pops,
		.flags		= ASYNC_BOOT_AUTOCONF,
	}
};

#ifdef CONFIG_SERIAL_OMAHA_CONSOLE
static void omahauart_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = omaha_ports + co->index;
	unsigned int status;
	int i;

	/*
	 *	First save the CR then disable the interrupts
	 */

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++) {
		do {
			status = UART_GET_FR(port);
		} while ((status & OMAHA_UTX_EMPTY) == 0);
		UART_PUT_CHAR(port, s[i]);
		if (s[i] == '\n') {
			do {
				status = UART_GET_FR(port);
			} while ((status & OMAHA_UTX_EMPTY) == 0);
			UART_PUT_CHAR(port, '\r');
		}
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the TCR
	 */
	do {
		status = UART_GET_FR(port);
	} while ((status & OMAHA_UTX_EMPTY) == 0);
}

static kdev_t omahauart_console_device(struct console *co)
{
	return MKDEV(SERIAL_OMAHA_MAJOR, SERIAL_OMAHA_MINOR + co->index);
}

static int omahauart_console_wait_key(struct console *co)
{
	struct uart_port *port = omaha_ports + co->index;
	unsigned int status;

	do {
		status = UART_FIFO_STATUS(port);
	} while (!UART_RX_DATA(status));
	return UART_GET_CHAR(port);
}

static void __init
omahauart_console_get_options(struct uart_port *port, int *baud, int *parity, int *bits)
{
	// Do nothing.
}

static int __init omahauart_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 38400;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	port = uart_get_console(omaha_ports, UART_NR, co);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		omahauart_console_get_options(port, &baud, &parity, &bits);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console omaha_console = {
	.write		= omahauart_console_write,
	.device		= omahauart_console_device,
	.wait_key	= omahauart_console_wait_key,
	.setup		= omahauart_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

void __init omahauart_console_init(void)
{
	register_console(&omaha_console);
}

#define OMAHA_CONSOLE	&omaha_console
#else
#define OMAHA_CONSOLE	NULL
#endif

static struct uart_driver omaha_reg = {
	.owner			= THIS_MODULE,
	.normal_major		= SERIAL_OMAHA_MAJOR,
#ifdef CONFIG_DEVFS_FS
	.normal_name		= "ttyOM%d",
	.callout_name		= "cuaom%d",
#else
	.normal_name		= "ttyOM",
	.callout_name		= "cuaom",
#endif
	.normal_driver		= &normal,
	.callout_major		= CALLOUT_OMAHA_MAJOR,
	.callout_driver		= &callout,
	.table			= omaha_table,
	.termios		= omaha_termios,
	.termios_locked		= omaha_termios_locked,
	.minor			= SERIAL_OMAHA_MINOR,
	.nr			= UART_NR,
	.port			= omaha_ports,
	.cons			= OMAHA_CONSOLE,
};

static int __init omahauart_init(void)
{
	return uart_register_driver(&omaha_reg);
}

static void __exit omahauart_exit(void)
{
	uart_unregister_driver(&omaha_reg);
}

module_init(omahauart_init);
module_exit(omahauart_exit);
