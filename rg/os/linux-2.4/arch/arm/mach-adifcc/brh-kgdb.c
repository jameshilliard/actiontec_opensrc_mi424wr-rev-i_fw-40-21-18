/*
 * arch/arm/kernel/brh-kgdb.h
 *
 * Low level kgdb code for the BRH board
 * TODO: This should be adapted to work on any 1655x UART
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

static struct serial_state ports[RS_TABLE_SIZE] = {
	SERIAL_PORT_DFNS
};

static short port = 0;
static volatile unsigned char *serial_base = NULL;

void kgdb_serial_init(void)
{

	// TODO: Make port number a config or cmdline option
	port = 1;

	serial_base = ports[port].iomem_base;	

	serial_base[UART_LCR] = 0x83;
	serial_base[UART_DLL] = 36;
	serial_base[UART_DLM] = 0;
	serial_base[UART_LCR] = 0x3;
	serial_base[UART_MCR] = 0;
	serial_base[UART_IER] = 0;
	serial_base[UART_FCR] = 0x7;

	return;
}

void kgdb_serial_putchar(unsigned char ch)
{
	unsigned char status;

	do
	{
		status = serial_base[UART_LSR];
	} while ((status & (UART_LSR_TEMT|UART_LSR_THRE)) !=
			(UART_LSR_TEMT|UART_LSR_THRE));

	*serial_base = ch;
}

unsigned char kgdb_serial_getchar(void)
{
	unsigned char ch;

	while(!(serial_base[UART_LSR] & 0x1));

	ch = *serial_base;

	return ch;
}

