/*
 * Philips UDA1341 mixer device driver
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * Portions are Copyright (C) 2000 Lernout & Hauspie Speech Products, N.V.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial release.
 *
 * 2000-08-19	Erik Bunce	More inline w/ OSS API and UDA1341 docs
 * 				including fixed AGC and audio source handling
 *
 * 2000-11-30	Nicolas Pitre	- More mixer functionalities.
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 *
 * 2001-08-13	Russell King	Re-written as part of the L3 interface
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/soundcard.h>
#include <linux/l3/l3.h>
#include <linux/l3/uda1341.h>

#include <asm/uaccess.h>

#define DEF_VOLUME	65

/*
 * UDA1341 L3 address and command types
 */
#define UDA1341_L3ADDR		5
#define UDA1341_DATA0		(UDA1341_L3ADDR << 2 | 0)
#define UDA1341_DATA1		(UDA1341_L3ADDR << 2 | 1)
#define UDA1341_STATUS		(UDA1341_L3ADDR << 2 | 2)

struct uda1341_regs {
	unsigned char	stat0;
#define STAT0			0x00
#define STAT0_RST		(1 << 6)
#define STAT0_SC_MASK		(3 << 4)
#define STAT0_SC_512FS		(0 << 4)
#define STAT0_SC_384FS		(1 << 4)
#define STAT0_SC_256FS		(2 << 4)
#define STAT0_IF_MASK		(7 << 1)
#define STAT0_IF_I2S		(0 << 1)
#define STAT0_IF_LSB16		(1 << 1)
#define STAT0_IF_LSB18		(2 << 1)
#define STAT0_IF_LSB20		(3 << 1)
#define STAT0_IF_MSB		(4 << 1)
#define STAT0_IF_LSB16MSB	(5 << 1)
#define STAT0_IF_LSB18MSB	(6 << 1)
#define STAT0_IF_LSB20MSB	(7 << 1)
#define STAT0_DC_FILTER		(1 << 0)

	unsigned char	stat1;
#define STAT1			0x80
#define STAT1_DAC_GAIN		(1 << 6)	/* gain of DAC */
#define STAT1_ADC_GAIN		(1 << 5)	/* gain of ADC */
#define STAT1_ADC_POL		(1 << 4)	/* polarity of ADC */
#define STAT1_DAC_POL		(1 << 3)	/* polarity of DAC */
#define STAT1_DBL_SPD		(1 << 2)	/* double speed playback */
#define STAT1_ADC_ON		(1 << 1)	/* ADC powered */
#define STAT1_DAC_ON		(1 << 0)	/* DAC powered */

	unsigned char	data0_0;
#define DATA0			0x00
#define DATA0_VOLUME_MASK	0x3f
#define DATA0_VOLUME(x)		(x)

	unsigned char	data0_1;
#define DATA1			0x40
#define DATA1_BASS(x)		((x) << 2)
#define DATA1_BASS_MASK		(15 << 2)
#define DATA1_TREBLE(x)		((x))
#define DATA1_TREBLE_MASK	(3)

	unsigned char	data0_2;
#define DATA2			0x80
#define DATA2_PEAKAFTER		(1 << 5)
#define DATA2_DEEMP_NONE	(0 << 3)
#define DATA2_DEEMP_32KHz	(1 << 3)
#define DATA2_DEEMP_44KHz	(2 << 3)
#define DATA2_DEEMP_48KHz	(3 << 3)
#define DATA2_MUTE		(1 << 2)
#define DATA2_FILTER_FLAT	(0 << 0)
#define DATA2_FILTER_MIN	(1 << 0)
#define DATA2_FILTER_MAX	(3 << 0)

#define EXTADDR(n)		(0xc0 | (n))
#define EXTDATA(d)		(0xe0 | (d))

	unsigned char	ext0;
#define EXT0			0
#define EXT0_CH1_GAIN(x)	(x)

	unsigned char	ext1;
#define EXT1			1
#define EXT1_CH2_GAIN(x)	(x)

	unsigned char	ext2;
#define EXT2			2
#define EXT2_MIC_GAIN_MASK	(7 << 2)
#define EXT2_MIC_GAIN(x)	((x) << 2)
#define EXT2_MIXMODE_DOUBLEDIFF	(0)
#define EXT2_MIXMODE_CH1	(1)
#define EXT2_MIXMODE_CH2	(2)
#define EXT2_MIXMODE_MIX	(3)

	unsigned char	ext4;
#define EXT4			4
#define EXT4_AGC_ENABLE		(1 << 4)
#define EXT4_INPUT_GAIN_MASK	(3)
#define EXT4_INPUT_GAIN(x)	((x) & 3)

	unsigned char	ext5;
#define EXT5			5
#define EXT5_INPUT_GAIN(x)	((x) >> 2)

	unsigned char	ext6;
#define EXT6			6
#define EXT6_AGC_CONSTANT_MASK	(7 << 2)
#define EXT6_AGC_CONSTANT(x)	((x) << 2)
#define EXT6_AGC_LEVEL_MASK	(3)
#define EXT6_AGC_LEVEL(x)	(x)
};

#define REC_MASK	(SOUND_MASK_LINE | SOUND_MASK_MIC)
#define DEV_MASK	(REC_MASK | SOUND_MASK_VOLUME | SOUND_MASK_BASS | SOUND_MASK_TREBLE)

struct uda1341 {
	struct uda1341_regs regs;
	int		active;
	unsigned short	volume;
	unsigned short	bass;
	unsigned short	treble;
	unsigned short	line;
	unsigned short	mic;
	int		mod_cnt;
};

#define ADD_FIELD(reg,field)				\
		*p++ = reg | uda->regs.field

#define ADD_EXTFIELD(reg,field)				\
		*p++ = EXTADDR(reg);			\
		*p++ = EXTDATA(uda->regs.field);

static void uda1341_sync(struct l3_client *clnt)
{
	struct uda1341 *uda = clnt->driver_data;
	char buf[24], *p = buf;

	ADD_FIELD(STAT0, stat0);
	ADD_FIELD(STAT1, stat1);

	if (p != buf)
		l3_write(clnt, UDA1341_STATUS, buf, p - buf);

	p = buf;
	ADD_FIELD(DATA0, data0_0);
	ADD_FIELD(DATA1, data0_1);
	ADD_FIELD(DATA2, data0_2);
	ADD_EXTFIELD(EXT0, ext0);
	ADD_EXTFIELD(EXT1, ext1);
	ADD_EXTFIELD(EXT2, ext2);
	ADD_EXTFIELD(EXT4, ext4);
	ADD_EXTFIELD(EXT5, ext5);
	ADD_EXTFIELD(EXT6, ext6);

	if (p != buf)
		l3_write(clnt, UDA1341_DATA0, buf, p - buf);
}

static void uda1341_cmd_init(struct l3_client *clnt)
{
	struct uda1341 *uda = clnt->driver_data;
	char buf[2];

	uda->active = 1;

	buf[0] = uda->regs.stat0 | STAT0_RST;
	buf[1] = uda->regs.stat0;

	l3_write(clnt, UDA1341_STATUS, buf, 2);

	/* resend all parameters */
	uda1341_sync(clnt);
}

static int uda1341_configure(struct l3_client *clnt, struct uda1341_cfg *conf)
{
	struct uda1341 *uda = clnt->driver_data;
	int ret = 0;

	uda->regs.stat0 &= ~(STAT0_SC_MASK | STAT0_IF_MASK);

	switch (conf->fs) {
	case 512: uda->regs.stat0 |= STAT0_SC_512FS;	break;
	case 384: uda->regs.stat0 |= STAT0_SC_384FS;	break;
	case 256: uda->regs.stat0 |= STAT0_SC_256FS;	break;
	default:  ret = -EINVAL;			break;
	}

	switch (conf->format) {
	case FMT_I2S:		uda->regs.stat0 |= STAT0_IF_I2S;	break;
	case FMT_LSB16:		uda->regs.stat0 |= STAT0_IF_LSB16;	break;
	case FMT_LSB18:		uda->regs.stat0 |= STAT0_IF_LSB18;	break;
	case FMT_LSB20:		uda->regs.stat0 |= STAT0_IF_LSB20;	break;
	case FMT_MSB:		uda->regs.stat0 |= STAT0_IF_MSB;	break;
	case FMT_LSB16MSB:	uda->regs.stat0 |= STAT0_IF_LSB16MSB;	break;
	case FMT_LSB18MSB:	uda->regs.stat0 |= STAT0_IF_LSB18MSB;	break;
	case FMT_LSB20MSB:	uda->regs.stat0 |= STAT0_IF_LSB20MSB;	break;
	}

	if (ret == 0 && uda->active) {
		char buf = uda->regs.stat0 | STAT0;
		l3_write(clnt, UDA1341_STATUS, &buf, 1);
	}
	return ret;
}

static int uda1341_update_direct(struct l3_client *clnt, int cmd, void *arg)
{
	struct uda1341 *uda = clnt->driver_data;
	struct l3_gain *v = arg;
	char newreg;
	int val;

	switch (cmd) {
	case L3_SET_VOLUME: /* set volume.  val =  0 to 100 => 62 to 1 */
		uda->regs.data0_0 = DATA0_VOLUME(62 - ((v->left * 61) / 100));
		newreg = uda->regs.data0_0 | DATA0;
		break;

	case L3_SET_BASS:   /* set bass.    val = 50 to 100 => 0 to 12 */
		val = v->left - 50;
		if (val < 0)
			val = 0;
		uda->regs.data0_1 &= ~DATA1_BASS_MASK;
		uda->regs.data0_1 |= DATA1_BASS((val * 12) / 50);
		newreg = uda->regs.data0_1 | DATA1;
		break;

	case L3_SET_TREBLE: /* set treble.  val = 50 to 100 => 0 to 3 */
		val = v->left - 50;
		if (val < 0)
			val = 0;
		uda->regs.data0_1 &= ~DATA1_TREBLE_MASK;
		uda->regs.data0_1 |= DATA1_TREBLE((val * 3) / 50);
		newreg = uda->regs.data0_1 | DATA1;
		break;

	default:
		return -EINVAL;
	}		

	if (uda->active)
		l3_write(clnt, UDA1341_DATA0, &newreg, 1);
	return 0;
}

static int uda1341_update_indirect(struct l3_client *clnt, int cmd, void *arg)
{
	struct uda1341 *uda = clnt->driver_data;
	struct l3_gain *gain = arg;
	struct l3_agc *agc = arg;
	char buf[8], *p = buf;
	int val, ret = 0;

	switch (cmd) {
	case L3_SET_GAIN:
		val = 31 - (gain->left * 31 / 100);
		switch (gain->channel) {
		case 1:
			uda->regs.ext0 = EXT0_CH1_GAIN(val);
			ADD_EXTFIELD(EXT0, ext0);
			break;

		case 2:
			uda->regs.ext1 = EXT1_CH2_GAIN(val);
			ADD_EXTFIELD(EXT1, ext1);
			break;

		default:
			ret = -EINVAL;
		}
		break;

	case L3_INPUT_AGC:
		if (agc->channel == 2) {
			if (agc->enable)
				uda->regs.ext4 |= EXT4_AGC_ENABLE;
			else
				uda->regs.ext4 &= ~EXT4_AGC_ENABLE;
#if 0
			agc->level
			agc->attack
			agc->decay
#endif
			ADD_EXTFIELD(EXT4, ext4);
		} else
			ret = -EINVAL;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (ret == 0 && uda->active)
		l3_write(clnt, UDA1341_DATA0, buf, p - buf);

	return ret;
}

static int uda1341_mixer_ioctl(struct l3_client *clnt, int cmd, void *arg)
{
	struct uda1341 *uda = clnt->driver_data;
	struct l3_gain gain;
	int val, nr = _IOC_NR(cmd), ret = 0;

	if (cmd == SOUND_MIXER_INFO) {
		struct mixer_info mi;

		strncpy(mi.id, "UDA1341", sizeof(mi.id));
		strncpy(mi.name, "Philips UDA1341", sizeof(mi.name));
		mi.modify_counter = uda->mod_cnt;
		return copy_to_user(arg, &mi, sizeof(mi));
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = get_user(val, (int *)arg);
		if (ret)
			goto out;

		gain.left    = val & 255;
		gain.right   = val >> 8;
		gain.channel = 0;

		switch (nr) {
		case SOUND_MIXER_VOLUME:
			uda->volume = val;
			uda->mod_cnt++;
			uda1341_update_direct(clnt, L3_SET_VOLUME, &gain);
			break;

		case SOUND_MIXER_BASS:
			uda->bass = val;
			uda->mod_cnt++;
			uda1341_update_direct(clnt, L3_SET_BASS, &gain);
			break;

		case SOUND_MIXER_TREBLE:
			uda->treble = val;
			uda->mod_cnt++;
			uda1341_update_direct(clnt, L3_SET_TREBLE, &gain);
			break;

		case SOUND_MIXER_LINE:
			uda->line = val;
			gain.channel = 1;
			uda->mod_cnt++;
			uda1341_update_indirect(clnt, L3_SET_GAIN, &gain);
			break;

		case SOUND_MIXER_MIC:
			uda->mic = val;
			gain.channel = 2;
			uda->mod_cnt++;
			uda1341_update_indirect(clnt, L3_SET_GAIN, &gain);
			break;

		case SOUND_MIXER_RECSRC:
			break;

		default:
			ret = -EINVAL;
		}
	}

	if (ret == 0 && _IOC_DIR(cmd) & _IOC_READ) {
		int nr = _IOC_NR(cmd);
		ret = 0;

		switch (nr) {
		case SOUND_MIXER_VOLUME:     val = uda->volume;	break;
		case SOUND_MIXER_BASS:       val = uda->bass;	break;
		case SOUND_MIXER_TREBLE:     val = uda->treble;	break;
		case SOUND_MIXER_LINE:       val = uda->line;	break;
		case SOUND_MIXER_MIC:        val = uda->mic;	break;
		case SOUND_MIXER_RECSRC:     val = REC_MASK;	break;
		case SOUND_MIXER_RECMASK:    val = REC_MASK;	break;
		case SOUND_MIXER_DEVMASK:    val = DEV_MASK;	break;
		case SOUND_MIXER_CAPS:       val = 0;		break;
		case SOUND_MIXER_STEREODEVS: val = 0;		break;
		default:	val = 0;     ret = -EINVAL;	break;
		}

		if (ret == 0)
			ret = put_user(val, (int *)arg);
	}
out:
	return ret;
}

static int uda1341_attach(struct l3_client *clnt)
{
	struct uda1341 *uda;

	uda = kmalloc(sizeof(*uda), GFP_KERNEL);
	if (!uda)
		return -ENOMEM;

	memset(uda, 0, sizeof(*uda));

	uda->volume = DEF_VOLUME | DEF_VOLUME << 8;
	uda->bass   = 50 | 50 << 8;
	uda->treble = 50 | 50 << 8;
	uda->line   = 88 | 88 << 8;
	uda->mic    = 88 | 88 << 8;

	uda->regs.stat0   = STAT0_SC_256FS | STAT0_IF_LSB16;
	uda->regs.stat1   = STAT1_DAC_GAIN | STAT1_ADC_GAIN |
			    STAT1_ADC_ON | STAT1_DAC_ON;
	uda->regs.data0_0 = DATA0_VOLUME(62 - ((DEF_VOLUME * 61) / 100));
	uda->regs.data0_1 = DATA1_BASS(0) | DATA1_TREBLE(0);
	uda->regs.data0_2 = DATA2_PEAKAFTER | DATA2_DEEMP_NONE |
			    DATA2_FILTER_MAX;
	uda->regs.ext0    = EXT0_CH1_GAIN(4);
	uda->regs.ext1    = EXT1_CH2_GAIN(4);
	uda->regs.ext2    = EXT2_MIXMODE_MIX | EXT2_MIC_GAIN(4);
	uda->regs.ext4    = EXT4_AGC_ENABLE | EXT4_INPUT_GAIN(0);
	uda->regs.ext5    = EXT5_INPUT_GAIN(0);
	uda->regs.ext6    = EXT6_AGC_CONSTANT(3) | EXT6_AGC_LEVEL(0);

	clnt->driver_data = uda;

	return 0;
}

static void uda1341_detach(struct l3_client *clnt)
{
	kfree(clnt->driver_data);
}

static int
uda1341_command(struct l3_client *clnt, int cmd, void *arg)
{
	int ret = -EINVAL;

	if (_IOC_TYPE(cmd) == 'M')
		ret = uda1341_mixer_ioctl(clnt, cmd, arg);
	else if (cmd == L3_UDA1341_CONFIGURE)
		ret = uda1341_configure(clnt, arg);

	return ret;
}

static int uda1341_open(struct l3_client *clnt)
{
	uda1341_cmd_init(clnt);
	return 0;
}

static void uda1341_close(struct l3_client *clnt)
{
	struct uda1341 *uda = clnt->driver_data;
	uda->active = 0;
}

static struct l3_ops uda1341_ops = {
	open:		uda1341_open,
	command:	uda1341_command,
	close:		uda1341_close,
};

static struct l3_driver uda1341 = {
	name:		UDA1341_NAME,
	attach_client:	uda1341_attach,
	detach_client:	uda1341_detach,
	ops:		&uda1341_ops,
	owner:		THIS_MODULE,
};

static int __init uda1341_init(void)
{
	return l3_add_driver(&uda1341);
}

static void __exit uda1341_exit(void)
{
	l3_del_driver(&uda1341);
}

module_init(uda1341_init);
module_exit(uda1341_exit);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("Philips UDA1341 CODEC driver");
