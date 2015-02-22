#ifndef _NET_IPV6_TUNNEL_H
#define _NET_IPV6_TUNNEL_H

#include <linux/ipv6.h>
#include <linux/netdevice.h>
#include <linux/ipv6_tunnel.h>
#include <linux/skbuff.h>
#include <asm/atomic.h>

#define IPV6_TUNNEL_MAX 128

struct ipv6_tunnel {
	struct ipv6_tunnel *next; /* next tunnel in current list */
 	struct net_device *dev; /* virtual device associated with tunnel */
	struct net_device_stats stat; /* statistics for tunnel device */
	int recursion; /* depth of hard_start_xmit recursion */
	struct ipv6_tunnel_parm parms; /* tunnel configuration paramters */
	struct flowi fl; /* flowi template for xmit */
	atomic_t refcnt; /* number of identical tunnels used by kernel */
};

#define IPV6_TUNNEL_PRE_ENCAP 0
#define IPV6_TUNNEL_PRE_DECAP 1
#define IPV6_TUNNEL_MAXHOOKS 2

#define IPV6_TUNNEL_DROP 0
#define IPV6_TUNNEL_ACCEPT 1
#define IPV6_TUNNEL_STOLEN 2

typedef int ipv6_tunnel_hookfn(struct ipv6_tunnel *t, 
			       struct sk_buff *skb, 
			       __u32 flags);

struct ipv6_tunnel_hook_ops {
	struct list_head list;
	unsigned int hooknum;
	int priority;
	ipv6_tunnel_hookfn *hook;
};

enum ipv6_tunnel_hook_priorities {
	IPV6_TUNNEL_PRI_FIRST = INT_MIN,
	IPV6_TUNNEL_PRI_LAST = INT_MAX
};

#ifdef __KERNEL__
extern struct ipv6_tunnel *
ipv6_ipv6_tunnel_lookup(struct in6_addr *remote, struct in6_addr *local);

extern int ipv6_ipv6_kernel_tunnel_add(struct ipv6_tunnel_parm *p);

extern int ipv6_ipv6_kernel_tunnel_del(struct ipv6_tunnel *t);

extern unsigned int ipv6_ipv6_tunnel_inc_max_kdev_count(unsigned int n);

extern unsigned int ipv6_ipv6_tunnel_dec_max_kdev_count(unsigned int n);

extern unsigned int ipv6_ipv6_tunnel_inc_min_kdev_count(unsigned int n);

extern unsigned int ipv6_ipv6_tunnel_dec_min_kdev_count(unsigned int n);

extern void 
ipv6_ipv6_tunnel_register_hook(struct ipv6_tunnel_hook_ops *reg);

extern void 
ipv6_ipv6_tunnel_unregister_hook(struct ipv6_tunnel_hook_ops *reg);
#endif
#endif
