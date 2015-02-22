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
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/byteorder.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/inet.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/smp.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <net/ipv6.h>
#include <net/sadb.h>
#include <net/spd.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/snmp.h>  
#include <net/ipsec6_utils.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/icmpv6.h>
#include <linux/ipsec.h>
#include <linux/ipsec6.h>
#include <net/pfkeyv2.h> /* sa proto type */
#include <net/pfkey.h>


static int ipsec6_output_check_core(struct selector *selector, struct ipsec_sp **policy_ptr)
{
	int error = 0;
	struct ipsec_sp *policy = NULL;
	int result = IPSEC_ACTION_BYPASS; 	/* default */

	IPSEC6_DEBUG("called\n");

	if (!selector) {
		IPSEC6_DEBUG("selector is NULL\n");
		error = -EINVAL;
		goto err;
	}
	
	policy = spd_get(selector);
	if (!policy) { /* not match ! */
		IPSEC6_DEBUG("no policy exists.\n");
		result = IPSEC_ACTION_BYPASS;
		goto err;
	}

	read_lock_bh(&policy->lock);
	if (policy->policy_action == IPSEC_POLICY_DROP) {
		result = IPSEC_ACTION_DROP;
		read_unlock_bh(&policy->lock);
		goto err;
	}  else if (policy->policy_action == IPSEC_POLICY_BYPASS) {
		result = IPSEC_ACTION_BYPASS;
		read_unlock_bh(&policy->lock);
		goto err;
	}
	
	/* policy must then be to apply ipsec */
	if (policy->auth_sa_idx) {
		if (policy->auth_sa_idx->sa) {
			read_lock_bh(&policy->auth_sa_idx->sa->lock);
			switch (policy->auth_sa_idx->sa->state) {
			case SADB_SASTATE_MATURE:
			case SADB_SASTATE_DYING:
				result |= IPSEC_ACTION_AUTH;
				break;
			default:
				result = IPSEC_ACTION_DROP;
			}
			read_unlock_bh(&policy->auth_sa_idx->sa->lock);
		} else {
			read_unlock_bh(&policy->lock);
			write_lock_bh(&policy->lock);
			/* check and see if another process attached an sa to
			 * the policy while we were acquiring the write lock.
			 * Note: refcnt guarantees policy is still in memory.
			 */
			if (!policy->auth_sa_idx->sa)
				policy->auth_sa_idx->sa = sadb_find_by_sa_index(policy->auth_sa_idx);
			write_unlock_bh(&policy->lock); 
			read_lock_bh(&policy->lock);
			if (policy->auth_sa_idx->sa) 
				result |= IPSEC_ACTION_AUTH;
		 	else 
				/* SADB_ACQUIRE message should be thrown up to KMd */
				result = IPSEC_ACTION_DROP;
		}
	}

	if (policy->esp_sa_idx) {
		if (policy->esp_sa_idx->sa) {
			read_lock_bh(&policy->esp_sa_idx->sa->lock);
			switch (policy->esp_sa_idx->sa->state) {
			case SADB_SASTATE_MATURE:
			case SADB_SASTATE_DYING:
				result |= IPSEC_ACTION_ESP;
				break;
			default:
				result = IPSEC_ACTION_DROP;
			}
			read_unlock_bh(&policy->esp_sa_idx->sa->lock);
		} else {
			read_unlock_bh(&policy->lock);
			write_lock_bh(&policy->lock);
			/* check and see if another process attached an sa to
			 * the policy while we were acquiring the write lock.
			 * Note: refcnt guarantees policy is still in memory.
			 */
			if (!policy->esp_sa_idx->sa)
				policy->esp_sa_idx->sa = sadb_find_by_sa_index(policy->esp_sa_idx);
			write_unlock_bh(&policy->lock);
			read_lock_bh(&policy->lock);
			if (policy->esp_sa_idx->sa) 
				result |= IPSEC_ACTION_ESP;
			 else 
				/* SADB_ACQUIRE message should be thrown up to KMd */
				result = IPSEC_ACTION_DROP;
		}
	}

	if (policy->comp_sa_idx) {
		if (policy->comp_sa_idx->sa) {
			read_lock_bh(&policy->comp_sa_idx->sa->lock);
			switch (policy->comp_sa_idx->sa->state) {
			case SADB_SASTATE_MATURE:
			case SADB_SASTATE_DYING:
				result |= IPSEC_ACTION_COMP;
				break;
			default:
				result |= IPSEC_ACTION_DROP;
			}
			read_unlock_bh(&policy->comp_sa_idx->sa->lock);
		} else {
			read_unlock_bh(&policy->lock);
			write_lock_bh(&policy->lock);
			/* check and see if another process attached an sa to
			 * the policy while we were acquiring the write lock.
			 * Note: refcnt guarantees policy is still in memory.
			 */
			if (!policy->comp_sa_idx->sa)
				policy->comp_sa_idx->sa = sadb_find_by_sa_index(policy->comp_sa_idx);
			write_unlock_bh(&policy->lock);
			read_lock_bh(&policy->lock);
			if (policy->comp_sa_idx->sa) 
				result |= IPSEC_ACTION_COMP;
			else 
				/* SADB_ACUIRE message should be thrown up to KMd */
				result |= IPSEC_ACTION_DROP;
		}
	}

	*policy_ptr= policy;
	read_unlock_bh(&policy->lock);
	IPSEC6_DEBUG("end\n");	

err:
	return result;
}

int ipsec6_output_check(struct sock *sk, struct flowi *fl, const u8 *data, struct ipsec_sp **policy_ptr)
{
	struct in6_addr *saddr,*daddr;
	u16 sport,dport;
	unsigned char proto;
	struct selector selector;
	int result = IPSEC_ACTION_BYPASS; 	/* default */

	IPSEC6_DEBUG("called\n");
	if (!sk && !fl) {
		printk(KERN_ERR "flowi and sock are NULL\n");
		result = -EINVAL;
		goto err;
	}
	
	if (fl && fl->fl6_src) {
		saddr = fl->fl6_src; 
	} else {
		if (sk) {
			saddr = &sk->net_pinfo.af_inet6.saddr; 
		} else {
			result = -EINVAL;
			printk(KERN_ERR "sock is null\n");
			goto err;
		}
	}

	if (fl && fl->fl6_dst) {
		daddr = fl->fl6_dst; 
	} else {
		if (sk) {
			daddr = &sk->net_pinfo.af_inet6.daddr; 
		} else { 
			result = -EINVAL;
			printk(KERN_ERR "flowi and sock are NULL\n");
			goto err;
		}
	}

	if (fl) { 
		sport=fl->uli_u.ports.sport;
		dport=fl->uli_u.ports.dport;
		proto=fl->proto;
	} else if (sk) {
		sport=sk->sport;
		dport=sk->dport;
		proto=sk->protocol;
	} else {
		result = -EINVAL;
		printk(KERN_ERR "flowi and sock are NULL\n");
		goto err;
	}

	/* for ISKAMP see RFC2408 */
	if (proto == IPPROTO_UDP && 
	    sport == __constant_htons(500) && dport == __constant_htons(500)) {
		result = IPSEC_ACTION_BYPASS; 	/* default */
		goto err;
	}

	/* XXX have to decide to the policy of ICMP messages -mk*/
	if (proto != IPPROTO_TCP && proto != IPPROTO_UDP) {
		sport = 0;
		dport = 0;
	}

	/* XXX config  port policy */
	memset(&selector, 0, sizeof(struct selector));


#ifdef CONFIG_IPV6_IPSEC_TUNNEL
	if (proto == IPPROTO_IPV6) {
		selector.mode = IPSEC_MODE_TUNNEL;
	} else {
#endif
		((struct sockaddr_in6 *)&selector.src)->sin6_port = sport;	
		((struct sockaddr_in6 *)&selector.dst)->sin6_port = dport;	
		selector.proto = proto;
#ifdef CONFIG_IPV6_IPSEC_TUNNEL
	}

	if (selector.mode == IPSEC_MODE_TUNNEL) {
		const struct ipv6hdr *h = (struct ipv6hdr*) data;
		((struct sockaddr_in6 *)&selector.src)->sin6_family = AF_INET6;
		ipv6_addr_copy(&((struct sockaddr_in6 *)&selector.src)->sin6_addr,
			       &h->saddr);
		((struct sockaddr_in6 *)&selector.dst)->sin6_family = AF_INET6;
		ipv6_addr_copy(&((struct sockaddr_in6 *)&selector.dst)->sin6_addr,
			       &h->daddr);
	} else { /* IPSEC_MODE_TRANSPORT */
#endif
		((struct sockaddr_in6 *)&selector.src)->sin6_family = AF_INET6;
		ipv6_addr_copy(&((struct sockaddr_in6 *)&selector.src)->sin6_addr,
			       saddr);
		((struct sockaddr_in6 *)&selector.dst)->sin6_family = AF_INET6;
		ipv6_addr_copy(&((struct sockaddr_in6 *)&selector.dst)->sin6_addr,
			       daddr);
#ifdef CONFIG_IPV6_IPSEC_TUNNEL
	}
#endif

	selector.prefixlen_d = 128;
	selector.prefixlen_s = 128;


#ifdef CONFIG_IPSEC_DEBUG
	{
		char buf[64];
		IPSEC6_DEBUG("original src addr: %s\n", in6_ntop(saddr, buf));
		IPSEC6_DEBUG("original src port: %u\n", ntohs(sport));
		IPSEC6_DEBUG("original dst addr: %s\n", in6_ntop(daddr, buf));
		IPSEC6_DEBUG("original dst port: %u\n", ntohs(dport));

		IPSEC6_DEBUG("selector src addr: %s\n", 
				in6_ntop( &((struct sockaddr_in6 *)&selector.src)->sin6_addr, buf));
		IPSEC6_DEBUG("selector src port: %u\n", 
				ntohs(((struct sockaddr_in6 *)&selector.src)->sin6_port));
		IPSEC6_DEBUG("selector dst addr: %s\n", 
				in6_ntop( &((struct sockaddr_in6 *)&selector.dst)->sin6_addr, buf));
		IPSEC6_DEBUG("selector dst port: %u\n", 
				ntohs(((struct sockaddr_in6 *)&selector.dst)->sin6_port));
		IPSEC6_DEBUG("selector proto: %u\n", selector.proto);
	}
#endif /* CONFIG_IPSEC_DEBUG */

	result = ipsec6_output_check_core(&selector, policy_ptr);

 err:
		return result;
}


int ipsec6_ndisc_check(struct in6_addr *saddr, struct in6_addr *daddr, struct ipsec_sp **policy_ptr)
{
	struct selector selector;
	int result = IPSEC_ACTION_BYPASS; 	/* default */

	IPSEC6_DEBUG("called\n");


	/* XXX config  port policy */
	memset(&selector, 0, sizeof(struct selector));

	((struct sockaddr_in6 *)&selector.src)->sin6_family = AF_INET6;
	ipv6_addr_copy(&((struct sockaddr_in6 *)&selector.src)->sin6_addr,
		       saddr);
	((struct sockaddr_in6 *)&selector.dst)->sin6_family = AF_INET6;
	ipv6_addr_copy(&((struct sockaddr_in6 *)&selector.dst)->sin6_addr,
		       daddr);
	selector.proto = IPPROTO_ICMPV6;
	selector.prefixlen_d = 128;
	selector.prefixlen_s = 128;

#ifdef CONFIG_IPSEC_DEBUG
	{
		char buf[64];
		IPSEC6_DEBUG("original dst addr: %s\n", in6_ntop(daddr, buf));
		IPSEC6_DEBUG("original src addr: %s\n", in6_ntop(saddr, buf));

		IPSEC6_DEBUG("selector dst addr: %s\n", 
				in6_ntop( &((struct sockaddr_in6 *)&selector.dst)->sin6_addr, buf));
		IPSEC6_DEBUG("selector src addr: %s\n", 
				in6_ntop( &((struct sockaddr_in6 *)&selector.src)->sin6_addr, buf));
		IPSEC6_DEBUG("selector proto: %u\n", selector.proto);
	}
#endif /* CONFIG_IPSEC_DEBUG */

	result = ipsec6_output_check_core(&selector, policy_ptr);

	return result;
}

/* XXX XXX XXX  -mk */
int ipsec6_forward_check(struct sk_buff* skb) 
{
	int rtn = 0;

	if (!skb) {
		printk(KERN_ERR "ipsec6_forward_check: skb is null.\n");
		rtn = -EINVAL;
	}

	/* AH */
	/* TODO: lookup spd using by dst addr, dst port, src addr, src port */
	
	/* ESP */
	/* TODO: lookup spd using by dst addr, dst port, src addr, src port */

	return rtn;
}

