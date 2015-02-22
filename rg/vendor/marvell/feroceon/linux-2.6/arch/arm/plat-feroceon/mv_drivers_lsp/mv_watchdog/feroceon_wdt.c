/*
 * drivers/watchdog/feroceon_wdt.c
 *
 * Watchdog driver for Feroceon/Kirkwood processors
 *
 * Author: Sylver Bruneau <sylver.bruneau@googlemail.com>
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * April 14 2011
 * Actiontec Electronics Inc.
 * This file was modified to support Watchdog on Marvell CPU.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <feroceon_wdt.h>

/*
 * Watchdog timer block registers.
 */

#define TIMER_VIRT_BASE 	0xF1020300
#define RSTOUTn_MASK		0xF1020108
#define BRIDGE_CAUSE		0xF1020110

#define WDT_TIMER_REG_MAX_VAL	0x7FFFFFFF

#define TIMER_CTRL_REG		(TIMER_VIRT_BASE + 0x0000)
#define WDT_VAL_REG		(TIMER_VIRT_BASE + 0x0024)

#define WDT_INT_REQ_BIT		0x8
#define WDT_RESET_OUT_EN_BIT	0x2
#define WDT_EN_BIT		0x10

#define WDT_IN_USE		0
#define WDT_OK_TO_CLOSE		1

#define WDT_MAX_TIMEOUT_VALUE	10 /* seconds, Derived from WDT_TIMER_REG_MAX_VAL/CPU_TICKS */

#define WDT_TIMEOUT_VALUE		2 /* seconds */
#define WDT_USERSPACE_TIMEOUT_VALUE	60	/* seconds */
#define WDT_TIMER_MINUTE_COUNTER	((WDT_USERSPACE_TIMEOUT_VALUE)/(WDT_TIMEOUT_VALUE))


/***********************************************************************
***********************************************************************/
/* Module parameters */
static int heartbeat = WDT_TIMEOUT_VALUE;

static unsigned int wdt_tclk;
static unsigned long wdt_status;

static struct timer_list wdt_timer;
static int wdt_timer_initialized;
static int wdt_userspace_heartbeat;
static spinlock_t wdt_lock;


/***********************************************************************
***********************************************************************/
static void feroceon_wdt_ping(void)
{
    spin_lock(&wdt_lock);
    /* Reload watchdog duration */
    writel(WDT_TIMER_REG_MAX_VAL, WDT_VAL_REG);
    spin_unlock(&wdt_lock);
}

/* 
 * Under heavy traffic load conditions the Userspave watchdog may not 
 * get enough CPU time to update the Watchdog Timer Register and subsequently
 * result in the system rebooting unnecessarily. To handle this condition
 * there is Kernel Watchdog timer also.
 */
static void watchdog_timer_cb(unsigned long data)
{
    /* Check if Userspace is locked up*/
    ++wdt_userspace_heartbeat;
    if (wdt_userspace_heartbeat >= WDT_TIMER_MINUTE_COUNTER)
	{
	    printk("%lu: Userspace watchdog has expired (time elapsed=%d seconds)\n", 
				jiffies, (wdt_userspace_heartbeat*WDT_TIMEOUT_VALUE));
	    wdt_userspace_heartbeat = 0;
	}

    feroceon_wdt_ping();
    mod_timer(&wdt_timer, (jiffies + WDT_TIMEOUT_VALUE*HZ));
}

static void feroceon_wdt_enable(void)
{
    u32 reg;

    spin_lock(&wdt_lock);

    /* Set watchdog duration */
    writel(WDT_TIMER_REG_MAX_VAL, WDT_VAL_REG);

    /* Clear watchdog timer interrupt */
    reg = readl(BRIDGE_CAUSE);
    reg &= ~WDT_INT_REQ_BIT;
    writel(reg, BRIDGE_CAUSE);

    /* Enable watchdog timer */
    reg = readl(TIMER_CTRL_REG);
    reg |= WDT_EN_BIT;
    writel(reg, TIMER_CTRL_REG);

    /* Enable reset on watchdog */
    reg = readl(RSTOUTn_MASK);
    reg |= WDT_RESET_OUT_EN_BIT;
    writel(reg, RSTOUTn_MASK);

    if (!wdt_timer_initialized)
    {
	init_timer(&wdt_timer);
	wdt_timer.function = watchdog_timer_cb;
	wdt_timer_initialized = 1;
    }

    mod_timer(&wdt_timer, jiffies + HZ);

    spin_unlock(&wdt_lock);
}

static void feroceon_wdt_disable(void)
{
    u32 reg;

    spin_lock(&wdt_lock);

    if (wdt_timer_initialized)
    {
	del_timer(&wdt_timer);
	wdt_timer_initialized = 0;
    }

    /* Disable reset on watchdog */
    reg = readl(RSTOUTn_MASK);
    reg &= ~WDT_RESET_OUT_EN_BIT;
    writel(reg, RSTOUTn_MASK);

    /* Disable watchdog timer */
    reg = readl(TIMER_CTRL_REG);
    reg &= ~WDT_EN_BIT;
    writel(reg, TIMER_CTRL_REG);

    spin_unlock(&wdt_lock);
}

#if 0
static int feroceon_wdt_get_timeleft(int *time_left)
{
    spin_lock(&wdt_lock);
    *time_left = readl(WDT_VAL_REG) / wdt_tclk;
    spin_unlock(&wdt_lock);
    return 0;
}
#endif

static int feroceon_wdt_open(struct inode *inode, struct file *file)
{
    if (test_and_set_bit(WDT_IN_USE, &wdt_status))
	return -EBUSY;
    clear_bit(WDT_OK_TO_CLOSE, &wdt_status);
    return nonseekable_open(inode, file);
}

static int feroceon_wdt_settimeout(int new_time)
{
    if ((new_time <= 0) || (new_time > WDT_MAX_TIMEOUT_VALUE))
	return -EINVAL;
    heartbeat = new_time;
    return 0;
}

static const struct watchdog_info ident = {
    .options	= WDIOF_MAGICCLOSE | WDIOF_SETTIMEOUT |
	WDIOF_KEEPALIVEPING,
    .identity	= "Feroceon Watchdog",
};

static long feroceon_wdt_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
    int ret = -ENOTTY;
    int new_options, time;

    switch (cmd) {
	case WDIOC_GETSUPPORT:
	    ret = copy_to_user((struct watchdog_info *)arg, &ident,
		    sizeof(ident)) ? -EFAULT : 0;
	    break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
	    ret = put_user(0, (int *)arg);
	    break;

	case WDIOC_KEEPALIVE:
	    feroceon_wdt_ping();
	    wdt_userspace_heartbeat = 0;
	    ret = 0;
	    break;

	case WDIOC_SETTIMEOUT:
	    ret = get_user(time, (int *)arg);
	    if (ret)
		break;

	    if (feroceon_wdt_settimeout(time)) {
		ret = -EINVAL;
		break;
	    }
	    feroceon_wdt_ping();
	    wdt_userspace_heartbeat = 0;
	    /* Fall through */

	case WDIOC_GETTIMEOUT:
	    ret = put_user(heartbeat, (int *)arg);
	    break;

#if 0
	case WDIOC_GETTIMELEFT:
	    if (feroceon_wdt_get_timeleft(&time)) {
		ret = -EINVAL;
		break;
	    }
	    ret = put_user(time, (int *)arg);
	    break;
#endif

	case WDIOC_SETOPTIONS:
	    if (get_user (new_options, (int *)arg))
	    {
		ret = -EFAULT;
		break;
	    }

	    if (new_options & WDIOS_DISABLECARD) {
		feroceon_wdt_disable();
		ret = 0;
	    }

	    if (new_options & WDIOS_ENABLECARD) {
		feroceon_wdt_enable();
		ret = 0;
	    }
	    wdt_userspace_heartbeat = 0;
	    break;
    }

    return ret;
}

static int feroceon_wdt_release(struct inode *inode, struct file *file)
{
    if (test_bit(WDT_OK_TO_CLOSE, &wdt_status))
	feroceon_wdt_disable();
    else
	printk(KERN_CRIT "WATCHDOG: Device closed unexpectedly - "
		"timer will not stop\n");
    clear_bit(WDT_IN_USE, &wdt_status);
    clear_bit(WDT_OK_TO_CLOSE, &wdt_status);

    return 0;
}


static struct file_operations feroceon_wdt_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= feroceon_wdt_ioctl,
	.open		= feroceon_wdt_open,
	.release	= feroceon_wdt_release,
};

static struct miscdevice feroceon_wdt_miscdev = {
	.minor		= WATCHDOG_MINOR,
	.name		= "watchdog",
	.fops		= &feroceon_wdt_fops,
};

static int __devinit feroceon_wdt_probe(struct platform_device *pdev)
{
    struct feroceon_wdt_platform_data *pdata = (struct feroceon_wdt_platform_data *) pdev->dev.platform_data;
    int ret;

    if (pdata) {
	wdt_tclk = pdata->tclk;
    } else {
	printk(KERN_ERR "Feroceon Watchdog misses platform data\n");
	return -ENODEV;
    }

    if (feroceon_wdt_miscdev.dev)
	return -EBUSY;
    feroceon_wdt_miscdev.dev = &pdev->dev;

    if (feroceon_wdt_settimeout(heartbeat))
	heartbeat = WDT_MAX_TIMEOUT_VALUE;

    ret = misc_register(&feroceon_wdt_miscdev);
    if (ret)
	return ret;

    printk("Feroceon Watchdog Timer: Initial timeout %d sec\n", heartbeat);
    return 0;
}

static int __devexit feroceon_wdt_remove(struct platform_device *pdev)
{
    int ret;

    if (test_bit(WDT_IN_USE, &wdt_status)) {
	feroceon_wdt_disable();
	clear_bit(WDT_IN_USE, &wdt_status);
    }

    ret = misc_deregister(&feroceon_wdt_miscdev);
    if (!ret)
	feroceon_wdt_miscdev.dev = NULL;

    return ret;
}

static void feroceon_wdt_shutdown(struct platform_device *pdev)
{
    if (test_bit(WDT_IN_USE, &wdt_status))
	feroceon_wdt_disable();
}

static struct platform_driver feroceon_wdt_driver = {
	.probe		= feroceon_wdt_probe,
	.remove		= __devexit_p(feroceon_wdt_remove),
	.shutdown	= feroceon_wdt_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "feroceon_wdt",
	},
};

static int __init feroceon_wdt_init(void)
{
    spin_lock_init(&wdt_lock);

    return platform_driver_register(&feroceon_wdt_driver);
}

static void __exit feroceon_wdt_exit(void)
{
    platform_driver_unregister(&feroceon_wdt_driver);
}

module_init(feroceon_wdt_init);
module_exit(feroceon_wdt_exit);

MODULE_AUTHOR("Sylver Bruneau <sylver.bruneau@googlemail.com>");
MODULE_DESCRIPTION("Feroceon Processor Watchdog");

module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog heartbeat (timer interval) in seconds");

MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
