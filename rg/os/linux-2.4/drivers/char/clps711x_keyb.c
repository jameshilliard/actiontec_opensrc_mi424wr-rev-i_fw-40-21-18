/*
 * drivers/char/clps711x_keyb.c
 *
 * Copyright (C) 2001 Thomas Gleixner <gleixner@autronix.de>
 *
 * based on drivers/edb7211_keyb.c, which is copyright (C) 2000 Bluemug Inc.
 * 
 * Keyboard driver for ARM Linux on EP7xxx and CS89712 processors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See the file COPYING 
 * in the main directory of this archive for more details.
 *
 *
 * Hardware: 
 *
 * matrix scan keyboards based on EP7209,7211,7212,7312 and CS89712 
 * on chip keyboard scanner.		
 * Adaption for different machines is done in init function.
 *
 * Basic Function:
 *
 * Basicly the driver is interrupt driven. It sets all column drivers	
 * high. If any key is pressed, a interrupt occures. Now a seperate scan of
 * each column is done. This scan is timer based, because we use a keyboard 
 * interface with decoupling capacitors (neccecary if you want to survive 
 * EMC compliance tests). Always one line is set high. When next timer event 
 * occures the scan data on port A are valid. This makes also sure, that no
 * spurious keys are scanned. The kbd int on these CPU's is not deglitched!
 * After scanning all columns, we switch back to int mode, if no key is
 * pressed. If any is pressed we reschedule the scan within a programmable
 * delay. If we would switch back to interrupt mode as long as a key is pressed,
 * we come right back to the interrupt, because the int. is level triggered !
 * The timer based scan of the seperate columns can also be done in one
 * timer event (set fastscan to 1).
 *
 * Summary:
 * The design of this keyboard controller chip is stupid at all !
 *
 * Matrix translation:
 * The matrix translation table is based on standard XT scancodes. Maybe
 * you have to adjust the KEYISPRINTABLE macro if you set other codes.
 *
 * HandyKey:
 *
 * On small matrix keyboards you don't have enough keys for operation.
 * The intention was to implement a operation mode as it's used on handys.
 * You can rotate trough four scancode levels and produce e.g. with a 4x3
 * matrix 4*3*4 = 48 different keycodes. That's basicly enough for editing
 * filenames or things like that. The HandyKey function takes care about 
 * nonprintable keys like cursors, backspace, del ...
 * If a key is pressed and is a printable keycode, the code is put to the
 * main keyboard handler and a cursor left is applied. If you press the same
 * key again, the current character is deleted and the next level character
 * is applied. (e.g. 1, a, b, c, 1 ....). If you press a different key, the
 * driver applies cursor right, before processing the new key.
 * The autocomplete feature moves the cursor right, if you do not press a
 * key within a programmable time.
 * If HandyKey is off, the keyboard behaviour is that of a standard keyboard
 * HandyKey can be en/disabled from userspace with the proc/keyboard entry
 * 
 * proc/keyboard:
 * 
 * Read access gives back the actual state of the HandyKey function	
 *	h:0	Disabled
 *	h:1	Enabled
 * Write access has two functions. Changing the HandyKey mode and applying
 * a different scancode translation table.
 * Syntax is: 	h:0	disable Handykey
 *		h:1	enabled Handykey
 *		t:array[256] of bytes	Transfer translation table	
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
#include <linux/proc_fs.h>

#include <asm/bitops.h>
#include <asm/keyboard.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>

#include <asm/io.h>
#include <asm/system.h>

void clps711x_kbd_init_hw(void);

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

/*
* Keycodes for cursor left/right and delete (used by HandyKey)	
*/
#define KEYCODE_CLEFT	0x4b
#define KEYCODE_CRIGHT  0x4d
#define KEYCODE_DEL	0x53
#define KEYISPRINTABLE(code) ( (code > 0x01 && code < 0x37 && code != 0x1c \
				 && code != 0x0e) || code == 0x39) 

/* Simple translation table for the SysRq keys */
#ifdef CONFIG_MAGIC_SYSRQ
unsigned char clps711x_kbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
	"\r\000/";					/* 0x60 - 0x6f */
#endif

/* 
 * This table maps row/column keyboard matrix positions to XT scancodes.
 * It's a default table, which can be overriden by writing to proc/keyboard 
 */
#ifdef CONFIG_ARCH_AUTCPU12
static unsigned char autcpu12_scancode[256] = 
{
/*  Column: 
  Row       0     1     2     3     4     5     6     7   */
/* A0 */  0x08, 0x09, 0x0a, 0x0e, 0x05, 0x06, 0x00, 0x00,
/* A1 */  0x07, 0x53, 0x02, 0x03, 0x04, 0x0f, 0x00, 0x00,
/* A2 */  0x0c, 0x0b, 0x33, 0x1c, 0xff, 0x4b, 0x00, 0x00,
/* A3 */  0x48, 0x50, 0x4d, 0x3b, 0x3c, 0x3d, 0x00, 0x00,
/* A4 */  0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A5 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A6 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A7 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

/* A0 */  0x1e, 0x20, 0x22, 0x0e, 0x24, 0x32, 0x00, 0x00,
/* A1 */  0x19, 0x53, 0x1f, 0x2f, 0x15, 0x0f, 0x00, 0x00,
/* A2 */  0x0c, 0x39, 0x34, 0x1c, 0xff, 0x4b, 0x00, 0x00,
/* A3 */  0x48, 0x50, 0x4d, 0x3b, 0x3c, 0x3d, 0x00, 0x00,
/* A4 */  0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A5 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A6 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A7 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

/* A0 */  0x30, 0x12, 0x23, 0x0e, 0x25, 0x31, 0x00, 0x00,
/* A1 */  0x10, 0x53, 0x14, 0x11, 0x2c, 0x0f, 0x00, 0x00,
/* A2 */  0x0c, 0x0b, 0x27, 0x1c, 0xff, 0x4b, 0x00, 0x00,
/* A3 */  0x48, 0x50, 0x4d, 0x3b, 0x3c, 0x3d, 0x00, 0x00,
/* A4 */  0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A5 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A6 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A7 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

/* A0 */  0x2e, 0x21, 0x17, 0x0e, 0x26, 0x18, 0x00, 0x00,
/* A1 */  0x13, 0x53, 0x16, 0x2D, 0x04, 0x0f, 0x00, 0x00,
/* A2 */  0x0c, 0x39, 0x35, 0x1c, 0xff, 0x4b, 0x00, 0x00,
/* A3 */  0x48, 0x50, 0x4d, 0x3b, 0x3c, 0x3d, 0x00, 0x00,
/* A4 */  0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A5 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A6 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* A7 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
#endif

static int keys[8];
static int new_keys[8];
static int previous_keys[8];

static int fastscan;
static int scan_interval;
static int scan_delay;
static int last_column; 
static int key_is_pressed;

static unsigned char *act_scancode;

static struct kbd_handy_key {
	int		ena;
	int		code;
	int		shift;
	int		autocomplete;
	unsigned long 	expires;
	unsigned long	delay;
	unsigned char 	left;
	unsigned char	right;
	unsigned char   del;
} khandy;

static struct tq_struct kbd_process_task;
static struct timer_list clps711x_kbd_timer;
static struct timer_list clps711x_kbdhandy_timer;
static struct proc_dir_entry *clps711x_keyboard_proc_entry = NULL;

/*
 * Translate a raw keycode to an XT keyboard scancode.
 */
static int clps711x_translate(unsigned char scancode, unsigned char *keycode,
		 		 char raw_mode)
{
	*keycode = act_scancode[scancode];
	return 1;
}

/*
* Initialize handykey structure
* clear code, clear shift
* scan scancode for cursor right/left and delete
*/
static void clps711x_handykey_init(void) {

	int	i;

	khandy.ena = 0;
	khandy.code = 0;
	khandy.shift = 0;
	khandy.autocomplete = 0;
	for(i = 0; i < 64; i++) {
		switch(act_scancode[i]) {
			case KEYCODE_CLEFT: 	khandy.left = i; break;	
			case KEYCODE_CRIGHT: 	khandy.right = i; break;	
			case KEYCODE_DEL: 	khandy.del = i; break;
		}
	}	
} 

/*
* Check for handy key and process it	
*/
void inline clps711x_checkhandy(int col, int row) {

	int scode, down;
	unsigned char kcode;
	
	scode = (row<<3) + col;
	down  = keys[col]>>row & 0x01;
	kcode = act_scancode[scode];
	
	if (!khandy.ena) {
		if (khandy.code) {			
			handle_scancode(khandy.right,1);
			handle_scancode(khandy.right,0);
		}
		khandy.code = 0;
		khandy.shift = 0;
		khandy.autocomplete = 0;
	}

	if(!kcode)
		return;

	if (!down || !khandy.ena) {
		if (khandy.ena && KEYISPRINTABLE(act_scancode[scode]))
			khandy.autocomplete = 1;
		else
			handle_scancode(scode + khandy.shift, down);
		return;	
	}
	
	khandy.autocomplete = 0;
	if (KEYISPRINTABLE(kcode)) {
		if (khandy.code) {	
			if(khandy.code == (scode|0x100)) {
				handle_scancode(khandy.del,1);
				handle_scancode(khandy.del,0);
				khandy.shift = khandy.shift < 3*64 ? khandy.shift + 64 : 0 ;
			} else {
				handle_scancode(khandy.right,1);
				handle_scancode(khandy.right,0);
				khandy.shift = 0;
			}			
		}
		handle_scancode(scode + khandy.shift, 1);
		handle_scancode(scode + khandy.shift, 0);
		khandy.code = scode | 0x100;
		handle_scancode(khandy.left,1);
		handle_scancode(khandy.left,0);
	} else {
		if (khandy.code) {
			khandy.code = 0;
			handle_scancode(khandy.right,1);
			handle_scancode(khandy.right,0);
		}
		khandy.shift = 0;
		handle_scancode(scode, down);
	}
}


/*
 * Process the new key data 
 */
static void clps711x_kbd_process(void* data)
{
	int col,row,res;

	for (col = 0; col < 8; col++) {
		if (( res = previous_keys[col] ^ keys[col]) == 0) 		
			continue;
		for(row = 0; row < 8; row++) {
			if ( ((res >> row) & 0x01) != 0) 
				clps711x_checkhandy(col,row);	
		}				
	}
	/* Update the state variables. */
	memcpy(previous_keys, keys, 8 * sizeof(int));

	/* reschedule, if autocomplete pending */
	if (khandy.autocomplete) {
		khandy.expires = jiffies + khandy.delay;
		mod_timer(&clps711x_kbdhandy_timer,khandy.expires);
	}

}

static char clps711x_unexpected_up(unsigned char scancode)
{
	return 0200;
}

/*
* Handle timer event, for autocomplete function
* Reschedule keyboard process task
*/
static void clps711x_kbdhandy_timeout(unsigned long data) 
{
	if(khandy.autocomplete) {
		khandy.code = 0;
		khandy.shift = 0;
		khandy.autocomplete = 0;
		handle_scancode(khandy.right,1);
		handle_scancode(khandy.right,0);
	}
}

/*
* Handle timer event, while in pollmode
*/
static void clps711x_kbd_timeout(unsigned long data)
{
	int i;
	unsigned long flags;
	/* 
	* read bits of actual column or all columns in fastscan-mode
	*/
	for (i = 0; i < 8; i++) {
		new_keys[last_column - KBSC_COL0] = clps_readb(PADR) & 0xff;
		key_is_pressed |= new_keys[last_column - KBSC_COL0];
		last_column = last_column < KBSC_COL7 ? last_column + 1 : KBSC_COL0;
		local_irq_save(flags);
		clps_writel( (clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK) 
				| last_column, SYSCON1);
		local_irq_restore(flags);
		/*
		* For fastscan, apply a short delay to settle scanlines
		* else break and wait for next timeout
		*/
		if (fastscan)
			udelay(5);	
		else
			break;
	}

	if (key_is_pressed)
		khandy.autocomplete = 0;

	/*
	* switch to interupt mode, if all columns scanned and no key pressed
	* else reschedule scan
	*/
	if (last_column == KBSC_COL0) {
		if (!key_is_pressed) {
			local_irq_save(flags);
			clps_writel( (clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK)
					 | KBSC_HI, SYSCON1);
			local_irq_restore(flags);
			clps_writel(0,KBDEOI);	
			enable_irq(IRQ_KBDINT);
		} else {
			clps711x_kbd_timer.expires = jiffies + scan_interval;
			add_timer(&clps711x_kbd_timer);
		}		
		key_is_pressed = 0;
		memcpy(keys, new_keys, 8 * sizeof(int));
		for (i = 0; i < 8; i++) {
			if (previous_keys[i] != keys[i]) {
				queue_task(&kbd_process_task, &tq_timer);
				return;
			}
		}
	} else {
		clps711x_kbd_timer.expires = jiffies + scan_delay;
		add_timer(&clps711x_kbd_timer);
	}
}

/*
* Keyboard interrupt, change to scheduling mode
*/
static void clps711x_kbd_int(int irq, void *dev_id, struct pt_regs *regs)
{

#ifdef CONFIG_VT
	kbd_pt_regs = regs;
#endif
	disable_irq(IRQ_KBDINT);
	khandy.autocomplete = 0;
	clps_writel( (clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK)
		 | KBSC_COL0, SYSCON1);
	clps711x_kbd_timer.expires = jiffies + scan_delay;
	add_timer(&clps711x_kbd_timer);
}


static int clps711x_kbd_proc_keyboard_read(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	if (count < 2) 
		return -EINVAL;

	return sprintf(page,"h:%d\n",khandy.ena);
}

static int clps711x_kbd_proc_keyboard_write(struct file *file, const char *buffer, 
		unsigned long count, void *data)
{
	unsigned char buf[260];

	if (count < 3|| count > 258)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count)) 
		return -EFAULT;
	if (buf[1] != ':')
		return -EINVAL;
			
	if (buf[0] == 'h') {
		switch (buf[2]) {
			case '0':
			case '1':
			case '2': khandy.ena = buf[2]-'0'; return count;	
		}
	}	
		
	if (buf[0] == 't' && count == 258) {
		memcpy(act_scancode,buf+2,256);
		/* rescan cursor left/right and del */ 
		clps711x_handykey_init();
		return count;
	}
	
	return -EINVAL;
}


/*
 * Initialize the keyboard hardware. 
 * Set all columns high
 * Install interrupt handler
 *
 * Machine dependent parameters:
 *
 * fastscan: 		0 = timer based scan for each column
 *			1 = full scan is done in one timer event
 * scan_delay:		time between column scans 
 * 			setup even if you use fastscan (leeds to timer mode)
 * scan_interval:	time between full scans
 * handy.delay:		timeout before last entry get's automatically valid
 * 
 */
void __init clps711x_kbd_init_hw(void)
{

	/*
	* put here  machine dependent init stuff 
	*/
	if (machine_is_autcpu12()) {
		fastscan = 0;
		scan_interval = 50*HZ/1000;
		scan_delay = 20*HZ/1000;
		khandy.delay = 750*HZ/1000;
		act_scancode = autcpu12_scancode;
	} else {
		printk("No initialization, keyboard killed\n");
		return;
	}

	last_column = KBSC_COL0;
	key_is_pressed = 0;

	clps711x_handykey_init();

	/* Register the /proc entry */
	clps711x_keyboard_proc_entry = create_proc_entry("keyboard", 0444,
		&proc_root);
	if (clps711x_keyboard_proc_entry == NULL)
	    	printk("Couldn't create the /proc entry for the keyboard\n");
	else {
		clps711x_keyboard_proc_entry->read_proc = 
			&clps711x_kbd_proc_keyboard_read;
		clps711x_keyboard_proc_entry->write_proc = 
			&clps711x_kbd_proc_keyboard_write;
	}

	/* Initialize the matrix processing task. */
	k_translate	= clps711x_translate;
	k_unexpected_up	= clps711x_unexpected_up;
	kbd_process_task.routine = clps711x_kbd_process;
	kbd_process_task.data = 0;
	
	/* Setup the timer for keyboard polling, after kbd int */
	init_timer(&clps711x_kbd_timer);
	clps711x_kbd_timer.function = clps711x_kbd_timeout;
	clps711x_kbd_timer.data = 0;
	init_timer(&clps711x_kbdhandy_timer);
	clps711x_kbdhandy_timer.function = clps711x_kbdhandy_timeout;
	clps711x_kbdhandy_timer.data = 1;

	/* Initialise scan hardware, request int */
	clps_writel( (clps_readl(SYSCON1) & ~SYSCON1_KBDSCANMASK)
		 | KBSC_HI, SYSCON1);
	request_irq(IRQ_KBDINT, clps711x_kbd_int, 0,"keyboard", NULL);

	printk("clps711x keyboard init done\n");

}
