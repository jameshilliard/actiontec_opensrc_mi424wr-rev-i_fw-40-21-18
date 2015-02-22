/*
 * linux/arch/arm/mach-sa1100/leds-system3.c
 *
 * Copyright (C) 2001 Stefan Eletzhofer <stefan.eletzhofer@gmx.de>
 *
 * Original (leds-footbridge.c) by Russell King
 *
 * $Id: leds-system3.c,v 1.1.1.1 2007/05/07 23:29:18 jungo Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * $Log: leds-system3.c,v $
 * Revision 1.1.1.1  2007/05/07 23:29:18  jungo
 * Import of Jungo RG SDK 4.6.2
 *
 * Revision 1.2  2003/09/21 19:58:58  igork
 * merge branch-dev-2421 into dev
 *
 * Revision 1.1.66.1  2003/09/18 21:33:26  igork
 * linux-2.4.21-rmk1 integration
 *
 * Revision 1.1.6.1  2001/12/04 15:19:26  seletz
 * - merged from linux_2_4_13_ac5_rmk2
 *
 * Revision 1.1.4.2  2001/11/19 17:58:53  seletz
 * - cleanup
 *
 * Revision 1.1.4.1  2001/11/16 13:49:54  seletz
 * - dummy LED support for PT Digital Board
 *
 * Revision 1.1.2.1  2001/10/15 16:03:39  seletz
 * - dummy function
 *
 *
 */
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/leds.h>
#include <asm/system.h>

#include "leds.h"


#define LED_STATE_ENABLED	1
#define LED_STATE_CLAIMED	2

static unsigned int led_state;
static unsigned int hw_led_state;

void system3_leds_event(led_event_t evt)
{
	/* TODO: support LEDs */
}
