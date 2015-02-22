/*
 * arch/arm/kernel/kgdb-serial.c
 *
 * Generic serial access routines for use by kgdb. 
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 * Based on original KGDB code developed by various people:
 *
 * This is a simple middle layer that allows KGDB to run on
 * top of any serial device.  This layer expects the board
 * port to provide the following functions:
 *
 * 	void kgdb_serial_init(void) 
 * 		initialize serial interface if needed
 * 	void kgdb_serial_putchar(unsigned char) 
 * 		send a character over the serial connection
 * 	kgdb_serial_getchar() 
 * 		get a character from the serial connection
 *
 * Note that this is not meant for debugging over sercons, but for
 * when you have a _dedicated_ serial port for kgdb.  To send debug
 * and console messages over the same port (ICK), you need to
 * turn on CONFIG_KGDB_CONSOLE, which uses kgb-console.c instead and
 * hooks into the console system.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/elf.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include <asm/kgdb.h>

#ifdef CONFIG_KGDB_CONSOLE
#include <linux/console.h>
#endif

static const char hexchars[]="0123456789abcdef";

static int 
hex(unsigned char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}

/* 
 * 
 * Scan the serial stream for the sequence $<data>#<checksum> 
 *
 */
void
kgdb_get_packet(unsigned char *buffer, int bufsize)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	unsigned char ch;

	do {
		/* wait around for the start character, ignore all other
		 * characters */
		while ((ch = (kgdb_serial_getchar() & 0x7f)) != '$') ;

		checksum = 0;
		xmitcsum = -1;

		count = 0;

		/* now, read until a # or end of buffer is found */
		while (count < bufsize) {
			ch = kgdb_serial_getchar() & 0x7f;
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}

		if (count >= bufsize)
			continue;

		buffer[count] = 0;

		if (ch == '#') {
			xmitcsum = hex(kgdb_serial_getchar() & 0x7f) << 4;
			xmitcsum |= hex(kgdb_serial_getchar() & 0x7f);
			if (checksum != xmitcsum)
				kgdb_serial_putchar('-');	/* failed checksum */
			else {
				/* successful transfer */
				kgdb_serial_putchar('+'); 

				/* if a sequence char is present, reply the ID */
				if (buffer[2] == ':') {
					kgdb_serial_putchar(buffer[0]);
					kgdb_serial_putchar(buffer[1]);
					/* remove sequence chars from buffer */
					count = strlen(buffer);
					for (i=3; i <= count; i++)
						buffer[i-3] = buffer[i];
				}
			}
		}
	} while (checksum != xmitcsum);
}


/* 
 * Send the following to GDB over a serial connection:
 *
 * $<packet info>#<checksum>. 
 */
void 
kgdb_put_packet(unsigned char *buffer)
{
	unsigned char checksum;
	int count;
	unsigned char ch, recv;

	do {
		kgdb_serial_putchar('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count])) {
			kgdb_serial_putchar(ch);
			checksum += ch;
			count += 1;
		}

		kgdb_serial_putchar('#');
		kgdb_serial_putchar(hexchars[checksum >> 4]);
		kgdb_serial_putchar(hexchars[checksum & 0xf]);
		recv = kgdb_serial_getchar();
	} while ((recv & 0x7f) != '+');
}

int
kgdb_io_init(void)
{
	kgdb_serial_init();
	return 0;
}


/*
 * If only one serial port is available on the board or if you want
 * to send console messages over KGDB, this is a nice and easy way
 * to accomplish this. Just add "console=ttyKGDB" to the command line
 * and all console output will be piped to the GDB client.
 */
#ifdef CONFIG_KGDB_CONSOLE

static void 
kgdb_console_write(struct console *co, const char *s, unsigned count)
{
	int i = 0;
	static int j = 0;
	char buf[4096];

	if(!kgdb_connected())
		return;

	buf[0] = 'O';
	j = 1;

	for(i = 0; i < count; i++, s++)
	{
		buf[j++] = hexchars[*s >> 4];
		buf[j++] = hexchars[*s & 0xf];

		if(*s == '\n')
		{
			buf[j] = 0;
			kgdb_put_packet(buf);

			buf[0]='O';
			j = 1;
		}
	}

	if(j != 1) 
	{
		buf[j] = 0;
		kgdb_put_packet(buf);
	}
}

static kdev_t
kgdb_console_device(struct console *c)
{
	return 0;
}

static int 
kgdb_console_setup(struct console *co, char *options)
{
	kgdb_io_init();

	return 0;
}

static struct console kgdb_console = {
	name:		"ttyKGDB",
	write:		kgdb_console_write,
	device:		kgdb_console_device,
	setup:		kgdb_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1
};

void __init 
kgdb_console_init(void)
{
	register_console(&kgdb_console);
}

#endif // CONFIG_KGDB_CONSOLE
