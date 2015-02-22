/*
 * arch/arm/mach-ixp2000/ixdp2800-leds.c
 *
 * Code to manipulate led display on IXDP2800
 *
 * Author: Jeff Daly <jeffrey.daly@intel.com>
 *	Modified from arch/arm/mach-footbridge/ebsa285-leds.c
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>

#include <asm/hardware.h>
#include <asm/leds.h>
#include <asm/mach-types.h>
#include <asm/system.h>

#define LED_STATE_ENABLED	1
#define LED_STATE_CLAIMED	2
static char led_state;
static char hw_led_state0;
static char hw_led_state1 = 0x1;
static char hw_led_state2 = ' ';
static char hw_led_state3 = ' ';

static char spinner[] = {'|','/','-','\\'};
static int spincnt = 0;

static spinlock_t leds_lock = SPIN_LOCK_UNLOCKED;

static void ixdp2800_leds_event(led_event_t evt)
{
	unsigned long flags;

	spin_lock_irqsave(&leds_lock, flags);

	switch (evt) {
	case led_start:
		led_state |= LED_STATE_ENABLED;
		break;

	case led_stop:
		led_state &= ~LED_STATE_ENABLED;
		break;

	case led_claim:
		led_state |= LED_STATE_CLAIMED;
		break;

	case led_release:
		led_state &= ~LED_STATE_CLAIMED;
		break;

#ifdef CONFIG_LEDS_TIMER
	case led_timer:
		if (!(led_state & LED_STATE_CLAIMED))
		{
			hw_led_state0 = spinner[spincnt];
			spincnt++;
			spincnt %= 4;
		}
		break;
#endif

#ifdef CONFIG_LEDS_CPU
	case led_idle_start:
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_state1 = ' ';
		break;

	case led_idle_end:
		if (!(led_state & LED_STATE_CLAIMED))
		hw_led_state1 = '*';
		break;
#endif

	case led_halted:
		if (!(led_state & LED_STATE_CLAIMED))
			hw_led_state0 = 'H';
			hw_led_state1 = 'A';
			hw_led_state2 = 'L';
			hw_led_state3 = 'T';
		break;

	default:
		break;
	}

	if  (led_state & LED_STATE_ENABLED)
	{
		*IXDP2800_LED0_REG = hw_led_state0;
		*IXDP2800_LED1_REG = hw_led_state1;
		*IXDP2800_LED2_REG = hw_led_state2;
		*IXDP2800_LED3_REG = hw_led_state3;
	}

	spin_unlock_irqrestore(&leds_lock, flags);
}

static int __init leds_init(void)
{
	/* clear leds */
	*IXDP2800_LED0_REG = ' ';
	*IXDP2800_LED1_REG = ' ';
	*IXDP2800_LED2_REG = ' ';
	*IXDP2800_LED3_REG = ' ';

	leds_event = ixdp2800_leds_event;

	leds_event(led_start);

	return 0;
}

__initcall(leds_init);
