/*
 *  system.h 
 *
 *  Copyright (C) 2002 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/hardware.h>

static inline void arch_idle(void)
{
#if 0
	if (!hlt_counter)
		cpu_do_idle(0);
#endif
}

#ifdef CONFIG_ARCH_IXP425_NAPA
#define IXP425_GPIO_RESET_PIN IXP425_GPIO_PIN_8
#elif defined (CONFIG_ARCH_IXP425_MI424WR) || \
    defined(CONFIG_ARCH_IXP425_VI414WG) || \
    defined(CONFIG_ARCH_IXP425_KI414WG)
#define IXP425_GPIO_RESET_PIN IXP425_GPIO_PIN_0
#else
#undef IXP425_GPIO_RESET_PIN
#endif

static inline void arch_reset(char mode)
{
#ifdef IXP425_GPIO_RESET_PIN
	gpio_line_config(IXP425_GPIO_RESET_PIN, IXP425_GPIO_OUT);
	gpio_line_set(IXP425_GPIO_RESET_PIN, 1);
	udelay(1000);
	gpio_line_set(IXP425_GPIO_RESET_PIN, 0);
#else
	if (mode == 's') {
		/* Jump into ROM at address 0 */
		cpu_reset(0);
	} else {
		/* Use on-chip reset capability */

		/* set the "key" register to enable access to
		 * "timer" and "enable" registers
		 */
		*IXP425_OSWK = 0x482e; 	    

		/* write 0 to the timer register for an immidiate reset */
		*IXP425_OSWT = 0;

		/* disable watchdog interrupt, enable reset, enable count */
		*IXP425_OSWE = 0x5;
	}
#endif
}

