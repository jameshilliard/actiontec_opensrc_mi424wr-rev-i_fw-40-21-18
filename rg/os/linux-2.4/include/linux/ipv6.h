#ifndef _IPV6_H
#define _IPV6_H

#include <linux/in6.h>
#include <asm/byteorder.h>

/* The latest drafts declared increase in minimal mtu up to 1280. */

#define IPV6_MIN_MTU	1280

/*
 *	Advanced API
 *	source interface/address selection, source routing, etc...
 *	*under construction*
 */


struct in6_pktinfo {
	struct in6_addr	ipi6_addr;
	int		ipi6_ifindex;
};


struct in6_ifreq {
	struct in6_addr	ifr6_addr;
	__u32		ifr6_prefixlen;
	int		ifr6_ifindex; 
};

#define IPV6_SRCRT_STRICT	0x01	/* this hop must be a neighbor	*/
#define IPV6_SRCRT_TYPE_0	0	/* IPv6 type 0 Routing Header	*/

/*
 *	routing header
 */
struct ipv6_rt_hdr {
	__u8		nexthdr;
	__u8		hdrlen;
	__u8		type;
	__u8		segments_left;

	/*
	 *	type specific data
	 *	variable length field
	 */
};


struct ipv6_opt_hdr {
	__u8 		nexthdr;
	__u8 		hdrlen;
	/* 
	 * TLV encoded option data follows.
	 */
};

#define ipv6_destopt_hdr ipv6_opt_hdr
#define ipv6_hopopt_hdr  ipv6_opt_hdr

#ifdef __KERNEL__
#define ipv6_optlen(p)  (((p)->hdrlen+1) << 3)
#endif

/*
 *	routing header type 0 (used in cmsghdr struct)
 */

struct rt0_hdr {
	struct ipv6_rt_hdr	rt_hdr;
	__u32			reserved;	/* 'bitmap' is deprecated */
	struct in6_addr		addr[0];

#define rt0_type		rt_hdr.type;
};

/* IPsec6 header */
struct ipv6_auth_hdr {
	__u8  nexthdr;
	__u8  hdrlen;		/* This one is measured in 32 bit units! */
	__u16 reserved;
	__u32 spi;
	__u32 seq_no;		/* Sequence number */
	__u8  auth_data[4];	/* Length variable but >=4. Mind the 64 bit alignment! */
};

struct ipv6_esp_hdr {
	__u32 spi;
	__u32 seq_no;		/* Sequence number */
	__u8  enc_data[8];	/* Length variable but >=8. Mind the 64 bit alignment! */
};

/*
 *	IPv6 fixed header
 */

/* XXX: RFC2292bis */
struct ipv6hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8			tclass1:4,
				version:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8			version:4,
				tclass1:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	__u8			tclass2_flow[3];

	__u16			payload_len;
	__u8			nexthdr;
	__u8			hop_limit;

	struct	in6_addr	saddr;
	struct	in6_addr	daddr;
};

#ifdef __KERNEL__

/* 
   This structure contains results of exthdrs parsing
   as offsets from skb->nh.
 */

struct inet6_skb_parm
{
	int			iif;
	__u16			ra;
	__u16			hop;
	__u16			auth;
	__u16			dst0;
	__u16			srcrt;
#if defined(CONFIG_IPV6_MOBILITY) || (CONFIG_IPV6_MOBILITY_MODULE)
	__u16			hao;
#endif
	__u16			dst1;
	__u32			espspi; /* not offset */
};

#endif

#endif
