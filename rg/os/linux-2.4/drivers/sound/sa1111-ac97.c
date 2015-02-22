/*
 * Glue audio driver for the CS4205 and CS4201 AC'97 codecs.
 * largely based on the framework provided by sa1111-uda1341.c.
 *
 * Copyright (c) 2002 Bertrik Sikken (bertrik.sikken@technolution.nl)
 * Copyright (c) 2002 Robert Whaley (rwhaley@applieddata.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * This driver makes use of the ac97_codec module (for mixer registers)
 * and the sa1100-audio module (for DMA).
 *
 * History:
 *
 * 2002-04-04	Initial version.
 * 2002-04-10	Updated mtd_audio_init to improve choppy sound
 *              and hanging sound issue.
 * 2002-05-16   Updated for ADS Bitsy+ Robert Whaley
 * 2002-06-28   Cleanup and added retry for read register timeouts
 * 2002-08-14   Updated for ADS AGC Robert Whaley
 * 2002-12-26   Cleanup, remove CONFIG_PM (it's handled by sa1100-audio.c)
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/ac97_codec.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/hardware/sa1111.h>

#include "sa1100-audio.h"

/* SAC FIFO depth, low nibble is transmit fifo, high nibble is receive FIFO */
#define SAC_FIFO_DEPTH		0x77

// #define DEBUG

#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif

/*
	Our codec data
*/
static struct ac97_codec ac97codec;
static int audio_dev_id, mixer_dev_id;
static audio_stream_t output_stream, input_stream;

/* proc info */

struct proc_dir_entry *ac97_ps;

static int sa1111_ac97_set_adc_rate(long rate);
static void sa1111_ac97_write_reg(struct ac97_codec *dev, u8 reg, u16 val);
static u16 sa1111_ac97_read_reg(struct ac97_codec *dev, u8 reg);

static int
mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	/*
	 * We only accept mixer (type 'M') ioctls.
	 */
	if (_IOC_TYPE(cmd) != 'M') {
		return -EINVAL;
	}

	/* pass the ioctl to the ac97 mixer */
	return ac97codec.mixer_ioctl(&ac97codec, cmd, arg);
}


static struct file_operations sa1111_ac97_mixer_fops = {
	ioctl:		mixer_ioctl,
	owner:		THIS_MODULE
};

static void sa1111_ac97_power_off(void *dummy)
{
#ifdef CONFIG_SA1100_ADSBITSYPLUS
	/* turn off audio and audio amp */
	ADS_CPLD_PCON |= (ADS_PCON_AUDIO_ON | ADS_PCON_AUDIOPA_ON);

	/* make GPIO11 high impeadence */
	GPDR &= ~GPIO_GPIO11;

	/* disable SACR0 so we can make these pins high impeadence */
	SACR0 &= ~SACR0_ENB;

	/* make BIT_CLK, SDATA_OUT, and SYNC high impeadence */
	PC_DDR |= (GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3);

#endif

#ifdef CONFIG_SA1100_ADSAGC
	/* turn off audio and audio amp */
	ADS_CR1 &= ~(ADS_CR1_CODEC | ADS_CR1_AMP);

	/* disable SACR0 so we can make these pins high impeadence */
	SACR0 &= ~SACR0_ENB;

	/* make BIT_CLK, SDATA_OUT, and SYNC high impeadence */
	PC_DDR |= (GPIO_GPIO1 | GPIO_GPIO2 | GPIO_GPIO3);

#endif
}


static void sa1111_ac97_power_on(void *dummy)
{
	int ret, i;

	/* disable L3 */
	SACR1 = 0;

	SKPCR |= (SKPCR_ACCLKEN);	/* enable ac97 clock */
	udelay(50);

	/* BIT_CLK is input to SA1111, DMA thresholds 9 (both dirs) */
	SACR0 |= SACR0_BCKD | (SAC_FIFO_DEPTH << 8);

	/* reset SAC registers */
	SACR0 &= ~SACR0_RST;
	udelay(50);
	SACR0 |= SACR0_RST;
	udelay(50);
	SACR0 &= ~SACR0_RST;

	/* setup SA1111 to use AC'97 */
	SBI_SKCR |= SKCR_SELAC;		/* select ac97 */
	udelay(50);

	/* issue a cold AC97 reset */
#ifdef CONFIG_SA1100_ADSBITSYPLUS

	/* initialize reset line */
	GAFR &= ~GPIO_GPIO11;
	GPDR |= GPIO_GPIO11;
	GPSR = GPIO_GPIO11;

	/* turn on audio and audio amp */
	ADS_CPLD_PCON &= ~(ADS_PCON_AUDIO_ON | ADS_PCON_AUDIOPA_ON);
	mdelay(5);

	/* reset by lowering the reset pin momentarily */
	DPRINTK("reseting codec via GPIO11\n");
	GPCR = GPIO_GPIO11;
	udelay(5);
	GPSR = GPIO_GPIO11;
	udelay(10);

#endif
#ifdef CONFIG_SA1100_ADSAGC

	/* turn on audio and audio amp */
	DPRINTK("before turning on power.  ADS_CR1: %x\n", ADS_CR1);
	ADS_CR1 |= (ADS_CR1_AMP | ADS_CR1_CODEC);
	DPRINTK("after turnning on power.  ADS_CR1: %x\n", ADS_CR1);
	mdelay(5);

	/* reset by lowering the reset pin momentarily */
	DPRINTK("reseting codec via CPLD\n");
	ADS_CR1 |= ADS_CR1_AUDIO_RST;
	DPRINTK("after reset1.  ADS_CR1: %x\n", ADS_CR1);
	udelay(5);
	ADS_CR1 &= ~ADS_CR1_AUDIO_RST;
	DPRINTK("after reset2.  ADS_CR1: %x\n", ADS_CR1);
	udelay(10);

#endif
	SACR2 = 0;
	udelay(50);

	DPRINTK("before SW reset:  SACR2: %x\n", SACR2);
	SACR2 = SACR2_RESET;
	DPRINTK("after SW reset:  SACR2: %x\n", SACR2);
	udelay(50);

	/* set AC97 slot 3 and 4 (PCM out) to valid */
	SACR2 = (SACR2_RESET | SACR2_TS3V | SACR2_TS4V);

	/* enable SAC */
	SACR0 |= SACR0_ENB;

	i = 100;
	while (!(SASR1 & SASR1_CRDY)) {
		if (!i--) {
			printk("Didn't get CRDY.  SASR1=%x SKID=%x\n", SASR1, SBI_SKID);
			break;
		}
		udelay(50);
	}

	if (!(ret = ac97_probe_codec(&ac97codec))) {
		printk("ac97_probe_codec failed  (%d)\n", ret);
		return;
	}

	/* mic ADC on, disable VRA, disable VRM */
	sa1111_ac97_write_reg(&ac97codec, AC97_EXTENDED_STATUS, 0x0200);
}


/*
 * Audio interface
 */


static int sa1111_ac97_audio_ioctl(struct inode *inode, struct file *file,
			     uint cmd, ulong arg)
{
	long val;
	int ret = 0;

	DPRINTK("sa1111_ac97_audio_ioctl\n");

	/*
	 * These are platform dependent ioctls which are not handled by the
	 * generic sa1100-audio module.
	 */
	switch (cmd) {
	case SNDCTL_DSP_STEREO:
		ret = get_user(val, (int *) arg);
		if (ret) {
			return ret;
		}
		/* the cs42xx is stereo only */
		ret = (val == 0) ? -EINVAL : 1;
		return put_user(ret, (int *) arg);

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS:
		/* the cs42xx is stereo only */
		return put_user(2, (long *) arg);

#define SA1100_AC97_IOCTL_EXTRAS

#ifdef SA1100_AC97_IOCTL_EXTRAS

#define SNDCTL_DSP_AC97_CMD _SIOWR('P', 99, int)
#define SNDCTL_DSP_INPUT_SPEED _SIOWR('P', 98, int)
#define SOUND_PCM_READ_INPUT_RATE _SIOWR('P', 97, int)

	case SNDCTL_DSP_AC97_CMD:

		ret = get_user(val, (long *) arg);
		if (ret) {
			break;
		}
		sa1111_ac97_write_reg(&ac97codec, (u8) ((val & 0xff000000) >> 24), (u16) (val & 0xffff));
		return 0;


	case SNDCTL_DSP_INPUT_SPEED:
		ret = get_user(val, (long *) arg);
		// acc code here to set the speed
		if (ret) {
			break;
		}
		// note that this only changes the ADC rate, not the
		// rate of the DAC.
		ret = sa1111_ac97_set_adc_rate(val);
		if (ret)
		  break;
		return put_user(val, (long *) arg);

	case SOUND_PCM_READ_INPUT_RATE:

		return put_user((long) sa1111_ac97_read_reg(&ac97codec, 0x32), (long *) arg);


#endif

	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (long *) arg);
		if (ret) {
			break;
		}

	case SOUND_PCM_READ_RATE:
		/* only 48 kHz playback is supported by the SA1111 */
		return put_user(48000L, (long *) arg);

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS:
		/* we can do 16-bit only */
		return put_user(AFMT_S16_LE, (long *) arg);

	default:
		/* Maybe this is meant for the mixer (As per OSS Docs) */
		return mixer_ioctl(inode, file, cmd, arg);
	}

	return ret;
}


static audio_state_t audio_state = {
	output_stream:	&output_stream,
	input_stream:	&input_stream,
	skip_dma_init:	1,  /* done locally */
	hw_init:        sa1111_ac97_power_on,
	hw_shutdown:	sa1111_ac97_power_off,
	client_ioctl:	sa1111_ac97_audio_ioctl,
	sem:			__MUTEX_INITIALIZER(audio_state.sem),
};


static int sa1111_ac97_audio_open(struct inode *inode, struct file *file)
{
	return sa1100_audio_attach(inode, file, &audio_state);
}


/*
 * Missing fields of this structure will be patched with the call
 * to sa1100_audio_attach().
 */
static struct file_operations sa1111_ac97_audio_fops = {
	open:		sa1111_ac97_audio_open,
	owner:		THIS_MODULE
};


static void sa1111_ac97_write_reg(struct ac97_codec *dev, u8 reg, u16 val)
{
	int i;

	/* reset status bits */
	SASCR = SASCR_DTS;

	/* write command and data registers */
	ACCAR = reg << 12;
	ACCDR = val << 4;

	/* wait for data to be transmitted */
	i = 0;
	while ((SASR1 & SASR1_CADT) == 0) {
		udelay(50);
		if (++i > 10) {
			DPRINTK("sa1111_ac97_write_reg failed (data not transmitted. SASR1: %x)\n", SASR1);
			break;
		}
	}

	DPRINTK("<%03d> sa1111_ac97_write_reg, [%02X]=%04X\n", i, reg, val);
}


static u16 sa1111_ac97_read_reg(struct ac97_codec *dev, u8 reg)
{
	u16		val;
	int		i;
	int		retry = 10;

	do {
		/* reset status bits */
		SASCR = SASCR_RDD | SASCR_STO;

		/* write command register */
		ACCAR = (reg | 0x80) << 12;
		ACCDR = 0;

		/* wait for SADR bit in SASR1 */
		i = 0;
		while ((SASR1 & SASR1_SADR) == 0) {
			udelay(50);
			if (++i > 10) {
				DPRINTK("<---> sa1111_ac97_read_reg failed\n");
				retry--;
				break;
			}
			if ((SASR1 & SASR1_RSTO) != 0) {
				DPRINTK("sa1111_ac97_read_reg *timeout*\n");
				retry--;
				break;
			}
		}

	} while ((SASR1 & SASR1_SADR) == 0 && retry > 0);

	val = ACSDR >> 4;

	DPRINTK("<%03d> sa1111_ac97_read_reg, [%02X]=%04X\n", i, reg, val);
	return val;
}


/* wait for codec ready */
static void sa1111_ac97_ready(struct ac97_codec *dev)
{
	int i;
	u16	val;

	i = 0;
	while ((SASR1 & SASR1_CRDY) == 0) {
		udelay(50);
		if (++i > 10) {
			DPRINTK("sa1111_ac97_ready failed\n");
			return;
		}
	}
	DPRINTK("codec_ready bit took %d cycles\n", i);

	/* Wait for analog parts of codec to initialise */
	i = 0;
	do {
		val = sa1111_ac97_read_reg(&ac97codec, AC97_POWER_CONTROL);
		if (++i > 100) {
			break;
		}
		mdelay(10);
	} while ((val & 0xF) != 0xF || val == 0xFFFF);

	/* the cs42xx typically takes 150 ms to initialise */

	DPRINTK("analog init took %d cycles\n", i);
}


static int __init sa1111_ac97_init(void)
{
	int ret;

	// SBI_SKCR |= SKCR_RCLKEN;

	DPRINTK("sa1111_ac97_init\n");

	/* install the ac97 mixer module */
	ac97codec.codec_read	= sa1111_ac97_read_reg;
	ac97codec.codec_write 	= sa1111_ac97_write_reg;
	ac97codec.codec_wait 	= sa1111_ac97_ready;

	/* Acquire and initialize DMA */
	ret = sa1111_sac_request_dma(&output_stream.dma_ch, "SA1111 audio out",
				     SA1111_SAC_XMT_CHANNEL);
	if (ret < 0) {
		printk("DMA request for SAC output failed\n");
		return ret;
	}

	ret = sa1111_sac_request_dma(&input_stream.dma_ch, "SA1111 audio in",
				     SA1111_SAC_RCV_CHANNEL);
	if (ret < 0) {
		printk("DMA request for SAC input failed\n");
		sa1100_free_dma(output_stream.dma_ch);
		return ret;
	}
	/* register devices */
	audio_dev_id = register_sound_dsp(&sa1111_ac97_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&sa1111_ac97_mixer_fops, -1);


	/* setup proc entry */
	ac97_ps = create_proc_read_entry ("driver/sa1111-ac97", 0, NULL,
					  ac97_read_proc, &ac97codec);

	return 0;
}


static void __exit sa1111_ac97_exit(void)
{
	SKPCR &= ~SKPCR_ACCLKEN;		/* disable ac97 clock */
	SBI_SKCR &= ~SKCR_SELAC;		/* deselect ac97 */

	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
	sa1100_free_dma(output_stream.dma_ch);
	sa1100_free_dma(input_stream.dma_ch);
}

static int sa1111_ac97_set_adc_rate(long rate)
{

  // note this only changes the rate of the ADC, the DAC is fixed at 48K.
  // this is due to limitations of the SA1111 chip

  u16 code = rate;

  switch (rate) {
  case  8000:
  case 11025:
  case 16000:
  case 22050:
  case 32000:
  case 44100:
  case 48000:
    break;
  default:
    return -1;
  }
  sa1111_ac97_write_reg(&ac97codec, 0x2A, 0x0001);
  sa1111_ac97_write_reg(&ac97codec, 0x32, code);
  return 0;
}

module_init(sa1111_ac97_init);
module_exit(sa1111_ac97_exit);

MODULE_AUTHOR("Bertrik Sikken, Technolution B.V., Netherlands");
MODULE_DESCRIPTION("Glue audio driver for AC'97 codec");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
