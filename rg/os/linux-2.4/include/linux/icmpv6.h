#ifndef _LINUX_ICMPV6_H
#define _LINUX_ICMPV6_H

#include <asm/byteorder.h>

struct icmp6hdr {

	__u8		icmp6_type;
	__u8		icmp6_code;
	__u16		icmp6_cksum;


	union {
		__u32			un_data32[1];
		__u16			un_data16[2];
		__u8			un_data8[4];

		struct icmpv6_echo {
			__u16		identifier;
			__u16		sequence;
		} u_echo;

                struct icmpv6_nd_advt {
#if defined(__LITTLE_ENDIAN_BITFIELD)
                        __u32		reserved:5,
                        		override:1,
                        		solicited:1,
                        		router:1,
					reserved2:24;
#elif defined(__BIG_ENDIAN_BITFIELD)
                        __u32		router:1,
					solicited:1,
                        		override:1,
                        		reserved:29;
#else
#error	"Please fix <asm/byteorder.h>"
#endif						
                } u_nd_advt;

                struct icmpv6_nd_ra {
			__u8		hop_limit;
#if defined(__LITTLE_ENDIAN_BITFIELD)
			__u8		reserved:5,
				        home_agent:1,
					other:1,
					managed:1;

#elif defined(__BIG_ENDIAN_BITFIELD)
			__u8		managed:1,
					other:1,
				        home_agent:1,
					reserved:5;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
			__u16		rt_lifetime;
                } u_nd_ra;

		struct icmpv6_ni {
			__u16		qtype;
			__u16		flags;
		} u_ni;

	} icmp6_dataun;

#define icmp6_identifier	icmp6_dataun.u_echo.identifier
#define icmp6_sequence		icmp6_dataun.u_echo.sequence
#define icmp6_pointer		icmp6_dataun.un_data32[0]
#define icmp6_mtu		icmp6_dataun.un_data32[0]
#define icmp6_unused		icmp6_dataun.un_data32[0]
#define icmp6_maxdelay		icmp6_dataun.un_data16[0]
#define icmp6_router		icmp6_dataun.u_nd_advt.router
#define icmp6_solicited		icmp6_dataun.u_nd_advt.solicited
#define icmp6_override		icmp6_dataun.u_nd_advt.override
#define icmp6_ndiscreserved	icmp6_dataun.u_nd_advt.reserved
#define icmp6_hop_limit		icmp6_dataun.u_nd_ra.hop_limit
#define icmp6_addrconf_managed	icmp6_dataun.u_nd_ra.managed
#define icmp6_addrconf_other	icmp6_dataun.u_nd_ra.other
#define icmp6_rt_lifetime	icmp6_dataun.u_nd_ra.rt_lifetime
#define icmp6_qtype		icmp6_dataun.u_ni.qtype
#define icmp6_flags		icmp6_dataun.u_ni.flags
#define icmp6_home_agent	icmp6_dataun.u_nd_ra.home_agent
};


#define ICMPV6_DEST_UNREACH		1
#define ICMPV6_PKT_TOOBIG		2
#define ICMPV6_TIME_EXCEED		3
#define ICMPV6_PARAMPROB		4

#define ICMPV6_INFOMSG_MASK		0x80

#define ICMPV6_ECHO_REQUEST		128
#define ICMPV6_ECHO_REPLY		129
#define ICMPV6_MGM_QUERY		130
#define ICMPV6_MGM_REPORT       	131
#define ICMPV6_MGM_REDUCTION    	132

#define ICMPV6_NI_QUERY			139
#define ICMPV6_NI_REPLY			140

/*
 *	Codes for Destination Unreachable
 */
#define ICMPV6_NOROUTE			0
#define ICMPV6_ADM_PROHIBITED		1
#define ICMPV6_NOT_NEIGHBOUR		2
#define ICMPV6_ADDR_UNREACH		3
#define ICMPV6_PORT_UNREACH		4

/*
 *	Codes for Time Exceeded
 */
#define ICMPV6_EXC_HOPLIMIT		0
#define ICMPV6_EXC_FRAGTIME		1

/*
 *	Codes for Parameter Problem
 */
#define ICMPV6_HDR_FIELD		0
#define ICMPV6_UNK_NEXTHDR		1
#define ICMPV6_UNK_OPTION		2

/*
 *	Codes for Node Information
 */
#define ICMPV6_NI_SUBJ_IPV6		0	/* Query Subject is an ipv6 address */
#define ICMPV6_NI_SUBJ_FQDN		1	/* Query Subject is a Domain name */
#define ICMPV6_NI_SUBJ_IPV4		2	/* Query Subject is an ipv4 address */

#define ICMPV6_NI_SUCCESS		0	/* NI successful reply */
#define ICMPV6_NI_REFUSED		1	/* NI request is refused */
#define ICMPV6_NI_UNKNOWN		2	/* unknown Qtype */

#define ICMPV6_NI_QTYPE_NOOP		0 	/* NOOP  */
#define ICMPV6_NI_QTYPE_SUPTYPES	1 	/* Supported Qtypes */
#define ICMPV6_NI_QTYPE_FQDN		2 	/* FQDN */
#define ICMPV6_NI_QTYPE_NODEADDR	3 	/* Node Addresses */
#define ICMPV6_NI_QTYPE_IPV4ADDR	4 	/* IPv4 Addresses */

/* Flags */
#if defined(__BIG_ENDIAN)
#define ICMPV6_NI_SUPTYPE_FLAG_COMPRESS		0x1
#elif defined(__LITTLE_ENDIAN)
#define ICMPV6_NI_SUPTYPE_FLAG_COMPRESS		0x0100
#endif
#if defined(__BIG_ENDIAN)
#define ICMPV6_NI_FQDN_FLAG_VALIDTTL		0x1
#elif defined(__LITTLE_ENDIAN)
#define ICMPV6_NI_FQDN_FLAG_VALIDTTL		0x0100
#endif
#if defined(__BIG_ENDIAN)
#define ICMPV6_NI_NODEADDR_FLAG_TRUNCATE	0x1
#define ICMPV6_NI_NODEADDR_FLAG_ALL		0x2
#define ICMPV6_NI_NODEADDR_FLAG_COMPAT		0x4
#define ICMPV6_NI_NODEADDR_FLAG_LINKLOCAL	0x8
#define ICMPV6_NI_NODEADDR_FLAG_SITELOCAL	0x10
#define ICMPV6_NI_NODEADDR_FLAG_GLOBAL		0x20
#define ICMPV6_NI_NODEADDR_FLAG_ANYCAST		0x40 /* just experimental. not in spec */
#elif defined(__LITTLE_ENDIAN)
#define ICMPV6_NI_NODEADDR_FLAG_TRUNCATE	0x0100
#define ICMPV6_NI_NODEADDR_FLAG_ALL		0x0200
#define ICMPV6_NI_NODEADDR_FLAG_COMPAT		0x0400
#define ICMPV6_NI_NODEADDR_FLAG_LINKLOCAL	0x0800
#define ICMPV6_NI_NODEADDR_FLAG_SITELOCAL	0x1000
#define ICMPV6_NI_NODEADDR_FLAG_GLOBAL		0x2000
#define ICMPV6_NI_NODEADDR_FLAG_ANYCAST		0x4000 /* just experimental. not in spec */
#else
#error	"Please fix <asm/byteorder.h>"
#endif
#define ICMPV6_NI_IPV4ADDR_FLAG_TRUNCATE	ICMPV6_NI_NODEADDR_FLAG_TRUNCATE
#define ICMPV6_NI_IPV4ADDR_FLAG_ALL		ICMPV6_NI_NODEADDR_FLAG_ALL


/*
 *	constants for (set|get)sockopt
 */

#define ICMPV6_FILTER			1

/*
 *	ICMPV6 filter
 */

#define ICMPV6_FILTER_BLOCK		1
#define ICMPV6_FILTER_PASS		2
#define ICMPV6_FILTER_BLOCKOTHERS	3
#define ICMPV6_FILTER_PASSONLY		4

struct icmp6_filter {
	__u32		data[8];
};

#ifdef __KERNEL__

#include <linux/netdevice.h>
#include <linux/skbuff.h>

#define IM_ICMPV6_SEND	"icmpv6_send"

extern void				icmpv6_send(struct sk_buff *skb,
						    int type, int code,
						    __u32 info, 
						    struct net_device *dev);

extern int				icmpv6_init(struct net_proto_family *ops);
extern int				icmpv6_err_convert(int type, int code,
							   int *err);
extern void				icmpv6_cleanup(void);
extern void				icmpv6_param_prob(struct sk_buff *skb,
							  int code, int pos);
#endif

#endif
