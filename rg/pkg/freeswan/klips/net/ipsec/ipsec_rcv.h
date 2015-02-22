/*
 * 
 * Copyright (C) 1996, 1997  John Ioannidis.
 * Copyright (C) 1998, 1999, 2000, 2001  Richard Guy Briggs.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#define DB_RX_PKTRX	0x0001
#define DB_RX_PKTRX2	0x0002
#define DB_RX_DMP	0x0004
#define DB_RX_TDB	0x0010
#define DB_RX_XF	0x0020
#define DB_RX_IPAD	0x0040
#define DB_RX_INAU	0x0080
#define DB_RX_OINFO	0x0100
#define DB_RX_OINFO2	0x0200
#define DB_RX_OH	0x0400
#define DB_RX_REPLAY	0x0800

#ifdef __KERNEL__
/* struct options; */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/config.h>	/* for CONFIG_IP_FORWARD */
#include <linux/version.h>
#include <freeswan.h>

extern int _ipsec_rcv(struct sk_buff *skb, struct sock *sk);

extern int
#ifdef PROTO_HANDLER_SINGLE_PARM
ipsec_rcv(struct sk_buff *skb);
#else /* PROTO_HANDLER_SINGLE_PARM */
ipsec_rcv(struct sk_buff *skb,
#ifdef NET_21
	  unsigned short xlen);
#else /* NET_21 */
	  struct net_device *dev,
	  struct options *opt, 
	  __u32 daddr,
	  unsigned short len,
	  __u32 saddr,
	  int redo,
	  struct inet_protocol *protocol);
#endif /* NET_21 */
#endif

extern int klips26_rcv_encap(struct sk_buff *skb, __u16 encap_type);

#ifdef CONFIG_IPSEC_DEBUG
extern int debug_rcv;
#endif /* CONFIG_IPSEC_DEBUG */
extern int sysctl_ipsec_inbound_policy_check;

struct ipsec_rcv_state {
    struct sk_buff *skb;
    struct net_device_stats *stats;
    struct ipsec_sa *ipsp;         /* current SA being processed */
    int len;                       /* length of packet */
    int ilen;                      /* length of inner payload (-authlen) */
    int hard_header_len;           /* layer 2 size */
    int iphlen;                    /* how big is IP header */
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
    __u8  natt_type;
    __u16 natt_sport;
    __u16 natt_dport;
    int   natt_len; 
#endif
};

#endif /* __KERNEL__ */

