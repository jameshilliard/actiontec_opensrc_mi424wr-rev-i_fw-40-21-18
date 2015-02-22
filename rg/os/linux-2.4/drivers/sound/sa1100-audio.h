/*
 * Common audio handling for the SA11x0
 *
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */


/*
 * Buffer Management
 */

typedef struct {
	int size;		/* buffer size */
	char *start;		/* points to actual buffer */
	dma_addr_t dma_addr;	/* physical buffer address */
	struct semaphore sem;	/* down before touching the buffer */
	int master;		/* owner for buffer allocation, contain size when true */
	struct audio_stream_s *stream;	/* owning stream */
} audio_buf_t;

typedef struct audio_stream_s {
	audio_buf_t *buffers;	/* pointer to audio buffer structures */
	audio_buf_t *buf;	/* current buffer used by read/write */
	u_int buf_idx;		/* index for the pointer above... */
	u_int fragsize;		/* fragment i.e. buffer size */
	u_int nbfrags;		/* nbr of fragments i.e. buffers */
	int bytecount;		/* nbr of processed bytes */
	int getptrCount;	/* value of bytecount last time anyone asked via GETxPTR */
	int fragcount;		/* nbr of fragment transitions */
	dmach_t dma_ch;		/* DMA channel ID */
	wait_queue_head_t wq;	/* for poll */
	int mapped:1;		/* mmap()'ed buffers */
	int active:1;		/* actually in progress */
	int stopped:1;		/* might be active but stopped */
} audio_stream_t;

/*
 * State structure for one instance
 */

typedef struct {
	audio_stream_t *output_stream;
	audio_stream_t *input_stream;
	dma_device_t output_dma;
	dma_device_t input_dma;
	char *output_id;
	char *input_id;
	int rd_ref:1;		/* open reference for recording */
	int wr_ref:1;		/* open reference for playback */
	int need_tx_for_rx:1;	/* if data must be sent while receiving */
	int tx_spinning:1;	/* tx spinning active */
	int skip_dma_init:1;	/* hack for the SA1111 */
	void *data;
	void (*hw_init)(void *);
	void (*hw_shutdown)(void *);
	int (*client_ioctl)(struct inode *, struct file *, uint, ulong);
	struct pm_dev *pm_dev;
	struct semaphore sem;	/* to protect against races in attach() */
} audio_state_t;

/*
 * Functions exported by this module
 */
extern int sa1100_audio_attach( struct inode *inode, struct file *file,
				audio_state_t *state);
