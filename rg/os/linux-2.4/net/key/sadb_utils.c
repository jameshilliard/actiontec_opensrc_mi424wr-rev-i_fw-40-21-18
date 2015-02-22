/* $USAGI: sadb_utils.c,v 1.5 2002/08/12 00:38:40 miyazawa Exp $ */
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
/*
 * sadb_utils.c include utility functions for SADB handling.
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/ipsec.h>
#include <net/pfkeyv2.h>
#include <net/sadb.h>
#include "sadb_utils.h"

#define BUFSIZE 64

/*
 * addr1, prefixlen1 : packet(must set 128 or 32 befor call this) 
 * addr2, prefixlen2 : sa/sp
 */
int
compare_address_with_prefix(struct sockaddr *addr1, __u8 prefixlen1,
			    struct sockaddr *addr2, __u8 prefixlen2)
{
	__u8 prefixlen;

	if (!addr1 || !addr2) {
		SADB_DEBUG("addr1 or add2 is NULL\n");
		return -EINVAL;
	}

	if (addr1->sa_family != addr2->sa_family) {
		SADB_DEBUG("sa_family not match\n");
		return 1;
	}

	if (prefixlen1 < prefixlen2) 
		prefixlen = prefixlen1;
	else
		prefixlen = prefixlen2;
	SADB_DEBUG("prefixlen: %d, prefixlen1: %d, prefixlen2: %d\n", prefixlen, prefixlen1, prefixlen2);

	switch (addr1->sa_family) {
	case AF_INET:
			if (prefixlen > 32 )
				return 1;
			return (((struct sockaddr_in *)addr1)->sin_addr.s_addr ^
				  ((struct sockaddr_in *)addr2)->sin_addr.s_addr) &
				 htonl((0xffffffff << (32 - prefixlen)));
	case AF_INET6:
			if (prefixlen > 128)
				return 1;

			return ipv6_prefix_cmp(&((struct sockaddr_in6 *)addr1)->sin6_addr,
					       &((struct sockaddr_in6 *)addr2)->sin6_addr,
					       prefixlen);
	default:
		SADB_DEBUG("unknown sa_family\n");
		return 1;
	}
}

