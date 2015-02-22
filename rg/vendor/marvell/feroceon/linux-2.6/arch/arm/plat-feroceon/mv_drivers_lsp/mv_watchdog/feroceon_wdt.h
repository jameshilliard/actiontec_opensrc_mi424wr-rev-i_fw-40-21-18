/*
 * arch/arm/plat-feroceon/include/plat/feroceon_wdt.h
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __PLAT_FEROCEON_WDT_H
#define __PLAT_FEROCEON_WDT_H

struct feroceon_wdt_platform_data {
	u32	tclk;		/* no <linux/clk.h> support yet */
};

#endif

