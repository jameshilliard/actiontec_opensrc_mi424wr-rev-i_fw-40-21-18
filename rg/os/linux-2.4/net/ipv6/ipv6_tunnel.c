/*
 *	IPv6 over IPv6 tunnel device
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Ville Nuorvala		<vnuorval@tml.hut.fi>	
 *
 *	$Id: ipv6_tunnel.c,v 1.1.1.1 2007/05/07 23:29:16 jungo Exp $
 *
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#define __NO_VERSION__
#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/icmpv6.h>
#include <linux/init.h>
#include <linux/route.h>
#include <linux/rtnetlink.h>
#include <linux/tqueue.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <net/sock.h>
#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/ipv6_tunnel.h>

/*
 * IPv6 tunnel socket for flow control 
 */

static struct socket *ipv6_socket;

static int ipv6_xmit_holder = -1;

static int ipv6_xmit_lock_bh(void)
{
	if (!spin_trylock(&ipv6_socket->sk->lock.slock)) {
		if (ipv6_xmit_holder == smp_processor_id())
			return -EAGAIN;
		spin_lock(&ipv6_socket->sk->lock.slock);
	}
	ipv6_xmit_holder = smp_processor_id();
	return 0;
}

static __inline__ int ipv6_xmit_lock(void)
{
	int ret;
	local_bh_disable();
	ret = ipv6_xmit_lock_bh();
	if (ret)
		local_bh_enable();
	return ret;
}

static void ipv6_xmit_unlock_bh(void)
{
	ipv6_xmit_holder = -1;
	spin_unlock(&ipv6_socket->sk->lock.slock);
}

static __inline__ void ipv6_xmit_unlock(void)
{
	ipv6_xmit_unlock_bh();
	local_bh_enable();
}

#define HASH_SIZE  32

#define HASH(addr) (((addr)->s6_addr16[0] ^ (addr)->s6_addr16[1] ^ \
	             (addr)->s6_addr16[2] ^ (addr)->s6_addr16[3] ^ \
                     (addr)->s6_addr16[4] ^ (addr)->s6_addr16[5] ^ \
	             (addr)->s6_addr16[6] ^ (addr)->s6_addr16[7]) & \
                    (HASH_SIZE - 1))

static int ipv6_ipv6_fb_tunnel_dev_init(struct net_device *dev);
static int ipv6_ipv6_tunnel_dev_init(struct net_device *dev);

/*
 * The ipv6-ipv6 tunnel fallback device
 */

static struct net_device ipv6_ipv6_fb_tunnel_dev = {
	"", 
	0, 
	0, 
	0, 
	0, 
	0, 
	0, 
	0, 
	0, 
	0, 
	NULL, 
	ipv6_ipv6_fb_tunnel_dev_init,
};


/*
 * The IPv6 IPv6 fallback tunnel 
 */

static struct ipv6_tunnel ipv6_ipv6_fb_tunnel = {
	NULL, 
	&ipv6_ipv6_fb_tunnel_dev, 
	{0, }, 
	0, 
	{"ip6tnl0", 0, IPPROTO_IPV6 },
};

static unsigned int max_kdev_count = 0;
static unsigned int min_kdev_count = 0;
static unsigned int kdev_count = 0;

/*
 * List of unused kernel tunnels
 */ 

static struct ipv6_tunnel *tunnels_kernel[1];

static struct ipv6_tunnel *tunnels_r_l[HASH_SIZE];
static struct ipv6_tunnel *tunnels_wc[1];
static struct ipv6_tunnel **tunnels[2] = { tunnels_wc, tunnels_r_l };

static struct list_head hooks[IPV6_TUNNEL_MAXHOOKS];

static rwlock_t ipv6_ipv6_lock = RW_LOCK_UNLOCKED;
static rwlock_t ipv6_ipv6_kernel_lock = RW_LOCK_UNLOCKED;
static rwlock_t ipv6_ipv6_hook_lock = RW_LOCK_UNLOCKED;

static int shutdown = 0;

/**
 * ipv6_ipv6_tunnel_lookup - fetch tunnel matching the end-point addresses
 *   @remote: the address of the tunnel exit-point 
 *   @local: the address of the tunnel entry-point 
 *
 * Return:  
 *   tunnel matching given end-points if found,
 *   else fallback tunnel if its device is up, 
 *   else %NULL
 *
 * Note:
 *   ipv6_ipv6_tunnel_lookup() should be called whenever one wants a reference
 *   to a previously configured tunneling device. For manual tunnel 
 *   configuration ipv6_ipv6_tunnel_locate() should be used.
 **/

struct ipv6_tunnel *
ipv6_ipv6_tunnel_lookup(struct in6_addr *remote, struct in6_addr *local)
{
	unsigned h0 = HASH(remote);
	unsigned h1 = HASH(local);
	struct ipv6_tunnel *t;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_lookup\n");
#endif
	for (t = tunnels_r_l[h0 ^ h1]; t; t = t->next) {
		if (!ipv6_addr_cmp(local, &t->parms.saddr) &&
		    !ipv6_addr_cmp(remote, &t->parms.daddr) && 
		    (t->dev->flags & IFF_UP))
			return t;
	}
	if ((t = tunnels_wc[0]) != NULL && (t->dev->flags & IFF_UP))
		return t;

	return NULL;
}


/**
 * ipv6_ipv6_bucket - get head of list matching given tunnel parameters
 *   @p: parameters containing tunnel end-points 
 *
 * Description:
 *   ipv6_ipv6_bucket() returns the head of the list matching the 
 *   &struct in6_addr entries saddr and daddr in @p.
 *
 * Return: head of IPv6 tunnel list 
 *
 * Note:
 *   ipv6_ipv6_bucket() is only called when a tunneling device is
 *   (re)configured. It is called by ipv6_ipv6_tunnel_link(), 
 *   ipv6_ipv6_tunnel_unlink() and __ipv6_ipv6_tunnel_locate() 
 *   and should not be called directly.
 **/

static struct ipv6_tunnel **ipv6_ipv6_bucket(struct ipv6_tunnel_parm *p)
{
	struct in6_addr *remote = &p->daddr;
	struct in6_addr *local = &p->saddr;
	unsigned h = 0;
	int prio = 0;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_bucket\n");
#endif
	if (!ipv6_addr_any(remote) || !ipv6_addr_any(local)) {
		prio = 1;
		h = HASH(remote) ^ HASH(local);
	}
	return &tunnels[prio][h];
}

/**
 * ipv6_ipv6_kernel_tunnel_link - add new kernel tunnel to cache
 *   @t: kernel tunnel
 *
 * Caveat:
 *   %IPV6_T_F_KERNEL_DEV is assumed to be raised in t->parms.flags.
 *
 * Note:
 *   See the comments on ipv6_ipv6_kernel_tunnel_add() for more information.
 **/

static __inline__ void ipv6_ipv6_kernel_tunnel_link(struct ipv6_tunnel *t)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_kernel_tunnel_link\n");
#endif
	write_lock_bh(&ipv6_ipv6_kernel_lock);
	t->next = tunnels_kernel[0];
	tunnels_kernel[0] = t;
	kdev_count++;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
}


/**
 * ipv6_ipv6_kernel_tunnel_unlink - remove first kernel tunnel from cache
 *
 * Return: first free kernel tunnel
 *
 * Note:
 *   See the comments on ipv6_ipv6_kernel_tunnel_add() for more information.
 **/

static __inline__ struct ipv6_tunnel *ipv6_ipv6_kernel_tunnel_unlink(void)
{
	struct ipv6_tunnel *t;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_kernel_tunnel_unlink\n");
#endif
	write_lock_bh(&ipv6_ipv6_kernel_lock);
	if ((t = tunnels_kernel[0]) != NULL) {
		tunnels_kernel[0] = t->next;
		kdev_count--;
	}
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	return t;
}

/**
 * ipv6_ipv6_tunnel_link - add tunnel to hash table
 *   @t: tunnel to be added
 **/

static void ipv6_ipv6_tunnel_link(struct ipv6_tunnel *t)
{
	struct ipv6_tunnel **tp = ipv6_ipv6_bucket(&t->parms);

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_link\n");	
#endif
	write_lock_bh(&ipv6_ipv6_lock);
	t->next = *tp;
	write_unlock_bh(&ipv6_ipv6_lock);
	*tp = t;
}


/**
 * ipv6_ipv6_tunnel_unlink - remove tunnel from hash table
 *   @t: tunnel to be removed
 **/

static void ipv6_ipv6_tunnel_unlink(struct ipv6_tunnel *t)
{
	struct ipv6_tunnel **tp;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_unlink\n");	
#endif
	for (tp = ipv6_ipv6_bucket(&t->parms); *tp; tp = &(*tp)->next) {
		if (t == *tp) {
			write_lock_bh(&ipv6_ipv6_lock);
			*tp = t->next;
			write_unlock_bh(&ipv6_ipv6_lock);
			break;
		}
	}
}


/**
 * ipv6_tunnel_addr_type_valid() - check address validity
 *   @addr: desired address
 *   @addr_type: the allowed address types for address
 *   Possible values are %IPV6_ADDR_UNICAST, %IPV6_ADDR_ANYCAST,
 *   %IPV6_ADDR_MULTICAST or a combination of these.
 * 
 * Return: 
 *   0 if @addr is loopback, unspecified or reserved address, else a 
 *   combination of all above mentioned address types matching @addr   
 **/

static __inline__ int 
ipv6_tunnel_addr_type_valid(struct in6_addr *addr, int addr_type)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_tunnel_addr_type_valid\n");	
#endif
	return ((ipv6_addr_type(addr) & 
		 (IPV6_ADDR_MULTICAST |
		  IPV6_ADDR_ANYCAST | 
		  IPV6_ADDR_ANY | 
		  IPV6_ADDR_LOOPBACK | 
		  IPV6_ADDR_RESERVED |
		  IPV6_ADDR_UNICAST)) &
		(addr_type & 
		 ~IPV6_ADDR_ANY & 
		 ~IPV6_ADDR_LOOPBACK & 
		 ~IPV6_ADDR_RESERVED));
}

/**
 * ipv6_addr_local() - check if address local
 *   @addr: desired address
 * 
 * Return: 
 *   1 if @addr assigned to any local network device,
 *   0 otherwise
 **/

static __inline__ int ipv6_addr_local(struct in6_addr *addr)
{
	struct inet6_ifaddr *ifr;
	int local = 0;	

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_addr_local\n");		
#endif
	if ((ifr = ipv6_get_ifaddr(addr, NULL)) != NULL) {
		local = 1;
		in6_ifa_put(ifr);
	}
	return local;
}

/**
 * ipv6_addrs_sane() - check tunnel end points sane
 *   @saddr: tunnel entry-point
 *   @daddr: tunnel exit-point
 *
 * Description:
 *   Sanity checks performed on tunnel end-points as suggested by
 *   RFCs 1853, 2003 and 2473.
 * 
 * Return: 
 *   0 if addresses sane
 **/

static __inline__ int 
ipv6_tunnel_addrs_sane(struct in6_addr *saddr, struct in6_addr *daddr) 
{	
	struct inet6_ifaddr *ifp;
	int result = 0;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_tunnel_addrs_sane\n");	
#endif
	if (ipv6_addr_cmp(saddr, daddr) &&
	    ipv6_tunnel_addr_type_valid(saddr, IPV6_ADDR_UNICAST) &&
	    ipv6_tunnel_addr_type_valid(daddr, (IPV6_ADDR_UNICAST |
						IPV6_ADDR_MULTICAST)) &&
	    ipv6_addr_local(saddr) &&
	    !ipv6_addr_local(daddr)) {
		ifp = ipv6_get_ifaddr(saddr, NULL);

		/* Address should be assigned to another device so the 
		   packets can be passed on to it after the encapsulation. */

		if (ifp != NULL) {
			if (!(ifp->valid_lft || 
			      (ifp->flags & IFA_F_PERMANENT)) ||
			    (ifp->flags & IFA_F_DEPRECATED)) {
				result = -EADDRNOTAVAIL;
			}
			in6_ifa_put(ifp);
		}
	} else {
		result = -EINVAL;
	}

	return result;
}


static struct ipv6_tunnel *ipv6_tunnel_create(struct ipv6_tunnel_parm *p) {
	struct net_device *dev;
	struct ipv6_tunnel *t;
	int kernel_dev;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_tunnel_create\n");	
#endif
	kernel_dev = (p->flags & IPV6_T_F_KERNEL_DEV);

	MOD_INC_USE_COUNT;
	dev = kmalloc(sizeof(*dev) + sizeof(*t), GFP_KERNEL);
	if (dev == NULL) {
		MOD_DEC_USE_COUNT;
		return NULL;
	}
	memset(dev, 0, sizeof(*dev) + sizeof(*t));
	dev->priv = (void *)(dev + 1);
	t = (struct ipv6_tunnel *)dev->priv;
	t->dev = dev;
	dev->init = ipv6_ipv6_tunnel_dev_init;
	dev->features |= NETIF_F_DYNALLOC;
	if (kernel_dev) {
		memcpy(t->parms.name, p->name, IFNAMSIZ);
		t->parms.proto = IPPROTO_IPV6;
		t->parms.flags = IPV6_T_F_KERNEL_DEV;
	} else {
		memcpy(&t->parms, p, sizeof(*p));
	}
	t->parms.name[IFNAMSIZ - 1] = '\0';
	if (t->parms.hop_limit > 255)
		t->parms.hop_limit = -1;
	strcpy(dev->name, t->parms.name);
	if (dev->name[0] == 0) {
		int i = 0;
		int exists = 0;

		do {
			sprintf(dev->name, "ip6tnl%d", ++i);
			exists = (__dev_get_by_name(dev->name) != NULL);
		} while (i < IPV6_TUNNEL_MAX && exists);

		if (i == IPV6_TUNNEL_MAX)
			goto failed;
		memcpy(t->parms.name, dev->name, IFNAMSIZ);
	}

	if (register_netdevice(dev) < 0) {
		goto failed;
	}
	if (kernel_dev) {
		ipv6_ipv6_kernel_tunnel_link(t);
	} else {
		ipv6_ipv6_tunnel_link(t);
	}
//	 Do not decrement MOD_USE_COUNT here. 

	return t;

failed:
	kfree(dev);
	MOD_DEC_USE_COUNT;
	return NULL;
}

static __inline__ int ipv6_tunnel_destroy(struct ipv6_tunnel *t) {
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_tunnel_destroy\n");
#endif
	return unregister_netdevice(t->dev);
}

static void manage_kernel_tunnels(void *p);

static struct tq_struct manager_task = {
	routine: manage_kernel_tunnels,
	data: NULL
};

static void manage_kernel_tunnels(void *p)
{
	struct ipv6_tunnel *t;
	struct ipv6_tunnel_parm parm;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "manage_kernel_tunnels\n");
#endif
	/* We can't do this processing in interrupt 
	   context so schedule it for later */
	if (in_interrupt()) {
		read_lock(&ipv6_ipv6_kernel_lock);
		if (!shutdown && 
		    (kdev_count < min_kdev_count || 
		     kdev_count > max_kdev_count)) {
			schedule_task(&manager_task);
		}
		read_unlock(&ipv6_ipv6_kernel_lock);
		return;
	}
		
	rtnl_lock();
	read_lock_bh(&ipv6_ipv6_kernel_lock);
	memset(&parm, 0, sizeof(parm));
	parm.flags = IPV6_T_F_KERNEL_DEV;
	/* Create tunnels until there are at least min_kdev_count */
	while (kdev_count < min_kdev_count) {
		read_unlock_bh(&ipv6_ipv6_kernel_lock);
		if ((t = ipv6_tunnel_create(&parm)) != NULL) {
			dev_open(t->dev);
		} else {
			goto err;
		}
		read_lock_bh(&ipv6_ipv6_kernel_lock);
	}
	
	/* Destroy tunnels until there are at most max_kdev_count */
	while (kdev_count > max_kdev_count) {			
		read_unlock_bh(&ipv6_ipv6_kernel_lock);
		if ((t = ipv6_ipv6_kernel_tunnel_unlink()) != NULL) {
			ipv6_tunnel_destroy(t);
		} else {
			goto err;
		}
		read_lock_bh(&ipv6_ipv6_kernel_lock);
	}
	read_unlock_bh(&ipv6_ipv6_kernel_lock);
err:
	rtnl_unlock();
}


unsigned int ipv6_ipv6_tunnel_inc_max_kdev_count(unsigned int n) 
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_inc_max_kdev_count\n");
#endif
	write_lock_bh(&ipv6_ipv6_kernel_lock);
	max_kdev_count += n;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	manage_kernel_tunnels(NULL);
	return max_kdev_count;
}

unsigned int ipv6_ipv6_tunnel_dec_max_kdev_count(unsigned int n) 
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_dec_max_kdev_count\n");
#endif
	write_lock_bh(&ipv6_ipv6_kernel_lock);
	max_kdev_count -= min(max_kdev_count, n);
	if (max_kdev_count < min_kdev_count)
		min_kdev_count = max_kdev_count;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	manage_kernel_tunnels(NULL);
	return max_kdev_count;
	
}

unsigned int ipv6_ipv6_tunnel_inc_min_kdev_count(unsigned int n) 
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_inc_min_kdev_count\n");
#endif
	write_lock_bh(&ipv6_ipv6_kernel_lock);
	min_kdev_count += n;
	if (min_kdev_count > max_kdev_count)
		max_kdev_count = min_kdev_count; 	
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	manage_kernel_tunnels(NULL);
	return min_kdev_count;
}

unsigned int ipv6_ipv6_tunnel_dec_min_kdev_count(unsigned int n) 
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_dec_min_kdev_count\n");
#endif
	write_lock_bh(&ipv6_ipv6_kernel_lock);
	min_kdev_count -= min(min_kdev_count, n);
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	manage_kernel_tunnels(NULL);
	return min_kdev_count;	
}


/**
 * __ipv6_ipv6_tunnel_locate - find or create tunnel matching given parameters
 *   @p: tunnel parameters 
 *   @create: != 0 if allowed to create new tunnel if no match found
 *
 * Description:
 *   __ipv6_ipv6_tunnel_locate() first triest to locate an existing tunnel
 *   based on @parms. If this is unsuccessful, but @create is true a new
 *   tunnel device is created and registered for use. Kernel tunnels must
 *   still be activated by ipv6_ipv6_kernel_tunnel_add() before they work.
 *
 * Return:
 *   tunnel matching given parameters if found or created,
 *   %NULL else
 *
 * Note:
 *   __ipv6_ipv6_tunnel_locate() should only be called when tunnels are 
 *   manually configured. For all other purposes ipv6_ipv6_tunnel_lookup()
 *   should do fine.
 *
 * Caveat:
 *   This function may only be called from process context.
 **/

static struct ipv6_tunnel *
__ipv6_ipv6_tunnel_locate(struct ipv6_tunnel_parm *p, int create)
{
	struct in6_addr *remote = &p->daddr;
	struct in6_addr *local = &p->saddr;
	struct ipv6_tunnel *t;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "__ipv6_ipv6_tunnel_locate\n");	
#endif
	for (t = *ipv6_ipv6_bucket(p); t; t = t->next) {
		if (!ipv6_addr_cmp(local, &t->parms.saddr) && 
		    !ipv6_addr_cmp(remote, &t->parms.daddr)) {
			return t;
		}
		/* Kernel devices shouldn't be created this way */
		if (!create || (p->flags & IPV6_T_F_KERNEL_DEV))
			return NULL;
	}

	return ipv6_tunnel_create(p);
}

/**
 * ipv6_ipv6_tunnel_locate - sanity checks before __ipv6_ipv6_tunnel_locate()
 *   @p: tunnel parameters 
 *   @pt: points to tunnel if parameters sane
 *   @create: != 0 if allowed to create new tunnel if no match found
 *
 * Return: 0 on success, %-EINVAL otherwise
 *   
 * Note:
 *   Same things as for __ipv6_ipv6_tunnel_locate apply here.
 **/

static __inline__ int 
ipv6_ipv6_tunnel_locate(struct ipv6_tunnel_parm *p, 
			struct ipv6_tunnel **pt,
			int create)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_locate\n");	
#endif
	if ((!(p->flags & IPV6_T_F_KERNEL_DEV) && 
	     ipv6_tunnel_addrs_sane(&p->saddr, &p->daddr)) ||
	    p->proto != IPPROTO_IPV6)
		return -EINVAL;
	
	*pt = __ipv6_ipv6_tunnel_locate(p, create);

	return 0;
}


/**
 * ipv6_ipv6_tunnel_dev_destructor - tunnel device destructor
 *   @dev: the device to be destroyed
 **/

static void ipv6_ipv6_tunnel_dev_destructor(struct net_device *dev)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_dev_destructor\n");	
#endif
	if (dev != &ipv6_ipv6_fb_tunnel_dev) {
		MOD_DEC_USE_COUNT;
	}
}


/**
 * ipv6_ipv6_tunnel_dev_uninit - tunnel device uninitializer
 *   @dev: the device to be destroyed
 *   
 * Description:
 *   ipv6_ipv6_tunnel_dev_uninit() removes tunnel 
 **/

static void ipv6_ipv6_tunnel_dev_uninit(struct net_device *dev)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_dev_uninit\n");	
#endif
	if (dev == &ipv6_ipv6_fb_tunnel_dev) {
		write_lock_bh(&ipv6_ipv6_lock);
		tunnels_wc[0] = NULL;
		write_unlock_bh(&ipv6_ipv6_lock);
	} else {
		struct ipv6_tunnel *t = (struct ipv6_tunnel *)dev->priv;
		ipv6_ipv6_tunnel_unlink(t);
	}
}


/**
 * ipv6_ipv6_err - tunnel error handler
 *
 * Description:
 *   ipv6_ipv6_err() should handle errors in the tunnel according
 *   to the specifications in RFC 2473.
 **/

void ipv6_ipv6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
		   int type, int code, int offset, __u32 info)
{
	struct ipv6hdr *tipv6h = (struct ipv6hdr *)skb->data;
	struct ipv6hdr *ipv6h = NULL;
	struct ipv6_tunnel *t;
	int rel_msg = 0;
	int rel_type = 0;
	int rel_code = 0;
	int rel_info = 0;
	__u32 mtu;
	__u16 len;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_err\n");	
#endif
	/* If the packet doesn't contain the original IPv6 header we are 
	   in trouble, since we might need the source address for furter 
	   processing of the error. */		   

	if (pskb_may_pull(skb, offset + sizeof(*ipv6h)))
		ipv6h = (struct ipv6hdr *)&skb->data[offset];

	read_lock(&ipv6_ipv6_lock);
	if (!(t = ipv6_ipv6_tunnel_lookup(&tipv6h->daddr, &tipv6h->saddr)))
		goto out;

	switch (type) {
	case ICMPV6_DEST_UNREACH:
	case ICMPV6_TIME_EXCEED:
	case ICMPV6_PARAMPROB:
		if (type == ICMPV6_DEST_UNREACH)
			printk(KERN_ERR "%s: Path to destination invalid or no longer active!\n", t->parms.name);
		else if (type == ICMPV6_TIME_EXCEED &&
		    code == ICMPV6_EXC_HOPLIMIT)
			printk(KERN_ERR "%s: Misconfigured hop limit or routing loop in tunnel!\n", t->parms.name);
		else if (type == ICMPV6_PARAMPROB &&
			 code == ICMPV6_HDR_FIELD && 
			 info + 2 < offset &&
			 skb->data[info] == IPV6_TLV_TUNENCAPLIM &&
			 skb->data[info + 1] == 1 &&
			 skb->data[info + 2] == 0)
			printk(KERN_ERR "%s: Misconfigured encapsulation limit or routing loop in tunnel!\n", t->parms.name);
		else 
			break;

		if (ipv6h) {
			rel_msg = 1;
			rel_type = ICMPV6_DEST_UNREACH;
			rel_code = ICMPV6_ADDR_UNREACH;
			rel_info = 0;
		}
		break;
	case ICMPV6_PKT_TOOBIG:	
		mtu = info - offset;
		if (mtu < IPV6_MIN_MTU)
			mtu = IPV6_MIN_MTU;
		t->dev->mtu =  mtu;

		if (ipv6h) {
			len = sizeof(*ipv6h) + ipv6h->payload_len;
			if (len > mtu) {
				rel_msg = 1;
				rel_type = ICMPV6_PKT_TOOBIG;
				rel_code = 0;
				rel_info = mtu;
			}
		}
	}
	if (rel_msg) {
		struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
		struct rt6_info *rt6i;

		if (skb2 == NULL)
		      goto out;
		dst_release(skb2->dst);
		skb2->dst = NULL;
		skb_pull(skb2, offset);
		skb2->nh.raw = skb2->data;
		
		/* Try to guess incoming interface */
		rt6i = rt6_lookup(&ipv6h->saddr, NULL, 0, 0);
		if (rt6i && rt6i->rt6i_dev)
			skb2->dev = rt6i->rt6i_dev;

		icmpv6_send(skb2, rel_type, rel_code, rel_info, skb2->dev);
		kfree_skb(skb2);
	}
out:	
	read_unlock(&ipv6_ipv6_lock);
	return;
}

static __inline__ int 
call_hooks(unsigned int hooknum, struct ipv6_tunnel *t, 
	   struct sk_buff *skb, __u32 flags)
{
	struct ipv6_tunnel_hook_ops *h;
	int accept = IPV6_TUNNEL_ACCEPT;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "call_hooks\n");	
#endif
	if (hooknum < IPV6_TUNNEL_MAXHOOKS) {
		struct list_head *i;
		read_lock(&ipv6_ipv6_hook_lock);
		for (i = hooks[hooknum].next; 
		     i != &hooks[hooknum]; 
		     i = i->next) {
			h = (struct ipv6_tunnel_hook_ops *)i;

			if (h->hook) {
			    accept = h->hook(t,skb, flags);

			    if (accept != IPV6_TUNNEL_ACCEPT)
				break;
			}
		}
		read_unlock(&ipv6_ipv6_hook_lock);
	}
	return accept;
}

/**
 * ipv6_ipv6_rcv - decapsulate IPv6 packet and retransmit it
 *   @skb: received socket buffer
 *
 * Return: 0
 **/

int ipv6_ipv6_rcv(struct sk_buff *skb)
{
	struct ipv6hdr *ipv6h;
	struct ipv6_tunnel *t;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_rcv\n");	
#endif
	if (!pskb_may_pull(skb, sizeof(*ipv6h)))
		goto out;
	
	ipv6h = skb->nh.ipv6h;
	
	read_lock(&ipv6_ipv6_lock);
	
	if ((t = ipv6_ipv6_tunnel_lookup(&ipv6h->saddr, &ipv6h->daddr))) {
		int hookval = call_hooks(IPV6_TUNNEL_PRE_DECAP, 
					 t, skb, t->parms.flags); 
		if (hookval == IPV6_TUNNEL_DROP)
			goto drop_packet;
		if (hookval == IPV6_TUNNEL_STOLEN)
			goto ignore_packet;
		
		skb->mac.raw = skb->nh.raw;
		skb->nh.raw = skb->data;
		skb->protocol = __constant_htons(ETH_P_IPV6);
		skb->pkt_type = PACKET_HOST;
		skb->dev = t->dev;
		dst_release(skb->dst);
		skb->dst = NULL;
		t->stat.rx_packets++;
		t->stat.rx_bytes += skb->len;
		netif_rx(skb);
		read_unlock(&ipv6_ipv6_lock);
		return 0;
	}
	icmpv6_send(skb, ICMPV6_DEST_UNREACH, 
		    ICMPV6_ADDR_UNREACH, 0, skb->dev);

drop_packet:
	kfree_skb(skb);
ignore_packet:
	read_unlock(&ipv6_ipv6_lock);
out:
	return 0;
}


/**
 * ipv6_ipv6_tunnel_parse_dest_tunencaplim - handle encapsulation limit option
 *   @skb: received socket buffer
 *
 * Return: 
 *   0 if none was found, else offset to option from skb->nh.raw
 **/

static __inline__ __u16 
ipv6_ipv6_tunnel_parse_dest_tunencaplim(struct sk_buff *skb)
{
	__u8 nexthdr = skb->nh.ipv6h->nexthdr;
	__u16 off = sizeof(struct ipv6hdr);

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_parse_dest\n");	
#endif
	while (ipv6_ext_hdr(nexthdr) && nexthdr != NEXTHDR_NONE) {
		__u16 len = 0;
		struct ipv6_opt_hdr *hdr;
		if (skb->nh.raw + off + sizeof(*hdr) > skb->data && 
		    !pskb_may_pull(skb, skb->nh.raw - skb->data + 
				   off + sizeof(*hdr)))
			break;

		hdr = (struct ipv6_opt_hdr *)(skb->nh.raw + off);
		if (nexthdr == NEXTHDR_FRAGMENT) {
			struct frag_hdr *frag_hdr = (struct frag_hdr *)hdr;
 			if (frag_hdr->frag_off)
			    break;
			len = 8;
		} else if (nexthdr == NEXTHDR_AUTH) {
			len = (hdr->hdrlen + 2) << 2;
		} else {
			len = (hdr->hdrlen + 1) << 3;
		}
		if (nexthdr == NEXTHDR_DEST) {
			__u16 i;
			for (i = off + 2; i < off + len; i++) {
				__u8 type = skb->nh.raw[i];

				if (type == IPV6_TLV_TUNENCAPLIM)
					return i;

				if (type)
					i += skb->nh.raw[++i];
			}
		}
		nexthdr = hdr->nexthdr;
		off += len;
	}
	return 0;		
}

/**
 * ipv6_tunnel_txopt_len - get necessary size for new &struct ipv6_txoptions
 *   @opt: old options
 *
 * Return:
 *   Size of old one plus size of tunnel encaptulation limit option
 **/

static __inline__ int ipv6_tunnel_txopt_len(struct ipv6_txoptions *opt) 
{
	int len = sizeof(*opt) + 8;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_tunnel_txopt_len\n");	
#endif
	if (opt && opt->dst0opt)
		len += (opt->dst0opt->hdrlen + 1) << 3;

	return len;
}


/**
 * ipv6_tunnel_merge_options - add encaptulation limit to original options
 *   @opt: new options
 *   @encap_lim: number of allowed encapsulation limits
 *   @orig_opt: original options
 **/

static __inline__ void 
ipv6_tunnel_merge_options(struct ipv6_txoptions *opt, __u8 encap_lim, 
			  struct ipv6_txoptions *orig_opt)
{
	__u8 *raw = (__u8 *)opt->dst0opt;	
	__u8 pad_to = 8;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_tunnel_merge_options\n");	
#endif
	opt->opt_nflen = (encap_lim ? 8 : 0);

	raw[2] = IPV6_TLV_TUNENCAPLIM;
	raw[3] = 1;
	raw[4] = encap_lim;
	if (orig_opt) {
		__u8 *orig_raw;

		opt->hopopt = orig_opt->hopopt;

		/* Keep the original destination options properly
		   aligned and merge possible old paddings to the
		   new padding option */
		if ((orig_raw = (__u8 *)orig_opt->dst0opt) != NULL) {
			__u8 type;
			pad_to += 2;
			if ((type = orig_raw[2]) == IPV6_TLV_PAD0)
				pad_to++;
			else if (type == IPV6_TLV_PADN)
				pad_to += orig_raw[3] + 2;
			memcpy(raw + pad_to, orig_raw + pad_to - 8, 
			       opt->tot_len - sizeof(*opt) - pad_to);
		}
		opt->srcrt = orig_opt->srcrt;
		opt->opt_nflen += orig_opt->opt_nflen;
		opt->dst1opt = orig_opt->dst1opt;
		opt->auth = orig_opt->auth;
		opt->opt_flen = orig_opt->opt_flen;
	}
	raw[5] = IPV6_TLV_PADN;
	raw[6] = pad_to - 7;
}


static int 
ipv6_ipv6_getfrag(const void *data, struct in6_addr *addr, char *buff, 
		  unsigned int offset, unsigned int len)
{
	memcpy(buff, data + offset, len); 
	return 0; 
}


/**
 * ipv6_ipv6_tunnel_addr_conflict - compare packets addresses to  tunnels own
 *   @t: the outgoing tunnel device
 *   @hdr: IPv6 header from the incoming packet 
 *
 * Description:
 *   Avoid trivial tunneling loop by checking that tunnel exit-point 
 *   doesn't match source of incoming packet.
 *
 * Return: 
 *   1 if conflict,
 *   0 else
 **/

static __inline__ int 
ipv6_ipv6_tunnel_addr_conflict(struct ipv6_tunnel *t, struct ipv6hdr *hdr) 
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_addr_conflict\n");	
#endif
	return !ipv6_addr_cmp(&t->parms.daddr, &hdr->saddr);		
}

/**
 * ipv6_ipv6_tunnel_xmit - encapsulate packet and send 
 *   @skb: the outgoing socket buffer
 *   @dev: the outgoing tunnel device 
 *
 * Description:
 *   Do some sanity checks before sending the entire packet to 
 *   ip6_build_xmit().
 *
 * Return: 
 *   0
 **/

static int ipv6_ipv6_tunnel_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ipv6_tunnel *t = (struct ipv6_tunnel *)dev->priv;
	struct net_device_stats *stats = &t->stat;	
	struct ipv6hdr *ipv6h = skb->nh.ipv6h;
	__u32 flags;
	int hookval;
	int opt_len;
	struct ipv6_txoptions *opt = NULL;
	__u8 encap_lim = 0;
	__u16 offset;	
	struct flowi fl;
	struct ip6_flowlabel *flowlabel = NULL;
	struct ipv6_txoptions *fl_opt = NULL;
	int err = 0;
	struct dst_entry *dst;
	struct sock *sk = ipv6_socket->sk;
	struct net_device *tdev;//Device to other host
	int mtu;			

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_xmit\n");	
#endif
	if (t->recursion++) {
		stats->collisions++;
		goto tx_error;
	}
	
	flags = t->parms.flags & ~IPV6_T_F_LOCAL_ORIGIN;
	if (ipv6_addr_local(&ipv6h->saddr))
		flags |= IPV6_T_F_LOCAL_ORIGIN;
	if (skb->protocol != __constant_htons(ETH_P_IPV6) || 
	    ipv6_ipv6_tunnel_addr_conflict(t, ipv6h) ||
	    (!(t->parms.flags & IPV6_T_F_LOCAL_ORIGIN) && 
	     (flags & IPV6_T_F_LOCAL_ORIGIN))) {
		stats->tx_dropped++;
		goto tx_error;
	}
	
	hookval = call_hooks(IPV6_TUNNEL_PRE_ENCAP, t, skb, flags);
	
	if (hookval == IPV6_TUNNEL_DROP) {
		stats->tx_dropped++;
		goto drop_packet;
	}
	if (hookval == IPV6_TUNNEL_STOLEN)
		goto ignore_packet;


	memcpy(&fl, &t->fl, sizeof(fl));		
	
	if (fl.fl6_flowlabel) {
		flowlabel = fl6_sock_lookup(sk, fl.fl6_flowlabel);
		if (flowlabel != NULL)
			fl_opt = flowlabel->opt;
	}
	if ((t->parms.flags & IPV6_T_F_USE_ORIG_TCLASS)) {
		fl.fl6_flowlabel |= (*(__u32 *)ipv6h & 
				     IPV6_FLOWINFO_MASK &
				     ~IPV6_FLOWLABEL_MASK);
	}

	if ((err = ipv6_xmit_lock()))
		goto tx_error;

	opt_len = ipv6_tunnel_txopt_len(fl_opt);
	if ((opt = sock_kmalloc(sk, opt_len, GFP_ATOMIC)) == NULL) {
		stats->tx_dropped++;
		ipv6_xmit_unlock();
		goto tx_error;
	}
	memset(opt, 0, opt_len);
	opt->tot_len = opt_len;

	if ((offset = ipv6_ipv6_tunnel_parse_dest_tunencaplim(skb)) > 0) {
		if ((encap_lim  = skb->nh.raw[offset + 2]) == 0) {
			icmpv6_send(skb, ICMPV6_PARAMPROB, ICMPV6_HDR_FIELD, 
				    offset + 2, skb->dev);
			goto tx_error_free_txopt;
		}
	} else if (!(t->parms.flags & IPV6_T_F_IGN_ENCAP_LIM)) {
		encap_lim = t->parms.encap_lim;
	}
	if (encap_lim) {
		opt->dst0opt = (struct ipv6_opt_hdr *)(opt + 1);
		ipv6_tunnel_merge_options(opt, encap_lim, NULL);
	}

	dst = ip6_route_output(sk, &fl);

	if (dst->error) {
		stats->tx_carrier_errors++;
		goto tx_error_icmp;
	}
	
	tdev = dst->dev;

	if (tdev == dev) {
		stats->collisions++;
		goto tx_error_dst_release;
	}

	mtu = dst->pmtu - sizeof(*ipv6h) - opt->opt_nflen - opt->opt_flen;

	if (mtu < IPV6_MIN_MTU)
		mtu = IPV6_MIN_MTU;
	if (skb->dst && mtu < skb->dst->pmtu) {
		struct rt6_info *rt6 = (struct rt6_info *)skb->dst;
		if (mtu < rt6->u.dst.pmtu) {
			rt6->rt6i_flags |= RTF_MODIFIED;
			rt6->u.dst.pmtu = mtu;
		}
	}

	if (skb->len > mtu && skb->len > IPV6_MIN_MTU) {
		icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, 
			    (mtu > IPV6_MIN_MTU ? mtu : IPV6_MIN_MTU), dev);
		goto tx_error_dst_release;
	}

	err = ip6_build_xmit(sk, ipv6_ipv6_getfrag, (void *)skb->nh.raw, 
			     &fl, skb->len, opt, t->parms.hop_limit, 0,
			     MSG_DONTWAIT);

	if (err == NET_XMIT_SUCCESS || err == NET_XMIT_CN) {
		stats->tx_bytes += skb->len;
		stats->tx_packets++;
	} else {
		stats->tx_errors++;
		stats->tx_aborted_errors++;
	}
	dst_release(dst);
	ipv6_xmit_unlock();
	fl6_sock_release(flowlabel);
	sock_kfree_s(sk, opt, opt_len);
	kfree_skb(skb);
	t->recursion--;

	return 0;
tx_error_icmp:
	dst_link_failure(skb);
tx_error_dst_release:
	dst_release(dst);
tx_error_free_txopt:
	ipv6_xmit_unlock();
	sock_kfree_s(sk, opt, opt_len);	
tx_error:
	stats->tx_errors++;
	fl6_sock_release(flowlabel);
drop_packet:
	kfree_skb(skb);
ignore_packet:
	t->recursion--;
	return 0;
}

struct ipv6_tunnel_link_parm {
	int iflink;
	unsigned short hard_header_len;
	unsigned mtu;
};

static int ipv6_ipv6_tunnel_link_parm_get(struct ipv6_tunnel_parm *p,
					  int active, 
					  struct ipv6_tunnel_link_parm *lp)
{
	int err = 0;

	if (active) {
		struct rt6_info *rt = rt6_lookup(&p->daddr, &p->saddr, 
						 p->link, 0);
		struct net_device *rtdev;
		if (rt == NULL) {
			err = -ENOENT;
		} else if ((rtdev = rt->rt6i_dev) == NULL) {
			err = -ENODEV;
		} else if (rtdev->type == ARPHRD_IPV6_IPV6_TUNNEL) {
			err = -ENOMEDIUM;
		} else {
			lp->iflink = rtdev->ifindex;	
			lp->hard_header_len = rtdev->hard_header_len + 
				sizeof(struct ipv6hdr);
			lp->mtu = rtdev->mtu - sizeof(struct ipv6hdr);
			
			if (lp->mtu < IPV6_MIN_MTU)
				lp->mtu = IPV6_MIN_MTU;
		}
		if (rt != NULL) {
			dst_release(&rt->u.dst);
		}
	} else {
		lp->iflink = 0;
		lp->hard_header_len = LL_MAX_HEADER + sizeof(struct ipv6hdr);
		lp->mtu = ETH_DATA_LEN - sizeof(struct ipv6hdr);
	}

	return err;
}

static void ipv6_ipv6_tunnel_dev_config(struct net_device *dev, 
					struct ipv6_tunnel_link_parm *lp)
{
	struct ipv6_tunnel *t = (struct ipv6_tunnel *)dev->priv;
	struct flowi *fl;
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_dev_config\n");	
#endif
	/* Set up flowi template */
	fl = &t->fl;
	fl->fl6_src = &t->parms.saddr;
	fl->fl6_dst = &t->parms.daddr;
	fl->oif = t->parms.link;
	fl->fl6_flowlabel = IPV6_FLOWLABEL_MASK & htonl(t->parms.flowlabel);

	dev->iflink = lp->iflink;
	dev->hard_header_len = lp->hard_header_len;
	dev->mtu = lp->mtu;
	
	return;
}

/**
 * ipv6_ipv6_tunnel_change - update the tunnel parameters
 *   @t: tunnel to be changed
 *   @p: tunnel configuration parameters
 *   @active: != 0 if tunnel is ready for use
 *
 * Description:
 *   ipv6_ipv6_tunnel_change() updates the tunnel parameters
 **/ 

static int ipv6_ipv6_tunnel_change(struct ipv6_tunnel *t,
				   struct ipv6_tunnel_parm *p,
				   int active)
{
	struct net_device *dev = t->dev;
	int daddr_type = ipv6_addr_type(&p->daddr);
	int err;
	struct ipv6_tunnel_link_parm lp;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_change\n");	
#endif
	if ((err = ipv6_ipv6_tunnel_link_parm_get(p, active, &lp)))
	    return err;

	if (daddr_type & IPV6_ADDR_UNICAST)
		dev->flags |= IFF_POINTOPOINT;
	else
		dev->flags &= ~IFF_POINTOPOINT;

	ipv6_addr_copy(&t->parms.saddr, &p->saddr);
	ipv6_addr_copy(&t->parms.daddr, &p->daddr);
	t->parms.flags = p->flags;
	t->parms.hop_limit = (p->hop_limit <= 255 ? p->hop_limit : -1);

	ipv6_ipv6_tunnel_dev_config(dev, &lp);
	return 0;
}

/**
 * ipv6_ipv6_kernel_tunnel_add - configure and add kernel tunnel to hash 
 *   @p: kernel tunnel configuration parameters
 *
 * Description:
 *   ipv6_ipv6_kernel_tunnel_add() fetches an unused kernel tunnel configures
 *   it according to @p and places it among the active tunnels.
 * 
 * Return:
 *   number of references to tunnel on success,
 *   %-EEXIST if there is already a device matching description
 *   %-EINVAL if p->flags doesn't have %IPV6_T_F_KERNEL_DEV raised,
 *   %-ENODEV if there are no unused kernel tunnels available 
 * 
 * Note:
 *   The code for creating, opening, closing and destroying network devices
 *   must apparently be called from process context, while the Mobile IP code,
 *   which needs these tunnel devices, unfortunately runs in interrupt 
 *   context. 
 *   
 *   The devices must be created and opened in advance, then placed in a 
 *   queue where the kernel can fetch and ready them for use at a later time.
 *
 *   A kernel thread for handling the creation and destruction of the virtual
 *   tunnel devices could perhaps be a cleaner solution, but we have to live 
 *   with this at the moment being... 
 **/

int ipv6_ipv6_kernel_tunnel_add(struct ipv6_tunnel_parm *p)
{
	struct ipv6_tunnel *t;
	int err;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_kernel_tunnel_add\n");	
#endif
	if(!(p->flags & IPV6_T_F_KERNEL_DEV) || 
	   ipv6_tunnel_addrs_sane(&p->saddr, &p->daddr) != 0)
		return -EINVAL;
	if ((t = ipv6_ipv6_tunnel_lookup(&p->daddr, &p->saddr)) != NULL &&
	    t != &ipv6_ipv6_fb_tunnel) {
		if (p->flags != t->parms.flags) {
			/* Incompatible tunnel already exists for endpoints */
			return -EEXIST;
		} else {
			atomic_inc(&t->refcnt);
			goto out;
		}
	}
	if ((t = ipv6_ipv6_kernel_tunnel_unlink()) == NULL)
		return -ENODEV;
	
	if ((err = ipv6_ipv6_tunnel_change(t, p, 1))) {
		ipv6_ipv6_kernel_tunnel_link(t);
		return err;
	}
	atomic_inc(&t->refcnt);

	ipv6_ipv6_tunnel_link(t);

	manage_kernel_tunnels(NULL);
out:
	return atomic_read(&t->refcnt);
} 


/**
 * ipv6_ipv6_kernel_tunnel_del - delete no longer needed kernel tunnel 
 *   @t: kernel tunnel to be removed from hash
 *
 * Description:
 *   ipv6_ipv6_kernel_tunnel_del() removes and deconfigures the tunnel @t
 *   and places it among the unused kernel devices.
 * 
 * Return:
 *   number of references on success,
 *   %-EINVAL if p->flags doesn't have %IPV6_T_F_KERNEL_DEV raised,
 * 
 * Note:
 *   See the comments on ipv6_ipv6_kernel_tunnel_add() for more information.
 **/

int ipv6_ipv6_kernel_tunnel_del(struct ipv6_tunnel *t)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_kernel_tunnel_del\n");	
#endif
	if (t == NULL)
		return -ENODEV;
	
	if(!(t->parms.flags & IPV6_T_F_KERNEL_DEV))
		return -EINVAL;

	if (atomic_dec_and_test(&t->refcnt)) {
		struct ipv6_tunnel_parm p;
		ipv6_ipv6_tunnel_unlink(t);
		memset(&p, 0, sizeof(p));
		p.flags = IPV6_T_F_KERNEL_DEV;
		
		ipv6_ipv6_tunnel_change(t, &p, 0);
		
		ipv6_ipv6_kernel_tunnel_link(t);
		
		manage_kernel_tunnels(NULL);
	}
	return atomic_read(&t->refcnt);
} 


/**
 * ipv6_ipv6_tunnel_ioctl - configure ipv6 tunnels from userspace 
 *   @dev: virtual device associated with tunnel
 *   @ifr: parameters passed from userspace
 *   @cmd: command to be performed
 *
 * Description:
 *   ipv6_ipv6_tunnel_ioctl() is used for manipulating IPv6 tunnels 
 *   from userspace. 
 *
 *   The possible commands are the following:
 *     %SIOCGETTUNNEL: get tunnel parameters for device
 *     %SIOCADDTUNNEL: add tunnel matching given tunnel parameters
 *     %SIOCCHGTUNNEL: change tunnel parameters to those given
 *     %SIOCDELTUNNEL: delete tunnel
 *
 *   The fallback device "ipv6tunl0", created during module 
 *   initialization, can be used for creating other tunnel devices.
 *
 * Return:
 *   0 on success,
 *   %-EFAULT if unable to copy data to or from userspace,
 *   %-EPERM if current process hasn't %CAP_NET_ADMIN set, 
 *   %-EINVAL if passed tunnel parameters are invalid,
 *   %-EEXIST if changing a tunnels parameters would cause a conflict
 *   %-ENODEV if attempting to change or delete a nonexisting device
 *
 * Note:
 *   Kernel devices are created and deleted by raising the @kernel_dev flag
 *   in the passed &struct ipv6_tunnel_parm. See the comments on 
 *   ipv6_ipv6_kernel_tunnel_add() for more information about kernel 
 *   tunnels.
 *
 * Caveat:
 *   The kernel devices are configured by the kernel and may not be changed 
 *   from userspace. Calling ipv6_ipv6_tunnel_ioctl() using %SIOCCHGTUNNEL
 *   will return %-EINVAL, but all other codes work.
 **/

static int 
ipv6_ipv6_tunnel_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int err = 0;
	struct ipv6_tunnel_parm p;
	struct ipv6_tunnel *t;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_ioctl\n");	
#endif
	MOD_INC_USE_COUNT;

	switch (cmd) {
	case SIOCGETTUNNEL:
		t = NULL;
		if (dev == &ipv6_ipv6_fb_tunnel_dev) {
			if (copy_from_user(&p, 
					   ifr->ifr_ifru.ifru_data,
					   sizeof(p))) {
				err = -EFAULT;
				break;
			}
			t = __ipv6_ipv6_tunnel_locate(&p, 0);
		}
		if (t == NULL)
			t = (struct ipv6_tunnel *)dev->priv;
		memcpy(&p, &t->parms, sizeof(p));
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &p, sizeof(p))) {
			err = -EFAULT;
		}
		break;

	case SIOCADDTUNNEL:
	case SIOCCHGTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			goto done;

		if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p))) {
			err = -EFAULT;
			goto done;
		}
		if (p.flags & IPV6_T_F_KERNEL_DEV)
			goto done;

		err = ipv6_ipv6_tunnel_locate(&p, &t, cmd == SIOCADDTUNNEL);

		if (err != 0)
			break;

		if (dev != &ipv6_ipv6_fb_tunnel_dev && 
		    cmd == SIOCCHGTUNNEL &&
		    t != &ipv6_ipv6_fb_tunnel) {
			if (t != NULL) {
				if (t->dev != dev) {
					err = -EEXIST;
					break;
				}
			} else 
				t = (struct ipv6_tunnel *)dev->priv;
		}
		if (t) {
			if (cmd == SIOCCHGTUNNEL) {
				if (t->parms.flags & IPV6_T_F_KERNEL_DEV) {
					err = -EPERM;
					goto done;
				}
				ipv6_ipv6_tunnel_unlink(t);
				err = ipv6_ipv6_tunnel_change(t, &p, 1);
				ipv6_ipv6_tunnel_link(t);
				netdev_state_change(dev);
			}
			if (copy_to_user(ifr->ifr_ifru.ifru_data, 
					 &t->parms, sizeof(p))) {
				err = -EFAULT;
			}
			else
				err = 0;
		} else
			err = (cmd == SIOCADDTUNNEL ? -ENOBUFS : -ENODEV);
		break;
	case SIOCDELTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			goto done;

		if (dev == &ipv6_ipv6_fb_tunnel_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p))) {
				err = -EFAULT;
				goto done;
			}
			t = __ipv6_ipv6_tunnel_locate(&p, 0);
			if (t == NULL) {
				err = -ENODEV;
				goto done;
			}
			
			if (t == &ipv6_ipv6_fb_tunnel) {
				err = -EPERM;
				goto done;
			}
			dev = t->dev;
		} else {
			t = (struct ipv6_tunnel *)dev->priv;
		}

		if (t->parms.flags & IPV6_T_F_KERNEL_DEV) 
			err = -EPERM;
		else
			err = ipv6_tunnel_destroy(t);
		break;
	default:
		err = -EINVAL;
	}

done:
	MOD_DEC_USE_COUNT;
	return err;
}


/**
 * ipv6_ipv6_tunnel_get_stats - return the stats for tunnel device 
 *   @dev: virtual device associated with tunnel
 *
 * Return: stats for device
 **/

static struct net_device_stats *
ipv6_ipv6_tunnel_get_stats(struct net_device *dev)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_get_stats\n");	
#endif
	return &(((struct ipv6_tunnel *)dev->priv)->stat);
}


/**
 * ipv6_ipv6_tunnel_change_mtu - change mtu manually for tunnel device
 *   @dev: virtual device associated with tunnel
 *   @new_mtu: the new mtu
 *
 * Return:
 *   0 on success,
 *   %-EINVAL of mtu too big or too small
 **/

static int ipv6_ipv6_tunnel_change_mtu(struct net_device *dev, int new_mtu)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_change_mtu\n");	
#endif
	if (new_mtu < IPV6_MIN_MTU) {
		return -EINVAL;
}
	dev->mtu = new_mtu;
	return 0;
}


/**
 * ipv6_ipv6_tunnel_dev_init_gen - general initializer for all tunnel devices
 *   @dev: virtual device associated with tunnel
 *
 * Description:
 *   Set function pointers and initialize the &struct flowi template used
 *   by the tunnel.
 **/

static void ipv6_ipv6_tunnel_dev_init_gen(struct net_device *dev)
{
	struct ipv6_tunnel *t = (struct ipv6_tunnel *)dev->priv;
	struct flowi *fl = &t->fl;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_dev_init_gen\n");	
#endif
	memset(fl, 0, sizeof(*fl));
	fl->proto = IPPROTO_IPV6;

	dev->destructor	= ipv6_ipv6_tunnel_dev_destructor;
	dev->uninit = ipv6_ipv6_tunnel_dev_uninit;
	dev->hard_start_xmit = ipv6_ipv6_tunnel_xmit;
	dev->get_stats = ipv6_ipv6_tunnel_get_stats;
	dev->do_ioctl = ipv6_ipv6_tunnel_ioctl;
	dev->change_mtu	= ipv6_ipv6_tunnel_change_mtu;
	dev->type = ARPHRD_IPV6_IPV6_TUNNEL; 
	dev->flags |= IFF_NOARP;
	if (ipv6_addr_type(&t->parms.daddr) & IPV6_ADDR_UNICAST)
		dev->flags |= IFF_POINTOPOINT;
	dev->iflink = 0;
	/* Hmm... MAX_ADDR_LEN is 8, so the ipv6 addresses can't be 
	   copied to dev->dev_addr and dev->broadcast, like the ipv4
	   addresses were in ipip.c, ip_gre.c and sit.c. */
	dev->addr_len = 0;
}


/**
 * ipv6_ipv6_tunnel_dev_init - initializer for all non fallback tunnel devices
 *   @dev: virtual device associated with tunnel
 **/

static int ipv6_ipv6_tunnel_dev_init(struct net_device *dev)
{	
	struct ipv6_tunnel *t = (struct ipv6_tunnel *)dev->priv;
	int active = !(t->parms.flags & IPV6_T_F_KERNEL_DEV);
	struct ipv6_tunnel_link_parm lp;
	int err;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_dev_init\n");	
#endif
	if ((err = ipv6_ipv6_tunnel_link_parm_get(&t->parms, active, &lp))) {
		return err;
	}
	ipv6_ipv6_tunnel_dev_init_gen(dev);
	ipv6_ipv6_tunnel_dev_config(dev, &lp);
	return 0;
}

#ifdef MODULE

/**
 * ipv6_ipv6_fb_tunnel_open - function called when fallback device opened
 *   @dev: fallback device
 *
 * Return: 0 
 **/

static int ipv6_ipv6_fb_tunnel_open(struct net_device *dev)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_fb_tunnel_open\n");	
#endif
	MOD_INC_USE_COUNT;
	return 0;
}


/**
 * ipv6_ipv6_fb_tunnel_close - function called when fallback device closed
 *   @dev: fallback device
 *
 * Return: 0 
 **/

static int ipv6_ipv6_fb_tunnel_close(struct net_device *dev)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_fb_tunnel_close\n");	
#endif
	MOD_DEC_USE_COUNT;
	return 0;
}
#endif


/**
 * ipv6_ipv6_fb_tunnel_dev_init - initializer for fallback tunnel devices
 *   @dev: fallback device
 *
 * Return: 0
 **/

int __init ipv6_ipv6_fb_tunnel_dev_init(struct net_device *dev)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_fb_tunnel_dev_init\n");	
#endif
	ipv6_ipv6_tunnel_dev_init_gen(dev);
#ifdef MODULE
	dev->open = ipv6_ipv6_fb_tunnel_open;
	dev->stop = ipv6_ipv6_fb_tunnel_close;
#endif
	tunnels_wc[0] = &ipv6_ipv6_fb_tunnel;
	return 0;
}

/*
 * The IPv6 on IPv6 protocol
 */
static struct inet6_protocol ipv6_ipv6_protocol = {
	ipv6_ipv6_rcv,
	ipv6_ipv6_err,
	NULL,
	IPPROTO_IPV6,
	0,
	NULL,
	"IPv6 over IPv6"
};


void ipv6_ipv6_tunnel_register_hook(struct ipv6_tunnel_hook_ops *reg) 
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_register_hook\n");	
#endif
	if (reg->hooknum < IPV6_TUNNEL_MAXHOOKS) {
		struct list_head *i;

		write_lock_bh(&ipv6_ipv6_hook_lock);
		for (i = hooks[reg->hooknum].next; 
		     i != &hooks[reg->hooknum]; 
		     i = i->next) {
			if (reg->priority < 
			    ((struct ipv6_tunnel_hook_ops *)i)->priority) {
				break;
			}
		}
		list_add(&reg->list, i->prev);
		write_unlock_bh(&ipv6_ipv6_hook_lock);		
	}
}


void ipv6_ipv6_tunnel_unregister_hook(struct ipv6_tunnel_hook_ops *reg)
{
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_unregister_hook\n");	
#endif
	if (reg->hooknum < IPV6_TUNNEL_MAXHOOKS) {
		write_lock_bh(&ipv6_ipv6_hook_lock);		
		list_del(&reg->list);
		write_unlock_bh(&ipv6_ipv6_hook_lock);		
	}
}

/**
 * ipv6_ipv6_tunnel_init - register protocol and reserve needed resources
 *
 * Return: 0 on success
 **/
int __init ipv6_ipv6_tunnel_init(void)
{
	int i, err;
	struct sock *sk;

#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_init\n");	
#endif
	ipv6_ipv6_fb_tunnel_dev.priv = (void *)&ipv6_ipv6_fb_tunnel;
	strcpy(ipv6_ipv6_fb_tunnel_dev.name, ipv6_ipv6_fb_tunnel.parms.name);
	err = sock_create(PF_INET6, SOCK_RAW, IPPROTO_IPV6, &ipv6_socket);
	if (err < 0) {
		printk(KERN_ERR
		       "Failed to create the IPv6 tunnel socket.\n");
		return err;
	}
	sk = ipv6_socket->sk;
	sk->allocation = GFP_ATOMIC;
	sk->sndbuf = SK_WMEM_MAX;
	sk->net_pinfo.af_inet6.hop_limit = 254;
	sk->net_pinfo.af_inet6.mc_loop = 0;
	sk->prot->unhash(sk);

	for (i = 0; i < IPV6_TUNNEL_MAXHOOKS; i++) {
		INIT_LIST_HEAD(&hooks[i]);
	}

	register_netdev(&ipv6_ipv6_fb_tunnel_dev);
	inet6_add_protocol(&ipv6_ipv6_protocol);
	return 0;
}

/**
 * ipv6_ipv6_tunnel_exit - free resources and unregister protocol
 **/

void __exit ipv6_ipv6_tunnel_exit(void)
{	
#ifdef TUNNEL_DEBUG
	printk(KERN_DEBUG "ipv6_ipv6_tunnel_exit\n");	
#endif
	write_lock_bh(&ipv6_ipv6_kernel_lock);
	shutdown = 1;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	flush_scheduled_tasks();
	manage_kernel_tunnels(NULL);
	inet6_del_protocol(&ipv6_ipv6_protocol);
	unregister_netdev(&ipv6_ipv6_fb_tunnel_dev);
	sock_release(ipv6_socket);
}

#ifdef MODULE
module_init(ipv6_ipv6_tunnel_init);
module_exit(ipv6_ipv6_tunnel_exit);
#endif

