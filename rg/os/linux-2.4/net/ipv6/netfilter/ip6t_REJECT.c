/*
 * This is a module which is used for rejecting packets.
 * 	Added support for customized reject packets (Jozsef Kadlecsik).
 * Sun 12 Nov 2000
 * 	Port to IPv6 / ip6tables (Harald Welte <laforge@gnumonks.org>)
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmpv6.h>
#include <net/tcp.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_REJECT.h>

#if 1
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#if 0
/* Send RST reply */
static void send_reset(struct sk_buff *oldskb)
{
	struct sk_buff *nskb;
	struct tcphdr *otcph, *tcph;
	struct rtable *rt;
	unsigned int otcplen;
	int needs_ack;

	/* IP header checks: fragment, too short. */
	if (oldskb->nh.iph->frag_off & htons(IP_OFFSET)
	    || oldskb->len < (oldskb->nh.iph->ihl<<2) + sizeof(struct tcphdr))
		return;

	otcph = (struct tcphdr *)((u_int32_t*)oldskb->nh.iph + oldskb->nh.iph->ihl);
	otcplen = oldskb->len - oldskb->nh.iph->ihl*4;

	/* No RST for RST. */
	if (otcph->rst)
		return;

	/* Check checksum. */
	if (tcp_v4_check(otcph, otcplen, oldskb->nh.iph->saddr,
			 oldskb->nh.iph->daddr,
			 csum_partial((char *)otcph, otcplen, 0)) != 0)
		return;

	/* Copy skb (even if skb is about to be dropped, we can't just
           clone it because there may be other things, such as tcpdump,
           interested in it) */
	nskb = skb_copy(oldskb, GFP_ATOMIC);
	if (!nskb)
		return;

	/* This packet will not be the same as the other: clear nf fields */
	nf_conntrack_put(nskb->nfct);
	nskb->nfct = NULL;
	nskb->nfcache = 0;
#ifdef CONFIG_NETFILTER_DEBUG
	nskb->nf_debug = 0;
#endif

	tcph = (struct tcphdr *)((u_int32_t*)nskb->nh.iph + nskb->nh.iph->ihl);

	nskb->nh.iph->daddr = xchg(&nskb->nh.iph->saddr, nskb->nh.iph->daddr);
	tcph->source = xchg(&tcph->dest, tcph->source);

	/* Truncate to length (no data) */
	tcph->doff = sizeof(struct tcphdr)/4;
	skb_trim(nskb, nskb->nh.iph->ihl*4 + sizeof(struct tcphdr));
	nskb->nh.iph->tot_len = htons(nskb->len);

	if (tcph->ack) {
		needs_ack = 0;
		tcph->seq = otcph->ack_seq;
		tcph->ack_seq = 0;
	} else {
		needs_ack = 1;
		tcph->ack_seq = htonl(ntohl(otcph->seq) + otcph->syn + otcph->fin
				      + otcplen - (otcph->doff<<2));
		tcph->seq = 0;
	}

	/* Reset flags */
	((u_int8_t *)tcph)[13] = 0;
	tcph->rst = 1;
	tcph->ack = needs_ack;

	tcph->window = 0;
	tcph->urg_ptr = 0;

	/* Adjust TCP checksum */
	tcph->check = 0;
	tcph->check = tcp_v4_check(tcph, sizeof(struct tcphdr),
				   nskb->nh.iph->saddr,
				   nskb->nh.iph->daddr,
				   csum_partial((char *)tcph,
						sizeof(struct tcphdr), 0));

	/* Adjust IP TTL, DF */
	nskb->nh.iph->ttl = MAXTTL;
	/* Set DF, id = 0 */
	nskb->nh.iph->frag_off = htons(IP_DF);
	nskb->nh.iph->id = 0;

	/* Adjust IP checksum */
	nskb->nh.iph->check = 0;
	nskb->nh.iph->check = ip_fast_csum((unsigned char *)nskb->nh.iph, 
					   nskb->nh.iph->ihl);

	/* Routing */
	if (ip_route_output(&rt, nskb->nh.iph->daddr, nskb->nh.iph->saddr,
			    RT_TOS(nskb->nh.iph->tos) | RTO_CONN,
			    0) != 0)
		goto free_nskb;

	dst_release(nskb->dst);
	nskb->dst = &rt->u.dst;

	/* "Never happens" */
	if (nskb->len > nskb->dst->pmtu)
		goto free_nskb;

	NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, nskb, NULL, nskb->dst->dev,
		ip_finish_output);
	return;

 free_nskb:
	kfree_skb(nskb);
}
#endif

static unsigned int reject6_target(struct sk_buff **pskb,
			   unsigned int hooknum,
			   const struct net_device *in,
			   const struct net_device *out,
			   const void *targinfo,
			   void *userinfo)
{
	const struct ip6t_reject_info *reject = targinfo;

	/* WARNING: This code causes reentry within ip6tables.
	   This means that the ip6tables jump stack is now crap.  We
	   must return an absolute verdict. --RR */
	DEBUGP("REJECTv6: calling icmpv6_send\n");
    	switch (reject->with) {
    	case IP6T_ICMP6_NO_ROUTE:
    		icmpv6_send(*pskb, ICMPV6_DEST_UNREACH, ICMPV6_NOROUTE, 0, out);
    		break;
    	case IP6T_ICMP6_ADM_PROHIBITED:
    		icmpv6_send(*pskb, ICMPV6_DEST_UNREACH, ICMPV6_ADM_PROHIBITED, 0, out);
    		break;
    	case IP6T_ICMP6_NOT_NEIGHBOUR:
    		icmpv6_send(*pskb, ICMPV6_DEST_UNREACH, ICMPV6_NOT_NEIGHBOUR, 0, out);
    		break;
    	case IP6T_ICMP6_ADDR_UNREACH:
    		icmpv6_send(*pskb, ICMPV6_DEST_UNREACH, ICMPV6_ADDR_UNREACH, 0, out);
    		break;
    	case IP6T_ICMP6_PORT_UNREACH:
    		icmpv6_send(*pskb, ICMPV6_DEST_UNREACH, ICMPV6_PORT_UNREACH, 0, out);
    		break;
#if 0
    	case IPT_ICMP_ECHOREPLY: {
		struct icmp6hdr *icmph  = (struct icmphdr *)
			((u_int32_t *)(*pskb)->nh.iph + (*pskb)->nh.iph->ihl);
		unsigned int datalen = (*pskb)->len - (*pskb)->nh.iph->ihl * 4;

		/* Not non-head frags, or truncated */
		if (((ntohs((*pskb)->nh.iph->frag_off) & IP_OFFSET) == 0)
		    && datalen >= 4) {
			/* Usually I don't like cut & pasting code,
                           but dammit, my party is starting in 45
                           mins! --RR */
			struct icmp_bxm icmp_param;

			icmp_param.icmph=*icmph;
			icmp_param.icmph.type=ICMP_ECHOREPLY;
			icmp_param.data_ptr=(icmph+1);
			icmp_param.data_len=datalen;
			icmp_reply(&icmp_param, *pskb);
		}
	}
	break;
	case IPT_TCP_RESET:
		send_reset(*pskb);
		break;
#endif
	default:
		printk(KERN_WARNING "REJECTv6: case %u not handled yet\n", reject->with);
		break;
	}

	return NF_DROP;
}

static inline int find_ping_match(const struct ip6t_entry_match *m)
{
	const struct ip6t_icmp *icmpinfo = (const struct ip6t_icmp *)m->data;

	if (strcmp(m->u.kernel.match->name, "icmp6") == 0
	    && icmpinfo->type == ICMPV6_ECHO_REQUEST
	    && !(icmpinfo->invflags & IP6T_ICMP_INV))
		return 1;

	return 0;
}

static int check(const char *tablename,
		 const struct ip6t_entry *e,
		 void *targinfo,
		 unsigned int targinfosize,
		 unsigned int hook_mask)
{
 	const struct ip6t_reject_info *rejinfo = targinfo;

 	if (targinfosize != IP6T_ALIGN(sizeof(struct ip6t_reject_info))) {
  		DEBUGP("REJECTv6: targinfosize %u != 0\n", targinfosize);
  		return 0;
  	}

	/* Only allow these for packet filtering. */
	if (strcmp(tablename, "filter") != 0) {
		DEBUGP("REJECTv6: bad table `%s'.\n", tablename);
		return 0;
	}
	if ((hook_mask & ~((1 << NF_IP6_LOCAL_IN)
			   | (1 << NF_IP6_FORWARD)
			   | (1 << NF_IP6_LOCAL_OUT))) != 0) {
		DEBUGP("REJECTv6: bad hook mask %X\n", hook_mask);
		return 0;
	}

	if (rejinfo->with == IP6T_ICMP6_ECHOREPLY) {
		/* Must specify that it's an ICMP ping packet. */
		if (e->ipv6.proto != IPPROTO_ICMPV6
		    || (e->ipv6.invflags & IP6T_INV_PROTO)) {
			DEBUGP("REJECTv6: ECHOREPLY illegal for non-icmp\n");
			return 0;
		}
		/* Must contain ICMP match. */
		if (IP6T_MATCH_ITERATE(e, find_ping_match) == 0) {
			DEBUGP("REJECTv6: ECHOREPLY illegal for non-ping\n");
			return 0;
		}
	} else if (rejinfo->with == IP6T_TCP_RESET) {
		/* Must specify that it's a TCP packet */
		if (e->ipv6.proto != IPPROTO_TCP
		    || (e->ipv6.invflags & IP6T_INV_PROTO)) {
			DEBUGP("REJECTv6: TCP_RESET illegal for non-tcp\n");
			return 0;
		}
	}

	return 1;
}

static struct ip6t_target ip6t_reject_reg
= { { NULL, NULL }, "REJECT", reject6_target, check, NULL, THIS_MODULE };

static int __init init(void)
{
	if (ip6t_register_target(&ip6t_reject_reg))
		return -EINVAL;
	return 0;
}

static void __exit fini(void)
{
	ip6t_unregister_target(&ip6t_reject_reg);
}

module_init(init);
module_exit(fini);
