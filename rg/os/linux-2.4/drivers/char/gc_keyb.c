/*
 * linux/arch/arm/drivers/char/gc_keyb.c
 *
 * Copyright 2000 Applied Data Systems
 *
 * Keyboard & Smartio driver for GraphicsClient ARM Linux.
 * Graphics Client is SA1110 based single board computer by
 *    Applied Data Systems (http://www.applieddata.net)
 *
 * Change log:
 *    7-10/6/01 Thomas Thaele <tthaele@papenmeier.de>
 *       - Added Keyboard Sniffer on /dev/sio12 <minor = 12>
 *       - First implementation of PC- compatible Scancodes (thanks to pc_keyb.c)
 *       3/23/01 Woojung Huh
 *          Power Management added
 * 		12/01/00 Woojung Huh
 * 			Bug fixed
 * 		11/16/00 Woojung Huh [whuh@applieddata.net]
 * 			Added smartio device driver on it
 */

/*
 * Introduced setkeycode, ketkeycode for the GC+ by Thomas Thaele
 * <tthaele@papenmeier.de> GC+ now performs like a real PC on the keyboard.
 * Warning: this code is still beta! PrntScrn and Pause keys are not
 * completely tested and implemented!!! Keyboard driver can be confused
 * by hacking like crazy on the keyboard. (hardware problem on serial line?)
 */

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kbd_ll.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kbd_kern.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/keyboard.h>
#include <linux/tqueue.h>
#include <linux/proc_fs.h>
#include <linux/pm.h>

#define ADS_AVR_IRQ	63

#define	SMARTIO_IOCTL_BASES		's'
#define	SMARTIO_KPD_TIMEOUT		_IOW(SMARTIO_IOCTL_BASES, 0, int)
#define	SMARTIO_KPD_SETUP		_IOW(SMARTIO_IOCTL_BASES, 1, short)
#define	SMARTIO_BL_CONTROL		_IOW(SMARTIO_IOCTL_BASES, 2, char)
#define	SMARTIO_BL_CONTRAST		_IOW(SMARTIO_IOCTL_BASES, 3, char)
#define SMARTIO_PORT_CONFIG		_IOW(SMARTIO_IOCTL_BASES, 4, char)
#define SMARTIO_SNIFFER_TIMEOUT		_IOW(SMARTIO_IOCTL_BASES, 5, long)


/* Simple translation table for the SysRq keys */

#ifdef CONFIG_MAGIC_SYSRQ
unsigned char pckbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
	"\r\000/";					/* 0x60 - 0x6f */
#endif

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
#define E0_BREAK   101  /* (control-pause) */
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

#define E1_PAUSE   119

/*
 * The keycodes below are randomly located in 89-95,112-118,120-127.
 * They could be thrown away (and all occurrences below replaced by 0),
 * but that would force many users to use the `setkeycodes' utility, where
 * they needed not before. It does not matter that there are duplicates, as
 * long as no duplication occurs for any single keyboard.
 */
#define SC_LIM 89

#define FOCUS_PF1 85           /* actual code! */
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
  RGN1, RGN2, RGN3, RGN4, 0, 0, 0,                   /* 0x59-0x5f */
  0, 0, 0, 0, 0, 0, 0, 0,                            /* 0x60-0x67 */
  0, 0, 0, 0, 0, FOCUS_PF11, 0, FOCUS_PF12,          /* 0x68-0x6f */
  0, 0, 0, FOCUS_PF2, FOCUS_PF9, 0, 0, FOCUS_PF3,    /* 0x70-0x77 */
  FOCUS_PF4, FOCUS_PF5, FOCUS_PF6, FOCUS_PF7,        /* 0x78-0x7b */
  FOCUS_PF8, JAP_86, FOCUS_PF10, 0                   /* 0x7c-0x7f */
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
#define E0_OK	124
/*
 * New microsoft keyboard is rumoured to have
 * e0 5b (left window button), e0 5c (right window button),
 * e0 5d (menu button). [or: LBANNER, RBANNER, RMENU]
 * [or: Windows_L, Windows_R, TaskMan]
 */
#define E0_MSLW	125
#define E0_MSRW	126
#define E0_MSTM	127

static unsigned char e0_keys[128] = {
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x00-0x07 */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x08-0x0f */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x10-0x17 */
  0, 0, 0, 0, E0_KPENTER, E0_RCTRL, 0, 0,	      /* 0x18-0x1f */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x20-0x27 */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x28-0x2f */
  0, 0, 0, 0, 0, E0_KPSLASH, 0, E0_PRSCR,	      /* 0x30-0x37 */
  E0_RALT, 0, 0, 0, 0, E0_F13, E0_F14, E0_HELP,	      /* 0x38-0x3f */
  E0_DO, E0_F17, 0, 0, 0, 0, E0_BREAK, E0_HOME,	      /* 0x40-0x47 */
  E0_UP, E0_PGUP, 0, E0_LEFT, E0_OK, E0_RIGHT, E0_KPMINPLUS, E0_END,/* 0x48-0x4f */
  E0_DOWN, E0_PGDN, E0_INS, E0_DEL, 0, 0, 0, 0,	      /* 0x50-0x57 */
  0, 0, 0, E0_MSLW, E0_MSRW, E0_MSTM, 0, 0,	      /* 0x58-0x5f */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x60-0x67 */
  0, 0, 0, 0, 0, 0, 0, E0_MACRO,		      /* 0x68-0x6f */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x70-0x77 */
  0, 0, 0, 0, 0, 0, 0, 0			      /* 0x78-0x7f */
};

int gc_kbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	if (scancode < SC_LIM || scancode > 255 || keycode > 127)
	  return -EINVAL;
	if (scancode < 128)
	  high_keys[scancode - SC_LIM] = keycode;
	else
	  e0_keys[scancode - 128] = keycode;
	return 0;
}

int gc_kbd_getkeycode(unsigned int scancode)
{
	return
	  (scancode < SC_LIM || scancode > 255) ? -EINVAL :
	  (scancode < 128) ? high_keys[scancode - SC_LIM] :
	    e0_keys[scancode - 128];
}

int gc_kbd_translate(unsigned char scancode, unsigned char *keycode,
		    char raw_mode)
{
	static int prev_scancode;

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
	      } else if (prev_scancode == 0x100 && scancode == 0x45) {
		  *keycode = E1_PAUSE;
		  prev_scancode = 0;
	      } else {
#ifdef KBD_REPORT_UNKN
		  if (!raw_mode)
		    printk(KERN_INFO "keyboard: unknown e1 escape sequence\n");
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
		    printk(KERN_INFO "keyboard: unknown scancode e0 %02x\n",
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
		  printk(KERN_INFO "keyboard: unrecognized scancode (%02x)"
			 " - ignored\n", scancode);
#endif
	      }
	      return 0;
	  }
 	} else
	  *keycode = scancode;
 	return 1;
}

// this table converts the hardware dependent codes of a MF-2 Keyboard to
// the codes normally comming out of a i8042. This table is 128 Bytes too
// big, but for stability reasons it should be kept like it is!
// There is no range checking in the code!
static int mf_two_kbdmap[256] = {
	00, 67, 65, 63, 61, 59, 60, 88, 00, 68, 66, 64, 62, 15, 41, 00,
	00, 56, 42, 00, 29, 16, 02, 00, 00, 00, 44, 31, 30, 17, 03, 00,
	00, 46, 45, 32, 18, 05, 04, 00, 00, 57, 47, 33, 20, 19, 06, 00,
	00, 49, 48, 35, 34, 21,  7, 00, 00, 00, 50, 36, 22,  8,  9, 00,
	00, 51, 37, 23, 24, 11, 10, 00, 00, 52, 53, 38, 39, 25, 12, 00,
	00, 00, 40, 00, 26, 13, 00, 00, 58, 54, 28, 27, 00, 43, 00, 00,
	00, 86, 00, 00, 00, 00, 14, 00, 00, 79, 00, 75, 71, 00, 00, 00,
	82, 83, 80, 76, 77, 72, 01, 69, 87, 78, 81, 74, 55, 73, 70, 00,
	00, 00, 00, 65, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
	00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00 };


// some texts displayed by the proc_file_system
static char *kbd_sniff[2] = { "off", "on" };
static char *kbd_sniff_mode[2] = { "passive", "active" };

#define PASSIVE 0
#define ACTIVE  1

// is the sniffer active (1) or inactive (0)
static int  SNIFFER = 0;
// do we get a copy (SNIFFMODE = PASSIVE) or do we get the original data (SNIFFMODE = ACTIVE)
// and have to reinsert the data
static int  SNIFFMODE = PASSIVE;

// we allow only one process to sniff
static int sniffer_in_use = 0;

// timeout for the keyboard sniffer -1 = blocking, otherwise timeout in msecs
static long sniffer_timeout = -1;

// the value we sniffed from the keyboard
static int sniffed_value;

static char *smartio_version = "1.02 MF-II compatibility patch <tthaele@papenmeier.de>";
static char *smartio_date = "Aug-27-2001";

static int sio_reset_flag;
static int kbd_press_flag;

static void send_SSP_msg(unchar *pBuf, int num)
{
	ushort	tmp;
	int		i;

	for (i=0;i<num;i++) {
		while ((Ser4SSSR & SSSR_TNF) == 0);
		tmp = pBuf[i];
		Ser4SSDR = (tmp << 8);
	}

	// Throw away Echo
	for (i=0;i<num;i++) {
		while ((Ser4SSSR & SSSR_RNE) == 0);
		tmp = Ser4SSDR;
	}
}

static unchar ReadSSPByte(void)
{
	if (Ser4SSSR & SSSR_ROR) {
		printk("%s() : Overrun\n", __FUNCTION__);
		return 0;
	}

	Ser4SSDR = 0x00;

	while ((Ser4SSSR & SSSR_RNE) == 0);

	return ((unchar) Ser4SSDR);
}

static ulong read_SSP_response(int num)
{
	int		i;
	ulong	ret;

	// discard leading 0x00 and command echo 0 (command group value)
	while (ReadSSPByte() == 0);
	// discard command echo 1 (command code value)
	ReadSSPByte();

	// data from SMARTIO
	// It assumes LSB first.
	// NOTE:Some command uses MSB first order
	ret = 0;
	for (i=0;i<num;i++) {
		ret |= ReadSSPByte() << (8*i);
	}

	return ret;
}

typedef	struct	t_SMARTIO_CMD {
	unchar	Group;
	unchar	Code;
	unchar  Opt[2];
}	SMARTIO_CMD;

static	SMARTIO_CMD RD_INT_CMD = { 0x83, 0x01, { 0x00, 0x00 } };
static	SMARTIO_CMD RD_KBD_CMD = { 0x83, 0x02, { 0x00, 0x00 } };
static	SMARTIO_CMD RD_ADC_CMD = { 0x83, 0x28, { 0x00, 0x00 } };
static	SMARTIO_CMD RD_KPD_CMD = { 0x83, 0x04, { 0x00, 0x00 } };

static	volatile ushort	adc_value;
static	volatile unchar	kpd_value;
static	unsigned int	kpd_timeout = 10000;			// 10000 msec

static  ulong kbd_int, kpd_int, adc_int;

static void smartio_interrupt_task(void *data);

static struct tq_struct tq_smartio = {
		{ NULL,	NULL },		// struct list_head
		0,			// unsigned long sync
		smartio_interrupt_task,	// void (*routine)(void *)
		NULL,			// void *data
};

DECLARE_WAIT_QUEUE_HEAD(smartio_queue);
DECLARE_WAIT_QUEUE_HEAD(smartio_adc_queue);
DECLARE_WAIT_QUEUE_HEAD(smartio_kpd_queue);
DECLARE_WAIT_QUEUE_HEAD(keyboard_done_queue);
DECLARE_WAIT_QUEUE_HEAD(sniffer_queue);

static spinlock_t smartio_busy_lock = SPIN_LOCK_UNLOCKED;
static atomic_t	smartio_busy = ATOMIC_INIT(0);

static int f_five_pressed = 0;
static int f_seven_pressed = 0;
//static int e_null_counter = 0;
//static int f_null_counter = 0;
//static int keydown = 0;
static unchar previous_code = 0;
//static int e0 = 0;

static void smartio_interrupt_task(void *arg)
{
	unchar	code;
	unsigned long flags;
	unchar  dummy;

	spin_lock_irqsave(&smartio_busy_lock, flags);
	if (atomic_read(&smartio_busy) == 1) {
		spin_unlock_irqrestore(&smartio_busy_lock, flags);
		queue_task(&tq_smartio, &tq_timer);
	}
	else {
		atomic_set(&smartio_busy, 1);
		spin_unlock_irqrestore(&smartio_busy_lock, flags);
	}

	/* Read SMARTIO Interrupt Status to check which Interrupt is occurred
	 * and Clear SMARTIO Interrupt */
	send_SSP_msg((unchar *) &RD_INT_CMD, 2);
	code = (unchar) (read_SSP_response(1) & 0xFF);

#ifdef CONFIG_VT
	if (code  & 0x04) {					// Keyboard Interrupt
		kbd_int++;
		/* Read Scan code */
		send_SSP_msg((unchar *) &RD_KBD_CMD, 2);
		code = (unchar) (read_SSP_response(1) & 0xFF);
		dummy = code & 0x80;
		if ((code == 0xE0) || (code == 0xE1) || (code == 0xF0)) {	// combined code
			if (code == 0xF0) {
				if (!previous_code) {
					code = 0xE0;
					previous_code = 0xF0;
				} else {
					code = mf_two_kbdmap[code & 0x7F] | dummy;
					previous_code = 0;
				}
			} else if (code == 0xE0) {
				if (previous_code != 0) {
					code = mf_two_kbdmap[code & 0x7F] | dummy;
					previous_code = 0;
				} else previous_code = code;
			} else {						// 0xE1
				if (!previous_code) {
					code = mf_two_kbdmap[code &0x7F] | dummy;
					previous_code = 0;
				} else {
					previous_code = code;
				}
			}
		} else {
			if (code == 0x03) {
				f_five_pressed = 1;
			} else if (code == 0x83) {
				if (f_five_pressed != 0) {
					f_five_pressed = 0;
					code = 0x03;
				} else if (f_seven_pressed == 0) {
					f_seven_pressed = 1;
					code = 2;
					dummy = 0;
				} else {
					f_seven_pressed = 0;
					code = 2;
				}
			}
			previous_code = 0;
			code &= 0x7F;
			code = mf_two_kbdmap[code] | dummy;
		}
		sniffed_value = (ushort)code;
		if (SNIFFER) wake_up_interruptible(&sniffer_queue);
		if (SNIFFMODE == PASSIVE) {
			handle_scancode( code, (code & 0x80) ? 0 : 1 );
			if (code & 0x80) {
				wake_up_interruptible(&keyboard_done_queue);
				mdelay(10);		// this makes the whole thing a bit more stable
							// keyboard handling can be corrupted when hitting
							// thousands of keys like crazy. kbd_translate might catch up
							// with irq routine? or there is simply a buffer overflow on
							// the serial device? somehow it looses some key sequences.
							// if a break code is lost or coruppted the keyboard starts
							// to autorepeat like crazy and appears to hang.
							// this needs further investigations! Thomas
				kbd_press_flag = 0;
			}
			else
				kbd_press_flag = 1;
		}
		code = 0;       // prevent furthermore if ... then to react!
	}
#endif
	// ADC resolution is 10bit (0x000 ~ 0x3FF)
	if (code & 0x02) {					// ADC Complete Interrupt
		adc_int++;
		send_SSP_msg((unchar *) &RD_ADC_CMD, 2);
		adc_value = (ushort) (read_SSP_response(2) & 0x3FF);
		wake_up_interruptible(&smartio_adc_queue);
	}

	if (code & 0x08) { 					// Keypad interrupt
		kpd_int++;
		send_SSP_msg((unchar *) &RD_KPD_CMD, 2);
		kpd_value = (unchar) (read_SSP_response(1) & 0xFF);
		wake_up_interruptible(&smartio_kpd_queue);
	}

	spin_lock_irqsave(&smartio_busy_lock, flags);
	atomic_set(&smartio_busy, 0);
	spin_unlock_irqrestore(&smartio_busy_lock, flags);

	enable_irq(ADS_AVR_IRQ);

	wake_up_interruptible(&smartio_queue);
}

static void gc_sio_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
#ifdef CONFIG_VT
	kbd_pt_regs = regs;
#endif

	// *NOTE*
	// ADS SMARTIO interrupt is cleared after reading interrupt status
	// from smartio.
	// disable SMARTIO IRQ here and re-enable at samrtio_bh.
	// 11/13/00 Woojung
	disable_irq(ADS_AVR_IRQ);

	queue_task(&tq_smartio, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

char gc_kbd_unexpected_up(unsigned char keycode)
{
	return 0;
}

static inline void gc_sio_init(void)
{
	GPDR |= (GPIO_GPIO10 | GPIO_GPIO12 | GPIO_GPIO13); 	// Output
	GPDR &= ~GPIO_GPIO11;

	// Alternative Function
	GAFR |= (GPIO_GPIO10 | GPIO_GPIO11 | GPIO_GPIO12 | GPIO_GPIO13);

	Ser4SSCR0 = 0xA707;
	Ser4SSSR = SSSR_ROR;
	Ser4SSCR1 = 0x0010;
	Ser4SSCR0 = 0xA787;

	// Reset SMARTIO
	ADS_AVR_REG &= 0xFE;
	mdelay(300);			// 10 mSec
	ADS_AVR_REG |= 0x01;
	mdelay(10);			// 10 mSec

}

void __init gc_kbd_init_hw(void)
{
	printk (KERN_INFO "Graphics Client keyboard driver v1.0\n");

	k_setkeycode	= gc_kbd_setkeycode;
	k_getkeycode	= gc_kbd_getkeycode;
	k_translate	= gc_kbd_translate;
	k_unexpected_up	= gc_kbd_unexpected_up;
#ifdef CONFIG_MAGIC_SYSRQ
	k_sysrq_key	= 0x54;
	/* sysrq table??? --rmk */
#endif

	gc_sio_init();

	if (request_irq(ADS_AVR_IRQ,gc_sio_interrupt,0,"smartio", NULL) != 0)
		printk("Could not allocate SMARTIO IRQ!\n");

	sio_reset_flag = 1;
}

/* SMARTIO ADC Interface */
#define SMARTIO_VERSION				0
#define SMARTIO_PORT_A				1
#define SMARTIO_PORT_B				2
#define SMARTIO_PORT_C				3
#define SMARTIO_PORT_D				4
#define SMARTIO_SELECT_OPTION			5
#define SMARTIO_BACKLITE			6
#define SMARTIO_KEYPAD				7
#define SMARTIO_ADC				8
#define	SMARTIO_VEE_PWM				9
#define SMARTIO_SLEEP				11
#define SMARTIO_KBD_SNIFFER			12

static	SMARTIO_CMD CONV_ADC_CMD = { 0x80, 0x28, { 0x00, 0x00 } };
static	SMARTIO_CMD READ_PORT_CMD = { 0x82, 0x00, { 0x00, 0x00 } };

static	SMARTIO_CMD READ_DEVVER_CMD = { 0x82, 0x05, { 0x00, 0x00 } };
static	SMARTIO_CMD READ_DEVTYPE_CMD = { 0x82, 0x06, { 0x00, 0x00 } };
static	SMARTIO_CMD READ_FWLEVEL_CMD = { 0x82, 0x07, { 0x00, 0x00 } };

static int lock_smartio(unsigned long *flags)
{
	spin_lock_irqsave(&smartio_busy_lock, *flags);
	if (atomic_read(&smartio_busy) == 1) {
		spin_unlock_irqrestore(&smartio_busy_lock, *flags);
		interruptible_sleep_on(&smartio_queue);
	}
	else {
		atomic_set(&smartio_busy, 1);
		spin_unlock_irqrestore(&smartio_busy_lock, *flags);
	}

	return 1;
}

static int unlock_smartio(unsigned long *flags)
{
	spin_lock_irqsave(&smartio_busy_lock, *flags);
	atomic_set(&smartio_busy, 0);
	spin_unlock_irqrestore(&smartio_busy_lock, *flags);

	return 1;
}

static ushort read_sio_adc(int channel)
{
	unsigned long	flags;

	if ((channel < 0) || (channel > 7))
		return 0xFFFF;

	CONV_ADC_CMD.Opt[0] = (unchar) channel;

	lock_smartio(&flags);
	send_SSP_msg((unchar *) &CONV_ADC_CMD, 3);
	unlock_smartio(&flags);

	interruptible_sleep_on(&smartio_adc_queue);

	return adc_value & 0x3FF;
}

static ushort read_sio_port(int port)
{
	unsigned long	flags;
	ushort			ret;

	if ((port < SMARTIO_PORT_B) || (port > SMARTIO_PORT_D))
		return 0xFFFF;

	READ_PORT_CMD.Code = (unchar) port;

	lock_smartio(&flags);
	send_SSP_msg((unchar *) &READ_PORT_CMD, 2);
	ret = read_SSP_response(1);
	unlock_smartio(&flags);

	return ret;
}

static ushort read_sio_kpd(void)
{
	long	timeout;

	// kpd_timeout is mSec order
	// interrupt_sleep_on_timeout is based on 10msec timer tick
	if (kpd_timeout == -1) {
		interruptible_sleep_on(&smartio_kpd_queue);
	}
	else {
		timeout = interruptible_sleep_on_timeout(&smartio_kpd_queue,
								kpd_timeout/10);
		if (timeout == 0) {
			// timeout without keypad input
			return 0xFFFF;
		}
	}
	return kpd_value;
}

static ushort read_sio_sniff(void)
{
        long    timeout;

        // kpd_timeout is mSec order
        // interrupt_sleep_on_timeout is based on 10msec timer tick
        if (sniffer_timeout == -1) {
                interruptible_sleep_on(&sniffer_queue);
        }
        else {
                timeout = interruptible_sleep_on_timeout(&sniffer_queue,
                                                                sniffer_timeout/10);
                if (timeout == 0) {
                        // timeout without keypad input
                        return -1;
                }
        }
        return (ushort)sniffed_value;
}

static struct sio_ver {
	uint	DevVer;
	uint	DevType;
	uint	FwLevel;
};

static ushort read_sio_version(struct sio_ver *ptr)
{
	unsigned long	flags;
	ushort          ret;

	// Read Device Version
	lock_smartio(&flags);
	send_SSP_msg((unchar *) &READ_DEVVER_CMD, 2);
	ret = read_SSP_response(1);
	unlock_smartio(&flags);
	ptr->DevVer = (uint)ret;
	// Read Device Type
	lock_smartio(&flags);
	send_SSP_msg((unchar *) &READ_DEVTYPE_CMD, 2);
	ret = read_SSP_response(2);
	unlock_smartio(&flags);
	// swap MSB & LSB
	ret = ((ret & 0xFF) << 8) | ((ret & 0xFF00) >> 8);
	ptr->DevType = (uint)ret;
	// Read Firmware Level
	lock_smartio(&flags);
	send_SSP_msg((unchar *) &READ_FWLEVEL_CMD, 2);
	ret = read_SSP_response(2);
	unlock_smartio(&flags);
	// swap MSB & LSB
	ret = ((ret & 0xFF) << 8) | ((ret & 0xFF00) >> 8);
	ptr->FwLevel = (uint)ret;

	return 0;
}

static ssize_t sio_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	unsigned int minor = MINOR(inode->i_rdev);
	ushort	*ret = (ushort *)buf;

	switch (minor) {
	case	SMARTIO_ADC:
			if ((*ret = read_sio_adc(buf[0])) != 0xFFFF)
				return sizeof(ushort);					// 2 bytes
	case	SMARTIO_PORT_B:
	case	SMARTIO_PORT_C:
	case	SMARTIO_PORT_D:
			if ((*ret = read_sio_port(minor)) != 0xFFFF)
				return sizeof(ushort);
	case	SMARTIO_VERSION:
			if ((read_sio_version((struct sio_ver *)buf)) != 0xFFFF)
				return sizeof(struct sio_ver);
	case	SMARTIO_KEYPAD:
			if ((*ret = read_sio_kpd()) != 0xFFFF)
				return sizeof(ushort);
	case	SMARTIO_KBD_SNIFFER:
			if ((*ret = read_sio_sniff()) != (ushort)-1)
				return 1;
	default :
			return -ENXIO;
	}
}

static	SMARTIO_CMD WRITE_PORT_CMD = { 0x81, 0x00, { 0x00, 0x00 } };
static	SMARTIO_CMD SELECT_OPT_CMD = { 0x80, 0x00, { 0x00, 0x00 } };
static	SMARTIO_CMD CONTROL_BL_CMD = { 0x80, 0x00, { 0x00, 0x00 } };
static	SMARTIO_CMD CONTRAST_BL_CMD = { 0x80, 0x21, { 0x00, 0x00 } };
static	SMARTIO_CMD CONTROL_KPD_CMD = { 0x80, 0x27, { 0x00, 0x00 } };
static	SMARTIO_CMD CONTROL_VEE_CMD = { 0x80, 0x22, { 0x00, 0x00 } };

static ushort write_sio_port(int port, unchar value)
{
	unsigned long	flags;

	if ((port < SMARTIO_PORT_B) || (port > SMARTIO_PORT_D))
		return 0xFFFF;

	WRITE_PORT_CMD.Code = (unchar) port;
	WRITE_PORT_CMD.Opt[0] = (unchar) value;

	lock_smartio(&flags);
	send_SSP_msg((unchar *) &WRITE_PORT_CMD, 3);
	unlock_smartio(&flags);

	return 0;
}

static ushort write_sio_select(unchar select)
{
	unsigned long	flags;

	if ((select < 1) || (select > 2))
		return 0xFFFF;

	SELECT_OPT_CMD.Code = (unchar) (select + 0x28);

	lock_smartio(&flags);
	send_SSP_msg((unchar *) &SELECT_OPT_CMD, 2);
	unlock_smartio(&flags);

	return 0;
}

static ushort control_sio_backlite(int cmd, int value)
{
	unsigned long	flags;

	if (cmd == SMARTIO_BL_CONTRAST) {
		value &= 0xFF;
		CONTRAST_BL_CMD.Opt[0] = (unchar) value;

		lock_smartio(&flags);
		send_SSP_msg((unchar *) &CONTRAST_BL_CMD, 3);
		unlock_smartio(&flags);
	}
	else if (cmd == SMARTIO_BL_CONTROL) {
		if (value == 0x00) {
			// Backlite OFF
			CONTROL_BL_CMD.Code = 0x24;
		}
		else {
			// Backlite ON
			CONTROL_BL_CMD.Code = 0x23;
		}
		lock_smartio(&flags);
		send_SSP_msg((unchar *) &CONTROL_BL_CMD, 2);
		unlock_smartio(&flags);
	}
	else
		return 0xFFFF;

	return 0;
}

static ushort control_sio_keypad(int x, int y)
{
	unsigned long	flags;

	if ( (x<1) || (x>8) || (y<1) || (y>8)) {
		return 0xFFFF;
	}

	CONTROL_KPD_CMD.Opt[0] = (unchar) x;
	CONTROL_KPD_CMD.Opt[1] = (unchar) y;

	lock_smartio(&flags);
	send_SSP_msg((unchar *) &CONTROL_KPD_CMD, 4);
	unlock_smartio(&flags);

	return 0;
}

static ushort control_sio_vee(int value)
{
	unsigned long	flags;

	value &= 0xFF;
	CONTROL_VEE_CMD.Opt[0] = (unchar) value;

	lock_smartio(&flags);
	send_SSP_msg((unchar *) &CONTROL_VEE_CMD, 3);
	unlock_smartio(&flags);

	return 0;
}

static ssize_t sio_write(struct file *file, const char *buf, size_t cont, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	unsigned int minor = MINOR(inode->i_rdev);

	switch (minor) {
	case	SMARTIO_PORT_B:
	case	SMARTIO_PORT_C:
	case	SMARTIO_PORT_D:
			if (write_sio_port(minor, buf[0]) != 0xFFFF)
				return 1;
	case	SMARTIO_SELECT_OPTION:
			if (write_sio_select(buf[0]) != 0xFFFF)
				return 1;
	case	SMARTIO_BACKLITE:
			if (control_sio_backlite(SMARTIO_BL_CONTROL, buf[0]) != 0xFFFF)
				return 1;
	case	SMARTIO_KEYPAD:
			if (control_sio_keypad(buf[0], buf[1]) != 0xFFFF)
				return 2;
	case	SMARTIO_VEE_PWM:
			if (control_sio_vee(buf[0]) != 0xFFFF)
				return 1;
	case	SMARTIO_KBD_SNIFFER:
			// here are the scancodes injected
			handle_scancode((unchar)buf[0], (buf[0] & 0x80) ? 0 : 1);
			wake_up_interruptible(&keyboard_done_queue);
			// give some time to process! File IO is a bit faster than manual typing ;-)
			udelay(10000);
			return 1;
	default:
		return -ENXIO;
	}
}

static unsigned int sio_poll(struct file *file, struct poll_table_struct *wait)
{
	return 0;
}

static	SMARTIO_CMD IOCTL_PORT_CMD = { 0x81, 0x00, { 0x00, 0x00 } };

static ushort ioctl_sio_port(int port, unchar value)
{
	unsigned long	flags;

	if ((port < SMARTIO_PORT_B) || (port > SMARTIO_PORT_D))
		return 0xFFFF;

	IOCTL_PORT_CMD.Code = (unchar) port + 0x04;		// 0x05 ~ 0x08
	if (port == SMARTIO_PORT_B) {
		// Port B has 4 bits only
		IOCTL_PORT_CMD.Opt[0] = (unchar) value & 0x0F;
	}
	else
		IOCTL_PORT_CMD.Opt[0] = (unchar) value;

	lock_smartio(&flags);
	send_SSP_msg((unchar *) &IOCTL_PORT_CMD, 3);
	unlock_smartio(&flags);

	return 0;
}

static int sio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int minor = MINOR(inode->i_rdev);
	unchar	*buf = (unchar *)arg;

	switch (minor) {
	case	SMARTIO_PORT_B:
	case	SMARTIO_PORT_C:
	case	SMARTIO_PORT_D:
			if (cmd == SMARTIO_PORT_CONFIG) {
				if (ioctl_sio_port(minor, buf[0]) != 0xFFFF)
					return 0;
			}
			return -EINVAL;
	case	SMARTIO_SELECT_OPTION:
			if (write_sio_select(buf[0]) != 0xFFFF) return 0;
			return -EINVAL;
	case	SMARTIO_BACKLITE:
			if (cmd == SMARTIO_BL_CONTROL) {
				if (control_sio_backlite(SMARTIO_BL_CONTROL, buf[0]) != 0xFFFF) return 0;
			}
			else if (cmd == SMARTIO_BL_CONTRAST) {
				if (control_sio_backlite(SMARTIO_BL_CONTRAST, buf[0]) != 0xFFFF) return 0;
			}
			else return -EINVAL;
	case	SMARTIO_KEYPAD:
			if (cmd == SMARTIO_KPD_TIMEOUT) {
				kpd_timeout = *(long*)buf;
				return 0;
			}
			else if (cmd == SMARTIO_KPD_SETUP) {
				if (control_sio_keypad(buf[0], buf[1]) != 0xFFFF) return 0;
			}
			return -EINVAL;
	case	SMARTIO_VEE_PWM:
			if (control_sio_vee(buf[0]) != 0xFFFF) return 0;
			return -EINVAL;
	case	SMARTIO_KBD_SNIFFER:
			if (cmd == SMARTIO_SNIFFER_TIMEOUT) {
				sniffer_timeout = *(long*)buf;
				if (sniffer_timeout < 0) sniffer_timeout = -1;
				// the value will be devided by 10 later on
				if (!sniffer_timeout) sniffer_timeout = 10;
				return 0;
			}
			return -EINVAL;
	default:
		return -ENXIO;
	}
}

static int sio_open(struct inode *inode, struct file *file)
{
        unsigned int minor = MINOR(inode->i_rdev);

	// we open all by default. we only have a special handler for the kbd sniffer
	switch (minor) {
		case SMARTIO_KBD_SNIFFER:
			if (sniffer_in_use) return -EBUSY;
			sniffer_in_use = 1;
			SNIFFER = 1;
			// sniff in active or passive mode
			if ((file->f_flags & O_RDWR) == O_RDWR) SNIFFMODE = 1; else SNIFFMODE = 0;
			// do we have a blocking or non blocking sniffer?
			if ((file->f_flags & O_NONBLOCK) == O_NONBLOCK) sniffer_timeout = 100; else sniffer_timeout = -1;
			break;
		default:
			break;
	}
	return 0;
}

static int sio_close(struct inode *inode, struct file *file)
{
        unsigned int minor = MINOR(inode->i_rdev);

	switch (minor) {
		case SMARTIO_KBD_SNIFFER:
			SNIFFER = 0;
			SNIFFMODE = 0;
			sniffer_in_use = 0;
			break;
		default:
			break;
	}
	return 0;
}

static struct file_operations sio_fops = {
	read: 		sio_read,
	write:		sio_write,
	poll:		sio_poll,
	ioctl:		sio_ioctl,
	open:		sio_open,
	release:	sio_close,
};

static struct proc_dir_entry *sio_dir, *parent_dir = NULL;

#define	SMARTIO_MAJOR	58
#define	MAJOR_NR	SMARTIO_MAJOR

#define	PROC_NAME	"sio"

static int sio_read_proc(char *buf, char **start, off_t pos, int count, int *eof, void *data)
{
	char	*p = buf;

	p += sprintf(p, "ADS SMARTIO Status: \n");
	p += sprintf(p, "\t Keyboard Interrupt : %lu\n", kbd_int);
	p += sprintf(p, "\t Keypad Interrupt : %lu\n", kpd_int);
	p += sprintf(p, "\t ADC Interrupt : %lu\n", adc_int);
	p += sprintf(p, "\t Keyboard Sniffer : %s mode : %s\n", kbd_sniff[ SNIFFER ], kbd_sniff_mode [ SNIFFMODE ]);

	return (p-buf);
}

#ifdef	CONFIG_PM
static int pm_smartio_callback(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch (rqst) {
		case	PM_RESUME:
			gc_sio_init();
			break;
		case	PM_SUSPEND:
			// 4/5/01 Woojung
			// It checks Keybard received pair of press/release code.
			// System can sleep before receiving release code
			if (kbd_press_flag) {
				interruptible_sleep_on(&keyboard_done_queue);
			}
			break;
	}

	return 0;
}
#endif

void __init sio_init(void)
{
	if (register_chrdev(MAJOR_NR, "sio", &sio_fops)) {
		printk("smartio : unable to get major %d\n", MAJOR_NR);
		return;
	}
	else {
		printk("smartio driver initialized. version %s, date:%s\n",
				smartio_version, smartio_date);

		if (sio_reset_flag != 1) {
			gc_sio_init();
			if (request_irq(ADS_AVR_IRQ, gc_sio_interrupt,0,"sio",NULL) != 0){
				printk("smartio : Could not allocate IRQ!\n");
				return;
			}
		}

		if ((sio_dir = create_proc_entry(PROC_NAME, 0, parent_dir)) == NULL) {
			printk("smartio : Unable to create /proc entry\n");
			return;
		}
		else {
			sio_dir->read_proc = sio_read_proc;
#ifdef	CONFIG_PM
			pm_register(PM_SYS_DEV, PM_SYS_KBC, pm_smartio_callback);
#endif
		}
	}
}
