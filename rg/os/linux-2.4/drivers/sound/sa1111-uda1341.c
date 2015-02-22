/*
 * Glue audio driver for the SA1111 compagnon chip & Philips UDA1341 codec.
 *
 * Copyright (c) 2000 John Dorsey
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2000-09-04	John Dorsey	SA-1111 Serial Audio Controller support
 * 				was initially added to the sa1100-uda1341.c
 * 				driver.
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 *
 * 2001-09-23	Russell King	Remove old L3 bus driver.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/ioport.h>
#include <linux/pm.h>
#include <linux/l3/l3.h>
#include <linux/l3/uda1341.h>

#include <asm/semaphore.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/arch/assabet.h>
#include <asm/hardware/sa1111.h>

#include "sa1100-audio.h"

#undef DEBUG
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


/*
 * Definitions
 */

#define AUDIO_RATE_DEFAULT	22050

#define AUDIO_CLK_BASE		561600



/*
 * Mixer interface
 */

static struct l3_client uda1341;

static int
mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	/*
	 * We only accept mixer (type 'M') ioctls.
	 */
	if (_IOC_TYPE(cmd) != 'M')
		return -EINVAL;

	return l3_command(&uda1341, cmd, (void *)arg);
}

static struct file_operations uda1341_mixer_fops = {
	ioctl:		mixer_ioctl,
	owner:		THIS_MODULE
};


/*
 * Audio interface
 */

static int audio_clk_div = (AUDIO_CLK_BASE + AUDIO_RATE_DEFAULT/2)/AUDIO_RATE_DEFAULT;

static void sa1111_audio_init(void *dummy)
{
#ifdef CONFIG_ASSABET_NEPONSET
	if (machine_is_assabet()) {
		/* Select I2S audio (instead of AC-Link) */
		AUD_CTL = AUD_SEL_1341;
	}
#endif
#ifdef CONFIG_SA1100_JORNADA720
	if (machine_is_jornada720()) {
		/* LDD4 is speaker, LDD3 is microphone */
		PPSR &= ~(PPC_LDD3 | PPC_LDD4);
		PPDR |= PPC_LDD3 | PPC_LDD4;
		PPSR |= PPC_LDD4; /* enable speaker */
		PPSR |= PPC_LDD3; /* enable microphone */
	}
#endif

	SBI_SKCR &= ~SKCR_SELAC;

	/* Enable the I2S clock and L3 bus clock: */
	SKPCR |= (SKPCR_I2SCLKEN | SKPCR_L3CLKEN);

	/* Activate and reset the Serial Audio Controller */
	SACR0 |= (SACR0_ENB | SACR0_RST);
	mdelay(5);
	SACR0 &= ~SACR0_RST;

	/* For I2S, BIT_CLK is supplied internally. The "SA-1111
	 * Specification Update" mentions that the BCKD bit should
	 * be interpreted as "0 = output". Default clock divider
	 * is 22.05kHz.
	 *
	 * Select I2S, L3 bus. "Recording" and "Replaying"
	 * (receive and transmit) are enabled.
	 */
	SACR1 = SACR1_L3EN;
	SKAUD = audio_clk_div - 1;

	/* Initialize the UDA1341 internal state */
	l3_open(&uda1341);
}

static void sa1111_audio_shutdown(void *dummy)
{
	l3_close(&uda1341);
	SACR0 &= ~SACR0_ENB;
}

static int sa1111_audio_ioctl( struct inode *inode, struct file *file,
				uint cmd, ulong arg)
{
	long val;
	int ret = 0;

	switch (cmd) {
	case SNDCTL_DSP_STEREO:
		ret = get_user(val, (int *) arg);
		if (ret)
			return ret;
		/* the UDA1341 is stereo only */
		ret = (val == 0) ? -EINVAL : 1;
		return put_user(ret, (int *) arg);

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS:
		/* the UDA1341 is stereo only */
		return put_user(2, (long *) arg);

	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (long *) arg);
		if (ret) break;
		if (val < 8000) val = 8000;
		if (val > 48000) val = 48000;
		audio_clk_div = (AUDIO_CLK_BASE + val/2)/val;
		SKAUD = audio_clk_div - 1;
		/* fall through */

	case SOUND_PCM_READ_RATE:
		return put_user(AUDIO_CLK_BASE/audio_clk_div, (long *) arg);

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS:
		/* we can do 16-bit only */
		return put_user(AFMT_S16_LE, (long *) arg);

	default:
		/* Maybe this is meant for the mixer (as per OSS Docs) */
		return mixer_ioctl(inode, file, cmd, arg);
	}

	return ret;
}

static audio_stream_t output_stream, input_stream;

static audio_state_t audio_state = {
	output_stream:	&output_stream,
	input_stream:	&input_stream,
	skip_dma_init:	1,  /* done locally */
	hw_init:	sa1111_audio_init,
	hw_shutdown:	sa1111_audio_shutdown,
	client_ioctl:	sa1111_audio_ioctl,
	sem:		__MUTEX_INITIALIZER(audio_state.sem),
};

static int sa1111_audio_open(struct inode *inode, struct file *file)
{
        return sa1100_audio_attach(inode, file, &audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to sa1100_audio_attach().
 */
static struct file_operations sa1111_audio_fops = {
	open:		sa1111_audio_open,
	owner:		THIS_MODULE
};

static int audio_dev_id, mixer_dev_id;

static int __init sa1111_uda1341_init(void)
{
	struct uda1341_cfg cfg;
	int ret;

	if ( !(	(machine_is_assabet() && machine_has_neponset()) ||
		machine_is_jornada720() ||
		machine_is_badge4() ))
		return -ENODEV;

	if (!request_mem_region(_SACR0, 512, "sound"))
		return -EBUSY;

	ret = l3_attach_client(&uda1341, "l3-sa1111", "uda1341");
	if (ret)
		goto out;

	/* Acquire and initialize DMA */
	ret = sa1111_sac_request_dma(&output_stream.dma_ch, "SA1111 audio out",
				     SA1111_SAC_XMT_CHANNEL);
	if (ret < 0)
		goto release_l3;
	
	ret = sa1111_sac_request_dma(&input_stream.dma_ch, "SA1111 audio in",
				     SA1111_SAC_RCV_CHANNEL);
	if (ret < 0)
		goto release_dma;

	cfg.fs     = 256;
	cfg.format = FMT_I2S;
	l3_command(&uda1341, L3_UDA1341_CONFIGURE, &cfg);

	/* register devices */
	audio_dev_id = register_sound_dsp(&sa1111_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&uda1341_mixer_fops, -1);

	printk(KERN_INFO "Sound: SA1111 UDA1341: dsp id %d mixer id %d\n",
		audio_dev_id, mixer_dev_id);
	return 0;

release_dma:
	sa1100_free_dma(output_stream.dma_ch);
release_l3:
	l3_detach_client(&uda1341);
out:
	release_mem_region(_SACR0, 512);
	return ret;
}

static void __exit sa1111_uda1341_exit(void)
{
	unregister_sound_dsp(audio_dev_id);
	unregister_sound_mixer(mixer_dev_id);
	sa1100_free_dma(output_stream.dma_ch);
	sa1100_free_dma(input_stream.dma_ch);
	l3_detach_client(&uda1341);

	release_mem_region(_SACR0, 512);
}

module_init(sa1111_uda1341_init);
module_exit(sa1111_uda1341_exit);

MODULE_AUTHOR("John Dorsey, Nicolas Pitre");
MODULE_DESCRIPTION("Glue audio driver for the SA1111 compagnon chip & Philips UDA1341 codec.");

EXPORT_NO_SYMBOLS;
