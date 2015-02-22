/*
 * linux/drivers/char/serial_21285.c
 *
 * Driver for the serial port on the 21285 StrongArm-110 core logic chip.
 *
 * Based on drivers/char/serial.c
 *
 *  $Id: 21285.c,v 1.1.1.1 2007/05/07 23:29:50 jungo Exp $
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/console.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/hardware/dec21285.h>
#include <asm/hardware.h>

#define BAUD_BASE		(mem_fclk_21285/64)

#define SERIAL_21285_NAME	"ttyFB"
#define SERIAL_21285_MAJOR	204
#define SERIAL_21285_MINOR	4

#define SERIAL_21285_AUXNAME	"cuafb"
#define SERIAL_21285_AUXMAJOR	205
#define SERIAL_21285_AUXMINOR	4

#ifdef CONFIG_SERIAL_21285_OLD
#include <asm/mach-types.h>
/*
 * Compatability with a mistake made a long time ago.
 * Note - the use of "ttyI", "/dev/ttyS0" and major/minor 5,64
 * is HIGHLY DEPRECIATED, and will be removed in the 2.5
 * kernel series.
 *					-- rmk 15/04/2000 
 */
#define SERIAL_21285_OLD_NAME	"ttyI"
#define SERIAL_21285_OLD_MAJOR	TTY_MAJOR
#define SERIAL_21285_OLD_MINOR	64

static struct tty_driver rs285_old_driver;
#endif

static struct tty_driver rs285_driver, callout_driver;
static int rs285_refcount;
static struct tty_struct *rs285_table[1];

static struct termios *rs285_termios[1];
static struct termios *rs285_termios_locked[1];

static char wbuf[1000], *putp = wbuf, *getp = wbuf, x_char;
static struct tty_struct *rs285_tty;
static DECLARE_MUTEX(rs285_sem);
static int rs285_use_count;
static unsigned long rs285_irq_enabled;

#define TX_IRQ_BIT	(0)
#define RX_IRQ_BIT	(1)

static void rs285_stop_tx(void)
{
	if (test_and_clear_bit(TX_IRQ_BIT, &rs285_irq_enabled))
		disable_irq(IRQ_CONTX);
}

static void rs285_start_tx(void)
{
	if (!test_and_set_bit(TX_IRQ_BIT, &rs285_irq_enabled))
		enable_irq(IRQ_CONTX);
}

static void rs285_stop_rx(void)
{
	if (test_and_clear_bit(RX_IRQ_BIT, &rs285_irq_enabled))
		disable_irq(IRQ_CONRX);
}

static void rs285_start_rx(void)
{
	if (!test_and_set_bit(RX_IRQ_BIT, &rs285_irq_enabled))
		enable_irq(IRQ_CONRX);
}

static int rs285_write_room(struct tty_struct *tty)
{
	return putp >= getp ? (sizeof(wbuf) - (long) putp + (long) getp) : ((long) getp - (long) putp - 1);
}

static void rs285_rx_int(int irq, void *dev_id, struct pt_regs *regs)
{
	if (!rs285_tty) {
		rs285_stop_rx();
		return;
	}
	while (!(*CSR_UARTFLG & 0x10)) {
		int ch, flag;
		ch = *CSR_UARTDR;
		flag = *CSR_RXSTAT;
		if (flag & 4)
			tty_insert_flip_char(rs285_tty, 0, TTY_OVERRUN);
		if (flag & 2)
			flag = TTY_PARITY;
		else if (flag & 1)
			flag = TTY_FRAME;
		tty_insert_flip_char(rs285_tty, ch, flag);
	}
	tty_flip_buffer_push(rs285_tty);
}

static void rs285_send_xchar(struct tty_struct *tty, char ch)
{
	x_char = ch;
	rs285_start_tx();
}

static void rs285_throttle(struct tty_struct *tty)
{
	if (I_IXOFF(tty))
		rs285_send_xchar(tty, STOP_CHAR(tty));
}

static void rs285_unthrottle(struct tty_struct *tty)
{
	if (I_IXOFF(tty)) {
		if (x_char)
			x_char = 0;
		else
			rs285_send_xchar(tty, START_CHAR(tty));
	}
}

static void rs285_tx_int(int irq, void *dev_id, struct pt_regs *regs)
{
	while (!(*CSR_UARTFLG & 0x20)) {
		if (x_char) {
			*CSR_UARTDR = x_char;
			x_char = 0;
			continue;
		}
		if (putp == getp) {
			rs285_stop_tx();
			break;
		}
		*CSR_UARTDR = *getp;
		if (++getp >= wbuf + sizeof(wbuf))
			getp = wbuf;
	}
	if (rs285_tty)
		wake_up_interruptible(&rs285_tty->write_wait);
}

static inline int rs285_xmit(int ch)
{
	if (putp + 1 == getp || (putp + 1 == wbuf + sizeof(wbuf) && getp == wbuf))
		return 0;
	*putp = ch;
	if (++putp >= wbuf + sizeof(wbuf))
		putp = wbuf;
	rs285_start_tx();
	return 1;
}

static int rs285_write(struct tty_struct *tty, int from_user,
		       const u_char * buf, int count)
{
	int i;

	if (from_user && verify_area(VERIFY_READ, buf, count))
		return -EINVAL;

	for (i = 0; i < count; i++) {
		char ch;
		if (from_user)
			__get_user(ch, buf + i);
		else
			ch = buf[i];
		if (!rs285_xmit(ch))
			break;
	}
	return i;
}

static void rs285_put_char(struct tty_struct *tty, u_char ch)
{
	rs285_xmit(ch);
}

static int rs285_chars_in_buffer(struct tty_struct *tty)
{
	return sizeof(wbuf) - rs285_write_room(tty);
}

static void rs285_flush_buffer(struct tty_struct *tty)
{
	rs285_stop_tx();
	putp = getp = wbuf;
	if (x_char)
		rs285_start_tx();
}

static inline void rs285_set_cflag(int cflag)
{
	int h_lcr, baud, quot;

	switch (cflag & CSIZE) {
	case CS5:
		h_lcr = 0x10;
		break;
	case CS6:
		h_lcr = 0x30;
		break;
	case CS7:
		h_lcr = 0x50;
		break;
	default: /* CS8 */
		h_lcr = 0x70;
		break;

	}
	if (cflag & CSTOPB)
		h_lcr |= 0x08;
	if (cflag & PARENB)
		h_lcr |= 0x02;
	if (!(cflag & PARODD))
		h_lcr |= 0x04;

	switch (cflag & CBAUD) {
	case B200:	baud = 200;		break;
	case B300:	baud = 300;		break;
	case B1200:	baud = 1200;		break;
	case B1800:	baud = 1800;		break;
	case B2400:	baud = 2400;		break;
	case B4800:	baud = 4800;		break;
	default:
	case B9600:	baud = 9600;		break;
	case B19200:	baud = 19200;		break;
	case B38400:	baud = 38400;		break;
	case B57600:	baud = 57600;		break;
	case B115200:	baud = 115200;		break;
	}

	/*
	 * The documented expression for selecting the divisor is:
	 *  BAUD_BASE / baud - 1
	 * However, typically BAUD_BASE is not divisible by baud, so
	 * we want to select the divisor that gives us the minimum
	 * error.  Therefore, we want:
	 *  int(BAUD_BASE / baud - 0.5) ->
	 *  int(BAUD_BASE / baud - (baud >> 1) / baud) ->
	 *  int((BAUD_BASE - (baud >> 1)) / baud)
	 */
	quot = (BAUD_BASE - (baud >> 1)) / baud;

	*CSR_UARTCON = 0;
	*CSR_L_UBRLCR = quot & 0xff;
	*CSR_M_UBRLCR = (quot >> 8) & 0x0f;
	*CSR_H_UBRLCR = h_lcr;
	*CSR_UARTCON = 1;
}

static void rs285_set_termios(struct tty_struct *tty, struct termios *old)
{
	if (old && tty->termios->c_cflag == old->c_cflag)
		return;
	rs285_set_cflag(tty->termios->c_cflag);
}


static void rs285_stop(struct tty_struct *tty)
{
	rs285_stop_tx();
}

static void rs285_start(struct tty_struct *tty)
{
	rs285_start_tx();
}

static void rs285_wait_until_sent(struct tty_struct *tty, int timeout)
{
	int orig_jiffies = jiffies;
	while (*CSR_UARTFLG & 8) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
	set_current_state(TASK_RUNNING);
}

static int rs285_open(struct tty_struct *tty, struct file *filp)
{
	int line, ret;

	MOD_INC_USE_COUNT;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if (line)
		return -ENODEV;

	ret = down_interruptible(&rs285_sem);
	if (ret)
		return ret;

	tty->driver_data = NULL;
	rs285_tty = tty;

	if (rs285_use_count == 0) {
		rs285_irq_enabled = 3;
		ret = request_irq(IRQ_CONRX, rs285_rx_int, 0, "rs285", NULL);
		if (ret == 0) {
			ret = request_irq(IRQ_CONTX, rs285_tx_int, 0, "rs285",
					  NULL);
			if (ret)
				free_irq(IRQ_CONRX, NULL);
		}
	}

	if (ret == 0)
		rs285_use_count++;

	up(&rs285_sem);

	return ret;
}

static void rs285_close(struct tty_struct *tty, struct file *filp)
{
	down(&rs285_sem);
	if (!--rs285_use_count) {
		rs285_wait_until_sent(tty, 0);
		rs285_stop_rx();
		rs285_stop_tx();
		rs285_tty = NULL;
		free_irq(IRQ_CONTX, NULL);
		free_irq(IRQ_CONRX, NULL);
	}
	up(&rs285_sem);
	MOD_DEC_USE_COUNT;
}

static int __init rs285_init(void)
{
	int baud = B9600;

	if (machine_is_personal_server())
		baud = B57600;

	rs285_driver.magic = TTY_DRIVER_MAGIC;
	rs285_driver.driver_name = "serial_21285";
	rs285_driver.name = SERIAL_21285_NAME;
	rs285_driver.major = SERIAL_21285_MAJOR;
	rs285_driver.minor_start = SERIAL_21285_MINOR;
	rs285_driver.num = 1;
	rs285_driver.type = TTY_DRIVER_TYPE_SERIAL;
	rs285_driver.subtype = SERIAL_TYPE_NORMAL;
	rs285_driver.init_termios = tty_std_termios;
	rs285_driver.init_termios.c_cflag = baud | CS8 | CREAD | HUPCL | CLOCAL;
	rs285_driver.flags = TTY_DRIVER_REAL_RAW;
	rs285_driver.refcount = &rs285_refcount;
	rs285_driver.table = rs285_table;
	rs285_driver.termios = rs285_termios;
	rs285_driver.termios_locked = rs285_termios_locked;

	rs285_driver.open = rs285_open;
	rs285_driver.close = rs285_close;
	rs285_driver.write = rs285_write;
	rs285_driver.put_char = rs285_put_char;
	rs285_driver.write_room = rs285_write_room;
	rs285_driver.chars_in_buffer = rs285_chars_in_buffer;
	rs285_driver.flush_buffer = rs285_flush_buffer;
	rs285_driver.throttle = rs285_throttle;
	rs285_driver.unthrottle = rs285_unthrottle;
	rs285_driver.send_xchar = rs285_send_xchar;
	rs285_driver.set_termios = rs285_set_termios;
	rs285_driver.stop = rs285_stop;
	rs285_driver.start = rs285_start;
	rs285_driver.wait_until_sent = rs285_wait_until_sent;

	callout_driver = rs285_driver;
	callout_driver.name = SERIAL_21285_AUXNAME;
	callout_driver.major = SERIAL_21285_AUXMAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

#ifdef CONFIG_SERIAL_21285_OLD
	if (!machine_is_ebsa285() && !machine_is_netwinder()) {
		rs285_old_driver = rs285_driver;
		rs285_old_driver.name = SERIAL_21285_OLD_NAME;
		rs285_old_driver.major = SERIAL_21285_OLD_MAJOR;
		rs285_old_driver.minor_start = SERIAL_21285_OLD_MINOR;

		if (tty_register_driver(&rs285_old_driver))
			printk(KERN_ERR "Couldn't register old 21285 serial driver\n");
	}
#endif

	if (tty_register_driver(&rs285_driver))
		printk(KERN_ERR "Couldn't register 21285 serial driver\n");
	if (tty_register_driver(&callout_driver))
		printk(KERN_ERR "Couldn't register 21285 callout driver\n");

	return 0;
}

static void __exit rs285_fini(void)
{
	unsigned long flags;
	int ret;

	save_flags(flags);
	cli();
	ret = tty_unregister_driver(&callout_driver);
	if (ret)
		printk(KERN_ERR "Unable to unregister 21285 callout driver "
			"(%d)\n", ret);
	ret = tty_unregister_driver(&rs285_driver);
	if (ret)
		printk(KERN_ERR "Unable to unregister 21285 driver (%d)\n",
			ret);
#ifdef CONFIG_SERIAL_21285_OLD
	if (!machine_is_ebsa285() && !machine_is_netwinder()) {
		ret = tty_unregister_driver(&rs285_old_driver);
		if (ret)
			printk(KERN_ERR "Unable to unregister old 21285 "
				"driver (%d)\n", ret);
	}
#endif
	free_irq(IRQ_CONTX, NULL);
	free_irq(IRQ_CONRX, NULL);
	restore_flags(flags);
}

module_init(rs285_init);
module_exit(rs285_fini);

#ifdef CONFIG_SERIAL_21285_CONSOLE
/************** console driver *****************/

static void rs285_console_write(struct console *co, const char *s, u_int count)
{
	int i;

	rs285_stop_tx();
	for (i = 0; i < count; i++) {
		while (*CSR_UARTFLG & 0x20);
		*CSR_UARTDR = s[i];
		if (s[i] == '\n') {
			while (*CSR_UARTFLG & 0x20);
			*CSR_UARTDR = '\r';
		}
	}
	rs285_start_tx();
}

static kdev_t rs285_console_device(struct console *c)
{
	return MKDEV(SERIAL_21285_MAJOR, SERIAL_21285_MINOR);
}

static int __init rs285_console_setup(struct console *co, char *options)
{
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int cflag = CREAD | HUPCL | CLOCAL;

	if (machine_is_personal_server())
		baud = 57600;

	if (options) {
		char *s = options;
		baud = simple_strtoul(options, NULL, 10);
		while (*s >= '0' && *s <= '9')
			s++;
		if (*s)
			parity = *s++;
		if (*s)
			bits = *s - '0';
	}

	/*
	 *    Now construct a cflag setting.
	 */
	switch (baud) {
	case 1200:
		cflag |= B1200;
		break;
	case 2400:
		cflag |= B2400;
		break;
	case 4800:
		cflag |= B4800;
		break;
	case 9600:
		cflag |= B9600;
		break;
	case 19200:
		cflag |= B19200;
		break;
	case 38400:
		cflag |= B38400;
		break;
	case 57600:
		cflag |= B57600;
		break;
	case 115200:
		cflag |= B115200;
		break;
	default:
		cflag |= B9600;
		break;
	}
	switch (bits) {
	case 7:
		cflag |= CS7;
		break;
	default:
		cflag |= CS8;
		break;
	}
	switch (parity) {
	case 'o':
	case 'O':
		cflag |= PARODD;
		break;
	case 'e':
	case 'E':
		cflag |= PARENB;
		break;
	}
	co->cflag = cflag;
	rs285_set_cflag(cflag);
	rs285_console_write(NULL, "\e[2J\e[Hboot ", 12);
	if (options)
		rs285_console_write(NULL, options, strlen(options));
	else
		rs285_console_write(NULL, "no options", 10);
	rs285_console_write(NULL, "\n", 1);

	return 0;
}

#ifdef CONFIG_SERIAL_21285_OLD
static struct console rs285_old_cons =
{
	SERIAL_21285_OLD_NAME,
	rs285_console_write,
	NULL,
	rs285_console_device,
	NULL,
	rs285_console_setup,
	CON_PRINTBUFFER,
	-1,
	0,
	NULL
};
#endif

static struct console rs285_cons =
{
	name:		SERIAL_21285_NAME,
	write:		rs285_console_write,
	device:		rs285_console_device,
	setup:		rs285_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};

void __init rs285_console_init(void)
{
#ifdef CONFIG_SERIAL_21285_OLD
	if (!machine_is_ebsa285() && !machine_is_netwinder())
		register_console(&rs285_old_cons);
#endif
	register_console(&rs285_cons);
}

#endif /* CONFIG_SERIAL_21285_CONSOLE */

EXPORT_NO_SYMBOLS;

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel Footbridge (21285) serial driver");
