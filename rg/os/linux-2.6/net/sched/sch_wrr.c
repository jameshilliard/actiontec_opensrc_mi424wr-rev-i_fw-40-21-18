/*
 * net/sched/sch_wrr.c	Weighted round-robin queue
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Martin Devera, <devik@cdi.cz>
 *
 *   Fixed: 2001/4/29, Lijie Sheng, <sheng_lijie@263.net>
 *               - return value of enqueue, requeue in 2.4
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <net/ip.h>
#include <net/route.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/pkt_sched.h>


/*	WRR algorithm.
	=======================================
	The qdisc contains number of queues and every of them
	is assigned weight. The available bandwidth is distributed
	among classes proportionaly to the weights.
*/

struct wrr_sched_data;
struct wrr_class
{
	u32			classid;
	long			quantum;	/* Max bytes transmited at once */
	struct Qdisc		*qdisc;		/* Ptr to WRR discipline */
	struct wrr_class	*sibling;	/* Sibling chain */
	struct wrr_class	*prev;		/* Prev. sibling */
	struct Qdisc		*q;		/* Elementary queueing discipline */

	struct tc_stats		stats;
	long			deficit;
	int			refcnt;
};

struct wrr_sched_data
{
	struct wrr_class	*classes;
	struct tcf_proto	*filter_list;
	int			filters;
};


#define WRRDBG 0

static __inline__ struct wrr_class *wrr_find(u32 handle, struct Qdisc *sch)
{
    struct wrr_sched_data *q = qdisc_priv(sch);
    struct wrr_class *cl = q->classes;
    if (cl) do {
	if (cl->classid == handle) return cl;

    } while ((cl = cl->sibling) != q->classes);
    return NULL;
}

static struct wrr_class *wrr_clasify(struct sk_buff *skb, struct Qdisc *sch)
{
    struct wrr_sched_data *q = qdisc_priv(sch);
    struct tcf_result res;
    u32 cid;

    cid = skb->priority;	/* provide shortcut semantic */
    if (TC_H_MAJ(skb->priority) != sch->handle) {
	if (!q->filter_list || tc_classify(skb, q->filter_list, &res))
	    	/* the filter doesn't classify skb, use default (X:1) class */
		cid = 1;
		else cid = res.classid;
    }
    return wrr_find(TC_H_MIN(cid) | TC_H_MAJ(sch->handle),sch);
}

static int
wrr_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct wrr_class *cl = wrr_clasify(skb,sch);
	int ret=NET_XMIT_DROP;
	    
	if (!cl) kfree_skb(skb);
	else if ((ret=cl->q->enqueue(skb, cl->q)) == 0) {
		sch->q.qlen++;
		sch->bstats.packets++;
		sch->bstats.bytes+=skb->len;
		cl->stats.packets++;
		cl->stats.bytes+=skb->len;
		return 0;
	}
	sch->qstats.drops++;
	if (cl) cl->stats.drops++;
	return ret;
}

static int
wrr_requeue(struct sk_buff *skb, struct Qdisc *sch)
{
	//struct wrr_sched_data *q = qdisc_priv(sch);
	struct wrr_class *cl = wrr_clasify(skb,sch);
	int ret=NET_XMIT_DROP;

	if (!cl) kfree_skb(skb);
	else if ((ret=cl->q->ops->requeue(skb, cl->q)) == 0) {
		sch->q.qlen++;
		return 0;
	}
	sch->qstats.drops++;
	if (cl) cl->stats.drops++;
	return ret;
}
#define RDBG 0
static struct sk_buff *
wrr_dequeue(struct Qdisc *sch)
{
	struct sk_buff *skb;
	struct wrr_sched_data *q = qdisc_priv(sch);
	struct wrr_class *cl = q->classes;
	int done;
	
#if RDBG
		printk("dequeue starting\n");
#endif
	if (cl) do {
	    	done=1;
#if RDBG
		printk("round starting\n");
#endif
		do {
#if RDBG
			printk("class %X, deficit %ld, qlen %d\n",
				cl->classid,cl->deficit,cl->q->q.qlen);
#endif
			if (cl->deficit <= 0) {
				if (cl->q->q.qlen) done = 0;
				cl->deficit += cl->quantum;
				continue;
			}
			if ((skb = cl->q->dequeue(cl->q)) == NULL) continue;
			cl->deficit -= skb->len;
			
#if RDBG
		printk("we have packet (sz=%d), deficit %ld\n",skb->len,cl->deficit);
#endif
			if (cl->deficit <= 0) {
				cl = cl->sibling;
//				cl->deficit += cl->quantum;
			}
			q->classes = cl;
			sch->q.qlen--;
			return skb;
		} while ((cl = cl->sibling) != q->classes);
	} while (!done);
		    
#if RDBG
		printk("done - no packet, class will be %X\n",
		    cl ? cl->classid : 0);
#endif
	q->classes = cl;	
	return NULL;
}

static unsigned int wrr_drop(struct Qdisc* sch)
{
	struct wrr_sched_data *q = qdisc_priv(sch);
	struct wrr_class *cl = q->classes;

	if (cl) do {
	    if (cl->q->ops->drop && cl->q->ops->drop(cl->q))
		return 1;
	} while ((cl = cl->sibling) != q->classes);
	return 0;
}

static void
wrr_reset(struct Qdisc* sch)
{
    struct wrr_sched_data *q = qdisc_priv(sch);
    struct wrr_class *cl = q->classes;
#if WRRDBG
    printk("wrr_reset sch=%p, handle=%X\n",sch,sch->handle);
#endif

    if (cl) do {
	qdisc_reset(cl->q);
	cl->deficit = cl->quantum;
	
    } while ((cl = cl->sibling) != q->classes);
}

static int wrr_init(struct Qdisc *sch, struct rtattr *opt)
{
	struct wrr_sched_data *q = qdisc_priv(sch);
#if WRRDBG
    printk("wrr_init sch=%p, handle=%X\n",sch,sch->handle);
#endif
	q->classes = NULL;
	q->filters = 0;
	return 0;
}

static int wrr_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	unsigned char	 *b = skb->tail;
#if WRRDBG
    printk("wrr_dump sch=%p, handle=%X\n",sch,sch->handle);
#endif
	RTA_PUT(skb, TCA_OPTIONS, 4, &sch->handle);
	return 4;
rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

static int
wrr_dump_class(struct Qdisc *sch, unsigned long arg,
	       struct sk_buff *skb, struct tcmsg *tcm)
{
	struct wrr_class *cl = (struct wrr_class*)arg;
	unsigned char	 *b = skb->tail;
#if WRRDBG
    printk("wrr_dclass sch=%p, handle=%X\n",sch,sch->handle);
#endif
	tcm->tcm_parent = TC_H_ROOT;
	tcm->tcm_handle = cl->classid;
	tcm->tcm_info = cl->q->handle;

	RTA_PUT(skb, TCA_OPTIONS, 4, &cl->quantum);
	RTA_PUT(skb, TCA_STATS, sizeof(cl->stats), &cl->stats);
	return skb->len;
rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

static int wrr_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old)
{
	struct wrr_class *cl = (struct wrr_class*)arg;

	if (cl) {
		if (new == NULL) {
			if ((new = qdisc_create_dflt(sch->dev, &pfifo_qdisc_ops)) == NULL)
				return -ENOBUFS;
		} 
		if ((*old = xchg(&cl->q, new)) != NULL)
			qdisc_reset(*old);
		return 0;
	}
	return -ENOENT;
}

static struct Qdisc *
wrr_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct wrr_class *cl = (struct wrr_class*)arg;

	return cl ? cl->q : NULL;
}

static unsigned long wrr_get(struct Qdisc *sch, u32 classid)
{
 	struct wrr_class *cl = wrr_find(classid,sch);
 	if (cl) cl->refcnt++;
	return (unsigned long)cl;
}

static void wrr_destroy_filters(struct wrr_sched_data *q)
{
	struct tcf_proto *tp;

	while ((tp = q->filter_list) != NULL) {
		q->filter_list = tp->next;
		tp->ops->destroy(tp);
	}
}

static void wrr_destroy_class(struct wrr_class *cl)
{
	qdisc_destroy(cl->q);
	kfree(cl);
}

static void
wrr_destroy(struct Qdisc* sch)
{
	struct wrr_sched_data *q = qdisc_priv(sch);
 	struct wrr_class *sibling, *cl = q->classes;

 	if (cl) do {
	    sibling = cl->sibling;
	    wrr_destroy_class(cl);

	} while ((cl = sibling) != q->classes);

	wrr_destroy_filters(q);
}

static void wrr_put(struct Qdisc *sch, unsigned long arg)
{
	//struct wrr_sched_data *q = qdisc_priv(sch);
	struct wrr_class *cl = (struct wrr_class*)arg;

	if (--cl->refcnt == 0) {
		wrr_destroy_class(cl);
	}
}

static int
wrr_change_class(struct Qdisc *sch, u32 classid, u32 parentid, struct rtattr **tca,
		 unsigned long *arg)
{
	int err;
	struct wrr_sched_data *q = qdisc_priv(sch);
	struct wrr_class *cl = (struct wrr_class*)*arg;
	struct rtattr *opt = tca[TCA_OPTIONS-1];
	long *qm = RTA_DATA(opt);
#if WRRDBG
    printk("wrr_ch_class sch=%p, handle=%X, clsid=%X, parentid=%X\n",
	    sch,sch->handle,classid,parentid);
#endif
	
	err = -ENOBUFS;
	cl = kmalloc(sizeof(*cl), GFP_KERNEL);
	if (cl == NULL)
		goto failure;
	memset(cl, 0, sizeof(*cl));
	cl->refcnt = 1;
	if (!(cl->q = qdisc_create_dflt(sch->dev, &pfifo_qdisc_ops)))
		cl->q = &noop_qdisc;
	cl->classid = classid;
	cl->qdisc = sch;
	cl->quantum = 1500;
	if (opt) cl->quantum = *qm;
	cl->deficit = cl->quantum;

	/* attach to the list */
	sch_tree_lock(sch);
	if (!q->classes) cl->sibling = q->classes = cl;
	cl->prev = q->classes;
	cl->sibling = q->classes->sibling;
	cl->prev->sibling = cl->sibling->prev = cl;
	sch_tree_unlock(sch);
	
	*arg = (unsigned long)cl;
	return 0;

failure:
	return err;
}

static int wrr_delete(struct Qdisc *sch, unsigned long arg)
{
	struct wrr_sched_data *q = qdisc_priv(sch);
	struct wrr_class *cl = (struct wrr_class*)arg;

	sch_tree_lock(sch);

	cl->sibling->prev = cl->prev;
	cl->prev->sibling = cl->sibling;
	if (q->classes == cl)
	    	q->classes = cl->sibling == cl ? NULL : cl->sibling;
	    
	sch_tree_unlock(sch);
	    
	if (--cl->refcnt == 0)
		wrr_destroy_class(cl);

	return 0;
}

static struct tcf_proto **wrr_find_tcf(struct Qdisc *sch, unsigned long arg)
{
	struct wrr_sched_data *q = qdisc_priv(sch);
	return arg ? NULL : &q->filter_list;
}

static unsigned long wrr_bind_filter(struct Qdisc *sch, unsigned long parent,
				     u32 classid)
{
	struct wrr_sched_data *q = qdisc_priv(sch);
	q->filters++;
	return 0;
}

static void wrr_unbind_filter(struct Qdisc *sch, unsigned long arg)
{
	struct wrr_sched_data *q = qdisc_priv(sch);
	q->filters--;
}

static void wrr_walk(struct Qdisc *sch, struct qdisc_walker *arg)
{
	struct wrr_sched_data *q = qdisc_priv(sch);
	struct wrr_class *cl = q->classes;

	if (arg->stop)
		return;
	if (cl) do {
	    if (arg->count < arg->skip) {
		arg->count++;
		continue;
	    }
	    if (arg->fn(sch, (unsigned long)cl, arg) < 0) {
		arg->stop = 1;
		return;
	    }
	    arg->count++;

	} while ((cl = cl->sibling) != q->classes);
}

static struct Qdisc_class_ops wrr_class_ops =
{
	wrr_graft,
	wrr_leaf,
	wrr_get,
	wrr_put,
	wrr_change_class,
	wrr_delete,
	wrr_walk,

	wrr_find_tcf,
	wrr_bind_filter,
	wrr_unbind_filter,
	wrr_dump_class,
};

struct Qdisc_ops wrr_qdisc_ops =
{
	NULL,
	&wrr_class_ops,
	"wrr",
	sizeof(struct wrr_sched_data),

	wrr_enqueue,
	wrr_dequeue,
	wrr_requeue,
	wrr_drop,

	wrr_init,
	wrr_reset,
	wrr_destroy,
	NULL /* wrr_change */,
	wrr_dump,
};

int init_module(void)
{
	return register_qdisc(&wrr_qdisc_ops);
}

void cleanup_module(void) 
{
	unregister_qdisc(&wrr_qdisc_ops);
}
module_init(init_module)
module_exit(cleanup_module)
