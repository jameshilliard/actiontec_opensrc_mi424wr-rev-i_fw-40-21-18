/*
 * arch/arm/mach-versatile/kgdb_serial.c
 *
 * Author: Manish Lachwani, mlachwani@mvista.com
 *
 * 2005 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 * Support for KGDB on ARM Versatile.
 */
#include <linux/config.h>
#include <linux/serial_reg.h>
#include <linux/kgdb.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/hardware.h>
#include <asm/hardware/amba_serial.h>
#include <asm/arch-versatile/hardware.h>

#define ARM_BAUD_38400		23
/*
 * Functions that will be used later
 */
#define UART_GET_INT_STATUS(p)	readb((p) + UART010_IIR)
#define UART_GET_MIS(p)		readw((p) + UART011_MIS)
#define UART_PUT_ICR(p, c)	writel((c), (p) + UART010_ICR)
#define UART_GET_FR(p)		readb((p) + UART01x_FR)
#define UART_GET_CHAR(p)	readb((p) + UART01x_DR)
#define UART_PUT_CHAR(p, c)     writel((c), (p) + UART01x_DR)
#define UART_GET_RSR(p)		readb((p) + UART01x_RSR)
#define UART_GET_CR(p)		readb((p) + UART010_CR)
#define UART_PUT_CR(p,c)        writel((c), (p) + UART010_CR)
#define UART_GET_LCRL(p)	readb((p) + UART010_LCRL)
#define UART_PUT_LCRL(p,c)	writel((c), (p) + UART010_LCRL)
#define UART_GET_LCRM(p)        readb((p) + UART010_LCRM)
#define UART_PUT_LCRM(p,c)	writel((c), (p) + UART010_LCRM)
#define UART_GET_LCRH(p)	readb((p) + UART010_LCRH)
#define UART_PUT_LCRH(p,c)	writel((c), (p) + UART010_LCRH)
#define UART_RX_DATA(s)		(((s) & UART01x_FR_RXFE) == 0)
#define UART_TX_READY(s)	(((s) & UART01x_FR_TXFF) == 0)
#define UART_TX_EMPTY(p)	((UART_GET_FR(p) & UART01x_FR_TMSK) == 0)

/*
 * KGDB IRQ
 */
static int kgdb_irq = 12;
static volatile unsigned char *port = NULL;

static int kgdb_serial_init(void)
{
	int rate = ARM_BAUD_38400;

	port = IO_ADDRESS(0x101F1000);
	UART_PUT_CR(port, 0);

	/* Set baud rate */
	UART_PUT_LCRM(port, ((rate & 0xf00) >> 8));
	UART_PUT_LCRL(port, (rate & 0xff));
	UART_PUT_LCRH(port, UART01x_LCRH_WLEN_8 | UART01x_LCRH_FEN);
	UART_PUT_CR(port, UART01x_CR_UARTEN);

	return 0;
}

static void kgdb_serial_putchar(int ch)
{
	unsigned int status;

	do {
		status = UART_GET_FR(port);
	} while (!UART_TX_READY(status));

	UART_PUT_CHAR(port, ch);
}

static int kgdb_serial_getchar(void)
{
	unsigned int status;
	int ch;

	do {
		status = UART_GET_FR(port);
	} while (!UART_RX_DATA(status));
	ch = UART_GET_CHAR(port);
	return ch;
}

static struct uart_port kgdb_amba_port = {
	.irq = 12,
	.iobase = 0,
	.iotype = UPIO_MEM,
	.membase = (unsigned char *)IO_ADDRESS(0x101F1000),
};

static irqreturn_t kgdb_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int status = UART_GET_MIS(port);

	if (irq != kgdb_irq)
		return IRQ_NONE;

	if (status & 0x40)
		breakpoint();

	return IRQ_HANDLED;
}

static void __init kgdb_hookup_irq(void)
{
	request_irq(kgdb_irq, kgdb_interrupt, SA_SHIRQ, "GDB-stub",
		    &kgdb_amba_port);
}

struct kgdb_io kgdb_io_ops = {
	.init = kgdb_serial_init,
	.write_char = kgdb_serial_putchar,
	.read_char = kgdb_serial_getchar,
	.late_init = kgdb_hookup_irq,
};
