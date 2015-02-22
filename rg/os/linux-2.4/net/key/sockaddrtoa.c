/* $USAGI: sockaddrtoa.c,v 1.1 2001/12/23 16:01:12 mk Exp $ */
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
#ifdef MODULE
#include <linux/module.h>
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#endif

#include <linux/config.h>
#include <asm/byteorder.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/inet.h>
#include "sadb_utils.h"

#define BUFSIZE 64 /* IPv6 addr */

size_t
sockaddrtoa(struct sockaddr *addr, char *buf, size_t buflen)
{
	char *ret = NULL;

	switch (addr->sa_family) {
	case AF_INET:
		ret = in_ntop(&(((struct sockaddr_in*)addr)->sin_addr), buf);
		break;
	case AF_INET6:
		ret = in6_ntop(&(((struct sockaddr_in6*)addr)->sin6_addr), buf);
		break;
	}

	return ret ? 0 : -EINVAL;
}

int
sockporttoa(struct sockaddr *addr, char *buf, size_t buflen)
{
       
	switch (addr->sa_family) {
	case AF_INET:
		sprintf(buf, "%hd", ntohs(((struct sockaddr_in *)addr)->sin_port));
		break;
	case AF_INET6:
		sprintf(buf, "%hd", ntohs(((struct sockaddr_in6*)addr)->sin6_port));
		break;
	default:
		printk(KERN_WARNING "sockporttoa: unrecognized socket family: %d\n", addr->sa_family);
		return -EINVAL;
		break;
	}

       return 0;
}


