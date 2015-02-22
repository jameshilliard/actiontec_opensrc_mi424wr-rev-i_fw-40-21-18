/*
 *  linux/drivers/char/anakin_ts.c
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   18-Apr-2001 TTC	Created
 *   23-Oct-2001 dwmw2	Cleanup
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

/*
 * TSBUF_SIZE must be a power of two
 */
#define ANAKIN_TS_MINOR	16
#define TSBUF_SIZE	256
#define NEXT(index)	(((index) + 1) & (TSBUF_SIZE - 1))

static unsigned short buffer[TSBUF_SIZE][4];
static int head, tail;
static DECLARE_WAIT_QUEUE_HEAD(queue);
static DECLARE_MUTEX(open_sem);
static spinlock_t tailptr_lock = SPIN_LOCK_UNLOCKED;
static struct fasync_struct *fasync;

/*
 * Interrupt handler and standard file operations
 */
static void
anakin_ts_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int status = __raw_readl(IO_BASE + IO_CONTROLLER + 0x24);

	/*
	 * iPAQ format (u16 pressure, x, y, millisecs)
	 */
	switch (status >> 20 & 3) {
	case 0:
		return;
	case 2:
		buffer[head][0] = 0;
		break;
	default:
		buffer[head][0] = 0x7f;
	}

	if (unlikely((volatile int)tail == NEXT(head))) {
		/* Run out of space in the buffer. Move the tail pointer */
		spin_lock(&tailptr_lock);

		if ((volatile int)tail == NEXT(head)) {
			tail = NEXT(NEXT(head));
		}
		spin_unlock(&tailptr_lock);
	}

	buffer[head][1] = status >> 2 & 0xff;
	buffer[head][2] = status >> 12 & 0xff;
	buffer[head][3] = jiffies;
	mb();
	head = NEXT(head);

	wake_up_interruptible(&queue);
	kill_fasync(&fasync, SIGIO, POLL_IN);

}

static ssize_t
anakin_ts_read(struct file *filp, char *buf, size_t count, loff_t *l)
{
	unsigned short data[4];
	ssize_t written = 0;

	if (head == tail) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(queue, (volatile int)head != (volatile int)tail))
			return -ERESTARTSYS;
	}

	while ((volatile int)head != (volatile int)tail && count >= sizeof data) {
		/* Copy the data out with the spinlock held, so the 
		   interrupt can't fill the buffer and move the tail 
		   pointer while we're doing it */
		spin_lock_irq(&tailptr_lock);

		memcpy(data, buffer[tail], sizeof data);
		tail = NEXT(tail);

		spin_unlock_irq(&tailptr_lock);

		if (copy_to_user(buf, data, sizeof data))
			return -EFAULT;
		count -= sizeof data;
		buf += sizeof data;
		written += sizeof data;
	}
	return written ? written : -EINVAL;
}

static unsigned int
anakin_ts_poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &queue, wait);
	return head != tail ? POLLIN | POLLRDNORM : 0;
}

static int
anakin_ts_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	/*
	 * Future ioctl goes here
	 */
	return 0;
}

static int
anakin_ts_open(struct inode *inode, struct file *filp)
{
	if (down_trylock(&open_sem))
		return -EBUSY;
	return 0;
}

static int
anakin_ts_fasync(int fd, struct file *filp, int on)
{
	return fasync_helper(fd, filp, on, &fasync);
}

static int
anakin_ts_release(struct inode *inode, struct file *filp)
{
	anakin_ts_fasync(-1, filp, 0);
	up(&open_sem);
	return 0;
}

static struct file_operations anakin_ts_fops = {
	owner:		THIS_MODULE,
	read:		anakin_ts_read,
	poll:		anakin_ts_poll,
	ioctl:		anakin_ts_ioctl,
	open:		anakin_ts_open,
	release:	anakin_ts_release,
	fasync:		anakin_ts_fasync,
};

static struct miscdevice anakin_ts_miscdev = {
        ANAKIN_TS_MINOR,
        "anakin_ts",
        &anakin_ts_fops
};

/*
 * Initialization and exit routines
 */
int __init
anakin_ts_init(void)
{
	int retval;

	if ((retval = request_irq(IRQ_TOUCHSCREEN, anakin_ts_handler,
			SA_INTERRUPT, "anakin_ts", 0))) {
		printk(KERN_WARNING "anakin_ts: failed to get IRQ\n");
		return retval;
	}
	__raw_writel(1, IO_BASE + IO_CONTROLLER + 8);
	misc_register(&anakin_ts_miscdev);

	printk(KERN_NOTICE "Anakin touchscreen driver initialised\n");

	return 0;
}

void __exit
anakin_ts_exit(void)
{
	__raw_writel(0, IO_BASE + IO_CONTROLLER + 8);
	free_irq(IRQ_TOUCHSCREEN, 0);
	misc_deregister(&anakin_ts_miscdev);
}

module_init(anakin_ts_init);
module_exit(anakin_ts_exit);

MODULE_AUTHOR("Tak-Shing Chan <chan@aleph1.co.uk>");
MODULE_DESCRIPTION("Anakin touchscreen driver");
MODULE_SUPPORTED_DEVICE("touchscreen/anakin");
