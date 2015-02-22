/*
 * Interface between the IPSEC code and the radix (radij) tree code
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

#include <linux/module.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h> /* printk() */

#include "ipsec_param.h"

#ifdef MALLOC_SLAB
# include <linux/slab.h> /* kmalloc() */
#else /* MALLOC_SLAB */
# include <linux/malloc.h> /* kmalloc() */
#endif /* MALLOC_SLAB */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/interrupt.h> /* mark_bh */

#include <linux/netdevice.h>   /* struct net_device, struct net_device_stats and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/skbuff.h>
#include <freeswan.h>
#ifdef SPINLOCK
# ifdef SPINLOCK_23
#  include <linux/spinlock.h> /* *lock* */
# else /* 23_SPINLOCK */
#  include <asm/spinlock.h> /* *lock* */
# endif /* 23_SPINLOCK */
#endif /* SPINLOCK */
#ifdef NET_21
# include <asm/uaccess.h>
# include <linux/in6.h>
#endif
#include <asm/checksum.h>
#include <net/ip.h>

#include "ipsec_eroute.h"
#include "ipsec_sa.h"
 
#include "radij.h"
#include "ipsec_encap.h"
#include "radij.h"
#include "ipsec_encap.h"
#include "ipsec_radij.h"
#include "ipsec_netlink.h"
#include "ipsec_tunnel.h"	/* struct ipsecpriv */
#include "ipsec_xform.h"
 
#include <pfkeyv2.h>
#include <pfkey.h>

#include "ipsec_proto.h"
#include "ipsec_log.h"

#ifdef CONFIG_IPSEC_DEBUG
int debug_radij = 0;
#endif /* CONFIG_IPSEC_DEBUG */

struct radij_node_head *rnh = NULL;
#ifdef SPINLOCK
spinlock_t eroute_lock = SPIN_LOCK_UNLOCKED;
#else /* SPINLOCK */
spinlock_t eroute_lock;
#endif /* SPINLOCK */
EXPORT_SYMBOL(eroute_lock);

int
ipsec_radijinit(void)
{
	maj_keylen = sizeof (struct sockaddr_encap);

	rj_init();
	
	if (rj_inithead((void **)&rnh, /*16*/offsetof(struct sockaddr_encap, sen_type) * sizeof(__u8)) == 0) /* 16 is bit offset of sen_type */
		return -1;
	return 0;
}

int
ipsec_radijcleanup(void)
{
	int error;

	spin_lock_bh(&eroute_lock);

	error = radijcleanup();

	spin_unlock_bh(&eroute_lock);

	return error;
}

int
ipsec_cleareroutes(void)
{
	int error = 0;

	spin_lock_bh(&eroute_lock);

	error = radijcleartree();

	spin_unlock_bh(&eroute_lock);

	return error;
}

int
ipsec_breakroute(struct sockaddr_encap *eaddr,
		 struct sockaddr_encap *emask,
		 struct sockaddr_encap *erange_end,
		 struct sk_buff **first,
		 struct sk_buff **last)
{
	struct eroute *ro;
	struct radij_node *rn;
	int error = 0;
#ifdef CONFIG_IPSEC_DEBUG
	char buf1[64], buf2[64];
	
	if (debug_eroute) {
		subnettoa(eaddr->sen_ip_src, emask->sen_ip_src, 0, buf1, sizeof(buf1));
		subnettoa(eaddr->sen_ip_dst, emask->sen_ip_dst, 0, buf2, sizeof(buf2));
		KLIPS_PRINT(debug_eroute, 
			    "klips_debug:ipsec_breakroute: "
			    "attempting to delete eroute for %s:%d->%s:%d %d\n",
			    buf1, ntohs(eaddr->sen_sport),
			    buf2, ntohs(eaddr->sen_dport), eaddr->sen_proto);
	}
#endif /* CONFIG_IPSEC_DEBUG */

	spin_lock_bh(&eroute_lock);

	if ((error = rj_delete(eaddr, emask, erange_end, rnh, &rn)) != 0) {
		spin_unlock_bh(&eroute_lock);
		KLIPS_PRINT(debug_eroute,
			    "klips_debug:ipsec_breakroute: "
			    "node not found, eroute delete failed.\n");
		return error;
	}

	spin_unlock_bh(&eroute_lock);
	
	ro = (struct eroute *)rn;
	
	KLIPS_PRINT(debug_eroute, 
		    "klips_debug:ipsec_breakroute: "
		    "deleted eroute=%p, ident=%p->%p, first=%p, last=%p\n",
		    ro,
		    ro->er_ident_s.data,
		    ro->er_ident_d.data,
		    ro->er_first,
		    ro->er_last);
	
	if (ro->er_ident_s.data != NULL) {
		kfree(ro->er_ident_s.data);
	}
	if (ro->er_ident_d.data != NULL) {
		kfree(ro->er_ident_d.data);
	}
	if (ro->er_first != NULL) {
#if 0
		struct net_device_stats *stats = (struct net_device_stats *) &(((struct ipsecpriv *)(ro->er_first->dev->priv))->mystats);
		stats->tx_dropped--;
#endif
		*first = ro->er_first;
	}
	if (ro->er_last != NULL) {
#if 0
		struct net_device_stats *stats = (struct net_device_stats *) &(((struct ipsecpriv *)(ro->er_last->dev->priv))->mystats);
		stats->tx_dropped--;
#endif
		*last = ro->er_last;
	}
	
	if (rn->rj_flags & (RJF_ACTIVE | RJF_ROOT))
		panic ("ipsec_breakroute RMT_DELEROUTE root or active node\n");
	memset((caddr_t)rn, 0, sizeof (struct eroute));
	kfree(rn);
	
	return 0;
}

int
ipsec_makeroute(struct sockaddr_encap *eaddr,
		struct sockaddr_encap *emask,
		struct sockaddr_encap *erange_end,
		struct sa_id said,
		uint32_t pid,
		struct sk_buff *skb,
		struct ident *ident_s,
		struct ident *ident_d)
{
	struct eroute *retrt;
	int error = 0;
	char sa[SATOA_BUF];
	size_t sa_len;
#ifdef CONFIG_IPSEC_DEBUG
	char buf1[64], buf2[64];
	
	if (debug_eroute) {
		subnettoa(eaddr->sen_ip_src, emask->sen_ip_src, 0, buf1, sizeof(buf1));
		subnettoa(eaddr->sen_ip_dst, emask->sen_ip_dst, 0, buf2, sizeof(buf2));
		sa_len = satoa(said, 0, sa, SATOA_BUF);
		KLIPS_PRINT(debug_eroute, 
			    "klips_debug:ipsec_makeroute: "
			    "attempting to insert eroute for %s:%d->%s:%d %d, SA: %s, PID:%d, skb=%p, ident:%s->%s\n",
			    buf1, ntohs(eaddr->sen_sport),
			    buf2, ntohs(eaddr->sen_dport),
			    eaddr->sen_proto,
			    sa_len ? sa : " (error)",
			    pid,
			    skb,
			    (ident_s ? (ident_s->data ? ident_s->data : "NULL") : "NULL"),
			    (ident_d ? (ident_d->data ? ident_d->data : "NULL") : "NULL"));
	}
#endif /* CONFIG_IPSEC_DEBUG */

	retrt = (struct eroute *)kmalloc(sizeof (struct eroute), GFP_ATOMIC);
	if (retrt == NULL) {
		ipsec_log("klips_error:ipsec_makeroute: "
		       "not able to allocate kernel memory");
		return -ENOMEM;
	}
	memset((caddr_t)retrt, 0, sizeof (struct eroute));

	retrt->er_eaddr = *eaddr;
	retrt->er_emask = *emask;
	retrt->er_erange_to = *erange_end;
	retrt->er_said = said;
	retrt->er_pid = pid;
	retrt->er_count = 0;
	retrt->er_lasttime = jiffies/HZ;
	rd_key((&(retrt->er_rjt))) = &(retrt->er_eaddr);
	
	if (ident_s && ident_s->type != SADB_IDENTTYPE_RESERVED) {
		int data_len = ident_s->len * IPSEC_PFKEYv2_ALIGN - sizeof(struct sadb_ident);
		
		retrt->er_ident_s.type = ident_s->type;
		retrt->er_ident_s.id = ident_s->id;
		retrt->er_ident_s.len = ident_s->len;
		if(data_len) {
			if(!(retrt->er_ident_s.data = kmalloc(data_len, GFP_KERNEL))) {
				kfree(retrt);
				ipsec_log("klips_error:ipsec_makeroute: not able to allocate kernel memory (%d)\n", data_len);
				return ENOMEM;
			}
			memcpy(retrt->er_ident_s.data, ident_s->data, data_len);
		} else {
			retrt->er_ident_s.data = NULL;
		}
	}
	
	if (ident_d && ident_d->type != SADB_IDENTTYPE_RESERVED) {
		int data_len = ident_d->len  * IPSEC_PFKEYv2_ALIGN - sizeof(struct sadb_ident);
		
		retrt->er_ident_d.type = ident_d->type;
		retrt->er_ident_d.id = ident_d->id;
		retrt->er_ident_d.len = ident_d->len;
		if(data_len) {
			if(!(retrt->er_ident_d.data = kmalloc(data_len, GFP_KERNEL))) {
				if (retrt->er_ident_s.data)
					kfree(retrt->er_ident_s.data);
				kfree(retrt);
				ipsec_log("klips_error:ipsec_makeroute: not able to allocate kernel memory (%d)\n", data_len);
				return ENOMEM;
			}
			memcpy(retrt->er_ident_d.data, ident_d->data, data_len);
		} else {
			retrt->er_ident_d.data = NULL;
		}
	}
	retrt->er_first = skb;
	retrt->er_last = NULL;
	
	spin_lock_bh(&eroute_lock);
	
	error = rj_addroute(&(retrt->er_eaddr), &(retrt->er_emask),
			    &retrt->er_erange_to, rnh, retrt->er_rjt.rd_nodes);

	spin_unlock_bh(&eroute_lock);
	
	if(error) {
		sa_len = satoa(said, 0, sa, SATOA_BUF);
		KLIPS_PRINT(debug_eroute, 
			    "klips_debug:ipsec_makeroute: "
			    "rj_addroute not able to insert eroute for SA:%s\n",
			    sa_len ? sa : " (error)");
		if (retrt->er_ident_s.data)
			kfree(retrt->er_ident_s.data);
		if (retrt->er_ident_d.data)
			kfree(retrt->er_ident_d.data);
		
		kfree(retrt);
		
		return error;
	}

#ifdef CONFIG_IPSEC_DEBUG
	if (debug_eroute && 0) {
/*
		subnettoa(eaddr->sen_ip_src, emask->sen_ip_src, 0, buf1, sizeof(buf1));
		subnettoa(eaddr->sen_ip_dst, emask->sen_ip_dst, 0, buf2, sizeof(buf2));
*/
		subnettoa(rd_key((&(retrt->er_rjt)))->sen_ip_src, rd_mask((&(retrt->er_rjt)))->sen_ip_src, 0, buf1, sizeof(buf1));
		subnettoa(rd_key((&(retrt->er_rjt)))->sen_ip_dst, rd_mask((&(retrt->er_rjt)))->sen_ip_dst, 0, buf2, sizeof(buf2));
		sa_len = satoa(retrt->er_said, 0, sa, SATOA_BUF);
		
		KLIPS_PRINT(debug_eroute,
			    "klips_debug:ipsec_makeroute: "
			    "pid=%05d "
			    "count=%10d "
			    "lasttime=%6d "
			    "%-18s -> %-18s => %s\n",
			    retrt->er_pid,
			    retrt->er_count,
			    (int)(jiffies/HZ - retrt->er_lasttime),
			    buf1,
			    buf2,
			    sa_len ? sa : " (error)");
	}
#endif /* CONFIG_IPSEC_DEBUG */
	KLIPS_PRINT(debug_eroute,
		    "klips_debug:ipsec_makeroute: "
		    "succeeded, I think...\n");
	return 0;
}

struct eroute *
ipsec_findroute(struct sockaddr_encap *eaddr)
{
	struct radij_node *rn;
#ifdef CONFIG_IPSEC_DEBUG
	char buf1[ADDRTOA_BUF], buf2[ADDRTOA_BUF];
	
	if (debug_radij & DB_RJ_FINDROUTE) {
		addrtoa(eaddr->sen_ip_src, 0, buf1, sizeof(buf1));
		addrtoa(eaddr->sen_ip_dst, 0, buf2, sizeof(buf2));
		KLIPS_PRINT(debug_eroute,
			    "klips_debug:ipsec_findroute: "
			    "%s:%d->%s:%d %d\n",
			    buf1, ntohs(eaddr->sen_sport),
			    buf2, ntohs(eaddr->sen_dport),
			    eaddr->sen_proto);
	}
#endif /* CONFIG_IPSEC_DEBUG */
	rn = rj_match((caddr_t)eaddr, rnh);
	if(rn) {
		KLIPS_PRINT(debug_eroute,
			    "klips_debug:ipsec_findroute: "
			    "found, points to proto=%d, spi=%x, dst=%x.\n",
			    ((struct eroute*)rn)->er_said.proto,
			    ntohl(((struct eroute*)rn)->er_said.spi),
			    ntohl(((struct eroute*)rn)->er_said.dst.s_addr));
	}
	return (struct eroute *)rn;
}
EXPORT_SYMBOL(ipsec_findroute);
		
#ifdef CONFIG_PROC_FS
int
ipsec_rj_walker_procprint(struct radij_node *rn, void *w0)
{
	struct eroute *ro = (struct eroute *)rn;
	struct rjtentry *rd = (struct rjtentry *)rn;
	struct wsbuf *w = (struct wsbuf *)w0;
	char buf1[64], buf2[64];
	char sa[SATOA_BUF];
	size_t sa_len, buf_len;
	struct sockaddr_encap *key, *mask, *range_end;
	struct in_addr range[2];
	
	KLIPS_PRINT(debug_radij,
		    "klips_debug:ipsec_rj_walker_procprint: "
		    "rn=%p, w0=%p\n",
		    rn,
		    w0);
	if (rn == NULL)	{
		return 120;
	}
	
	key = rd_key(rd);
	mask = rd_mask(rd);
	range_end = rd_range_end(rd);
	
	if ((key == 0) || (mask == 0 && range_end == 0)) {
		return 0;
	}

	if (range_end->sen_ip_src.s_addr)
	{
	        range[0] = key->sen_ip_src;
	        range[1] = range_end->sen_ip_src;
	        buf_len = rangetoa(range, 0, buf1, sizeof(buf1));
		buf_len--;
	}
	else
	        buf_len = subnettoa(key->sen_ip_src, mask->sen_ip_src, 0, buf1, sizeof(buf1));
	buf_len += sprintf(buf1+buf_len-1, ":%d", ntohs(key->sen_sport));
	buf1[buf_len] = 0;

	if (range_end->sen_ip_dst.s_addr)
	{
	        range[0] = key->sen_ip_dst;
	        range[1] = range_end->sen_ip_dst;
	        buf_len = rangetoa(range, 0, buf2, sizeof(buf2));
		buf_len--;
	}
	else
	        buf_len = subnettoa(key->sen_ip_dst, mask->sen_ip_dst, 0, buf2, sizeof(buf2));
	buf_len += sprintf(buf2+buf_len-1, ":%d", ntohs(key->sen_dport));
	buf2[buf_len] = 0;
	sa_len = satoa(ro->er_said, 0, sa, SATOA_BUF);

#define IPSEC_EROUTE_IDENT_
#ifndef IPSEC_EROUTE_IDENT
	w->len += sprintf(w->buffer + w->len,
/*
			  "%05d "
*/
			  "%-10d "
/*
			  "%6d "
*/
			  "%s -> %s => %s:%d\n",
/*
			  ro->er_pid,
*/
			  ro->er_count,
/*
			  jiffies / HZ - ro->er_lasttime,
*/
			  buf1,
			  buf2,
			  sa_len ? sa : " (error)",
			  key->sen_proto);
#else /* IPSEC_EROUTE_IDENT */
	w->len += sprintf(w->buffer + w->len,
/*
			  "%05d "
*/
			  "%-10d "
/*
			  "%6d "
*/
			  "%s -> %s => %s:%d (%s) (%s)\n",
/*
			  ro->er_pid,
*/
			  ro->er_count,
/*
			  jiffies / HZ - ro->er_lasttime,
*/
			  buf1,
			  buf2,
			  sa_len ? sa : " (error)",
			  key->sen_proto,
			  (ro->er_ident_s.data ? ro->er_ident_s.data : ""),
			  (ro->er_ident_d.data ? ro->er_ident_d.data : ""));
#endif /* IPSEC_EROUTE_IDENT */
	
	w->pos = w->begin + w->len;
	if(w->pos < w->offset) {
		w->len = 0;
		w->begin = w->pos;
	}
	if (w->pos > w->offset + w->length) {
		return -ENOBUFS;
	}
	return 0;
}
#endif          /* CONFIG_PROC_FS */

int
ipsec_rj_walker_delete(struct radij_node *rn, void *w0)
{
	struct eroute *ro;
	struct rjtentry *rd = (struct rjtentry *)rn;
	struct radij_node *rn2;
	int error = 0;
	struct sockaddr_encap *key, *mask;
#ifdef CONFIG_IPSEC_DEBUG
	char buf1[64] = { 0 }, buf2[64] = { 0 };
#endif /* CONFIG_IPSEC_DEBUG */

	if (rn == NULL)	{
		return 120;
	}
	
	key = rd_key(rd);
	mask = rd_mask(rd);
	
	if(!key) {
		return -ENODATA;
	}
#ifdef CONFIG_IPSEC_DEBUG
	if(debug_radij)	{
		subnettoa(key->sen_ip_src, mask->sen_ip_src, 0, buf1, sizeof(buf1));
		subnettoa(key->sen_ip_dst, mask->sen_ip_dst, 0, buf2, sizeof(buf2));
		KLIPS_PRINT(debug_radij, 
			    "klips_debug:ipsec_rj_walker_delete: "
			    "deleting: %s -> %s\n",
			    buf1,
			    buf2);
	}
#endif /* CONFIG_IPSEC_DEBUG */

	if((error = rj_delete(key, mask, rd_range_end(rd), rnh, &rn2))) {
		KLIPS_PRINT(debug_radij,
			    "klips_debug:ipsec_rj_walker_delete: "
			    "rj_delete failed with error=%d.\n", error);
		return error;
	}

	if(rn2 != rn) {
		ipsec_log("klips_debug:ipsec_rj_walker_delete: "
		       "tried to delete a different node?!?  This should never happen!\n");
	}
 
	ro = (struct eroute *)rn;
	
	if (ro->er_ident_s.data)
		kfree(ro->er_ident_s.data);
	if (ro->er_ident_d.data)
		kfree(ro->er_ident_d.data);
	
	memset((caddr_t)rn, 0, sizeof (struct eroute));
	kfree(rn);
	
	return 0;
}

