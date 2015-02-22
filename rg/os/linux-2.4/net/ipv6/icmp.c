/*
 *	Internet Control Message Protocol (ICMPv6)
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>
 *
 *	$Id: icmp.c,v 1.1.1.1 2007/05/07 23:29:16 jungo Exp $
 *
 *	Based on net/ipv4/icmp.c
 *
 *	RFC 1885
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 *	Changes:
 *
 *	Andi Kleen		:	exception handling
 *	Andi Kleen			add rate limits. never reply to a icmp.
 *					add more length checks and other fixes.
 *	yoshfuji		:	ensure to sent parameter problem for
 *					fragments.
 *	YOSHIFUJI Hideaki @USAGI:	added sysctl for icmp rate limit.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/init.h>

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/icmpv6.h>

#ifdef CONFIG_IPV6_NODEINFO
#include <linux/utsname.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/md5.h>
#endif

#include <net/ip.h>
#include <net/sock.h>

#include <net/ipv6.h>
#include <net/checksum.h>
#include <net/protocol.h>
#include <net/raw.h>
#include <net/rawv6.h>
#include <net/transp_v6.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/icmp.h>


#include <asm/uaccess.h>
#include <asm/system.h>

struct icmpv6_mib icmpv6_statistics[NR_CPUS*2];

/*
 *	ICMP socket(s) for flow control.
 */

static struct socket *__icmpv6_socket[NR_CPUS];
#define icmpv6_socket	__icmpv6_socket[smp_processor_id()]
#define icmpv6_socket_cpu(X) __icmpv6_socket[(X)]

int icmpv6_rcv(struct sk_buff *skb);

static struct inet6_protocol icmpv6_protocol = 
{
	icmpv6_rcv,		/* handler		*/
	NULL,			/* error control	*/
	NULL,			/* next			*/
	IPPROTO_ICMPV6,		/* protocol ID		*/
	0,			/* copy			*/
	NULL,			/* data			*/
	"ICMPv6"	       	/* name			*/
};

struct icmpv6_msg {
	struct icmp6hdr		icmph;
	struct sk_buff		*skb;
	int			offset;
	struct in6_addr		*daddr;
	int			len;
	__u32			csum;
};


static int icmpv6_xmit_holder = -1;

static int icmpv6_xmit_lock_bh(void)
{
	if (!spin_trylock(&icmpv6_socket->sk->lock.slock)) {
		if (icmpv6_xmit_holder == smp_processor_id())
			return -EAGAIN;
		spin_lock(&icmpv6_socket->sk->lock.slock);
	}
	icmpv6_xmit_holder = smp_processor_id();
	return 0;
}

static __inline__ int icmpv6_xmit_lock(void)
{
	int ret;
	local_bh_disable();
	ret = icmpv6_xmit_lock_bh();
	if (ret)
		local_bh_enable();
	return ret;
}

static void icmpv6_xmit_unlock_bh(void)
{
	icmpv6_xmit_holder = -1;
	spin_unlock(&icmpv6_socket->sk->lock.slock);
}

static __inline__ void icmpv6_xmit_unlock(void)
{
	icmpv6_xmit_unlock_bh();
	local_bh_enable();
}

/*
 *	getfrag callback
 */

static int icmpv6_getfrag(const void *data, struct in6_addr *saddr, 
			   char *buff, unsigned int offset, unsigned int len)
{
	struct icmpv6_msg *msg = (struct icmpv6_msg *) data;
	struct icmp6hdr *icmph;
	__u32 csum;

	if (offset) {
		csum = skb_copy_and_csum_bits(msg->skb, msg->offset +
					      (offset - sizeof(struct icmp6hdr)),
					      buff, len, msg->csum);
		msg->csum = csum;
		return 0;
	}

	csum = csum_partial_copy_nocheck((void *) &msg->icmph, buff,
					 sizeof(struct icmp6hdr), msg->csum);

	csum = skb_copy_and_csum_bits(msg->skb, msg->offset,
				      buff + sizeof(struct icmp6hdr),
				      len - sizeof(struct icmp6hdr), csum);

	icmph = (struct icmp6hdr *) buff;

	icmph->icmp6_cksum = csum_ipv6_magic(saddr, msg->daddr, msg->len,
					     IPPROTO_ICMPV6, csum);
	return 0; 
}


/* 
 * Slightly more convenient version of icmpv6_send.
 */
void icmpv6_param_prob(struct sk_buff *skb, int code, int pos)
{
	icmpv6_send(skb, ICMPV6_PARAMPROB, code, pos, skb->dev);
	kfree_skb(skb);
}

/*
 * Figure out, may we reply to this packet with icmp error.
 *
 * We do not reply, if:
 *	- it was icmp error message.
 *	- it is truncated, so that it is known, that protocol is ICMPV6
 *	  (i.e. in the middle of some exthdr)
 *
 *	--ANK (980726)
 *
 * Note: We MUST send Parameter Problem even if it is not for the first
 *       fragment.  See RFC2460.
 *
 *	--yoshfuji (2001/01/05)
 */

static int is_ineligible(struct sk_buff *skb)
{
	int ptr = (u8*)(skb->nh.ipv6h+1) - skb->data;
	int len = skb->len - ptr;
	__u8 nexthdr = skb->nh.ipv6h->nexthdr;

	if (len < 0)
		return 1;

	ptr = ipv6_skip_exthdr(skb, ptr, &nexthdr, len);
	if (ptr < 0)
		return 0;
	if (nexthdr == IPPROTO_ICMPV6) {
		u8 type;
		if (skb_copy_bits(skb, ptr+offsetof(struct icmp6hdr, icmp6_type),
				  &type, 1)
		    || !(type & ICMPV6_INFOMSG_MASK))
			return 1;
	}
	return 0;
}

int sysctl_icmpv6_time = 1*HZ; 

/* 
 * Check the ICMP output rate limit 
 */
static inline int icmpv6_xrlim_allow(struct sock *sk, int type, int code,
				     struct flowi *fl)
{
	struct dst_entry *dst;
	int res = 0;

	/* Informational messages are not limited. */
	if (type & ICMPV6_INFOMSG_MASK) {
		/* but 'REFUSED' or 'UNKNOWN' NI Reply should be limited */
		if (type != ICMPV6_NI_REPLY)
			return 1;
		if (!(code == ICMPV6_NI_REFUSED || code == ICMPV6_NI_UNKNOWN))
			return 1;
	}

	/* Do not limit pmtu discovery, it would break it. */
	if (type == ICMPV6_PKT_TOOBIG)
		return 1;

	/* 
	 * Look up the output route.
	 * XXX: perhaps the expire for routing entries cloned by
	 * this lookup should be more aggressive (not longer than timeout).
	 */
	dst = ip6_route_output(sk, fl);
	if (dst->error) {
		IP6_INC_STATS(((struct inet6_dev *)NULL),Ip6OutNoRoutes);	/* XXX(?) */
	} else if (dst->dev && (dst->dev->flags&IFF_LOOPBACK)) {
		res = 1;
	} else {
		struct rt6_info *rt = (struct rt6_info *)dst;
		int tmo = sysctl_icmpv6_time;

		/* Give more bandwidth to wider prefixes. */
		if (rt->rt6i_dst.plen < 128)
			tmo >>= ((128 - rt->rt6i_dst.plen)>>5);

		res = xrlim_allow(dst, tmo);
	}
	dst_release(dst);
	return res;
}

/*
 *	an inline helper for the "simple" if statement below
 *	checks if parameter problem report is caused by an
 *	unrecognized IPv6 option that has the Option Type 
 *	highest-order two bits set to 10
 */

static __inline__ int opt_unrec(struct sk_buff *skb, __u32 offset)
{
	u8 optval;

	offset += skb->nh.raw - skb->data;
	if (skb_copy_bits(skb, offset, &optval, 1))
		return 1;
	return (optval&0xC0) == 0x80;
}

/*
 *	Send an ICMP message in response to a packet in error
 */

void icmpv6_send(struct sk_buff *skb, int type, int code, __u32 info, 
		 struct net_device *dev)
{
	struct ipv6hdr *hdr = skb->nh.ipv6h;
	struct sock *sk = icmpv6_socket->sk;
	struct inet6_dev *idev;
	struct in6_addr *saddr = NULL;
	int iif = 0;
	struct icmpv6_msg msg;
	struct flowi fl;
	int addr_type = 0;
	int len;

	/*
	 *	sanity check pointer in case of parameter problem
	 */

	if ((u8*)hdr < skb->head || (u8*)(hdr+1) > skb->tail)
		return;

	/*
	 *	Make sure we respect the rules 
	 *	i.e. RFC 1885 2.4(e)
	 *	Rule (e.1) is enforced by not using icmpv6_send
	 *	in any code that processes icmp errors.
	 */
	addr_type = ipv6_addr_type(&hdr->daddr);

	if (ipv6_chk_addr(&hdr->daddr, skb->dev))
		saddr = &hdr->daddr;

	/*
	 *	Dest addr check
	 */

	if ((addr_type & IPV6_ADDR_MULTICAST || skb->pkt_type != PACKET_HOST)) {
		if (type != ICMPV6_PKT_TOOBIG &&
		    !(type == ICMPV6_PARAMPROB && 
		      code == ICMPV6_UNK_OPTION && 
		      (opt_unrec(skb, info))))
			return;

		saddr = NULL;
	}

	addr_type = ipv6_addr_type(&hdr->saddr);

	/*
	 *	Source addr check
	 */

	if (addr_type & IPV6_ADDR_LINKLOCAL)
		iif = skb->dev->ifindex;

	/*
	 *	Must not send if we know that source is Anycast also.
	 *	for now we don't know that.
	 */
	if ((addr_type == IPV6_ADDR_ANY) || 
	    (addr_type & (IPV6_ADDR_MULTICAST|IPV6_ADDR_ANYCAST))
#ifdef CONFIG_IPV6_ANYCAST
	    || (saddr && ipv6_chk_acast_addr(0, saddr))
#endif
	    ) {
		if (net_ratelimit())
			printk(KERN_DEBUG "icmpv6_send: unspec/anycast/mcast source\n");
		return;
	}

	/* 
	 *	Never answer to a ICMP packet.
	 */
	if (is_ineligible(skb)) {
		if (net_ratelimit())
			printk(KERN_DEBUG "icmpv6_send: no reply to icmp error\n"); 
		return;
	}

	fl.proto = IPPROTO_ICMPV6;
	fl.nl_u.ip6_u.daddr = &hdr->saddr;
	fl.nl_u.ip6_u.saddr = saddr;
	fl.oif = iif;
	fl.fl6_flowlabel = 0;
	fl.uli_u.icmpt.type = type;
	fl.uli_u.icmpt.code = code;

	if (icmpv6_xmit_lock())
	    return;

	if (!icmpv6_xrlim_allow(sk, type, code, &fl))
		goto out;

	/*
	 *	ok. kick it. checksum will be provided by the 
	 *	getfrag_t callback.
	 */

	msg.icmph.icmp6_type = type;
	msg.icmph.icmp6_code = code;
	msg.icmph.icmp6_cksum = 0;
	msg.icmph.icmp6_pointer = htonl(info);

	msg.skb = skb;
	msg.offset = skb->nh.raw - skb->data;
	msg.csum = 0;
	msg.daddr = &hdr->saddr;

	len = skb->len - msg.offset + sizeof(struct icmp6hdr);
	len = min_t(unsigned int, len, IPV6_MIN_MTU - sizeof(struct ipv6hdr));

	if (len < 0) {
		if (net_ratelimit())
			printk(KERN_DEBUG "icmp: len problem\n");
		goto out;
	}

	msg.len = len;

	ip6_build_xmit(sk, icmpv6_getfrag, &msg, &fl, len, NULL, -1, -1,
		       MSG_DONTWAIT);
	idev = in6_dev_get(dev);
	if (type >= ICMPV6_DEST_UNREACH && type <= ICMPV6_PARAMPROB) {
		(&(icmpv6_statistics[smp_processor_id()*2].Icmp6OutDestUnreachs))[type-ICMPV6_DEST_UNREACH]++;
		(&(idev->stats.icmpv6[smp_processor_id()*2].Icmp6OutDestUnreachs))[type-ICMPV6_DEST_UNREACH]++;
	}
	if (type == ICMPV6_DEST_UNREACH && code == ICMPV6_ADM_PROHIBITED)
		ICMP6_INC_STATS_BH(idev,Icmp6OutAdminProhibs);
	ICMP6_INC_STATS_BH(idev,Icmp6OutMsgs);
	if (idev)
		in6_dev_put(idev);
out:
	icmpv6_xmit_unlock();
}

static void icmpv6_echo_reply(struct sk_buff *skb)
{
	struct sock *sk = icmpv6_socket->sk;
	struct inet6_dev *idev;
	struct icmp6hdr *icmph = (struct icmp6hdr *) skb->h.raw;
	struct in6_addr *saddr;
	struct icmpv6_msg msg;
	struct flowi fl;

	saddr = &skb->nh.ipv6h->daddr;

	if ((ipv6_addr_type(saddr) & (IPV6_ADDR_MULTICAST|IPV6_ADDR_ANYCAST))
#ifdef CONFIG_IPV6_ANYCAST
	    || ipv6_chk_acast_addr(0, saddr)
#endif
	    )
		saddr = NULL;

	msg.icmph.icmp6_type = ICMPV6_ECHO_REPLY;
	msg.icmph.icmp6_code = 0;
	msg.icmph.icmp6_cksum = 0;
	msg.icmph.icmp6_identifier = icmph->icmp6_identifier;
	msg.icmph.icmp6_sequence = icmph->icmp6_sequence;

	msg.skb = skb;
	msg.offset = 0;
	msg.csum = 0;
	msg.len = skb->len + sizeof(struct icmp6hdr);
	msg.daddr =  &skb->nh.ipv6h->saddr;

	fl.proto = IPPROTO_ICMPV6;
	fl.nl_u.ip6_u.daddr = msg.daddr;
	fl.nl_u.ip6_u.saddr = saddr;
	fl.oif = skb->dev->ifindex;
	fl.fl6_flowlabel = 0;
	fl.uli_u.icmpt.type = ICMPV6_ECHO_REPLY;
	fl.uli_u.icmpt.code = 0;

	if (icmpv6_xmit_lock_bh())
	    return;

	ip6_build_xmit(sk, icmpv6_getfrag, &msg, &fl, msg.len, NULL, -1, -1,
		       MSG_DONTWAIT);
	idev = in6_dev_get(skb->dev);
	ICMP6_INC_STATS_BH(idev,Icmp6OutEchoReplies);
	ICMP6_INC_STATS_BH(idev,Icmp6OutMsgs);
	if (idev)
		in6_dev_put(idev);

	icmpv6_xmit_unlock_bh();
}

#ifdef CONFIG_IPV6_NODEINFO
/*
 *	IPv6 Node Information Queries
 *	<draft-ietf-ipngwg-icmp-name-lookups-07.txt>
 */
/*
 * Authors:
 *	Koichi KUNITAKE
 *	Hideaki YOSHIFUJI
 */

#define __UTS_NODENAME_NONE	"(none)"
#define NI_NONCE_LEN		8
#define NI_TTL_LEN		4	/* sizeof(__u32) */
#define NI_MAX_SYSNAME_LEN	(sizeof(system_utsname.domainname) + \
				sizeof(system_utsname.nodename) + 1)
#define NI_MAX_REPLY_LEN	(IPV6_MIN_MTU - sizeof(struct ipv6hdr))

static int icmpv6_ni_getname(struct sk_buff *skb, 
			     __u8 *subj, int subjlen, 
			     struct net_device **devp, 
			     struct in6_addr *daddr, __u16 *flagsp);
static int icmpv6_ni_getaddrs6(struct sk_buff *skb,
			       __u8 *subj, int subjlen, 
			       struct net_device **devp, 
			       struct in6_addr *daddr, __u16 *flagsp);
static int icmpv6_ni_getaddrs4(struct sk_buff *skb,
			       __u8 *subj, int subjlen, 
			       struct net_device **devp, 
			       struct in6_addr *daddr, __u16 *flagsp);

struct icmpv6_ni_table {
	const char	*name;
	int		(*getinfo)(struct sk_buff *skb,
				   __u8 *subj, int subjlen,
				   struct net_device **devp,
				   struct in6_addr *daddr,
				   __u16 *flagsp);
};

struct icmpv6_ni_table icmpv6_ni_code_table[] = {
	{	"IPV6",		icmpv6_ni_getaddrs6,	},
	{	"FQDN",		icmpv6_ni_getname,	},
	{	"IPV4",		icmpv6_ni_getaddrs4,	},
};

struct icmpv6_ni_table icmpv6_ni_qtype_table[] = {
	{	"NOOP",		NULL,			},
	{	"SUPTYPES",	NULL,			},
	{	"FQDN",		icmpv6_ni_getname,	},
	{	"NODEADDR",	icmpv6_ni_getaddrs6,	},
	{	"IPV4ADDR",	icmpv6_ni_getaddrs4,	},
};

struct in6_addr icmpv6_ni_in6;
DECLARE_RWSEM(icmpv6_ni_in6_sem);

static __inline__ const char *icmpv6_ni_qtypename(__u16 qtype)
{
	return ((qtype < sizeof(icmpv6_ni_qtype_table)/sizeof(icmpv6_ni_qtype_table[0])) ?
		icmpv6_ni_qtype_table[qtype].name : "unknown");
}

static size_t str2lower(char *dst, const char *src, size_t len)
{
	const char *p;
	char *q;
	size_t i;
	for (p = src, q = dst, i = 0; *p && i + 1 < len; p++, i++)
		*q++ = (isascii(*p) && isupper(*p)) ? tolower(*p) : *p;
	*q = '\0';
	return i;
}

struct in6_addr icmpv6_ni_group(const char *name)
{
	char *p;
	MD5_CTX ctxt;
	__u8 digest[16];
	char hbuf[NI_MAX_SYSNAME_LEN];
	struct in6_addr in6;
	int len;
	(void)str2lower(&hbuf[1], name, sizeof(hbuf)-1);
	p = strchr(&hbuf[1], '.');
	len = p ? (p - &hbuf[1]) : strlen(&hbuf[1]);
	if (len >= 0x40) {
		memset(&in6, 0, sizeof(in6));
		return in6;	/* XXX */
	}
	hbuf[0] = (char)len;
	memset(&ctxt, 0, sizeof(ctxt));
	MD5Init(&ctxt);
	MD5Update(&ctxt, hbuf, 1 + len);
	MD5Final(digest, &ctxt);
	ipv6_addr_set (&in6, 
			__constant_htonl(0xff020000), __constant_htonl(0x00000000),
			__constant_htonl(0x00000002), *(__u32 *)digest);   
	return in6;
}

void icmpv6_ni_join_nigroup_dev(struct net_device *dev)
{
	struct sock *sock = icmpv6_socket->sk;
	if (!(dev->flags&IFF_UP))
		return;
	down_read(&icmpv6_ni_in6_sem);
	if (icmpv6_ni_in6.s6_addr32[0] == __constant_htonl(0x00000000)) {
		up_read(&icmpv6_ni_in6_sem);
		return;
	}
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
	printk(KERN_DEBUG
		"ni_join_nigroup_dev(): %s (index=%d)\n",
		dev->name ? dev->name : "(null)",
		dev->ifindex);
#endif
	ipv6_sock_mc_join(sock, dev->ifindex, &icmpv6_ni_in6);
	up_read(&icmpv6_ni_in6_sem);
}

static void icmpv6_ni_joinleave_group(struct in6_addr *nigroup, int join)
{
	struct sock *sock = icmpv6_socket->sk;
	struct net_device *dev;
	if (nigroup->s6_addr32[0] == __constant_htonl(0x00000000))
		return;
	read_lock(&dev_base_lock);
	for (dev = dev_base; dev; dev = dev->next) {
		if (!(dev->flags&IFF_UP))
			continue;
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
		printk(KERN_DEBUG
			"ni_joinleave_group(): %s, %s (index=%d)\n",
			join ? "join" : "leave",
			dev->name ? dev->name : "(null)",
			dev->ifindex);
#endif
		if (join) {
			ipv6_sock_mc_join(sock, dev->ifindex, nigroup);
		} else {
			ipv6_sock_mc_drop(sock, dev->ifindex, nigroup);
		}
	}
	read_unlock(&dev_base_lock);
}

static void __icmpv6_sethostname_hook(struct new_utsname *sysname)
{
	struct in6_addr new;
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
	char addrbuf[128];
#endif
	down_write(&icmpv6_ni_in6_sem);
	if (strcmp(sysname->nodename, __UTS_NODENAME_NONE) == 0) {
		memset(&new, 0, sizeof(new));
	} else {
		new = icmpv6_ni_group(sysname->nodename);
	}
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
	in6_ntop(&icmpv6_ni_in6, addrbuf);
	printk(KERN_DEBUG
		"icmpv6_sethostname_hook: old = %s\n",
		addrbuf);
	in6_ntop(&new, addrbuf);
	printk(KERN_DEBUG
		"icmpv6_sethostname_hook: new = %s\n",
		addrbuf);
#endif
	if (ipv6_addr_cmp(&icmpv6_ni_in6, &new)) {
		if (new.s6_addr32[0] != __constant_htonl(0x00000000)) {
			icmpv6_ni_joinleave_group(&new, 1);
		}
		if (icmpv6_ni_in6.s6_addr32[0] != __constant_htonl(0x00000000)) {
			icmpv6_ni_joinleave_group(&icmpv6_ni_in6, 0);
		}
		icmpv6_ni_in6 = new;
	}
	up_write(&icmpv6_ni_in6_sem);
}

static int icmpv6_ni_dnscmp(__u8 *p, int plen, __u8 *q, int qlen, int qtrunc)
{
	/* be sure that p is subject, q is encoded system name */
	__u8 *p0 = p, *q0 = q;
	int done = 0, retcode = 0;
	if (plen < 1 || qlen < 1)
		return -1;	/* invalid length */
	/* simple case */
	if (plen == qlen && memcmp(p0, q0, plen) == 0)
		return 0;
	if (*(p0 + plen - 1) || *(q0 + qlen - 1))
		return -1; /* invalid termination */
	while(p < p0 + plen && q < q0 + qlen) {
		if (*p >= 0x40 || *q >= 0x40)
			return -1;	/* DNS compression cannot be used in subject */
		if (p + *p + 1 > p0 + plen || q + *q + 1 > q0 + qlen)
			return -1;	/* overrun */
		if (*p == '\0') {
			if (p == p0 + plen - 1)
				break;		/* FQDN */
			else if (p + 1 == p0 + plen - 1)
				return retcode;	/* truncated */
			else
				return -1;	/* more than one subject ?! */
		}
		if (!done) {
			if (*q == '\0') {
				if (q == q0 + qlen - 1) {
					done = 1;	/* FQDN */
				} else if (q + 1 == q0 + qlen - 1) {
					retcode = qtrunc;
					done = 1;
				} else
					return -1;	/* more than one subject ?! */
			} else {
				if (*p != *q) {
					retcode = 1;
					done = 1;
				} else {
					if (memcmp(p+1, q+1, *p)) {
						retcode = 1;
						done = 1;
					}
				}
			}
		}
		p += *p + 1;
		q += done ? 0 : (*q + 1);
	}
	return retcode;
}

static int icmpv6_ni_getname(struct sk_buff *skb,
			     __u8 *subj, int subjlen, 
			     struct net_device **devp, 
			     struct in6_addr *daddr, __u16 *flagsp)
{
	__u8 tmpbuf[NI_MAX_SYSNAME_LEN];
	__u8 *dst;
	int sysname_known = 0, sysname_len = 0;
	int buflen;
	int ret;
	if (subj && subjlen < 2)
		return -1;	/* too short subject */
	if (skb) {
		buflen = skb_tailroom(skb) - NI_TTL_LEN;
		if (buflen < 0) {
			printk(KERN_WARNING
				"icmpv6_ni_getname(): too short skb %p\n", skb);
			return -1;
		}
		memset(skb_put(skb, NI_TTL_LEN), 0, NI_TTL_LEN);
		dst = skb->tail;	/* XXX */
	} else {
		dst = tmpbuf;
		buflen = sizeof(tmpbuf);
	}
	down_read(&uts_sem);
	if (strcmp(system_utsname.nodename, __UTS_NODENAME_NONE)) {
		char *cp = (char *)dst;
		int dcnt = 0;
#ifdef CONFIG_IPV6_NODEINFO_USE_UTS_DOMAIN
		size_t nodelen = str2lower(cp + 1, system_utsname.nodename, sizeof(system_utsname.nodename));
		if (strcmp(system_utsname.domainname, __UTS_NODENAME_NONE)) {
			*(cp + 1 + nodelen) = '.';
			(void)str2lower(cp + 1 + nodelen + 1, system_utsname.domainname, sizeof(system_utsname.domainname));
		}
#else
		(void)str2lower(cp + 1, system_utsname.nodename, sizeof(system_utsname.nodename));
#endif
		up_read(&uts_sem);
		sysname_known = 1;
		while (cp) {
			char *p;
			size_t l;
			p = strchr(cp + 1, '.');
			l = p ? ((size_t)(p - (cp + 1))) : strlen(cp + 1);
			/* RFC1035 limits size of single component */
			if (l == 0 || l >= 0x40) {
				if (net_ratelimit()) {
					printk(KERN_WARNING
						"ICMPv6 NI: invalid length of single component of your hostname (%d); %s.\n",
						l, l ? "too large" : "zero");
				}
                                sysname_known = -1; /* truncated by an error */
                                break;
			}
			/* RFC1035 limits size of names */
			/* XXX: currently cannot be happeded because nodename is 64 bytes at max. */
			if (sysname_len + l + 1 > 255 || sysname_len + l + 1 > buflen) {
				if (net_ratelimit()) {
					printk(KERN_WARNING
						"ICMPv6 NI: length of your hostname (%d) is too large\n",
						1);
				}
				sysname_known = -1; /* truncated by an error */
				break;
			}
			*cp = (char)l;
			sysname_len += l + 1;
			if (p) {
				dcnt++;
			}
			cp = p;
		}
		if (sysname_len > 0) {
			dst[sysname_len++] = '\0';
			if (dcnt < 2 || sysname_known < 0) {
				dst[sysname_len] = '\0';
				sysname_len++;
			}
		}
		if (subj && icmpv6_ni_dnscmp(subj, subjlen, dst, sysname_len, sysname_known > 0 ? 0 : 1)) {
			ret = -1;
		} else {
			if (skb)
				skb_put(skb, sysname_len);
			ret = NI_TTL_LEN + sysname_len;
		}
	} else {
		up_read(&uts_sem);
		if (net_ratelimit()) {
			printk(KERN_WARNING "ICMPv6 NI: your hostname is unknown\n");
		}
		ret = -1;
	}
	return ret;
}

static int __icmpv6_ni_getaddrs6(struct sk_buff *skb,
				 struct in6_addr *in6, 
				 struct net_device *dev, 
				 __u16 *flagsp, int *subjokp)
{
	struct inet6_dev *idev;
	struct inet6_ifaddr *ifp;
	__u16 flags = flagsp ? *flagsp : 0;
	int len = 0;
	int subjok = in6 ? (subjokp ? *subjokp : 0) : 1;
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
	char addrbuf[128];
	if (in6) {
		in6_ntop(in6, addrbuf);
		printk(KERN_DEBUG
			"ni_getaddrs6(): subject address: %s, device=%s\n", addrbuf, dev ? dev->name : "(null)");
	} else {
		printk(KERN_DEBUG
			"ni_getaddrs6(): device=%s\n", dev ? dev->name : "(null)");
	}
	if (!skb) {
		printk(KERN_DEBUG
			"ni_getaddrs6():- result is not needed\n");
	}
#endif
	idev = in6_dev_get(dev);
	if (!idev)
		return -1;	/* XXX */

	read_lock(&idev->lock);
	for (ifp=idev->addr_list; ifp; ifp=ifp->if_next) {
		int copy_flag = 0;
		if (subjok >= 0 && in6) {
			/* we don't allow loopback address as a subject; XXX: REFUSED? */
			if (ifp->scope != IFA_HOST && !ipv6_addr_cmp(in6, &ifp->addr)) {
				subjok = ifp->flags&IFA_F_TEMPORARY ? -1 : 1;
			}
		}
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
		in6_ntop(&ifp->addr, addrbuf);
		printk(KERN_DEBUG
			"ni_getaddrs6():- address: %s, subjok=%d\n", addrbuf, subjok);
#endif
		if ((!skb && subjok) || subjok < 0)
			break;
		spin_lock_bh(&ifp->lock);
		if (skb && !(ifp->flags & (IFA_F_DEPRECATED|IFA_F_TENTATIVE|IFA_F_TEMPORARY))) {
			switch (ifp->scope) {
			case IPV6_ADDR_COMPATv4:
				/* If IPv4-compatible addresses are requested, set copy_flag to 1. */
				if (flags & ICMPV6_NI_NODEADDR_FLAG_COMPAT) {
					copy_flag = 1;
				}
				break;
			case IFA_LINK:
				/* If Link-scope addresses are requested, set copy_flag to 1. */
				if (flags & ICMPV6_NI_NODEADDR_FLAG_LINKLOCAL) {
					copy_flag = 1;
				}
				break;
			case IFA_SITE:
				/* If Site-local addresses are requested, set copy_flag to 1. */
				if (flags & ICMPV6_NI_NODEADDR_FLAG_SITELOCAL) {
					copy_flag = 1;
				}
				break;
			case IFA_GLOBAL:
				/* If Global-scope addresses are requested, set copy_flag to 1. */
				if (flags & ICMPV6_NI_NODEADDR_FLAG_GLOBAL) {
					copy_flag = 1;
				}
				break;
			default:
				copy_flag = 0;
				break;
			}

			if (copy_flag) {
				if (skb_tailroom(skb) < NI_TTL_LEN + sizeof(struct in6_addr)) {
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
					printk(KERN_DEBUG
						"ni_getaddrs6():- to be copied but truncated\n");
#endif
					flags |= ICMPV6_NI_NODEADDR_FLAG_TRUNCATE;
					if (subjok)
						break;
				} else {
					if (skb) {
						__u8 *p = skb_put(skb, NI_TTL_LEN + sizeof(struct in6_addr));
						__u32 ttl;
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
						printk(KERN_DEBUG
							"ni_getaddrs6():- copied\n");
#endif
						if (ifp->valid_lft) {
							if (ifp->valid_lft > (jiffies - ifp->tstamp) / HZ)
								ttl = ifp->valid_lft - (jiffies - ifp->tstamp) / HZ;
							else
								ttl = 0;
							if (ttl > 0x7fffffff)
								ttl = 0x7fffffff;
						} else {
							ttl = 0x7fffffff;
						}
						ttl = htonl(ttl);
						memcpy(p, &ttl, sizeof(ttl));
						ipv6_addr_copy((struct in6_addr *)(p + NI_TTL_LEN), &ifp->addr);
						len += NI_TTL_LEN + sizeof(struct in6_addr);
					}
				}
			}
		}
		spin_unlock_bh(&ifp->lock);
	}
	read_unlock(&idev->lock);
	if (flagsp) {
		*flagsp = flags;
	}
	if (subjok && subjokp) {
		*subjokp = subjok;
	}
	if (!subjok && !(flags & ICMPV6_NI_NODEADDR_FLAG_ALL)) {
		len = -1;
	} else if (subjok < 0) {
		len = -2;
	}
	

#ifdef CONFIG_IPV6_NODEINFO_DEBUG
	printk(KERN_DEBUG
		"ni_getaddrs6():- len=%d, flags=%04x\n", len, ntohs(flags));
#endif
	in6_dev_put(idev);
	return len;
}

static int icmpv6_ni_getaddrs6(struct sk_buff *skb,
			       __u8 *subj, int subjlen, 
			       struct net_device **devp, 
			       struct in6_addr *daddr, __u16 *flagsp)
{
	struct in6_addr *in6 = (struct in6_addr *)subj;
	struct net_device *dev = devp ? *devp : NULL;
	__u16 flags = flagsp ? *flagsp : 0;
	int len = 0;
	int subjok = in6 ? 0 : 1;

	if (subj && subjlen != sizeof(struct in6_addr))
		return -1;	/* too short subject */

	flags &= (ICMPV6_NI_NODEADDR_FLAG_ALL|
			ICMPV6_NI_NODEADDR_FLAG_LINKLOCAL|
			ICMPV6_NI_NODEADDR_FLAG_SITELOCAL|
			ICMPV6_NI_NODEADDR_FLAG_GLOBAL|
			ICMPV6_NI_NODEADDR_FLAG_COMPAT);

	if (!subjok) {
		subjok = !ipv6_addr_cmp(in6, daddr);	/* for special case */
	}

	if (dev && !(flags & ICMPV6_NI_NODEADDR_FLAG_ALL)) {
		len = __icmpv6_ni_getaddrs6(skb, in6, dev, flagsp ? &flags : NULL, &subjok);
	} else {
		read_lock(&dev_base_lock);
		for (dev = dev_base; dev; dev = dev->next) {
			int sublen = __icmpv6_ni_getaddrs6(skb, in6, dev, flagsp ? &flags : NULL, &subjok);
			if (sublen >= 0) {
				len += sublen;
				if (devp && !*devp) {
					dev_hold(dev);
					*devp = dev;
				}
			} else if (subjok < 0) {
				len = -2;
				break;
			}
		}
		read_unlock(&dev_base_lock);
	}
	if (!subjok) {
		len = -1;
		flags &= ~ICMPV6_NI_NODEADDR_FLAG_TRUNCATE;
	}
	if (flagsp) {
		*flagsp = flags;
	}

	return len;
}

static int __icmpv6_ni_getaddrs4(struct sk_buff *skb,
				 struct in_addr *in, 
				 struct net_device *dev, 
				 __u16 *flagsp, int *subjokp)
{
	struct in_device *in_dev;
	struct in_ifaddr *ifa;
	__u16 flags = 0;
	int len = 0;
	int subjok = in ? (subjokp ? *subjokp : 0) : 1;
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
	char addrbuf[128];
	if (in) {
		in_ntop(in, addrbuf);
		printk(KERN_DEBUG
			"ni_getaddrs4(): subject address: %s, device=%s\n", addrbuf, dev ? dev->name : "(null)");
	} else {
		printk(KERN_DEBUG
			"ni_getaddrs4(): device=%s\n", dev ? dev->name : "(null)");
	}
	if (!skb) {
		printk(KERN_DEBUG
			"ni_getaddrs4():- result is not needed\n");
	}
#endif
	read_lock(&inetdev_lock);
	if ((in_dev = __in_dev_get(dev)) == NULL) {
		read_unlock(&inetdev_lock);
		return -1;	/*XXX*/
	}

	read_lock(&in_dev->lock);
	for (ifa = in_dev->ifa_list; ifa; ifa = ifa->ifa_next) {
		int copy_flag = 0;
		if (!subjok && in) {
			/* we don't allow loopback address as a subject; XXX: REFUSED? */
			if (ifa->ifa_scope != RT_SCOPE_HOST && memcmp(in, &ifa->ifa_address, sizeof(*in)) == 0) {
				subjok = 1;
			}
		}
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
		in_ntop((struct in_addr*)&ifa->ifa_address, addrbuf);	/*XXX*/
		printk(KERN_DEBUG
			"ni_getaddrs4():- address: %s, subjok=%d\n", addrbuf, subjok);
#endif

		if (!skb && subjok)
			break;
		if (skb && !(ifa->ifa_flags & (IFA_F_DEPRECATED|IFA_F_TENTATIVE))) {
			switch (ifa->ifa_scope) {
			case RT_SCOPE_HOST:
				break;	/*XXX*/
			default:
				copy_flag = 1;
			}
		}
		if (copy_flag) {
			if (skb_tailroom(skb) < NI_TTL_LEN + sizeof(struct in_addr)) {
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
				printk(KERN_DEBUG
					"ni_getaddrs4():- to be copied but truncated\n");
#endif
				flags |= ICMPV6_NI_IPV4ADDR_FLAG_TRUNCATE;
				if (subjok)
					break;
			} else {
				if (skb) {
					__u8 *p = skb_put(skb, NI_TTL_LEN + sizeof(struct in_addr));
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
					printk(KERN_DEBUG
						"ni_getaddrs4():- copied\n");
#endif
					memset(p, 0, NI_TTL_LEN);	/* XXX */
					memcpy(p + NI_TTL_LEN, &ifa->ifa_address, sizeof(struct in_addr));
					len += NI_TTL_LEN + sizeof(struct in_addr);
				}
			}
		}
	}
	read_unlock(&in_dev->lock);
	read_unlock(&inetdev_lock);
	if (flagsp) {
		*flagsp = flags;
	}
	if (subjok && subjokp) {
		*subjokp = 1;
	}
	if (!subjok && !(flags & ICMPV6_NI_IPV4ADDR_FLAG_ALL)) {
		len = -1;
	}
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
	printk(KERN_DEBUG
		"ni_getaddrs4():- len=%d, flags=%04x\n", len, ntohs(flags));
#endif
	return len;
}

static int icmpv6_ni_getaddrs4(struct sk_buff *skb,
			       __u8 *subj, int subjlen, 
			       struct net_device **devp, 
			       struct in6_addr *daddr, __u16 *flagsp)
{
	struct in_addr *in = (struct in_addr *)subj;
	struct net_device *dev = devp ? *devp : NULL;
	__u16 flags = flagsp ? *flagsp : 0;
	int len = 0;
	int subjok = in ? 0 : 1;

	if (subj && subjlen != sizeof(struct in_addr))
		return -1;	/* too short subject */

	flags &= ICMPV6_NI_IPV4ADDR_FLAG_ALL;

	if (dev && !(flags & ICMPV6_NI_IPV4ADDR_FLAG_ALL)) {
		len = __icmpv6_ni_getaddrs4(skb, in, dev, flagsp ? &flags : NULL, &subjok);
	} else {
		read_lock(&dev_base_lock);
		for (dev = dev_base; dev; dev = dev->next) {
			int sublen = __icmpv6_ni_getaddrs4(skb, in, dev, flagsp ? &flags : NULL, &subjok);
			if (sublen >= 0) {
				len += sublen;
				if (devp && !*devp) {
					dev_hold(dev);
					*devp = dev;
				}
			}
		}
		read_unlock(&dev_base_lock);
	}
	if (!subjok) {
		len = -1;
		flags &= ~ICMPV6_NI_IPV4ADDR_FLAG_TRUNCATE;
	}
	if (flagsp) {
		*flagsp = flags;
	}

	return len;
}

static void icmpv6_ni_reply(struct sk_buff *skb, int config_accept_ni)
{
	struct sock *sk = icmpv6_socket->sk;
	struct ipv6hdr *hdr = skb->nh.ipv6h;
	struct icmp6hdr *icmph = (struct icmp6hdr *) skb->h.raw;
	struct in6_addr *saddr;
	struct icmpv6_msg msg;
	struct flowi fl;
	__u8 *data = (__u8 *) (icmph + 1);
	int datalen = skb->tail - (unsigned char *)data;
	__u8 *subject;
	int subjlen;
	__u16 qtype = ntohs(icmph->icmp6_qtype);
	struct sk_buff *ni_reply_skb = NULL;
	int len = 0;
	__u8 ni_code;
	__u16 ni_flags = 0;
	int flen = 0;
	__u16 fni_flags = 0;
	struct net_device *dev = NULL;
	struct inet6_dev *idev = NULL;
	struct icmpv6_ni_table *code_table = NULL, *qtype_table = NULL;


#ifdef CONFIG_SYSCTL
	/*
	 * [Step 0] Decide our policy through sysctl and/or /proc fs
	 *    >  0 : accept NI
	 *    == 0 : just ignore NI
	 *    <  0 : refuse NI
	 */
	if (config_accept_ni > 0) {
		idev = in6_dev_get(skb->dev);
		if (idev == NULL) {
			/* Do I need this part? */
			if (net_ratelimit()) {
				printk(KERN_WARNING 
					"ICMPv6 NI: can't find in6 device\n");
			}
			goto ni_return;
		}
		read_lock(&idev->lock);
		config_accept_ni = idev->cnf.accept_ni;
		read_unlock(&idev->lock);
	}
	if (!config_accept_ni)
		goto ni_return;
#endif

	/*
	 * [Step 1] Check the size of query packet
	 */
	if (datalen < NI_NONCE_LEN) {
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
		if (net_ratelimit()) {
			printk(KERN_WARNING 
				"ICMPv6 NI: invalid query packet length(%d): code=%u, qtype=%u, flags=0x%04x\n",
				datalen, icmph->icmp6_code, qtype, icmph->icmp6_flags);
		}
#endif
		goto ni_return;
	}
	subject = data + NI_NONCE_LEN;		/* subject */
	subjlen = datalen - NI_NONCE_LEN;	/* subject length */

	ni_reply_skb = alloc_skb(NI_MAX_REPLY_LEN, GFP_ATOMIC);
	if (ni_reply_skb == NULL)
		goto ni_return;
	skb_reserve(ni_reply_skb, NI_NONCE_LEN);

	/*
	 * [Step 2] Code / qtype / subject validation
	 *  (1) qtype validation (set ni_code to ICMPV6_NI_SUCCESS if it is 
	 *      known)
	 *  (2) code validation for codes except for NOOP and SUPTYPES
	 *      - reply nothing if it is unknown.
	 *  (3) subject validation
	 *      a. qtype NOOP and SUPTYPES
	 *         - only subject name with no data is valid.
	 *         - we must ignore code for NOOP.
	 *         - it is not clarified, but we should ignore code for 
	 *           SUPTYPES.
	 *      b. qtype FQDN, NODEADDR and IPV4ADDR
	 *         1) IPV6 subject must have one ipv6 address.
	 *            this should be one of our address.
	 *         2) FQDN subject must match up to our system name.
	 *         3) IPV4 subject must have one ipv4 address.
	 *            this should be one of our ipv4 address. 
	 *  NOTE: we do not supply any reply for malformed query;
	 *        spec. says we must silently dicard the query
	 *        if subject does not match up to our configuration.
	 */
	ni_code = ICMPV6_NI_UNKNOWN;
	switch(qtype) {
	case ICMPV6_NI_QTYPE_NOOP:
	case ICMPV6_NI_QTYPE_SUPTYPES:
		/* simple case: code should be ICMPV6_NI_SUBJ_FQDN and must have no subject */
		switch(icmph->icmp6_code) {
		case ICMPV6_NI_SUBJ_FQDN:
			if (subjlen) {
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
				if (net_ratelimit()) {
					printk(KERN_WARNING
						"ICMPv6 NI (%s): invalid subject length (%d)\n",
						icmpv6_ni_qtypename(qtype), subjlen);
				}
#endif
				goto ni_return;
			}
			break;
		default:
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
			if(net_ratelimit()) {
				printk(KERN_WARNING
					"ICMPv6 NI (%s): invalid/unknown code(%u)\n",
					icmpv6_ni_qtypename(qtype), icmph->icmp6_code);
			}
#endif
			/* we ignore code */
		}
		ni_code = ICMPV6_NI_SUCCESS;
		break;
	default:
		if (qtype < sizeof(icmpv6_ni_qtype_table)/sizeof(icmpv6_ni_qtype_table[0])) {
			qtype_table = &icmpv6_ni_qtype_table[qtype];
			ni_code = ICMPV6_NI_SUCCESS;
		}
		if (icmph->icmp6_code < sizeof(icmpv6_ni_code_table)/sizeof(icmpv6_ni_code_table[0])) {
			code_table = &icmpv6_ni_code_table[icmph->icmp6_code];
			fni_flags = icmph->icmp6_flags;
			if (qtype_table && qtype_table->getinfo == code_table->getinfo) {
				flen = code_table->getinfo(ni_reply_skb,
							   subject, subjlen,
							   NULL, 
							   &hdr->daddr, 
							   &fni_flags);
			} else {
				flen = code_table->getinfo(NULL,
							   subject, subjlen,
							   &dev, 
							   &hdr->daddr,
							   NULL);
			}
			if (flen < -1) {
				config_accept_ni = -1;	/*refused*/
			} else if (flen < 0) {
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
				if (net_ratelimit()) {
					printk(KERN_WARNING
						"ICMPv6 NI (%s): invalid subject %s, length %d\n",
						icmpv6_ni_qtypename(qtype),
						code_table->name, subjlen);
				}
#endif
				goto ni_return;
			}
		} else {
#ifdef CONFIG_IPV6_NODEINFO_DEBUG
			if(net_ratelimit()) {
				printk(KERN_WARNING
					"ICMPv6 NI (%s): unknown code(%d)\n",
					icmpv6_ni_qtypename(qtype), icmph->icmp6_code);
			}
#endif
			goto ni_return;
		}
	}

	/*
	 * [Step 3] Fill common field
	 */
	memcpy(skb_push(ni_reply_skb, NI_NONCE_LEN), data, NI_NONCE_LEN);

	/*
	 * [Step 4] Unsuccessful reply
	 *  - UNKNOWN reply should be done before policy based refusal (section 3)
	 */
	if (ni_code != ICMPV6_NI_SUCCESS)
		goto ni_unknown;
	if (config_accept_ni < 0) {
		ni_code = ICMPV6_NI_REFUSED;
		goto ni_refused;
	}

	/*
	 * [Step 5] Fill the reply
	 */
	switch(qtype) {
	case ICMPV6_NI_QTYPE_NOOP:
		break;
	case ICMPV6_NI_QTYPE_SUPTYPES:
	{
		/* supports NOOP, SUPTYPES, FQDN and NODEADDR */
		__u32 suptypes = __constant_htonl(0x0000001f);	/* 00000000 00000000 00000000 00011111 */
		memcpy(skb_put(ni_reply_skb, sizeof(suptypes)), &suptypes, sizeof(suptypes));
		len += sizeof(suptypes);
		break;
	}
	default:
		if (qtype_table->getinfo != code_table->getinfo) {
			flen = icmpv6_ni_qtype_table[qtype].getinfo(ni_reply_skb,
								    NULL, 0,
								    &dev, NULL,
								    &fni_flags);
		}
		len += flen;
		ni_flags = fni_flags;
	}

ni_unknown:
ni_refused:
	/*
	 * [Step 6] Send reply
	 */
	saddr = &hdr->daddr;

	if ((ipv6_addr_type(saddr) & (IPV6_ADDR_MULTICAST|IPV6_ADDR_ANYCAST))
#ifdef CONFIG_IPV6_ANYCAST
	    || ipv6_chk_acast_addr(0, saddr)
#endif
	    ) {
		saddr = NULL;
	}

	msg.icmph.icmp6_type = ICMPV6_NI_REPLY;
	msg.icmph.icmp6_code = ni_code;
	msg.icmph.icmp6_cksum = 0;
	msg.icmph.icmp6_qtype = icmph->icmp6_qtype;
	msg.icmph.icmp6_flags = ni_flags;

	msg.skb = ni_reply_skb;
	msg.offset = 0;
	msg.csum = 0;
	msg.len = sizeof(struct icmp6hdr) + ni_reply_skb->len;
	msg.daddr = &hdr->saddr;

	fl.proto = IPPROTO_ICMPV6;
	fl.nl_u.ip6_u.daddr = &hdr->saddr;
	fl.nl_u.ip6_u.saddr = saddr;
	fl.oif = skb->dev->ifindex;
	fl.fl6_flowlabel = 0;
	fl.uli_u.icmpt.type = ICMPV6_NI_REPLY;
	fl.uli_u.icmpt.code = ni_code;

	if (icmpv6_xmit_lock_bh())
		goto ni_return;

	/* rate-limit */
	if (!icmpv6_xrlim_allow(sk, ICMPV6_NI_REPLY, ni_code, &fl))
		goto out;

	ip6_build_xmit(sk, icmpv6_getfrag, &msg, &fl, msg.len, NULL, -1, -1,
		       MSG_DONTWAIT);

#ifndef CONFIG_SYSCTL
	idev = in6_dev_get(skb->dev);
#endif
	ICMP6_INC_STATS_BH(idev,Icmp6OutMsgs);
out:
	icmpv6_xmit_unlock_bh();
ni_return:
	if (ni_reply_skb)
		kfree_skb(ni_reply_skb);
	if (idev) {
		in6_dev_put(idev);
	}
	if (dev) {
		dev_put(dev);
	}
}
#endif

static void icmpv6_notify(struct sk_buff *skb, int type, int code, u32 info)
{
	struct in6_addr *saddr, *daddr;
	struct inet6_protocol *ipprot;
	struct sock *sk;
	int inner_offset;
	int hash;
	u8 nexthdr;

	if (!pskb_may_pull(skb, sizeof(struct ipv6hdr)))
		return;

	nexthdr = ((struct ipv6hdr *)skb->data)->nexthdr;
	if (ipv6_ext_hdr(nexthdr)) {
		/* now skip over extension headers */
		inner_offset = ipv6_skip_exthdr(skb, sizeof(struct ipv6hdr), &nexthdr, skb->len - sizeof(struct ipv6hdr));
		if (inner_offset<0)
			return;
	} else {
		inner_offset = sizeof(struct ipv6hdr);
	}

	/* Checkin header including 8 bytes of inner protocol header. */
	if (!pskb_may_pull(skb, inner_offset+8))
		return;

	saddr = &skb->nh.ipv6h->saddr;
	daddr = &skb->nh.ipv6h->daddr;

	/* BUGGG_FUTURE: we should try to parse exthdrs in this packet.
	   Without this we will not able f.e. to make source routed
	   pmtu discovery.
	   Corresponding argument (opt) to notifiers is already added.
	   --ANK (980726)
	 */

	hash = nexthdr & (MAX_INET_PROTOS - 1);

	for (ipprot = (struct inet6_protocol *) inet6_protos[hash]; 
	     ipprot != NULL; 
	     ipprot=(struct inet6_protocol *)ipprot->next) {
		if (ipprot->protocol != nexthdr)
			continue;

		if (ipprot->err_handler)
			ipprot->err_handler(skb, NULL, type, code, inner_offset, info);
	}

	read_lock(&raw_v6_lock);
	if ((sk = raw_v6_htable[hash]) != NULL) {
		while((sk = __raw_v6_lookup(sk, nexthdr, daddr, saddr))) {
			rawv6_err(sk, skb, NULL, type, code, inner_offset, info);
			sk = sk->next;
		}
	}
	read_unlock(&raw_v6_lock);
}
  
/*
 *	Handle icmp messages
 */

int icmpv6_rcv(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct inet6_dev *idev = in6_dev_get(dev);
	struct in6_addr *saddr, *daddr;
	struct ipv6hdr *orig_hdr;
	struct icmp6hdr *hdr;
	int type;

	ICMP6_INC_STATS_BH(idev,Icmp6InMsgs);

	saddr = &skb->nh.ipv6h->saddr;
	daddr = &skb->nh.ipv6h->daddr;

	/* Perform checksum. */
	if (skb->ip_summed == CHECKSUM_HW) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		if (csum_ipv6_magic(saddr, daddr, skb->len, IPPROTO_ICMPV6,
				    skb->csum)) {
			if (net_ratelimit())
				printk(KERN_WARNING "ICMPv6 hw checksum failed\n");
			skb->ip_summed = CHECKSUM_NONE;
		}
	}
	if (skb->ip_summed == CHECKSUM_NONE) {
		if (csum_ipv6_magic(saddr, daddr, skb->len, IPPROTO_ICMPV6,
				    skb_checksum(skb, 0, skb->len, 0))) {
			char sbuf[64], dbuf[64];
			in6_ntop(saddr, sbuf);
			in6_ntop(daddr, dbuf);
			if (net_ratelimit())
				printk(KERN_WARNING "ICMPv6 checksum failed [%s > %s]\n",
					sbuf, dbuf);
			goto discard_it;
		}
	}

	if (!pskb_pull(skb, sizeof(struct icmp6hdr)))
		goto discard_it;

	hdr = (struct icmp6hdr *) skb->h.raw;

	type = hdr->icmp6_type;

	if (type >= ICMPV6_DEST_UNREACH && type <= ICMPV6_PARAMPROB) {
		(&icmpv6_statistics[smp_processor_id()*2].Icmp6InDestUnreachs)[type-ICMPV6_DEST_UNREACH]++;
		if (idev)
			(&idev->stats.icmpv6[smp_processor_id()*2].Icmp6InDestUnreachs)[type-ICMPV6_DEST_UNREACH]++;
	} else if (type >= ICMPV6_ECHO_REQUEST && type <= NDISC_REDIRECT) {
		(&icmpv6_statistics[smp_processor_id()*2].Icmp6InEchos)[type-ICMPV6_ECHO_REQUEST]++;
		if (idev)
			(&idev->stats.icmpv6[smp_processor_id()*2].Icmp6InEchos)[type-ICMPV6_ECHO_REQUEST]++;
	}

	switch (type) {
	case ICMPV6_ECHO_REQUEST:
		icmpv6_echo_reply(skb);
		break;

	case ICMPV6_ECHO_REPLY:
		/* we coulnd't care less */
		break;

	case ICMPV6_PKT_TOOBIG:
		/* BUGGG_FUTURE: if packet contains rthdr, we cannot update
		   standard destination cache. Seems, only "advanced"
		   destination cache will allow to solve this problem
		   --ANK (980726)
		 */
		if (!pskb_may_pull(skb, sizeof(struct ipv6hdr)))
			goto discard_it;
		hdr = (struct icmp6hdr *) skb->h.raw;
		orig_hdr = (struct ipv6hdr *) (hdr + 1);
		rt6_pmtu_discovery(&orig_hdr->daddr, &orig_hdr->saddr, dev,
				   ntohl(hdr->icmp6_mtu));

		/*
		 *	Drop through to notify
		 */

	case ICMPV6_DEST_UNREACH:
		if (hdr->icmp6_code == ICMPV6_ADM_PROHIBITED)
			ICMP6_INC_STATS_BH(idev,Icmp6InAdminProhibs);
	case ICMPV6_TIME_EXCEED:
	case ICMPV6_PARAMPROB:
		icmpv6_notify(skb, type, hdr->icmp6_code, hdr->icmp6_mtu);
		break;

	case NDISC_ROUTER_SOLICITATION:
	case NDISC_ROUTER_ADVERTISEMENT:
	case NDISC_NEIGHBOUR_SOLICITATION:
	case NDISC_NEIGHBOUR_ADVERTISEMENT:
	case NDISC_REDIRECT:
		if (skb_is_nonlinear(skb) &&
		    skb_linearize(skb, GFP_ATOMIC) != 0) {
			kfree_skb(skb);
			return 0;
		}

		ndisc_rcv(skb);
		break;

	case ICMPV6_MGM_QUERY:
		igmp6_event_query(skb);
		break;

	case ICMPV6_MGM_REPORT:
		igmp6_event_report(skb);
		break;

	case ICMPV6_MGM_REDUCTION:
		break;

	case ICMPV6_NI_QUERY:
	{
#ifdef CONFIG_IPV6_NODEINFO
#ifdef CONFIG_SYSCTL
		int config_accept_ni = ipv6_devconf.accept_ni;
		if (!config_accept_ni)
			break;
		icmpv6_ni_reply(skb, config_accept_ni);
#else
		icmpv6_ni_reply(skb, 1);
#endif
#endif
		break;
	}

	case ICMPV6_NI_REPLY:
		break;

	default:
#ifdef CONFIG_IPV6_DEBUG
		if (net_ratelimit()) {
			char abuf[64];
			in6_ntop(saddr, abuf);
			printk(KERN_DEBUG "icmpv6: msg of unknown type %d code %d from %s\n", type, hdr->icmp6_code, abuf);
		}
#endif

		/* informational */
		if (type & ICMPV6_INFOMSG_MASK)
			break;

		/* 
		 * error of unkown type. 
		 * must pass to upper level 
		 */

		icmpv6_notify(skb, type, hdr->icmp6_code, hdr->icmp6_mtu);
	};
	if (idev)
		in6_dev_put(idev);
	kfree_skb(skb);
	return 0;

discard_it:
	ICMP6_INC_STATS_BH(idev,Icmp6InErrors);
	if (idev)
		in6_dev_put(idev);
	kfree_skb(skb);
	return 0;
}

int __init icmpv6_init(struct net_proto_family *ops)
{
	struct sock *sk;
	int err, i, j;

	for (i = 0; i < NR_CPUS; i++) {
		icmpv6_socket_cpu(i) = sock_alloc();
		if (icmpv6_socket_cpu(i) == NULL) {
			printk(KERN_ERR
			       "Failed to create the ICMP6 control socket.\n");
			err = -1;
			goto fail;
		}
		icmpv6_socket_cpu(i)->inode->i_uid = 0;
		icmpv6_socket_cpu(i)->inode->i_gid = 0;
		icmpv6_socket_cpu(i)->type = SOCK_RAW;

		if ((err = ops->create(icmpv6_socket_cpu(i), IPPROTO_ICMPV6)) < 0) {
			printk(KERN_ERR
			       "Failed to initialize the ICMP6 control socket "
			       "(err %d).\n",
			       err);
			goto fail;
		}

		sk = icmpv6_socket_cpu(i)->sk;
		sk->allocation = GFP_ATOMIC;
		sk->sndbuf = SK_WMEM_MAX*2;
		sk->prot->unhash(sk);
	}

	inet6_add_protocol(&icmpv6_protocol);

#ifdef CONFIG_IPV6_NODEINFO
	down_write(&icmpv6_sethostname_hook_sem);
	icmpv6_sethostname_hook = __icmpv6_sethostname_hook;
	up_write(&icmpv6_sethostname_hook_sem);
#endif

	return 0;
fail:
	for (j = 0; j < i; j++) {
		sock_release(icmpv6_socket_cpu(j));
		icmpv6_socket_cpu(j) = NULL;
	}
	return err;
}

void icmpv6_cleanup(void)
{
	int i;

#ifdef CONFIG_IPV6_NODEINFO
	down_write(&icmpv6_sethostname_hook_sem);
	icmpv6_sethostname_hook = NULL;
	up_write(&icmpv6_sethostname_hook_sem);
#endif

	for (i = 0; i < NR_CPUS; i++) {
		sock_release(icmpv6_socket_cpu(i));
		icmpv6_socket_cpu(i) = NULL;
	}
	inet6_del_protocol(&icmpv6_protocol);
}

static struct icmp6_err {
	int err;
	int fatal;
} tab_unreach[] = {
	{ ENETUNREACH,	0},	/* NOROUTE		*/
	{ EACCES,	1},	/* ADM_PROHIBITED	*/
	{ EHOSTUNREACH,	0},	/* Was NOT_NEIGHBOUR, now reserved */
	{ EHOSTUNREACH,	0},	/* ADDR_UNREACH		*/
	{ ECONNREFUSED,	1},	/* PORT_UNREACH		*/
};

int icmpv6_err_convert(int type, int code, int *err)
{
	int fatal = 0;

	*err = EPROTO;

	switch (type) {
	case ICMPV6_DEST_UNREACH:
		fatal = 1;
		if (code <= ICMPV6_PORT_UNREACH) {
			*err  = tab_unreach[code].err;
			fatal = tab_unreach[code].fatal;
		}
		break;

	case ICMPV6_PKT_TOOBIG:
		*err = EMSGSIZE;
		break;
		
	case ICMPV6_PARAMPROB:
		*err = EPROTO;
		fatal = 1;
		break;

	case ICMPV6_TIME_EXCEED:
		*err = EHOSTUNREACH;
		break;
	};

	return fatal;
}

#ifdef CONFIG_SYSCTL
ctl_table ipv6_icmp_table[] = {
	{NET_IPV6_ICMP_RATELIMIT, "ratelimit",
	&sysctl_icmpv6_time, sizeof(int), 0644, NULL, &proc_dointvec},
	{0},
};
#endif

