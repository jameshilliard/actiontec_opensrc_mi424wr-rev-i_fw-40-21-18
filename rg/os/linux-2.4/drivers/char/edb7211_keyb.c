/*
 * drivers/char/edb7211_keyb.c
 *
 * Copyright (C) 2000 Blue Mug, Inc.  All Rights Reserved.
 *
 * EDB7211 Keyboard driver for ARM Linux.
 *
 * The EP7211 keyboard hardware only supports generating interrupts for 64 keys.
 * The EBD7211's keyboard has 84 keys. Therefore we need to poll for keys,
 * instead of waiting for interrupts.
 *
 * In a real-world hardware situation, this would be a bad thing. It would
 * kill power management.
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/ptrace.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/tqueue.h>
#include <linux/random.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/kbd_kern.h>
#include <linux/delay.h>

#include <asm/bitops.h>
#include <asm/keyboard.h>
#include <asm/irq.h>
#include <asm/hardware.h>

#include <asm/io.h>
#include <asm/system.h>


/*
 * The number of jiffies between keyboard scans.
 */
#define KEYBOARD_SCAN_INTERVAL	5

/*
 * Values for the keyboard column scan control register.
 */
#define KBSC_HI	    0x0	    /*   All driven high */
#define KBSC_LO	    0x1	    /*   All driven low */
#define KBSC_X	    0x2	    /*   All high impedance */
#define KBSC_COL0   0x8	    /*   Column 0 high, others high impedance */
#define KBSC_COL1   0x9	    /*   Column 1 high, others high impedance */
#define KBSC_COL2   0xa	    /*   Column 2 high, others high impedance */
#define KBSC_COL3   0xb	    /*   Column 3 high, others high impedance */
#define KBSC_COL4   0xc	    /*   Column 4 high, others high impedance */
#define KBSC_COL5   0xd	    /*   Column 5 high, others high impedance */
#define KBSC_COL6   0xe	    /*   Column 6 high, others high impedance */
#define KBSC_COL7   0xf	    /*   Column 7 high, others high impedance */


/* XXX: Figure out what these values should be... */
/* Simple translation table for the SysRq keys */
#ifdef CONFIG_MAGIC_SYSRQ
unsigned char edb7211_kbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
	"\r\000/";					/* 0x60 - 0x6f */
#endif

/* 
 * Row/column to scancode mappings.
 *
 * This table maps row/column keyboard matrix positions to XT scancodes.
 * 
 * The port A rows come first, followed by the extended rows.
 */
static unsigned char colrow_2_scancode[128] = 
{
/*  Column: 
  Row       0     1     2     3     4     5     6     7   */
/* A0 */  0x01, 0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x40, 0x41,
/* A1 */  0x02, 0x07, 0x06, 0x05, 0x04, 0x03, 0x08, 0x09,
/* A2 */  0x0f, 0x14, 0x13, 0x12, 0x11, 0x10, 0x15, 0x16,
/* A3 */  0x3a, 0x22, 0x21, 0x20, 0x1f, 0x1e, 0x23, 0x24,
/* A4 */  0x29, 0x30, 0x2f, 0x2e, 0x2d, 0x2c, 0x31, 0x32,
/* A5 */  0x39, 0x35, 0x6F, 0x52, 0x00, 0x6B, 0x34, 0x33,
/* A6 */  0x6A, 0x27, 0x28, 0x00, 0x1c, 0x6D, 0x26, 0x25,
/* A7 */  0x67, 0x19, 0x1a, 0x1b, 0x2b, 0x68, 0x18, 0x17,
/* E0 */  0x6C, 0x0c, 0x0d, 0x0e, 0x00, 0x66, 0x0b, 0x0a,
/* E1 */  0x69, 0x44, 0x45, 0x37, 0x46, 0x77, 0x43, 0x42,
/* E2 */  0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* E3 */  0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* E4 */  0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* E5 */  0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* E6 */  0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* E7 */  0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * A bitfield array which contains the state of the keyboard after the last
 * scan. A bit set in this array corresponds to a key down. Only the lower
 * 16 bits of each array element are used.
 */
static unsigned long previous_keys[8];
static unsigned long keys[8];


/* This will be set to a non-zero value if a key was found to be pressed
 * in the last scan. */
static int key_is_pressed;

static struct tq_struct kbd_process_task;
static struct timer_list edb7211_kbd_timer;

/*
 * External methods.
 */
void edb7211_kbd_init_hw(void);

/* 
 * Internal methods.
 */
static int edb7211_kbd_scan_matrix(u_long* keys);
static void edb7211_kbd_timeout(unsigned long data);
static void edb7211_kbd_process(void* data);

/*
 * Translate a raw keycode to an XT keyboard scancode.
 */
static int
edb7211_translate(unsigned char scancode, unsigned char *keycode,
		  char raw_mode)
{
	*keycode = colrow_2_scancode[scancode & 0x7f];
	return 1;
}

/*
 * Scan the keyboard matrix; for each key that is pressed, set the
 * corresponding bit in the bitfield array.
 *
 * The parameter is expected to be an array of 8 32-bit values. Only the lower
 * 16 bits of each value is used. Each value contains the row bits for the
 * corresponding column.
 */
static int
edb7211_kbd_scan_matrix(u_long* keys)
{
	int column, row, key_pressed;
	unsigned char port_a_data, ext_port_data;

	key_pressed = 0;

	/* Drive all the columns low. */
	clps_writel((clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK) | KBSC_LO, 
		SYSCON1);

	for (column = 0; column < 8; column++) {

		/* Drive the column high. */
		clps_writel((clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK) | 
			    (KBSC_COL0 + column), SYSCON1);

		/* Read port A and the extended port. */
		port_a_data = clps_readb(PADR) & 0xff;
		ext_port_data = __raw_readb(EP7211_VIRT_EXTKBD) & 0xff;

		/* Drive all columns tri-state. */
		clps_writel((clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK) | KBSC_X, 
			SYSCON1);

		/* Look at each column in port A. */
		for (row=0; row < 8; row++) {
			/* If the row's bit is set, set the bit in the bitfield.
			 * Otherwise, clear it. 
			 */
			if (port_a_data & (1 << row)) {
				keys[column] |= (1 << row); 
				key_pressed = 1;
			} else {
				keys[column] &= ~(1 << row); 
			}
		}

		/* Look at each column in the extended port. */
		for (row=0; row < 8; row++) {
			/* If the row's bit is set, set the bit in the bitfield.
			 * Otherwise, clear it. 
			 */
			if (ext_port_data & (1 << row)) {
				keys[column] |= (1 << (row + 8)); 
				key_pressed = 1;
			} else {
				keys[column] &= ~(1 << (row + 8)); 
			}
		}

		/* 
		 * Short delay: The example code for the EDB7211 runs an empty
		 * loop 256 times. At this rate, there were some spurious keys
		 * generated. I doubled the delay to let the column drives 
		 * settle some. 
		 */
		for (row=0; row < 512; row++) { }
	}

	/* If we could use interrupts, we would drive all columns high so 
	 * that interrupts will be generated on key presses. But we can't,
	 * so we leave all columns floating. 
	 */
	clps_writel((clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK) | KBSC_X, 
		SYSCON1);

	return key_pressed;
}

/* 
 * XXX: This is really ugly; this needs to be reworked to have less levels of
 *   	indentation.
 */
static void
edb7211_kbd_timeout(unsigned long data)
{
	/* Schedule the next timer event. */
	edb7211_kbd_timer.expires = jiffies + KEYBOARD_SCAN_INTERVAL;
	add_timer(&edb7211_kbd_timer);

	if (edb7211_kbd_scan_matrix(keys) || key_is_pressed) {
		queue_task(&kbd_process_task, &tq_timer);
	} else {
		key_is_pressed = 0;
	}
}

/*
 * Process the keys that have been pressed. 
 */
static void
edb7211_kbd_process(void* data)
{
	int i;

	/* First check if any keys have been released. */
	if (key_is_pressed) {
		for (i=0; i < 8; i++) {
			if (previous_keys[i]) {
				int row;

				for (row=0; row < 16; row++) {
					if ((previous_keys[i] & (1 << row)) &&
						!(keys[i] & (1 << row))) {
						/* Generate the up event. */
						handle_scancode(
								(row<<3)+i, 0);
					}
				}
			}
		}
	}

	key_is_pressed = 0;

	/* Now scan the keys and send press events. */
	for (i=0; i < 8; i++) {
		if (keys[i]) {
			int row;

			for (row=0; row < 16; row++) {
				if (keys[i] & (1 << row)) {
					if (previous_keys[i] & (1 << row)) {
						/* Generate the hold event. */
						handle_scancode((row<<3)+i, 1);
					} else {
						/* Generate the down event. */
						handle_scancode((row<<3)+i, 1);
					}

					key_is_pressed = 1;
				}
			}
		}
	}

	/* Update the state variables. */
	memcpy(previous_keys, keys, 8 * sizeof(unsigned long));
}

static char edb7211_unexpected_up(unsigned char scancode)
{
	return 0200;
}

static void edb7211_leds(unsigned char leds)
{
}

/*
 * Initialize the keyboard hardware. Set the column drives low and
 * start the timer.
 */
void __init
edb7211_kbd_init_hw(void)
{
	k_translate	= edb7211_translate;
	k_unexpected_up	= edb7211_unexpected_up;
	k_leds		= edb7211_leds;

	/* 
	 * If we had the ability to use interrupts, we would want to drive all
	 * columns high. But we have more keys than can generate interrupts, so
	 * we leave them floating.
	 */
	clps_writel((clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK) | KBSC_X, 
		SYSCON1);

	/* Initialize the matrix processing task. */
	kbd_process_task.routine = edb7211_kbd_process;
	kbd_process_task.data = NULL;

	/* Setup the timer to poll the keyboard. */
	init_timer(&edb7211_kbd_timer);
	edb7211_kbd_timer.function = edb7211_kbd_timeout;
	edb7211_kbd_timer.data = (unsigned long)NULL;
	edb7211_kbd_timer.expires = jiffies + KEYBOARD_SCAN_INTERVAL;
	add_timer(&edb7211_kbd_timer); 
}


