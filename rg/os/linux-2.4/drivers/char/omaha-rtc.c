/*
 *	(C) ARM Limited 2002.
 *
 *	Real Time Clock interface for Linux on Omaha
 *
 *	Based on sa1100-rtc.c
 *
 *	Copyright (c) 2000 Nils Faerber
 *
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
 *	1.00	2001-06-08	Nicolas Pitre <nico@cam.org>
 *	- added periodic timer capability using OSMR1
 *	- flag compatibility with other RTC chips
 *	- permission checks for ioctls
 *	- major cleanup, partial rewrite
 *
 *	0.03	2001-03-07	CIH <cih@coventive.com>
 *	- Modify the bug setups RTC clock.
 *
 *	0.02	2001-02-27	Nils Faerber <nils@@kernelconcepts.de>
 *	- removed mktime(), added alarm irq clear
 *
 *	0.01	2000-10-01	Nils Faerber <nils@@kernelconcepts.de>
 *	- initial release
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <linux/rtc.h>
#include <linux/mc146818rtc.h>

#define	DRIVER_VERSION		"1.00"

#define epoch			1970

#define TIMER_FREQ		3686400

#define RTC_DEF_DIVIDER		32768 - 1
#define RTC_DEF_TRIM		0

/* Those are the bits from a classic RTC we want to mimic */
#define RTC_IRQF		0x80	/* any of the following 3 is active */
#define RTC_PF			0x40
#define RTC_AF			0x20
#define RTC_UF			0x10

// bitdefs for rtc registers
#define TICNT_ENABLE	0x80	// Enable tick interrupt
#define TICNT_PERIOD	0x7F	// Divisor required for 1Hz tick
#define RTC_ENABLE	0x1	// Enable bit for RTC

static unsigned long rtc_status;
static unsigned long rtc_irq_data;
static unsigned long rtc_freq = 1024;

static struct fasync_struct *rtc_async_queue;
static DECLARE_WAIT_QUEUE_HEAD(rtc_wait);

extern spinlock_t rtc_lock;

static const unsigned char days_in_mo[] =
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define is_leap(year) \
	((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

// all the alarm and rtc registers
static volatile unsigned int almsec = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_ALMSEC);
static volatile unsigned int almmin = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_ALMMIN);
static volatile unsigned int almhour = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_ALMHOUR);
static volatile unsigned int almday = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_ALMDAY);
static volatile unsigned int almmon = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_ALMMON);
static volatile unsigned int almyear = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_ALMYEAR);

static volatile unsigned int bcdsec = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_BCDSEC);
static volatile unsigned int bcdmin = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_BCDMIN);
static volatile unsigned int bcdhour = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_BCDHOUR);
static volatile unsigned int bcdday = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_BCDDAY);
static volatile unsigned int bcddate = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_BCDDATE);
static volatile unsigned int bcdmon = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_BCDMON);
static volatile unsigned int bcdyear = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_BCDYEAR);

/*
 * Converts seconds since 1970-01-01 00:00:00 to Gregorian date.
 */

static void decodetime (unsigned long t, struct rtc_time *tval)
{
	long days, month, year, rem;

	days = t / 86400;
	rem = t % 86400;
	tval->tm_hour = rem / 3600;
	rem %= 3600;
	tval->tm_min = rem / 60;
	tval->tm_sec = rem % 60;
	tval->tm_wday = (4 + days) % 7;

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

	year = epoch;
	while (days >= (365 + is_leap(year))) {
		unsigned long yg = year + days / 365;
		days -= ((yg - year) * 365
				+ LEAPS_THRU_END_OF (yg - 1)
				- LEAPS_THRU_END_OF (year - 1));
		year = yg;
	}
	tval->tm_year = year - 1900;
	tval->tm_yday = days + 1;

	month = 0;
	if (days >= 31) {
		days -= 31;
		month++;
		if (days >= (28 + is_leap(year))) {
			days -= (28 + is_leap(year));
			month++;
			while (days >= days_in_mo[month]) {
				days -= days_in_mo[month];
				month++;
			}
		}
	}
	tval->tm_mon = month;
	tval->tm_mday = days + 1;
}

// Get alarm time in seconds
static unsigned long get_alarm_time(void)
{
	int sec, min,hour,date,mon,year;
	
	// Read data from h/w
	year = __raw_readb(almyear);
	mon = __raw_readb(almmon);
	date = __raw_readb(almday);
	hour = __raw_readb(almhour);
	min = __raw_readb(almmin);
	sec = __raw_readb(almsec);
	
	// convert all the data into binary
	year = BCD_TO_BIN(year);
	mon = BCD_TO_BIN(mon);
	date = BCD_TO_BIN(date);
	hour = BCD_TO_BIN(hour);
	min = BCD_TO_BIN(min);
	sec = BCD_TO_BIN(sec);
		
	// convert year to 19xx or 20xx as appropriate
	if (year > 69)
		year += 1900;
	else
		year += 2000;
		
	// Now calculate number of seconds since time began...
	return mktime(year,mon,date,hour,min,sec);	
}

// Get rtc time in seconds
static unsigned long get_rtc_time(void)
{
	int sec,min,hour,day,date,mon,year;
	
	// Read data from h/w
	year = __raw_readb(bcdyear);
	mon = __raw_readb(bcdmon);
	date = __raw_readb(bcdday);
	day = __raw_readb(bcddate);
	hour = __raw_readb(bcdhour);
	min = __raw_readb(bcdmin);
	sec = __raw_readb(bcdsec);
	
	// convert all the data into binary
	year = BCD_TO_BIN(year);
	mon = BCD_TO_BIN(mon);
	date = BCD_TO_BIN(date);
	day = BCD_TO_BIN(day);
	hour = BCD_TO_BIN(hour);
	min = BCD_TO_BIN(min);
	sec = BCD_TO_BIN(sec);
	
	// convert year to 19xx or 20xx as appropriate
	if (year > 69)
		year += 1900;
	else
		year += 2000;

	// Now calculate number of seconds since time began...
	return mktime(year,mon,date,hour,min,sec);	
}

/* Sets time of alarm */
static void set_alarm_time(struct rtc_time *tval)
{
	
	int sec,min,hour,day,mon,year;
	 
	 // Convert data from binary to 8-bit bcd
	 sec = BIN_TO_BCD(tval->tm_sec);
	 min = BIN_TO_BCD(tval->tm_min);
	 hour = BIN_TO_BCD(tval->tm_hour);
	 day = BIN_TO_BCD(tval->tm_mday);
	 mon = BIN_TO_BCD(tval->tm_mon);
		
	// Year is special
	year = tval->tm_year;
	if(year > 1999)
	 	year -=2000;
	else	
		year -=1900;
	
	year = BIN_TO_BCD(year);	
	 
	 // Write all the registers
	 __raw_writeb(year,almyear);	 
	 __raw_writeb(mon,almmon);
	 __raw_writeb(day,almday);
	 __raw_writeb(hour,almhour);
	 __raw_writeb(min,almmin);
	 __raw_writeb(sec,almsec);
}

/* Sets time of alarm */
static void set_rtc_time(struct rtc_time *tval)
{
	
	int sec,min,hour,day,date,mon,year;
	 
	// Convert data from binary to 8-bit bcd
	sec = BIN_TO_BCD(tval->tm_sec);
	min = BIN_TO_BCD(tval->tm_min);
	hour = BIN_TO_BCD(tval->tm_hour);
	day = BIN_TO_BCD(tval->tm_mday);
	date = BIN_TO_BCD(tval->tm_wday);
	mon = BIN_TO_BCD(tval->tm_mon);
	 
	// Year is special
	year = tval->tm_year;
	if(year > 1999)
	 	year -=2000;
	else	
		year -=1900;
	
	year = BIN_TO_BCD(year);	
	
	 // Write all the registers
	 __raw_writeb(year,bcdyear);	 
	 __raw_writeb(mon,bcdmon);
	 __raw_writeb(date,bcddate);
	 __raw_writeb(day,bcdday);
	 __raw_writeb(hour,bcdhour);
	 __raw_writeb(min,bcdmin);
	 __raw_writeb(sec,bcdsec);
}

static void rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/* update irq data & counter */
	rtc_irq_data += 0x100;

	/* wake up waiting process */
	wake_up_interruptible(&rtc_wait);
	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
}

static int rtc_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit (1, &rtc_status))
		return -EBUSY;
	MOD_INC_USE_COUNT;
	rtc_irq_data = 0;
	return 0;
}

static int rtc_release(struct inode *inode, struct file *file)
{
	rtc_status = 0;
	MOD_DEC_USE_COUNT;
	return 0;
}

static int rtc_fasync (int fd, struct file *filp, int on)
{
	return fasync_helper (fd, filp, on, &rtc_async_queue);
}

static unsigned int rtc_poll(struct file *file, poll_table *wait)
{
	poll_wait (file, &rtc_wait, wait);
	return (rtc_irq_data) ? 0 : POLLIN | POLLRDNORM;
}

static loff_t rtc_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

ssize_t rtc_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long data;
	ssize_t retval;

	if (count < sizeof(unsigned long))
		return -EINVAL;

	add_wait_queue(&rtc_wait, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	for (;;) {
		spin_lock_irq (&rtc_lock);
		data = rtc_irq_data;
		if (data != 0) {
			rtc_irq_data = 0;
			break;
		}
		spin_unlock_irq (&rtc_lock);

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

	spin_unlock_irq (&rtc_lock);

	data -= 0x100;	/* the first IRQ wasn't actually missed */

	retval = put_user(data, (unsigned long *)buf);
	if (!retval)
		retval = sizeof(unsigned long);

out:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&rtc_wait, &wait);
	return retval;
}

static int rtc_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	volatile unsigned int rtcalm = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_RTCALM);
	volatile unsigned int ticnt = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_TICINT);
	
	struct rtc_time tm, tm2;
	switch (cmd) {
	case RTC_AIE_OFF:
		spin_lock_irq(&rtc_lock);
		__raw_writel(0,rtcalm);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_AIE_ON:
		spin_lock_irq(&rtc_lock);
		__raw_writel(0x7F,rtcalm);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_UIE_OFF:
		spin_lock_irq(&rtc_lock);
		__raw_writel(~TICNT_ENABLE,ticnt);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_UIE_ON:
		spin_lock_irq(&rtc_lock);
		__raw_writel(TICNT_ENABLE|TICNT_PERIOD,ticnt);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_PIE_OFF:
		spin_lock_irq(&rtc_lock);
		// Periodic int not available
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_PIE_ON:
		spin_lock_irq(&rtc_lock);
		// Periodic int not available
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	case RTC_ALM_READ:
		decodetime(get_alarm_time(),&tm);
		break;
	case RTC_ALM_SET:
		if (copy_from_user (&tm2, (struct rtc_time*)arg, sizeof (tm2)))
			return -EFAULT;
		decodetime(get_rtc_time(),&tm);
		if ((unsigned)tm2.tm_hour < 24)
			tm.tm_hour = tm2.tm_hour;
		if ((unsigned)tm2.tm_min < 60)
			tm.tm_min = tm2.tm_min;
		if ((unsigned)tm2.tm_sec < 60)
			tm.tm_sec = tm2.tm_sec;
		
		// Munge (as per sa1100)
		tm.tm_year+=1900;
		tm.tm_mon+=1;	
		
		// Set the alarm
		set_alarm_time(&tm);
		return 0;
	case RTC_RD_TIME:
		decodetime (get_rtc_time(), &tm);
		break;
	case RTC_SET_TIME:
		if (!capable(CAP_SYS_TIME))
			return -EACCES;
		if (copy_from_user (&tm, (struct rtc_time*)arg, sizeof (tm)))
			return -EFAULT;	
		tm.tm_year += 1900;
		if (tm.tm_year < epoch || (unsigned)tm.tm_mon >= 12 ||
		    tm.tm_mday < 1 || tm.tm_mday > (days_in_mo[tm.tm_mon] +
				(tm.tm_mon == 1 && is_leap(tm.tm_year))) ||
		    (unsigned)tm.tm_hour >= 24 ||
		    (unsigned)tm.tm_min >= 60 ||
		    (unsigned)tm.tm_sec >= 60)
			return -EINVAL;
		tm.tm_mon +=1;	// wierd: same as sa1100 though (gets month wrong otherwise!)
		set_rtc_time(&tm);
		return 0;
	case RTC_IRQP_READ:
		return put_user(rtc_freq, (unsigned long *)arg);
	case RTC_IRQP_SET:
		if (arg < 1 || arg > TIMER_FREQ)
			        return -EINVAL;
		if ((arg > 64) && (!capable(CAP_SYS_RESOURCE)))
			        return -EACCES;
		rtc_freq = arg;
		return 0;
	case RTC_EPOCH_READ:
		return put_user (epoch, (unsigned long *)arg);
	default:
		return -EINVAL;
	}
	return copy_to_user ((void *)arg, &tm, sizeof (tm)) ? -EFAULT : 0;
}

static struct file_operations rtc_fops = {
	.owner		= THIS_MODULE,
	.llseek		= rtc_llseek,
	.read		= rtc_read,
	.poll		= rtc_poll,
	.ioctl		= rtc_ioctl,
	.open		= rtc_open,
	.release	= rtc_release,
	.fasync		= rtc_fasync,
};

static struct miscdevice omahartc_miscdev = {
	.minor		= RTC_MINOR,
	.name		= "rtc",
	.fops		= &rtc_fops,
};

static int rtc_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len;
	struct rtc_time tm;

	decodetime (get_rtc_time(), &tm);
	p += sprintf(p, "rtc_time\t: %02d:%02d:%02d\n"
			"rtc_date\t: %04d-%02d-%02d\n"
			"rtc_epoch\t: %04d\n",
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, epoch);
	decodetime (get_alarm_time(), &tm);
	p += sprintf(p, "alrm_time\t: %02d:%02d:%02d\n"
			"alrm_date\t: %04d-%02d-%02d\n",
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	p += sprintf(p, "periodic_freq\t: %ld\n", rtc_freq);

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int __init rtc_init(void)
{
	volatile unsigned int ticnt = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_TICINT);
	volatile unsigned int rtccon = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_RTCCON);
	int ret;

	misc_register (&omahartc_miscdev);
	create_proc_read_entry ("driver/rtc", 0, 0, rtc_read_proc, NULL);
	
	// Enable RTC
	__raw_writel(RTC_ENABLE,rtccon);
	
	// Acquire 1Hz timer
	ret = request_irq (OMAHA_INT_TICK, rtc_interrupt, SA_INTERRUPT, "rtc 1Hz", NULL);
	if (ret) {
		printk (KERN_ERR "rtc: IRQ %d already in use.\n", OMAHA_INT_TICK);
		goto IRQ_TICK_failed;
	}

	// Acquire RTC (Alarm interrupt)
	ret = request_irq (OMAHA_INT_RTC, rtc_interrupt, SA_INTERRUPT, "rtc Alrm", NULL);
	if (ret) {
		printk (KERN_ERR "rtc: IRQ %d already in use.\n", OMAHA_INT_RTC);
		goto IRQ_RTC_failed;
	}

	printk (KERN_INFO "Omaha Real Time Clock driver v" DRIVER_VERSION "\n");

	// Program tick interrupt divisor to generate real 1Hz clock and enable the interrupt
	__raw_writeb(TICNT_ENABLE|TICNT_PERIOD,ticnt);	

	return 0;

IRQ_TICK_failed:
	free_irq (OMAHA_INT_TICK, NULL);
IRQ_RTC_failed:
	free_irq(OMAHA_INT_RTC, NULL);
	remove_proc_entry ("driver/rtc", NULL);
	misc_deregister (&omahartc_miscdev);
	return ret;
}

static void __exit rtc_exit(void)
{
	free_irq (OMAHA_INT_TICK, NULL);
	remove_proc_entry ("driver/rtc", NULL);
	misc_deregister (&omahartc_miscdev);
}

module_init(rtc_init);
module_exit(rtc_exit);

MODULE_AUTHOR("ARM Limited <support@arm.com>");
MODULE_DESCRIPTION("Omaha Realtime Clock Driver (RTC)");
EXPORT_NO_SYMBOLS;
