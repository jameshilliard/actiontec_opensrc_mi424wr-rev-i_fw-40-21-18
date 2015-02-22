/*
 *  linux/include/asm-arm/arch-iop80310/uncompress.h
 */

#include <linux/serial_reg.h>

#ifdef CONFIG_ARCH_IQ80310
#define UART_BASE    ((volatile unsigned char *)0xfe810000)
#elif defined(CONFIG_ARCH_IQ80321) || defined (CONFIG_ARCH_IQ31244)
#define UART_BASE    ((volatile unsigned char *)0xfe800000)
#endif

static __inline__ void putc(char c)
{
	while ((UART_BASE[5] & 0x60) != 0x60);
	UART_BASE[0] = c;
}

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
	while (*s) {
		putc(*s);
		if (*s == '\n')
			putc('\r');
		s++;
	}
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
