/*
 * IPSEC Tunneling code. Heavily based on drivers/net/new_tunnel.c
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
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/config.h>	/* for CONFIG_IP_FORWARD */
#include <linux/version.h>
#include <linux/kernel.h> /* printk() */

/* XXX-mcr remove this definition when the code has been properly rototiled */
#define IPSEC_KLIPS1_COMPAT 1
#include "ipsec_param.h"

#ifdef MALLOC_SLAB
# include <linux/slab.h> /* kmalloc() */
#else /* MALLOC_SLAB */
# include <linux/malloc.h> /* kmalloc() */
#endif /* MALLOC_SLAB */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/interrupt.h> /* mark_bh */

#include <linux/netdevice.h>   /* struct net_device, struct net_device_stats, dev_queue_xmit() and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <net/route.h>         /* arp_broken_ops */
#include <net/arp.h>           /* struct rtable */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/udp.h>         /* struct udphdr */
#include <linux/skbuff.h>
#include <freeswan.h>
#ifdef NET_21
# define MSS_HACK_		/* experimental */
# include <asm/uaccess.h>
# include <linux/in6.h>
# define ip_chk_addr inet_addr_type
# define IS_MYADDR RTN_LOCAL
# include <net/dst.h>
# undef dev_kfree_skb
# define dev_kfree_skb(a,b) kfree_skb(a)
# define proto_priv cb
# define PHYSDEV_TYPE
#endif /* NET_21 */
#include <asm/checksum.h>
#include <net/icmp.h>		/* icmp_send() */
#include <net/ip.h>
#ifdef NETDEV_23
# include <linux/netfilter_ipv4.h>
#endif /* NETDEV_23 */

#include "ipsec_alg.h"

#include <linux/if_arp.h>
#ifdef MSS_HACK
# include <net/tcp.h>		/* TCP options */
#endif	/* MSS_HACK */

#include "radij.h"
#include "ipsec_life.h"
#include "ipsec_xform.h"
#include "ipsec_eroute.h"
#include "ipsec_encap.h"
#include "ipsec_radij.h"
#include "ipsec_netlink.h"
#include "ipsec_sa.h"
#include "ipsec_tunnel.h"
#include "ipsec_ipe4.h"
#include "ipsec_ah.h"
#include "ipsec_esp.h"
#include "ipsec_tunnel_common.h"

#ifdef CONFIG_IPSEC_IPCOMP
# include "ipcomp.h"
#endif /* CONFIG_IPSEC_IPCOMP */

#include <pfkeyv2.h>
#include <pfkey.h>
#include <linux/if_ipsec.h>
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
#include <linux/udp.h>
#endif

#include "ipsec_proto.h"

#include "ipsec_log.h"

static __u32 zeroes[64];

#ifdef CONFIG_IPSEC_DEBUG
int debug_tunnel = 0;
int sysctl_ipsec_debug_verbose = 0;
#endif /* CONFIG_IPSEC_DEBUG */

int sysctl_ipsec_icmp = 0;
int sysctl_ipsec_tos = 0;

static int add_ipsec_dev(char *name);
static int del_ipsec_dev(char *name);

/*
 * If the IP packet (iph) is a carrying TCP/UDP, then set the encaps
 * source and destination ports to those from the TCP/UDP header.
 */
void extract_ports(struct iphdr * iph, struct sockaddr_encap * er)
{
	struct udphdr *udp;

	switch (iph->protocol) {
	case IPPROTO_UDP:
	case IPPROTO_TCP:
		/*
		 * The ports are at the same offsets in a TCP and UDP
		 * header so hack it ...
		 */
		udp = (struct udphdr*)(((char*)iph)+(iph->ihl<<2));
		er->sen_sport = udp->source;
		er->sen_dport = udp->dest;
		break;
	default:
		er->sen_sport = 0;
		er->sen_dport = 0;
		break;
	}
}

/*
 * A TRAP eroute is installed and we want to replace it with a HOLD
 * eroute.
 */
static int create_hold_eroute(struct sk_buff * skb, struct iphdr * iph,
			      uint32_t eroute_pid)
{
	struct eroute hold_eroute;
	struct sa_id hold_said;
	struct sk_buff *first, *last;
	int error;

	first = last = NULL;
	memset((caddr_t)&hold_eroute, 0, sizeof(hold_eroute));
	memset((caddr_t)&hold_said, 0, sizeof(hold_said));
	
	hold_said.proto = IPPROTO_INT;
	hold_said.spi = htonl(SPI_HOLD);
	hold_said.dst.s_addr = INADDR_ANY;
	
	hold_eroute.er_eaddr.sen_len = sizeof(struct sockaddr_encap);
	hold_eroute.er_emask.sen_len = sizeof(struct sockaddr_encap);
	hold_eroute.er_erange_to.sen_len = sizeof(struct sockaddr_encap);
	hold_eroute.er_eaddr.sen_family = AF_ENCAP;
	hold_eroute.er_emask.sen_family = AF_ENCAP;
	hold_eroute.er_erange_to.sen_family = AF_ENCAP;
	hold_eroute.er_eaddr.sen_type = SENT_IP4;
	hold_eroute.er_emask.sen_type = 255;
	hold_eroute.er_erange_to.sen_type = SENT_IP4;
	
	hold_eroute.er_eaddr.sen_ip_src.s_addr = iph->saddr;
	hold_eroute.er_eaddr.sen_ip_dst.s_addr = iph->daddr;
	hold_eroute.er_emask.sen_ip_src.s_addr = INADDR_BROADCAST;
	hold_eroute.er_emask.sen_ip_dst.s_addr = INADDR_BROADCAST;
	hold_eroute.er_emask.sen_sport = ~0;
	hold_eroute.er_emask.sen_dport = ~0;
	hold_eroute.er_pid = eroute_pid;
	hold_eroute.er_count = 0;
	hold_eroute.er_lasttime = jiffies/HZ;
						
	hold_eroute.er_eaddr.sen_proto = iph->protocol;
	extract_ports(iph, &hold_eroute.er_eaddr);

#ifdef CONFIG_IPSEC_DEBUG
	if (debug_pfkey) {
		char buf1[64], buf2[64];
		subnettoa(hold_eroute.er_eaddr.sen_ip_src,
			  hold_eroute.er_emask.sen_ip_src, 0, buf1, sizeof(buf1));
		subnettoa(hold_eroute.er_eaddr.sen_ip_dst,
			  hold_eroute.er_emask.sen_ip_dst, 0, buf2, sizeof(buf2));
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "calling breakeroute and makeroute for %s:%d->%s:%d %d HOLD eroute.\n",
			    buf1, ntohs(hold_eroute.er_eaddr.sen_sport),
			    buf2, ntohs(hold_eroute.er_eaddr.sen_dport),
			    hold_eroute.er_eaddr.sen_proto);
	}
#endif /* CONFIG_IPSEC_DEBUG */

	if (ipsec_breakroute(&(hold_eroute.er_eaddr), &(hold_eroute.er_emask),
			     &hold_eroute.er_erange_to, &first, &last)) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "HOLD breakeroute found nothing.\n");
	} else {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "HOLD breakroute deleted %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u %u\n",
			    NIPQUAD(hold_eroute.er_eaddr.sen_ip_src),
			    ntohs(hold_eroute.er_eaddr.sen_sport),
			    NIPQUAD(hold_eroute.er_eaddr.sen_ip_dst),
			    ntohs(hold_eroute.er_eaddr.sen_dport),
			    hold_eroute.er_eaddr.sen_proto);
	}
	if (first != NULL)
		kfree_skb(first);
	if (last != NULL)
		kfree_skb(last);

	error = ipsec_makeroute(&(hold_eroute.er_eaddr),
				&(hold_eroute.er_emask),
				&hold_eroute.er_erange_to,
				hold_said, eroute_pid, skb, NULL, NULL);
	if (error) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "HOLD makeroute returned %d, failed.\n", error);
	} else {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "HOLD makeroute call successful.\n");
	}
	return (error == 0);
}



#ifdef CONFIG_IPSEC_DEBUG_
DEBUG_NO_STATIC void
dmp(char *s, caddr_t bb, int len)
{
	int i;
	unsigned char *b = bb;
  
	if (debug_tunnel && ipsec_rate_limit()) {
		printk(KERN_INFO "klips_debug:ipsec_tunnel_:dmp: "
		       "at %s, len=%d:",
		       s,
		       len);
		for (i=0; i < len; i++) {
			if(!(i%16)){
				printk("\nklips_debug:  ");
			}
			printk(" %02x", *b++);
		}
		printk("\n");
	}
}
#else /* CONFIG_IPSEC_DEBUG */
#define dmp(_x, _y, _z) 
#endif /* CONFIG_IPSEC_DEBUG */

#ifndef SKB_COPY_EXPAND
/*
 *	This is mostly skbuff.c:skb_copy().
 */
struct sk_buff *
skb_copy_expand(struct sk_buff *skb, int headroom, int tailroom, int priority)
{
	struct sk_buff *n;
	unsigned long offset;

	/*
	 *	Do sanity checking
	 */
	if((headroom < 0) || (tailroom < 0) || ((headroom+tailroom) < 0)) {
		ipsec_log(KERN_WARNING
		       "klips_error:skb_copy_expand: "
		       "Illegal negative head,tailroom %d,%d\n",
		       headroom,
		       tailroom);
		return NULL;
	}
	/*
	 *	Allocate the copy buffer
	 */
	 
#ifndef NET_21
	IS_SKB(skb);
#endif /* !NET_21 */


	n=alloc_skb(skb->end - skb->head + headroom + tailroom, priority);

	KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
		    "klips_debug:skb_copy_expand: "
		    "head=%p data=%p tail=%p end=%p end-head=%d tail-data=%d\n",
		    skb->head,
		    skb->data,
		    skb->tail,
		    skb->end,
		    skb->end - skb->head,
		    skb->tail - skb->data);

	if(n==NULL)
		return NULL;

	/*
	 *	Shift between the two data areas in bytes
	 */
	 
	/* offset=n->head-skb->head; */ /* moved down a few lines */

	/* Set the data pointer */
	skb_reserve(n,skb->data-skb->head+headroom);
	/* Set the tail pointer and length */
	if(skb_tailroom(n) < skb->len) {
		ipsec_log(KERN_WARNING "klips_error:skb_copy_expand: "
		       "tried to skb_put %ld, %d available.  This should never happen, please report.\n",
		       (unsigned long int)skb->len,
		       skb_tailroom(n));
		dev_kfree_skb(n, FREE_WRITE);
		return NULL;
	}
	skb_put(n,skb->len);

	offset=n->head + headroom - skb->head;

	/* Copy the bytes */
	memcpy(n->head + headroom, skb->head,skb->end-skb->head);
#ifdef NET_21
	n->csum=skb->csum;
	n->priority=skb->priority;
	n->dst=dst_clone(skb->dst);
	if(skb->nh.raw)
		n->nh.raw=skb->nh.raw+offset;
#ifndef NETDEV_23
	n->is_clone=0;
#endif /* NETDEV_23 */
	atomic_set(&n->users, 1);
	n->destructor = NULL;
#ifdef HAVE_SOCK_SECURITY
	n->security=skb->security;
#endif
#else /* NET_21 */
	n->link3=NULL;
	n->when=skb->when;
	if(skb->ip_hdr)
	        n->ip_hdr=(struct iphdr *)(((char *)skb->ip_hdr)+offset);
	n->saddr=skb->saddr;
	n->daddr=skb->daddr;
	n->raddr=skb->raddr;
	n->seq=skb->seq;
	n->end_seq=skb->end_seq;
	n->ack_seq=skb->ack_seq;
	n->acked=skb->acked;
	n->free=1;
	n->arp=skb->arp;
	n->tries=0;
	n->lock=0;
	n->users=0;
#endif /* NET_21 */
	n->protocol=skb->protocol;
	n->list=NULL;
	n->sk=NULL;
	n->dev=skb->dev;
	if(skb->h.raw)
		n->h.raw=skb->h.raw+offset;
	if(skb->mac.raw) 
		n->mac.raw=skb->mac.raw+offset;
	memcpy(n->proto_priv, skb->proto_priv, sizeof(skb->proto_priv));
#ifndef NETDEV_23
	n->used=skb->used;
#endif /* !NETDEV_23 */
	n->pkt_type=skb->pkt_type;
	n->stamp=skb->stamp;
	
#ifndef NET_21
	IS_SKB(n);
#endif /* !NET_21 */
	return n;
}
#endif /* !SKB_COPY_EXPAND */

#ifdef CONFIG_IPSEC_DEBUG
void
ipsec_print_ip(struct iphdr *ip)
{
	char buf[ADDRTOA_BUF];

	printk(KERN_INFO "klips_debug:   IP:");
	printk(" ihl:%d", ip->ihl*4);
	printk(" ver:%d", ip->version);
	printk(" tos:%d", ip->tos);
	printk(" tlen:%d", ntohs(ip->tot_len));
	printk(" id:%d", ntohs(ip->id));
	printk(" %s%s%sfrag_off:%d",
               ip->frag_off & __constant_htons(IP_CE) ? "CE " : "",
               ip->frag_off & __constant_htons(IP_DF) ? "DF " : "",
               ip->frag_off & __constant_htons(IP_MF) ? "MF " : "",
               (ntohs(ip->frag_off) & IP_OFFSET) << 3);
	printk(" ttl:%d", ip->ttl);
	printk(" proto:%d", ip->protocol);
	if(ip->protocol == IPPROTO_UDP)
		printk(" (UDP)");
	if(ip->protocol == IPPROTO_TCP)
		printk(" (TCP)");
	if(ip->protocol == IPPROTO_ICMP)
		printk(" (ICMP)");
	printk(" chk:%d", ntohs(ip->check));
	addrtoa(*((struct in_addr*)(&ip->saddr)), 0, buf, sizeof(buf));
	printk(" saddr:%s", buf);
	if(ip->protocol == IPPROTO_UDP)
		printk(":%d",
		       ntohs(((struct udphdr*)((caddr_t)ip + (ip->ihl << 2)))->source));
	if(ip->protocol == IPPROTO_TCP)
		printk(":%d",
		       ntohs(((struct tcphdr*)((caddr_t)ip + (ip->ihl << 2)))->source));
	addrtoa(*((struct in_addr*)(&ip->daddr)), 0, buf, sizeof(buf));
	printk(" daddr:%s", buf);
	if(ip->protocol == IPPROTO_UDP)
		printk(":%d",
		       ntohs(((struct udphdr*)((caddr_t)ip + (ip->ihl << 2)))->dest));
	if(ip->protocol == IPPROTO_TCP)
		printk(":%d",
		       ntohs(((struct tcphdr*)((caddr_t)ip + (ip->ihl << 2)))->dest));
	if(ip->protocol == IPPROTO_ICMP)
		printk(" type:code=%d:%d",
		       ((struct icmphdr*)((caddr_t)ip + (ip->ihl << 2)))->type,
		       ((struct icmphdr*)((caddr_t)ip + (ip->ihl << 2)))->code);
	printk("\n");

	if(sysctl_ipsec_debug_verbose) {
		__u8 *c;
		int i;
		
		c = ((__u8*)ip) + ip->ihl*4;
		for(i = 0; i < ntohs(ip->tot_len) - ip->ihl*4; i++ /*, c++*/) {
			if(!(i % 16)) {
				printk(KERN_INFO
				       "klips_debug:   @%03x:",
				       i);
			}
			printk(" %02x", /***/c[i]);
			if(!((i + 1) % 16)) {
				printk("\n");
			}
		}
		if(i % 16) {
			printk("\n");
		}
	}
}
#endif /* CONFIG_IPSEC_DEBUG */

#ifdef REAL_LOCKING_P
/*
 *	Locking
 */
 
#if 0
DEBUG_NO_STATIC int
ipsec_tunnel_lock(struct ipsecpriv *prv)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	/*
	 *	Lock in an interrupt may fail
	 */
	if(prv->locked && in_interrupt()) {
		restore_flags(flags);
		return 0;
	}
	while(prv->locked)
		sleep_on(&prv->wait_queue);
	prv->locked=1;
	restore_flags(flags);
	return 1;
}
#endif

#if 0
DEBUG_NO_STATIC void
ipsec_tunnel_unlock(struct ipsecpriv *prv)
{
	prv->locked=0;
	wake_up(&prv->wait_queue);
}
#endif
#endif /* REAL_LOCKING_P */

DEBUG_NO_STATIC int
ipsec_tunnel_open(struct net_device *dev)
{
	struct ipsecpriv *prv = dev->priv;
	
	/*
	 * Can't open until attached.
	 */

	KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
		    "klips_debug:ipsec_tunnel_open: "
		    "dev = %s, prv->dev = %s\n",
		    dev->name, prv->dev?prv->dev->name:"NONE");

	if (prv->dev == NULL)
		return -ENODEV;
	
	KLIPS_INC_USE;
	return 0;
}

DEBUG_NO_STATIC int
ipsec_tunnel_close(struct net_device *dev)
{
	KLIPS_DEC_USE;
	return 0;
}

#ifdef MSS_HACK
/*
 * Issues:
 *  1) Fragments arriving in the tunnel should probably be rejected.
 *  2) How does this affect syncookies, mss_cache, dst cache ?
 *  3) Path MTU discovery handling needs to be reviewed.  For example,
 *     if we receive an ICMP 'packet too big' message from an intermediate 
 *     router specifying it's next hop MTU, our stack may process this and
 *     adjust the MSS without taking our AH/ESP overheads into account.
 */

 
/*
 * Recaclulate checksum using differences between changed datum, 
 * borrowed from netfilter.
 */
DEBUG_NO_STATIC u_int16_t 
ipsec_fast_csum(u_int32_t oldvalinv, u_int32_t newval, u_int16_t oldcheck)
{
	u_int32_t diffs[] = { oldvalinv, newval };
	return csum_fold(csum_partial((char *)diffs, sizeof(diffs),
	oldcheck^0xFFFF));
}

/*
 * Determine effective MSS.
 *
 * Note that we assume that there is always an MSS option for our own
 * SYN segments, which is mentioned in tcp_syn_build_options(), kernel 2.2.x.
 * This could change, and we should probably parse TCP options instead.
 *
 */
DEBUG_NO_STATIC u_int8_t
ipsec_adjust_mss(struct sk_buff *skb, struct tcphdr *tcph, u_int16_t mtu)
{
	u_int16_t oldmss, newmss;
	u_int32_t *mssp;
	struct sock *sk = skb->sk;
	
	newmss = tcp_sync_mss(sk, mtu);
	ipsec_log(KERN_INFO "klips: setting mss to %u\n", newmss);
	mssp = (u_int32_t *)tcph + sizeof(struct tcphdr) / sizeof(u_int32_t);
	oldmss = ntohl(*mssp) & 0x0000FFFF;
	*mssp = htonl((TCPOPT_MSS << 24) | (TCPOLEN_MSS << 16) | newmss);
	tcph->check = ipsec_fast_csum(htons(~oldmss), 
	                              htons(newmss), tcph->check);
	return 1;
}
#endif	/* MSS_HACK */
                                                        
#ifdef NETDEV_23
static inline int ipsec_tunnel_xmit2(struct sk_buff *skb)
{
#ifdef NETDEV_25	/* 2.6 kernels */
	return dst_output(skb);
#else
	return ip_send(skb);
#endif
}
#endif /* NETDEV_23 */

/*
 *	This function assumes it is being called from dev_queue_xmit()
 *	and that skb is filled properly by that function.
 */

int
ipsec_tunnel_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ipsecpriv *prv;		/* Our device' private space */
	struct sk_buff *oskb = NULL;	/* Original skb pointer */
	struct net_device_stats *stats;	/* This device's statistics */
	struct iphdr  *iph;		/* Our new IP header */
	__u32   newdst;			/* The other SG's IP address */
	__u32	orgdst;			/* Original IP destination address */
	__u32	orgedst;		/* 1st SG's IP address */
	__u32   newsrc;			/* The new source SG's IP address */
	__u32	orgsrc;			/* Original IP source address */
	__u32	innersrc;		/* Innermost IP source address */
	int	iphlen;			/* IP header length */
	int	pyldsz;			/* upper protocol payload size */
	int	headroom;
	int	tailroom;
	int     max_headroom = 0;	/* The extra header space needed */
	int	max_tailroom = 0;	/* The extra stuffing needed */
	int     ll_headroom;		/* The extra link layer hard_header space needed */
	int     tot_headroom = 0;	/* The total header space needed */
	int	tot_tailroom = 0;	/* The totalstuffing needed */
	__u8	*saved_header = NULL;	/* saved copy of the hard header */
	int i;
	unsigned short   sport,dport;

	struct sockaddr_encap matcher;	/* eroute search key */
	struct eroute *er;
	struct ipsec_sa *tdbp, *tdbq;	/* Tunnel Descriptor Block pointers */
	char sa[SATOA_BUF];
	size_t sa_len;
	int hard_header_stripped = 0;	/* has the hard header been removed yet? */
	int hard_header_len = 0;
	struct net_device *physdev;
	short physmtu;
	short mtudiff;
	int blocksize = 8;
#ifdef NET_21
	struct rtable *rt = NULL;
#endif /* NET_21 */
	struct sa_id outgoing_said;
#ifdef NET_21
	int pass = 0;
#endif /* NET_21 */
	int error = 0;
	uint32_t eroute_pid = 0;
	struct ipsec_sa tdb;
#ifdef CONFIG_IPSEC_ALG
	struct ipsec_alg_enc *ixt_e = NULL;
	struct ipsec_alg_auth *ixt_a = NULL;
#endif /* CONFIG_IPSEC_ALG */
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
	uint8_t natt_type = 0;
	uint8_t natt_head = 0;
	uint16_t natt_sport = 0;
	uint16_t natt_dport = 0;
#endif

	dport=sport=0;

	memset((char*)&tdb, 0, sizeof(struct ipsec_sa));

	/*
	 *	Return if there is nothing to do.  (Does this ever happen?) XXX
	 */
	if (skb == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
			    "klips_error:ipsec_tunnel_start_xmit: "
			    "Nothing to do!\n" );
		goto cleanup;
	}
	if (dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
			    "klips_error:ipsec_tunnel_start_xmit: "
			    "No device associated with skb!\n" );
		goto cleanup;
	}

	prv = dev->priv;
	if (prv == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
			    "klips_error:ipsec_tunnel_start_xmit: "
			    "Device has no private structure!\n" );
		goto cleanup;
	}

	physdev = prv->dev;
	if (physdev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
			    "klips_error:ipsec_tunnel_start_xmit: "
			    "Device is not attached to physical device!\n" );
		goto cleanup;
	}

	physmtu = physdev->mtu;

	stats = (struct net_device_stats *) &(prv->mystats);

#ifdef NET_21
	/* if skb was cloned (most likely due to a packet sniffer such as
	   tcpdump being momentarily attached to the interface), make
	   a copy of our own to modify */
	if(skb_cloned(skb)) {
		if
#ifdef SKB_COW_NEW
	       (skb_cow(skb, skb_headroom(skb)) != 0)
#else /* SKB_COW_NEW */
	       ((skb = skb_cow(skb, skb_headroom(skb))) == NULL)
#endif /* SKB_COW_NEW */
		{
			KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
				    "klips_error:ipsec_tunnel_start_xmit: "
				    "skb_cow failed to allocate buffer, dropping.\n" );
			stats->tx_dropped++;
			goto cleanup;
		}
	}
#endif /* NET_21 */

#ifdef NET_21
	iph = skb->nh.iph;
#else /* NET_21 */
	iph = skb->ip_hdr;
#endif /* NET_21 */

	/* sanity check for IP version as we can't handle IPv6 right now */
	if (iph->version != 4) {
		KLIPS_PRINT(debug_tunnel,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "found IP Version %d but cannot process other IP versions than v4.\n",
			    iph->version); /* XXX */
		stats->tx_dropped++;
		goto cleanup;
	}
	
	/* physdev->hard_header_len is unreliable and should not be used */
	hard_header_len = (unsigned char *)iph - skb->data;

	if(hard_header_len < 0) {
		KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
			    "klips_error:ipsec_tunnel_start_xmit: "
			    "Negative hard_header_len (%d)?!\n", hard_header_len);
		stats->tx_dropped++;
		goto cleanup;
	}

	if(hard_header_len == 0) { /* no hard header present */
		hard_header_stripped = 1;
	}

#ifdef CONFIG_IPSEC_DEBUG
	if ((debug_tunnel & DB_TN_XMIT) && ipsec_rate_limit()) {
		int i;
		char c;
		
		printk(KERN_INFO "klips_debug:ipsec_tunnel_start_xmit: "
		       ">>> skb->len=%ld hard_header_len:%d",
		       (unsigned long int)skb->len, hard_header_len);
		c = ' ';
		for (i=0; i < hard_header_len; i++) {
			printk("%c%02x", c, skb->data[i]);
			c = ':';
		}
		printk(" \n");
	}
#endif /* CONFIG_IPSEC_DEBUG */

	KLIPS_IP_PRINT(debug_tunnel & DB_TN_XMIT, iph);

	/*
	 * Sanity checks
	 */

	if ((iph->ihl << 2) != sizeof (struct iphdr)) {
		KLIPS_PRINT(debug_tunnel,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "cannot process IP header options yet.  May be mal-formed packet.\n"); /* XXX */
		stats->tx_dropped++;
		goto cleanup;
	}
	
#ifndef NET_21
	/* TTL decrement code (on the way out!) borrowed from ip_forward.c */
	if(0) {
		unsigned long checksum = iph->check;
		iph->ttl--;
	/*
	 *	Re-compute the IP header checksum.
	 *	This is efficient. We know what has happened to the header
	 *	and can thus adjust the checksum as Phil Karn does in KA9Q
	 *	except we do this in "network byte order".
	 */
		checksum += htons(0x0100);
		/* carry overflow? */
		checksum += checksum >> 16;
		iph->check = checksum;
	}
	if (iph->ttl <= 0) {
		/* Tell the sender its packet died... */
		ICMP_SEND(skb, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, 0, physdev);

		KLIPS_PRINT(debug_tunnel, "klips_debug:ipsec_tunnel_start_xmit: "
			    "TTL=0, too many hops!\n");
		stats->tx_dropped++;
		goto cleanup;
	}
#endif /* !NET_21 */

	/*
	 * First things first -- look us up in the erouting tables.
	 */
	matcher.sen_len = sizeof (struct sockaddr_encap);
	matcher.sen_family = AF_ENCAP;
	matcher.sen_type = SENT_IP4;
	matcher.sen_ip_src.s_addr = iph->saddr;
	matcher.sen_ip_dst.s_addr = iph->daddr;
	matcher.sen_proto = iph->protocol;
	extract_ports(iph, &matcher);

	/*
	 * The spinlock is to prevent any other process from accessing or deleting
	 * the eroute while we are using and updating it.
	 */
	spin_lock(&eroute_lock);
	
	er = ipsec_findroute(&matcher);

	if(iph->protocol == IPPROTO_UDP) {
		if(skb->sk) {
#ifdef NET_26
#ifdef HAVE_INET_SK_SPORT
                        sport = ntohs(inet_sk(skb->sk)->sport);
                        dport = ntohs(inet_sk(skb->sk)->dport);
#else
			struct udp_sock *us;
			
			us = (struct udp_sock *)skb->sk;

			sport = ntohs(us->inet.sport);
			dport = ntohs(us->inet.dport);
#endif
#else
			sport=ntohs(skb->sk->sport);
			dport=ntohs(skb->sk->dport);
#endif
		} else if((ntohs(iph->frag_off) & IP_OFFSET) == 0 &&
			  iph->ihl << 2 > sizeof(struct iphdr) + sizeof(struct udphdr)) {
			sport=ntohs(((struct udphdr*)((caddr_t)iph+(iph->ihl<<2)))->source);
			dport=ntohs(((struct udphdr*)((caddr_t)iph + (iph->ihl<<2)))->dest);
		} else {
			sport=0; dport=0;
		}
	}

	/* default to a %drop eroute */
	outgoing_said.proto = IPPROTO_INT;
	outgoing_said.spi = htonl(SPI_DROP);
	outgoing_said.dst.s_addr = INADDR_ANY;
	KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "checking for local udp/500 IKE packet "
		    "saddr=%x, er=%p, daddr=%x, er_dst=%x, proto=%d sport=%d dport=%d\n",
		    ntohl((unsigned int)iph->saddr),
		    er,
		    ntohl((unsigned int)iph->daddr),
		    er ? ntohl((unsigned int)er->er_said.dst.s_addr) : 0,
		    iph->protocol,
		    sport,
		    dport); 

	/*
	 * Quick cheat for now...are we udp/500? If so, let it through
	 * without interference since it is most likely an IKE packet.
	 */

	if (ip_chk_addr((unsigned long)iph->saddr) == IS_MYADDR
	    && (!er
		|| iph->daddr == er->er_said.dst.s_addr
		|| INADDR_ANY == er->er_said.dst.s_addr)
	    && (iph->protocol == IPPROTO_UDP &&
	       (sport == 500 || sport == 4500))) {
		/* Whatever the eroute, this is an IKE message
		 * from us (i.e. not being forwarded).
		 * Furthermore, if there is a tunnel eroute,
		 * the destination is the peer for this eroute.
		 * So %pass the packet: modify the default %drop.
		 */
		outgoing_said.spi = htonl(SPI_PASS);
		if(!(skb->sk) && ((ntohs(iph->frag_off) & IP_MF) != 0)) {
			KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "local UDP/500 (probably IKE) passthrough: base fragment, rest of fragments will probably get filtered.\n");
		}
	} else if (er) {
		er->er_count++;
		er->er_lasttime = jiffies/HZ;
		if(er->er_said.proto==IPPROTO_INT
		   && er->er_said.spi==htonl(SPI_HOLD)) {
			KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "shunt SA of HOLD: skb stored in HOLD.\n");
			if(er->er_last != NULL) {
				kfree_skb(er->er_last);
			}
			er->er_last = skb;
			skb = NULL;
			stats->tx_dropped++;
			spin_unlock(&eroute_lock);
			goto cleanup;
		}
		outgoing_said = er->er_said;
		eroute_pid = er->er_pid;
		/* Copy of the ident for the TRAP/TRAPSUBNET eroutes */
		if(outgoing_said.proto==IPPROTO_INT
		   && (outgoing_said.spi==htonl(SPI_TRAP)
		       || (outgoing_said.spi==htonl(SPI_TRAPSUBNET)))) {
			int len;
			
			tdb.tdb_ident_s.type = er->er_ident_s.type;
			tdb.tdb_ident_s.id = er->er_ident_s.id;
			tdb.tdb_ident_s.len = er->er_ident_s.len;
			if (tdb.tdb_ident_s.len) {
				len = tdb.tdb_ident_s.len * IPSEC_PFKEYv2_ALIGN - sizeof(struct sadb_ident);
				if ((tdb.tdb_ident_s.data = kmalloc(len, GFP_ATOMIC)) == NULL) {
					ipsec_log(KERN_WARNING "klips_debug:ipsec_tunnel_start_xmit: "
					       "Failed, tried to allocate %d bytes for source ident.\n", 
					       len);
					stats->tx_dropped++;
					spin_unlock(&eroute_lock);
					goto cleanup;
				}
				memcpy(tdb.tdb_ident_s.data, er->er_ident_s.data, len);
			}
			tdb.tdb_ident_d.type = er->er_ident_d.type;
			tdb.tdb_ident_d.id = er->er_ident_d.id;
			tdb.tdb_ident_d.len = er->er_ident_d.len;
			if (tdb.tdb_ident_d.len) {
				len = tdb.tdb_ident_d.len * IPSEC_PFKEYv2_ALIGN - sizeof(struct sadb_ident);
				if ((tdb.tdb_ident_d.data = kmalloc(len, GFP_ATOMIC)) == NULL) {
					ipsec_log(KERN_WARNING "klips_debug:ipsec_tunnel_start_xmit: "
					       "Failed, tried to allocate %d bytes for dest ident.\n", 
					       len);
					stats->tx_dropped++;
					spin_unlock(&eroute_lock);
					goto cleanup;
				}
				memcpy(tdb.tdb_ident_d.data, er->er_ident_d.data, len);
			}
		}
	}

	spin_unlock(&eroute_lock);

	KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "Original head,tailroom: %d,%d\n",
		    skb_headroom(skb), skb_tailroom(skb));

	innersrc = iph->saddr;
	/* start encapsulation loop here XXX */
	do {
		struct ipsec_sa *tdbprev = NULL;

		newdst = orgdst = iph->daddr;
		newsrc = orgsrc = iph->saddr;
		orgedst = outgoing_said.dst.s_addr;
		iphlen = iph->ihl << 2;
		pyldsz = ntohs(iph->tot_len) - iphlen;
		max_headroom = max_tailroom = 0;
		
		if (outgoing_said.proto == IPPROTO_INT) {
			switch (ntohl(outgoing_said.spi)) {
			case SPI_DROP:
				KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
					    "klips_debug:ipsec_tunnel_start_xmit: "
					    "shunt SA of DROP or no eroute: dropping.\n");
				stats->tx_dropped++;
				break;
				
			case SPI_REJECT:
				KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
					    "klips_debug:ipsec_tunnel_start_xmit: "
					    "shunt SA of REJECT: notifying and dropping.\n");
				ICMP_SEND(skb,
					  ICMP_DEST_UNREACH,
					  ICMP_PKT_FILTERED,
					  0,
					  physdev);
				stats->tx_dropped++;
				break;
				
			case SPI_PASS:
#ifdef NET_21
				pass = 1;
#endif /* NET_21 */
				KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
					    "klips_debug:ipsec_tunnel_start_xmit: "
					    "PASS: calling dev_queue_xmit\n");
				goto bypass;
				
			case SPI_HOLD:
				KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
					    "klips_debug:ipsec_tunnel_start_xmit: "
					    "shunt SA of HOLD: this does not make sense here, dropping.\n");
				stats->tx_dropped++;
			break;

			case SPI_TRAP:
			case SPI_TRAPSUBNET:
			{
				struct sockaddr_in src, dst;
#ifdef CONFIG_IPSEC_DEBUG
				char bufsrc[ADDRTOA_BUF], bufdst[ADDRTOA_BUF];
#endif /* CONFIG_IPSEC_DEBUG */
				
				/* Signal all listening KMds with a PF_KEY ACQUIRE */
				tdb.tdb_said.proto = iph->protocol;
				src.sin_family = AF_INET;
				dst.sin_family = AF_INET;
				src.sin_addr.s_addr = iph->saddr;
				dst.sin_addr.s_addr = iph->daddr;
				src.sin_port = 
					(iph->protocol == IPPROTO_UDP
					 ? ((struct udphdr*) (((caddr_t)iph) + (iph->ihl << 2)))->source
					 : (iph->protocol == IPPROTO_TCP
					    ? ((struct tcphdr*)((caddr_t)iph + (iph->ihl << 2)))->source
					    : 0));
				dst.sin_port = 
					(iph->protocol == IPPROTO_UDP
					 ? ((struct udphdr*) (((caddr_t)iph) + (iph->ihl << 2)))->dest
					 : (iph->protocol == IPPROTO_TCP
					    ? ((struct tcphdr*)((caddr_t)iph + (iph->ihl << 2)))->dest
					    : 0));
				for(i = 0;
				    i < sizeof(struct sockaddr_in)
					    - offsetof(struct sockaddr_in, sin_zero);
				    i++) {
					src.sin_zero[i] = 0;
					dst.sin_zero[i] = 0;
				}
				
				tdb.tdb_addr_s = (struct sockaddr*)(&src);
				tdb.tdb_addr_d = (struct sockaddr*)(&dst);
				KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
					    "klips_debug:ipsec_tunnel_start_xmit: "
					    "SADB_ACQUIRE sent with src=%s:%d, dst=%s:%d, proto=%d.\n",
					    addrtoa(((struct sockaddr_in*)(tdb.tdb_addr_s))->sin_addr, 0, bufsrc, sizeof(bufsrc)) <= ADDRTOA_BUF ? bufsrc : "BAD_ADDR",
					    ntohs(((struct sockaddr_in*)(tdb.tdb_addr_s))->sin_port),
					    addrtoa(((struct sockaddr_in*)(tdb.tdb_addr_d))->sin_addr, 0, bufdst, sizeof(bufdst)) <= ADDRTOA_BUF ? bufdst : "BAD_ADDR",
					    ntohs(((struct sockaddr_in*)(tdb.tdb_addr_d))->sin_port),
					    tdb.tdb_said.proto);
				
				if (pfkey_acquire(&tdb) == 0) {
					
					if (outgoing_said.spi==htonl(SPI_TRAPSUBNET)) {
						/*
						 * The spinlock is to prevent any other
						 * process from accessing or deleting
						 * the eroute while we are using and
						 * updating it.
						 */
						spin_lock(&eroute_lock);
						er = ipsec_findroute(&matcher);
						if(er) {
							er->er_said.spi = htonl(SPI_HOLD);
							er->er_first = skb;
							skb = NULL;
						}
						spin_unlock(&eroute_lock);
					} else if (create_hold_eroute(skb, iph, eroute_pid)) {
						skb = NULL;
					}
				}
				stats->tx_dropped++;
			}
			default: ;
				/* XXX what do we do with an unknown shunt spi? */
			} /* switch (ntohl(outgoing_said.spi)) */
			goto cleanup;
		} /* if (outgoing_said.proto == IPPROTO_INT) */
		
		/*
		  The spinlock is to prevent any other process from
		  accessing or deleting the TDB hash table or any of the
		  TDBs while we are using and updating them.
		  
		  This is not optimal, but was relatively straightforward
		  at the time.  A better way to do it has been planned for
		  more than a year, to lock the hash table and put reference
		  counts on each TDB instead.  This is not likely to happen
		  in KLIPS1 unless a volunteer contributes it, but will be
		  designed into KLIPS2.
		*/
		spin_lock(&tdb_lock);

		tdbp = ipsec_sa_getbyid(&outgoing_said);
		sa_len = satoa(outgoing_said, 0, sa, SATOA_BUF);

		if (tdbp == NULL) {
			spin_unlock(&tdb_lock);
			KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "no Tunnel Descriptor Block for SA%s: outgoing packet with no SA, dropped.\n",
				    sa_len ? sa : " (error)");
			stats->tx_dropped++;
			goto cleanup;
		}
		
		KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "found Tunnel Descriptor Block -- SA:<%s%s%s> %s\n",
			    IPS_XFORM_NAME(tdbp),
			    sa_len ? sa : " (error)");
		
		/*
		 * How much headroom do we need to be able to apply
		 * all the grouped transforms?
		 */
		tdbq = tdbp;	/* save the head of the tdb chain */
		while (tdbp)	{
			sa_len = satoa(tdbp->tdb_said, 0, sa, SATOA_BUF);
			if(sa_len == 0) {
				strcpy(sa, "(error)");
			}

			/* If it is in larval state, drop the packet, we cannot process yet. */
			if(tdbp->tdb_state == SADB_SASTATE_LARVAL) {
				KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
					    "klips_debug:ipsec_tunnel_start_xmit: "
					    "TDB in larval state for SA:<%s%s%s> %s, cannot be used yet, dropping packet.\n",
					    IPS_XFORM_NAME(tdbp),
					    sa_len ? sa : " (error)");
				spin_unlock(&tdb_lock);
				stats->tx_errors++;
				goto cleanup;
			}

			if(tdbp->tdb_state == SADB_SASTATE_DEAD) {
				KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
					    "klips_debug:ipsec_tunnel_start_xmit: "
					    "TDB in dead state for SA:<%s%s%s> %s, can no longer be used, dropping packet.\n",
					    IPS_XFORM_NAME(tdbp),
					    sa_len ? sa : " (error)");
				spin_unlock(&tdb_lock);
				stats->tx_errors++;
				goto cleanup;
			}

			/* If the replay window counter == -1, expire SA, it will roll */
			if(tdbp->tdb_replaywin && tdbp->tdb_replaywin_lastseq == -1) {
				pfkey_expire(tdbp, 1);
				KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
					    "klips_debug:ipsec_tunnel_start_xmit: "
					    "replay window counter rolled for SA:<%s%s%s> %s, packet dropped, expiring SA.\n",
					    IPS_XFORM_NAME(tdbp),
					    sa_len ? sa : " (error)");
				ipsec_sa_delchain(tdbp);
				spin_unlock(&tdb_lock);
				stats->tx_errors++;
				goto cleanup;
			}

			/*
			 * if this is the first time we are using this SA, mark start time,
			 * and offset hard/soft counters by "now" for later checking.
			 */
#if 0
			if(tdbp->ips_life.ipl_usetime.count == 0) {
				tdbp->ips_life.ipl_usetime.count = jiffies;
				tdbp->ips_life.ipl_usetime.hard += jiffies;
				tdbp->ips_life.ipl_usetime.soft += jiffies;
			}
#endif
			  

			if(ipsec_lifetime_check(&tdbp->ips_life.ipl_bytes, "bytes", sa, 
						ipsec_life_countbased, ipsec_outgoing, tdbp) == ipsec_life_harddied ||
			   ipsec_lifetime_check(&tdbp->ips_life.ipl_addtime, "addtime",sa,
						ipsec_life_timebased,  ipsec_outgoing, tdbp) == ipsec_life_harddied ||
			   ipsec_lifetime_check(&tdbp->ips_life.ipl_usetime, "usetime",sa,
						ipsec_life_timebased,  ipsec_outgoing, tdbp) == ipsec_life_harddied ||
			   ipsec_lifetime_check(&tdbp->ips_life.ipl_packets, "packets",sa,
						ipsec_life_countbased, ipsec_outgoing, tdbp) == ipsec_life_harddied) {
				
				ipsec_sa_delchain(tdbp);
				spin_unlock(&tdb_lock);
				stats->tx_errors++;
				goto cleanup;
			}
			

			headroom = tailroom = 0;
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "calling room for <%s%s%s>, SA:%s\n", 
				    IPS_XFORM_NAME(tdbp),
				    sa_len ? sa : " (error)");
			switch(tdbp->tdb_said.proto) {
#ifdef CONFIG_IPSEC_AH
			case IPPROTO_AH:
				headroom += sizeof(struct ah);
				break;
#endif /* CONFIG_IPSEC_AH */
#ifdef CONFIG_IPSEC_ESP
			case IPPROTO_ESP:
#ifdef CONFIG_IPSEC_ALG
				if ((ixt_e=IPSEC_ALG_SA_ESP_ENC(tdbp))) {
					blocksize = ixt_e->ixt_blocksize;
					headroom += ESP_HEADER_LEN + ixt_e->ixt_ivlen/8;
				} else
#endif /* CONFIG_IPSEC_ALG */
				switch(tdbp->tdb_encalg) {
#ifdef USE_SINGLE_DES
				case ESP_DES:
					headroom += sizeof(struct esp);
					break;
#endif /* USE_SINGLE_DES */
#ifdef CONFIG_IPSEC_ENC_3DES
				case ESP_3DES:
					headroom += sizeof(struct esp);
					break;
#endif /* CONFIG_IPSEC_ENC_3DES */
#ifdef CONFIG_IPSEC_ENC_NULL
				case ESP_NULL:
					headroom += offsetof(struct esp, esp_iv);
					break;
#endif /* CONFIG_IPSEC_ENC_NULL */
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}
#ifdef CONFIG_IPSEC_ALG
				if ((ixt_a=IPSEC_ALG_SA_ESP_AUTH(tdbp))) {
					tailroom += AHHMAC_HASHLEN;
				} else
#endif /* CONFIG_IPSEC_ALG */
				switch(tdbp->tdb_authalg) {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
				case AH_MD5:
					tailroom += AHHMAC_HASHLEN;
					break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_MD5 */
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
				case AH_SHA:
					tailroom += AHHMAC_HASHLEN;
					break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_SHA1 */
				case AH_NONE:
					break;
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}			
				tailroom +=
					blocksize != 1 ?
					((blocksize - ((pyldsz + 2) % blocksize)) % blocksize) + 2 :
					((4 - ((pyldsz + 2) % 4)) % 4) + 2;
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
		if ((tdbp->ips_natt_type) && (!natt_type)) {
			natt_type = tdbp->ips_natt_type;
			natt_sport = tdbp->ips_natt_sport;
			natt_dport = tdbp->ips_natt_dport;
			switch (natt_type) {
				case ESPINUDP_WITH_NON_IKE:
					natt_head = sizeof(struct udphdr)+(2*sizeof(__u32));
					break;
					
				case ESPINUDP_WITH_NON_ESP:
					natt_head = sizeof(struct udphdr);
					break;
					
				default:
				  KLIPS_PRINT(debug_tunnel & DB_TN_CROUT
					      , "klips_xmit: invalid nat-t type %d"
					      , natt_type);
				  stats->tx_errors++;
				  goto cleanup;
					      
					break;
			}
			tailroom += natt_head;
		}
#endif
				break;
#endif /* !CONFIG_IPSEC_ESP */
#ifdef CONFIG_IPSEC_IPIP
			case IPPROTO_IPIP:
				headroom += sizeof(struct iphdr);
				break;
#endif /* !CONFIG_IPSEC_IPIP */
			case IPPROTO_COMP:
#ifdef CONFIG_IPSEC_IPCOMP
				/*
				  We can't predict how much the packet will
				  shrink without doing the actual compression.
				  We could do it here, if we were the first
				  encapsulation in the chain.  That might save
				  us a skb_copy_expand, since we might fit
				  into the existing skb then.  However, this
				  would be a bit unclean (and this hack has
				  bit us once), so we better not do it. After
				  all, the skb_copy_expand is cheap in
				  comparison to the actual compression.
				  At least we know the packet will not grow.
				*/
				break;
#endif /* CONFIG_IPSEC_IPCOMP */
			default:
				spin_unlock(&tdb_lock);
				stats->tx_errors++;
				goto cleanup;
			}
			tdbp = tdbp->tdb_onext;
#ifdef CONFIG_IPSEC_ALG
			ixt_e = NULL;	/* invalidate ipsec_alg */
			ixt_a = NULL;
#endif /* CONFIG_IPSEC_ALG */
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "Required head,tailroom: %d,%d\n", 
				    headroom, tailroom);
			max_headroom += headroom;
			max_tailroom += tailroom;
			pyldsz += (headroom + tailroom);
		}
		tdbp = tdbq;	/* restore the head of the tdb chain */
		
		KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "existing head,tailroom: %d,%d before applying xforms with head,tailroom: %d,%d .\n",
			    skb_headroom(skb), skb_tailroom(skb),
			    max_headroom, max_tailroom);
		
		tot_headroom += max_headroom;
		tot_tailroom += max_tailroom;
		
		mtudiff = prv->mtu + tot_headroom + tot_tailroom - physmtu;

		KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "mtu:%d physmtu:%d tothr:%d tottr:%d mtudiff:%d ippkttotlen:%d\n",
			    prv->mtu, physmtu,
			    tot_headroom, tot_tailroom, mtudiff, ntohs(iph->tot_len));
		if(mtudiff > 0) {
			int newmtu = physmtu - (tot_headroom + ((tot_tailroom + 2) & ~7) + 5);

			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_info:ipsec_tunnel_start_xmit: "
				    "dev %s mtu of %d decreased by %d to %d\n",
				    dev->name,
				    prv->mtu,
				    prv->mtu - newmtu,
				    newmtu);
			prv->mtu = newmtu;
#ifdef NET_21
#if 0
			skb->dst->pmtu = prv->mtu; /* RGB */
#endif /* 0 */
#else /* NET_21 */
#if 0
			dev->mtu = prv->mtu; /* RGB */
#endif /* 0 */
#endif /* NET_21 */
		}

		/* 
		   If the sender is doing PMTU discovery, and the
		   packet doesn't fit within prv->mtu, notify him
		   (unless it was an ICMP packet, or it was not the
		   zero-offset packet) and send it anyways.

		   Note: buggy firewall configuration may prevent the
		   ICMP packet from getting back.
		*/
		if(sysctl_ipsec_icmp
		   && prv->mtu < ntohs(iph->tot_len)
		   && (iph->frag_off & __constant_htons(IP_DF)) ) {
			int notify = iph->protocol != IPPROTO_ICMP
				&& (iph->frag_off & __constant_htons(IP_OFFSET)) == 0;
			
#ifdef IPSEC_obey_DF
			spin_unlock(&tdb_lock);
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "fragmentation needed and DF set; %sdropping packet\n",
				    notify ? "sending ICMP and " : "");
			if (notify)
				ICMP_SEND(skb,
					  ICMP_DEST_UNREACH,
					  ICMP_FRAG_NEEDED,
					  prv->mtu,
					  physdev);
			stats->tx_errors++;
			goto cleanup;
#else /* IPSEC_obey_DF */
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "fragmentation needed and DF set; %spassing packet\n",
				    notify ? "sending ICMP and " : "");
			if (notify)
				ICMP_SEND(skb,
					  ICMP_DEST_UNREACH,
					  ICMP_FRAG_NEEDED,
					  prv->mtu,
					  physdev);
#endif /* IPSEC_obey_DF */
		}
		
#ifdef MSS_HACK
		/*
		 * If this is a transport mode TCP packet with
		 * SYN set, determine an effective MSS based on 
		 * AH/ESP overheads determined above.
		 */
		if (iph->protocol == IPPROTO_TCP 
		    && outgoing_said.proto != IPPROTO_IPIP) {
			struct tcphdr *tcph = skb->h.th;
			if (tcph->syn && !tcph->ack) {
				if(!ipsec_adjust_mss(skb, tcph, prv->mtu)) {
					spin_unlock(&tdb_lock);
					ipsec_log(KERN_WARNING
					       "klips_warning:ipsec_tunnel_start_xmit: "
					       "ipsec_adjust_mss() failed\n");
					stats->tx_errors++;
					goto cleanup;
				}
			}
		}
#endif /* MSS_HACK */

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
		if ((natt_type) && (outgoing_said.proto != IPPROTO_IPIP)) {
		        ipsec_tunnel_correct_tcp_udp_csum(skb, iph, tdbp);
		}
#endif

		if(!hard_header_stripped) {
			if((saved_header = kmalloc(hard_header_len, GFP_ATOMIC)) == NULL) {
				spin_unlock(&tdb_lock);
				ipsec_log(KERN_WARNING "klips_debug:ipsec_tunnel_start_xmit: "
				       "Failed, tried to allocate %d bytes for temp hard_header.\n", 
				       hard_header_len);
				stats->tx_errors++;
				goto cleanup;
			}
			for (i = 0; i < hard_header_len; i++) {
				saved_header[i] = skb->data[i];
			}
			if(skb->len < hard_header_len) {
				spin_unlock(&tdb_lock);
				ipsec_log(KERN_WARNING "klips_error:ipsec_tunnel_start_xmit: "
				       "tried to skb_pull hhlen=%d, %d available.  This should never happen, please report.\n",
				       hard_header_len, (int)(skb->len));
				stats->tx_errors++;
				goto cleanup;
			}
			skb_pull(skb, hard_header_len);
			hard_header_stripped = 1;
			
/*			iph = (struct iphdr *) (skb->data); */
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "head,tailroom: %d,%d after hard_header stripped.\n",
				    skb_headroom(skb), skb_tailroom(skb));
			KLIPS_IP_PRINT(debug_tunnel & DB_TN_CROUT, iph);
		} else {
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "hard header already stripped.\n");
		}
		
		ll_headroom = (hard_header_len + 15) & ~15;

		if ((skb_headroom(skb) >= max_headroom + 2 * ll_headroom) && 
		    (skb_tailroom(skb) >= max_tailroom)
#ifndef NET_21
			&& skb->free
#endif /* !NET_21 */
			) {
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "data fits in existing skb\n");
		} else {
			struct sk_buff* tskb = skb;

			if(!oskb) {
				oskb = skb;
			}

			tskb = skb_copy_expand(skb,
			/* The reason for 2 * link layer length here still baffles me...RGB */
					       max_headroom + 2 * ll_headroom,
					       max_tailroom,
					       GFP_ATOMIC);
#ifdef NET_21
			if(tskb && skb->sk) {
				skb_set_owner_w(tskb, skb->sk);
			}
#endif /* NET_21 */
			if(!(skb == oskb) ) {
				dev_kfree_skb(skb, FREE_WRITE);
			}
			skb = tskb;
			if (!skb) {
				spin_unlock(&tdb_lock);
				ipsec_log(KERN_WARNING
				       "klips_debug:ipsec_tunnel_start_xmit: "
				       "Failed, tried to allocate %d head and %d tailroom\n", 
				       max_headroom, max_tailroom);
				stats->tx_errors++;
				goto cleanup;
			}
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "head,tailroom: %d,%d after allocation\n",
				    skb_headroom(skb), skb_tailroom(skb));
		}
		
		/*
		 * Apply grouped transforms to packet
		 */
		while (tdbp) {
#ifdef CONFIG_IPSEC_ESP
			struct esp *espp;
#ifdef CONFIG_IPSEC_ENC_3DES
			__u32 iv[ESP_IV_MAXSZ_INT];
#endif /* !CONFIG_IPSEC_ENC_3DES */
			unsigned char *idat, *pad;
			int authlen = 0, padlen = 0, i;
#endif /* !CONFIG_IPSEC_ESP */
#ifdef CONFIG_IPSEC_AH
			struct iphdr ipo;
			struct ah *ahp;
#endif /* CONFIG_IPSEC_AH */
#if defined(CONFIG_IPSEC_AUTH_HMAC_MD5) || defined(CONFIG_IPSEC_AUTH_HMAC_SHA1)
			union {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
				MD5_CTX md5;
#endif /* CONFIG_IPSEC_AUTH_HMAC_MD5 */
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
				SHA1_CTX sha1;
#endif /* CONFIG_IPSEC_AUTH_HMAC_SHA1 */
			} tctx;
			__u8 hash[AH_AMAX];
#endif /* defined(CONFIG_IPSEC_AUTH_HMAC_MD5) || defined(CONFIG_IPSEC_AUTH_HMAC_SHA1) */
			int headroom = 0, tailroom = 0, ilen = 0, len = 0;
			unsigned char *dat;
			
			iphlen = iph->ihl << 2;
			pyldsz = ntohs(iph->tot_len) - iphlen;
			sa_len = satoa(tdbp->tdb_said, 0, sa, SATOA_BUF);
			KLIPS_PRINT(debug_tunnel & DB_TN_OXFS,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "calling output for <%s%s%s>, SA:%s\n", 
				    IPS_XFORM_NAME(tdbp),
				    sa_len ? sa : " (error)");
			
			switch(tdbp->tdb_said.proto) {
#ifdef CONFIG_IPSEC_AH
			case IPPROTO_AH:
				headroom += sizeof(struct ah);
				break;
#endif /* CONFIG_IPSEC_AH */
#ifdef CONFIG_IPSEC_ESP
			case IPPROTO_ESP:
#ifdef CONFIG_IPSEC_ALG
				if ((ixt_e=IPSEC_ALG_SA_ESP_ENC(tdbp))) {
					blocksize = ixt_e->ixt_blocksize;
					headroom += ESP_HEADER_LEN + ixt_e->ixt_ivlen/8;
				} else
#endif /* CONFIG_IPSEC_ALG */
				switch(tdbp->tdb_encalg) {
#ifdef USE_SINGLE_DES
				case ESP_DES:
					headroom += sizeof(struct esp);
					break;
#endif /* USE_SINGLE_DES */
#ifdef CONFIG_IPSEC_ENC_3DES
				case ESP_3DES:
					headroom += sizeof(struct esp);
					break;
#endif /* CONFIG_IPSEC_ENC_3DES */
#ifdef CONFIG_IPSEC_ENC_NULL
				case ESP_NULL:
					headroom += offsetof(struct esp, esp_iv);
					break;
#endif /* CONFIG_IPSEC_ENC_NULL */
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}
#ifdef CONFIG_IPSEC_ALG
				if ((ixt_a=IPSEC_ALG_SA_ESP_AUTH(tdbp))) {
					authlen = AHHMAC_HASHLEN;
				} else
#endif /* CONFIG_IPSEC_ALG */
				switch(tdbp->tdb_authalg) {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
				case AH_MD5:
					authlen = AHHMAC_HASHLEN;
					break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_MD5 */
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
				case AH_SHA:
					authlen = AHHMAC_HASHLEN;
					break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_SHA1 */
				case AH_NONE:
					break;
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}		
				tailroom +=
					blocksize != 1 ?
					((blocksize - ((pyldsz + 2) % blocksize)) % blocksize) + 2 :
					((4 - ((pyldsz + 2) % 4)) % 4) + 2;
				tailroom += authlen;
				break;
#endif /* !CONFIG_IPSEC_ESP */
#ifdef CONFIG_IPSEC_IPIP
			case IPPROTO_IPIP:
				headroom += sizeof(struct iphdr);
				break;
#endif /* !CONFIG_IPSEC_IPIP */
#ifdef CONFIG_IPSEC_IPCOMP
			case IPPROTO_COMP:
				break;
#endif /* CONFIG_IPSEC_IPCOMP */
			default:
				spin_unlock(&tdb_lock);
				stats->tx_errors++;
				goto cleanup;
			}
			
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "pushing %d bytes, putting %d, proto %d.\n", 
				    headroom, tailroom, tdbp->tdb_said.proto);
			if(skb_headroom(skb) < headroom) {
				spin_unlock(&tdb_lock);
				ipsec_log(KERN_WARNING
				       "klips_error:ipsec_tunnel_start_xmit: "
				       "tried to skb_push headroom=%d, %d available.  This should never happen, please report.\n",
				       headroom, skb_headroom(skb));
				stats->tx_errors++;
				goto cleanup;
			}
			dat = skb_push(skb, headroom);
			ilen = skb->len - tailroom;
			if(skb_tailroom(skb) < tailroom) {
				spin_unlock(&tdb_lock);
				ipsec_log(KERN_WARNING
				       "klips_error:ipsec_tunnel_start_xmit: "
				       "tried to skb_put %d, %d available.  This should never happen, please report.\n",
				       tailroom, skb_tailroom(skb));
				stats->tx_errors++;
				goto cleanup;
			}
			skb_put(skb, tailroom);
			KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "head,tailroom: %d,%d before xform.\n",
				    skb_headroom(skb), skb_tailroom(skb));
			len = skb->len;
			if(len > 0xfff0) {
				spin_unlock(&tdb_lock);
				ipsec_log(KERN_WARNING "klips_error:ipsec_tunnel_start_xmit: "
				       "tot_len (%d) > 65520.  This should never happen, please report.\n",
				       len);
				stats->tx_errors++;
				goto cleanup;
			}
			memmove((void *)dat, (void *)(dat + headroom), iphlen);
			iph = (struct iphdr *)dat;
			iph->tot_len = htons(skb->len);
			
			switch(tdbp->tdb_said.proto) {
#ifdef CONFIG_IPSEC_ESP
			case IPPROTO_ESP:
				espp = (struct esp *)(dat + iphlen);
				espp->esp_spi = tdbp->tdb_said.spi;
				espp->esp_rpl = htonl(++(tdbp->tdb_replaywin_lastseq));
				
#ifdef CONFIG_IPSEC_ALG
				if (!ixt_e)
#endif /* CONFIG_IPSEC_ALG */
				switch(tdbp->tdb_encalg) {
#if defined(USE_SINGLE_DES) || defined(CONFIG_IPSEC_ENC_3DES)
#ifdef USE_SINGLE_DES
				case ESP_DES:
#endif /* USE_SINGLE_DES */
#ifdef CONFIG_IPSEC_ENC_3DES
				case ESP_3DES:
#endif /* CONFIG_IPSEC_ENC_3DES */
					iv[0] = *((__u32*)&(espp->esp_iv)    ) =
						((__u32*)(tdbp->tdb_iv))[0];
					iv[1] = *((__u32*)&(espp->esp_iv) + 1) =
						((__u32*)(tdbp->tdb_iv))[1];
					break;
#endif /* USE_SINGLE_DES || CONFIG_IPSEC_ENC_3DES */
#ifdef CONFIG_IPSEC_ENC_NULL
				case ESP_NULL:
					break;
#endif /* CONFIG_IPSEC_ENC_NULL */
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}
				
				idat = dat + iphlen + headroom;
				ilen = len - (iphlen + headroom + authlen);
				
				/* Self-describing padding */
				pad = &dat[len - tailroom];
				padlen = tailroom - 2 - authlen;
				for (i = 0; i < padlen; i++) {
					pad[i] = i + 1; 
				}
				dat[len - authlen - 2] = padlen;
				
				dat[len - authlen - 1] = iph->protocol;
				iph->protocol = IPPROTO_ESP;
				
#ifdef CONFIG_IPSEC_ALG
				/* Do all operations here:
				 * copy IV->ESP, encrypt, update ips IV
				 */
				if (ixt_e) {
					int ret;
					memcpy(espp->esp_iv, 
						tdbp->ips_iv, 
						ixt_e->ixt_ivlen/8);
					ret=ipsec_alg_esp_encrypt(tdbp, 
						idat, ilen, espp->esp_iv,
						IPSEC_ALG_ENCRYPT);
					memcpy(tdbp->ips_iv,
						idat + ilen - ixt_e->ixt_ivlen/8,
						ixt_e->ixt_ivlen/8);
				} else
#endif /* CONFIG_IPSEC_ALG */
				switch(tdbp->tdb_encalg) {
#ifdef USE_SINGLE_DES
				case ESP_DES:
					des_cbc_encrypt(idat, idat, ilen,
					    (caddr_t)tdbp->tdb_key_e,
					    (caddr_t)iv, 1);
					break;
#endif /* USE_SINGLE_DES */
#ifdef CONFIG_IPSEC_ENC_3DES
				case ESP_3DES:
					des_ede3_cbc_encrypt((des_cblock *)idat,
							     (des_cblock *)idat,
							     ilen,
							     ((struct des_eks *)(tdbp->tdb_key_e))[0].ks,
							     ((struct des_eks *)(tdbp->tdb_key_e))[1].ks,
							     ((struct des_eks *)(tdbp->tdb_key_e))[2].ks,
							     (des_cblock *)iv, 1);
					break;
#endif /* CONFIG_IPSEC_ENC_3DES */
#ifdef CONFIG_IPSEC_ENC_NULL
				case ESP_NULL:
					break;
#endif /* CONFIG_IPSEC_ENC_NULL */
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}
#ifdef CONFIG_IPSEC_ALG
				if (!ixt_e)
#endif /* CONFIG_IPSEC_ALG */
				
				switch(tdbp->tdb_encalg) {
#if defined(USE_SINGLE_DES) || defined(CONFIG_IPSEC_ENC_3DES)
#ifdef USE_SINGLE_DES
				case ESP_DES:
#endif /* USE_SINGLE_DES */
#ifdef CONFIG_IPSEC_ENC_3DES
				case ESP_3DES:
#endif /* CONFIG_IPSEC_ENC_3DES */
					/* XXX update IV with the last 8 octets of the encryption */
					((__u32*)(tdbp->tdb_iv))[0] =
						((__u32 *)(idat))[(ilen >> 2) - 2];
					((__u32*)(tdbp->tdb_iv))[1] =
						((__u32 *)(idat))[(ilen >> 2) - 1];
					break;
#endif /* USE_SINGLE_DES || CONFIG_IPSEC_ENC_3DES */
#ifdef CONFIG_IPSEC_ENC_NULL
				case ESP_NULL:
					break;
#endif /* CONFIG_IPSEC_ENC_NULL */
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}
#ifdef CONFIG_IPSEC_ALG
				if (ixt_a) {
					ipsec_alg_sa_esp_hash(tdbp,
					(caddr_t)espp, len - iphlen - authlen,
					&(dat[len - authlen]), authlen);

				} else
#endif /* CONFIG_IPSEC_ALG */
				
				switch(tdbp->tdb_authalg) {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
				case AH_MD5:
					dmp("espp", (char*)espp, len - iphlen - authlen);
					tctx.md5 = ((struct md5_ctx*)(tdbp->tdb_key_a))->ictx;
					dmp("ictx", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Update(&tctx.md5, (caddr_t)espp, len - iphlen - authlen);
					dmp("ictx+dat", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Final(hash, &tctx.md5);
					dmp("ictx hash", (char*)&hash, sizeof(hash));
					tctx.md5 = ((struct md5_ctx*)(tdbp->tdb_key_a))->octx;
					dmp("octx", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Update(&tctx.md5, hash, AHMD596_ALEN);
					dmp("octx+hash", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Final(hash, &tctx.md5);
					dmp("octx hash", (char*)&hash, sizeof(hash));
					memcpy(&(dat[len - authlen]), hash, authlen);

					/* paranoid */
					memset((caddr_t)&tctx.md5, 0, sizeof(tctx.md5));
					memset((caddr_t)hash, 0, sizeof(*hash));
					break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_MD5 */
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
				case AH_SHA:
					tctx.sha1 = ((struct sha1_ctx*)(tdbp->tdb_key_a))->ictx;
					SHA1Update(&tctx.sha1, (caddr_t)espp, len - iphlen - authlen);
					SHA1Final(hash, &tctx.sha1);
					tctx.sha1 = ((struct sha1_ctx*)(tdbp->tdb_key_a))->octx;
					SHA1Update(&tctx.sha1, hash, AHSHA196_ALEN);
					SHA1Final(hash, &tctx.sha1);
					memcpy(&(dat[len - authlen]), hash, authlen);
					
					/* paranoid */
					memset((caddr_t)&tctx.sha1, 0, sizeof(tctx.sha1));
					memset((caddr_t)hash, 0, sizeof(*hash));
					break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_SHA1 */
				case AH_NONE:
					break;
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}
#ifdef NET_21
				skb->h.raw = (unsigned char*)espp;
#endif /* NET_21 */
				break;
#endif /* !CONFIG_IPSEC_ESP */
#ifdef CONFIG_IPSEC_AH
			case IPPROTO_AH:
				ahp = (struct ah *)(dat + iphlen);
				ahp->ah_spi = tdbp->tdb_said.spi;
				ahp->ah_rpl = htonl(++(tdbp->tdb_replaywin_lastseq));
				ahp->ah_rv = 0;
				ahp->ah_nh = iph->protocol;
				ahp->ah_hl = (headroom >> 2) - sizeof(__u64)/sizeof(__u32);
				iph->protocol = IPPROTO_AH;
				dmp("ahp", (char*)ahp, sizeof(*ahp));
				
				ipo = *iph;
				ipo.tos = 0;
				ipo.frag_off = 0;
				ipo.ttl = 0;
				ipo.check = 0;
				dmp("ipo", (char*)&ipo, sizeof(ipo));
				
				switch(tdbp->tdb_authalg) {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
				case AH_MD5:
					tctx.md5 = ((struct md5_ctx*)(tdbp->tdb_key_a))->ictx;
					dmp("ictx", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Update(&tctx.md5, (unsigned char *)&ipo, sizeof (struct iphdr));
					dmp("ictx+ipo", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Update(&tctx.md5, (unsigned char *)ahp, headroom - sizeof(ahp->ah_data));
					dmp("ictx+ahp", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Update(&tctx.md5, (unsigned char *)zeroes, AHHMAC_HASHLEN);
					dmp("ictx+zeroes", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Update(&tctx.md5,  dat + iphlen + headroom, len - iphlen - headroom);
					dmp("ictx+dat", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Final(hash, &tctx.md5);
					dmp("ictx hash", (char*)&hash, sizeof(hash));
					tctx.md5 = ((struct md5_ctx*)(tdbp->tdb_key_a))->octx;
					dmp("octx", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Update(&tctx.md5, hash, AHMD596_ALEN);
					dmp("octx+hash", (char*)&tctx.md5, sizeof(tctx.md5));
					MD5Final(hash, &tctx.md5);
					dmp("octx hash", (char*)&hash, sizeof(hash));
					
					memcpy(ahp->ah_data, hash, AHHMAC_HASHLEN);
					
					/* paranoid */
					memset((caddr_t)&tctx.md5, 0, sizeof(tctx.md5));
					memset((caddr_t)hash, 0, sizeof(hash));
					break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_MD5 */
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
				case AH_SHA:
					tctx.sha1 = ((struct sha1_ctx*)(tdbp->tdb_key_a))->ictx;
					SHA1Update(&tctx.sha1, (unsigned char *)&ipo, sizeof (struct iphdr));
					SHA1Update(&tctx.sha1, (unsigned char *)ahp, headroom - sizeof(ahp->ah_data));
					SHA1Update(&tctx.sha1, (unsigned char *)zeroes, AHHMAC_HASHLEN);
					SHA1Update(&tctx.sha1,  dat + iphlen + headroom, len - iphlen - headroom);
					SHA1Final(hash, &tctx.sha1);
					tctx.sha1 = ((struct sha1_ctx*)(tdbp->tdb_key_a))->octx;
					SHA1Update(&tctx.sha1, hash, AHSHA196_ALEN);
					SHA1Final(hash, &tctx.sha1);
					
					memcpy(ahp->ah_data, hash, AHHMAC_HASHLEN);
					
					/* paranoid */
					memset((caddr_t)&tctx.sha1, 0, sizeof(tctx.sha1));
					memset((caddr_t)hash, 0, sizeof(hash));
					break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_SHA1 */
				default:
					spin_unlock(&tdb_lock);
					stats->tx_errors++;
					goto cleanup;
				}
#ifdef NET_21
				skb->h.raw = (unsigned char*)ahp;
#endif /* NET_21 */
				break;
#endif /* CONFIG_IPSEC_AH */
#ifdef CONFIG_IPSEC_IPIP
			case IPPROTO_IPIP:
				iph->version  = 4;
				switch(sysctl_ipsec_tos) {
				case 0:
#ifdef NET_21
					iph->tos = skb->nh.iph->tos;
#else /* NET_21 */
					iph->tos = skb->ip_hdr->tos;
#endif /* NET_21 */
					break;
				case 1:
					iph->tos = 0;
					break;
				default: ;
				}
#ifdef NET_21
#ifdef NETDEV_23
				iph->ttl      = sysctl_ip_default_ttl;
#else
				iph->ttl      = ip_statistics.IpDefaultTTL;
#endif
#else /* NET_21 */
				iph->ttl      = 64; /* ip_statistics.IpDefaultTTL; */
#endif /* NET_21 */
				iph->frag_off = 0;
				iph->saddr    = ((struct sockaddr_in*)(tdbp->tdb_addr_s))->sin_addr.s_addr;
				iph->daddr    = ((struct sockaddr_in*)(tdbp->tdb_addr_d))->sin_addr.s_addr;
				iph->protocol = IPPROTO_IPIP;
				iph->ihl      = sizeof(struct iphdr) >> 2 /* 5 */;
#ifdef IP_SELECT_IDENT
				/* XXX use of skb->dst below is a questionable
				   substitute for &rt->u.dst which is only
				   available later-on */
#ifdef IP_SELECT_IDENT_NEW
				ip_select_ident(iph, skb->dst, NULL);
#else /* IP_SELECT_IDENT_NEW */
                                ip_select_ident(iph, skb->dst);
#endif /* IP_SELECT_IDENT_NEW */
#else /* IP_SELECT_IDENT */
				iph->id       = htons(ip_id_count++);   /* Race condition here? */
#endif /* IP_SELECT_IDENT */

				newdst = (__u32)iph->daddr;
				newsrc = (__u32)iph->saddr;
		
#ifdef NET_21
				skb->h.ipiph = skb->nh.iph;
#endif /* NET_21 */
				break;
#endif /* !CONFIG_IPSEC_IPIP */
#ifdef CONFIG_IPSEC_IPCOMP
			case IPPROTO_COMP:
				{
					unsigned int flags = 0;
#ifdef CONFIG_IPSEC_DEBUG
					unsigned int old_tot_len = ntohs(iph->tot_len);
#endif /* CONFIG_IPSEC_DEBUG */
					tdbp->tdb_comp_ratio_dbytes += ntohs(iph->tot_len);

					skb = skb_compress(skb, tdbp, &flags);

#ifdef NET_21
					iph = skb->nh.iph;
#else /* NET_21 */
					iph = skb->ip_hdr;
#endif /* NET_21 */

					tdbp->tdb_comp_ratio_cbytes += ntohs(iph->tot_len);

#ifdef CONFIG_IPSEC_DEBUG
					if (debug_tunnel & DB_TN_CROUT)
					{
						if (old_tot_len > ntohs(iph->tot_len))
							KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
								    "klips_debug:ipsec_tunnel_start_xmit: "
								    "packet shrunk from %d to %d bytes after compression, cpi=%04x (should be from spi=%08x, spi&0xffff=%04x.\n",
								    old_tot_len, ntohs(iph->tot_len),
								    ntohs(((struct ipcomphdr*)(((char*)iph) + ((iph->ihl) << 2)))->ipcomp_cpi),
								    ntohl(tdbp->tdb_said.spi),
								    (__u16)(ntohl(tdbp->tdb_said.spi) & 0x0000ffff));
						else
							KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
								    "klips_debug:ipsec_tunnel_start_xmit: "
								    "packet did not compress (flags = %d).\n",
								    flags);
					}
#endif /* CONFIG_IPSEC_DEBUG */
				}
				break;
#endif /* CONFIG_IPSEC_IPCOMP */
			default:
				spin_unlock(&tdb_lock);
				stats->tx_errors++;
				goto cleanup;
			}
			
#ifdef NET_21
			skb->nh.raw = skb->data;
#else /* NET_21 */
			skb->ip_hdr = skb->h.iph = (struct iphdr *) skb->data;
#endif /* NET_21 */
			iph->check = 0;
			iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
			
			KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
				    "klips_debug:ipsec_tunnel_start_xmit: "
				    "after <%s%s%s>, SA:%s:\n",
				    IPS_XFORM_NAME(tdbp),
				    sa_len ? sa : " (error)");
			KLIPS_IP_PRINT(debug_tunnel & DB_TN_XMIT, iph);
 			
			tdbp->ips_life.ipl_bytes.ipl_count += len;
			tdbp->ips_life.ipl_bytes.ipl_last = len;

			if(!tdbp->ips_life.ipl_usetime.ipl_count) {
				tdbp->ips_life.ipl_usetime.ipl_count = jiffies / HZ;
			}
			tdbp->ips_life.ipl_usetime.ipl_last = jiffies / HZ;
			tdbp->ips_life.ipl_packets.ipl_count++; 

			tdbprev = tdbp;
			tdbp = tdbp->ips_onext;
#ifdef CONFIG_IPSEC_ALG
			ixt_e = NULL;	/* invalidate ipsec_alg */
			ixt_a = NULL;
#endif /* CONFIG_IPSEC_ALG */
			
		}
		/* end encapsulation loop here XXX */

		spin_unlock(&tdb_lock);

		matcher.sen_ip_src.s_addr = iph->saddr;
		matcher.sen_ip_dst.s_addr = iph->daddr;
		matcher.sen_proto = iph->protocol;
		extract_ports(iph, &matcher);
		
		spin_lock(&eroute_lock);
		er = ipsec_findroute(&matcher);
		if(er) {
			outgoing_said = er->er_said;
			eroute_pid = er->er_pid;
			er->er_count++;
			er->er_lasttime = jiffies/HZ;
		}
		spin_unlock(&eroute_lock);
		KLIPS_PRINT((debug_tunnel & DB_TN_XMIT) &&
			    /* ((orgdst != newdst) || (orgsrc != newsrc)) */
			    (orgedst != outgoing_said.dst.s_addr) &&
			    outgoing_said.dst.s_addr &&
			    er,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "We are recursing here.\n");
	} while(/*((orgdst != newdst) || (orgsrc != newsrc))*/
		(orgedst != outgoing_said.dst.s_addr) &&
		outgoing_said.dst.s_addr &&
		er);
	
	KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "After recursive xforms -- head,tailroom: %d,%d\n",
		    skb_headroom(skb), skb_tailroom(skb));

	if(saved_header) {
		if(skb_headroom(skb) < hard_header_len) {
			ipsec_log(KERN_WARNING
			       "klips_error:ipsec_tunnel_start_xmit: "
			       "tried to skb_push hhlen=%d, %d available.  This should never happen, please report.\n",
			       hard_header_len, skb_headroom(skb));
			stats->tx_errors++;
			goto cleanup;
		}
		skb_push(skb, hard_header_len);
		for (i = 0; i < hard_header_len; i++) {
			skb->data[i] = saved_header[i];
		}
	}
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
	if (natt_type && natt_head && ipsec_tunnel_udp_encap(
	    skb, natt_type, natt_head, natt_sport, natt_dport)<0)
	{
	        stats->tx_errors++;
	        goto cleanup;
	}
#endif	

 bypass:
	KLIPS_PRINT(debug_tunnel & DB_TN_CROUT,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "With hard_header, final head,tailroom: %d,%d\n",
		    skb_headroom(skb), skb_tailroom(skb));

#ifdef NET_21	/* 2.2 and 2.4 kernels */
	/* new route/dst cache code from James Morris */
	skb->dev = physdev;
	/*skb_orphan(skb);*/
#ifdef NETDEV_25
	{
	struct flowi fl;

 	fl.oif = physdev->iflink;
 	fl.nl_u.ip4_u.daddr = skb->nh.iph->daddr;
 	fl.nl_u.ip4_u.saddr = pass ? 0 : skb->nh.iph->saddr;
 	fl.nl_u.ip4_u.tos = RT_TOS(skb->nh.iph->tos);
 	fl.proto = skb->nh.iph->protocol;
 	if ((error = ip_route_output_key(&rt, &fl))) {
#else
	if((error = ip_route_output(&rt,
				    skb->nh.iph->daddr,
				    pass ? 0 : skb->nh.iph->saddr,
				    RT_TOS(skb->nh.iph->tos),
				    physdev->iflink /* rgb: should this be 0? */))) {
#endif
		stats->tx_errors++;
		KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "ip_route_output failed with error code %d, rt->u.dst.dev=%s, dropped\n",
			    error,
			    rt->u.dst.dev->name);
		goto cleanup;
	}
#ifdef NETDEV_25
	}
#endif
	if(dev == rt->u.dst.dev) {
		ip_rt_put(rt);
		/* This is recursion, drop it. */
		stats->tx_errors++;
		KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
			    "klips_debug:ipsec_tunnel_start_xmit: "
			    "suspect recursion, dev=rt->u.dst.dev=%s, dropped\n", dev->name);
		goto cleanup;
	}
	dst_release(skb->dst);
	skb->dst = &rt->u.dst;
	stats->tx_bytes += skb->len;
	if(skb->len < skb->nh.raw - skb->data) {
		stats->tx_errors++;
		ipsec_log(KERN_WARNING
		       "klips_error:ipsec_tunnel_start_xmit: "
		       "tried to __skb_pull nh-data=%d, %d available.  This should never happen, please report.\n",
		       skb->nh.raw - skb->data, skb->len);
		goto cleanup;
	}
	__skb_pull(skb, skb->nh.raw - skb->data);
#ifdef SKB_RESET_NFCT
	nf_conntrack_put(skb->nfct);
	skb->nfct = NULL;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug = 0;
#endif /* CONFIG_NETFILTER_DEBUG */
#endif /* SKB_RESET_NFCT */
	KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "...done, calling ip_send() on device:%s\n",
		    skb->dev ? skb->dev->name : "NULL");
	KLIPS_IP_PRINT(debug_tunnel & DB_TN_XMIT, skb->nh.iph);
#ifdef NETDEV_23	/* 2.4 kernels */
	{
		int err;

		err = NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->u.dst.dev,
			      ipsec_tunnel_xmit2);
		if(err != NET_XMIT_SUCCESS && err != NET_XMIT_CN) {
			if(net_ratelimit())
				ipsec_log(KERN_ERR
				       "klips_error:ipsec_tunnel_start_xmit: "
				       "ip_send() failed, err=%d\n", 
				       -err);
			stats->tx_errors++;
			stats->tx_aborted_errors++;
			skb = NULL;
			goto cleanup;
		}
	}
#else /* NETDEV_23 */	/* 2.2 kernels */
	ip_send(skb);
#endif /* NETDEV_23 */
#else /* NET_21 */	/* 2.0 kernels */
	skb->arp = 1;
	/* ISDN/ASYNC PPP from Matjaz Godec. */
	/*	skb->protocol = htons(ETH_P_IP); */
	KLIPS_PRINT(debug_tunnel & DB_TN_XMIT,
		    "klips_debug:ipsec_tunnel_start_xmit: "
		    "...done, calling dev_queue_xmit() or ip_fragment().\n");
	IP_SEND(skb, physdev);
#endif /* NET_21 */
	stats->tx_packets++;

	skb = NULL;
 cleanup:
#if defined(HAS_NETIF_QUEUE) || defined (HAVE_NETIF_QUEUE)
	netif_wake_queue(dev);
#else /* defined(HAS_NETIF_QUEUE) || defined (HAVE_NETIF_QUEUE) */
	dev->tbusy = 0;
#endif /* defined(HAS_NETIF_QUEUE) || defined (HAVE_NETIF_QUEUE) */
	if(saved_header) {
		kfree(saved_header);
	}
	if(skb) {
		dev_kfree_skb(skb, FREE_WRITE);
	}
	if(oskb) {
		dev_kfree_skb(oskb, FREE_WRITE);
	}
	if (tdb.tdb_ident_s.data) {
		kfree(tdb.tdb_ident_s.data);
	}
	if (tdb.tdb_ident_d.data) {
		kfree(tdb.tdb_ident_d.data);
	}
	return 0;
}

DEBUG_NO_STATIC struct net_device_stats *
ipsec_tunnel_get_stats(struct net_device *dev)
{
	return &(((struct ipsecpriv *)(dev->priv))->mystats);
}

/*
 * Revectored calls.
 * For each of these calls, a field exists in our private structure.
 */

DEBUG_NO_STATIC int
ipsec_tunnel_hard_header(struct sk_buff *skb, struct net_device *dev,
	unsigned short type, void *daddr, void *saddr, unsigned len)
{
	struct ipsecpriv *prv = dev->priv;
	struct net_device *tmp;
	int ret;
	struct net_device_stats *stats;	/* This device's statistics */
	
	if(skb == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_hard_header: "
			    "no skb...\n");
		return -ENODATA;
	}

	if(dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_hard_header: "
			    "no device...\n");
		return -ENODEV;
	}

	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "klips_debug:ipsec_tunnel_hard_header: "
		    "skb->dev=%s dev=%s.\n",
		    skb->dev ? skb->dev->name : "NULL",
		    dev->name);
	
	if(prv == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_hard_header: "
			    "no private space associated with dev=%s\n",
			    dev->name ? dev->name : "NULL");
		return -ENODEV;
	}

	stats = (struct net_device_stats *) &(prv->mystats);

	if(prv->dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_hard_header: "
			    "no physical device associated with dev=%s\n",
			    dev->name ? dev->name : "NULL");
		stats->tx_dropped++;
		return -ENODEV;
	}

	/* check if we have to send a IPv6 packet. It might be a Router
	   Solicitation, where the building of the packet happens in
	   reverse order:
	   1. ll hdr,
	   2. IPv6 hdr,
	   3. ICMPv6 hdr
	   -> skb->nh.raw is still uninitialized when this function is
	   called!!  If this is no IPv6 packet, we can print debugging
	   messages, otherwise we skip all debugging messages and just
	   build the ll header */
	if(type != ETH_P_IPV6) {
		/* execute this only, if we don't have to build the
		   header for a IPv6 packet */
		if(!prv->hard_header) {
			KLIPS_PRINTMORE_START(debug_tunnel & DB_TN_REVEC,
				    "klips_debug:ipsec_tunnel_hard_header: "
				    "physical device has been detached, packet dropped 0x%p->0x%p len=%d type=%d dev=%s->NULL ",
				    saddr,
				    daddr,
				    len,
				    type,
				    dev->name);
#ifdef NET_21
			KLIPS_PRINTMORE(debug_tunnel & DB_TN_REVEC,
					"ip=%08x->%08x",
					(__u32)ntohl(skb->nh.iph->saddr),
					(__u32)ntohl(skb->nh.iph->daddr) );
#else /* NET_21 */
			KLIPS_PRINTMORE(debug_tunnel & DB_TN_REVEC,
					"ip=%08x->%08x",
					(__u32)ntohl(skb->ip_hdr->saddr),
					(__u32)ntohl(skb->ip_hdr->daddr) );
#endif /* NET_21 */
			KLIPS_PRINTMORE_FINISH(debug_tunnel & DB_TN_REVEC);
			stats->tx_dropped++;
			return -ENODEV;
		}
		
#define da ((struct net_device *)(prv->dev))->dev_addr
		KLIPS_PRINTMORE_START(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_hard_header: "
			    "Revectored 0x%p->0x%p len=%d type=%d dev=%s->%s dev_addr=%02x:%02x:%02x:%02x:%02x:%02x ",
			    saddr,
			    daddr,
			    len,
			    type,
			    dev->name,
			    prv->dev->name,
			    da[0], da[1], da[2], da[3], da[4], da[5]);
#ifdef NET_21
		KLIPS_PRINTMORE(debug_tunnel & DB_TN_REVEC,
			    "ip=%08x->%08x",
			    (__u32)ntohl(skb->nh.iph->saddr),
			    (__u32)ntohl(skb->nh.iph->daddr) );
#else /* NET_21 */
		KLIPS_PRINTMORE(debug_tunnel & DB_TN_REVEC,
			    "ip=%08x->%08x",
			    (__u32)ntohl(skb->ip_hdr->saddr),
			    (__u32)ntohl(skb->ip_hdr->daddr) );
#endif /* NET_21 */
		KLIPS_PRINTMORE_FINISH(debug_tunnel & DB_TN_REVEC);
	} else {
		KLIPS_PRINT(debug_tunnel,
			    "klips_debug:ipsec_tunnel_hard_header: "
			    "is IPv6 packet, skip debugging messages, only revector and build linklocal header.\n");
	}                                                                       
	tmp = skb->dev;
	skb->dev = prv->dev;
	ret = prv->hard_header(skb, prv->dev, type, (void *)daddr, (void *)saddr, len);
	skb->dev = tmp;
	return ret;
}

DEBUG_NO_STATIC int
#ifdef NET_21
ipsec_tunnel_rebuild_header(struct sk_buff *skb)
#else /* NET_21 */
ipsec_tunnel_rebuild_header(void *buff, struct net_device *dev,
			unsigned long raddr, struct sk_buff *skb)
#endif /* NET_21 */
{
	struct ipsecpriv *prv = skb->dev->priv;
	struct net_device *tmp;
	int ret;
	struct net_device_stats *stats;	/* This device's statistics */
	
	if(skb->dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_rebuild_header: "
			    "no device...");
		return -ENODEV;
	}

	if(prv == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_rebuild_header: "
			    "no private space associated with dev=%s",
			    skb->dev->name ? skb->dev->name : "NULL");
		return -ENODEV;
	}

	stats = (struct net_device_stats *) &(prv->mystats);

	if(prv->dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_rebuild_header: "
			    "no physical device associated with dev=%s",
			    skb->dev->name ? skb->dev->name : "NULL");
		stats->tx_dropped++;
		return -ENODEV;
	}

	if(!prv->rebuild_header) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_rebuild_header: "
			    "physical device has been detached, packet dropped skb->dev=%s->NULL ",
			    skb->dev->name);
#ifdef NET_21
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "ip=%08x->%08x\n",
			    (__u32)ntohl(skb->nh.iph->saddr),
			    (__u32)ntohl(skb->nh.iph->daddr) );
#else /* NET_21 */
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "ip=%08x->%08x\n",
			    (__u32)ntohl(skb->ip_hdr->saddr),
			    (__u32)ntohl(skb->ip_hdr->daddr) );
#endif /* NET_21 */
		stats->tx_dropped++;
		return -ENODEV;
	}

	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "klips_debug:ipsec_tunnel: "
		    "Revectored rebuild_header dev=%s->%s ",
		    skb->dev->name, prv->dev->name);
#ifdef NET_21
	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "ip=%08x->%08x\n",
		    (__u32)ntohl(skb->nh.iph->saddr),
		    (__u32)ntohl(skb->nh.iph->daddr) );
#else /* NET_21 */
	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "ip=%08x->%08x\n",
		    (__u32)ntohl(skb->ip_hdr->saddr),
		    (__u32)ntohl(skb->ip_hdr->daddr) );
#endif /* NET_21 */
	tmp = skb->dev;
	skb->dev = prv->dev;
	
#ifdef NET_21
	ret = prv->rebuild_header(skb);
#else /* NET_21 */
	ret = prv->rebuild_header(buff, prv->dev, raddr, skb);
#endif /* NET_21 */
	skb->dev = tmp;
	return ret;
}

DEBUG_NO_STATIC int
ipsec_tunnel_set_mac_address(struct net_device *dev, void *addr)
{
	struct ipsecpriv *prv = dev->priv;
	
	struct net_device_stats *stats;	/* This device's statistics */
	
	if(dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_set_mac_address: "
			    "no device...");
		return -ENODEV;
	}

	if(prv == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_set_mac_address: "
			    "no private space associated with dev=%s",
			    dev->name ? dev->name : "NULL");
		return -ENODEV;
	}

	stats = (struct net_device_stats *) &(prv->mystats);

	if(prv->dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_set_mac_address: "
			    "no physical device associated with dev=%s",
			    dev->name ? dev->name : "NULL");
		stats->tx_dropped++;
		return -ENODEV;
	}

	if(!prv->set_mac_address) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_set_mac_address: "
			    "physical device has been detached, cannot set - skb->dev=%s->NULL\n",
			    dev->name);
		return -ENODEV;
	}

	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "klips_debug:ipsec_tunnel_set_mac_address: "
		    "Revectored dev=%s->%s addr=%p\n",
		    dev->name, prv->dev->name, addr);
	return prv->set_mac_address(prv->dev, addr);

}

#ifndef NET_21
DEBUG_NO_STATIC void
ipsec_tunnel_cache_bind(struct hh_cache **hhp, struct net_device *dev,
				 unsigned short htype, __u32 daddr)
{
	struct ipsecpriv *prv = dev->priv;
	
	struct net_device_stats *stats;	/* This device's statistics */
	
	if(dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_cache_bind: "
			    "no device...");
		return;
	}

	if(prv == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_cache_bind: "
			    "no private space associated with dev=%s",
			    dev->name ? dev->name : "NULL");
		return;
	}

	stats = (struct net_device_stats *) &(prv->mystats);

	if(prv->dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_cache_bind: "
			    "no physical device associated with dev=%s",
			    dev->name ? dev->name : "NULL");
		stats->tx_dropped++;
		return;
	}

	if(!prv->header_cache_bind) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_cache_bind: "
			    "physical device has been detached, cannot set - skb->dev=%s->NULL\n",
			    dev->name);
		stats->tx_dropped++;
		return;
	}

	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "klips_debug:ipsec_tunnel_cache_bind: "
		    "Revectored \n");
	prv->header_cache_bind(hhp, prv->dev, htype, daddr);
	return;
}
#endif /* !NET_21 */


DEBUG_NO_STATIC void
ipsec_tunnel_cache_update(struct hh_cache *hh, struct net_device *dev, unsigned char *  haddr)
{
	struct ipsecpriv *prv = dev->priv;
	
	struct net_device_stats *stats;	/* This device's statistics */
	
	if(dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_cache_update: "
			    "no device...");
		return;
	}

	if(prv == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_cache_update: "
			    "no private space associated with dev=%s",
			    dev->name ? dev->name : "NULL");
		return;
	}

	stats = (struct net_device_stats *) &(prv->mystats);

	if(prv->dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_cache_update: "
			    "no physical device associated with dev=%s",
			    dev->name ? dev->name : "NULL");
		stats->tx_dropped++;
		return;
	}

	if(!prv->header_cache_update) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_cache_update: "
			    "physical device has been detached, cannot set - skb->dev=%s->NULL\n",
			    dev->name);
		return;
	}

	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "klips_debug:ipsec_tunnel: "
		    "Revectored cache_update\n");
	prv->header_cache_update(hh, prv->dev, haddr);
	return;
}

#ifdef NET_21
DEBUG_NO_STATIC int
ipsec_tunnel_neigh_setup(struct neighbour *n)
{
	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "klips_debug:ipsec_tunnel_neigh_setup:\n");

        if (n->nud_state == NUD_NONE) {
                n->ops = &arp_broken_ops;
                n->output = n->ops->output;
        }
        return 0;
}

DEBUG_NO_STATIC int
ipsec_tunnel_neigh_setup_dev(struct net_device *dev, struct neigh_parms *p)
{
	KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
		    "klips_debug:ipsec_tunnel_neigh_setup_dev: "
		    "setting up %s\n",
		    dev ? dev->name : "NULL");

        if (p->tbl->family == AF_INET) {
                p->neigh_setup = ipsec_tunnel_neigh_setup;
                p->ucast_probes = 0;
                p->mcast_probes = 0;
        }
        return 0;
}
#endif /* NET_21 */

/*
 * We call the attach routine to attach another device.
 */

DEBUG_NO_STATIC int
ipsec_tunnel_attach(struct net_device *dev, struct net_device *physdev)
{
        int i;
	struct ipsecpriv *prv = dev->priv;

	if(dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_attach: "
			    "no device...");
		return -ENODEV;
	}

	if(prv == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_attach: "
			    "no private space associated with dev=%s",
			    dev->name ? dev->name : "NULL");
		return -ENODATA;
	}

	prv->dev = physdev;
	prv->hard_start_xmit = physdev->hard_start_xmit;
	prv->get_stats = physdev->get_stats;

	if (physdev->hard_header) {
		prv->hard_header = physdev->hard_header;
		dev->hard_header = ipsec_tunnel_hard_header;
	} else
		dev->hard_header = NULL;
	
	if (physdev->rebuild_header) {
		prv->rebuild_header = physdev->rebuild_header;
		dev->rebuild_header = ipsec_tunnel_rebuild_header;
	} else
		dev->rebuild_header = NULL;
	
	if (physdev->set_mac_address) {
		prv->set_mac_address = physdev->set_mac_address;
		dev->set_mac_address = ipsec_tunnel_set_mac_address;
	} else
		dev->set_mac_address = NULL;
	
#ifndef NET_21
	if (physdev->header_cache_bind) {
		prv->header_cache_bind = physdev->header_cache_bind;
		dev->header_cache_bind = ipsec_tunnel_cache_bind;
	} else
		dev->header_cache_bind = NULL;
#endif /* !NET_21 */

	if (physdev->header_cache_update) {
		prv->header_cache_update = physdev->header_cache_update;
		dev->header_cache_update = ipsec_tunnel_cache_update;
	} else
		dev->header_cache_update = NULL;

	dev->hard_header_len = physdev->hard_header_len;

#ifdef NET_21
/*	prv->neigh_setup        = physdev->neigh_setup; */
	dev->neigh_setup        = ipsec_tunnel_neigh_setup_dev;
#endif /* NET_21 */
	dev->mtu = 16260; /* 0xfff0; */ /* dev->mtu; */
	prv->mtu = physdev->mtu;

#ifdef PHYSDEV_TYPE
	dev->type = physdev->type /* ARPHRD_TUNNEL */;	/* initially */
#endif /*  PHYSDEV_TYPE */

	dev->addr_len = physdev->addr_len;
	for (i=0; i<dev->addr_len; i++) {
		dev->dev_addr[i] = physdev->dev_addr[i];
	}
#ifdef CONFIG_IPSEC_DEBUG
	if((debug_tunnel & DB_TN_INIT) && ipsec_rate_limit()) {
		printk(KERN_INFO "klips_debug:ipsec_tunnel_attach: "
		       "physical device %s being attached has HW address: %2x",
		       physdev->name, physdev->dev_addr[0]);
		for (i=1; i < physdev->addr_len; i++) {
			printk(":%02x", physdev->dev_addr[i]);
		}
		printk("\n");
	}
#endif /* CONFIG_IPSEC_DEBUG */

	return 0;
}

/*
 * We call the detach routine to detach the ipsec tunnel from another device.
 */

DEBUG_NO_STATIC int
ipsec_tunnel_detach(struct net_device *dev)
{
        int i;
	struct ipsecpriv *prv = dev->priv;

	if(dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_detach: "
			    "no device...");
		return -ENODEV;
	}

	if(prv == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_REVEC,
			    "klips_debug:ipsec_tunnel_detach: "
			    "no private space associated with dev=%s",
			    dev->name ? dev->name : "NULL");
		return -ENODATA;
	}

	KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
		    "klips_debug:ipsec_tunnel_detach: "
		    "physical device %s being detached from virtual device %s\n",
		    prv->dev ? prv->dev->name : "NULL",
		    dev->name);

	prv->dev = NULL;
	prv->hard_start_xmit = NULL;
	prv->get_stats = NULL;

	prv->hard_header = NULL;
#ifdef DETACH_AND_DOWN
	dev->hard_header = NULL;
#endif /* DETACH_AND_DOWN */
	
	prv->rebuild_header = NULL;
#ifdef DETACH_AND_DOWN
	dev->rebuild_header = NULL;
#endif /* DETACH_AND_DOWN */
	
	prv->set_mac_address = NULL;
#ifdef DETACH_AND_DOWN
	dev->set_mac_address = NULL;
#endif /* DETACH_AND_DOWN */
	
#ifndef NET_21
	prv->header_cache_bind = NULL;
#ifdef DETACH_AND_DOWN
	dev->header_cache_bind = NULL;
#endif /* DETACH_AND_DOWN */
#endif /* !NET_21 */

	prv->header_cache_update = NULL;
#ifdef DETACH_AND_DOWN
	dev->header_cache_update = NULL;
#endif /* DETACH_AND_DOWN */

#ifdef NET_21
/*	prv->neigh_setup        = NULL; */
#ifdef DETACH_AND_DOWN
	dev->neigh_setup        = NULL;
#endif /* DETACH_AND_DOWN */
#endif /* NET_21 */
	dev->hard_header_len = 0;
#ifdef DETACH_AND_DOWN
	dev->mtu = 0;
#endif /* DETACH_AND_DOWN */
	prv->mtu = 0;
	for (i=0; i<MAX_ADDR_LEN; i++) {
		dev->dev_addr[i] = 0;
	}
	dev->addr_len = 0;
#ifdef PHYSDEV_TYPE
	dev->type = ARPHRD_TUNNEL;
#endif /*  PHYSDEV_TYPE */
	
	return 0;
}

/*
 * We call the clear routine to detach all ipsec tunnels from other devices.
 */
DEBUG_NO_STATIC int
ipsec_tunnel_clear(void)
{
	struct net_device *ipsecdev = NULL, *prvdev;
	struct ipsecpriv *prv;
	int ret;
	ipsec_dev_list *cur = ipsec_dev_head;

	KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
		    "klips_debug:ipsec_tunnel_clear: .\n");

	for(; cur; cur = cur->next) {
		if((ipsecdev = ipsec_dev_get(cur->ipsec_dev->name)) != NULL) {
			if((prv = (struct ipsecpriv *)(ipsecdev->priv))) {
				prvdev = (struct net_device *)(prv->dev);
				if(prvdev) {
					KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
						    "klips_debug:ipsec_tunnel_clear: "
						    "physical device for device %s is %s\n",
						    cur->ipsec_dev->name, prvdev->name);
					if((ret = ipsec_tunnel_detach(ipsecdev))) {
						KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
							    "klips_debug:ipsec_tunnel_clear: "
							    "error %d detatching device %s from device %s.\n",
							    ret, cur->ipsec_dev->name, prvdev->name);
						return ret;
					}
				}
			}
		}
	}
	return 0;
}

DEBUG_NO_STATIC int
ipsec_tunnel_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct ipsectunnelconf *cf = (struct ipsectunnelconf *)&ifr->ifr_data;
	struct ipsecpriv *prv = dev->priv;
	struct net_device *them; /* physical device */
#ifdef CONFIG_IP_ALIAS
	char *colon;
	char realphysname[IFNAMSIZ];
#endif /* CONFIG_IP_ALIAS */
	
	if(dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_tunnel_ioctl: "
			    "device not supplied.\n");
		return -ENODEV;
	}

	KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
		    "klips_debug:ipsec_tunnel_ioctl: "
		    "tncfg service call #%d for dev=%s\n",
		    cmd,
		    dev->name ? dev->name : "NULL");
	switch (cmd) {
	/* attach a virtual ipsec? device to a physical device */
	case IPSEC_SET_DEV:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_tunnel_ioctl: "
			    "calling ipsec_tunnel_attatch...\n");
#ifdef CONFIG_IP_ALIAS
		/* If this is an IP alias interface, get its real physical name */
		strncpy(realphysname, cf->cf_name, IFNAMSIZ);
		realphysname[IFNAMSIZ-1] = 0;
		colon = strchr(realphysname, ':');
		if (colon) *colon = 0;
		them = ipsec_dev_get(realphysname);
#else /* CONFIG_IP_ALIAS */
		them = ipsec_dev_get(cf->cf_name);
#endif /* CONFIG_IP_ALIAS */

		if (them == NULL) {
			KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
				    "klips_debug:ipsec_tunnel_ioctl: "
				    "physical device %s requested is null\n",
				    cf->cf_name);
			return -ENXIO;
		}
		
#if 0
		if (them->flags & IFF_UP) {
			KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
				    "klips_debug:ipsec_tunnel_ioctl: "
				    "physical device %s requested is not up.\n",
				    cf->cf_name);
			return -ENXIO;
		}
#endif
		
		if (prv && prv->dev) {
			KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
				    "klips_debug:ipsec_tunnel_ioctl: "
				    "virtual device is already connected to %s.\n",
				    prv->dev->name ? prv->dev->name : "NULL");
			return -EBUSY;
		}
		return ipsec_tunnel_attach(dev, them);

	case IPSEC_DEL_DEV:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_tunnel_ioctl: "
			    "calling ipsec_tunnel_detatch.\n");
		if (! prv->dev) {
			KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
				    "klips_debug:ipsec_tunnel_ioctl: "
				    "physical device not connected.\n");
			return -ENODEV;
		}
		return ipsec_tunnel_detach(dev);
	       
	case IPSEC_CLR_DEV:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_tunnel_ioctl: "
			    "calling ipsec_tunnel_clear.\n");
		return ipsec_tunnel_clear();

	default:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_tunnel_ioctl: "
			    "unknown command %d.\n",
			    cmd);
		return -EOPNOTSUPP;
	}
}

int
ipsec_device_event(struct notifier_block *unused, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct net_device *ipsec_dev;
	struct ipsecpriv *priv;
	ipsec_dev_list *cur;

	if (dev == NULL) {
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "dev=NULL for event type %ld.\n",
			    event);
		return(NOTIFY_DONE);
	}

	/* check for loopback devices */
	if (dev && (dev->flags & IFF_LOOPBACK)) {
		return(NOTIFY_DONE);
	}

	switch (event) {
	case NETDEV_DOWN:
		/* look very carefully at the scope of these compiler
		   directives before changing anything... -- RGB */
#ifdef NET_21
	case NETDEV_UNREGISTER:
		switch (event) {
		case NETDEV_DOWN:
#endif /* NET_21 */
			KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
				    "klips_debug:ipsec_device_event: "
				    "NETDEV_DOWN dev=%s flags=%x\n",
				    dev->name,
				    dev->flags);
			if(strncmp(dev->name, "ipsec", strlen("ipsec")) == 0) {
				ipsec_log(KERN_DEBUG "IPSEC EVENT: KLIPS device %s shut down.\n",
				       dev->name);
			}
#ifdef NET_21
			break;
		case NETDEV_UNREGISTER:
			KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
				    "klips_debug:ipsec_device_event: "
				    "NETDEV_UNREGISTER dev=%s flags=%x\n",
				    dev->name,
				    dev->flags);
			break;
		}
#endif /* NET_21 */
		
		/* find the attached physical device and detach it. */
		for(cur=ipsec_dev_head; cur; cur=cur->next) {
			ipsec_dev = ipsec_dev_get(cur->ipsec_dev->name);
			if(ipsec_dev) {
				priv = (struct ipsecpriv *)(ipsec_dev->priv);
				if(priv) {
					;
					if(((struct net_device *)(priv->dev)) == dev) {
						/* dev_close(ipsec_dev); */
						/* return */ ipsec_tunnel_detach(ipsec_dev);
						KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
							    "klips_debug:ipsec_device_event: "
							    "device '%s' has been detached.\n",
							    ipsec_dev->name);
						break;
					}
				} else {
					KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
						    "klips_debug:ipsec_device_event: "
						    "device '%s' has no private data space!\n",
						    ipsec_dev->name);
				}
			}
		}
		break;
	case NETDEV_UP:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "NETDEV_UP dev=%s\n",
			    dev->name);
		break;
#ifdef NET_21
	case NETDEV_REBOOT:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "NETDEV_REBOOT dev=%s\n",
			    dev->name);
		break;
	case NETDEV_CHANGE:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "NETDEV_CHANGE dev=%s flags=%x\n",
			    dev->name,
			    dev->flags);
		break;
	case NETDEV_REGISTER:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "NETDEV_REGISTER dev=%s\n",
			    dev->name);
		break;
	case NETDEV_CHANGEMTU:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "NETDEV_CHANGEMTU dev=%s to mtu=%d\n",
			    dev->name,
			    dev->mtu);
		break;
	case NETDEV_CHANGEADDR:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "NETDEV_CHANGEADDR dev=%s\n",
			    dev->name);
		break;
	case NETDEV_GOING_DOWN:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "NETDEV_GOING_DOWN dev=%s\n",
			    dev->name);
		break;
	case NETDEV_CHANGENAME:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "NETDEV_CHANGENAME dev=%s\n",
			    dev->name);
		break;
#endif /* NET_21 */
	default:
		KLIPS_PRINT(debug_tunnel & DB_TN_INIT,
			    "klips_debug:ipsec_device_event: "
			    "event type %ld unrecognised for dev=%s\n",
			    event,
			    dev->name);
		break;
	}
	return NOTIFY_DONE;
}

/*
 *	Called when an ipsec tunnel device is initialized.
 *	The ipsec tunnel device structure is passed to us.
 */
 
int
ipsec_tunnel_init(struct net_device *dev)
{
	int i;

	/* Add our tunnel functions to the device */
	dev->open		= ipsec_tunnel_open;
	dev->stop		= ipsec_tunnel_close;
	dev->hard_start_xmit	= ipsec_tunnel_start_xmit;
	dev->get_stats		= ipsec_tunnel_get_stats;

	dev->priv = kmalloc(sizeof(struct ipsecpriv), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0, sizeof(struct ipsecpriv));

	for(i = 0; i < sizeof(zeroes); i++) {
		((__u8*)(zeroes))[i] = 0;
	}
	
#ifndef NET_21
	/* Initialize the tunnel device structure */
	for (i = 0; i < DEV_NUMBUFFS; i++)
		skb_queue_head_init(&dev->buffs[i]);
#endif /* !NET_21 */

	dev->set_multicast_list = NULL;
	dev->do_ioctl		= ipsec_tunnel_ioctl;
	dev->hard_header	= NULL;
	dev->rebuild_header 	= NULL;
	dev->set_mac_address 	= NULL;
#ifndef NET_21
	dev->header_cache_bind 	= NULL;
#endif /* !NET_21 */
	dev->header_cache_update= NULL;

#ifdef NET_21
/*	prv->neigh_setup        = NULL; */
	dev->neigh_setup        = ipsec_tunnel_neigh_setup_dev;
#endif /* NET_21 */
	dev->hard_header_len 	= 0;
	dev->mtu		= 0;
	dev->addr_len		= 0;
	dev->type		= ARPHRD_TUNNEL; /* 0 */ /* ARPHRD_ETHER; */ /* initially */
	dev->tx_queue_len	= 10;		/* Small queue */
	memset(dev->broadcast,0xFF, ETH_ALEN);	/* what if this is not attached to ethernet? */

	/* New-style flags. */
	dev->flags		= IFF_NOARP /* 0 */ /* Petr Novak */;

	/* We're done.  Have I forgotten anything? */
	return 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*  Module specific interface (but it links with the rest of IPSEC  */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int
ipsec_tunnel_probe(struct net_device *dev)
{
	ipsec_tunnel_init(dev); 
	return 0;
}

int ipsec_ioctl_handle(unsigned long arg)
{
	struct ipsec_ioctl_args args;

	/* everything here needs root permissions, except aguably the
	 * hack ioctls for sending packets.  However, I know _I_ don't
	 * want users running that on my network! --BLG
	 */
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (copy_from_user(&args, (void*)arg, sizeof(struct ipsec_ioctl_args)))
		return -EFAULT;
	args.dev_name[IFNAMSIZ-1] = 0;

	switch (args.cmd) {
	case ADD_IPSEC_DEV_CMD:
	    return add_ipsec_dev(args.dev_name);
	case DEL_IPSEC_DEV_CMD:
	    return del_ipsec_dev(args.dev_name);
	case IPSEC_ANTI_REPLAY_CMD:
	    pfkey_anti_replay_enabled_set(strcmp(args.dev_name, "off"));
	    break;
	default:
		/* pass on to underlying device instead?? */
		KLIPS_PRINT(debug_tunnel, "klips_debug:ipsec_ioctl_handle: "
		       ": Unknown command: %x \n", args.cmd);
		return -EINVAL;
	}
	return 0;
}

int 
ipsec_tunnel_init_devices(void)
{
#ifdef NET_26
	ipsec_ioctl_hook_set(ipsec_ioctl_handle);
#else
	ipsec_ioctl_hook = ipsec_ioctl_handle;
#endif
	return 0;
}

static void ipsec_tunnel_del_device(ipsec_dev_list **cur)
{
        ipsec_dev_list *tmp;

	if (!cur || !*cur)
	    return;
	tmp = *cur;
	*cur = tmp->next;
	unregister_netdev(tmp->ipsec_dev);
	kfree(tmp->ipsec_dev->priv);
	kfree(tmp->ipsec_dev);
	kfree(tmp);
}

/* void */
int
ipsec_tunnel_cleanup_devices(void)
{
#ifdef NET_26
	ipsec_ioctl_hook_set(NULL);
#else
	ipsec_ioctl_hook = NULL;
#endif
	while (ipsec_dev_head)
	    ipsec_tunnel_del_device(&ipsec_dev_head);
	return 0;
}

static int add_ipsec_dev(char *name)
{
	ipsec_dev_list **cur = &ipsec_dev_head;

	for (; *cur; cur = &(*cur)->next)
	{
		if (!strcmp((*cur)->ipsec_dev->name, name))
			return 0;
	}
	*cur = (ipsec_dev_list *)kmalloc(sizeof(ipsec_dev_list), GFP_KERNEL);
	if (!*cur)
		return -1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
	/* kmalloc like in dev_alloc in kernel 2.2 */
	(*cur)->ipsec_dev = (struct net_device *)kmalloc(
		sizeof(struct net_device) + IFNAMSIZ, GFP_KERNEL);
#else
	(*cur)->ipsec_dev = (struct net_device *)kmalloc(sizeof(struct net_device),
		GFP_KERNEL);
#endif	
	if (!(*cur)->ipsec_dev)
		goto Error;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
	memset((*cur)->ipsec_dev, 0, sizeof(struct net_device)+IFNAMSIZ);
	(*cur)->ipsec_dev->name = (char *)((*cur)->ipsec_dev+1);
#else
	memset((*cur)->ipsec_dev, 0, sizeof(struct net_device));
#endif
	(*cur)->next = NULL;
	strncpy((*cur)->ipsec_dev->name, name, IFNAMSIZ);
	(*cur)->ipsec_dev->name[IFNAMSIZ-1] = 0;
	(*cur)->ipsec_dev->init = ipsec_tunnel_probe;
	if (register_netdev((*cur)->ipsec_dev) != 0)
		goto Error;
	return 0;

Error:
	if (*cur)
		kfree((*cur)->ipsec_dev);
	kfree(*cur);
	return -1;
}

static int del_ipsec_dev(char *name)
{
    ipsec_dev_list **cur = &ipsec_dev_head;

    for (; *cur; cur = &(*cur)->next)
    {
	if (!strcmp((*cur)->ipsec_dev->name, name))
	    break;
    }
    ipsec_tunnel_del_device(cur);
    return 0;
}

EXPORT_SYMBOL(extract_ports);

