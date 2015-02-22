/*
 *	IPv6 BSD socket options interface
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	Based on linux/net/ipv4/ip_sockglue.c
 *
 *	$Id: ipv6_sockglue.c,v 1.1.1.1 2007/05/07 23:29:16 jungo Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *	FIXME: Make the setsockopt code POSIX compliant: That is
 *
 *	o	Return an optlen of the truncated length if need be
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/netfilter.h>

#include <net/sock.h>
#include <net/snmp.h>
#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/protocol.h>
#include <net/transp_v6.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/inet_common.h>
#include <net/tcp.h>
#include <net/udp.h>

#include <asm/uaccess.h>

#ifdef CONFIG_IPV6_IPSEC
#include <linux/ipsec.h>
#include <linux/ipsec6.h>
#include <net/ipsec6_utils.h>
#endif /* CONFIG_IPV6_IPSEC */

struct ipv6_mib ipv6_statistics[NR_CPUS*2];

struct packet_type ipv6_packet_type =
{
	__constant_htons(ETH_P_IPV6), 
	NULL,					/* All devices */
	ipv6_rcv,
	(void*)1,
	NULL
};

/*
 *	addrconf module should be notifyed of a device going up
 */
static struct notifier_block ipv6_dev_notf = {
	addrconf_notify,
	NULL,
	0
};

struct ip6_ra_chain *ip6_ra_chain;
rwlock_t ip6_ra_lock = RW_LOCK_UNLOCKED;

int ip6_ra_control(struct sock *sk, int sel, void (*destructor)(struct sock *))
{
	struct ip6_ra_chain *ra, *new_ra, **rap;

	/* RA packet may be delivered ONLY to IPPROTO_RAW socket */
	if (sk->type != SOCK_RAW || sk->num != IPPROTO_RAW)
		return -EINVAL;

	new_ra = (sel>=0) ? kmalloc(sizeof(*new_ra), GFP_KERNEL) : NULL;

	write_lock_bh(&ip6_ra_lock);
	for (rap = &ip6_ra_chain; (ra=*rap) != NULL; rap = &ra->next) {
		if (ra->sk == sk) {
			if (sel>=0) {
				write_unlock_bh(&ip6_ra_lock);
				if (new_ra)
					kfree(new_ra);
				return -EADDRINUSE;
			}

			*rap = ra->next;
			write_unlock_bh(&ip6_ra_lock);

			if (ra->destructor)
				ra->destructor(sk);
			sock_put(sk);
			kfree(ra);
			return 0;
		}
	}
	if (new_ra == NULL) {
		write_unlock_bh(&ip6_ra_lock);
		return -ENOBUFS;
	}
	new_ra->sk = sk;
	new_ra->sel = sel;
	new_ra->destructor = destructor;
	new_ra->next = ra;
	*rap = new_ra;
	sock_hold(sk);
	write_unlock_bh(&ip6_ra_lock);
	return 0;
}


int ipv6_setsockopt(struct sock *sk, int level, int optname, char *optval, 
		    int optlen)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	int val, valbool;
	int retv = -ENOPROTOOPT;

	if(level==SOL_IP && sk->type != SOCK_RAW)
		return udp_prot.setsockopt(sk, level, optname, optval, optlen);

	if(level!=SOL_IPV6)
		goto out;

	if (optlen < 0)
		goto e_inval;

	if (optval == NULL)
		val=0;
	else if (get_user(val, (int *) optval) && get_user(val, (u8 *) optval))
		return -EFAULT;

	valbool = (val!=0);

	lock_sock(sk);

	switch (optname) {

	/* XXX: IPV6_ADDRFORM is deprecated */
	case IPV6_ADDRFORM:
		if (val == PF_INET) {
			struct ipv6_txoptions *opt;
			struct sk_buff *pktopt;

			if (sk->protocol != IPPROTO_UDP &&
			    sk->protocol != IPPROTO_TCP)
				break;

			if (sk->state != TCP_ESTABLISHED) {
				retv = -ENOTCONN;
				break;
			}

			if (ipv6_only_sock(sk) ||
			    !(ipv6_addr_type(&np->daddr) & IPV6_ADDR_MAPPED)) {
				retv = -EADDRNOTAVAIL;
				break;
			}

			fl6_free_socklist(sk);
			ipv6_sock_mc_close(sk);

			if (sk->protocol == IPPROTO_TCP) {
				struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

				local_bh_disable();
				sock_prot_dec_use(sk->prot);
				sock_prot_inc_use(&tcp_prot);
				local_bh_enable();
				sk->prot = &tcp_prot;
				tp->af_specific = &ipv4_specific;
				sk->socket->ops = &inet_stream_ops;
				sk->family = PF_INET;
				tcp_sync_mss(sk, tp->pmtu_cookie);
			} else {
				local_bh_disable();
				sock_prot_dec_use(sk->prot);
				sock_prot_inc_use(&udp_prot);
				local_bh_enable();
				sk->prot = &udp_prot;
				sk->socket->ops = &inet_dgram_ops;
				sk->family = PF_INET;
			}
			opt = xchg(&np->opt, NULL);
			if (opt)
				sock_kfree_s(sk, opt, opt->tot_len);
			pktopt = xchg(&np->pktoptions, NULL);
			if (pktopt)
				kfree_skb(pktopt);

			sk->destruct = inet_sock_destruct;
#ifdef INET_REFCNT_DEBUG
			atomic_dec(&inet6_sock_nr);
#endif
			MOD_DEC_USE_COUNT;
			retv = 0;
			break;
		}
		goto e_inval;

	case IPV6_PKTINFO:
		if (optlen != sizeof(int))
			goto e_inval;
		np->rxopt.bits.rxinfo = valbool;
		retv = 0;
		break;

	case IPV6_HOPLIMIT:
		if (optlen != sizeof(int))
			goto e_inval;
		np->rxopt.bits.rxhlim = valbool;
		retv = 0;
		break;

	case IPV6_RTHDR:
		if (optlen != sizeof(int))
			goto e_inval;
		if (val < 0 || val > 2)
			goto e_inval;
		np->rxopt.bits.srcrt = val;
		retv = 0;
		break;

	case IPV6_HOPOPTS:
		if (optlen != sizeof(int))
			goto e_inval;
		np->rxopt.bits.hopopts = valbool;
		retv = 0;
		break;

	case IPV6_AUTHHDR:
		if (optlen != sizeof(int))
			goto e_inval;
		np->rxopt.bits.authhdr = valbool;
		retv = 0;
		break;

	case IPV6_DSTOPTS:
		if (optlen != sizeof(int))
			goto e_inval;
		np->rxopt.bits.dstopts = valbool;
		retv = 0;
		break;

	case IPV6_FLOWINFO:
		if (optlen != sizeof(int))
			goto e_inval;
		np->rxopt.bits.rxflow = valbool;
		retv = 0;
		break;

	case IPV6_PKTOPTIONS:
	{
		struct ipv6_txoptions *opt = NULL;
		struct msghdr msg;
		struct flowi fl;
		int junk;

		fl.fl6_flowlabel = 0;
		fl.oif = sk->bound_dev_if;

		if (!optval || optlen == 0)
			goto update;

		/* 1K is probably excessive
		 * 1K is surely not enough, 2K per standard header is 16K.
		 */
		retv = -EINVAL;
		if (optlen > 64*1024)
			break;

		opt = sock_kmalloc(sk, sizeof(*opt) + optlen, GFP_KERNEL);
		retv = -ENOBUFS;
		if (opt == NULL)
			break;

		memset(opt, 0, sizeof(*opt));
		opt->tot_len = sizeof(*opt) + optlen;
		retv = -EFAULT;
		if (copy_from_user(opt+1, optval, optlen))
			goto done;

		msg.msg_controllen = optlen;
		msg.msg_control = (void*)(opt+1);

		retv = datagram_send_ctl(&msg, &fl, opt, &junk, &junk);
		if (retv)
			goto done;
update:
		retv = 0;
		if (sk->type == SOCK_STREAM) {
			if (opt) {
				struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
				if (!((1<<sk->state)&(TCPF_LISTEN|TCPF_CLOSE))
				    && sk->daddr != LOOPBACK4_IPV6) {
#ifdef CONFIG_IPV6_IPSEC
					struct ipsec_sp *policy_ptr = NULL;
					int action = ipsec6_output_check(sk,NULL,NULL,&policy_ptr);
#endif /* CONFIG_IPV6_IPSEC */
					tp->ext_header_len = opt->opt_flen + opt->opt_nflen;
#ifdef CONFIG_IPV6_IPSEC
					/* StS: Adapt ext_header_len according to AH/ESP size */

					if (action != IPSEC_ACTION_DROP) {
						printk(KERN_DEBUG "ipv6_setsockopt: ipsec6_output_check said DROP.\n");
						if (opt->auth) {
							action &=! IPSEC_ACTION_AUTH;
						} else {
							tp->ext_header_len += ipsec6_out_get_hdrsize(policy_ptr);
						}
					}
					ipsec6_out_finish(NULL, policy_ptr);
					/* StS end */
#endif /* CONFIG_IPV6_IPSEC */
					tcp_sync_mss(sk, tp->pmtu_cookie);
				}
			}
			opt = xchg(&np->opt, opt);
			sk_dst_reset(sk);
		} else {
			write_lock(&sk->dst_lock);
			opt = xchg(&np->opt, opt);
			write_unlock(&sk->dst_lock);
			sk_dst_reset(sk);
		}

done:
		if (opt)
			sock_kfree_s(sk, opt, opt->tot_len);
		break;
	}
	case IPV6_UNICAST_HOPS:
		if (optlen != sizeof(int))
			goto e_inval;
		if (val > 255 || val < -1)
			goto e_inval;
		np->hop_limit = val;
		retv = 0;
		break;

	case IPV6_MULTICAST_HOPS:
		if (optlen != sizeof(int))
			goto e_inval;
		if (sk->type == SOCK_STREAM)
			goto e_inval;
		if (val > 255 || val < -1)
			goto e_inval;
		np->mcast_hops = val;
		retv = 0;
		break;

	case IPV6_MULTICAST_LOOP:
		if (optlen != sizeof(int))
			goto e_inval;
		if (val != 0 && val != 1)
			goto e_inval;
		np->mc_loop = valbool;
		retv = 0;
		break;

	case IPV6_MULTICAST_IF:
		if (optlen != sizeof(int))
			goto e_inval;
		if (sk->type == SOCK_STREAM)
			goto e_inval;
		if (sk->bound_dev_if && sk->bound_dev_if != val)
			goto e_inval;

		if (__dev_get_by_index(val) == NULL) {
			retv = -ENODEV;
			break;
		}
		np->mcast_oif = val;
		retv = 0;
		break;
	case IPV6_JOIN_GROUP:
	case IPV6_LEAVE_GROUP:
	{
		struct ipv6_mreq mreq;

		if (optlen != sizeof(struct ipv6_mreq))
			goto e_inval;

		retv = -EFAULT;
		if (copy_from_user(&mreq, optval, sizeof(struct ipv6_mreq)))
			break;

		if (optname == IPV6_JOIN_GROUP)
			retv = ipv6_sock_mc_join(sk, mreq.ipv6mr_ifindex, &mreq.ipv6mr_multiaddr);
		else
			retv = ipv6_sock_mc_drop(sk, mreq.ipv6mr_ifindex, &mreq.ipv6mr_multiaddr);
		break;
	}
#ifdef CONFIG_IPV6_ANYCAST_GROUP
	case IPV6_JOIN_ANYCAST:
	case IPV6_LEAVE_ANYCAST:
	{
		struct ipv6_mreq mreq;

		if (optlen != sizeof(struct ipv6_mreq))
			goto e_inval;

		retv = -EFAULT;
		if (copy_from_user(&mreq, optval, sizeof(struct ipv6_mreq)))
			break;

		if (optname == IPV6_JOIN_ANYCAST)
			retv = ipv6_sock_ac_join(sk, mreq.ipv6mr_ifindex, &mreq.ipv6mr_acaddr);
		else
			retv = ipv6_sock_ac_drop(sk, mreq.ipv6mr_ifindex, &mreq.ipv6mr_acaddr);
		break;
	}
#endif
	case IPV6_ROUTER_ALERT:
		retv = ip6_ra_control(sk, val, NULL);
		break;
	case IPV6_MTU_DISCOVER:
		if (optlen != sizeof(int))
			goto e_inval;
		if (val<0 || val>2)
			goto e_inval;
		np->pmtudisc = val;
		retv = 0;
		break;
	case IPV6_MTU:
		if (optlen != sizeof(int))
			goto e_inval;
		if (val && val < IPV6_MIN_MTU)
			goto e_inval;
		np->frag_size = val;
		retv = 0;
		break;
	case IPV6_RECVERR:
		if (optlen != sizeof(int))
			goto e_inval;
		np->recverr = valbool;
		if (!val)
			skb_queue_purge(&sk->error_queue);
		retv = 0;
		break;
	case IPV6_FLOWINFO_SEND:
		if (optlen != sizeof(int))
			goto e_inval;
		np->sndflow = valbool;
		retv = 0;
		break;
	case IPV6_FLOWLABEL_MGR:
		retv = ipv6_flowlabel_opt(sk, optval, optlen);
		break;

	case IPV6_V6ONLY:
		if (optlen != sizeof(int))
			goto e_inval;
		if (sk->userlocks&SOCK_BINDADDR_LOCK)
			goto e_inval;
		np->ipv6only = valbool;
		retv = 0;
		break;

	case IPV6_RECVTCLASS:
		if (optlen != sizeof(int))
			goto e_inval;
		np->recvopt.tclass = valbool;
		retv = 0;
		break;

	case IPV6_TCLASS:
		if (optlen != sizeof(int))
			goto e_inval;
		np->tclass = val;
		retv = 0;
		break;

#ifdef CONFIG_IPV6_PRIVACY
	case IPV6_PRIVACY:
		if (optlen != sizeof(int))
			goto e_inval;
		if (val > 0)
			np->use_tempaddr = 1;
		else if (val < 0)
			np->use_tempaddr = -1;
		else
			np->use_tempaddr = 0;
		retv = 0;
		break;
#endif

#ifdef CONFIG_NETFILTER
	default:
		retv = nf_setsockopt(sk, PF_INET6, optname, optval, 
					    optlen);
		break;
#endif

	}
	release_sock(sk);

out:
	return retv;

e_inval:
	release_sock(sk);
	return -EINVAL;
}

int ipv6_getsockopt(struct sock *sk, int level, int optname, char *optval, 
		    int *optlen)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	int len;
	int val;

	if(level==SOL_IP && sk->type != SOCK_RAW)
		return udp_prot.getsockopt(sk, level, optname, optval, optlen);
	if(level!=SOL_IPV6)
		return -ENOPROTOOPT;
	if (get_user(len, optlen))
		return -EFAULT;
	if (optlen < 0)
		return -EINVAL;
	switch (optname) {
	case IPV6_PKTOPTIONS:
	{
		struct msghdr msg;
		struct sk_buff *skb;

		if (sk->type != SOCK_STREAM)
			return -ENOPROTOOPT;

		msg.msg_control = optval;
		msg.msg_controllen = len;
		msg.msg_flags = 0;

		lock_sock(sk);
		skb = np->pktoptions;
		if (skb)
			atomic_inc(&skb->users);
		release_sock(sk);

		if (skb) {
			int err = datagram_recv_ctl(sk, &msg, skb);
			kfree_skb(skb);
			if (err)
				return err;
		} else {
			if (np->rxopt.bits.rxinfo) {
				struct in6_pktinfo src_info;
				src_info.ipi6_ifindex = np->mcast_oif;
				ipv6_addr_copy(&src_info.ipi6_addr, &np->daddr);
				put_cmsg(&msg, SOL_IPV6, IPV6_PKTINFO, sizeof(src_info), &src_info);
			}
			if (np->rxopt.bits.rxhlim) {
				int hlim = np->mcast_hops;
				put_cmsg(&msg, SOL_IPV6, IPV6_HOPLIMIT, sizeof(hlim), &hlim);
			}
		}
		len -= msg.msg_controllen;
		return put_user(len, optlen);
	}

	case IPV6_MTU:
	{
		struct dst_entry *dst;
		val = 0;	
		lock_sock(sk);
		dst = sk_dst_get(sk);
		if (dst) {
			val = dst->pmtu;
			dst_release(dst);
		}
		release_sock(sk);
		if (!val)
			return -ENOTCONN;
		break;
	}

	case IPV6_PKTINFO:
		val = np->rxopt.bits.rxinfo;
		break;

	case IPV6_HOPLIMIT:
		val = np->rxopt.bits.rxhlim;
		break;

	case IPV6_RTHDR:
		val = np->rxopt.bits.srcrt;
		break;

	case IPV6_HOPOPTS:
		val = np->rxopt.bits.hopopts;
		break;

	case IPV6_AUTHHDR:
		val = np->rxopt.bits.authhdr;
		break;

	case IPV6_DSTOPTS:
		val = np->rxopt.bits.dstopts;
		break;

	case IPV6_FLOWINFO:
		val = np->rxopt.bits.rxflow;
		break;

	case IPV6_UNICAST_HOPS:
		val = np->hop_limit;
		break;

	case IPV6_MULTICAST_HOPS:
		val = np->mcast_hops;
		break;

	case IPV6_MULTICAST_LOOP:
		val = np->mc_loop;
		break;

	case IPV6_MULTICAST_IF:
		val = np->mcast_oif;
		break;

	case IPV6_MTU_DISCOVER:
		val = np->pmtudisc;
		break;

	case IPV6_RECVERR:
		val = np->recverr;
		break;

	case IPV6_FLOWINFO_SEND:
		val = np->sndflow;
		break;

	case IPV6_JOIN_GROUP:
	case IPV6_LEAVE_GROUP:
		return -EOPNOTSUPP;

	case IPV6_V6ONLY:
		val = np->ipv6only;
		break;

	case IPV6_RECVTCLASS:
                val = np->recvopt.tclass;
                break;

	case IPV6_TCLASS:
		val = np->tclass;
                break;

#ifdef CONFIG_IPV6_PRIVACY
	case IPV6_PRIVACY:
		val = np->use_tempaddr;
		break;
#endif

	default:
#ifdef CONFIG_NETFILTER
		lock_sock(sk);
		val = nf_getsockopt(sk, PF_INET6, optname, optval, 
				    &len);
		release_sock(sk);
		if (val >= 0)
			val = put_user(len, optlen);
		return val;
#else
		return -EINVAL;
#endif
	}
	len = min_t(unsigned int, sizeof(int), len);
	if(put_user(len, optlen))
		return -EFAULT;
	if(copy_to_user(optval,&val,len))
		return -EFAULT;
	return 0;
}

#if defined(MODULE) && defined(CONFIG_SYSCTL)

/*
 *	sysctl registration functions defined in sysctl_net_ipv6.c
 */

extern void ipv6_sysctl_register(void);
extern void ipv6_sysctl_unregister(void);
#endif

void __init ipv6_packet_init(void)
{
	dev_add_pack(&ipv6_packet_type);
}

void __init ipv6_netdev_notif_init(void)
{
	register_netdevice_notifier(&ipv6_dev_notf);
}

#ifdef MODULE
void ipv6_packet_cleanup(void)
{
	dev_remove_pack(&ipv6_packet_type);
}

void ipv6_netdev_notif_cleanup(void)
{
	unregister_netdevice_notifier(&ipv6_dev_notf);
}
#endif
