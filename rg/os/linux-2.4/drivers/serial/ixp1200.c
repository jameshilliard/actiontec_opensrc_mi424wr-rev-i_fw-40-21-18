/*
 * linux/drivers/serial/serial_ixp1200.c
 *
 * Driver for IXP1200 serial console
 *
 * Copyright (c) 2001, 2002 MontaVista Software, Inc
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Heavilly based on original code from Intel 2.3.99-pre3 driver.
 * Deep Blue Solution's serial_sa1100.c driver used as a guide.
 *
 * 05/03/2001:
 *	Ported SA1xx driver to IXP1200
 * 02/12/2002:
 * 	Massive rewrite to make it fit new serial layer 
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#ifdef CONFIG_SERIAL_IXP1200_CONSOLE
#include <linux/console.h>
#endif

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/hardware.h>
#include <asm/arch/serial_reg.h>

#include <linux/serial_core.h>

#define IXP1200_ISR_PASS_LIMIT 256
#define NR_PORTS 1

static struct tty_driver normal, callout;
static struct tty_struct *ixp1200_table[NR_PORTS];
static struct termios *ixp1200_termios[NR_PORTS], 
		*ixp1200_termios_locked[NR_PORTS];


#ifdef CONFIG_SERIAL_IXP1200_CONSOLE
static struct console ixp1200_console;
#endif

/* We've been assigned a range on the "Low-density serial ports" major */
#define  SERIAL_IXP1200_MAJOR	204
#define CALLOUT_IXP1200_MAJOR	205
#define MINOR_START		5

static inline unsigned int serial_in(struct  uart_port*port, int offset)
{
	return ((volatile unsigned long *)port->membase)[offset];
}

static inline void serial_out(struct uart_port *port, int offset, int value)
{
	((volatile unsigned long *)port->membase)[offset] = value;
}

static void ixp1200_stop_tx(struct uart_port *port, u_int from_tty)
{
	u32 utcr = serial_in(port, UTCR);
	serial_out(port, UTCR, utcr & ~UTCR_XIE);
	port->read_status_mask &= ~UTSR_TXR;
}

static void ixp1200_start_tx(struct uart_port *port, u_int nonempty, 
					u_int from_tty)
{
	if(nonempty)
	{
		u32 utcr;
		unsigned long flags;

		local_irq_save(flags);
		utcr = serial_in(port, UTCR);
		port->read_status_mask |= UTSR_TXR;
		serial_out(port, UTCR, utcr | UTCR_XIE);
		local_irq_restore(flags);
	}
}

static void ixp1200_stop_rx(struct uart_port *port)
{
	u32 utcr = serial_in(port, UTCR);
	serial_out(port, UTCR, utcr & ~UTCR_RIE);
}

static void ixp1200_enable_ms(struct uart_port *port)
{
	return;
}

static void inline ixp1200_rx_chars(struct uart_info *info)
{
	struct tty_struct *tty = info->tty;
	unsigned char status, ch, flg, ignored = 0;
	struct uart_port *port = info->port;

	status = serial_in(port, UTSR);

	while(status & UTSR_RXR) 
	{
		ch = (unsigned char)serial_in(port, UART_RX);

		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		port->icount.rx++;
	
		flg = TTY_NORMAL;

		if (status & (UTSR_RPE | UTSR_RFE | UTSR_ROR)) 
			goto handle_error;
		if(uart_handle_sysrq_char(info, ch, regs))
			goto ignore_char;
		
	error_return:
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
	ignore_char:
		status = serial_in(port, UTSR);
	}
out:
	tty_flip_buffer_push(tty);
	return;

handle_error:
	if(status & UTSR_RPE)
		port->icount.parity++;
	if(status & UTSR_RFE)
		port->icount.frame++;
	if(status & UTSR_ROR)
		port->icount.overrun++;
	
	if(status & port->ignore_status_mask)
	{
		if(++ignored > 100)
			goto out;
		goto ignore_char;
	}

	status &= port->read_status_mask;

	if(status & UTSR_RPE)
		flg = TTY_PARITY;
	else if(status & UTSR_RFE)
		flg = TTY_FRAME;
	
	if (status & UTSR_ROR) {
		/*
		 * overrun does *not* affect the character
		 * we read from the FIFO
		 */
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		ch = 0;
		flg = TTY_OVERRUN;
	}
#ifdef SUPPORT_SYSRQ
	info->sysrq = 0;
#endif
	goto error_return;
}

static void inline ixp1200_tx_chars(struct uart_info *info)
{
	int count;
	struct uart_port *port = info->port;

	if (port->x_char) {
		serial_out(port, UART_TX, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
		ixp1200_stop_tx(info->port, 0);	
		return;
	}
	
	while(serial_in(port, UTSR) & UTSR_TXR) {
		serial_out(port, UART_TX, info->xmit.buf[info->xmit.tail]);
		info->xmit.tail = (info->xmit.tail + 1) & (UART_XMIT_SIZE-1);
                port->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	};

	if (CIRC_CNT(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE) < 
			WAKEUP_CHARS)
		uart_event(info, EVT_WRITE_WAKEUP);

	if (info->xmit.head == info->xmit.tail)
		ixp1200_stop_tx(info->port, 0);
}

static void ixp1200_serial_int(int irq, void *dev_id, struct pt_regs * regs)
{
	struct uart_info *info = (struct uart_info *)dev_id;
	struct uart_port *port = info->port;
	int status;
	int pass_counter = 0;

	status = serial_in(port, UTSR);
	status &= (port->read_status_mask | UTSR_TXE);
	do {
		if (status & UTSR_RXR)
			ixp1200_rx_chars(info);

		if (status & UTSR_TXE)
			ixp1200_tx_chars(info);

		if (pass_counter++ > IXP1200_ISR_PASS_LIMIT) {
			break;
		}

		status = serial_in(port, UTSR);
		status &= port->read_status_mask | UTSR_TXE;

	} while (status);
}


static u_int ixp1200_tx_empty(struct uart_port *port)
{
	return (serial_in(port, UTSR) & UTSR_TXE);
}

static void ixp1200_set_mctrl(struct uart_port *port, u_int mctrl)
{
	return;
}

static u_int ixp1200_get_mctrl(struct uart_port *unused)
{
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void ixp1200_break_ctl(struct uart_port *port, int break_state)
{
	u32 utcr = serial_in(port, UTCR);

	if(break_state == -1)
		utcr |= UTCR_BRK;
	else
		utcr &= ~UTCR_BRK;

	serial_out(port, UTCR, utcr);
}

static int ixp1200_startup(struct uart_port *port, struct uart_info *info)
{
	int	retval=0;

	retval = request_irq(port->irq, ixp1200_serial_int, SA_INTERRUPT,
			     "serial_ixp1200", (void*)info);
	if (retval) 
		return retval;

	/*
	 * Enable interrupts
	 */
	serial_out(port, UTCR, UTCR_EN | UTCR_RIE | UTCR_XIE);

	return 0;
}

static void ixp1200_shutdown(struct uart_port *port, struct uart_info *info)
{
	free_irq(port->irq, info);

	serial_out(port, UTCR, 0);	/* disable UART */
}

static void ixp1200_change_speed(struct uart_port *port, u_int cflag, 
					u_int iflag, u_int quot)
{
	unsigned long	flags;
	unsigned int utcr, utcr_old;
	u32 status;

	/* byte size and parity */
	switch (cflag & CSIZE) {
	      case CS7:
		utcr = UTCR_7_BIT; 
		break;
	      case CS8:
	      default:  
		utcr = UTCR_8_BIT; 
		break;
	}

	if (cflag & CSTOPB) {
		utcr |= UTCR_2_STOP_BIT;
	}
       	else
		utcr |= UTCR_1_STOP_BIT;

	port->read_status_mask = UTSR_RXR | UTSR_ROR | UTSR_TXE | UTSR_RXF;

	if(iflag & INPCK)
		port->read_status_mask |= UTSR_RPE | UTSR_RFE;

	port->ignore_status_mask = 0;

	if(iflag & IGNPAR)
		port->ignore_status_mask |= UTSR_RPE | UTSR_RFE;

	save_flags_cli(flags);

	/*
	 * Wait for transmiter to empty;
	 */
	do {
		status = serial_in(port, UTSR);
	} while ( !(status & UTSR_TXE ));
  
	utcr_old = serial_in(port, UTCR);

	/*
	 * Keep same interrupt state as before
	 */
	utcr_old &= (UTCR_XIE | UTCR_RIE);

	/* Disable everything */
	serial_out(port, UTCR, 0);

	utcr |= UTCR_EN;
	utcr |= utcr_old;

	quot -= 1;
	utcr |= (quot << 16);

	serial_out(port, UTCR, utcr);

	restore_flags(flags);
}

static int ixp1200_request_port(struct uart_port *port)
{
	if(request_mem_region(port->membase, (u32)0xc00, "serial_ixp100"))
		return 0;

	return -EBUSY;
}

static void ixp1200_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, 0xc00);
}

static void ixp1200_config_port(struct uart_port *port, int flags)
{
	if(flags & UART_CONFIG_TYPE && !ixp1200_request_port(port))
		port->type = PORT_IXP1200;
}

static int ixp1200_verify_port(struct uart_port *port, 
				struct serial_struct *ser)
{
	if(ser->type != PORT_UNKNOWN && ser->type != PORT_IXP1200)
		return -EINVAL;
	if (port->irq != ser->irq)
		return  -EINVAL;
	if (ser->io_type != SERIAL_IO_MEM)
		return -EINVAL;
	if (port->uartclk / 16 != ser->baud_base)
		return -EINVAL;
	if ((void *)port->mapbase != ser->iomem_base)
		return -EINVAL;
	if (port->iobase != ser->port)
		return -EINVAL;
	if (ser->hub6 != 0)
		return -EINVAL;

	return 0;
}

static const char *ixp1200_type(struct uart_port *port)
{
	return port->type == PORT_IXP1200 ? "IXP1200" : NULL;
}

static struct uart_ops ixp1200_pops = {
	tx_empty:	ixp1200_tx_empty,
	set_mctrl:	ixp1200_set_mctrl,
	get_mctrl:	ixp1200_get_mctrl,
	stop_tx:	ixp1200_stop_tx,
	start_tx:	ixp1200_start_tx,
	stop_rx:	ixp1200_stop_rx,
	enable_ms:	ixp1200_enable_ms,
	startup:	ixp1200_startup,
	shutdown:	ixp1200_shutdown,
	change_speed:	ixp1200_change_speed,
	release_port:	ixp1200_release_port,
	request_port:	ixp1200_request_port,
	config_port:	ixp1200_config_port,
	verify_port:	ixp1200_verify_port,
	type:		ixp1200_type,
};

static struct uart_port ixp1200_port = 
{
	uartclk:3686400,
	ops:&ixp1200_pops,
	fifosize:8,
	membase:(void *)CSR_UARTSR,
	mapbase:0x90003400,
	irq:IXP1200_IRQ_UART,
	iotype:SERIAL_IO_MEM,
	flags:ASYNC_BOOT_AUTOCONF
};

/*
 * ------------------------------------------------------------
 * Serial console driver
 * ------------------------------------------------------------
 */
#ifdef CONFIG_SERIAL_IXP1200_CONSOLE

static kdev_t ixp1200_console_device(struct console *co)
{
	return MKDEV(SERIAL_IXP1200_MAJOR, MINOR_START + co->index);
}

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 */
static void ixp1200_console_write(struct console *co, const char *s,
				unsigned count)
{
	struct uart_port *port = &ixp1200_port;
	int utcr;
	unsigned i, flags;

	/*
	 * Save the UTCR, disable IRQs
	 */
	save_flags_cli(flags);
	utcr = serial_in(port, UTCR);
	serial_out(port, UTCR, utcr & ~(UTCR_RIE | UTCR_XIE));
	restore_flags(flags);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++, s++) {
		while(!(serial_in(port, UTSR) & UTSR_TXR));
		serial_out(port, UART_TX, *s);
		/*
		 *	Send the character out.
		 *	If a LF, also do CR...
		 */
		if (*s == '\n') {
			while(!(serial_in(port, UTSR) & UTSR_TXR));
			serial_out(port, UART_TX, '\r');
		}
	}

	/*
	 * Wait for the tranceiver to empty
	 */
	while(!(serial_in(port, UTSR) & UTSR_TXE));

	/*
	 * Re-enable IRQs
	 */
	serial_out(port, UTCR, utcr);
}

static int __init 
ixp1200_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = CONFIG_IXP1200_DEFAULT_BAUDRATE;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	unsigned long utcr;

	port = uart_get_console(&ixp1200_port, NR_PORTS, co);

	serial_out(port, UTCR, 0);

	if(options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console ixp1200_console = {
	name:		"ttyIXP",
	write:		ixp1200_console_write,
	device:		ixp1200_console_device,
	setup:		ixp1200_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1
};

/*
 *	Register console.
 */
void __init ixp1200_console_init(void)
{
	register_console(&ixp1200_console);
}
#endif


#ifdef CONFIG_SERIAL_IXP1200_CONSOLE
#define	IXP1200_CONSOLE		&ixp1200_console
#else
#define	IXP1200_CONSOLE		NULL
#endif

static struct uart_driver ixp1200_uart = {
	owner:			THIS_MODULE,
	normal_major:		SERIAL_IXP1200_MAJOR,
#ifdef CONFIG_DEVFS_FS
	normal_name:		"ttySA%d",
	callout_name:		"cusa%d",
#else
	normal_name:		"ttySA",
	callout_name:		"cusa",
#endif
	normal_driver:		&normal,
	callout_major:		CALLOUT_IXP1200_MAJOR,
	callout_driver:		&callout,
	table:			ixp1200_table,
	termios:		ixp1200_termios,
	termios_locked:		ixp1200_termios_locked,
	minor:			MINOR_START,
	nr:			NR_PORTS,
	port:			&ixp1200_port,
	cons:			IXP1200_CONSOLE
};

/*
 * The serial driver boot-time initialization code!
 */
static int __init ixp1200_serial_init(void)
{
	return uart_register_driver(&ixp1200_uart);
}

static void __exit ixp1200_serial_exit(void) 
{
	uart_unregister_driver(&ixp1200_uart);
}

module_init(ixp1200_serial_init);
module_exit(ixp1200_serial_exit);

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Deepak Saxena <dsaxena@mvista.com>")
MODULE_DESCRIPTION("IXP12xx serial port driver");
MODULE_LICENSE("GPL");

