/*
 *	Watchdog driver for the Omaha
 *	(C) ARM Limited 2002.
 *
 *	Based on sa1100_wdt.c
 *
 *      (c) Copyright 2000 Oleg Drokin <green@crimea.edu>
 *          Based on SoftDog driver by Alan Cox <alan@redhat.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Oleg Drokin nor iXcelerator.com admit liability nor provide
 *	warranty for any of this software. This material is provided
 *	"AS-IS" and at no charge.
 *
 *	(c) Copyright 2000           Oleg Drokin <green@crimea.edu>
 *
 *      27/11/2000 Initial release
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/bitops.h>

#define TIMER_MARGIN	8		/* (secs) Default is 1 minute */
#define WT_TPS		7812		/* Watchdog ticks per second. */
#define WT_ENABLE	0x21		// Enable bits for watchdog
#define WT_CLKSEL_128	0x18		// Select 1/128 divider

static int omaha_margin = TIMER_MARGIN;	/* in seconds */
static int omahawdt_users;
static int pre_margin;
#ifdef MODULE
MODULE_PARM(omaha_margin,"i");
#endif

/*
 *	Allow only one person to hold it open
 */

static int omahadog_open(struct inode *inode, struct file *file)
{
	volatile unsigned int wtcon = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_WTCON);
	volatile unsigned int wtdat = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_WTDAT);
	volatile unsigned int wtcnt = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_WTCNT);
	unsigned int tmp;

	if(test_and_set_bit(1,&omahawdt_users))
		return -EBUSY;
	MOD_INC_USE_COUNT;
	/* Activate omaha Watchdog timer */
	
	/* Assume that uhal has set up pre-scaler according
	 * to the system clock frequency (don't change it!)
	 * 
	 * Ie. all counting occurs at 1MHz / 128 = 7812.5Hz
	 *
	 * Since we have 16-bit counter, maximum period is
	 * 65536/7812.5	= 8.338608 seconds!
	 */
	 
	pre_margin = WT_TPS * omaha_margin;
	
	// Set count to the maximum
	__raw_writel(pre_margin,wtcnt);
	
	// Set the clock division factor
	tmp = __raw_readl(wtcon);
	tmp |= WT_CLKSEL_128;
	__raw_writel(tmp,wtcon);	
	
	// Program an initial count into WTDAT
	__raw_writel(0x8000,wtdat);
	
	// enable the watchdog
	tmp = __raw_readl(wtcon);
	tmp |= WT_ENABLE;

	__raw_writel(tmp,wtcon);
	
	return 0;
}

static int omahadog_release(struct inode *inode, struct file *file)
{
	volatile unsigned int wtcon = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_WTCON);
	unsigned int tmp;

	/*
	 *	Shut off the timer.
	 * 	Lock it in if it's a module and we defined ...NOWAYOUT
	 */
#ifndef CONFIG_WATCHDOG_NOWAYOUT
	tmp = __raw_readl(wtcon);
	tmp &= ~WT_ENABLE;
	__raw_writel(tmp,wtcon);
#endif
	omahawdt_users = 0;
	MOD_DEC_USE_COUNT;
	return 0;
}

static ssize_t omahadog_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
	volatile unsigned int wtcnt = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_WTCNT);

	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	/* Refresh timer. */
	if(len) {
		__raw_writel(pre_margin,wtcnt);
		return 1;
	}
	return 0;
}

static int omahadog_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	volatile unsigned int wtdat = IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_WTDAT);
	static struct watchdog_info ident = {
		identity: "omaha Watchdog",
	};

	switch(cmd){
	default:
		return -ENOIOCTLCMD;
	case WDIOC_GETSUPPORT:
		return copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident));
	case WDIOC_GETSTATUS:
		return put_user(0,(int *)arg);
	case WDIOC_GETBOOTSTATUS:
		return 0;
	case WDIOC_KEEPALIVE:
		__raw_writel(pre_margin,wtdat);
		return 0;
	}
}

static struct file_operations omahadog_fops=
{
	.owner		= THIS_MODULE,
	.write		= omahadog_write,
	.ioctl		= omahadog_ioctl,
	.open		= omahadog_open,
	.release	= omahadog_release,
};

static struct miscdevice omahadog_miscdev=
{
	.minor		= WATCHDOG_MINOR,
	.name		= "omaha watchdog",
	.fops		= &omahadog_fops
};

static int __init omahadog_init(void)
{
	int ret;

	ret = misc_register(&omahadog_miscdev);

	if (ret)
		return ret;

	printk("Omaha Watchdog Timer: timer margin %d sec\n", omaha_margin);

	return 0;
}

static void __exit omahadog_exit(void)
{
	misc_deregister(&omahadog_miscdev);
}

module_init(omahadog_init);
module_exit(omahadog_exit);
