/*
 * linux/include/asm-arm/arch-ix2000/timex.h
 *
 * IXP2000 architecture timex specifications
 */


/*
 * This is somewhat bogus, but we need something to make <linux/timex.h>
 * happy. Each board can have a different timer tick rate, so we 
 * determine the * _real_ latch at run time.
 */
#define CLOCK_TICK_RATE (50000000)	/* 50MHz APB clock - OK for most */
