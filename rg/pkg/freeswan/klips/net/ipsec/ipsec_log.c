/****************************************************************************
 *
 * rg/pkg/freeswan/klips/net/ipsec/ipsec_log.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h> /* jiffies */
#include <asm/param.h> /* HZ */
#include "ipsec_log.h"

#ifdef CONFIG_RG_OS_LINUX_22
/* Kernel 2.2 has no vsnprintf.
 * Use define like printk.
 */
#define vsnprintf(buf, size, fmt, va) vsprintf(buf, fmt, va)
#endif

int debug_log_all;
static char print_more[256];
static int print_more_off;

int ipsec_rate_limit(void)
{
    static int rl_count;
    static unsigned long rl_time;	/* last time printed (seconds) */
    unsigned long curr = jiffies/HZ;

    if (debug_log_all)
	return 1;

    if (curr > rl_time) /* once a second - allow and zero counter */
    {
	if (rl_count)
	{
	    printk(KERN_INFO "RATELIMIT: %d messages of type IPSec kernel "
		" packet reported %lu second(s) ago\n", rl_count,
		curr - rl_time);
	}
	rl_count = 0;
	rl_time = curr;
	return 1;
    }
    rl_count++;
    return 0;
}

void ipsec_log(char *format, ...)
{
    va_list args;
    char msg[1024];

    if (!ipsec_rate_limit())
	return;

    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    printk("%s", msg);
    va_end(args);
}

void klips_print_more(int is_start, char *format, ...)
{
    va_list args;
    int len;

    if (print_more_off >= sizeof(print_more) && !is_start)
	return;
    if (is_start)
	print_more_off = 0;
    va_start(args, format);
    len = vsnprintf(print_more + print_more_off,
	sizeof(print_more)-print_more_off, format, args);
    va_end(args);

    if (len <= 0)
	return;
    
    print_more_off += len;
}

void klips_print_more_finish(void)
{
    if (print_more_off && ipsec_rate_limit())
	printk(KERN_DEBUG "%s\n", print_more);
    if (print_more_off >= sizeof(print_more))
    {
	printk(KERN_DEBUG "klips_debug: Buffer is full, messages were lost or "
	    "truncated\n");
    }
    print_more_off = 0;
}

EXPORT_SYMBOL(ipsec_rate_limit);
EXPORT_SYMBOL(ipsec_log);

