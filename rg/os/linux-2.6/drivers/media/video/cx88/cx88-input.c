/*
 *
 * Device driver for GPIO attached remote control interfaces
 * on Conexant 2388x based TV/DVB cards.
 *
 * Copyright (c) 2003 Pavel Machek
 * Copyright (c) 2004 Gerd Knorr
 * Copyright (c) 2004, 2005 Chris Pascoe
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "cx88.h"
#include <media/ir-common.h>

/* ---------------------------------------------------------------------- */

/* DigitalNow DNTV Live DVB-T Remote */
static IR_KEYTAB_TYPE ir_codes_dntv_live_dvb_t[IR_KEYTAB_SIZE] = {
	[0x00] = KEY_ESC,		/* 'go up a level?' */
	/* Keys 0 to 9 */
	[0x0a] = KEY_KP0,
	[0x01] = KEY_KP1,
	[0x02] = KEY_KP2,
	[0x03] = KEY_KP3,
	[0x04] = KEY_KP4,
	[0x05] = KEY_KP5,
	[0x06] = KEY_KP6,
	[0x07] = KEY_KP7,
	[0x08] = KEY_KP8,
	[0x09] = KEY_KP9,

	[0x0b] = KEY_TUNER,		/* tv/fm */
	[0x0c] = KEY_SEARCH,		/* scan */
	[0x0d] = KEY_STOP,
	[0x0e] = KEY_PAUSE,
	[0x0f] = KEY_LIST,		/* source */

	[0x10] = KEY_MUTE,
	[0x11] = KEY_REWIND,		/* backward << */
	[0x12] = KEY_POWER,
	[0x13] = KEY_S,			/* snap */
	[0x14] = KEY_AUDIO,		/* stereo */
	[0x15] = KEY_CLEAR,		/* reset */
	[0x16] = KEY_PLAY,
	[0x17] = KEY_ENTER,
	[0x18] = KEY_ZOOM,		/* full screen */
	[0x19] = KEY_FASTFORWARD,	/* forward >> */
	[0x1a] = KEY_CHANNELUP,
	[0x1b] = KEY_VOLUMEUP,
	[0x1c] = KEY_INFO,		/* preview */
	[0x1d] = KEY_RECORD,		/* record */
	[0x1e] = KEY_CHANNELDOWN,
	[0x1f] = KEY_VOLUMEDOWN,
};

/* ---------------------------------------------------------------------- */

/* IO-DATA BCTV7E Remote */
static IR_KEYTAB_TYPE ir_codes_iodata_bctv7e[IR_KEYTAB_SIZE] = {
	[0x40] = KEY_TV,
	[0x20] = KEY_RADIO,		/* FM */
	[0x60] = KEY_EPG,
	[0x00] = KEY_POWER,

	/* Keys 0 to 9 */
	[0x44] = KEY_KP0,		/* 10 */
	[0x50] = KEY_KP1,
	[0x30] = KEY_KP2,
	[0x70] = KEY_KP3,
	[0x48] = KEY_KP4,
	[0x28] = KEY_KP5,
	[0x68] = KEY_KP6,
	[0x58] = KEY_KP7,
	[0x38] = KEY_KP8,
	[0x78] = KEY_KP9,

	[0x10] = KEY_L,			/* Live */
	[0x08] = KEY_T,			/* Time Shift */

	[0x18] = KEY_PLAYPAUSE,		/* Play */

	[0x24] = KEY_ENTER,		/* 11 */
	[0x64] = KEY_ESC,		/* 12 */
	[0x04] = KEY_M,			/* Multi */

	[0x54] = KEY_VIDEO,
	[0x34] = KEY_CHANNELUP,
	[0x74] = KEY_VOLUMEUP,
	[0x14] = KEY_MUTE,

	[0x4c] = KEY_S,			/* SVIDEO */
	[0x2c] = KEY_CHANNELDOWN,
	[0x6c] = KEY_VOLUMEDOWN,
	[0x0c] = KEY_ZOOM,

	[0x5c] = KEY_PAUSE,
	[0x3c] = KEY_C,			/* || (red) */
	[0x7c] = KEY_RECORD,		/* recording */
	[0x1c] = KEY_STOP,

	[0x41] = KEY_REWIND,		/* backward << */
	[0x21] = KEY_PLAY,
	[0x61] = KEY_FASTFORWARD,	/* forward >> */
	[0x01] = KEY_NEXT,		/* skip >| */
};

/* ---------------------------------------------------------------------- */

/* ADS Tech Instant TV DVB-T PCI Remote */
static IR_KEYTAB_TYPE ir_codes_adstech_dvb_t_pci[IR_KEYTAB_SIZE] = {
	/* Keys 0 to 9 */
	[0x4d] = KEY_0,
	[0x57] = KEY_1,
	[0x4f] = KEY_2,
	[0x53] = KEY_3,
	[0x56] = KEY_4,
	[0x4e] = KEY_5,
	[0x5e] = KEY_6,
	[0x54] = KEY_7,
	[0x4c] = KEY_8,
	[0x5c] = KEY_9,

	[0x5b] = KEY_POWER,
	[0x5f] = KEY_MUTE,
	[0x55] = KEY_GOTO,
	[0x5d] = KEY_SEARCH,
	[0x17] = KEY_EPG,		/* Guide */
	[0x1f] = KEY_MENU,
	[0x0f] = KEY_UP,
	[0x46] = KEY_DOWN,
	[0x16] = KEY_LEFT,
	[0x1e] = KEY_RIGHT,
	[0x0e] = KEY_SELECT,		/* Enter */
	[0x5a] = KEY_INFO,
	[0x52] = KEY_EXIT,
	[0x59] = KEY_PREVIOUS,
	[0x51] = KEY_NEXT,
	[0x58] = KEY_REWIND,
	[0x50] = KEY_FORWARD,
	[0x44] = KEY_PLAYPAUSE,
	[0x07] = KEY_STOP,
	[0x1b] = KEY_RECORD,
	[0x13] = KEY_TUNER,		/* Live */
	[0x0a] = KEY_A,
	[0x12] = KEY_B,
	[0x03] = KEY_PROG1,		/* 1 */
	[0x01] = KEY_PROG2,		/* 2 */
	[0x00] = KEY_PROG3,		/* 3 */
	[0x06] = KEY_DVD,
	[0x48] = KEY_AUX,		/* Photo */
	[0x40] = KEY_VIDEO,
	[0x19] = KEY_AUDIO,		/* Music */
	[0x0b] = KEY_CHANNELUP,
	[0x08] = KEY_CHANNELDOWN,
	[0x15] = KEY_VOLUMEUP,
	[0x1c] = KEY_VOLUMEDOWN,
};

/* ---------------------------------------------------------------------- */

/* MSI TV@nywhere remote */
static IR_KEYTAB_TYPE ir_codes_msi_tvanywhere[IR_KEYTAB_SIZE] = {
	/* Keys 0 to 9 */
	[0x00] = KEY_0,
	[0x01] = KEY_1,
	[0x02] = KEY_2,
	[0x03] = KEY_3,
	[0x04] = KEY_4,
	[0x05] = KEY_5,
	[0x06] = KEY_6,
	[0x07] = KEY_7,
	[0x08] = KEY_8,
	[0x09] = KEY_9,

	[0x0c] = KEY_MUTE,
	[0x0f] = KEY_SCREEN,		/* Full Screen */
	[0x10] = KEY_F,			/* Funtion */
	[0x11] = KEY_T,			/* Time shift */
	[0x12] = KEY_POWER,
	[0x13] = KEY_MEDIA,		/* MTS */
	[0x14] = KEY_SLOW,
	[0x16] = KEY_REWIND,		/* backward << */
	[0x17] = KEY_ENTER,		/* Return */
	[0x18] = KEY_FASTFORWARD,	/* forward >> */
	[0x1a] = KEY_CHANNELUP,
	[0x1b] = KEY_VOLUMEUP,
	[0x1e] = KEY_CHANNELDOWN,
	[0x1f] = KEY_VOLUMEDOWN,
};

/* ---------------------------------------------------------------------- */

/* Cinergy 1400 DVB-T */
static IR_KEYTAB_TYPE ir_codes_cinergy_1400[IR_KEYTAB_SIZE] = {
	[0x01] = KEY_POWER,
	[0x02] = KEY_1,
	[0x03] = KEY_2,
	[0x04] = KEY_3,
	[0x05] = KEY_4,
	[0x06] = KEY_5,
	[0x07] = KEY_6,
	[0x08] = KEY_7,
	[0x09] = KEY_8,
	[0x0a] = KEY_9,
	[0x0c] = KEY_0,

	[0x0b] = KEY_VIDEO,
	[0x0d] = KEY_REFRESH,
	[0x0e] = KEY_SELECT,
	[0x0f] = KEY_EPG,
	[0x10] = KEY_UP,
	[0x11] = KEY_LEFT,
	[0x12] = KEY_OK,
	[0x13] = KEY_RIGHT,
	[0x14] = KEY_DOWN,
	[0x15] = KEY_TEXT,
	[0x16] = KEY_INFO,

	[0x17] = KEY_RED,
	[0x18] = KEY_GREEN,
	[0x19] = KEY_YELLOW,
	[0x1a] = KEY_BLUE,

	[0x1b] = KEY_CHANNELUP,
	[0x1c] = KEY_VOLUMEUP,
	[0x1d] = KEY_MUTE,
	[0x1e] = KEY_VOLUMEDOWN,
	[0x1f] = KEY_CHANNELDOWN,

	[0x40] = KEY_PAUSE,
	[0x4c] = KEY_PLAY,
	[0x58] = KEY_RECORD,
	[0x54] = KEY_PREVIOUS,
	[0x48] = KEY_STOP,
	[0x5c] = KEY_NEXT,
};

/* ---------------------------------------------------------------------- */

/* AVERTV STUDIO 303 Remote */
static IR_KEYTAB_TYPE ir_codes_avertv_303[IR_KEYTAB_SIZE] = {
	[ 0x2a ] = KEY_KP1,
	[ 0x32 ] = KEY_KP2,
	[ 0x3a ] = KEY_KP3,
	[ 0x4a ] = KEY_KP4,
	[ 0x52 ] = KEY_KP5,
	[ 0x5a ] = KEY_KP6,
	[ 0x6a ] = KEY_KP7,
	[ 0x72 ] = KEY_KP8,
	[ 0x7a ] = KEY_KP9,
	[ 0x0e ] = KEY_KP0,

	[ 0x02 ] = KEY_POWER,
	[ 0x22 ] = KEY_VIDEO,
	[ 0x42 ] = KEY_AUDIO,
	[ 0x62 ] = KEY_ZOOM,
	[ 0x0a ] = KEY_TV,
	[ 0x12 ] = KEY_CD,
	[ 0x1a ] = KEY_TEXT,

	[ 0x16 ] = KEY_SUBTITLE,
	[ 0x1e ] = KEY_REWIND,
	[ 0x06 ] = KEY_PRINT,

	[ 0x2e ] = KEY_SEARCH,
	[ 0x36 ] = KEY_SLEEP,
	[ 0x3e ] = KEY_SHUFFLE,
	[ 0x26 ] = KEY_MUTE,

	[ 0x4e ] = KEY_RECORD,
	[ 0x56 ] = KEY_PAUSE,
	[ 0x5e ] = KEY_STOP,
	[ 0x46 ] = KEY_PLAY,

	[ 0x6e ] = KEY_RED,
	[ 0x0b ] = KEY_GREEN,
	[ 0x66 ] = KEY_YELLOW,
	[ 0x03 ] = KEY_BLUE,

	[ 0x76 ] = KEY_LEFT,
	[ 0x7e ] = KEY_RIGHT,
	[ 0x13 ] = KEY_DOWN,
	[ 0x1b ] = KEY_UP,
};

/* ---------------------------------------------------------------------- */

/* DigitalNow DNTV Live! DVB-T Pro Remote */
static IR_KEYTAB_TYPE ir_codes_dntv_live_dvbt_pro[IR_KEYTAB_SIZE] = {
	[ 0x16 ] = KEY_POWER,
	[ 0x5b ] = KEY_HOME,

	[ 0x55 ] = KEY_TV,		/* live tv */
	[ 0x58 ] = KEY_TUNER,		/* digital Radio */
	[ 0x5a ] = KEY_RADIO,		/* FM radio */
	[ 0x59 ] = KEY_DVD,		/* dvd menu */
	[ 0x03 ] = KEY_1,
	[ 0x01 ] = KEY_2,
	[ 0x06 ] = KEY_3,
	[ 0x09 ] = KEY_4,
	[ 0x1d ] = KEY_5,
	[ 0x1f ] = KEY_6,
	[ 0x0d ] = KEY_7,
	[ 0x19 ] = KEY_8,
	[ 0x1b ] = KEY_9,
	[ 0x0c ] = KEY_CANCEL,
	[ 0x15 ] = KEY_0,
	[ 0x4a ] = KEY_CLEAR,
	[ 0x13 ] = KEY_BACK,
	[ 0x00 ] = KEY_TAB,
	[ 0x4b ] = KEY_UP,
	[ 0x4e ] = KEY_LEFT,
	[ 0x4f ] = KEY_OK,
	[ 0x52 ] = KEY_RIGHT,
	[ 0x51 ] = KEY_DOWN,
	[ 0x1e ] = KEY_VOLUMEUP,
	[ 0x0a ] = KEY_VOLUMEDOWN,
	[ 0x02 ] = KEY_CHANNELDOWN,
	[ 0x05 ] = KEY_CHANNELUP,
	[ 0x11 ] = KEY_RECORD,
	[ 0x14 ] = KEY_PLAY,
	[ 0x4c ] = KEY_PAUSE,
	[ 0x1a ] = KEY_STOP,
	[ 0x40 ] = KEY_REWIND,
	[ 0x12 ] = KEY_FASTFORWARD,
	[ 0x41 ] = KEY_PREVIOUSSONG,	/* replay |< */
	[ 0x42 ] = KEY_NEXTSONG,	/* skip >| */
	[ 0x54 ] = KEY_CAMERA,		/* capture */
	[ 0x50 ] = KEY_LANGUAGE,	/* sap */
	[ 0x47 ] = KEY_TV2,		/* pip */
	[ 0x4d ] = KEY_SCREEN,
	[ 0x43 ] = KEY_SUBTITLE,
	[ 0x10 ] = KEY_MUTE,
	[ 0x49 ] = KEY_AUDIO,		/* l/r */
	[ 0x07 ] = KEY_SLEEP,
	[ 0x08 ] = KEY_VIDEO,		/* a/v */
	[ 0x0e ] = KEY_PREVIOUS,	/* recall */
	[ 0x45 ] = KEY_ZOOM,		/* zoom + */
	[ 0x46 ] = KEY_ANGLE,		/* zoom - */
	[ 0x56 ] = KEY_RED,
	[ 0x57 ] = KEY_GREEN,
	[ 0x5c ] = KEY_YELLOW,
	[ 0x5d ] = KEY_BLUE,
};

/* ---------------------------------------------------------------------- */

struct cx88_IR {
	struct cx88_core *core;
	struct input_dev *input;
	struct ir_input_state ir;
	char name[32];
	char phys[32];

	/* sample from gpio pin 16 */
	u32 sampling;
	u32 samples[16];
	int scount;
	unsigned long release;

	/* poll external decoder */
	int polling;
	struct work_struct work;
	struct timer_list timer;
	u32 gpio_addr;
	u32 last_gpio;
	u32 mask_keycode;
	u32 mask_keydown;
	u32 mask_keyup;
};

static int ir_debug = 0;
module_param(ir_debug, int, 0644);	/* debug level [IR] */
MODULE_PARM_DESC(ir_debug, "enable debug messages [IR]");

#define ir_dprintk(fmt, arg...)	if (ir_debug) \
	printk(KERN_DEBUG "%s IR: " fmt , ir->core->name , ##arg)

/* ---------------------------------------------------------------------- */

static void cx88_ir_handle_key(struct cx88_IR *ir)
{
	struct cx88_core *core = ir->core;
	u32 gpio, data;

	/* read gpio value */
	gpio = cx_read(ir->gpio_addr);
	if (ir->polling) {
		if (ir->last_gpio == gpio)
			return;
		ir->last_gpio = gpio;
	}

	/* extract data */
	data = ir_extract_bits(gpio, ir->mask_keycode);
	ir_dprintk("irq gpio=0x%x code=%d | %s%s%s\n",
		   gpio, data,
		   ir->polling ? "poll" : "irq",
		   (gpio & ir->mask_keydown) ? " down" : "",
		   (gpio & ir->mask_keyup) ? " up" : "");

	if (ir->mask_keydown) {
		/* bit set on keydown */
		if (gpio & ir->mask_keydown) {
			ir_input_keydown(ir->input, &ir->ir, data, data);
		} else {
			ir_input_nokey(ir->input, &ir->ir);
		}

	} else if (ir->mask_keyup) {
		/* bit cleared on keydown */
		if (0 == (gpio & ir->mask_keyup)) {
			ir_input_keydown(ir->input, &ir->ir, data, data);
		} else {
			ir_input_nokey(ir->input, &ir->ir);
		}

	} else {
		/* can't distinguish keydown/up :-/ */
		ir_input_keydown(ir->input, &ir->ir, data, data);
		ir_input_nokey(ir->input, &ir->ir);
	}
}

static void ir_timer(unsigned long data)
{
	struct cx88_IR *ir = (struct cx88_IR *)data;

	schedule_work(&ir->work);
}

static void cx88_ir_work(void *data)
{
	struct cx88_IR *ir = data;
	unsigned long timeout;

	cx88_ir_handle_key(ir);
	timeout = jiffies + (ir->polling * HZ / 1000);
	mod_timer(&ir->timer, timeout);
}

/* ---------------------------------------------------------------------- */

int cx88_ir_init(struct cx88_core *core, struct pci_dev *pci)
{
	struct cx88_IR *ir;
	struct input_dev *input_dev;
	IR_KEYTAB_TYPE *ir_codes = NULL;
	int ir_type = IR_TYPE_OTHER;

	ir = kzalloc(sizeof(*ir), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ir || !input_dev) {
		kfree(ir);
		input_free_device(input_dev);
		return -ENOMEM;
	}

	ir->input = input_dev;

	/* detect & configure */
	switch (core->board) {
	case CX88_BOARD_DNTV_LIVE_DVB_T:
	case CX88_BOARD_KWORLD_DVB_T:
	case CX88_BOARD_KWORLD_DVB_T_CX22702:
		ir_codes = ir_codes_dntv_live_dvb_t;
		ir->gpio_addr = MO_GP1_IO;
		ir->mask_keycode = 0x1f;
		ir->mask_keyup = 0x60;
		ir->polling = 50; /* ms */
		break;
	case CX88_BOARD_TERRATEC_CINERGY_1400_DVB_T1:
		ir_codes = ir_codes_cinergy_1400;
		ir_type = IR_TYPE_PD;
		ir->sampling = 0xeb04; /* address */
		break;
	case CX88_BOARD_HAUPPAUGE:
	case CX88_BOARD_HAUPPAUGE_DVB_T1:
	case CX88_BOARD_HAUPPAUGE_NOVASE2_S1:
	case CX88_BOARD_HAUPPAUGE_NOVASPLUS_S1:
	case CX88_BOARD_HAUPPAUGE_HVR1100:
		ir_codes = ir_codes_hauppauge_new;
		ir_type = IR_TYPE_RC5;
		ir->sampling = 1;
		break;
	case CX88_BOARD_WINFAST2000XP_EXPERT:
		ir_codes = ir_codes_winfast;
		ir->gpio_addr = MO_GP0_IO;
		ir->mask_keycode = 0x8f8;
		ir->mask_keyup = 0x100;
		ir->polling = 1; /* ms */
		break;
	case CX88_BOARD_IODATA_GVBCTV7E:
		ir_codes = ir_codes_iodata_bctv7e;
		ir->gpio_addr = MO_GP0_IO;
		ir->mask_keycode = 0xfd;
		ir->mask_keydown = 0x02;
		ir->polling = 5; /* ms */
		break;
	case CX88_BOARD_PIXELVIEW_PLAYTV_ULTRA_PRO:
		ir_codes = ir_codes_pixelview;
		ir->gpio_addr = MO_GP1_IO;
		ir->mask_keycode = 0x1f;
		ir->mask_keyup = 0x80;
		ir->polling = 1; /* ms */
		break;
	case CX88_BOARD_ADSTECH_DVB_T_PCI:
		ir_codes = ir_codes_adstech_dvb_t_pci;
		ir->gpio_addr = MO_GP1_IO;
		ir->mask_keycode = 0xbf;
		ir->mask_keyup = 0x40;
		ir->polling = 50; /* ms */
		break;
	case CX88_BOARD_MSI_TVANYWHERE_MASTER:
		ir_codes = ir_codes_msi_tvanywhere;
		ir->gpio_addr = MO_GP1_IO;
		ir->mask_keycode = 0x1f;
		ir->mask_keyup = 0x40;
		ir->polling = 1; /* ms */
		break;
	case CX88_BOARD_AVERTV_303:
	case CX88_BOARD_AVERTV_STUDIO_303:
		ir_codes         = ir_codes_avertv_303;
		ir->gpio_addr    = MO_GP2_IO;
		ir->mask_keycode = 0xfb;
		ir->mask_keydown = 0x02;
		ir->polling      = 50; /* ms */
		break;
	case CX88_BOARD_DNTV_LIVE_DVB_T_PRO:
		ir_codes = ir_codes_dntv_live_dvbt_pro;
		ir_type = IR_TYPE_PD;
		ir->sampling = 0xff00; /* address */
		break;
	}

	if (NULL == ir_codes) {
		kfree(ir);
		input_free_device(input_dev);
		return -ENODEV;
	}

	/* init input device */
	snprintf(ir->name, sizeof(ir->name), "cx88 IR (%s)",
		 cx88_boards[core->board].name);
	snprintf(ir->phys, sizeof(ir->phys), "pci-%s/ir0", pci_name(pci));

	ir_input_init(input_dev, &ir->ir, ir_type, ir_codes);
	input_dev->name = ir->name;
	input_dev->phys = ir->phys;
	input_dev->id.bustype = BUS_PCI;
	input_dev->id.version = 1;
	if (pci->subsystem_vendor) {
		input_dev->id.vendor = pci->subsystem_vendor;
		input_dev->id.product = pci->subsystem_device;
	} else {
		input_dev->id.vendor = pci->vendor;
		input_dev->id.product = pci->device;
	}
	input_dev->cdev.dev = &pci->dev;
	/* record handles to ourself */
	ir->core = core;
	core->ir = ir;

	if (ir->polling) {
		INIT_WORK(&ir->work, cx88_ir_work, ir);
		init_timer(&ir->timer);
		ir->timer.function = ir_timer;
		ir->timer.data = (unsigned long)ir;
		schedule_work(&ir->work);
	}
	if (ir->sampling) {
		core->pci_irqmask |= (1 << 18);	/* IR_SMP_INT */
		cx_write(MO_DDS_IO, 0xa80a80);	/* 4 kHz sample rate */
		cx_write(MO_DDSCFG_IO, 0x5);	/* enable */
	}

	/* all done */
	input_register_device(ir->input);

	return 0;
}

int cx88_ir_fini(struct cx88_core *core)
{
	struct cx88_IR *ir = core->ir;

	/* skip detach on non attached boards */
	if (NULL == ir)
		return 0;

	if (ir->sampling) {
		cx_write(MO_DDSCFG_IO, 0x0);
		core->pci_irqmask &= ~(1 << 18);
	}
	if (ir->polling) {
		del_timer(&ir->timer);
		flush_scheduled_work();
	}

	input_unregister_device(ir->input);
	kfree(ir);

	/* done */
	core->ir = NULL;
	return 0;
}

/* ---------------------------------------------------------------------- */

void cx88_ir_irq(struct cx88_core *core)
{
	struct cx88_IR *ir = core->ir;
	u32 samples, ircode;
	int i;

	if (NULL == ir)
		return;
	if (!ir->sampling)
		return;

	samples = cx_read(MO_SAMPLE_IO);
	if (0 != samples && 0xffffffff != samples) {
		/* record sample data */
		if (ir->scount < ARRAY_SIZE(ir->samples))
			ir->samples[ir->scount++] = samples;
		return;
	}
	if (!ir->scount) {
		/* nothing to sample */
		if (ir->ir.keypressed && time_after(jiffies, ir->release))
			ir_input_nokey(ir->input, &ir->ir);
		return;
	}

	/* have a complete sample */
	if (ir->scount < ARRAY_SIZE(ir->samples))
		ir->samples[ir->scount++] = samples;
	for (i = 0; i < ir->scount; i++)
		ir->samples[i] = ~ir->samples[i];
	if (ir_debug)
		ir_dump_samples(ir->samples, ir->scount);

	/* decode it */
	switch (core->board) {
	case CX88_BOARD_TERRATEC_CINERGY_1400_DVB_T1:
	case CX88_BOARD_DNTV_LIVE_DVB_T_PRO:
		ircode = ir_decode_pulsedistance(ir->samples, ir->scount, 1, 4);

		if (ircode == 0xffffffff) { /* decoding error */
			ir_dprintk("pulse distance decoding error\n");
			break;
		}

		ir_dprintk("pulse distance decoded: %x\n", ircode);

		if (ircode == 0) { /* key still pressed */
			ir_dprintk("pulse distance decoded repeat code\n");
			ir->release = jiffies + msecs_to_jiffies(120);
			break;
		}

		if ((ircode & 0xffff) != (ir->sampling & 0xffff)) { /* wrong address */
			ir_dprintk("pulse distance decoded wrong address\n");
			break;
		}

		if (((~ircode >> 24) & 0xff) != ((ircode >> 16) & 0xff)) { /* wrong checksum */
			ir_dprintk("pulse distance decoded wrong check sum\n");
			break;
		}

		ir_dprintk("Key Code: %x\n", (ircode >> 16) & 0x7f);

		ir_input_keydown(ir->input, &ir->ir, (ircode >> 16) & 0x7f, (ircode >> 16) & 0xff);
		ir->release = jiffies + msecs_to_jiffies(120);
		break;
	case CX88_BOARD_HAUPPAUGE:
	case CX88_BOARD_HAUPPAUGE_DVB_T1:
	case CX88_BOARD_HAUPPAUGE_NOVASE2_S1:
	case CX88_BOARD_HAUPPAUGE_NOVASPLUS_S1:
	case CX88_BOARD_HAUPPAUGE_HVR1100:
		ircode = ir_decode_biphase(ir->samples, ir->scount, 5, 7);
		ir_dprintk("biphase decoded: %x\n", ircode);
		if ((ircode & 0xfffff000) != 0x3000)
			break;
		ir_input_keydown(ir->input, &ir->ir, ircode & 0x3f, ircode);
		ir->release = jiffies + msecs_to_jiffies(120);
		break;
	}

	ir->scount = 0;
	return;
}

/* ---------------------------------------------------------------------- */

MODULE_AUTHOR("Gerd Knorr, Pavel Machek, Chris Pascoe");
MODULE_DESCRIPTION("input driver for cx88 GPIO-based IR remote controls");
MODULE_LICENSE("GPL");
/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
