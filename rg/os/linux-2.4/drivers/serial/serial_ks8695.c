/*
 *  linux/drivers/char/serial_ks8695.c
 *
 *  Driver for AMBA serial ports
 *
 *  Based on drivers/serial/serial_amba.c, by Kam Lee.
 *
 *  Copyright 2002 Micrel Inc.
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
 * This is a generic driver for ARM AMBA-type serial ports.  This is 
 * based on 16550.
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

#if defined(CONFIG_SERIAL_KS8695_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>

/*RLQ, 8/28/2003 */
#define	FIQ_VERIFICATION
#define	TIMEOUT_COUNT	1000000

#define UART_NR		        1

#define SERIAL_AMBA_MAJOR	204
#define SERIAL_AMBA_MINOR	16
#define SERIAL_AMBA_NR		UART_NR

#define CALLOUT_AMBA_NAME	"cuaam"
#define CALLOUT_AMBA_MAJOR	205
#define CALLOUT_AMBA_MINOR	16
#define CALLOUT_AMBA_NR		UART_NR

static struct tty_driver normal, callout;
static struct tty_struct *amba_table[UART_NR];
static struct termios *amba_termios[UART_NR], *amba_termios_locked[UART_NR];

#define AMBA_ISR_PASS_LIMIT	256

/*
 * Access macros for the AMBA UARTs
 */
#define UART_GET_INT_STATUS(p)    (*(volatile u_int *)((p)->membase + KS8695_INT_STATUS))
#define UART_CLR_INT_STATUS(p, c) (*(u_int *)((p)->membase + KS8695_INT_STATUS) = (c))
#define UART_GET_CHAR(p)	  ((*(volatile u_int *)((p)->membase + KS8695_UART_RX_BUFFER)) & 0xFF)
#ifndef B22447_FIXED
/* Temorary hack. Remove when B22447 is fixed! */
extern int dont_print;
#define UART_PUT_CHAR(p, c)       { if (!dont_print) (*(u_int *)((p)->membase + KS8695_UART_TX_HOLDING) = (c)); }
#else
#define UART_PUT_CHAR(p, c)       (*(u_int *)((p)->membase + KS8695_UART_TX_HOLDING) = (c))
#endif
#define UART_GET_IER(p)	          (*(volatile u_int *)((p)->membase + KS8695_INT_ENABLE))
#define UART_PUT_IER(p, c)        (*(u_int *)((p)->membase + KS8695_INT_ENABLE) = (c))
#define UART_GET_FCR(p)	          (*(volatile u_int *)((p)->membase + KS8695_UART_FIFO_CTRL))
#define UART_PUT_FCR(p, c)        (*(u_int *)((p)->membase + KS8695_UART_FIFO_CTRL) = (c))
#define UART_GET_MSR(p)	          (*(volatile u_int *)((p)->membase + KS8695_UART_MODEM_STATUS))
#define UART_GET_LSR(p)	          (*(volatile u_int *)((p)->membase + KS8695_UART_LINE_STATUS))
#define UART_GET_LCR(p)	          (*(volatile u_int *)((p)->membase + KS8695_UART_LINE_CTRL))
#define UART_PUT_LCR(p, c)        (*(u_int *)((p)->membase + KS8695_UART_LINE_CTRL) = (c))
#define UART_GET_MCR(p)	          (*(volatile u_int *)((p)->membase + KS8695_UART_MODEM_CTRL))
#define UART_PUT_MCR(p, c)        (*(u_int *)((p)->membase + KS8695_UART_MODEM_CTRL) = (c))
#define UART_GET_BRDR(p)	  (*(volatile u_int *)((p)->membase + KS8695_UART_DIVISOR))
#define UART_PUT_BRDR(p, c)       (*(u_int *)((p)->membase + KS8695_UART_DIVISOR) = (c))
#define UART_RX_DATA(s)		  (((s) & KS8695_UART_LINES_RXFE) != 0)
#define UART_TX_READY(s)	  (((s) & KS8695_UART_LINES_TXFE) != 0)

#define UART_DUMMY_LSR_RX	0x100
#define UART_PORT_SIZE		(KS8695_IRQ_PEND_PRIORITY - KS8695_UART_RX_BUFFER + 4)

static void ambauart_stop_tx(struct uart_port *port, unsigned int tty_stop)
{
	unsigned int ier;

#if DEBUG
       printk("ambauart_stop_tx() called\n");

#endif

	ier = UART_GET_IER(port);
	ier &= ~KS8695_INT_ENABLE_TX;
	UART_PUT_IER(port, ier);
}

static void ambauart_start_tx(struct uart_port *port, unsigned int tty_start)
{
	unsigned int ier;

#if DEBUG
       printk("ambauart_start_tx() called\n");

#endif

        ier = UART_GET_IER(port);
        if ( ier &  KS8695_INT_ENABLE_TX )
            return;
        else
        {
	    ier |= KS8695_INT_ENABLE_TX; 
	    UART_PUT_IER(port, ier);
	}
}

static void ambauart_stop_rx(struct uart_port *port)
{
	unsigned int ier;

#if DEBUG
       printk("ambauart_stop_rx() called\n");

#endif
	ier = UART_GET_IER(port);
	ier &= ~KS8695_INT_ENABLE_RX;
	UART_PUT_IER(port, ier);
}

static void ambauart_enable_ms(struct uart_port *port)
{
#if DEBUG
       printk("ambauart_enable_ms() called\n");
#endif
       UART_PUT_IER(port, UART_GET_IER(port) | KS8695_INT_ENABLE_MODEM);
}

static void
#ifdef SUPPORT_SYSRQ
ambauart_rx_chars(struct uart_port *port, struct pt_regs *regs,
    unsigned short status)
#else
ambauart_rx_chars(struct uart_port *port, unsigned short status)
#endif
{
	struct tty_struct *tty = port->info->tty;
	unsigned short ch, lsr, max_count = 256;
	
	while (UART_RX_DATA(status) && max_count--) {
	    lsr = status;
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
		lsr |= UART_DUMMY_LSR_RX;

		if (lsr & KS8695_UART_LINES_ANY) {
			if (lsr & KS8695_UART_LINES_BE) {
				lsr &= ~(KS8695_UART_LINES_FE | KS8695_UART_LINES_PE);
				port->icount.brk++;
				if (uart_handle_break(port))
					goto ignore_char;
			} else if (lsr & KS8695_UART_LINES_PE)
				port->icount.parity++;
			else if (lsr & KS8695_UART_LINES_FE)
				port->icount.frame++;
			if (lsr & KS8695_UART_LINES_OE)
				port->icount.overrun++;

			lsr &= port->read_status_mask;

			if (lsr & KS8695_UART_LINES_BE)
				*tty->flip.flag_buf_ptr = TTY_BREAK;
			else if (lsr & KS8695_UART_LINES_PE)
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			else if (lsr & KS8695_UART_LINES_FE)
				*tty->flip.flag_buf_ptr = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(port, ch, regs))
			goto ignore_char;

		if ((lsr & port->ignore_status_mask) == 0) {
			tty->flip.flag_buf_ptr++;
			tty->flip.char_buf_ptr++;
			tty->flip.count++;
		}
		if ((lsr & KS8695_UART_LINES_OE) &&
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
		status = UART_GET_LSR(port);
	}
	tty_flip_buffer_push(tty);
	return;
}


static void ambauart_modem_status(struct uart_port *port)
{
	unsigned int status, delta;

#if DEBUG
       printk("ambauart_modem_status() called\n");
#endif

	/*
	 * clear modem interrupt by reading MSR
	 */
	status = UART_GET_MSR(port);

	delta = status & 0x0B;

	if (!delta)
		return;

	if (delta & KS8695_UART_MODEM_DDCD)
		uart_handle_dcd_change(port, status & KS8695_UART_MODEM_DDCD);

	if (delta & KS8695_UART_MODEM_DDSR)
		port->icount.dsr++;

	if (delta & KS8695_UART_MODEM_DCTS)
		uart_handle_cts_change(port, status & KS8695_UART_MODEM_DCTS);

	wake_up_interruptible(&port->info->delta_msr_wait);
}

static void ambauart_int(int irq, void *dev_id, struct pt_regs *regs)
{
    struct uart_port *port = dev_id;
    struct uart_info *info = port->info;
    unsigned int status, ier, old_ier, count, lsr;

    old_ier  = UART_GET_IER(port);
    UART_PUT_IER(port, (old_ier & 0xFFFFF0FF));

    status = UART_GET_INT_STATUS(port);
    
    lsr = UART_GET_LSR(port);
    
    /* KS8695_INTMASK_UART_RX is not set during breakpoint as it should (looks
     * like a HW bug), so we specifically check for a breakpoint condition in
     * the UART line status register.
     * Some bits from the UART line status register are cleared only when they
     * are read by CPU. That is why we cannot read the line status register
     * twice, and should pass the first read as argument to ambauart_rx_chars.
     * Refer to CENTAUR KS8695PX's Register Description document:
     * KS8695PX_REG_DESCP_v1.0.pdf, page 58: "UART Line Status Register".
     */
    if (status & KS8695_INTMASK_UART_RX || lsr & KS8695_UART_LINES_BE)
    {
#ifdef SUPPORT_SYSRQ
	ambauart_rx_chars(port, regs, lsr);
#else
	ambauart_rx_chars(port, lsr);
#endif
    }
    if (status & KS8695_INTMASK_UART_TX) 
    {
        if (port->x_char)
        {
                UART_CLR_INT_STATUS(port, KS8695_INTMASK_UART_TX);
                UART_PUT_CHAR(port, (u_int) port->x_char);
                port->icount.tx++;
                port->x_char = 0;
                ier = UART_GET_IER(port);
                ier &= 0xFFFFFEFF;
                UART_PUT_IER(port, ier);
                printk("XOn/Off sent\n");
                return;
        }
        for ( count = 0; count < 16; count++)
        {
              if (info->xmit.head == info->xmit.tail)
              {
                 /*ier = UART_GET_IER(port);
                 ier &= 0xFFFFFEFF;
                 UART_PUT_IER(port, ier);*/
                 break;
              }
              UART_CLR_INT_STATUS(port, KS8695_INTMASK_UART_TX);
              UART_PUT_CHAR(port, (u_int) (info->xmit.buf[info->xmit.tail]));
              info->xmit.tail = (info->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
              port->icount.tx++;
        };
        if (CIRC_CNT(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE) < WAKEUP_CHARS)
                uart_write_wakeup(port);
 
        if (info->xmit.head == info->xmit.tail)
        {
           ier = UART_GET_IER(port);
           ier &= 0xFFFFFEFF;
           UART_PUT_IER(port, ier);
        }
    }
    if (status & KS8695_INTMASK_UART_MODEMS)
    {
	ambauart_modem_status(port);
    }
    if (status & KS8695_INTMASK_UART_MODEMS)
    {
         ambauart_modem_status(port);
    }
    if ( status & KS8695_INTMASK_UART_LINE_ERR)
    {
         UART_GET_LSR(port);
    }
    if (info->xmit.head == info->xmit.tail)
       UART_PUT_IER(port, (old_ier & 0xFFFFFEFF));
    else
       UART_PUT_IER(port, old_ier | KS8695_INTMASK_UART_TX);
}

static u_int ambauart_tx_empty(struct uart_port *port)
{
	unsigned int status;

	status = UART_GET_LSR(port);
	return UART_TX_READY(status) ? TIOCSER_TEMT : 0; 
}

static u_int ambauart_get_mctrl(struct uart_port *port)
{
	unsigned int result = 0;
	unsigned int status;

	status = UART_GET_MSR(port);
	if (status & KS8695_UART_MODEM_DCD)
		result |= TIOCM_CAR;
	if (status & KS8695_UART_MODEM_DSR)
		result |= TIOCM_DSR;
	if (status & KS8695_UART_MODEM_CTS)
		result |= TIOCM_CTS;

	return result;
}

static void ambauart_set_mctrl(struct uart_port *port, u_int mctrl)
{
	unsigned int mcr;

	mcr = UART_GET_MCR(port);
	if (mctrl & TIOCM_RTS)
		mcr |= KS8695_UART_MODEMC_RTS;
	else
		mcr &= ~KS8695_UART_MODEMC_RTS;

	if (mctrl & TIOCM_DTR)
		mcr |= KS8695_UART_MODEMC_DTR;
	else
		mcr &= ~KS8695_UART_MODEMC_DTR;

	UART_PUT_MCR(port, mcr);
}

static void ambauart_break_ctl(struct uart_port *port, int break_state)
{
	unsigned int lcr;

	lcr = UART_GET_LCR(port);
	if (break_state == -1)
		lcr |= KS8695_UART_LINEC_BRK;
	else
		lcr &= ~KS8695_UART_LINEC_BRK;
	UART_PUT_LCR(port, lcr);
}

static int ambauart_startup(struct uart_port *port)
{
	int retval;

#if DEBUG
  	printk("ambauart_startup ier=%x\n",UART_GET_IER(port));
#endif

	/*
	 * Allocate the IRQ, let IRQ KS8695_INT_UART_RX, KS8695_INT_UART_TX comes to same 
         * routine
	 */

        UART_PUT_IER(port, UART_GET_IER(port) & 0xFFFFF0FF);

	retval = request_irq(KS8695_INT_UART_TX, ambauart_int, SA_SHIRQ | SA_INTERRUPT, "amba", port);
	if (retval)
		return retval;

        retval = request_irq(KS8695_INT_UART_RX, ambauart_int, SA_SHIRQ | SA_INTERRUPT, "amba", port);
        if (retval)
                return retval;

        retval = request_irq(KS8695_INT_UART_LINE_ERR, ambauart_int, SA_SHIRQ | SA_INTERRUPT, "amba", port);
        if (retval)
                return retval;

        retval = request_irq(KS8695_INT_UART_MODEMS, ambauart_int, SA_SHIRQ | SA_INTERRUPT, "amba", port);
        if (retval)
                return retval;
	
        /*
	 * Finally, enable interrupts
	 */
        UART_PUT_IER(port, ((UART_GET_IER(port) & 0xFFFFF0FF) | KS8695_INT_ENABLE_RX | 0x800 | 0x400));
	return 0;
}

static void ambauart_shutdown(struct uart_port *port)
{
#if DEBUG
       printk("ambauart_shutdown\n");
#endif
        /*
         * disable all interrupts, disable the port
         */

        UART_PUT_IER(port, UART_GET_IER(port) & 0xFFFFF0FF);

        /* disable break condition and fifos */
        UART_PUT_LCR(port, UART_GET_LCR(port) & ~KS8695_UART_LINEC_BRK);
        UART_PUT_FCR(port, UART_GET_FCR(port) & ~KS8695_UART_FIFO_FEN);

        free_irq(KS8695_INT_UART_RX, port);
        free_irq(KS8695_INT_UART_TX, port);
        free_irq(KS8695_INT_UART_MODEMS, port);
        free_irq(KS8695_INT_UART_LINE_ERR, port);
}

static void ambauart_change_speed(struct uart_port *port, u_int cflag, u_int iflag, u_int quot)
{
	u_int lcr, old_ier, fcr=0;
	unsigned long flags;

#if DEBUG
	printk("ambauart_set_cflag(0x%x) called\n", cflag);
#endif
	//printk("ambauart_change_speed\n");
	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5: lcr = KS8695_UART_LINEC_WLEN5; break;
	case CS6: lcr = KS8695_UART_LINEC_WLEN6; break;
	case CS7: lcr = KS8695_UART_LINEC_WLEN7; break;
	default:  lcr = KS8695_UART_LINEC_WLEN8; break; // CS8
	}
	if (cflag & CSTOPB)
		lcr |= KS8695_UART_LINEC_STP2;
	if (cflag & PARENB) {
		lcr |= KS8695_UART_LINEC_PEN;
		if (!(cflag & PARODD))
			lcr |= KS8695_UART_LINEC_EPS;
	}
	if (port->fifosize > 1)
		fcr = KS8695_UART_FIFO_TRIG04 | KS8695_UART_FIFO_TXRST | KS8695_UART_FIFO_RXRST | KS8695_UART_FIFO_FEN;

	port->read_status_mask = KS8695_UART_LINES_OE;
	if (iflag & INPCK)
		port->read_status_mask |= (KS8695_UART_LINES_FE | KS8695_UART_LINES_PE);
	if (iflag & (BRKINT | PARMRK))
		port->read_status_mask |= KS8695_UART_LINES_BE;

	/*
	 * Characters to ignore
	 */
	port->ignore_status_mask = 0;
	if (iflag & IGNPAR)
		port->ignore_status_mask |= (KS8695_UART_LINES_FE | KS8695_UART_LINES_PE);
	if (iflag & IGNBRK) {
		port->ignore_status_mask |= KS8695_UART_LINES_BE;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (iflag & IGNPAR)
			port->ignore_status_mask |= KS8695_UART_LINES_OE;
	}

	/*
	 * Ignore all characters if CREAD is not set.
	 */
	if ((cflag & CREAD) == 0)
		port->ignore_status_mask |= UART_DUMMY_LSR_RX;

	/* first, disable everything */
	save_flags(flags); cli();
	old_ier = UART_GET_IER(port);
	UART_PUT_IER(port, old_ier & 0xFFFFF0FF);
	old_ier &= ~KS8695_INT_ENABLE_MODEM;

	if ((port->flags & ASYNC_HARDPPS_CD) ||
	    (cflag & CRTSCTS) || !(cflag & CLOCAL))
		old_ier |= KS8695_INT_ENABLE_MODEM;


	/* Set baud rate */
	//	UART_PUT_BRDR(port, port->uartclk / quot); 
	UART_PUT_BRDR(port, 0x28B); 

	UART_PUT_LCR(port, lcr);
	UART_PUT_FCR(port, fcr);
	UART_PUT_IER(port, old_ier & 0xFFFFFEFF);

	restore_flags(flags);
}

static const char *ambauart_type(struct uart_port *port)
{
	return port->type == PORT_AMBA ? "AMBA" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'
 */
static void ambauart_release_port(struct uart_port *port)
{
#if DEBUG
       printk("ambauart_release_port\n");
#endif
	release_mem_region(port->mapbase, UART_PORT_SIZE);
}

/*
 * Request the memory region(s) being used by 'port'
 */
static int ambauart_request_port(struct uart_port *port)
{
#if DEBUG
     printk("ambauart_request_port\n");
#endif
     return request_mem_region(port->mapbase, UART_PORT_SIZE, "serial_amba") != NULL ? 0 : -EBUSY;
}

/*
 * Configure/autoconfigure the port.
 */
static void ambauart_config_port(struct uart_port *port, int flags)
{
#if DEBUG
  printk("ambauart_config_port\n");
#endif
  if (flags & UART_CONFIG_TYPE)
  {
     port->type = PORT_AMBA;
     ambauart_request_port(port);
  }
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int ambauart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	int ret = 0;

#if DEBUG
	printk("ambauart_verify_port\n");
#endif
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_AMBA)
		ret = -EINVAL;
	if (ser->irq < 0 || ser->irq >= NR_IRQS)
		ret = -EINVAL;
	if (ser->baud_base < 9600)
		ret = -EINVAL;
	return ret;
}

static struct uart_ops amba_pops = {
	tx_empty:	ambauart_tx_empty,
	set_mctrl:	ambauart_set_mctrl,
	get_mctrl:	ambauart_get_mctrl,
	stop_tx:	ambauart_stop_tx,
	start_tx:	ambauart_start_tx,
	stop_rx:	ambauart_stop_rx,
	enable_ms:	ambauart_enable_ms,
	break_ctl:	ambauart_break_ctl,
	startup:	ambauart_startup,
	shutdown:	ambauart_shutdown,
	change_speed:	ambauart_change_speed,
	type:		ambauart_type,
	release_port:	ambauart_release_port,
	request_port:	ambauart_request_port,
	config_port:	ambauart_config_port,
	verify_port:	ambauart_verify_port,
};

struct uart_port amba_ports[UART_NR] = {
	{
		membase:	(void *)IO_ADDRESS(KS8695_IO_BASE),
		mapbase:	KS8695_IO_BASE,
		iotype:		SERIAL_IO_MEM,
		irq:		KS8695_INT_UART_RX,
		uartclk:	25000000,
		fifosize:	16,
		ops:		&amba_pops,
		flags:		ASYNC_BOOT_AUTOCONF,
	}
};

#define used_and_not_const_char_pointer
#ifdef CONFIG_SERIAL_KS8695_CONSOLE
#ifdef used_and_not_const_char_pointer
int ambauart_console_read(struct uart_port *port, char *s, u_int count)
{
	unsigned int status;
	int c;
#if DEBUG
	printk("ambauart_console_read() called\n");
#endif

	c = 0;
	while (c < count) {
		status = UART_GET_LSR(port);
		if (UART_RX_DATA(status)) {
			*s++ = (char) UART_GET_CHAR(port);
			c++;
		} else {
			// nothing more to get, return
			return c;
		}
	}
	// return the count
	return c;
}
#endif

void ambauart_console_write(struct console *co, const char *s, u_int count)
{
	struct uart_port *port = amba_ports + co->index;
	unsigned int status, old_ier;
	int i = 0;
#ifdef	FIQ_VERIFICATION
	u32 j = 0;
#endif
	u_char special_char;

	/*
	 *  Wait for any pending characters to be send first and then 
         *  save the CR and disable the interrupts add count in case
         *  the interrupt is locking the system.
	 */
         
         do
         {
            old_ier = UART_GET_IER(port);
            i++;
#ifndef	FIQ_VERIFICATION
         } while ( old_ier & KS8695_INT_ENABLE_TX && i < 4000000 );
#else
         } while ( old_ier & KS8695_INT_ENABLE_TX && i < 2000000 );
#endif

	 UART_PUT_IER(port, old_ier & 0xFFFFF0FF);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++) {
		do {
			status = UART_GET_LSR(port);
#ifdef	FIQ_VERIFICATION
			if (++j > TIMEOUT_COUNT)
				break;
#endif
		} while (!UART_TX_READY(status));
		UART_PUT_CHAR(port, (u_int) s[i]);
		if (s[i] == '\n') {
#ifdef	FIQ_VERIFICATION
			j = 0;
#endif
			do {
				status = UART_GET_LSR(port);
#ifdef	FIQ_VERIFICATION
				if (++j > TIMEOUT_COUNT)
					break;
#endif
			} while (!UART_TX_READY(status));
			special_char = '\r';
			UART_PUT_CHAR(port, (u_int) special_char);
		}
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the TCR
	 */
#ifdef	FIQ_VERIFICATION
	j = 0;
#endif
	do {
		status = UART_GET_LSR(port);
#ifdef	FIQ_VERIFICATION
		if (++j > TIMEOUT_COUNT)
			break;
#endif
	} while (!UART_TX_READY(status));
	UART_PUT_IER(port, old_ier);
}

static kdev_t ambauart_console_device(struct console *co)
{
	return MKDEV(SERIAL_AMBA_MAJOR, SERIAL_AMBA_MINOR + co->index);
}

static int ambauart_console_wait_key(struct console *co)
{
	struct uart_port *port = amba_ports + co->index;
	unsigned int status;

	do {
		status = UART_GET_LSR(port);
	} while (!UART_RX_DATA(status));
	return UART_GET_CHAR(port);
}

static void __init
ambauart_console_get_options(struct uart_port *port, int *baud, int *parity, int *bits)
{  
	u_int lcr;

	lcr = UART_GET_LCR(port);

	*parity = 'n';
	if (lcr & KS8695_UART_LINEC_PEN) { 
	        if (lcr & KS8695_UART_LINEC_EPS) 
		        *parity = 'e';
		else
		        *parity = 'o';
	}

	if ((lcr & 0x03) == KS8695_UART_LINEC_WLEN5)
		*bits = 5;
	else if ((lcr & 0x03) == KS8695_UART_LINEC_WLEN6)
		*bits = 6;
	else if ((lcr & 0x03) == KS8695_UART_LINEC_WLEN7)
		*bits = 7;
	else
		*bits = 8;

	*baud = port->uartclk / (UART_GET_BRDR(port) & 0x0FFF);
	*baud &= 0xFFFFFFF0;
}

static int __init ambauart_console_setup(struct console *co, char *options)
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
	port = uart_get_console(amba_ports, UART_NR, co);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		ambauart_console_get_options(port, &baud, &parity, &bits);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

struct console amba_console = {
	name:		"ttyAM",
	write:		ambauart_console_write,
#ifdef used_and_not_const_char_pointer
	read:		ambauart_console_read,
#endif
	device:		ambauart_console_device,
/*	wait_key:	ambauart_console_wait_key,*/
	setup:		ambauart_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init ks8695_console_init(void)
{
	register_console(&amba_console);
}

#define AMBA_CONSOLE	&amba_console
#else
#define AMBA_CONSOLE	NULL
#endif

static struct uart_driver amba_reg = {
	owner:			THIS_MODULE,
	normal_major:		SERIAL_AMBA_MAJOR,
#ifdef CONFIG_DEVFS_FS
	normal_name:		"ttyAM%d",
	callout_name:		"cuaam%d",
#else
	normal_name:		"ttyAM",
	callout_name:		"cuaam",
#endif
	normal_driver:		&normal,
	callout_major:		CALLOUT_AMBA_MAJOR,
	callout_driver:		&callout,
	table:			amba_table,
	termios:		amba_termios,
	termios_locked:		amba_termios_locked,
	minor:			SERIAL_AMBA_MINOR,
	nr:			UART_NR,
	cons:			AMBA_CONSOLE,
};

static int __init ambauart_init(void)
{
    int i, ret;
    
    if ((ret = uart_register_driver(&amba_reg)))
	return ret;

    for (i = 0; i < UART_NR; i++)
    {
	if ((ret = uart_add_one_port(&amba_reg, &amba_ports[i])))
	    return ret;
    }
}

static void __exit ambauart_exit(void)
{
#if DEBUG
	printk("ambauart_exit\n");
#endif
	uart_unregister_driver(&amba_reg);
}

module_init(ambauart_init);
module_exit(ambauart_exit);

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Micrel Semiconductor");
MODULE_DESCRIPTION("ARM AMBA serial port driver base on 16550");
MODULE_LICENSE("GPL");
