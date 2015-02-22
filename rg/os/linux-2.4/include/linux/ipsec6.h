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
#ifndef _LINUX_IPSEC6_H
#define _LINUX_IPSEC6_H

#include <linux/skbuff.h>
#include <net/ipv6.h>
#include <net/spd.h>

#ifdef __KERNEL__

#ifdef CONFIG_IPSEC_DEBUG
# ifdef CONFIG_SYSCTL
#  define IPSEC6_DEBUG(fmt, args...) 					\
do {									\
	if (sysctl_ipsec_debug_ipv6) {					\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args);	\
	}								\
} while(0)
# else
#  define IPSEC6_DEBUG(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
# endif /* CONFIG_SYSCTL */
#else
#  define IPSEC6_DEBUG(fmt, args...)
#endif

/* return value for ipsec6_output_check() -mk */
#define IPSEC_ACTION_BYPASS		0x0
#define IPSEC_ACTION_AUTH		0x1
#define IPSEC_ACTION_ESP		0x2
#define IPSEC_ACTION_COMP		0x4
#define IPSEC_ACTION_DROP		0x8

int ipsec6_init(void);
void ipsec6_cleanup (void); 
int ipsec6_input_check(struct sk_buff **skb, u8 *nexthdr);
int ipsec6_output_check(struct sock *sk, struct flowi *fl, const u8* data, struct ipsec_sp **policy_ptr);
int ipsec6_ndisc_check(struct in6_addr *saddr, struct in6_addr *daddr, struct ipsec_sp **policy_ptr);
int ipsec6_forward_check(struct sk_buff *skb);

#endif /* __KERNEL__ */

#endif /* _LINUX_IPSEC6_H */
