/*
 * uncompress.h 
 *
 *  Copyright (C) 2002 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _ARCH_UNCOMPRESS_H_
#define _ARCH_UNCOMPRESS_H_

#include <asm/hardware.h>
#include <linux/serial_reg.h>

#define UART_BASE	((volatile u32*)IXP425_CONSOLE_UART_BASE_PHYS)
#define TX_DONE		(UART_LSR_TEMT | UART_LSR_THRE)

static __inline__ void putc(char c)
{
	/* Check THRE and TEMT bits before we transmit the character */
	while ((UART_BASE[UART_LSR] & TX_DONE) != TX_DONE); 
	*UART_BASE = c;
}

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
	while (*s)
	{
		putc(*s);
		if (*s == '\n')
			putc('\r');
		s++;
	}
}

/*
*  #   regaddr	byte	REG
*  -------------------------------
*  0   0	3	RBR/THR/DLL
*  1   4	7	IER/DLM
*  2   8	B	IIR/FCR
*  3   C	F	LCR
*  4	10	13	MCR
*  5	14	17	LSR
*  6	18	1B	MSR
*  7	1C	1F	SPR
*  8	20	23	ISR
*/

#include <linux/serial_reg.h>

static void arch_decomp_setup()
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

#if defined(CONFIG_ARCH_IXP425_IXDP425) || defined(CONFIG_ARCH_IXP425_MATECUMBE)
	*(unsigned long volatile *)(0xc4000008) = 0xBfff0002;
	*(unsigned short volatile *)(0x52000000) = 0x1234;
#endif
}

#define arch_decomp_wdog()

#endif
