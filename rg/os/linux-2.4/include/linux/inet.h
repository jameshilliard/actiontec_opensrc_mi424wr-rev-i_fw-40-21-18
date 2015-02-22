/*
 *		Swansea University Computer Society NET3
 *
 *	This work is derived from NET2Debugged, which is in turn derived
 *	from NET2D which was written by:
 * 		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This work was derived from Ross Biro's inspirational work
 *		for the LINUX operating system.  His version numbers were:
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_INET_H
#define _LINUX_INET_H

#ifdef __KERNEL__

struct net_proto;
struct in_addr;

extern void		inet_proto_init(struct net_proto *pro);
extern char		*in_ntoa(__u32 in);
extern char		*in_ntop(struct in_addr *in, char *buf);
extern __u32		in_aton(const char *str);
#if defined(CONFIG_IPV6)
struct in6_addr;
extern char  *in6_ntop(const struct in6_addr *in6, char *buf);
#elif defined(CONFIG_IPV6_MODULE)
#include <linux/in6.h>
extern __inline__ char	*in6_ntop(const struct in6_addr *in6, char *buf){
	if (!buf)
		return NULL;
	sprintf(buf,
		"%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
		ntohs(in6->s6_addr16[0]), ntohs(in6->s6_addr16[1]),
		ntohs(in6->s6_addr16[2]), ntohs(in6->s6_addr16[3]),
		ntohs(in6->s6_addr16[4]), ntohs(in6->s6_addr16[5]),
		ntohs(in6->s6_addr16[6]), ntohs(in6->s6_addr16[7]));
	return buf;
}
#endif

#endif
#endif	/* _LINUX_INET_H */
