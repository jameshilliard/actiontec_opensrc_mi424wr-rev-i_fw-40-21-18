/*
 * linux/include/asm-arm/arch-ixp2000/uncompress.h
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * 3/27/03: Jeff Daly <jeffrey.daly@intel.com> 
 *	Modified to support multiple machine types
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

/* At this point, the MMU is not on, so use physical addresses */


#define UART_BASE	0xc0030000

#ifdef __ARMEB__
#define PHYS(x)          ((volatile unsigned char *)(UART_BASE + x + 3))
#else
#define PHYS(x)          ((volatile unsigned char *)(UART_BASE + x))
#endif

#define UARTDR          PHYS(0x00)      /* Transmit reg dlab=0 */
#define UARTDLL         PHYS(0x00)      /* Divisor Latch reg dlab=1*/
#define UARTDLM         PHYS(0x04)      /* Divisor Latch reg dlab=1*/
#define UARTIER         PHYS(0x04)      /* Interrupt enable reg */
#define UARTFCR         PHYS(0x08)      /* FIFO control reg dlab =0*/
#define UARTLCR         PHYS(0x0c)      /* Control reg */
#define UARTSR          PHYS(0x14)      /* Status reg */

/*
 * The following code assumes the serial port has already been
 * initialized by the bootloader or such...
 */

#define THRE 0x20 /* bit5=1, means transmit holding reg empty */

static void puts( const char *s )
{
	int i,j;

	for (i = 0; *s; i++, s++) {
		/* wait for space in the UART's transmiter */
		j = 0x1000;
		while (--j && !(*UARTSR & THRE));
		/* if a LF, also do CR... */
		if (*s == '\n') {
			/* send the CR character out. */
			*UARTDR = '\r';
			/* wait for space in the UART's transmiter */
			j = 0x1000;
			while (--j && !(*UARTSR & THRE));
		}
		/* send the character out. */
		*UARTDR = *s;
	}
}

static void arch_decomp_setup()
{
	/* UART init should be done by bootmonitor code */
}

#define arch_decomp_wdog()
