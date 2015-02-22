/*
 *  linux/drivers/misc/ucb1x00-ts.c
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 21-Jan-2002 <jco@ict.es> :
 *
 * Added support for synchronous A/D mode. This mode is useful to
 * avoid noise induced in the touchpanel by the LCD, provided that
 * the UCB1x00 has a valid LCD sync signal routed to its ADCSYNC pin.
 * It is important to note that the signal connected to the ADCSYNC
 * pin should provide pulses even when the LCD is blanked, otherwise
 * a pen touch needed to unblank the LCD will never be read.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/pm.h>

#include <asm/dma.h>
#include <asm/semaphore.h>

#include "ucb1x00.h"

/*
 * Define this if you want the UCB1x00 stuff to talk to the input layer
 */
#undef USE_INPUT

#ifndef USE_INPUT

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>

/*
 * This structure is nonsense - millisecs is not very useful
 * since the field size is too small.  Also, we SHOULD NOT
 * be exposing jiffies to user space directly.
 */
struct ts_event {
	u16		pressure;
	u16		x;
	u16		y;
	u16		pad;
	struct timeval	stamp;
};

#define NR_EVENTS	16

#else

#include <linux/input.h>

#endif

struct ucb1x00_ts {
#ifdef USE_INPUT
	struct input_dev	idev;
#endif
	struct ucb1x00		*ucb;
#ifdef CONFIG_PM
	struct pm_dev		*pmdev;
#endif

	wait_queue_head_t	irq_wait;
	struct semaphore	sem;
	struct completion	init_exit;
	struct task_struct	*rtask;
	int			use_count;
	u16			x_res;
	u16			y_res;

#ifndef USE_INPUT
	struct fasync_struct	*fasync;
	wait_queue_head_t	read_wait;
	u8			evt_head;
	u8			evt_tail;
	struct ts_event		events[NR_EVENTS];
#endif
	int			restart:1;
	int			adcsync:1;
};

static struct ucb1x00_ts ucbts;
static int adcsync = UCB_NOSYNC;

static int ucb1x00_ts_startup(struct ucb1x00_ts *ts);
static void ucb1x00_ts_shutdown(struct ucb1x00_ts *ts);

#ifndef USE_INPUT

#define ucb1x00_ts_evt_pending(ts)	((volatile u8)(ts)->evt_head != (ts)->evt_tail)
#define ucb1x00_ts_evt_get(ts)		((ts)->events + (ts)->evt_tail)
#define ucb1x00_ts_evt_pull(ts)		((ts)->evt_tail = ((ts)->evt_tail + 1) & (NR_EVENTS - 1))
#define ucb1x00_ts_evt_clear(ts)	((ts)->evt_head = (ts)->evt_tail = 0)

static inline void ucb1x00_ts_evt_add(struct ucb1x00_ts *ts, u16 pressure, u16 x, u16 y)
{
	int next_head;

	next_head = (ts->evt_head + 1) & (NR_EVENTS - 1);
	if (next_head != ts->evt_tail) {
		ts->events[ts->evt_head].pressure = pressure;
		ts->events[ts->evt_head].x = x;
		ts->events[ts->evt_head].y = y;
		do_gettimeofday(&ts->events[ts->evt_head].stamp);
		ts->evt_head = next_head;

		if (ts->fasync)
			kill_fasync(&ts->fasync, SIGIO, POLL_IN);
		wake_up_interruptible(&ts->read_wait);
	}
}

static inline void ucb1x00_ts_event_release(struct ucb1x00_ts *ts)
{
	ucb1x00_ts_evt_add(ts, 0, 0, 0);
}

/*
 * User space driver interface.
 */
static ssize_t
ucb1x00_ts_read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	struct ucb1x00_ts *ts = filp->private_data;
	char *ptr = buffer;
	int err = 0;

	add_wait_queue(&ts->read_wait, &wait);
	while (count >= sizeof(struct ts_event)) {
		err = -ERESTARTSYS;
		if (signal_pending(current))
			break;

		if (ucb1x00_ts_evt_pending(ts)) {
			struct ts_event *evt = ucb1x00_ts_evt_get(ts);

			err = copy_to_user(ptr, evt, sizeof(struct ts_event));
			ucb1x00_ts_evt_pull(ts);

			if (err)
				break;

			ptr += sizeof(struct ts_event);
			count -= sizeof(struct ts_event);
			continue;
		}

		set_current_state(TASK_INTERRUPTIBLE);
		err = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			break;
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&ts->read_wait, &wait);
 
	return ptr == buffer ? err : ptr - buffer;
}

static unsigned int ucb1x00_ts_poll(struct file *filp, poll_table *wait)
{
	struct ucb1x00_ts *ts = filp->private_data;
	int ret = 0;

	poll_wait(filp, &ts->read_wait, wait);
	if (ucb1x00_ts_evt_pending(ts))
		ret = POLLIN | POLLRDNORM;

	return ret;
}

static int ucb1x00_ts_fasync(int fd, struct file *filp, int on)
{
	struct ucb1x00_ts *ts = filp->private_data;

	return fasync_helper(fd, filp, on, &ts->fasync);
}

static int ucb1x00_ts_open(struct inode *inode, struct file *filp)
{
	struct ucb1x00_ts *ts = &ucbts;
	int ret = 0;

	ret = ucb1x00_ts_startup(ts);
	if (ret == 0)
		filp->private_data = ts;

	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static int ucb1x00_ts_release(struct inode *inode, struct file *filp)
{
	struct ucb1x00_ts *ts = filp->private_data;

	down(&ts->sem);
	ucb1x00_ts_fasync(-1, filp, 0);
	ucb1x00_ts_shutdown(ts);
	up(&ts->sem);

	return 0;
}

static struct file_operations ucb1x00_fops = {
	owner:		THIS_MODULE,
	read:		ucb1x00_ts_read,
	poll:		ucb1x00_ts_poll,
	open:		ucb1x00_ts_open,
	release:	ucb1x00_ts_release,
	fasync:		ucb1x00_ts_fasync,
};

/*
 * The official UCB1x00 touchscreen is a miscdevice:
 *   10 char        Non-serial mice, misc features
 *                   14 = /dev/touchscreen/ucb1x00  UCB 1x00 touchscreen
 */
static struct miscdevice ucb1x00_ts_dev = {
	minor:	14,
	name:	"touchscreen/ucb1x00",
	fops:	&ucb1x00_fops,
};

static inline int ucb1x00_ts_register(struct ucb1x00_ts *ts)
{
	init_waitqueue_head(&ts->read_wait);
	return misc_register(&ucb1x00_ts_dev);
}

static inline void ucb1x00_ts_deregister(struct ucb1x00_ts *ts)
{
	misc_deregister(&ucb1x00_ts_dev);
}

#else

#define ucb1x00_ts_evt_clear(ts)	do { } while (0)

static inline void ucb1x00_ts_evt_add(struct ucb1x00_ts *ts, u16 pressure, u16 x, u16 y)
{
	input_report_abs(&ts->idev, ABS_X, x);
	input_report_abs(&ts->idev, ABS_Y, y);
	input_report_abs(&ts->idev, ABS_PRESSURE, pressure);
}

static int ucb1x00_ts_open(struct input_dev *idev)
{
	struct ucb1x00_ts *ts = (struct ucb1x00_ts *)idev;

	return ucb1x00_ts_startup(ts);
}

static void ucb1x00_ts_close(struct input_dev *idev)
{
	struct ucb1x00_ts *ts = (struct ucb1x00_ts *)idev;

	down(&ts->sem);
	ucb1x00_ts_shutdown(ts);
	up(&ts->sem);
}

static inline int ucb1x00_ts_register(struct ucb1x00_ts *ts)
{
	ts->idev.name      = "Touchscreen panel";
	ts->idev.idproduct = ts->ucb->id;
	ts->idev.open      = ucb1x00_ts_open;
	ts->idev.close     = ucb1x00_ts_close;

	__set_bit(EV_ABS, ts->idev.evbit);
	__set_bit(ABS_X, ts->idev.absbit);
	__set_bit(ABS_Y, ts->idev.absbit);
	__set_bit(ABS_PRESSURE, ts->idev.absbit);

	input_register_device(&ts->idev);

	return 0;
}

static inline void ucb1x00_ts_deregister(struct ucb1x00_ts *ts)
{
	input_unregister_device(&ts->idev);
}

#endif

/*
 * Switch to interrupt mode.
 */
static inline void ucb1x00_ts_mode_int(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_INT);
}

/*
 * Switch to pressure mode, and read pressure.  We don't need to wait
 * here, since both plates are being driven.
 */
static inline unsigned int ucb1x00_ts_read_pressure(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);

	return ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPY, ts->adcsync);
}

/*
 * Switch to X position mode and measure Y plate.  We switch the plate
 * configuration in pressure mode, then switch to position mode.  This
 * gives a faster response time.  Even so, we need to wait about 55us
 * for things to stabilise.
 */
static inline unsigned int ucb1x00_ts_read_xpos(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);

	udelay(55);

	return ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPY, ts->adcsync);
}

/*
 * Switch to Y position mode and measure X plate.  We switch the plate
 * configuration in pressure mode, then switch to position mode.  This
 * gives a faster response time.  Even so, we need to wait about 55us
 * for things to stabilise.
 */
static inline unsigned int ucb1x00_ts_read_ypos(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);

	udelay(55);

	return ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPX, ts->adcsync);
}

/*
 * Switch to X plate resistance mode.  Set MX to ground, PX to
 * supply.  Measure current.
 */
static inline unsigned int ucb1x00_ts_read_xres(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	return ucb1x00_adc_read(ts->ucb, 0, ts->adcsync);
}

/*
 * Switch to Y plate resistance mode.  Set MY to ground, PY to
 * supply.  Measure current.
 */
static inline unsigned int ucb1x00_ts_read_yres(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	return ucb1x00_adc_read(ts->ucb, 0, ts->adcsync);
}

/*
 * This is a RT kernel thread that handles the ADC accesses
 * (mainly so we can use semaphores in the UCB1200 core code
 * to serialise accesses to the ADC).
 */
static int ucb1x00_thread(void *_ts)
{
	struct ucb1x00_ts *ts = _ts;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);
	int valid;

	ts->rtask = tsk;

	daemonize();
	reparent_to_init();
	strcpy(tsk->comm, "ktsd");
	tsk->tty = NULL;
	/*
	 * We could run as a real-time thread.  However, thus far
	 * this doesn't seem to be necessary.
	 */
//	tsk->policy = SCHED_FIFO;
//	tsk->rt_priority = 1;

	/* only want to receive SIGKILL */
	spin_lock_irq(&tsk->sigmask_lock);
	siginitsetinv(&tsk->blocked, sigmask(SIGKILL));
	recalc_sigpending(tsk);
	spin_unlock_irq(&tsk->sigmask_lock);

	complete(&ts->init_exit);

	valid = 0;

	add_wait_queue(&ts->irq_wait, &wait);
	for (;;) {
		unsigned int x, y, p, val;
		signed long timeout;

		ts->restart = 0;

		ucb1x00_adc_enable(ts->ucb);

		x = ucb1x00_ts_read_xpos(ts);
		y = ucb1x00_ts_read_ypos(ts);
		p = ucb1x00_ts_read_pressure(ts);

		/*
		 * Switch back to interrupt mode.
		 */
		ucb1x00_ts_mode_int(ts);
		ucb1x00_adc_disable(ts->ucb);

		set_task_state(tsk, TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ / 100);
		if (signal_pending(tsk))
			break;

		ucb1x00_enable(ts->ucb);
		val = ucb1x00_reg_read(ts->ucb, UCB_TS_CR);

		if (val & (UCB_TS_CR_TSPX_LOW | UCB_TS_CR_TSMX_LOW)) {
			set_task_state(tsk, TASK_INTERRUPTIBLE);

			ucb1x00_enable_irq(ts->ucb, UCB_IRQ_TSPX, UCB_FALLING);
			ucb1x00_disable(ts->ucb);

			/*
			 * If we spat out a valid sample set last time,
			 * spit out a "pen off" sample here.
			 */
			if (valid) {
				ucb1x00_ts_event_release(ts);
				valid = 0;
			}

			timeout = MAX_SCHEDULE_TIMEOUT;
		} else {
			ucb1x00_disable(ts->ucb);

			/*
			 * Filtering is policy.  Policy belongs in user
			 * space.  We therefore leave it to user space
			 * to do any filtering they please.
			 */
			if (!ts->restart) {
				ucb1x00_ts_evt_add(ts, p, x, y);
				valid = 1;
			}

			set_task_state(tsk, TASK_INTERRUPTIBLE);
			timeout = HZ / 100;
		}

		schedule_timeout(timeout);
		if (signal_pending(tsk))
			break;
	}

	remove_wait_queue(&ts->irq_wait, &wait);

	ts->rtask = NULL;
	ucb1x00_ts_evt_clear(ts);
	complete_and_exit(&ts->init_exit, 0);
}

/*
 * We only detect touch screen _touches_ with this interrupt
 * handler, and even then we just schedule our task.
 */
static void ucb1x00_ts_irq(int idx, void *id)
{
	struct ucb1x00_ts *ts = id;
	ucb1x00_disable_irq(ts->ucb, UCB_IRQ_TSPX, UCB_FALLING);
	wake_up(&ts->irq_wait);
}

static int ucb1x00_ts_startup(struct ucb1x00_ts *ts)
{
	int ret = 0;

	if (down_interruptible(&ts->sem))
		return -EINTR;

	if (ts->use_count++ != 0)
		goto out;

	if (ts->rtask)
		panic("ucb1x00: rtask running?");

	init_waitqueue_head(&ts->irq_wait);
	ret = ucb1x00_hook_irq(ts->ucb, UCB_IRQ_TSPX, ucb1x00_ts_irq, ts);
	if (ret < 0)
		goto out;

	/*
	 * If we do this at all, we should allow the user to
	 * measure and read the X and Y resistance at any time.
	 */
	ucb1x00_adc_enable(ts->ucb);
	ts->x_res = ucb1x00_ts_read_xres(ts);
	ts->y_res = ucb1x00_ts_read_yres(ts);
	ucb1x00_adc_disable(ts->ucb);

	init_completion(&ts->init_exit);
	ret = kernel_thread(ucb1x00_thread, ts, 0);
	if (ret >= 0) {
		wait_for_completion(&ts->init_exit);
		ret = 0;
	} else {
		ucb1x00_free_irq(ts->ucb, UCB_IRQ_TSPX, ts);
	}

 out:
	if (ret)
		ts->use_count--;
	up(&ts->sem);
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static void ucb1x00_ts_shutdown(struct ucb1x00_ts *ts)
{
	if (--ts->use_count == 0) {
		if (ts->rtask) {
			send_sig(SIGKILL, ts->rtask, 1);
			wait_for_completion(&ts->init_exit);
		}

		ucb1x00_enable(ts->ucb);
		ucb1x00_free_irq(ts->ucb, UCB_IRQ_TSPX, ts);
		ucb1x00_reg_write(ts->ucb, UCB_TS_CR, 0);
		ucb1x00_disable(ts->ucb);
	}
}

#ifdef CONFIG_PM
static int ucb1x00_ts_pm (struct pm_dev *dev, pm_request_t rqst, void *data)
{
	struct ucb1x00_ts *ts = (struct ucb1x00_ts *) (dev->data);

	if (rqst == PM_RESUME && ts->rtask != NULL) {
		/*
		 * Restart the TS thread to ensure the
		 * TS interrupt mode is set up again
		 * after sleep.
		 */
		ts->restart = 1;
		wake_up(&ts->irq_wait);
	}
	return 0;
}
#endif


/*
 * Initialisation.
 */
static int __init ucb1x00_ts_init(void)
{
	struct ucb1x00_ts *ts = &ucbts;

	ts->ucb = ucb1x00_get();
	if (!ts->ucb)
		return -ENODEV;

	ts->adcsync = adcsync;
	init_MUTEX(&ts->sem);

#ifdef CONFIG_PM
	ts->pmdev = pm_register(PM_SYS_DEV, PM_SYS_UNKNOWN, ucb1x00_ts_pm);
	if (ts->pmdev == NULL)
		printk("ucb1x00_ts: unable to register in PM.\n");
	else
		ts->pmdev->data = ts;
#endif
	return ucb1x00_ts_register(ts);
}

static void __exit ucb1x00_ts_exit(void)
{
	struct ucb1x00_ts *ts = &ucbts;

	ucb1x00_ts_deregister(ts);

#ifdef CONFIG_PM
	if (ts->pmdev)
		pm_unregister(ts->pmdev);
#endif
}

#ifndef MODULE

/*
 * Parse kernel command-line options.
 *
 * syntax : ucbts=[sync|nosync],...
 */
static int __init ucb1x00_ts_setup(char *str)
{
	char *p;

	while ((p = strsep(&str, ",")) != NULL) {
		if (strcmp(p, "sync") == 0)
			adcsync = UCB_SYNC;
	}

	return 1;
}

__setup("ucbts=", ucb1x00_ts_setup);

#else

MODULE_PARM(adcsync, "i");
MODULE_PARM_DESC(adcsync, "Enable use of ADCSYNC signal");

#endif

module_init(ucb1x00_ts_init);
module_exit(ucb1x00_ts_exit);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("UCB1x00 touchscreen driver");
MODULE_LICENSE("GPL");
