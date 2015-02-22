/*
 * SA1111 PS/2 keyboard/mouse driver
 *
 * 2000 by VASARA RESEARCH INC.
 *
 * Changelog:
 *     Jun.xx,2000:    Kunihiko IMAI <imai@vasara.co.jp>
 *                     Port to 2.4.0test1-ac19-rmk1-np1
 *     Apr.17,2000:    Takafumi Kawana <kawana@pro.or.jp>
 *                     Internal Release for XP860
 *
 *
 * This driver is based on linux/drivers/char/pc_keyb.c
 * Original declaration follows:

 *
 * linux/drivers/char/pc_keyb.c
 *
 * Separation of the PC low-level part by Geert Uytterhoeven, May 1997
 * See keyboard.c for the whole history.
 *
 * Major cleanup by Martin Mares, May 1997
 *
 * Combined the keyboard and PS/2 mouse handling into one file,
 * because they share the same hardware.
 * Johan Myreen <jem@iki.fi> 1998-10-08.
 *
 * Code fixes to handle mouse ACKs properly.
 * C. Scott Ananian <cananian@alumni.princeton.edu> 1999-01-29.
 *
 */
#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/kbd_kern.h>
#include <linux/ioport.h>

#include <asm/hardware.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <asm/io.h>

/* Some configuration switches are present in the include file... */

#include <linux/pc_keyb.h>
#include <asm/keyboard.h>
#include <asm/hardware/sa1111.h>

#define KBD_STAT_RXB    (1<<4)
#define KBD_STAT_RXF    (1<<5)
#define KBD_STAT_TXB    (1<<6)
#define KBD_STAT_TXE    (1<<7)
#define KBD_STAT_STP    (1<<8)

#define MSE_STAT_RXB    (1<<4)
#define MSE_STAT_RXF    (1<<5)
#define MSE_STAT_TXB    (1<<6)
#define MSE_STAT_TXE    (1<<7)
#define MSE_STAT_STP    (1<<8)

/* Simple translation table for the SysRq keys */

#ifdef CONFIG_MAGIC_SYSRQ
unsigned char sa1111_sysrq_xlate[128] = "\000\0331234567890-=\177\t"	/* 0x00 - 0x0f */
    "qwertyuiop[]\r\000as"	/* 0x10 - 0x1f */
    "dfghjkl;'`\000\\zxcv"	/* 0x20 - 0x2f */
    "bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
    "\206\207\210\211\212\000\000789-456+1"	/* 0x40 - 0x4f */
    "230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000"	/* 0x50 - 0x5f */
    "\r\000/";			/* 0x60 - 0x6f */
#endif

// static void kbd_write_command_w(int data);
static void kbd_write_output_w(int data);

spinlock_t kbd_controller_lock = SPIN_LOCK_UNLOCKED;
static void handle_kbd_event(void);

/* used only by send_data - set by keyboard_interrupt */
static volatile unsigned char reply_expected = 0;
static volatile unsigned char acknowledge = 0;
static volatile unsigned char resend = 0;


#if defined CONFIG_PSMOUSE
/*
 *     PS/2 Auxiliary Device
 */

static int __init psaux_init(void);

static struct aux_queue *queue;	/* Mouse data buffer. */
static int aux_count = 0;
/* used when we send commands to the mouse that expect an ACK. */
static unsigned char mouse_reply_expected = 0;

#define AUX_INTS_OFF (KBD_MODE_KCC | KBD_MODE_DISABLE_MOUSE | KBD_MODE_SYS | KBD_MODE_KBD_INT)
#define AUX_INTS_ON  (KBD_MODE_KCC | KBD_MODE_SYS | KBD_MODE_MOUSE_INT | KBD_MODE_KBD_INT)

#define MAX_RETRIES    60	/* some aux operations take long time */
#endif				/* CONFIG_PSMOUSE */

/*
 * Wait for keyboard controller input buffer to drain.
 *
 * Don't use 'jiffies' so that we don't depend on
 * interrupts..
 *
 * Quote from PS/2 System Reference Manual:
 *
 * "Address hex 0060 and address hex 0064 should be written only when
 * the input-buffer-full bit and output-buffer-full bit in the
 * Controller Status register are set 0."
 */

static void kb_wait(void)
{
	unsigned long timeout = KBC_TIMEOUT;

	do {
		/*
		 * "handle_kbd_event()" will handle any incoming events
		 * while we wait - keypresses or mouse movement.
		 */
		handle_kbd_event();
		if (KBDSTAT & KBD_STAT_TXE)
			return;
		mdelay(1);
		timeout--;
	}
	while (timeout);
#ifdef KBD_REPORT_TIMEOUTS
	printk(KERN_WARNING "Keyboard timed out[1]\n");
#endif
}

/*
 * Translation of escaped scancodes to keycodes.
 * This is now user-settable.
 * The keycodes 1-88,96-111,119 are fairly standard, and
 * should probably not be changed - changing might confuse X.
 * X also interprets scancode 0x5d (KEY_Begin).
 *
 * For 1-88 keycode equals scancode.
 */

#define E0_KPENTER 96
#define E0_RCTRL   97
#define E0_KPSLASH 98
#define E0_PRSCR   99
#define E0_RALT    100
#define E0_BREAK   101		/* (control-pause) */
#define E0_HOME    102
#define E0_UP      103
#define E0_PGUP    104
#define E0_LEFT    105
#define E0_RIGHT   106
#define E0_END     107
#define E0_DOWN    108
#define E0_PGDN    109
#define E0_INS     110
#define E0_DEL     111

/* for USB 106 keyboard */
#define E0_YEN         124
#define E0_BACKSLASH   89


#define E1_PAUSE   119

/*
 * The keycodes below are randomly located in 89-95,112-118,120-127.
 * They could be thrown away (and all occurrences below replaced by 0),
 * but that would force many users to use the `setkeycodes' utility, where
 * they needed not before. It does not matter that there are duplicates, as
 * long as no duplication occurs for any single keyboard.
 */
#define SC_LIM 89

#define FOCUS_PF1 85		/* actual code! */
#define FOCUS_PF2 89
#define FOCUS_PF3 90
#define FOCUS_PF4 91
#define FOCUS_PF5 92
#define FOCUS_PF6 93
#define FOCUS_PF7 94
#define FOCUS_PF8 95
#define FOCUS_PF9 120
#define FOCUS_PF10 121
#define FOCUS_PF11 122
#define FOCUS_PF12 123

#define JAP_86     124
/* tfj@olivia.ping.dk:
 * The four keys are located over the numeric keypad, and are
 * labelled A1-A4. It's an rc930 keyboard, from
 * Regnecentralen/RC International, Now ICL.
 * Scancodes: 59, 5a, 5b, 5c.
 */
#define RGN1 124
#define RGN2 125
#define RGN3 126
#define RGN4 127

static unsigned char high_keys[128 - SC_LIM] = {
	RGN1, RGN2, RGN3, RGN4, 0, 0, 0,	/* 0x59-0x5f */
	0, 0, 0, 0, 0, 0, 0, 0,	/* 0x60-0x67 */
	0, 0, 0, 0, 0, FOCUS_PF11, 0, FOCUS_PF12,	/* 0x68-0x6f */
	0, 0, 0, FOCUS_PF2, FOCUS_PF9, 0, 0, FOCUS_PF3,	/* 0x70-0x77 */
	FOCUS_PF4, FOCUS_PF5, FOCUS_PF6, FOCUS_PF7,	/* 0x78-0x7b */
	FOCUS_PF8, JAP_86, FOCUS_PF10, 0	/* 0x7c-0x7f */
};

/* BTC */
#define E0_MACRO   112
/* LK450 */
#define E0_F13     113
#define E0_F14     114
#define E0_HELP    115
#define E0_DO      116
#define E0_F17     117
#define E0_KPMINPLUS 118
/*
 * My OmniKey generates e0 4c for  the "OMNI" key and the
 * right alt key does nada. [kkoller@nyx10.cs.du.edu]
 */
#define E0_OK  124
/*
 * New microsoft keyboard is rumoured to have
 * e0 5b (left window button), e0 5c (right window button),
 * e0 5d (menu button). [or: LBANNER, RBANNER, RMENU]
 * [or: Windows_L, Windows_R, TaskMan]
 */
#define E0_MSLW        125
#define E0_MSRW        126
#define E0_MSTM        127

static unsigned char e0_keys[128] = {
	0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00-0x07 */
	0, 0, 0, 0, 0, 0, 0, 0,	/* 0x08-0x0f */
	0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10-0x17 */
	0, 0, 0, 0, E0_KPENTER, E0_RCTRL, 0, 0,	/* 0x18-0x1f */
	0, 0, 0, 0, 0, 0, 0, 0,	/* 0x20-0x27 */
	0, 0, 0, 0, 0, 0, 0, 0,	/* 0x28-0x2f */
	0, 0, 0, 0, 0, E0_KPSLASH, 0, E0_PRSCR,	/* 0x30-0x37 */
	E0_RALT, 0, 0, 0, 0, E0_F13, E0_F14, E0_HELP,	/* 0x38-0x3f */
	E0_DO, E0_F17, 0, 0, 0, 0, E0_BREAK, E0_HOME,	/* 0x40-0x47 */
	E0_UP, E0_PGUP, 0, E0_LEFT, E0_OK, E0_RIGHT, E0_KPMINPLUS, E0_END,	/* 0x48-0x4f */
	E0_DOWN, E0_PGDN, E0_INS, E0_DEL, 0, 0, 0, 0,	/* 0x50-0x57 */
	0, 0, 0, E0_MSLW, E0_MSRW, E0_MSTM, 0, 0,	/* 0x58-0x5f */
	0, 0, 0, 0, 0, 0, 0, 0,	/* 0x60-0x67 */
	0, 0, 0, 0, 0, 0, 0, E0_MACRO,	/* 0x68-0x6f */
	//0, 0, 0, 0, 0, 0, 0, 0,                          /* 0x70-0x77 */
	0, 0, 0, 0, 0, E0_BACKSLASH, 0, 0,	/* 0x70-0x77 */
	0, 0, 0, E0_YEN, 0, 0, 0, 0	/* 0x78-0x7f */
};

int sa1111_setkeycode(unsigned int scancode, unsigned int keycode)
{
	if (scancode < SC_LIM || scancode > 255 || keycode > 127)
		return -EINVAL;
	if (scancode < 128)
		high_keys[scancode - SC_LIM] = keycode;
	else
		e0_keys[scancode - 128] = keycode;
	return 0;
}

int sa1111_getkeycode(unsigned int scancode)
{
	return
	    (scancode < SC_LIM || scancode > 255) ? -EINVAL :
	    (scancode <
	     128) ? high_keys[scancode - SC_LIM] : e0_keys[scancode - 128];
}

static int do_acknowledge(unsigned char scancode)
{
	if (reply_expected) {
		/* Unfortunately, we must recognise these codes only if we know they
		 * are known to be valid (i.e., after sending a command), because there
		 * are some brain-damaged keyboards (yes, FOCUS 9000 again) which have
		 * keys with such codes :(
		 */
		if (scancode == KBD_REPLY_ACK) {
			acknowledge = 1;
			reply_expected = 0;
			return 0;
		} else if (scancode == KBD_REPLY_RESEND) {
			resend = 1;
			reply_expected = 0;
			return 0;
		}
		/* Should not happen... */
#if 0
		printk(KERN_DEBUG "keyboard reply expected - got %02x\n",
		       scancode);
#endif
	}
	return 1;
}

int
sa1111_translate(unsigned char scancode, unsigned char *keycode,
		 char raw_mode)
{
	static int prev_scancode = 0;

	/* special prefix scancodes.. */
	if (scancode == 0xe0 || scancode == 0xe1) {
		prev_scancode = scancode;
		return 0;
	}

	/* 0xFF is sent by a few keyboards, ignore it. 0x00 is error */
	if (scancode == 0x00 || scancode == 0xff) {
		prev_scancode = 0;
		return 0;
	}

	scancode &= 0x7f;

	if (prev_scancode) {
		/*
		 * usually it will be 0xe0, but a Pause key generates
		 * e1 1d 45 e1 9d c5 when pressed, and nothing when released
		 */
		if (prev_scancode != 0xe0) {
			if (prev_scancode == 0xe1 && scancode == 0x1d) {
				prev_scancode = 0x100;
				return 0;
			}
				else if (prev_scancode == 0x100
					 && scancode == 0x45) {
				*keycode = E1_PAUSE;
				prev_scancode = 0;
			} else {
#ifdef KBD_REPORT_UNKN
				if (!raw_mode)
					printk(KERN_INFO
					       "keyboard: unknown e1 escape sequence\n");
#endif
				prev_scancode = 0;
				return 0;
			}
		} else {
			prev_scancode = 0;
			/*
			 *  The keyboard maintains its own internal caps lock and
			 *  num lock statuses. In caps lock mode E0 AA precedes make
			 *  code and E0 2A follows break code. In num lock mode,
			 *  E0 2A precedes make code and E0 AA follows break code.
			 *  We do our own book-keeping, so we will just ignore these.
			 */
			/*
			 *  For my keyboard there is no caps lock mode, but there are
			 *  both Shift-L and Shift-R modes. The former mode generates
			 *  E0 2A / E0 AA pairs, the latter E0 B6 / E0 36 pairs.
			 *  So, we should also ignore the latter. - aeb@cwi.nl
			 */
			if (scancode == 0x2a || scancode == 0x36)
				return 0;

			if (e0_keys[scancode])
				*keycode = e0_keys[scancode];
			else {
#ifdef KBD_REPORT_UNKN
				if (!raw_mode)
					printk(KERN_INFO
					       "keyboard: unknown scancode e0 %02x\n",
					       scancode);
#endif
				return 0;
			}
		}
	} else if (scancode >= SC_LIM) {
		/* This happens with the FOCUS 9000 keyboard
		   Its keys PF1..PF12 are reported to generate
		   55 73 77 78 79 7a 7b 7c 74 7e 6d 6f
		   Moreover, unless repeated, they do not generate
		   key-down events, so we have to zero up_flag below */
		/* Also, Japanese 86/106 keyboards are reported to
		   generate 0x73 and 0x7d for \ - and \ | respectively. */
		/* Also, some Brazilian keyboard is reported to produce
		   0x73 and 0x7e for \ ? and KP-dot, respectively. */

		*keycode = high_keys[scancode - SC_LIM];

		if (!*keycode) {
			if (!raw_mode) {
#ifdef KBD_REPORT_UNKN
				printk(KERN_INFO
				       "keyboard: unrecognized scancode (%02x)"
				       " - ignored\n", scancode);
#endif
			}
			return 0;
		}
	} else
		*keycode = scancode;
	return 1;
}

char sa1111_unexpected_up(unsigned char keycode)
{
	/* unexpected, but this can happen: maybe this was a key release for a
	   FOCUS 9000 PF key; if we want to see it, we have to clear up_flag */
	if (keycode >= SC_LIM || keycode == 85)
		return 0;
	else
		return 0200;
}

static unsigned char kbd_exists = 1;

static inline void handle_keyboard_event(unsigned char scancode)
{
#ifdef CONFIG_VT
	kbd_exists = 1;
	if (do_acknowledge(scancode))
		handle_scancode(scancode, !(scancode & 0x80));
#endif
	tasklet_schedule(&keyboard_tasklet);
}

/*
 * This reads the keyboard status port, and does the
 * appropriate action.
 *
 * It requires that we hold the keyboard controller
 * spinlock.
 */
static void handle_kbd_event(void)
{
	unsigned int status = KBDSTAT;
	unsigned int work = 10000;
	unsigned char scancode;

	while (status & KBD_STAT_RXF) {
		while (status & KBD_STAT_RXF) {
			scancode = KBDDATA & 0xff;
			if (!(status & KBD_STAT_STP))
				handle_keyboard_event(scancode);
			if (!--work) {
				printk(KERN_ERR
				       "pc_keyb: keyboard controller jammed (0x%02X).\n",
				       status);
				return;
			}
			status = KBDSTAT;
		}
		work = 10000;
	}

	if (status & KBD_STAT_STP)
		KBDSTAT = KBD_STAT_STP;
}

static void keyboard_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

#ifdef CONFIG_VT
	kbd_pt_regs = regs;
#endif
	spin_lock_irqsave(&kbd_controller_lock, flags);
	handle_kbd_event();
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}

/*
 * send_data sends a character to the keyboard and waits
 * for an acknowledge, possibly retrying if asked to. Returns
 * the success status.
 *
 * Don't use 'jiffies', so that we don't depend on interrupts
 */
static int send_data(unsigned char data)
{
	int retries = 3;

	do {
		unsigned long timeout = KBD_TIMEOUT;

		acknowledge = 0;	/* Set by interrupt routine on receipt of ACK. */
		resend = 0;
		reply_expected = 1;
		kbd_write_output_w(data);
		for (;;) {
			if (acknowledge)
				return 1;
			if (resend)
				break;
			mdelay(1);
			if (!--timeout) {
#ifdef KBD_REPORT_TIMEOUTS
				printk(KERN_WARNING
				       "keyboard: Timeout - AT keyboard not present?\n");
#endif
				return 0;
			}
		}
	}
	while (retries-- > 0);
#ifdef KBD_REPORT_TIMEOUTS
	printk(KERN_WARNING
	       "keyboard: Too many NACKs -- noisy kbd cable?\n");
#endif
	return 0;
}

void sa1111_leds(unsigned char leds)
{
	if (kbd_exists
	    && (!send_data(KBD_CMD_SET_LEDS) || !send_data(leds))) {
		send_data(KBD_CMD_ENABLE);	/* re-enable kbd if any errors */
		kbd_exists = 0;
	}
}

#define KBD_NO_DATA    (-1)	/* No data */
#define KBD_BAD_DATA   (-2)	/* Parity or other error */

static int __init kbd_read_data(void)
{
	int retval = KBD_NO_DATA;
	unsigned int status;

	status = KBDSTAT;
	if (status & KBD_STAT_RXF) {
		unsigned char data = KBDDATA;

		retval = data;
		if (status & KBD_STAT_STP)
			retval = KBD_BAD_DATA;
	}
	return retval;
}

static void __init kbd_clear_input(void)
{
	int maxread = 100;	/* Random number */

	do {
		if (kbd_read_data() == KBD_NO_DATA)
			break;
	}
	while (--maxread);
}

static int __init kbd_wait_for_input(void)
{
	long timeout = KBD_INIT_TIMEOUT;

	do {
		int retval = kbd_read_data();
		if (retval >= 0)
			return retval;
		mdelay(1);
	}
	while (--timeout);
	return -1;
}

#if 0
static void kbd_write_command_w(int data)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	kb_wait();
	kbd_write_command(data);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}
#endif

static void kbd_write_output_w(int data)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	kb_wait();
	KBDDATA = data & 0xff;
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}

/*
 * Test the keyboard interface.  We basically check to make sure that
 * we can drive each line to the keyboard independently of each other.
 */
static int kbdif_test(void)
{
	int ret = 0;

	KBDCR = KBDCR_ENA | KBDCR_FKC;
	udelay(2);
	if ((KBDSTAT & (KBDSTAT_KBC | KBDSTAT_KBD)) != KBDSTAT_KBD) {
		printk("Keyboard interface test failed[1]: %02x\n",
		       KBDSTAT);
		ret = -ENODEV;
	}

	KBDCR = KBDCR_ENA;
	udelay(2);
	if ((KBDSTAT & (KBDSTAT_KBC | KBDSTAT_KBD)) != (KBDSTAT_KBC | KBDSTAT_KBD)) {
		printk("Keyboard interface test failed[2]: %02x\n",
		       KBDSTAT);
		ret = -ENODEV;
	}

	KBDCR = KBDCR_ENA | KBDCR_FKD;
	udelay(2);
	if ((KBDSTAT & (KBDSTAT_KBC | KBDSTAT_KBD)) != KBDSTAT_KBC) {
		printk("Keyboard interface test failed[3]: %02x\n",
		       KBDSTAT);
		ret = -ENODEV;
	}

	return ret;
}

static char *__init initialize_kbd(void)
{
	int status;

	/*
	 * Test the keyboard interface.
	 */
	kbdif_test();

	/*
	 * Ok, drop the force low bits, and wait a while,
	 * and clear the stop bit error flag.
	 */
	KBDCR = KBDCR_ENA;
	udelay(4);
	KBDSTAT = KBD_STAT_STP;

	/*
	 * Ok, we're now ready to talk to the keyboard.  Reset
	 * it, just to make sure we're starting in a sane state.
	 *
	 * Set up to try again if the keyboard asks for RESEND.
	 */
	do {
		KBDDATA = KBD_CMD_RESET;
		status = kbd_wait_for_input();
		if (status == KBD_REPLY_ACK)
			break;
		if (status != KBD_REPLY_RESEND)
			return "Keyboard reset failed, no ACK";
	} while (1);

	if (kbd_wait_for_input() != KBD_REPLY_POR)
		return "Keyboard reset failed, no POR";

	/*
	 * Set keyboard controller mode. During this, the keyboard should be
	 * in the disabled state.
	 *
	 * Set up to try again if the keyboard asks for RESEND.
	 */
	do {
		kbd_write_output_w(KBD_CMD_DISABLE);
		status = kbd_wait_for_input();
		if (status == KBD_REPLY_ACK)
			break;
		if (status != KBD_REPLY_RESEND)
			return "Disable keyboard: no ACK";
	} while (1);

#if 0				/*@@@ */
	kbd_write_command_w(KBD_CCMD_WRITE_MODE);
	kbd_write_output_w(KBD_MODE_KBD_INT
			   | KBD_MODE_SYS | KBD_MODE_DISABLE_MOUSE |
			   KBD_MODE_KCC);

	/* ibm powerpc portables need this to use scan-code set 1 -- Cort */
	kbd_write_command_w(KBD_CCMD_READ_MODE);
	if (!(kbd_wait_for_input() & KBD_MODE_KCC)) {
		/*
		 * If the controller does not support conversion,
		 * Set the keyboard to scan-code set 1.
		 */
		kbd_write_output_w(0xF0);
		kbd_wait_for_input();
		kbd_write_output_w(0x01);
		kbd_wait_for_input();
	}
#else
	kbd_write_output_w(0xf0);
	kbd_wait_for_input();
	kbd_write_output_w(0x01);
	kbd_wait_for_input();
#endif


	kbd_write_output_w(KBD_CMD_ENABLE);
	if (kbd_wait_for_input() != KBD_REPLY_ACK)
		return "Enable keyboard: no ACK";

	/*
	 * Finally, set the typematic rate to maximum.
	 */
	kbd_write_output_w(KBD_CMD_SET_RATE);
	if (kbd_wait_for_input() != KBD_REPLY_ACK)
		return "Set rate: no ACK";
	kbd_write_output_w(0x00);
	if (kbd_wait_for_input() != KBD_REPLY_ACK)
		return "Set rate: no ACK";

	return NULL;
}

int __init sa1111_kbd_init_hw(void)
{
	char *msg;
	int ret;

	if (!request_mem_region(_KBDCR, 512, "keyboard"))
		return -EBUSY;

	SKPCR |= SKPCR_PTCLKEN;
	KBDCLKDIV = 0;
	KBDPRECNT = 127;

	/* Flush any pending input. */
	kbd_clear_input();

	msg = initialize_kbd();
	if (msg)
		printk(KERN_WARNING "initialize_kbd: %s\n", msg);

#if defined CONFIG_PSMOUSE
	psaux_init();
#endif

	k_setkeycode	= sa1111_setkeycode;
	k_getkeycode	= sa1111_getkeycode;
	k_translate	= sa1111_translate;
	k_unexpected_up	= sa1111_unexpected_up;
	k_leds		= sa1111_leds;
#ifdef CONFIG_MAGIC_SYSRQ
	k_sysrq_xlate	= sa1111_sysrq_xlate;
	k_sysrq_key	= 0x54;
#endif

	/* Ok, finally allocate the IRQ, and off we go.. */
	ret = request_irq(IRQ_TPRXINT, keyboard_interrupt, 0, "keyboard", NULL);
	if (ret)
		release_mem_region(_KBDCR, 512);

	return ret;
}

#if defined CONFIG_PSMOUSE

static inline void handle_mouse_event(unsigned char scancode)
{
	if (mouse_reply_expected) {
		if (scancode == AUX_ACK) {
			mouse_reply_expected--;
			return;
		}
		mouse_reply_expected = 0;
	}

	add_mouse_randomness(scancode);
	if (aux_count) {
		int head = queue->head;

		queue->buf[head] = scancode;
		head = (head + 1) & (AUX_BUF_SIZE - 1);
		if (head != queue->tail) {
			queue->head = head;
			if (queue->fasync)
				kill_fasync(&queue->fasync, SIGIO,
					    POLL_IN);
			wake_up_interruptible(&queue->proc_list);
		}
	}
}

static void handle_mse_event(void)
{
	unsigned int msests = MSESTAT;
	unsigned int work = 10000;
	unsigned char scancode;

	while (msests & MSE_STAT_RXF) {
		while (msests & MSE_STAT_RXF) {
			scancode = MSEDATA & 0xff;
			if (!(msests & MSE_STAT_STP))
				handle_mouse_event(scancode);
			if (!--work) {
				printk(KERN_ERR
				       "pc_keyb: mouse controller jammed (0x%02X).\n",
				       msests);
				return;
			 /*XXX*/}
			msests = MSESTAT;
		}
		work = 10000;
	}
}

static void ms_wait(void)
{
	unsigned long timeout = KBC_TIMEOUT;

	do {
		/*
		 * "handle_kbd_event()" will handle any incoming events
		 * while we wait - keypresses or mouse movement.
		 */
		handle_mse_event();
		if (MSESTAT & MSE_STAT_TXE)
			return;
		mdelay(1);
		timeout--;
	}
	while (timeout);
#ifdef KBD_REPORT_TIMEOUTS
	printk(KERN_WARNING "Mouse timed out[1]\n");
#endif
}

static void mouse_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	handle_mse_event();
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}

/*
 * Check if this is a dual port controller.
 */
static int __init detect_auxiliary_port(void)
{
	unsigned long flags;
	int loops = 10;
	int retval = 0;

	/* Check if the BIOS detected a device on the auxiliary port. */
	if (aux_device_present == 0xaa)
		return 1;

	spin_lock_irqsave(&kbd_controller_lock, flags);

	/* Put the value 0x5A in the output buffer using the "Write
	 * Auxiliary Device Output Buffer" command (0xD3). Poll the
	 * Status Register for a while to see if the value really
	 * turns up in the Data Register. If the KBD_STAT_MOUSE_OBF
	 * bit is also set to 1 in the Status Register, we assume this
	 * controller has an Auxiliary Port (a.k.a. Mouse Port).
	 */
	// kb_wait();
	// kbd_write_command(KBD_CCMD_WRITE_AUX_OBUF);

	SKPCR |= SKPCR_PMCLKEN;

	MSECLKDIV = 0;
	MSEPRECNT = 127;
	MSECR = MSECR_ENA;
	mdelay(50);
	MSEDATA = 0xf4;
	mdelay(50);

	do {
		unsigned int msests = MSESTAT;

		if (msests & MSE_STAT_RXF) {
			do {
				msests = MSEDATA;	/* dummy read */
				mdelay(50);
				msests = MSESTAT;
			}
			while (msests & MSE_STAT_RXF);
			printk(KERN_INFO "Detected PS/2 Mouse Port.\n");
			retval = 1;
			break;
		}
		mdelay(1);
	}
	while (--loops);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);

	return retval;
}

/*
 * Send a byte to the mouse.
 */
static void aux_write_dev(int val)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	// kb_wait();
	// kbd_write_command(KBD_CCMD_WRITE_MOUSE);
	ms_wait();
	MSEDATA = val;
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}

/*
 * Send a byte to the mouse & handle returned ack
 */
static void aux_write_ack(int val)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	// kb_wait();
	// kbd_write_command(KBD_CCMD_WRITE_MOUSE);
	ms_wait();
	MSEDATA = val;
	/* we expect an ACK in response. */
	mouse_reply_expected++;
	ms_wait();
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}

static unsigned char get_from_queue(void)
{
	unsigned char result;
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	result = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (AUX_BUF_SIZE - 1);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
	return result;
}


static inline int queue_empty(void)
{
	return queue->head == queue->tail;
}

static int fasync_aux(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &queue->fasync);
	if (retval < 0)
		return retval;
	return 0;
}


/*
 * Random magic cookie for the aux device
 */
#define AUX_DEV ((void *)queue)

static int release_aux(struct inode *inode, struct file *file)
{
	fasync_aux(-1, file, 0);
	if (--aux_count)
		return 0;
	// kbd_write_cmd(AUX_INTS_OFF);                     /* Disable controller ints */
	// kbd_write_command_w(KBD_CCMD_MOUSE_DISABLE);
	aux_write_ack(AUX_DISABLE_DEV);	/* Disable aux device */
	MSECR &= ~MSECR_ENA;
	free_irq(IRQ_MSRXINT, AUX_DEV);
	return 0;
}

/*
 * Install interrupt handler.
 * Enable auxiliary device.
 */

static int open_aux(struct inode *inode, struct file *file)
{
	if (aux_count++) {
		return 0;
	}
	queue->head = queue->tail = 0;	/* Flush input queue */
	/* Don't enable the mouse controller until we've registered IRQ handler */
	if (request_irq(IRQ_MSRXINT, mouse_interrupt, SA_SHIRQ, "PS/2 Mouse", AUX_DEV)) {
		aux_count--;
		return -EBUSY;
	}
	MSECLKDIV = 0;
	MSEPRECNT = 127;
	MSECR &= ~MSECR_ENA;
	mdelay(50);
	MSECR = MSECR_ENA;
	mdelay(50);
	MSEDATA = 0xf4;
	mdelay(50);
	if (MSESTAT & 0x0100) {
		MSESTAT = 0x0100;	/* clear IRQ status */
	}
/*  kbd_write_command_w(KBD_CCMD_MOUSE_ENABLE); *//* Enable the
   auxiliary port on
   controller. */
	aux_write_ack(AUX_ENABLE_DEV);	/* Enable aux device */
	// kbd_write_cmd(AUX_INTS_ON); /* Enable controller ints */

	// send_data(KBD_CMD_ENABLE);   /* try to workaround toshiba4030cdt problem */

	return 0;
}

/*
 * Put bytes from input queue to buffer.
 */

static ssize_t
read_aux(struct file *file, char *buffer, size_t count, loff_t * ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	ssize_t i = count;
	unsigned char c;

	if (queue_empty()) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		add_wait_queue(&queue->proc_list, &wait);
	      repeat:
		set_current_state(TASK_INTERRUPTIBLE);
		if (queue_empty() && !signal_pending(current)) {
			schedule();
			goto repeat;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&queue->proc_list, &wait);
	}
	while (i > 0 && !queue_empty()) {
		c = get_from_queue();
		put_user(c, buffer++);
		i--;
	}
	if (count - i) {
		file->f_dentry->d_inode->i_atime = CURRENT_TIME;
		return count - i;
	}
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

/*
 * Write to the aux device.
 */

static ssize_t
write_aux(struct file *file, const char *buffer, size_t count,
	  loff_t * ppos)
{
	ssize_t retval = 0;

	if (count) {
		ssize_t written = 0;

		if (count > 32)
			count = 32;	/* Limit to 32 bytes. */
		do {
			char c;
			get_user(c, buffer++);
			aux_write_dev(c);
			written++;
		}
		while (--count);
		retval = -EIO;
		if (written) {
			retval = written;
			file->f_dentry->d_inode->i_mtime = CURRENT_TIME;
		}
	}

	return retval;
}

static unsigned int aux_poll(struct file *file, poll_table * wait)
{
	poll_wait(file, &queue->proc_list, wait);
	if (!queue_empty())
		return POLLIN | POLLRDNORM;
	return 0;
}

struct file_operations psaux_fops = {
	read:		read_aux,
	write:		write_aux,
	poll:		aux_poll,
	open:		open_aux,
	release:	release_aux,
	fasync:		fasync_aux,
};

/*
 * Initialize driver.
 */
static struct miscdevice psaux_mouse = {
	PSMOUSE_MINOR, "psaux", &psaux_fops
};


static int __init psaux_init(void)
{
	int ret;

	if (!request_mem_region(_MSECR, 512, "psaux"))
		return -EBUSY;

	if (!detect_auxiliary_port()) {
		ret = -EIO;
		goto out;
	}

	misc_register(&psaux_mouse);
	queue = (struct aux_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	memset(queue, 0, sizeof(*queue));
	queue->head = queue->tail = 0;
	init_waitqueue_head(&queue->proc_list);

#ifdef CONFIG_PSMOUSE
	aux_write_ack(AUX_SET_SAMPLE);
	aux_write_ack(100);	/* 100 samples/sec */
	aux_write_ack(AUX_SET_RES);
	aux_write_ack(3);	/* 8 counts per mm */
	aux_write_ack(AUX_SET_SCALE21);	/* 2:1 scaling */
#endif
	ret = 0;

 out:
 	if (ret)
 		release_mem_region(_MSECR, 512);
	return ret;
}

#endif				/* CONFIG_PSMOUSE */
