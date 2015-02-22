/*
 * linux/include/asm-arm/arch-ixp425/timex.h
 *
 * XScale architecture timex specifications
 */

/*
 * We use IXP425 General purpose timer for our timer needs, it runs at 66 MHz
 */
#ifdef CONFIG_ARCH_IXP425_MONTEJADE
#define CLOCK_TICK_RATE (66000000)
#else
#define CLOCK_TICK_RATE (66666000)
#endif
