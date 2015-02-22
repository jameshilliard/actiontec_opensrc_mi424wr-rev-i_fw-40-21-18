/*
 * Copyright (C)2001 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/byteorder.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/inet.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/smp.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <net/ipv6.h>
#include <net/sadb.h>
#include <net/spd.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/snmp.h>  
#include <net/ipsec6_utils.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/icmpv6.h>
#include <linux/ipsec.h>
#include <linux/ipsec6.h>
#include <net/pfkeyv2.h> /* sa proto type */
#include <net/pfkey.h>

int ipsec6_init(void)
{
	printk(KERN_INFO "IPv6/IPsec registered\n");
	return 0;
}

void ipsec6_cleanup (void)
{
	printk(KERN_INFO "IPv6/IPsec unregistered\n");
}

