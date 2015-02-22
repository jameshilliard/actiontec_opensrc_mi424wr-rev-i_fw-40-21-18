/*
 *	  Linux IPv6 multicast routing support for BSD pim6sd
 *
 *	(c) 2004 Mickael Hoerdt, <hoerdt@clarinet.u-strasbg.fr>
 *		LSIIT Laboratory, Strasbourg, France 
 *	(c) 2004 Jean-Philippe Andriot, <jean-philippe.andriot@6WIND.com>
 *		6WIND, Paris, France 
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Version: $Id: ip6mr.c,v 1.3 2006/06/15 21:52:44 itay Exp $
 *
 *	Fixes:
 *	Michael Chastain	:	Incorrect size of copying.
 *	Alan Cox		:	Added the cache manager code
 *	Alan Cox		:	Fixed the clone/copy bug and device race.
 *	Mike McLagan		:	Routing by source
 *	Malcolm Beattie		:	Buffer handling fixes.
 *	Alexey Kuznetsov	:	Double buffer free and other fixes.
 *	SVR Anand		:	Fixed several multicast bugs and problems.
 *	Alexey Kuznetsov	:	Status, optimisations and more.
 *	Brad Parker		:	Better behaviour on mrouted upcall
 *					overflow.
 *      Carlos Picoto           :       PIMv1 Support
 *	Pavlin Ivanov Radoslavov:	PIMv2 Registers must checksum only PIM header
 *					Relax this requirement to work with older peers.
 * 	Mickael Hoerdt and	: 	IPv6 support based on linux/net/ipv4/ipmr.c [Linux 2.x]
 *	Jean-Philippe Andriot 		     		   on netinet/ip6_mroute.c [*BSD]
 *
 */

#include <linux/config.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/igmp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mroute.h>
#include <linux/init.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/raw.h>
#include <net/route.h>
#include <linux/notifier.h>
#include <linux/if_arp.h>
#include <linux/netfilter_ipv4.h>
#include <net/ipip.h>
#include <net/checksum.h>


#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <linux/mroute6.h>
#include <net/addrconf.h>
#include <linux/netfilter_ipv6.h>

struct sock *mroute6_socket;


/* Big lock, protecting vif table, mrt cache and mroute socket state.
   Note that the changes are semaphored via rtnl_lock.
 */

static DEFINE_RWLOCK(mrt_lock);

/*
 *	Multicast router control variables
 */

static struct mif_device vif6_table[MAXMIFS];		/* Devices 		*/
static int maxvif;

#define MIF_EXISTS(idx) (vif6_table[idx].dev != NULL)

static int mroute_do_assert;				/* Set in PIM assert	*/
static int mroute_do_pim;

static struct mfc6_cache *mfc6_cache_array[MFC_LINES];	/* Forwarding cache	*/

static struct mfc6_cache *mfc_unres_queue;		/* Queue of unresolved entries */
static atomic_t cache_resolve_queue_len;		/* Size of unresolved	*/

/* Special spinlock for queue of unresolved entries */
static DEFINE_SPINLOCK(mfc_unres_lock);

/* We return to original Alan's scheme. Hash table of resolved
   entries is changed only in process context and protected
   with weak lock mrt_lock. Queue of unresolved entries is protected
   with strong spinlock mfc_unres_lock.

   In this case data path is free of exclusive locks at all.
 */

static kmem_cache_t *mrt_cachep;

static int ip6_mr_forward(struct sk_buff *skb, struct mfc6_cache *cache, int local);
static int ip6mr_cache_report(struct sk_buff *pkt, vifi_t vifi, int assert);
static int ip6mr_fill_mroute(struct sk_buff *skb, struct mfc6_cache *c, struct rtmsg *rtm);

static struct inet6_protocol pim6_protocol;

static struct timer_list ipmr_expire_timer;


#ifdef CONFIG_PROC_FS	

struct ipmr_mfc_iter {
	struct mfc6_cache **cache;
	int ct;
};


static struct mfc6_cache *ipmr_mfc_seq_idx(struct ipmr_mfc_iter *it, loff_t pos)
{
	struct mfc6_cache *mfc;

	it->cache = mfc6_cache_array;
	read_lock(&mrt_lock);
	for (it->ct = 0; it->ct < MFC_LINES; it->ct++) 
		for(mfc = mfc6_cache_array[it->ct]; mfc; mfc = mfc->next) 
			if (pos-- == 0) 
				return mfc;
	read_unlock(&mrt_lock);

	it->cache = &mfc_unres_queue;
	spin_lock_bh(&mfc_unres_lock);
	for(mfc = mfc_unres_queue; mfc; mfc = mfc->next) 
		if (pos-- == 0)
			return mfc;
	spin_unlock_bh(&mfc_unres_lock);

	it->cache = NULL;
	return NULL;
}




/*
 *	The /proc interfaces to multicast routing /proc/ip6_mr_cache /proc/ip6_mr_vif
 */

struct ipmr_vif_iter {
	int ct;
};

static struct mif_device *ip6mr_vif_seq_idx(struct ipmr_vif_iter *iter,
					   loff_t pos)
{
	for (iter->ct = 0; iter->ct < maxvif; ++iter->ct) {
		if(!MIF_EXISTS(iter->ct))
			continue;
		if (pos-- == 0) 
			return &vif6_table[iter->ct];
	}
	return NULL;
}

static void *ip6mr_vif_seq_start(struct seq_file *seq, loff_t *pos)
{
	read_lock(&mrt_lock);
	return *pos ? ip6mr_vif_seq_idx(seq->private, *pos - 1) 
		: SEQ_START_TOKEN;
}

static void *ip6mr_vif_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct ipmr_vif_iter *iter = seq->private;

	++*pos;
	if (v == SEQ_START_TOKEN)
		return ip6mr_vif_seq_idx(iter, 0);
	
	while (++iter->ct < maxvif) {
		if(!MIF_EXISTS(iter->ct))
			continue;
		return &vif6_table[iter->ct];
	}
	return NULL;
}

static void ip6mr_vif_seq_stop(struct seq_file *seq, void *v)
{
	read_unlock(&mrt_lock);
}

static int ip6mr_vif_seq_show(struct seq_file *seq, void *v)
{
	if (v == SEQ_START_TOKEN) {
		seq_puts(seq, 
			 "Interface      BytesIn  PktsIn  BytesOut PktsOut Flags\n");
	} else {
		const struct mif_device *vif = v;
		const char *name =  vif->dev ? vif->dev->name : "none";

		seq_printf(seq,
			   "%2Zd %-10s %8ld %7ld  %8ld %7ld %05X\n",
			   vif - vif6_table,
			   name, vif->bytes_in, vif->pkt_in, 
			   vif->bytes_out, vif->pkt_out,
			   vif->flags);
	}
	return 0;
}

static struct seq_operations ip6mr_vif_seq_ops = {
	.start = ip6mr_vif_seq_start,
	.next  = ip6mr_vif_seq_next,
	.stop  = ip6mr_vif_seq_stop,
	.show  = ip6mr_vif_seq_show,
};

static int ip6mr_vif_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq;
	int rc = -ENOMEM;
	struct ipmr_vif_iter *s = kmalloc(sizeof(*s), GFP_KERNEL);
       
	if (!s)
		goto out;

	rc = seq_open(file, &ip6mr_vif_seq_ops);
	if (rc)
		goto out_kfree;

	s->ct = 0;
	seq = file->private_data;
	seq->private = s;
out:
	return rc;
out_kfree:
	kfree(s);
	goto out;

}

static struct file_operations ip6mr_vif_fops = {
	.owner	 = THIS_MODULE,
	.open    = ip6mr_vif_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static void *ipmr_mfc_seq_start(struct seq_file *seq, loff_t *pos)
{
	return *pos ? ipmr_mfc_seq_idx(seq->private, *pos - 1) 
		: SEQ_START_TOKEN;
}

static void *ipmr_mfc_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct mfc6_cache *mfc = v;
	struct ipmr_mfc_iter *it = seq->private;

	++*pos;

	if (v == SEQ_START_TOKEN)
		return ipmr_mfc_seq_idx(seq->private, 0);

	if (mfc->next)
		return mfc->next;
	
	if (it->cache == &mfc_unres_queue) 
		goto end_of_list;

	BUG_ON(it->cache != mfc6_cache_array);

	while (++it->ct < MFC_LINES) {
		mfc = mfc6_cache_array[it->ct];
		if (mfc)
			return mfc;
	}

	/* exhausted cache_array, show unresolved */
	read_unlock(&mrt_lock);
	it->cache = &mfc_unres_queue;
	it->ct = 0;
		
	spin_lock_bh(&mfc_unres_lock);
	mfc = mfc_unres_queue;
	if (mfc) 
		return mfc;

 end_of_list:
	spin_unlock_bh(&mfc_unres_lock);
	it->cache = NULL;

	return NULL;
}

static void ipmr_mfc_seq_stop(struct seq_file *seq, void *v)
{
	struct ipmr_mfc_iter *it = seq->private;

	if (it->cache == &mfc_unres_queue)
		spin_unlock_bh(&mfc_unres_lock);
	else if (it->cache == mfc6_cache_array)
		read_unlock(&mrt_lock);
}

static int ipmr_mfc_seq_show(struct seq_file *seq, void *v)
{
	int n;

	if (v == SEQ_START_TOKEN) {
		seq_puts(seq, 
		 "Group                            Origin                           Iif      Pkts  Bytes     Wrong  Oifs\n");
	} else {
		const struct mfc6_cache *mfc = v;
		const struct ipmr_mfc_iter *it = seq->private;
		int i;

		for(i=0;i<16;i++) {
			seq_printf(seq,"%02x",mfc->mf6c_mcastgrp.s6_addr[i]);
		}
		seq_printf(seq," ");
		for(i=0;i<16;i++) {
			seq_printf(seq,"%02x",mfc->mf6c_origin.s6_addr[i]);
		}
		seq_printf(seq," ");

		seq_printf(seq, "%-3d %8ld %8ld %8ld",
			   mfc->mf6c_parent,
			   mfc->mfc_un.res.pkt,
			   mfc->mfc_un.res.bytes,
			   mfc->mfc_un.res.wrong_if);

		if (it->cache != &mfc_unres_queue) {
			for(n = mfc->mfc_un.res.minvif; 
			    n < mfc->mfc_un.res.maxvif; n++ ) {
				if(MIF_EXISTS(n) 
				   && mfc->mfc_un.res.ttls[n] < 255)
				seq_printf(seq, 
					   " %2d:%-3d", 
					   n, mfc->mfc_un.res.ttls[n]);
			}
		}
		seq_putc(seq, '\n');
	}
	return 0;
}

static struct seq_operations ipmr_mfc_seq_ops = {
	.start = ipmr_mfc_seq_start,
	.next  = ipmr_mfc_seq_next,
	.stop  = ipmr_mfc_seq_stop,
	.show  = ipmr_mfc_seq_show,
};

static int ipmr_mfc_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq;
	int rc = -ENOMEM;
	struct ipmr_mfc_iter *s = kmalloc(sizeof(*s), GFP_KERNEL);
       
	if (!s)
		goto out;

	rc = seq_open(file, &ipmr_mfc_seq_ops);
	if (rc)
		goto out_kfree;

	memset(s, 0, sizeof(*s));
	seq = file->private_data;
	seq->private = s;
out:
	return rc;
out_kfree:
	kfree(s);
	goto out;

}

static struct file_operations ip6mr_mfc_fops = {
	.owner	 = THIS_MODULE,
	.open    = ipmr_mfc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};
#endif

#ifdef CONFIG_IPV6_PIMSM_V2
static int reg_vif_num = -1;

static int pim6_rcv(struct sk_buff **pskb)
{
	struct pimreghdr *pim;
	struct ipv6hdr   *encap;
	struct sk_buff *skb = *pskb;
	struct net_device  *reg_dev = NULL;

	if (!pskb_may_pull(skb, sizeof(*pim) + sizeof(*encap))) 
		goto drop;

	pim = (struct pimreghdr*)skb->h.raw;
        if (pim->type != ((PIM_VERSION<<4)|(PIM_REGISTER)) ||
	    (pim->flags&PIM_NULL_REGISTER) ||
	    (ip_compute_csum((void *)pim, sizeof(*pim)) != 0 && 
	     (u16)csum_fold(skb_checksum(skb, 0, skb->len, 0)))) 
		goto drop;

	/* check if the inner packet is destined to mcast group */
	encap = (struct ipv6hdr*)(skb->h.raw + sizeof(struct pimreghdr));

	if(!(ipv6_addr_type(&encap->daddr)&IPV6_ADDR_MULTICAST) ||
	    encap->payload_len == 0 ||
	    ntohs(encap->payload_len) + sizeof(*pim) > skb->len) 
		goto drop;

	read_lock(&mrt_lock);
	if (reg_vif_num >= 0)
		reg_dev = vif6_table[reg_vif_num].dev;
	if (reg_dev)
		dev_hold(reg_dev);
	read_unlock(&mrt_lock);

	if (reg_dev == NULL) 
		goto drop;

	skb->mac.raw = skb->nh.raw;
	skb_pull(skb, (u8*)encap - skb->data);
	skb->nh.ipv6h = (struct ipv6hdr *)skb->data;
	skb->dev = reg_dev;
	skb->protocol = htons(ETH_P_IP);
	skb->ip_summed = 0;
	skb->pkt_type = PACKET_HOST;
	dst_release(skb->dst);
	((struct net_device_stats*)reg_dev->priv)->rx_bytes += skb->len;
	((struct net_device_stats*)reg_dev->priv)->rx_packets++;
	skb->dst = NULL;
#ifdef CONFIG_NETFILTER
	nf_conntrack_put(skb->nfct);
	skb->nfct = NULL;
#endif
	netif_rx(skb);
	dev_put(reg_dev);
	return 0;
 drop:
	kfree_skb(skb);
	return 0;
}

static struct inet6_protocol pim6_protocol = {
	.handler	=	pim6_rcv,
};
#endif

/* Service routines creating virtual interfaces: PIMREG */
#ifdef CONFIG_IPV6_PIMSM_V2


static int reg_vif_xmit(struct sk_buff *skb, struct net_device *dev)
{
	read_lock(&mrt_lock);
	((struct net_device_stats*)dev->priv)->tx_bytes += skb->len;
	((struct net_device_stats*)dev->priv)->tx_packets++;
	ip6mr_cache_report(skb, reg_vif_num, MRT6MSG_WHOLEPKT);
	read_unlock(&mrt_lock);
	kfree_skb(skb);
	return 0;
}

static struct net_device_stats *reg_vif_get_stats(struct net_device *dev)
{
	return (struct net_device_stats*)dev->priv;
}

static void reg_vif_setup(struct net_device *dev)
{
	dev->type		= ARPHRD_PIMREG;
	dev->mtu		= 1500 - sizeof(struct ipv6hdr) - 8;
	dev->flags		= IFF_NOARP;
	dev->hard_start_xmit	= reg_vif_xmit;
	dev->get_stats		= reg_vif_get_stats;
	dev->destructor		= free_netdev;
}

static struct net_device *ip6mr_reg_vif(void)
{
	struct net_device *dev;
	struct inet6_dev *in_dev;

	dev = alloc_netdev(sizeof(struct net_device_stats), "pim6reg",
			   reg_vif_setup);

	if (dev == NULL)
		return NULL;

	if (register_netdevice(dev)) {
		free_netdev(dev);
		return NULL;
	}
	dev->iflink = 0;

	if ((in_dev = ipv6_find_idev(dev)) == NULL) {
		goto failure;
	}
	
/*
 * 	if ((in_dev = __in6_dev_get(dev)) == NULL)
		goto failure;
*/
#if 0
	in_dev->cnf.rp_filter = 0;
#endif

	if (dev_open(dev))
		goto failure;

	return dev;

failure:
	/* allow the register to be completed before unregistering. */
	rtnl_unlock();
	rtnl_lock();

	unregister_netdevice(dev);
	return NULL;
}
#endif

/*
 *	Delete a VIF entry
 */
 
static int mif6_delete(int vifi)
{
	struct mif_device *v;
	struct net_device *dev;
	struct inet6_dev *in_dev;

	if (vifi < 0 || vifi >= maxvif)
		return -EADDRNOTAVAIL;

	v = &vif6_table[vifi];

	write_lock_bh(&mrt_lock);
	dev = v->dev;
	v->dev = NULL;

	if (!dev) {
		write_unlock_bh(&mrt_lock);
		return -EADDRNOTAVAIL;
	}

#ifdef CONFIG_IPV6_PIMSM_V2
	if (vifi == reg_vif_num)
		reg_vif_num = -1;
#endif

	if (vifi+1 == maxvif) {
		int tmp;
		for (tmp=vifi-1; tmp>=0; tmp--) {
			if (MIF_EXISTS(tmp))
				break;
		}
		maxvif = tmp+1;
	}

	write_unlock_bh(&mrt_lock);

	dev_set_allmulti(dev, -1);

	if ((in_dev = __in6_dev_get(dev)) != NULL) {
		in_dev->cnf.mc_forwarding--;
	}

	if (v->flags&(MIFF_REGISTER))
		unregister_netdevice(dev);

	dev_put(dev);
	return 0;
}

/* Destroy an unresolved cache entry, killing queued skbs
   and reporting error to netlink readers.
 */

static void ip6mr_destroy_unres(struct mfc6_cache *c)
{
	struct sk_buff *skb;

	atomic_dec(&cache_resolve_queue_len);

	while((skb=skb_dequeue(&c->mfc_un.unres.unresolved))) {
		if (skb->nh.ipv6h->version == 0) {
			struct nlmsghdr *nlh = (struct nlmsghdr *)skb_pull(skb, sizeof(struct ipv6hdr));
			nlh->nlmsg_type = NLMSG_ERROR;
			nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
			skb_trim(skb, nlh->nlmsg_len);
			((struct nlmsgerr*)NLMSG_DATA(nlh))->error = -ETIMEDOUT;
			netlink_unicast(rtnl, skb, NETLINK_CB(skb).dst_pid, MSG_DONTWAIT);
		} else
			kfree_skb(skb);
	}

	kmem_cache_free(mrt_cachep, c);
}


/* Single timer process for all the unresolved queue. */

static void ipmr_expire_process(unsigned long dummy)
{
	unsigned long now;
	unsigned long expires;
	struct mfc6_cache *c, **cp;

	if (!spin_trylock(&mfc_unres_lock)) {
		mod_timer(&ipmr_expire_timer, jiffies+HZ/10);
		return;
	}

	if (atomic_read(&cache_resolve_queue_len) == 0)
		goto out;

	now = jiffies;
	expires = 10*HZ;
	cp = &mfc_unres_queue;

	while ((c=*cp) != NULL) {
		if (time_after(c->mfc_un.unres.expires, now)) {
			unsigned long interval = c->mfc_un.unres.expires - now;
			if (interval < expires)
				expires = interval;
			cp = &c->next;
			continue;
		}

		*cp = c->next;

		ip6mr_destroy_unres(c);
	}

	if (atomic_read(&cache_resolve_queue_len))
		mod_timer(&ipmr_expire_timer, jiffies + expires);

out:
	spin_unlock(&mfc_unres_lock);
}

/* Fill oifs list. It is called under write locked mrt_lock. */

static void ip6mr_update_threshoulds(struct mfc6_cache *cache, unsigned char *ttls)
{
	int vifi;

	cache->mfc_un.res.minvif = MAXVIFS;
	cache->mfc_un.res.maxvif = 0;
	memset(cache->mfc_un.res.ttls, 255, MAXVIFS);

	for (vifi=0; vifi<maxvif; vifi++) {
		if (MIF_EXISTS(vifi) && ttls[vifi] && ttls[vifi] < 255) {
			cache->mfc_un.res.ttls[vifi] = ttls[vifi];
			if (cache->mfc_un.res.minvif > vifi)
				cache->mfc_un.res.minvif = vifi;
			if (cache->mfc_un.res.maxvif <= vifi)
				cache->mfc_un.res.maxvif = vifi + 1;
		}
	}
}

static int mif6_add(struct mif6ctl *vifc, int mrtsock)
{
	int vifi = vifc->mif6c_mifi;
	struct mif_device *v = &vif6_table[vifi];
	struct net_device *dev;
	struct inet6_dev *in_dev;

	/* Is vif busy ? */
	if (MIF_EXISTS(vifi))
		return -EADDRINUSE;

	switch (vifc->mif6c_flags) {
#ifdef CONFIG_IPV6_PIMSM_V2
	case MIFF_REGISTER:
		/*
		 * Special Purpose VIF in PIM
		 * All the packets will be sent to the daemon
		 */
		if (reg_vif_num >= 0)
			return -EADDRINUSE;
		dev = ip6mr_reg_vif();
		if (!dev)
			return -ENOBUFS;
		break;
#endif
	case 0:
		dev=dev_get_by_index(vifc->mif6c_pifi);
		if (!dev)
			return -EADDRNOTAVAIL;
		__dev_put(dev);
		break;
	default:
		return -EINVAL;
	}

	if ((in_dev = __in6_dev_get(dev)) == NULL)
		return -EADDRNOTAVAIL;
	in_dev->cnf.mc_forwarding++;
	dev_set_allmulti(dev, +1);

	/*
	 *	Fill in the VIF structures
	 */
	v->rate_limit=vifc->vifc_rate_limit;
	v->flags=vifc->mif6c_flags;
	if(!mrtsock)
		v->flags |= VIFF_STATIC;
	v->threshold=vifc->vifc_threshold;
	v->bytes_in = 0;
	v->bytes_out = 0;
	v->pkt_in = 0;
	v->pkt_out = 0;
	v->link = dev->ifindex;
	if (v->flags&(MIFF_REGISTER))
		v->link = dev->iflink;

	/* And finish update writing critical data */
	write_lock_bh(&mrt_lock);
	dev_hold(dev);
	v->dev=dev;
#ifdef CONFIG_IPV6_PIMSM_V2
	if (v->flags&MIFF_REGISTER)
		reg_vif_num = vifi;
#endif
	if (vifi+1 > maxvif)
		maxvif = vifi+1;
	write_unlock_bh(&mrt_lock);
	return 0;
}

static struct mfc6_cache *ip6mr_cache_find(struct in6_addr origin,struct in6_addr mcastgrp)
{
	int line=MFC6_HASH(mcastgrp,origin);
	struct mfc6_cache *c;

	for (c=mfc6_cache_array[line]; c; c = c->next) {
		if (IN6_ARE_ADDR_EQUAL(&c->mf6c_origin,&origin) && 
		    IN6_ARE_ADDR_EQUAL(&c->mf6c_mcastgrp,&mcastgrp))
			break;
	}
	return c;
}

/*
 *	Allocate a multicast cache entry
 */
static struct mfc6_cache *ip6mr_cache_alloc(void)
{
	struct mfc6_cache *c=kmem_cache_alloc(mrt_cachep, GFP_KERNEL);
	if(c==NULL)
		return NULL;
	memset(c, 0, sizeof(*c));
	c->mfc_un.res.minvif = MAXVIFS;
	return c;
}

static struct mfc6_cache *ip6mr_cache_alloc_unres(void)
{
	struct mfc6_cache *c=kmem_cache_alloc(mrt_cachep, GFP_ATOMIC);
	if(c==NULL)
		return NULL;
	memset(c, 0, sizeof(*c));
	skb_queue_head_init(&c->mfc_un.unres.unresolved);
	c->mfc_un.unres.expires = jiffies + 10*HZ;
	return c;
}

/*
 *	A cache entry has gone into a resolved state from queued
 */
 
static void ip6mr_cache_resolve(struct mfc6_cache *uc, struct mfc6_cache *c)
{
	struct sk_buff *skb;

	/*
	 *	Play the pending entries through our router
	 */

	while((skb=__skb_dequeue(&uc->mfc_un.unres.unresolved))) {
		if (skb->nh.ipv6h->version == 0) {
			int err;
			struct nlmsghdr *nlh = (struct nlmsghdr *)skb_pull(skb, sizeof(struct ipv6hdr));

			if (ip6mr_fill_mroute(skb, c, NLMSG_DATA(nlh)) > 0) {
				nlh->nlmsg_len = skb->tail - (u8*)nlh;
			} else {
				nlh->nlmsg_type = NLMSG_ERROR;
				nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct nlmsgerr));
				skb_trim(skb, nlh->nlmsg_len);
				((struct nlmsgerr*)NLMSG_DATA(nlh))->error = -EMSGSIZE;
			}
			err = netlink_unicast(rtnl, skb, NETLINK_CB(skb).dst_pid, MSG_DONTWAIT);
		} else
			ip6_mr_forward(skb, c, 0);
	}
}

/*
 *	Bounce a cache query up to pim6sd. We could use netlink for this but pim6sd
 *	expects the following bizarre scheme.
 *
 *	Called under mrt_lock.
 */
 
static int ip6mr_cache_report(struct sk_buff *pkt, vifi_t vifi, int assert)
{
	struct sk_buff *skb;
	struct mrt6msg *msg;
	int ret;

#ifdef CONFIG_IPV6_PIMSM_V2
	if (assert == MRT6MSG_WHOLEPKT)
		skb = skb_realloc_headroom(pkt, sizeof(struct ipv6hdr));
	else
#endif
		skb = alloc_skb(128, GFP_ATOMIC);

	if(!skb)
		return -ENOBUFS;

	/* I suppose that internal messages 
	 * do not require checksums */

	skb->ip_summed = CHECKSUM_UNNECESSARY;

#ifdef CONFIG_IPV6_PIMSM_V2
	if (assert == MRT6MSG_WHOLEPKT) {
		/* Ugly, but we have no choice with this interface.
		   Duplicate old header, fix length etc.
		   And all this only to mangle msg->im6_msgtype and
		   to set msg->im6_mbz to "mbz" :-)
		 */
		msg = (struct mrt6msg*)skb_push(skb, sizeof(struct ipv6hdr));
		skb->nh.raw = skb->h.raw = (u8*)msg;
		memcpy(msg, pkt->nh.raw, sizeof(struct ipv6hdr));
		msg->im6_msgtype = MRT6MSG_WHOLEPKT;
		msg->im6_mbz = 0;
 		msg->im6_mif = reg_vif_num;
	} else 
#endif
	{	
		
	/*
	 *	Copy the IP header
	 */

	skb->nh.ipv6h = (struct ipv6hdr *)skb_put(skb, sizeof(struct ipv6hdr)); 
	memcpy(skb->data,pkt->data,sizeof(struct ipv6hdr)); 

	msg = (struct mrt6msg*)skb->nh.ipv6h; 
	skb->dst = dst_clone(pkt->dst);

	/*
	 *	Add our header
	 */

	msg->im6_msgtype = assert;
	msg->im6_mbz = 0;
	msg->im6_mif = vifi;
	skb->h.raw = skb->nh.raw;
        }

	if (mroute6_socket == NULL) {
		kfree_skb(skb);
		return -EINVAL;
	}

	/*
	 *	Deliver to user space multicast routing algorithms
	 */
	if ((ret=sock_queue_rcv_skb(mroute6_socket,skb))<0) {
		if (net_ratelimit())
			printk(KERN_WARNING "mroute6: pending queue full, dropping entries.\n");
		kfree_skb(skb);
	}

	return ret;
}

/*
 *	Queue a packet for resolution. It gets locked cache entry!
 */
 
static int
ip6mr_cache_unresolved(vifi_t vifi, struct sk_buff *skb)
{
	int err;
	struct mfc6_cache *c;

	spin_lock_bh(&mfc_unres_lock);
	for (c=mfc_unres_queue; c; c=c->next) {
		if (IN6_ARE_ADDR_EQUAL(&c->mf6c_mcastgrp,&skb->nh.ipv6h->daddr) && 
		    IN6_ARE_ADDR_EQUAL(&c->mf6c_origin,&skb->nh.ipv6h->saddr))
			break;
	}

	if (c == NULL) {
		/*
		 *	Create a new entry if allowable
		 */

		if (atomic_read(&cache_resolve_queue_len)>=10 ||
		    (c=ip6mr_cache_alloc_unres())==NULL) {
			spin_unlock_bh(&mfc_unres_lock);

			kfree_skb(skb);
			return -ENOBUFS;
		}

		/*
		 *	Fill in the new cache entry
		 */
		c->mf6c_parent=-1;
		c->mf6c_origin=skb->nh.ipv6h->saddr;
		c->mf6c_mcastgrp=skb->nh.ipv6h->daddr;

		/*
		 *	Reflect first query at pim6sd
		 */
		if ((err = ip6mr_cache_report(skb, vifi, MRT6MSG_NOCACHE))<0) {
			/* If the report failed throw the cache entry 
			   out - Brad Parker
			 */
			spin_unlock_bh(&mfc_unres_lock);

			kmem_cache_free(mrt_cachep, c);
			kfree_skb(skb);
			return err;
		}

		atomic_inc(&cache_resolve_queue_len);
		c->next = mfc_unres_queue;
		mfc_unres_queue = c;

		mod_timer(&ipmr_expire_timer, c->mfc_un.unres.expires);
	}

	/*
	 *	See if we can append the packet
	 */
	if (c->mfc_un.unres.unresolved.qlen>3) {
		kfree_skb(skb);
		err = -ENOBUFS;
	} else {
		skb_queue_tail(&c->mfc_un.unres.unresolved,skb);
		err = 0;
	}

	spin_unlock_bh(&mfc_unres_lock);
	return err;
}

/*
 *	MFC6 cache manipulation by user space 
 */

static int ip6mr_mfc_delete(struct mf6cctl *mfc)
{
	int line;
	struct mfc6_cache *c, **cp;

	line=MFC6_HASH(mfc->mf6cc_mcastgrp.sin6_addr, mfc->mf6cc_origin.sin6_addr);

	for (cp=&mfc6_cache_array[line]; (c=*cp) != NULL; cp = &c->next) {
		if (IN6_ARE_ADDR_EQUAL(&c->mf6c_origin,&mfc->mf6cc_origin.sin6_addr) && 
		    IN6_ARE_ADDR_EQUAL(&c->mf6c_mcastgrp,&mfc->mf6cc_mcastgrp.sin6_addr)) {
			write_lock_bh(&mrt_lock);
			*cp = c->next;
			write_unlock_bh(&mrt_lock);

			kmem_cache_free(mrt_cachep, c);
			return 0;
		}
	}
	return -ENOENT;
}

static int ip6mr_device_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct mif_device *v;
	int ct;
	if (event != NETDEV_UNREGISTER)
		return NOTIFY_DONE;
	v=&vif6_table[0];
	for(ct=0;ct<maxvif;ct++,v++) {
		if (v->dev==ptr)
			mif6_delete(ct);
	}
	return NOTIFY_DONE;
}

static struct notifier_block ip6_mr_notifier = {
	.notifier_call = ip6mr_device_event
};

/*
 *	Setup for IP multicast routing
 */
 
void __init ip6_mr_init(void)
{
	printk(KERN_INFO "6WIND/LSIIT IPv6 multicast forwarding 0.1 plus PIM-SM/SSM with *BSD API\n");
	
	mrt_cachep = kmem_cache_create("ip6_mrt_cache",
				       sizeof(struct mfc6_cache),
				       0, SLAB_HWCACHE_ALIGN,
				       NULL, NULL);
	if (!mrt_cachep)
		panic("cannot allocate ip_mrt_cache");

	init_timer(&ipmr_expire_timer);
	ipmr_expire_timer.function=ipmr_expire_process;
	register_netdevice_notifier(&ip6_mr_notifier);
#ifdef CONFIG_PROC_FS	
	proc_net_fops_create("ip6_mr_vif", 0, &ip6mr_vif_fops);
	proc_net_fops_create("ip6_mr_cache", 0, &ip6mr_mfc_fops);
#endif	
}


static int ip6mr_mfc_add(struct mf6cctl *mfc, int mrtsock)
{
	int line;
	struct mfc6_cache *uc, *c, **cp;
	unsigned char ttls[MAXVIFS];
	int i;

	memset(ttls, 255, MAXVIFS);
	for(i=0;i<MAXVIFS;i++) {
		if(IF_ISSET(i,&mfc->mf6cc_ifset)) 
			ttls[i]=1;
		
	}

	line=MFC6_HASH(mfc->mf6cc_mcastgrp.sin6_addr, mfc->mf6cc_origin.sin6_addr);

	for (cp=&mfc6_cache_array[line]; (c=*cp) != NULL; cp = &c->next) {
		if (IN6_ARE_ADDR_EQUAL(&c->mf6c_origin,&mfc->mf6cc_origin.sin6_addr) && 
		    IN6_ARE_ADDR_EQUAL(&c->mf6c_mcastgrp,&mfc->mf6cc_mcastgrp.sin6_addr)) 
			break;
	}

	if (c != NULL) {
		write_lock_bh(&mrt_lock);
		c->mf6c_parent = mfc->mf6cc_parent;
		ip6mr_update_threshoulds(c, ttls);
		if (!mrtsock)
			c->mfc_flags |= MFC_STATIC;
		write_unlock_bh(&mrt_lock);
		return 0;
	}

	if(!(ipv6_addr_type(&mfc->mf6cc_mcastgrp.sin6_addr)&IPV6_ADDR_MULTICAST))
		return -EINVAL;

	c=ip6mr_cache_alloc();
	if (c==NULL)
		return -ENOMEM;

	c->mf6c_origin=mfc->mf6cc_origin.sin6_addr;
	c->mf6c_mcastgrp=mfc->mf6cc_mcastgrp.sin6_addr;
	c->mf6c_parent=mfc->mf6cc_parent;
	ip6mr_update_threshoulds(c, ttls);
	if (!mrtsock)
		c->mfc_flags |= MFC_STATIC;

	write_lock_bh(&mrt_lock);
	c->next = mfc6_cache_array[line];
	mfc6_cache_array[line] = c;
	write_unlock_bh(&mrt_lock);

	/*
	 *	Check to see if we resolved a queued list. If so we
	 *	need to send on the frames and tidy up.
	 */
	spin_lock_bh(&mfc_unres_lock);
	for (cp = &mfc_unres_queue; (uc=*cp) != NULL;
	     cp = &uc->next) {
		if (IN6_ARE_ADDR_EQUAL(&uc->mf6c_origin,&c->mf6c_origin) && 
		    IN6_ARE_ADDR_EQUAL(&uc->mf6c_mcastgrp,&c->mf6c_mcastgrp)) {
			*cp = uc->next;
			if (atomic_dec_and_test(&cache_resolve_queue_len))
				del_timer(&ipmr_expire_timer);
			break;
		}
	}
	spin_unlock_bh(&mfc_unres_lock);

	if (uc) {
		ip6mr_cache_resolve(uc, c);
		kmem_cache_free(mrt_cachep, uc);
	}
	return 0;
}

/*
 *	Close the multicast socket, and clear the vif tables etc
 */
 
static void mroute_clean_tables(struct sock *sk)
{
	int i;
		
	/*
	 *	Shut down all active vif entries
	 */
	for(i=0; i<maxvif; i++) {
		if (!(vif6_table[i].flags&VIFF_STATIC))
			mif6_delete(i);
	}

	/*
	 *	Wipe the cache
	 */
	for (i=0;i<MFC_LINES;i++) {
		struct mfc6_cache *c, **cp;

		cp = &mfc6_cache_array[i];
		while ((c = *cp) != NULL) {
			if (c->mfc_flags&MFC_STATIC) {
				cp = &c->next;
				continue;
			}
			write_lock_bh(&mrt_lock);
			*cp = c->next;
			write_unlock_bh(&mrt_lock);

			kmem_cache_free(mrt_cachep, c);
		}
	}

	if (atomic_read(&cache_resolve_queue_len) != 0) {
		struct mfc6_cache *c;

		spin_lock_bh(&mfc_unres_lock);
		while (mfc_unres_queue != NULL) {
			c = mfc_unres_queue;
			mfc_unres_queue = c->next;
			spin_unlock_bh(&mfc_unres_lock);

			ip6mr_destroy_unres(c);

			spin_lock_bh(&mfc_unres_lock);
		}
		spin_unlock_bh(&mfc_unres_lock);
	}
}

static void mrtsock_destruct(struct sock *sk)
{
	rtnl_lock();
	if (sk == mroute6_socket) {
		ipv6_devconf.mc_forwarding--;

		write_lock_bh(&mrt_lock);
		mroute6_socket=NULL;
		write_unlock_bh(&mrt_lock);

		mroute_clean_tables(sk);
	}
	rtnl_unlock();
}

/*
 *	Socket options and virtual interface manipulation. The whole
 *	virtual interface system is a complete heap, but unfortunately
 *	that's how BSD mrouted happens to think. Maybe one day with a proper
 *	MOSPF/PIM router set up we can clean this up.
 */
 
int ip6_mroute_setsockopt(struct sock *sk,int optname,char __user *optval,int optlen)
{
	int ret;
	struct mif6ctl vif;
	struct mf6cctl mfc;
	mifi_t mifi;
	
	if(optname!=MRT6_INIT)
	{
		if(sk!=mroute6_socket && !capable(CAP_NET_ADMIN))
			return -EACCES;
	}

	switch(optname)
	{
		case MRT6_INIT:
			if (sk->sk_type != SOCK_RAW ||
			    inet_sk(sk)->num != IPPROTO_ICMPV6)
				return -EOPNOTSUPP;
			if(optlen!=sizeof(int))
				return -ENOPROTOOPT;

			rtnl_lock();
			if (mroute6_socket) {
				rtnl_unlock();
				return -EADDRINUSE;
			}

			ret = ip6_ra_control(sk, 1, mrtsock_destruct);
			if (ret == 0) {
				write_lock_bh(&mrt_lock);
				mroute6_socket=sk;
				write_unlock_bh(&mrt_lock);

				ipv6_devconf.mc_forwarding++;
			}
			rtnl_unlock();
			return ret;
		case MRT6_DONE:
			if (sk!=mroute6_socket)
				return -EACCES;
			return ip6_ra_control(sk, -1, NULL);
		case MRT6_ADD_MIF:
			if(optlen!=sizeof(vif))
				return -EINVAL;
			if (copy_from_user(&vif,optval,sizeof(vif)))
				return -EFAULT; 
			if(vif.mif6c_mifi >= MAXVIFS)
				return -ENFILE;
			rtnl_lock();
			ret = mif6_add(&vif, sk==mroute6_socket);
			rtnl_unlock();
			return ret;
		case MRT6_DEL_MIF:
			if(optlen!=sizeof(mifi_t))
				return -EINVAL;
			if (copy_from_user(&mifi,optval,sizeof(mifi_t)))
				return -EFAULT; 
			rtnl_lock();
			ret = mif6_delete(mifi);
			rtnl_unlock();
			return ret;

		/*
		 *	Manipulate the forwarding caches. These live
		 *	in a sort of kernel/user symbiosis.
		 */
		case MRT6_ADD_MFC:
		case MRT6_DEL_MFC:
			if(optlen!=sizeof(mfc))
				return -EINVAL;
			if (copy_from_user(&mfc,optval, sizeof(mfc)))
				return -EFAULT;
			rtnl_lock();
			if (optname==MRT6_DEL_MFC)
				ret = ip6mr_mfc_delete(&mfc);
			else
				ret = ip6mr_mfc_add(&mfc, sk==mroute6_socket);
			rtnl_unlock();
			return ret;
		/*
		 *	Control PIM assert (to activate pim will activate assert)
		 */
		case MRT6_ASSERT:
		{
			int v;
			if(get_user(v,(int __user *)optval))
				return -EFAULT;
			mroute_do_assert=(v)?1:0;
			return 0;
		}
#ifdef CONFIG_IPV6_PIMSM_V2
		case MRT6_PIM:
		{
			int v, ret;
			if(get_user(v,(int __user *)optval))
				return -EFAULT;
			v = (v)?1:0;
			rtnl_lock();
			ret = 0;
			if (v != mroute_do_pim) {
				mroute_do_pim = v;
				mroute_do_assert = v;
				if (mroute_do_pim)
					ret = inet6_add_protocol(&pim6_protocol,
								IPPROTO_PIM);
				else
					ret = inet6_del_protocol(&pim6_protocol,
								IPPROTO_PIM);
				if (ret < 0)
					ret = -EAGAIN;
			}
			rtnl_unlock();
			return ret;
		}
#endif
		/*
		 *	Spurious command, or MRT_VERSION which you cannot
		 *	set.
		 */
		default:
			return -ENOPROTOOPT;
	}
}

/*
 *	Getsock opt support for the multicast routing system.
 */
 
int ip6_mroute_getsockopt(struct sock *sk,int optname,char __user *optval,int __user *optlen)
{
	int olr;
	int val;

	if(optname!=MRT6_VERSION && 
#ifdef CONFIG_IPV6_PIMSM_V2
	   optname!=MRT6_PIM &&
#endif
	   optname!=MRT6_ASSERT)
		return -ENOPROTOOPT;

	if (get_user(olr, optlen))
		return -EFAULT;

	olr = min_t(unsigned int, olr, sizeof(int));
	if (olr < 0)
		return -EINVAL;
		
	if(put_user(olr,optlen))
		return -EFAULT;
	if(optname==MRT6_VERSION)
		val=0x0305;
#ifdef CONFIG_IPV6_PIMSM_V2
	else if(optname==MRT6_PIM)
		val=mroute_do_pim;
#endif
	else
		val=mroute_do_assert;
	if(copy_to_user(optval,&val,olr))
		return -EFAULT;
	return 0;
}

/*
 *	The IP multicast ioctl support routines.
 */
 
int ip6mr_ioctl(struct sock *sk, int cmd, void __user *arg)
{
	struct sioc_sg_req6 sr;
	struct sioc_mif_req6 vr;
	struct mif_device *vif;
	struct mfc6_cache *c;
	
	switch(cmd)
	{
		case SIOCGETMIFCNT_IN6:
			if (copy_from_user(&vr,arg,sizeof(vr)))
				return -EFAULT; 
			if(vr.mifi>=maxvif)
				return -EINVAL;
			read_lock(&mrt_lock);
			vif=&vif6_table[vr.mifi];
			if(MIF_EXISTS(vr.mifi))	{
				vr.icount=vif->pkt_in;
				vr.ocount=vif->pkt_out;
				vr.ibytes=vif->bytes_in;
				vr.obytes=vif->bytes_out;
				read_unlock(&mrt_lock);

				if (copy_to_user(arg,&vr,sizeof(vr)))
					return -EFAULT;
				return 0;
			}
			read_unlock(&mrt_lock);
			return -EADDRNOTAVAIL;
		case SIOCGETSGCNT_IN6:
			if (copy_from_user(&sr,arg,sizeof(sr)))
				return -EFAULT;

			read_lock(&mrt_lock);
			c = ip6mr_cache_find(sr.src.sin6_addr, sr.grp.sin6_addr);
			if (c) {
				sr.pktcnt = c->mfc_un.res.pkt;
				sr.bytecnt = c->mfc_un.res.bytes;
				sr.wrong_if = c->mfc_un.res.wrong_if;
				read_unlock(&mrt_lock);

				if (copy_to_user(arg,&sr,sizeof(sr)))
					return -EFAULT;
				return 0;
			}
			read_unlock(&mrt_lock);
			return -EADDRNOTAVAIL;
		default:
			return -ENOIOCTLCMD;
	}
}


static inline int ip6mr_forward_finish(struct sk_buff *skb)
{
#ifdef notyet
	struct ip_options * opt	= &(IP6CB(skb)->opt);

	IP_INC_STATS_BH(OutForwDatagrams);

	if (unlikely(opt->optlen))
		ip_forward_options(skb);
#endif

	return dst_output(skb);
}

/*
 *	Processing handlers for ip6mr_forward
 */

static void ip6mr_queue_xmit(struct sk_buff *skb, struct mfc6_cache *c, int vifi)
{
	struct ipv6hdr *ipv6h = skb->nh.ipv6h;
	struct mif_device *vif = &vif6_table[vifi];
	struct net_device *dev;
#if 0
	struct rtable *rt;
	int    encap = 0;
#endif
	struct in6_addr *snd_addr=&ipv6h->daddr;
	int full_len = skb->len;

	if (vif->dev == NULL)
		goto out_free;

#ifdef CONFIG_IPV6_PIMSM_V2
	if (vif->flags & MIFF_REGISTER) {
		vif->pkt_out++;
		vif->bytes_out+=skb->len;
		((struct net_device_stats*)vif->dev->priv)->tx_bytes += skb->len;
		((struct net_device_stats*)vif->dev->priv)->tx_packets++;
		ip6mr_cache_report(skb, vifi, MRT6MSG_WHOLEPKT);
		kfree_skb(skb);
		return;
	}
#endif
	/*
	 * RFC1584 teaches, that DVMRP/PIM router must deliver packets locally
	 * not only before forwarding, but after forwarding on all output
	 * interfaces. It is clear, if mrouter runs a multicasting
	 * program, it should receive packets not depending to what interface
	 * program is joined.
	 * If we will not make it, the program will have to join on all
	 * interfaces. On the other hand, multihoming host (or router, but
	 * not mrouter) cannot join to more than one interface - it will
	 * result in receiving multiple packets.
	 */
	dev = vif->dev;
	skb->dev=dev;
	vif->pkt_out++;
	vif->bytes_out+=skb->len;

	ipv6h = skb->nh.ipv6h;

	ipv6h->hop_limit--;

	if(dev->hard_header) {
		unsigned char ha[MAX_ADDR_LEN];
		ndisc_mc_map(snd_addr,ha,dev,1);
		if(dev->hard_header(skb,dev, ETH_P_IPV6,ha,NULL,full_len) < 0)
			goto out_free;
	}	
	
	NF_HOOK(PF_INET6, NF_IP6_LOCAL_OUT, skb, skb->dev, dev, 
		dev_queue_xmit);
/*	NF_HOOK(PF_INET6, NF_IP6_POST_ROUTING, skb, skb->dev, dev, 
		ip6mr_forward_finish);
*/
	

 /* 	NF_HOOK(PF_INET6, NF_IP6_FORWARD, skb, skb->dev, dev, 
		ip6mr_forward_finish);
	*/
	return;
	/* XXX */

out_free:
	kfree_skb(skb);
	return;
}

static int ip6mr_find_vif(struct net_device *dev)
{
	int ct;
	for (ct=maxvif-1; ct>=0; ct--) {
		if (vif6_table[ct].dev == dev)
			break;
	}
	return ct;
}

static int ip6_mr_forward(struct sk_buff *skb, struct mfc6_cache *cache, int local)
{
	int psend = -1;
	int vif, ct;

	vif = cache->mf6c_parent;
	cache->mfc_un.res.pkt++;
	cache->mfc_un.res.bytes += skb->len;

	/*
	 * Wrong interface: drop packet and (maybe) send PIM assert.
	 */
	if (vif6_table[vif].dev != skb->dev) {
		int true_vifi;

		if (((struct rtable*)skb->dst)->fl.iif == 0) {
			/* It is our own packet, looped back.
			   Very complicated situation...

			   The best workaround until routing daemons will be
			   fixed is not to redistribute packet, if it was
			   send through wrong interface. It means, that
			   multicast applications WILL NOT work for
			   (S,G), which have default multicast route pointing
			   to wrong oif. In any case, it is not a good
			   idea to use multicasting applications on router.
			 */
			goto dont_forward;
		}

		cache->mfc_un.res.wrong_if++;
		true_vifi = ip6mr_find_vif(skb->dev);

		if (true_vifi >= 0 && mroute_do_assert &&
		    /* pimsm uses asserts, when switching from RPT to SPT,
		       so that we cannot check that packet arrived on an oif.
		       It is bad, but otherwise we would need to move pretty
		       large chunk of pimd to kernel. Ough... --ANK
		     */
		    (mroute_do_pim || cache->mfc_un.res.ttls[true_vifi] < 255) &&
		    time_after(jiffies, 
			       cache->mfc_un.res.last_assert + MFC_ASSERT_THRESH)) {
			cache->mfc_un.res.last_assert = jiffies;
			ip6mr_cache_report(skb, true_vifi, MRT6MSG_WRONGMIF);
		}
		goto dont_forward;
	}

	vif6_table[vif].pkt_in++;
	vif6_table[vif].bytes_in+=skb->len;

	/*
	 *	Forward the frame
	 */
	for (ct = cache->mfc_un.res.maxvif-1; ct >= cache->mfc_un.res.minvif; ct--) {
		if (skb->nh.ipv6h->hop_limit > cache->mfc_un.res.ttls[ct]) {
			struct ipv6hdr *ipv6h = skb->nh.ipv6h;
			if (psend != -1) {
				struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
				if (skb2) {
					ip6mr_queue_xmit(skb2, cache, psend);
					ipv6h->hop_limit++;
				}
			}
			psend=ct;
		}
	}
	if (psend != -1) {
		struct ipv6hdr *ipv6h = skb->nh.ipv6h;
		if (local) {
			struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);
			if (skb2) {
				ip6mr_queue_xmit(skb2, cache, psend);
				ipv6h->hop_limit++;
			}
		} else {
			ip6mr_queue_xmit(skb, cache, psend);
			ipv6h->hop_limit++;
			return 0;
		}
	}

dont_forward:
	if (!local)
		kfree_skb(skb);
	return 0;
}


/*
 *	Multicast packets for forwarding arrive here
 */

int ip6_mr_input(struct sk_buff *skb)
{
	struct mfc6_cache *cache;
	int local = ((struct rt6_info*)skb->dst)->rt6i_flags&RTCF_LOCAL;
#if 0
	IP6CB(skb)->flags = 0; 
#endif

	read_lock(&mrt_lock);
	cache = ip6mr_cache_find(skb->nh.ipv6h->saddr, skb->nh.ipv6h->daddr);

	/*
	 *	No usable cache entry
	 */
	if (cache==NULL) {
		int vif;

		vif = ip6mr_find_vif(skb->dev);
		if (vif >= 0) {
			int err = ip6mr_cache_unresolved(vif, skb);
			read_unlock(&mrt_lock);

			return err;
		}
		read_unlock(&mrt_lock);
		kfree_skb(skb);
		return -ENODEV;
	}

	ip6_mr_forward(skb, cache, local);

	read_unlock(&mrt_lock);

	return 0;
#if 0
dont_forward:
	kfree_skb(skb);
	return 0;
#endif
}


static int
ip6mr_fill_mroute(struct sk_buff *skb, struct mfc6_cache *c, struct rtmsg *rtm)
{
	int ct;
	struct rtnexthop *nhp;
	struct net_device *dev = vif6_table[c->mf6c_parent].dev;
	u8 *b = skb->tail;
	struct rtattr *mp_head;

	if (dev)
		RTA_PUT(skb, RTA_IIF, 4, &dev->ifindex);

	mp_head = (struct rtattr*)skb_put(skb, RTA_LENGTH(0));

	for (ct = c->mfc_un.res.minvif; ct < c->mfc_un.res.maxvif; ct++) {
		if (c->mfc_un.res.ttls[ct] < 255) {
			if (skb_tailroom(skb) < RTA_ALIGN(RTA_ALIGN(sizeof(*nhp)) + 4))
				goto rtattr_failure;
			nhp = (struct rtnexthop*)skb_put(skb, RTA_ALIGN(sizeof(*nhp)));
			nhp->rtnh_flags = 0;
			nhp->rtnh_hops = c->mfc_un.res.ttls[ct];
			nhp->rtnh_ifindex = vif6_table[ct].dev->ifindex;
			nhp->rtnh_len = sizeof(*nhp);
		}
	}
	mp_head->rta_type = RTA_MULTIPATH;
	mp_head->rta_len = skb->tail - (u8*)mp_head;
	rtm->rtm_type = RTN_MULTICAST;
	return 1;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -EMSGSIZE;
}
