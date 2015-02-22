/*
 *	Watchdog driver for the SA11x0
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
#include <asm/hardware.h>
#include <asm/bitops.h>

#define TIMER_MARGIN	60		/* (secs) Default is 1 minute */

static int sa1100_margin = TIMER_MARGIN;	/* in seconds */
static int sa1100wdt_users;
static int pre_margin;
#ifdef MODULE
MODULE_PARM(sa1100_margin,"i");
#endif

/*
 *	Allow only one person to hold it open
 */

static int sa1100dog_open(struct inode *inode, struct file *file)
{
	if(test_and_set_bit(1,&sa1100wdt_users))
		return -EBUSY;
	MOD_INC_USE_COUNT;
	/* Activate SA1100 Watchdog timer */
	pre_margin=3686400 * sa1100_margin;
	OSMR3 = OSCR + pre_margin;
	OSSR = OSSR_M3;
	OWER = OWER_WME;
	OIER |= OIER_E3;
	return 0;
}

static int sa1100dog_release(struct inode *inode, struct file *file)
{
	/*
	 *	Shut off the timer.
	 * 	Lock it in if it's a module and we defined ...NOWAYOUT
	 */
	OSMR3 = OSCR + pre_margin;
#ifndef CONFIG_WATCHDOG_NOWAYOUT
	OIER &= ~OIER_E3;
#endif
	sa1100wdt_users = 0;
	MOD_DEC_USE_COUNT;
	return 0;
}

static ssize_t sa1100dog_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	/* Refresh OSMR3 timer. */
	if(len) {
		OSMR3 = OSCR + pre_margin;
		return 1;
	}
	return 0;
}

static int sa1100dog_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	static struct watchdog_info ident = {
		identity: "SA1100 Watchdog",
	};

	switch(cmd){
	default:
		return -ENOIOCTLCMD;
	case WDIOC_GETSUPPORT:
		return copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident));
	case WDIOC_GETSTATUS:
		return put_user(0,(int *)arg);
	case WDIOC_GETBOOTSTATUS:
		return put_user((RCSR & RCSR_WDR) ? WDIOF_CARDRESET : 0, (int *)arg);
	case WDIOC_KEEPALIVE:
		OSMR3 = OSCR + pre_margin;
		return 0;
	}
}

static struct file_operations sa1100dog_fops=
{
	owner:		THIS_MODULE,
	write:		sa1100dog_write,
	ioctl:		sa1100dog_ioctl,
	open:		sa1100dog_open,
	release:	sa1100dog_release,
};

static struct miscdevice sa1100dog_miscdev=
{
	WATCHDOG_MINOR,
	"SA1100 watchdog",
	&sa1100dog_fops
};

static int __init sa1100dog_init(void)
{
	int ret;

	ret = misc_register(&sa1100dog_miscdev);

	if (ret)
		return ret;

	printk("SA1100 Watchdog Timer: timer margin %d sec\n", sa1100_margin);

	return 0;
}

static void __exit sa1100dog_exit(void)
{
	misc_deregister(&sa1100dog_miscdev);
}

module_init(sa1100dog_init);
module_exit(sa1100dog_exit);
