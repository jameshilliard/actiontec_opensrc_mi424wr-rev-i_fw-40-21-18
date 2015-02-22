/*
 * Common audio handling for the SA11x0 processor
 *
 * Copyright (C) 2000, 2001 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 *
 * This module handles the generic buffering/DMA/mmap audio interface for
 * codecs connected to the SA1100 chip.  All features depending on specific
 * hardware implementations like supported audio formats or samplerates are
 * relegated to separate specific modules.
 *
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial release.
 *
 * 2000-06-10	Erik Bunce	Add initial poll support.
 *
 * 2000-08-22	Nicolas Pitre	Removed all DMA stuff. Now using the
 * 				generic SA1100 DMA interface.
 *
 * 2000-11-30	Nicolas Pitre	- Validation of opened instances;
 * 				- Power handling at open/release time instead
 * 				  of driver load/unload;
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 *
 * 2001-07-22	Nicolas Pitre	- added mmap() and realtime support
 * 				- corrected many details to better comply
 * 				  with the OSS API
 *
 * 2001-10-19	Nicolas Pitre	- brought DMA registration processing
 * 				  into this module for better ressource
 * 				  management.  This also fixes a bug
 * 				  with the suspend/resume logic.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/sysrq.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include "sa1100-audio.h"


#undef DEBUG 
/* #define DEBUG 1 */
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif


#define AUDIO_NAME		"sa1100-audio"
#define AUDIO_NBFRAGS_DEFAULT	8
#define AUDIO_FRAGSIZE_DEFAULT	8192

#define NEXT_BUF(_s_,_b_) { \
	(_s_)->_b_##_idx++; \
	(_s_)->_b_##_idx %= (_s_)->nbfrags; \
	(_s_)->_b_ = (_s_)->buffers + (_s_)->_b_##_idx; }

#define AUDIO_ACTIVE(state)	((state)->rd_ref || (state)->wr_ref)

/*
 * This function frees all buffers
 */

static void audio_clear_buf(audio_stream_t * s)
{
	DPRINTK("audio_clear_buf\n");

	/* ensure DMA won't run anymore */
	s->active = 0;
	s->stopped = 0;
	sa1100_dma_flush_all(s->dma_ch);

	if (s->buffers) {
		int frag;
		for (frag = 0; frag < s->nbfrags; frag++) {
			if (!s->buffers[frag].master)
				continue;
			consistent_free(s->buffers[frag].start,
					s->buffers[frag].master,
					s->buffers[frag].dma_addr);
		}
		kfree(s->buffers);
		s->buffers = NULL;
	}

	s->buf_idx = 0;
	s->buf = NULL;
}


/*
 * This function allocates the buffer structure array and buffer data space
 * according to the current number of fragments and fragment size.
 */

static int audio_setup_buf(audio_stream_t * s)
{
	int frag;
	int dmasize = 0;
	char *dmabuf = NULL;
	dma_addr_t dmaphys = 0;

	if (s->buffers)
		return -EBUSY;

	s->buffers = (audio_buf_t *)
		kmalloc(sizeof(audio_buf_t) * s->nbfrags, GFP_KERNEL);
	if (!s->buffers)
		goto err;
	memset(s->buffers, 0, sizeof(audio_buf_t) * s->nbfrags);

	for (frag = 0; frag < s->nbfrags; frag++) {
		audio_buf_t *b = &s->buffers[frag];

		/*
		 * Let's allocate non-cached memory for DMA buffers.
		 * We try to allocate all memory at once.
		 * If this fails (a common reason is memory fragmentation),
		 * then we allocate more smaller buffers.
		 */
		if (!dmasize) {
			dmasize = (s->nbfrags - frag) * s->fragsize;
			do {
			  	dmabuf = consistent_alloc(GFP_KERNEL|GFP_DMA,
							  dmasize,
							  &dmaphys);
				if (!dmabuf)
					dmasize -= s->fragsize;
			} while (!dmabuf && dmasize);
			if (!dmabuf)
				goto err;
			b->master = dmasize;
			memzero(dmabuf, dmasize);
		}

		b->start = dmabuf;
		b->dma_addr = dmaphys;
		b->stream = s;
		sema_init(&b->sem, 1);
		DPRINTK("buf %d: start %p dma %p\n", frag, b->start,
			b->dma_addr);

		dmabuf += s->fragsize;
		dmaphys += s->fragsize;
		dmasize -= s->fragsize;
	}

	s->buf_idx = 0;
	s->buf = &s->buffers[0];
	s->bytecount = 0;
	s->getptrCount = 0;
	s->fragcount = 0;

	return 0;

err:
	printk(AUDIO_NAME ": unable to allocate audio memory\n ");
	audio_clear_buf(s);
	return -ENOMEM;
}


/*
 * This function yanks all buffers from the DMA code's control and
 * resets them ready to be used again.
 */

static void audio_reset_buf(audio_stream_t * s)
{
	int frag;

	s->active = 0;
	s->stopped = 0;
	sa1100_dma_flush_all(s->dma_ch);
	if (s->buffers) {
		for (frag = 0; frag < s->nbfrags; frag++) {
			audio_buf_t *b = &s->buffers[frag];
			b->size = 0;
			sema_init(&b->sem, 1);
		}
	}
	s->bytecount = 0;
	s->getptrCount = 0;
	s->fragcount = 0;
}


/*
 * DMA callback functions
 */

static void audio_dmaout_done_callback(void *buf_id, int size)
{
	audio_buf_t *b = (audio_buf_t *) buf_id;
	audio_stream_t *s = b->stream;

	/* Accounting */
	s->bytecount += size;
	s->fragcount++;

	/* Recycle buffer */
	if (s->mapped)
		sa1100_dma_queue_buffer(s->dma_ch, buf_id,
					b->dma_addr, s->fragsize);
	else
		up(&b->sem);

	/* And any process polling on write. */
	wake_up(&s->wq);
}

static void audio_dmain_done_callback(void *buf_id, int size)
{
	audio_buf_t *b = (audio_buf_t *) buf_id;
	audio_stream_t *s = b->stream;

	/* Accounting */
	s->bytecount += size;
	s->fragcount++;

	/* Recycle buffer */
	if (s->mapped) {
		sa1100_dma_queue_buffer(s->dma_ch, buf_id,
					b->dma_addr, s->fragsize);
	} else {
		b->size = size;
		up(&b->sem);
	}

	/* And any process polling on write. */
	wake_up(&s->wq);
}

static int audio_sync(struct file *file)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s = state->output_stream;
	audio_buf_t *b;

	DPRINTK("audio_sync\n");

	if (!(file->f_mode & FMODE_WRITE) || !s->buffers || s->mapped)
		return 0;

	/*
	 * Send current buffer if it contains data.  Be sure to send
	 * a full sample count.
	 */
	b = s->buf;
	if (b->size &= ~3) {
		down(&b->sem);
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, b->size);
		b->size = 0;
		NEXT_BUF(s, buf);
	}

	/*
	 * Let's wait for the last buffer we sent i.e. the one before the
	 * current buf_idx.  When we acquire the semaphore, this means either:
	 * - DMA on the buffer completed or
	 * - the buffer was already free thus nothing else to sync.
	 */
	b = s->buffers + ((s->nbfrags + s->buf_idx - 1) % s->nbfrags);
	if (down_interruptible(&b->sem))
		return -EINTR;
	up(&b->sem);
	return 0;
}


static int audio_write(struct file *file, const char *buffer,
		       size_t count, loff_t * ppos)
{
	const char *buffer0 = buffer;
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s = state->output_stream;
	int chunksize, ret = 0;

	DPRINTK("audio_write: count=%d\n", count);

	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (s->mapped)
		return -ENXIO;
	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;

	while (count > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become free */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&b->sem))
				break;
		}

		/* Feed the current buffer */
		chunksize = s->fragsize - b->size;
		if (chunksize > count)
			chunksize = count;
		DPRINTK("write %d to %d\n", chunksize, s->buf_idx);
		if (copy_from_user(b->start + b->size, buffer, chunksize)) {
			up(&b->sem);
			return -EFAULT;
		}
		b->size += chunksize;
		buffer += chunksize;
		count -= chunksize;
		if (b->size < s->fragsize) {
			up(&b->sem);
			break;
		}

		/* Send current buffer to dma */
		s->active = 1;
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, b->size);
		b->size = 0;	/* indicate that the buffer has been sent */
		NEXT_BUF(s, buf);
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	DPRINTK("audio_write: return=%d\n", ret);
	return ret;
}


static inline void audio_check_tx_spin(audio_state_t *state)
{
	/*
	 * With some codecs like the Philips UDA1341 we must ensure
	 * there is an output stream at any time while recording since
	 * this is how the UDA1341 gets its clock from the SA1100.
	 * So while there is no playback data to send, the output DMA
	 * will spin with all zeroes.  We use the cache flush special
	 * area for that.
	 */
	if (state->need_tx_for_rx && !state->tx_spinning) {
		sa1100_dma_set_spin(state->output_stream->dma_ch,
				    (dma_addr_t)FLUSH_BASE_PHYS, 2048);
		state->tx_spinning = 1;
	}
}


static void audio_prime_dma(audio_stream_t *s)
{
	int i;

	s->active = 1;
	for (i = 0; i < s->nbfrags; i++) {
		audio_buf_t *b = s->buf;
		down(&b->sem);
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, s->fragsize);
		NEXT_BUF(s, buf);
	}
}


static int audio_read(struct file *file, char *buffer,
		      size_t count, loff_t * ppos)
{
	char *buffer0 = buffer;
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s = state->input_stream;
	int chunksize, ret = 0;

	DPRINTK("audio_read: count=%d\n", count);

	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (s->mapped)
		return -ENXIO;

	if (!s->active) {
		if (!s->buffers && audio_setup_buf(s))
			return -ENOMEM;
		audio_check_tx_spin(state);
		audio_prime_dma(s);
	}

	while (count > 0) {
		audio_buf_t *b = s->buf;

		/* Wait for a buffer to become full */
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			if (down_trylock(&b->sem))
				break;
		} else {
			ret = -ERESTARTSYS;
			if (down_interruptible(&b->sem))
				break;
		}

		/* Grab data from the current buffer */
		chunksize = b->size;
		if (chunksize > count)
			chunksize = count;
		DPRINTK("read %d from %d\n", chunksize, s->buf_idx);
		if (copy_to_user(buffer,
				 b->start + s->fragsize - b->size,
				 chunksize)) {
			up(&b->sem);
			return -EFAULT;
		}
		b->size -= chunksize;
		buffer += chunksize;
		count -= chunksize;
		if (b->size > 0) {
			up(&b->sem);
			break;
		}

		/* Make current buffer available for DMA again */
		sa1100_dma_queue_buffer(s->dma_ch, (void *) b,
					b->dma_addr, s->fragsize);
		NEXT_BUF(s, buf);
	}

	if ((buffer - buffer0))
		ret = buffer - buffer0;
	DPRINTK("audio_read: return=%d\n", ret);
	return ret;
}


static int audio_mmap(struct file *file, struct vm_area_struct *vma)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *s;
	unsigned long size, vma_addr;
	int i, ret;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (vma->vm_flags & VM_WRITE) {
		if (!state->wr_ref)
			return -EINVAL;;
		s = state->output_stream;
	} else if (vma->vm_flags & VM_READ) {
		if (!state->rd_ref)
			return -EINVAL;
		s = state->input_stream;
	} else return -EINVAL;

	if (s->mapped)
		return -EINVAL;
	size = vma->vm_end - vma->vm_start;
	if (size != s->fragsize * s->nbfrags)
		return -EINVAL;
	if (!s->buffers && audio_setup_buf(s))
		return -ENOMEM;
	vma_addr = vma->vm_start;
	for (i = 0; i < s->nbfrags; i++) {
		audio_buf_t *buf = &s->buffers[i];
		if (!buf->master)
			continue;
		ret = remap_page_range(vma_addr, buf->dma_addr, buf->master, vma->vm_page_prot);
		if (ret)
			return ret;
		vma_addr += buf->master;
	}
	s->mapped = 1;

	return 0;
}


static unsigned int audio_poll(struct file *file,
			       struct poll_table_struct *wait)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	unsigned int mask = 0;
	int i;

	DPRINTK("audio_poll(): mode=%s%s\n",
		(file->f_mode & FMODE_READ) ? "r" : "",
		(file->f_mode & FMODE_WRITE) ? "w" : "");

	if (file->f_mode & FMODE_READ) {
		/* Start audio input if not already active */
		if (!is->active) {
			if (!is->buffers && audio_setup_buf(is))
				return -ENOMEM;
			audio_check_tx_spin(state);
			audio_prime_dma(is);
		}
		poll_wait(file, &is->wq, wait);
	}

	if (file->f_mode & FMODE_WRITE) {
		if (!os->buffers && audio_setup_buf(os))
			return -ENOMEM;
		poll_wait(file, &os->wq, wait);
	}

	if (file->f_mode & FMODE_READ) {
		if (is->mapped) {
/* if the buffer is mapped assume we care that there are more bytes available than 
   when we last asked using SNDCTL_DSP_GETxPTR */
			if (is->bytecount != is->getptrCount)
				mask |= POLLIN | POLLRDNORM;
		} else {
			for (i = 0; i < is->nbfrags; i++) {
				if (atomic_read(&is->buffers[i].sem.count) > 0) {
					mask |= POLLIN | POLLRDNORM;
					break;
				}
			}
		}
	}
	if (file->f_mode & FMODE_WRITE) {
		if (os->mapped) {
			if (os->bytecount != os->getptrCount)
				mask |= POLLOUT | POLLWRNORM;
		} else {
			for (i = 0; i < os->nbfrags; i++) {
				if (atomic_read(&os->buffers[i].sem.count) > 0) {
					mask |= POLLOUT | POLLWRNORM;
					break;
				}
			}
		}
	}

	DPRINTK("audio_poll() returned mask of %s%s\n",
		(mask & POLLIN) ? "r" : "",
		(mask & POLLOUT) ? "w" : "");

	return mask;
}


static loff_t audio_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}


static int audio_set_fragments(audio_stream_t *s, int val)
{
	if (s->active)
		return -EBUSY;
	if (s->buffers)
		audio_clear_buf(s);
	s->nbfrags = (val >> 16) & 0x7FFF;
	val &= 0xffff;
	if (val < 4)
		val = 4;
	if (val > 15)
		val = 15;
	s->fragsize = 1 << val;
	if (s->nbfrags < 2)
		s->nbfrags = 2;
	if (s->nbfrags * s->fragsize > 128 * 1024)
		s->nbfrags = 128 * 1024 / s->fragsize;
	if (audio_setup_buf(s))
		return -ENOMEM;
	return val|(s->nbfrags << 16);
}

static int audio_ioctl(struct inode *inode, struct file *file,
		       uint cmd, ulong arg)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;
	long val;

	/* dispatch based on command */
	switch (cmd) {
	case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *)arg);

	case SNDCTL_DSP_GETBLKSIZE:
		if (file->f_mode & FMODE_WRITE)
			return put_user(os->fragsize, (int *)arg);
		else
			return put_user(is->fragsize, (int *)arg);

	case SNDCTL_DSP_GETCAPS:
		val = DSP_CAP_REALTIME|DSP_CAP_TRIGGER|DSP_CAP_MMAP;
		if (is && os)
			val |= DSP_CAP_DUPLEX;
		return put_user(val, (int *)arg);

	case SNDCTL_DSP_SETFRAGMENT:
		if (get_user(val, (long *) arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			int ret = audio_set_fragments(is, val);
			if (ret < 0)
				return ret;
			ret = put_user(ret, (int *)arg);
			if (ret)
				return ret;
		}
		if (file->f_mode & FMODE_WRITE) {
			int ret = audio_set_fragments(os, val);
			if (ret < 0)
				return ret;
			ret = put_user(ret, (int *)arg);
			if (ret)
				return ret;
		}
		return 0;

	case SNDCTL_DSP_SYNC:
		return audio_sync(file);

	case SNDCTL_DSP_SETDUPLEX:
		return 0;

	case SNDCTL_DSP_POST:
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
		val = 0;
		if (file->f_mode & FMODE_READ && is->active && !is->stopped)
			val |= PCM_ENABLE_INPUT;
		if (file->f_mode & FMODE_WRITE && os->active && !os->stopped)
			val |= PCM_ENABLE_OUTPUT;
		return put_user(val, (int *)arg);

	case SNDCTL_DSP_SETTRIGGER:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			if (val & PCM_ENABLE_INPUT) {
				if (!is->active) {
					if (!is->buffers && audio_setup_buf(is))
						return -ENOMEM;
					audio_prime_dma(is);
				}
				audio_check_tx_spin(state);
				if (is->stopped) {
					is->stopped = 0;
					sa1100_dma_resume(is->dma_ch);
				}
			} else {
				sa1100_dma_stop(is->dma_ch);
				is->stopped = 1;
			}
		}
		if (file->f_mode & FMODE_WRITE) {
			if (val & PCM_ENABLE_OUTPUT) {
				if (!os->active) {
					if (!os->buffers && audio_setup_buf(os))
						return -ENOMEM;
					if (os->mapped)
						audio_prime_dma(os);
				}
				if (os->stopped) {
					os->stopped = 0;
					sa1100_dma_resume(os->dma_ch);
				}
			} else {
				sa1100_dma_stop(os->dma_ch);
				os->stopped = 1;
			}
		}
		return 0;

	case SNDCTL_DSP_GETOPTR:
	case SNDCTL_DSP_GETIPTR:
	    {
		count_info inf = { 0, };
		audio_stream_t *s = (cmd == SNDCTL_DSP_GETOPTR) ? os : is;
		audio_buf_t *b;
		dma_addr_t ptr;
		int bytecount, offset, flags;

		if ((s == is && !(file->f_mode & FMODE_READ)) ||
		    (s == os && !(file->f_mode & FMODE_WRITE)))
			return -EINVAL;
		if (s->active) {
			save_flags_cli(flags);
			if (sa1100_dma_get_current(s->dma_ch, (void *)&b, &ptr) == 0) {
				offset = ptr - b->dma_addr;
				inf.ptr = (b - s->buffers) * s->fragsize + offset;
			} else offset = 0;
			bytecount = s->bytecount + offset;
			s->getptrCount = s->bytecount;	/* so poll can tell if it changes */
			inf.blocks = s->fragcount;
			s->fragcount = 0;
			restore_flags(flags);
			if (bytecount < 0)
				bytecount = 0;
			inf.bytes = bytecount;
		}
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }

	case SNDCTL_DSP_GETOSPACE:
	    {
		audio_buf_info inf = { 0, };
		int i;

		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		if (!os->buffers && audio_setup_buf(os))
			return -ENOMEM;
		for (i = 0; i < os->nbfrags; i++) {
			if (atomic_read(&os->buffers[i].sem.count) > 0) {
				if (os->buffers[i].size == 0)
					inf.fragments++;
				inf.bytes += os->fragsize - os->buffers[i].size;
			}
		}
		inf.fragstotal = os->nbfrags;
		inf.fragsize = os->fragsize;
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }

	case SNDCTL_DSP_GETISPACE:
	    {
		audio_buf_info inf = { 0, };
		int i;

		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		if (!is->buffers && audio_setup_buf(is))
			return -ENOMEM;
		for (i = 0; i < is->nbfrags; i++) {
			if (atomic_read(&is->buffers[i].sem.count) > 0) {
				if (is->buffers[i].size == is->fragsize)
					inf.fragments++;
				inf.bytes += is->buffers[i].size;
			}
		}
		inf.fragstotal = is->nbfrags;
		inf.fragsize = is->fragsize;
		return copy_to_user((void *)arg, &inf, sizeof(inf));
	    }

	case SNDCTL_DSP_NONBLOCK:
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_RESET:
		if (file->f_mode & FMODE_READ) {
			if (state->tx_spinning) {
				sa1100_dma_set_spin(os->dma_ch, 0, 0);
				state->tx_spinning = 0;
			}
			audio_reset_buf(is);
		}
		if (file->f_mode & FMODE_WRITE) {
			audio_reset_buf(os);
		}
		return 0;

	default:
		/*
		 * Let the client of this module handle the
		 * non generic ioctls
		 */
		return state->client_ioctl(inode, file, cmd, arg);
	}

	return 0;
}


static int audio_release(struct inode *inode, struct file *file)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	DPRINTK("audio_release\n");

	down(&state->sem);

	if (file->f_mode & FMODE_READ) {
		if (state->tx_spinning) {
			sa1100_dma_set_spin(state->output_stream->dma_ch, 0, 0);
			state->tx_spinning = 0;
		}
		audio_clear_buf(state->input_stream);
		if (!state->skip_dma_init) {
			sa1100_free_dma(state->input_stream->dma_ch);
			if (state->need_tx_for_rx && !state->wr_ref)
				sa1100_free_dma(state->output_stream->dma_ch);
		}
		state->rd_ref = 0;
	}

	if (file->f_mode & FMODE_WRITE) {
		audio_sync(file);
		audio_clear_buf(state->output_stream);
		if (!state->skip_dma_init)
			if (!state->need_tx_for_rx || !state->rd_ref)
				sa1100_free_dma(state->output_stream->dma_ch);
		state->wr_ref = 0;
	}

	if (!AUDIO_ACTIVE(state)) {
	       if (state->hw_shutdown)
		       state->hw_shutdown(state->data);
#ifdef CONFIG_PM
	       pm_unregister(state->pm_dev);
#endif
	}

	up(&state->sem);
	return 0;
}


#ifdef CONFIG_PM

static int audio_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	audio_state_t *state = (audio_state_t *)pm_dev->data;

	switch (req) {
	case PM_SUSPEND: /* enter D1-D3 */
		if (state->output_stream)
			sa1100_dma_sleep(state->output_stream->dma_ch);
		if (state->input_stream)
			sa1100_dma_sleep(state->input_stream->dma_ch);
		if (AUDIO_ACTIVE(state) && state->hw_shutdown)
			state->hw_shutdown(state->data);
		break;
	case PM_RESUME:  /* enter D0 */
		if (AUDIO_ACTIVE(state) && state->hw_init)
			state->hw_init(state->data);
		if (state->input_stream)
			sa1100_dma_wakeup(state->input_stream->dma_ch);
		if (state->output_stream)
			sa1100_dma_wakeup(state->output_stream->dma_ch);
		break;
	}
	return 0;
}

#endif


int sa1100_audio_attach(struct inode *inode, struct file *file,
			  audio_state_t *state)
{
	int err, need_tx_dma;

	DPRINTK("audio_open\n");

	down(&state->sem);

	/* access control */
	err = -ENODEV;
	if ((file->f_mode & FMODE_WRITE) && !state->output_stream)
		goto out;
	if ((file->f_mode & FMODE_READ) && !state->input_stream)
		goto out;
	err = -EBUSY;
	if ((file->f_mode & FMODE_WRITE) && state->wr_ref)
		goto out;
	if ((file->f_mode & FMODE_READ) && state->rd_ref)
		goto out;
	err = -EINVAL;
	if ((file->f_mode & FMODE_READ) && state->need_tx_for_rx && !state->output_stream)
		goto out;

	/* request DMA channels */
	if (state->skip_dma_init)
		goto skip_dma;
	need_tx_dma = ((file->f_mode & FMODE_WRITE) ||
		       ((file->f_mode & FMODE_READ) && state->need_tx_for_rx));
	if (state->wr_ref || (state->rd_ref && state->need_tx_for_rx))
		need_tx_dma = 0;
	if (need_tx_dma) {
		err = sa1100_request_dma(&state->output_stream->dma_ch,
					 state->output_id,
					 state->output_dma);
		if (err)
			goto out;
	}
	if (file->f_mode & FMODE_READ) {
		err = sa1100_request_dma(&state->input_stream->dma_ch,
					 state->input_id,
					 state->input_dma);
		if (err) {
			if (need_tx_dma)
				sa1100_free_dma(state->output_stream->dma_ch);
			goto out;
		}
	}
skip_dma:

	/* now complete initialisation */
	if (!AUDIO_ACTIVE(state)) {
		if (state->hw_init)
			state->hw_init(state->data);
#ifdef CONFIG_PM
		state->pm_dev = pm_register(PM_SYS_DEV, 0, audio_pm_callback);
		if (state->pm_dev)
			state->pm_dev->data = state;
#endif
	}

	if ((file->f_mode & FMODE_WRITE)) {
		state->wr_ref = 1;
		audio_clear_buf(state->output_stream);
		state->output_stream->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		state->output_stream->nbfrags = AUDIO_NBFRAGS_DEFAULT;
		state->output_stream->mapped = 0;
		sa1100_dma_set_callback(state->output_stream->dma_ch,
					audio_dmaout_done_callback);
		init_waitqueue_head(&state->output_stream->wq);
	}
	if (file->f_mode & FMODE_READ) {
		state->rd_ref = 1;
		audio_clear_buf(state->input_stream);
		state->input_stream->fragsize = AUDIO_FRAGSIZE_DEFAULT;
		state->input_stream->nbfrags = AUDIO_NBFRAGS_DEFAULT;
		state->input_stream->mapped = 0;
		sa1100_dma_set_callback(state->input_stream->dma_ch,
					audio_dmain_done_callback);
		init_waitqueue_head(&state->input_stream->wq);
	}

	file->private_data	= state;
	file->f_op->release	= audio_release;
	file->f_op->write	= audio_write;
	file->f_op->read	= audio_read;
	file->f_op->mmap	= audio_mmap;
	file->f_op->poll	= audio_poll;
	file->f_op->ioctl	= audio_ioctl;
	file->f_op->llseek	= audio_llseek;
	err = 0;

out:
	up(&state->sem);
	return err;
}

EXPORT_SYMBOL(sa1100_audio_attach);

MODULE_AUTHOR("Nicolas Pitre");
MODULE_DESCRIPTION("Common audio handling for the SA11x0 processor");
MODULE_LICENSE("GPL");
