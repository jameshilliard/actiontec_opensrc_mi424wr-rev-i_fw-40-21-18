/*
 *	Real Time Clock interface for Linux on Atmel AT91RM9200
 *
 *	Copyright (c) 2002 Rick Bronson
 *
 *      Based on sa1100-rtc.c by Nils Faerber
 *	Based on rtc.c by Paul Gortmaker
 *	Date/time conversion routines taken from arch/arm/kernel/time.c
 *			by Linus Torvalds and Russell King
 *		and the GNU C Library
 *	( ... I love the GPL ... just take what you need! ;)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <asm/bitops.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <linux/rtc.h>

#define AT91_RTC_FREQ	1
#define EPOCH		1970

/* Those are the bits from a classic RTC we want to mimic */
#define AT91_RTC_IRQF	0x80	/* any of the following 3 is active */
#define AT91_RTC_PF	0x40
#define AT91_RTC_AF	0x20
#define AT91_RTC_UF	0x10

#define BCD2BIN(val) (((val)&15) + ((val)>>4)*10)
#define BIN2BCD(val) ((((val)/10)<<4) + (val)%10)

static unsigned long rtc_status = 0;
static unsigned long rtc_irq_data;
static unsigned int at91_alarm_year = EPOCH;

static struct fasync_struct *at91_rtc_async_queue;
static DECLARE_WAIT_QUEUE_HEAD(at91_rtc_wait);
static DECLARE_WAIT_QUEUE_HEAD(at91_rtc_update);
static spinlock_t at91_rtc_updlock;	/* some spinlocks for saving/restoring interrupt levels */
extern spinlock_t at91_rtc_lock;

static const unsigned char days_in_mo[] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

#define is_leap(year) \
	((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

static const unsigned short int __mon_yday[2][13] =
{
	/* Normal years.  */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* Leap years.  */
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

/*
 * Returns day since start of the year [0-365]
 *  (from drivers/char/efirtc.c)
 */
static inline int compute_yday(int year, int month, int day)
{
	return  __mon_yday[is_leap(year)][month] + day-1;
}

/*
 * Set current time and date in RTC
 */
static void at91_rtc_settime(struct rtc_time *tval)
{
	unsigned long flags;

	/* Stop Time/Calendar from counting */
	AT91_SYS->RTC_CR |= (AT91C_RTC_UPDCAL | AT91C_RTC_UPDTIM);

	spin_lock_irqsave(&at91_rtc_updlock, flags);	/* stop int's else we wakeup b4 we sleep */
	AT91_SYS->RTC_IER = AT91C_RTC_ACKUPD;
	interruptible_sleep_on(&at91_rtc_update);	/* wait for ACKUPD interrupt to hit */
	spin_unlock_irqrestore(&at91_rtc_updlock, flags);
	AT91_SYS->RTC_IDR = AT91C_RTC_ACKUPD;

	AT91_SYS->RTC_TIMR = BIN2BCD(tval->tm_sec) << 0
			| BIN2BCD(tval->tm_min) << 8
			| BIN2BCD(tval->tm_hour) << 16;

	AT91_SYS->RTC_CALR = BIN2BCD(tval->tm_year / 100)		/* century */
	    		| BIN2BCD(tval->tm_year % 100) << 8	/* year */
	    		| BIN2BCD(tval->tm_mon + 1) << 16	/* tm_mon starts at zero */
	    		| BIN2BCD(tval->tm_wday + 1) << 21	/* day of the week [0-6], Sunday=0 */
			| BIN2BCD(tval->tm_mday) << 24;

	/* Restart Time/Calendar */
	AT91_SYS->RTC_CR &= ~(AT91C_RTC_UPDCAL | AT91C_RTC_UPDTIM);
}

/*
 * Decode time/date into rtc_time structure
 */
static void at91_rtc_decodetime(AT91_REG *timereg, AT91_REG *calreg, struct rtc_time *tval)
{
	unsigned int time, date;

	do {			/* must read twice in case it changes */
		time = *timereg;
		date = *calreg;
	} while ((time != *timereg) || (date != *calreg));

	tval->tm_sec = BCD2BIN((time & AT91C_RTC_SEC) >> 0);
	tval->tm_min = BCD2BIN((time & AT91C_RTC_MIN) >> 8);
	tval->tm_hour = BCD2BIN((time & AT91C_RTC_HOUR) >> 16);

	/* The Calendar Alarm register does not have a field for
	   the year - so these will return an invalid value.  When an
	   alarm is set, at91_alarm_year wille store the current year. */
	tval->tm_year = BCD2BIN(date & AT91C_RTC_CENT) * 100;		/* century */
	tval->tm_year += BCD2BIN((date & AT91C_RTC_YEAR) >> 8);		/* year */

	tval->tm_wday = BCD2BIN((date & AT91C_RTC_DAY) >> 21) - 1;	/* day of the week [0-6], Sunday=0 */
	tval->tm_mon = BCD2BIN(((date & AT91C_RTC_MONTH) >> 16) - 1);
	tval->tm_mday = BCD2BIN((date & AT91C_RTC_DATE) >> 24);
}

/*
 * IRQ handler for the RTC
 */
static void at91_rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int rtsr = AT91_SYS->RTC_SR & AT91_SYS->RTC_IMR;

	/* update irq data & counter */
	if (rtsr) {		/* this interrupt is shared!  Is it ours? */
		if (rtsr & AT91C_RTC_ALARM)
			rtc_irq_data |= (AT91_RTC_AF | AT91_RTC_IRQF);
		if (rtsr & AT91C_RTC_SECEV)
			rtc_irq_data |= (AT91_RTC_UF | AT91_RTC_IRQF);
		if (rtsr & AT91C_RTC_ACKUPD)
			wake_up_interruptible(&at91_rtc_update);
		rtc_irq_data += 0x100;
		AT91_SYS->RTC_SCCR = rtsr;		/* clear status reg */

		/* wake up waiting process */
		wake_up_interruptible(&at91_rtc_wait);
		kill_fasync(&at91_rtc_async_queue, SIGIO, POLL_IN);
	}
}

static int at91_rtc_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(1, &rtc_status))
		return -EBUSY;
	rtc_irq_data = 0;
	return 0;
}

static int at91_rtc_release(struct inode *inode, struct file *file)
{
	rtc_status = 0;
	return 0;
}

static int at91_rtc_fasync(int fd, struct file *filp, int on)
{
	return fasync_helper(fd, filp, on, &at91_rtc_async_queue);
}

static unsigned int at91_rtc_poll(struct file *file, poll_table * wait)
{
	poll_wait(file, &at91_rtc_wait, wait);
	return (rtc_irq_data) ? 0 : POLLIN | POLLRDNORM;
}

ssize_t at91_rtc_read(struct file * file, char *buf, size_t count, loff_t * ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long data;
	ssize_t retval;

	if (count < sizeof(unsigned long))
		return -EINVAL;

	add_wait_queue(&at91_rtc_wait, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	for (;;) {
		spin_lock_irq(&at91_rtc_lock);
		data = rtc_irq_data;
		if (data != 0) {
			rtc_irq_data = 0;
			break;
		}
		spin_unlock_irq(&at91_rtc_lock);

		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto out;
		}

		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			goto out;
		}

		schedule();
	}
	spin_unlock_irq(&at91_rtc_lock);

	data -= 0x100;		/* the first IRQ wasn't actually missed */
	retval = put_user(data, (unsigned long *) buf);
	if (!retval)
		retval = sizeof(unsigned long);

out:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&at91_rtc_wait, &wait);
	remove_wait_queue(&at91_rtc_update, &wait);
	return retval;
}

/*
 * Handle commands from user-space
 */
static int at91_rtc_ioctl(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	struct rtc_time tm, tm2;
	int ret = 0;

	spin_lock_irq(&at91_rtc_lock);
	switch (cmd) {
	case RTC_AIE_OFF:	/* alarm off */
		AT91_SYS->RTC_IDR = AT91C_RTC_ALARM;
		rtc_irq_data = 0;
		break;
	case RTC_AIE_ON:	/* alarm on */
		AT91_SYS->RTC_IER = AT91C_RTC_ALARM;
		rtc_irq_data = 0;
		break;
	case RTC_UIE_OFF:	/* update off */
		AT91_SYS->RTC_IDR = AT91C_RTC_SECEV;
		rtc_irq_data = 0;
		break;
	case RTC_UIE_ON:	/* update on */
		AT91_SYS->RTC_IER = AT91C_RTC_SECEV;
		rtc_irq_data = 0;
		break;
	case RTC_PIE_OFF:	/* periodic off */
		AT91_SYS->RTC_IDR = AT91C_RTC_SECEV;
		rtc_irq_data = 0;
		break;
	case RTC_PIE_ON:	/* periodic on */
		AT91_SYS->RTC_IER = AT91C_RTC_SECEV;
		rtc_irq_data = 0;
		break;
	case RTC_ALM_READ:	/* read alarm */
		at91_rtc_decodetime(&(AT91_SYS->RTC_TIMALR), &(AT91_SYS->RTC_CALALR), &tm);
		tm.tm_yday = compute_yday(tm.tm_year, tm.tm_mon, tm.tm_mday);
		tm.tm_year = at91_alarm_year - 1900;
		ret = copy_to_user((void *) arg, &tm, sizeof(tm)) ? -EFAULT : 0;
		break;
	case RTC_ALM_SET:	/* set alarm */
		if (copy_from_user(&tm2, (struct rtc_time *) arg, sizeof(tm2)))
			ret = -EFAULT;
		else {
			at91_rtc_decodetime(&(AT91_SYS->RTC_TIMR), &(AT91_SYS->RTC_CALR), &tm);
			at91_alarm_year = tm.tm_year;
			if ((unsigned) tm2.tm_hour < 24)	/* do some range checking */
				tm.tm_hour = tm2.tm_hour;
			if ((unsigned) tm2.tm_min < 60)
				tm.tm_min = tm2.tm_min;
			if ((unsigned) tm2.tm_sec < 60)
				tm.tm_sec = tm2.tm_sec;
			AT91_SYS->RTC_TIMALR = BIN2BCD(tm.tm_sec) << 0
				| BIN2BCD(tm.tm_min) << 8
				| BIN2BCD(tm.tm_hour) << 16
				| AT91C_RTC_HOUREN | AT91C_RTC_MINEN
				| AT91C_RTC_SECEN;
			AT91_SYS->RTC_CALALR = BIN2BCD(tm.tm_mon + 1) << 16	/* tm_mon starts at zero */
				| BIN2BCD(tm.tm_mday) << 24
				| AT91C_RTC_DATEEN | AT91C_RTC_MONTHEN;
		}
		break;
	case RTC_RD_TIME:	/* read time */
		at91_rtc_decodetime(&(AT91_SYS->RTC_TIMR), &(AT91_SYS->RTC_CALR), &tm);
		tm.tm_yday = compute_yday(tm.tm_year, tm.tm_mon, tm.tm_mday);
		tm.tm_year = tm.tm_year - 1900;
		ret = copy_to_user((void *) arg, &tm, sizeof(tm)) ? -EFAULT : 0;
		break;
	case RTC_SET_TIME:	/* set time */
		if (!capable(CAP_SYS_TIME))
			ret = -EACCES;
		else {
			if (copy_from_user(&tm, (struct rtc_time *) arg, sizeof(tm)))
				ret = -EFAULT;
			else {
				int tm_year = tm.tm_year + 1900;
				if (tm_year < EPOCH
				    || (unsigned) tm.tm_mon >= 12
				    || tm.tm_mday < 1
				    || tm.tm_mday > (days_in_mo[tm.tm_mon] + (tm.tm_mon == 1 && is_leap(tm_year)))
				    || (unsigned) tm.tm_hour >= 24
				    || (unsigned) tm.tm_min >= 60
				    || (unsigned) tm.tm_sec >= 60)
					ret = -EINVAL;
				else
					at91_rtc_settime(&tm);
			}
		}
		break;
	case RTC_IRQP_READ:	/* read periodic alarm frequency */
		ret = put_user(AT91_RTC_FREQ, (unsigned long *) arg);
		break;
	case RTC_IRQP_SET:	/* set periodic alarm frequency */
		if (arg != AT91_RTC_FREQ)
			ret = -EINVAL;
		break;
	case RTC_EPOCH_READ:	/* read epoch */
		ret = put_user(EPOCH, (unsigned long *) arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	spin_unlock_irq(&at91_rtc_lock);
	return ret;
}

/*
 * Provide RTC information in /proc/driver/rtc
 */
static int at91_rtc_read_proc(char *page, char **start, off_t off,
			      int count, int *eof, void *data)
{
	char *p = page;
	int len;
	struct rtc_time tm;

	at91_rtc_decodetime(&(AT91_SYS->RTC_TIMR), &(AT91_SYS->RTC_CALR), &tm);
	p += sprintf(p, "rtc_time\t: %02d:%02d:%02d\n"
			"rtc_date\t: %04d-%02d-%02d\n"
			"rtc_epoch\t: %04d\n",
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			tm.tm_year, tm.tm_mon + 1, tm.tm_mday, EPOCH);
	at91_rtc_decodetime(&(AT91_SYS->RTC_TIMALR), &(AT91_SYS->RTC_CALALR), &tm);
	p += sprintf(p, "alrm_time\t: %02d:%02d:%02d\n"
			"alrm_date\t: %04d-%02d-%02d\n",
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			at91_alarm_year, tm.tm_mon + 1, tm.tm_mday);
	p += sprintf(p, "alarm_IRQ\t: %s\n", (AT91_SYS->RTC_IMR & AT91C_RTC_ALARM) ? "yes" : "no");
	p += sprintf(p, "update_IRQ\t: %s\n", (AT91_SYS->RTC_IMR & AT91C_RTC_ACKUPD) ? "yes" : "no");
	p += sprintf(p, "periodic_IRQ\t: %s\n", (AT91_SYS->RTC_IMR & AT91C_RTC_SECEV) ? "yes" : "no");
	p += sprintf(p, "periodic_freq\t: %ld\n", (unsigned long) AT91_RTC_FREQ);

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static struct file_operations at91_rtc_fops = {
	owner:THIS_MODULE,
	llseek:no_llseek,
	read:at91_rtc_read,
	poll:at91_rtc_poll,
	ioctl:at91_rtc_ioctl,
	open:at91_rtc_open,
	release:at91_rtc_release,
	fasync:at91_rtc_fasync,
};

static struct miscdevice at91_rtc_miscdev = {
	minor:RTC_MINOR,
	name:"rtc",
	fops:&at91_rtc_fops,
};

/*
 * Initialize and install RTC driver
 */
static int __init at91_rtc_init(void)
{
	int ret;

	AT91_SYS->RTC_CR = 0;
	AT91_SYS->RTC_MR = 0;	/* put in 24 hour format */
	/* Disable all interrupts */
	AT91_SYS->RTC_IDR = AT91C_RTC_ACKUPD | AT91C_RTC_ALARM | AT91C_RTC_SECEV | AT91C_RTC_TIMEV | AT91C_RTC_CALEV;

	spin_lock_init(&at91_rtc_updlock);
	spin_lock_init(&at91_rtc_lock);

	misc_register(&at91_rtc_miscdev);
	create_proc_read_entry("driver/rtc", 0, 0, at91_rtc_read_proc, NULL);
	ret = request_irq(AT91C_ID_SYS, at91_rtc_interrupt, SA_SHIRQ,
			"at91_rtc", &rtc_status);
	if (ret) {
		printk(KERN_ERR "at91_rtc: IRQ %d already in use.\n", AT91C_ID_SYS);
		remove_proc_entry("driver/rtc", NULL);
		misc_deregister(&at91_rtc_miscdev);
		return ret;
	}

	printk(KERN_INFO "AT91 Real Time Clock driver\n");
	return 0;
}

/*
 * Disable and remove the RTC driver
 */
static void __exit at91_rtc_exit(void)
{
	/* Disable all interrupts */
	AT91_SYS->RTC_IDR = AT91C_RTC_ACKUPD | AT91C_RTC_ALARM | AT91C_RTC_SECEV | AT91C_RTC_TIMEV | AT91C_RTC_CALEV;
	free_irq(AT91C_ID_SYS, &rtc_status);

	rtc_status = 0;
	remove_proc_entry("driver/rtc", NULL);
	misc_deregister(&at91_rtc_miscdev);
}

module_init(at91_rtc_init);
module_exit(at91_rtc_exit);

MODULE_AUTHOR("Rick Bronson");
MODULE_DESCRIPTION("AT91 Realtime Clock Driver (AT91_RTC)");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
