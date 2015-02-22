/*
 *	Types and definitions for AF_INET6 
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	Sources:
 *	IPv6 Program Interfaces for BSD Systems
 *      <draft-ietf-ipngwg-bsd-api-05.txt>
 *
 *	Advanced Sockets API for IPv6
 *	<draft-stevens-advanced-api-00.txt>
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_IN6_H
#define _LINUX_IN6_H

#define __USAGI__

#include <linux/types.h>

/*
 *	IPv6 address structure
 */

struct in6_addr
{
	union 
	{
		__u8		u6_addr8[16];
		__u16		u6_addr16[8];
		__u32		u6_addr32[4];
	} in6_u;
#define s6_addr			in6_u.u6_addr8
#define s6_addr16		in6_u.u6_addr16
#define s6_addr32		in6_u.u6_addr32
};

/* IPv6 Wildcard Address (::) and Loopback Address (::1) defined in RFC2553
 * NOTE: Be aware the IN6ADDR_* constants and in6addr_* externals are defined
 * in network byte order, not in host byte order as are the IPv4 equivalents
 */
extern const struct in6_addr in6addr_any;
#define IN6ADDR_ANY_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } }
extern const struct in6_addr in6addr_loopback;
#define IN6ADDR_LOOPBACK_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }

struct sockaddr_in6 {
	unsigned short int	sin6_family;    /* AF_INET6 */
	__u16			sin6_port;      /* Transport layer port # */
	__u32			sin6_flowinfo;  /* IPv6 flow information */
	struct in6_addr		sin6_addr;      /* IPv6 address */
	__u32			sin6_scope_id;  /* scope id (new in RFC2553) */
};

struct ipv6_mreq {
	/* IPv6 multicast address of group */
	struct in6_addr ipv6mr_multiaddr;

	/* local IPv6 address of interface */
	int		ipv6mr_ifindex;
};

#define ipv6mr_acaddr	ipv6mr_multiaddr

struct in6_flowlabel_req
{
	struct in6_addr	flr_dst;
	__u32	flr_label;
	__u8	flr_action;
	__u8	flr_share;
	__u16	flr_flags;
	__u16 	flr_expires;
	__u16	flr_linger;
	__u32	__flr_pad;
	/* Options in format of IPV6_PKTOPTIONS */
};

#define IPV6_FL_A_GET	0
#define IPV6_FL_A_PUT	1
#define IPV6_FL_A_RENEW	2

#define IPV6_FL_F_CREATE	1
#define IPV6_FL_F_EXCL		2

#define IPV6_FL_S_NONE		0
#define IPV6_FL_S_EXCL		1
#define IPV6_FL_S_PROCESS	2
#define IPV6_FL_S_USER		3
#define IPV6_FL_S_ANY		255


/*
 *	Bitmask constant declarations to help applications select out the 
 *	flow label and traffic class fields.
 *
 *	Note that this are in host byte order while the flowinfo field of
 *	sockaddr_in6 is in network byte order.
 */

#define IPV6_FLOWINFO_FLOWLABEL		0x000fffff
#define IPV6_FLOWINFO_TCLASS		0x0ff00000


/*
 *	IPV6 extension headers
 */
#define IPPROTO_HOPOPTS		0	/* IPv6 hop-by-hop options	*/
#define IPPROTO_ROUTING		43	/* IPv6 routing header		*/
#define IPPROTO_FRAGMENT	44	/* IPv6 fragmentation header	*/
#define IPPROTO_ICMPV6		58	/* ICMPv6			*/
#define IPPROTO_NONE		59	/* IPv6 no next header		*/
#define IPPROTO_DSTOPTS		60	/* IPv6 destination options	*/

/*
 *	IPv6 TLV options.
 */
#define IPV6_TLV_PAD0		0
#define IPV6_TLV_PADN		1
#define IPV6_TLV_ROUTERALERT	5
#define IPV6_TLV_JUMBO		194

/*
 *	Mobile IPv6 TLV options.
 */
#define MIPV6_TLV_BINDACK	7
#define MIPV6_TLV_BINDRQ	8
#define MIPV6_TLV_BINDUPDATE	198
#define MIPV6_TLV_HOMEADDR	201

/*
 *	IPV6 socket options
 */

/*
 *	NOTE: 	26, 48-63 are reserved by USAGI Project.
 *		For further information, see <http://www.linux-ipv6.org>
 *
 */

#define IPV6_ADDRFORM		1
#define IPV6_PKTINFO		2
#define IPV6_HOPOPTS		3
#define IPV6_DSTOPTS		4
#define IPV6_RTHDR		5
#define IPV6_PKTOPTIONS		6
#define IPV6_CHECKSUM		7
#define IPV6_HOPLIMIT		8
#define IPV6_NEXTHOP		9
#define IPV6_AUTHHDR		10
#define IPV6_FLOWINFO		11

#define IPV6_UNICAST_HOPS	16
#define IPV6_MULTICAST_IF	17
#define IPV6_MULTICAST_HOPS	18
#define IPV6_MULTICAST_LOOP	19
#define IPV6_JOIN_GROUP		20
#define IPV6_ADD_MEMBERSHIP	IPV6_JOIN_GROUP
#define IPV6_LEAVE_GROUP	21
#define IPV6_DROP_MEMBERSHIP	IPV6_LEAVE_GROUP
#define IPV6_ROUTER_ALERT	22
#define IPV6_MTU_DISCOVER	23
#define IPV6_MTU		24
#define IPV6_RECVERR		25
#define IPV6_V6ONLY		26
#define IPV6_JOIN_ANYCAST	27
#define IPV6_LEAVE_ANYCAST	28

/* IPV6_MTU_DISCOVER values */
#define IPV6_PMTUDISC_DONT		0
#define IPV6_PMTUDISC_WANT		1
#define IPV6_PMTUDISC_DO		2

/* Flowlabel */
#define IPV6_FLOWLABEL_MGR	32
#define IPV6_FLOWINFO_SEND	33

/* draft-itojun-ipv6-tclass-api-01.txt */
#define IPV6_RECVTCLASS		66
#define IPV6_TCLASS		67

/* usagi extension */
#define IPV6_PRIVACY		68

/*
 * IPv6 address testing macros (RFC2553; some from KAME)
 * (to avoid alignment issue, don't use __u64)
 */
#ifdef __KERNEL__
#define IPV6_ADDR_SCOPE_NODELOCAL	0x01
#define IPV6_ADDR_SCOPE_LINKLOCAL	0x02
#define IPV6_ADDR_SCOPE_SITELOCAL	0x05
#define IPV6_ADDR_SCOPE_ORGLOCAL	0x08
#define IPV6_ADDR_SCOPE_GLOBAL		0x0e

#define	IN6_IS_ADDR_UNSPECIFIED(a)			\
	((((a)->s6_addr32[0]) == 0) &&			\
	 (((a)->s6_addr32[1]) == 0) &&			\
	 (((a)->s6_addr32[2]) == 0) &&			\
	 (((a)->s6_addr32[3]) == 0))
#define	IN6_IS_ADDR_LOOPBACK(a)				\
	((((a)->s6_addr32[0]) == 0) &&			\
	 (((a)->s6_addr32[1]) == 0) &&			\
	 (((a)->s6_addr32[2]) == 0) && 			\
	 (((a)->s6_addr32[3]) == __constant_htonl(1)))
#define IN6_IS_ADDR_V4COMPAT(a)				\
	((((a)->s6_addr32[0]) == 0) &&			\
	 (((a)->s6_addr32[1]) == 0) &&			\
	 (((a)->s6_addr32[2]) == 0) &&			\
	 (((a)->s6_addr32[3]) != __constant_htonl(1)))
#define IN6_IS_ADDR_V4MAPPED(a)				\
	((((a)->s6_addr32[0]) == 0) &&			\
	 (((a)->s6_addr32[1]) == 0) &&			\
	 (((a)->s6_addr32[2]) == __constant_htonl(0x0000ffff)))
#define IN6_IS_ADDR_LINKLOCAL(a)	\
	(((a)->s6_addr32[0] & __constant_htonl(0xffc00000)) == __constant_htons(0xfe800000))
#define IN6_IS_ADDR_SITELOCAL(a)	\
	(((a)->s6_addr32[0] & __constant_htonl(0xffc00000)) == __constant_htons(0xfec00000))
#define IN6_IS_ADDR_MULTICAST(a)	\
	((a)->s6_addr[0] == 0xff)

#define IPV6_ADDR_MC_SCOPE(a)		\
	((a)->s6_addr[1] & 0x0f)	/* XXX nonstandard */
#define IN6_IS_ADDR_MC_SCOPE(a,b)	\
	(IPV6_ADDR_MC_SCOPE(a) == (b))	/* XXX nonstandard */

#define IN6_IS_ADDR_MC_NODELOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 IN6_IS_ADDR_MC_SCOPE(a,IPV6_ADDR_SCOPE_NODELOCAL))
#define IN6_IS_ADDR_MC_LINKLOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 IN6_IS_ADDR_MC_SCOPE(a,IPV6_ADDR_SCOPE_LINKLOCAL))
#define IN6_IS_ADDR_MC_SITELOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 IN6_IS_ADDR_MC_SCOPE(a,IPV6_ADDR_SCOPE_SITELOCAL))
#define IN6_IS_ADDR_MC_ORGLOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 IN6_IS_ADDR_MC_SCOPE(a,IPV6_ADDR_SCOPE_ORGLOCAL))
#define IN6_IS_ADDR_MC_GLOBAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 IN6_IS_ADDR_MC_SCOPE(a,IPV6_ADDR_SCOPE_GLOBAL))
#endif

#endif
