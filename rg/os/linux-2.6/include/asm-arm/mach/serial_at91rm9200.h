/*
 *  linux/include/asm-arm/mach/serial_at91rm9200.h
 *
 *  Based on serial_sa1100.h  by Nicolas Pitre
 *
 *  Copyright (C) 2002 ATMEL Rousset
 *
 *  Low level machine dependent UART functions.
 */
#include <linux/config.h>

struct uart_port;

/*
 * This is a temporary structure for registering these
 * functions; it is intended to be discarded after boot.
 */
struct at91rm9200_port_fns {
	void	(*set_mctrl)(struct uart_port *, u_int);
	u_int	(*get_mctrl)(struct uart_port *);
	void	(*enable_ms)(struct uart_port *);
	void	(*pm)(struct uart_port *, u_int, u_int);
	int	(*set_wake)(struct uart_port *, u_int);
	int	(*open)(struct uart_port *);
	void	(*close)(struct uart_port *);
};

#if defined(CONFIG_SERIAL_AT91)
void at91_register_uart_fns(struct at91rm9200_port_fns *fns);
void at91_register_uart(int idx, int port);
#else
#define at91_register_uart_fns(fns) do { } while (0)
#define at91_register_uart(idx,port) do { } while (0)
#endif


