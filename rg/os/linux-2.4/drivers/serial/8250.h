/*
 *  linux/drivers/serial/8250.h
 *
 *  Driver for 8250/16550-type serial ports
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Copyright (C) 2001 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *  $Id: 8250.h,v 1.1.1.1 2007/05/07 23:29:50 jungo Exp $
 */

#include <linux/config.h>

struct serial8250_probe {
	struct module	*owner;
	int		(*pci_init_one)(struct pci_dev *dev);
	void		(*pci_remove_one)(struct pci_dev *dev);
	void		(*pnp_init)(void);
};

int serial8250_register_probe(struct serial8250_probe *probe);
void serial8250_unregister_probe(struct serial8250_probe *probe);
void serial8250_get_irq_map(unsigned int *map);

struct old_serial_port {
	unsigned int uart;
	unsigned int baud_base;
	unsigned int port;
	unsigned int irq;
	unsigned int flags;
	unsigned char hub6;
	unsigned char io_type;
	unsigned char *iomem_base;
	unsigned short iomem_reg_shift;
};

struct serial8250_config {
	const char	*name;
	unsigned int	dfl_xmit_fifo_size;
	unsigned int	flags;
};

#define UART_CLEAR_FIFO		0x01
#define UART_USE_FIFO		0x02
#define UART_STARTECH		0x04
#define UART_NATSEMI		0x08
#define UART_MCRAFE		0x10	/* TI16C750-style auto-flow */
#define UART_EFRAFE		0x20	/* TI16C752/startech auto-flow */

#define UART_BAD_TX_ENABLE	0x80000000

#if defined(__i386__) && (defined(CONFIG_M386) || defined(CONFIG_M486))
#define SERIAL_INLINE
#endif
  
#ifdef SERIAL_INLINE
#define _INLINE_ inline
#else
#define _INLINE_
#endif

#define PROBE_RSA	(1 << 0)
#define PROBE_ANY	(~0)

#define HIGH_BITS_OFFSET ((sizeof(long)-sizeof(int))*8)

#ifdef CONFIG_SERIAL_8250_SHARE_IRQ
#define SERIAL8250_SHARE_IRQS 1
#else
#define SERIAL8250_SHARE_IRQS 0
#endif

#if defined(__alpha__) && !defined(CONFIG_PCI)
/*
 * Digital did something really horribly wrong with the OUT1 and OUT2
 * lines on at least some ALPHA's.  The failure mode is that if either
 * is cleared, the machine locks up with endless interrupts.
 */
#define ALPHA_KLUDGE_MCR  (UART_MCR_OUT2 | UART_MCR_OUT1)
#else
#define ALPHA_KLUDGE_MCR 0
#endif
