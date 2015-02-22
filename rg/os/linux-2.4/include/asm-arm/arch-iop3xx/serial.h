/*
 * include/asm-arm/arch-iop3xx/serial.h
 */

/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD ( 1843200 / 16 )

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#ifdef CONFIG_ARCH_IQ80310

#define IRQ_UART1	IRQ_IQ80310_UART1
#define IRQ_UART2	IRQ_IQ80310_UART2

#define RS_TABLE_SIZE 2

#define STD_SERIAL_PORT_DEFNS			\
	{ 					\
	 magic: 0, 				\
	 baud_base: BASE_BAUD, 			\
	 irq: IRQ_UART2, 			\
         flags: STD_COM_FLAGS,  		\
	 iomem_base: 0xfe810000, 		\
	 io_type: SERIAL_IO_MEM			\
	}, /* ttyS0 */				\
	{ 					\
	 magic: 0, 				\
	 baud_base: BASE_BAUD, 			\
	 irq: IRQ_UART1, 			\
         flags: STD_COM_FLAGS,  		\
	 iomem_base: 0xfe800000, 		\
	 io_type: SERIAL_IO_MEM			\
	} /* ttyS0 */

#endif // CONFIG_ARCH_IQ80310

#if defined(CONFIG_ARCH_IQ80321) || defined(CONFIG_ARCH_IQ31244)

#define RS_TABLE_SIZE 1

#define STD_SERIAL_PORT_DEFNS			\
	{					\
		magic: 0,			\
		baud_base: BASE_BAUD,		\
		irq: IRQ_IOP321_XINT1,		\
		flags: STD_COM_FLAGS,		\
		iomem_base: 0xfe800000,		\
		io_type: SERIAL_IO_MEM		\
	} /* ttyS0 */
#endif

#define EXTRA_SERIAL_PORT_DEFNS

