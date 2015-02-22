/* net/atm/clip.c - RFC1577 Classical IP over ATM */

/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */

#include <linux/config.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kernel.h> /* for UINT_MAX */
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/if_arp.h> /* for some manifest constants */
#include <linux/notifier.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/atmclip.h>
#include <linux/ip.h> /* for net/route.h */
#include <linux/in.h> /* for struct sockaddr_in */
#include <linux/if.h> /* for IFF_UP */
#include <linux/inetdevice.h>
#include <linux/bitops.h>
#include <net/route.h> /* for struct rtable and routing */
#include <net/icmp.h> /* icmp_send */
#include <asm/param.h> /* for HZ */
#include <asm/byteorder.h> /* for htons etc. */
#include <asm/system.h> /* save/restore_flags */
#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <net/atmclip.h>

/* declarations taken from kos/knet.h */
#include "linux/version.h"
typedef struct net_device knet_netdev_t;
#define knet_netdev_start_queue(dev) netif_start_queue(dev)
#define knet_netdev_wake_queue(dev) netif_wake_queue(dev)
#define knet_netdev_stop_queue(dev) netif_stop_queue(dev)

#include "clip.h"

/* Implemented in linux/net/atm/ipcommon.c */
void skb_migrate(struct sk_buff_head *from,struct sk_buff_head *to);

/* For Linux 2.2 */
#ifndef dev_kfree_skb_any
#define dev_kfree_skb_any dev_kfree_skb
#endif

/* RFC 1577 ATM ARP header */

struct atmarphdr {
    u16 ar_hrd;	/* Hardware type */
    u16 ar_pro;	/* Protocol type */
    u8  ar_shtl; /* Type & length of source ATM number (q) */
    u8  ar_sstl; /* Type & length of source ATM subaddress (r) */
    u16 ar_op; /* Operation code (request, reply, or NAK) */
    u8 ar_spln; /* Length of source protocol address (s) */
    u8 ar_thtl; /* Type & length of target ATM number (x) */
    u8 ar_tstl; /* Type & length of target ATM subaddress (y) */
    u8 ar_tpln; /* Length of target protocol address (z) */
    /* ar_sha, at_ssa, ar_spa, ar_tha, ar_tsa, ar_tpa */
    u8 data[1];
};

static const unsigned char llc_oui_arp[] = {
    0xaa, /* DSAP: non-ISO */
    0xaa, /* SSAP: non-ISO */
    0x03, /* Ctrl: Unnumbered Information Command PDU */
    0x00, /* OUI: EtherType */
    0x00,
    0x00,
    0x08, /* ARP protocol */
    0x06
};

#define	TL_LEN 0x3f /* ATMARP Type/Length field structure */
#define	TL_E164	0x40

#define MAX_ATMARP_SIZE (sizeof(struct atmarphdr)-1+2*(ATM_E164_LEN+ \
    ATM_ESA_LEN+4))

#define MAX_DELAY (300*HZ) /* never wait more than 5 min for anything */
#define INIT_RESOLVE_DELAY (1*HZ) /* Initial retransmit time of InARPs */
#define CLIP_ENTRY_EXPIRE (600*HZ) /* After this time a neighbour expires. */

#define ATMARP_RETRY_DELAY 30 /* request next resolution or forget NAK after 30
			       * sec - should go into atmclip.h */
#define ATMARP_MAX_UNRES_PACKETS 5 /* queue that many packets while waiting for
				    * the resolver */

#define CLIP_CHECK_INTERVAL 10 /* check every ten seconds */

struct clip_priv {
#ifdef KOS_LINUX_22
    char name[IFNAMSIZ];	/* So that we won't be forced to allocate the
				 * name separately */
#endif
    int number;			/* for convenience ... */
    spinlock_t xoff_lock;	/* ensures that pop is atomic (SMP) */
    struct net_device_stats stats;
    knet_netdev_t *next;	/* next CLIP interface */
};

#define PRIV(dev) ((struct clip_priv *) ((knet_netdev_t *) (dev) + 1))

#if 0
#define DPRINTK(format,args...) printk(format,##args)
#else
#define DPRINTK(format,args...)
#endif

knet_netdev_t *clip_devs = NULL;
static struct timer_list neigh_expire_timer;

static struct clip_vcc *clip_vccs = NULL;

static int clip_inarp_request_send(struct atm_vcc *vcc);
static int clip_inarp_reply_send(struct atm_vcc *vcc, u32 remote_ip);
static void clip_start_resolving(struct clip_vcc *clip_vcc);
static void neigh_expire_timer_check(unsigned long dummy);

static void link_vcc(struct clip_vcc *clip_vcc,struct atmarp_entry *entry)
{
    DPRINTK("link_vcc %p to entry %p (neigh %p)\n",clip_vcc,entry,
	entry->neigh);
    clip_vcc->entry = entry;
    clip_vcc->xoff = 0; /* @@@ may overrun buffer by one packet */
    clip_vcc->next = entry->vccs;
    entry->vccs = clip_vcc;
    entry->neigh->used = jiffies;
}

static void unlink_clip_vcc(struct clip_vcc *clip_vcc)
{
    struct atmarp_entry *entry = clip_vcc->entry;
    struct clip_vcc **walk;

    DPRINTK("unlink_clip_vcc(%p)\n", clip_vcc);
    if (!entry)
    {
	printk(KERN_CRIT "!clip_vcc->entry (clip_vcc %p)\n",clip_vcc);
	return;
    }
    entry->neigh->used = jiffies;

    for (walk = &entry->vccs; *walk && *walk != clip_vcc; 
	walk = &(*walk)->next);
    
    if (!*walk)
    {
	printk(KERN_CRIT "ATMARP: unlink_clip_vcc failed (entry %p, vcc "
	    "0x%p)\n", entry, clip_vcc);
	return;
    }
    
    *walk = clip_vcc->next; /* atomic */
    clip_vcc->entry = NULL;
    if (clip_vcc->xoff)
	knet_netdev_wake_queue(entry->neigh->dev);
    if (entry->vccs)
	return;
    entry->expires = jiffies-1;

    /* Force expiration */
    DPRINTK("Releasing neighbor\n");
    neigh_expire_timer_check(0);    
}

static void neigh_expire_timer_check(unsigned long dummy)
{
    int i;
    unsigned long flags;

    save_flags(flags);
    cli();

    write_lock(&clip_tbl.lock);

    for (i = 0; i <= NEIGH_HASHMASK; i++)
    {
	struct neighbour **np;

	for (np = &clip_tbl.hash_buckets[i]; *np;)
	{
	    struct neighbour *n = *np;
	    struct atmarp_entry *entry = NEIGH2ENTRY(n);
	    struct clip_vcc *clip_vcc;
	    int need_resolving = 1;

    	    if (time_before(jiffies + CLIP_ENTRY_EXPIRE / 4, entry->expires))
		need_resolving = 0;

	    for (clip_vcc = entry->vccs; clip_vcc; clip_vcc = clip_vcc->next)
	    {
		if (clip_vcc->idle_timeout && time_after(jiffies, 
		    clip_vcc->last_use + clip_vcc->idle_timeout))
		{
		    DPRINTK("releasing vcc %p->%p of entry %p\n",
			clip_vcc, clip_vcc->vcc, entry);
		    atm_async_release_vcc(clip_vcc->vcc, -ETIMEDOUT);
		}
		else if (need_resolving && !clip_vcc->resolve_timeout)
		    clip_start_resolving(clip_vcc);
	    }

	    if (!need_resolving || time_before(jiffies, entry->expires))
	    {
		np = &n->next;
		continue;
	    }
	    
	    if (atomic_read(&n->refcnt) > 1)
	    {
		struct sk_buff *skb;

		DPRINTK("destruction postponed with ref %d\n",
		    atomic_read(&n->refcnt));
		while ((skb = skb_dequeue(&n->arp_queue)))
		    dev_kfree_skb(skb);
		np = &n->next;
		continue;
	    }
	    *np = n->next;
	    DPRINTK("expired neigh %p\n",n);
	    n->dead = 1;
	    neigh_release(n);
	}
    }
    mod_timer(&neigh_expire_timer, jiffies + CLIP_CHECK_INTERVAL * HZ);
    write_unlock(&clip_tbl.lock);
    restore_flags(flags);
}

static void *get_addr(u8 **here, int len)
{
    if (!len)
	return NULL;
    (*here) += len;
    return *here-len;
}

static u32 get_ip(unsigned char *ptr)
{
    if (!ptr)
	return 0;
    /* awkward, but this way we avoid bus errors on architectures that
     * don't support mis-aligned accesses */
    return htonl((ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]);
}

static int clip_learn(struct clip_vcc *clip_vcc, u32 ip)
{
    struct neighbour *neigh;
    struct atmarp_entry *entry;
    int error;

    if (!ip)
    {
	if (!clip_vcc->entry)
	{
	    printk(KERN_ERR "hiding hidden ATMARP entry\n");
	    return 0;
	}
	DPRINTK("setentry: remove\n");
	unlink_clip_vcc(clip_vcc);
	return 0;
    }

    neigh = __neigh_lookup(&clip_tbl, &ip, clip_vcc->dev, 1);
    if (!neigh)
	return -ENOMEM;
    del_timer(&clip_vcc->resolve_timer);
    clip_vcc->resolve_timeout = 0; /* Mark resolved */
    entry = NEIGH2ENTRY(neigh);
    if (entry != clip_vcc->entry)
    {
	if (!clip_vcc->entry)
	    DPRINTK("setentry: add\n");
	else
	{
	    DPRINTK("setentry: update %p\n", clip_vcc->entry);
	    unlink_clip_vcc(clip_vcc);
	}
	link_vcc(clip_vcc, entry);
    }
    error = neigh_update(neigh, llc_oui, NUD_PERMANENT, 1, 0);
    entry->expires = jiffies + CLIP_ENTRY_EXPIRE;
    neigh_release(neigh);
    return error;
}

static void incoming_arp(struct clip_vcc *clip_vcc, struct atmarphdr *hdr,
    int len)
{
    void *spa,*tpa;
    u32 src_ip,tgt_ip;
    u8 *here;

    if (len < hdr->data - (u8 *)hdr)
    {
	printk(KERN_WARNING "got truncated ARP packet (%d bytes)\n", len);
	return;
    }
    if (hdr->ar_hrd != htons(ARPHRD_ATM))
    {
	printk(KERN_WARNING "unknown hw protocol 0x%04x", ntohs(hdr->ar_hrd));
	return;
    }
    if (hdr->ar_pro != htons(ETH_P_IP))
    {
	printk(KERN_WARNING "unknown upper protocol 0x%04x",
	    ntohs(hdr->ar_pro));
	return;
    }
    if (!(hdr->ar_shtl & TL_LEN)) hdr->ar_shtl = 0; /* paranoia */
    if (!(hdr->ar_thtl & TL_LEN)) hdr->ar_thtl = 0;
    here = hdr->data;
    get_addr(&here, hdr->ar_shtl & TL_LEN);
    get_addr(&here, hdr->ar_sstl & TL_LEN);
    spa = get_addr(&here, hdr->ar_spln);
    get_addr(&here, hdr->ar_thtl & TL_LEN);
    get_addr(&here, hdr->ar_tstl & TL_LEN);
    tpa = get_addr(&here, hdr->ar_tpln);
    if (here - (u8 *)hdr > len)
    {
	printk(KERN_WARNING "message too short (got %d, need %d)",len,
	    here - (u8 *)hdr);
	return;
    }
    src_ip = get_ip(spa);
    tgt_ip = get_ip(tpa);
    
    switch (ntohs(hdr->ar_op)) {
    case ARPOP_InREQUEST:
	DPRINTK("got InARP_REQ");
	if (!clip_learn(clip_vcc, src_ip))
	    clip_inarp_reply_send(clip_vcc->vcc, src_ip);
	break;
    case ARPOP_InREPLY:
	DPRINTK("got InARP_REP");
	clip_learn(clip_vcc, src_ip);
	break;
    default:
	DPRINTK("unrecognized ARP op 0x%x", ntohs(hdr->ar_op));
    }
}

static int clip_arp_rcv(struct sk_buff *skb)
{
    struct atm_vcc *vcc;

    DPRINTK("clip_arp_rcv\n");
    vcc = ATM_SKB(skb)->vcc;
    if (!vcc)
	goto Exit;

    incoming_arp(CLIP_VCC(vcc), (struct atmarphdr *)skb->data, skb->len);

Exit:
    dev_kfree_skb_any(skb);
    return 0;
}

static u32 clip_vcc_to_local_ip(struct clip_vcc *vcc)
{
    struct in_device *in_dev;

    if (!(in_dev = (struct in_device *)vcc->dev->ip_ptr) ||
	!in_dev->ifa_list)
    {
	return 0;
    }

    return in_dev->ifa_list->ifa_local;
}

static int clip_inarp_send(struct atm_vcc *vcc, u32 remote_ip, u16 op)
{
    struct sk_buff *skb;
    int allocated_size, error, size = 0;
    u8 *buff;
    u32 local_ip;
    struct atmarphdr *hdr;

    if (test_bit(ATM_VF_RELEASED, &vcc->flags) ||
	test_bit(ATM_VF_CLOSE, &vcc->flags))
    {
	return vcc->reply;
    }
    if (!test_bit(ATM_VF_READY, &vcc->flags))
	return -EPIPE;
    
    /* align to word boundary */
    allocated_size = (MAX_ATMARP_SIZE + RFC1483LLC_LEN + 3) & ~3;

    if (atomic_read(&vcc->tx_inuse) && !atm_may_send(vcc, allocated_size))
    {
	DPRINTK("Sorry: tx_inuse = %d, size = %d, sndbuf = %d\n",
	    atomic_read(&vcc->tx_inuse), allocated_size, vcc->sk->sndbuf);
	return -1;
    }
    if(!(skb = alloc_skb(allocated_size, GFP_ATOMIC)))
	return -1;
    DPRINTK("AlTx %d += %d\n",atomic_read(&vcc->tx_inuse), skb->truesize);
    atomic_add(skb->truesize+ATM_PDU_OVHD,&vcc->tx_inuse);

    skb->dev = NULL; /* for paths shared with net_device interfaces */
    ATM_SKB(skb)->iovcnt = 0;
    ATM_SKB(skb)->atm_options = vcc->atm_options;
    buff = skb->data;
    memcpy(buff, llc_oui_arp, RFC1483LLC_LEN);
    hdr = (struct atmarphdr *) (buff + RFC1483LLC_LEN);
    memset(hdr, 0, MAX_ATMARP_SIZE);
    hdr->ar_hrd = htons(ARPHRD_ATM);
    hdr->ar_pro = htons(ETH_P_IP);
    hdr->ar_op = htons(op);
    local_ip = clip_vcc_to_local_ip(CLIP_VCC(vcc));
    size = hdr->data - buff;
    /* XXX: htonl might be needed */
    memcpy(hdr->data, &local_ip, sizeof(u32));
    hdr->ar_spln = sizeof(u32);
    size += sizeof(u32);
    if (remote_ip)
    {
	/* XXX: htonl might be needed */
	memcpy(hdr->data + sizeof(u32), &remote_ip, sizeof(u32));
	hdr->ar_tpln = sizeof(u32);
	size += sizeof(u32);
    }

    skb_put(skb, size);
    if (allocated_size != size)
	memset(skb->data + size, 0, allocated_size - size);
    error = vcc->dev->ops->send(vcc,skb);
    return error ? error : size;
}

static int clip_inarp_request_send(struct atm_vcc *vcc)
{
    return clip_inarp_send(vcc, 0, ARPOP_InREQUEST);
}

static int clip_inarp_reply_send(struct atm_vcc *vcc, u32 remote_ip)
{
    return clip_inarp_send(vcc, remote_ip, ARPOP_InREPLY);
}

static void res_timer_check(unsigned long arg)
{
    struct clip_vcc *clip_vcc = (struct clip_vcc *)arg;
    unsigned long flags;

    save_flags(flags);
    cli();
    
    if (clip_vcc->resolve_timeout)
    {
	clip_vcc->resolve_timeout *= 2;
	if (clip_vcc->resolve_timeout > MAX_DELAY)
	    clip_vcc->resolve_timeout = MAX_DELAY;
	clip_inarp_request_send(clip_vcc->vcc);
	mod_timer(&clip_vcc->resolve_timer,
	    jiffies + clip_vcc->resolve_timeout);
    }
    restore_flags(flags);
}

static void clip_start_resolving(struct clip_vcc *clip_vcc)
{
    struct timer_list *res_timer = &clip_vcc->resolve_timer;

    del_timer(res_timer);
    clip_vcc->resolve_timeout = INIT_RESOLVE_DELAY;
    init_timer(res_timer);
    res_timer->function = res_timer_check;
    res_timer->data = (unsigned long)clip_vcc; 
    clip_inarp_request_send(clip_vcc->vcc);
    res_timer->expires = jiffies + clip_vcc->resolve_timeout;
    add_timer(res_timer);
}

static void all_clip_vccs_start_resolving(void)
{
    struct clip_vcc *vcc;

    for (vcc = clip_vccs; vcc; vcc = vcc->global_next)
    {
	if (vcc->resolve_timeout) /* Not marked as resolved */
	    clip_start_resolving(vcc);
    }
}

static void clip_vcc_remove(struct clip_vcc *clip_vcc)
{
    struct clip_vcc **find_vcc = &clip_vccs;

    DPRINTK("removing VCC %p\n", clip_vcc);
    del_timer(&clip_vcc->resolve_timer);
    if (clip_vcc->entry)
	unlink_clip_vcc(clip_vcc);
    for (find_vcc = &clip_vccs; *find_vcc && *find_vcc != clip_vcc; 
	find_vcc = &(*find_vcc)->global_next);
    if (*find_vcc)
	(*find_vcc) = (*find_vcc)->global_next;
    clip_vcc->old_push(clip_vcc->vcc, NULL); /* pass on the bad news */
    kfree(clip_vcc);
}

void clip_push(struct atm_vcc *vcc,struct sk_buff *skb)
{
    struct clip_vcc *clip_vcc = CLIP_VCC(vcc);

    DPRINTK("clip push\n");
    if (!skb)
    {
	clip_vcc_remove(clip_vcc);
	return;
    }
    atm_return(vcc, skb->truesize);
    skb->dev = clip_vcc->entry ? clip_vcc->entry->neigh->dev : clip_devs;
    /* clip_vcc->entry == NULL if we don't have an IP address yet */
    if (!skb->dev)
    {
	dev_kfree_skb_any(skb);
	return;
    }
    ATM_SKB(skb)->vcc = vcc;
    skb->mac.raw = skb->data;
    if (!clip_vcc->encap || skb->len < RFC1483LLC_LEN || memcmp(skb->data,
	llc_oui, sizeof(llc_oui))) skb->protocol = htons(ETH_P_IP);
    else
    {
	skb->protocol = ((u16 *) skb->data)[3];
	skb_pull(skb, RFC1483LLC_LEN);
	if (skb->protocol == htons(ETH_P_ARP))
	{
	    PRIV(skb->dev)->stats.rx_packets++;
	    PRIV(skb->dev)->stats.rx_bytes += skb->len;
	    clip_arp_rcv(skb);
	    return;
	}
    }
    clip_vcc->last_use = jiffies;
    PRIV(skb->dev)->stats.rx_packets++;
    PRIV(skb->dev)->stats.rx_bytes += skb->len;
    netif_rx(skb);
}

/*
 * Note: these spinlocks _must_not_ block on non-SMP. The only goal is that
 * clip_pop is atomic with respect to the critical section in clip_start_xmit.
 */

static void clip_pop(struct atm_vcc *vcc,struct sk_buff *skb)
{
    struct clip_vcc *clip_vcc = CLIP_VCC(vcc);
    knet_netdev_t *dev = skb->dev;
    int old;
    unsigned long flags;

    DPRINTK("clip_pop(vcc %p)\n",vcc);
    clip_vcc->old_pop(vcc, skb);
    /* skb->dev == NULL in outbound ARP packets */
    if (!dev)
	return;
    spin_lock_irqsave(&PRIV(dev)->xoff_lock, flags);
    if (atm_may_send(vcc, 0))
    {
	old = xchg(&clip_vcc->xoff, 0);
	if (old)
	    knet_netdev_wake_queue(dev);
    }
    spin_unlock_irqrestore(&PRIV(dev)->xoff_lock, flags);
}

static void clip_neigh_destroy(struct neighbour *neigh)
{
    struct clip_vcc *clip_vcc, *next;
    struct atmarp_entry *entry = NEIGH2ENTRY(neigh);
    
    DPRINTK("clip_neigh_destroy (neigh %p) ent %p vccs %p\n", neigh, entry,
	entry->vccs);
    for (clip_vcc = entry->vccs; clip_vcc; clip_vcc = next)
    {
	next = clip_vcc->next;
	clip_vcc->entry = NULL;
	clip_vcc->next = NULL;
    }
    NEIGH2ENTRY(neigh)->vccs = (void *) 0xdeadbeef;
}

static void clip_neigh_solicit(struct neighbour *neigh, struct sk_buff *skb)
{
    DPRINTK("clip_neigh_solicit (neigh %p, skb %p)\n",neigh,skb);
}

static void clip_neigh_error(struct neighbour *neigh,struct sk_buff *skb)
{
#ifndef CONFIG_ATM_CLIP_NO_ICMP
    icmp_send(skb,ICMP_DEST_UNREACH,ICMP_HOST_UNREACH,0);
#endif
    kfree_skb(skb);
}

static struct neigh_ops clip_neigh_ops = {
family:			AF_INET,
destructor:		clip_neigh_destroy,
solicit:		clip_neigh_solicit,
error_report:		clip_neigh_error,
output:			dev_queue_xmit,
connected_output:	dev_queue_xmit,
hh_output:		dev_queue_xmit,
queue_xmit:		dev_queue_xmit,
};

static int clip_constructor(struct neighbour *neigh)
{
    struct atmarp_entry *entry = NEIGH2ENTRY(neigh);
    knet_netdev_t *dev = neigh->dev;
    struct in_device *in_dev = dev->ip_ptr;

    DPRINTK("clip_constructor (neigh %p, entry %p)\n",neigh,entry);
    if (!in_dev)
	return -EINVAL;
    neigh->type = inet_addr_type(entry->ip);
    if (neigh->type != RTN_UNICAST)
	return -EINVAL;
    if (in_dev->arp_parms)
	neigh->parms = in_dev->arp_parms;
    neigh->ops = &clip_neigh_ops;
    neigh->output = neigh->nud_state & NUD_VALID ?
	neigh->ops->connected_output : neigh->ops->output;
    entry->neigh = neigh;
    entry->vccs = NULL;
    entry->expires = jiffies-1;
    return 0;
}

static u32 clip_hash(const void *pkey, const knet_netdev_t *dev)
{
    u32 hash_val;

    hash_val = *(u32*)pkey;
    hash_val ^= (hash_val>>16);
    hash_val ^= hash_val>>8;
    hash_val ^= hash_val>>3;
    hash_val = (hash_val^dev->ifindex)&NEIGH_HASHMASK;

    return hash_val;
}

struct neigh_table clip_tbl = {
    NULL,			/* next */
    AF_INET,		/* family */
    sizeof(struct neighbour)+sizeof(struct atmarp_entry), /* entry_size */
    4,			/* key_len */
    clip_hash,
    clip_constructor,	/* constructor */
    NULL,			/* pconstructor */
    NULL,			/* pdestructor */
    NULL,			/* proxy_redo */
    "clip_arp_cache",
    {			/* neigh_parms */
	NULL,		/* next */
	NULL,		/* neigh_setup */
	&clip_tbl,	/* tbl */
	0,		/* entries */
	NULL,		/* priv */
	NULL,		/* sysctl_table */
	30*HZ,		/* base_reachable_time */
	1*HZ,		/* retrans_time */
	60*HZ,		/* gc_staletime */
	30*HZ,		/* reachable_time */
	5*HZ,		/* delay_probe_time */
	3,		/* queue_len */
	3,		/* ucast_probes */
	0,		/* app_probes */
	3,		/* mcast_probes */
	1*HZ,		/* anycast_delay */
	(8*HZ)/10,	/* proxy_delay */
	1*HZ,		/* proxy_qlen */
	64		/* locktime */
    },
    30*HZ,128,512,1024	/* copied from ARP ... */
};

static int clip_encap(struct atm_vcc *vcc,int mode)
{
    CLIP_VCC(vcc)->encap = mode;
    return 0;
}

static int clip_start_xmit(struct sk_buff *skb, knet_netdev_t *dev)
{
    struct clip_priv *clip_priv = PRIV(dev);
    struct atmarp_entry *entry;
    struct atm_vcc *vcc;
    int old;
    unsigned long flags;

    DPRINTK("clip_start_xmit (skb %p)\n", skb);
    if (!skb->dst)
    {
	printk(KERN_ERR "clip_start_xmit: skb->dst == NULL\n");
	dev_kfree_skb(skb);
	clip_priv->stats.tx_dropped++;
	return 0;
    }
    if (!skb->dst->neighbour)
    {
	printk(KERN_ERR "clip_start_xmit: NO NEIGHBOUR!\n");
	dev_kfree_skb(skb);
	clip_priv->stats.tx_dropped++;
	return 0;
    }
    entry = NEIGH2ENTRY(skb->dst->neighbour);
    if (!entry->vccs)
    {
	if (time_after(jiffies, entry->expires))
	{
	    /* should be resolved */
	    entry->expires = jiffies + ATMARP_RETRY_DELAY * HZ;
	} 
	if (entry->neigh->arp_queue.qlen < ATMARP_MAX_UNRES_PACKETS)
	    skb_queue_tail(&entry->neigh->arp_queue, skb);
	else
	{
	    dev_kfree_skb(skb);
	    clip_priv->stats.tx_dropped++;
	}

	/* If a vcc was not resolved for a long time, it sends an InARP
	 * packet every 5 minutes. But if the other side connected now
	 * we do not want to wait.
	 */
	all_clip_vccs_start_resolving();
	return 0;
    }
    DPRINTK("neigh %p, vccs %p\n", entry, entry->vccs);
    ATM_SKB(skb)->vcc = vcc = entry->vccs->vcc;
    DPRINTK("using neighbour %p, vcc %p\n", skb->dst->neighbour, vcc);
    if (entry->vccs->encap)
    {
	void *here;

	here = skb_push(skb, RFC1483LLC_LEN);
	memcpy(here, llc_oui, sizeof(llc_oui));
	((u16 *) here)[3] = skb->protocol;
    }
    atomic_add(skb->truesize, &vcc->tx_inuse);
    ATM_SKB(skb)->iovcnt = 0;
    ATM_SKB(skb)->atm_options = vcc->atm_options;
    entry->vccs->last_use = jiffies;
    DPRINTK("atm_skb(%p)->vcc(%p)->dev(%p)\n", skb, vcc,vcc->dev);
    old = xchg(&entry->vccs->xoff, 1); /* assume XOFF ... */
    if (old)
    {
	printk(KERN_WARNING "clip_start_xmit: XOFF->XOFF transition\n");
	return 0;
    }
    clip_priv->stats.tx_packets++;
    clip_priv->stats.tx_bytes += skb->len;
    vcc->dev->ops->send(vcc, skb);
    if (atm_may_send(vcc, 0))
    {
	entry->vccs->xoff = 0;
	return 0;
    }
    spin_lock_irqsave(&clip_priv->xoff_lock, flags);
    knet_netdev_stop_queue(dev); /* XOFF -> throttle immediately */
    barrier();
    if (!entry->vccs->xoff)
	knet_netdev_start_queue(dev);
    /* Oh, we just raced with clip_pop. netif_start_queue should be
       good enough, because nothing should really be asleep because
       of the brief netif_stop_queue. If this isn't true or if it
       changes, use netif_wake_queue instead. */
    spin_unlock_irqrestore(&clip_priv->xoff_lock, flags);
    return 0;
}

static struct net_device_stats *clip_get_stats(knet_netdev_t *dev)
{
    return &PRIV(dev)->stats;
}

static int clip_mkip(struct atm_vcc *vcc,int timeout)
{
    struct clip_vcc *clip_vcc;
    struct sk_buff_head copy;
    struct sk_buff *skb;

    if (!vcc->push) return -EBADFD;
    clip_vcc = kmalloc(sizeof(struct clip_vcc),GFP_KERNEL);
    if (!clip_vcc) return -ENOMEM;
    DPRINTK("mkip clip_vcc %p vcc %p\n",clip_vcc,vcc);
    clip_vcc->vcc = vcc;
    vcc->user_back = clip_vcc;
    clip_vcc->entry = NULL;
    clip_vcc->xoff = 0;
    clip_vcc->encap = 1;
    clip_vcc->last_use = jiffies;
    clip_vcc->idle_timeout = timeout*HZ;
    clip_vcc->old_push = vcc->push;
    clip_vcc->old_pop = vcc->pop;
    clip_vcc->dev = NULL;
    clip_vcc->global_next = NULL;
    memset(&clip_vcc->resolve_timer, 0, sizeof(clip_vcc->resolve_timer));
    vcc->push = clip_push;
    vcc->pop = clip_pop;
    skb_queue_head_init(&copy);
    skb_migrate(&vcc->recvq,&copy);
    /* re-process everything received between connection setup and MKIP */
    while ((skb = skb_dequeue(&copy)))
    {
	if (!clip_devs)
	{
	    atm_return(vcc,skb->truesize);
	    kfree_skb(skb);
	}
	else
	{
	    unsigned int len = skb->len;

	    clip_push(vcc,skb);
	    PRIV(skb->dev)->stats.rx_packets--;
	    PRIV(skb->dev)->stats.rx_bytes -= len;
	}
    }
    return 0;
}

static int clip_assign_device(struct atm_vcc *vcc, int itf)
{
    struct clip_vcc *clip_vcc;
    knet_netdev_t *dev;

    if (vcc->push != clip_push)
    {
	printk(KERN_WARNING "clip_assign_device: non-CLIP VCC\n");
	return -EBADF;
    }

    clip_vcc = CLIP_VCC(vcc);
    for (dev = clip_devs; dev && PRIV(dev)->number != itf;
	dev = PRIV(dev)->next);

    if (!dev)
    {
	printk(KERN_WARNING "clip_assign_device: no such device\n");
	return -ENODEV;
    }

    if (clip_vcc->entry)
	unlink_clip_vcc(clip_vcc);

    clip_vcc->dev = dev;

    clip_vcc->global_next = clip_vccs;
    clip_vccs = clip_vcc;

    clip_start_resolving(clip_vcc);

    return 0;
}

static int clip_init(knet_netdev_t *dev)
{
    DPRINTK("clip_init %s\n",dev->name);
    dev->hard_start_xmit = clip_start_xmit;
    /* sg_xmit ... */
    dev->hard_header = NULL;
    dev->rebuild_header = NULL;
    dev->set_mac_address = NULL;
    dev->hard_header_parse = NULL;
    dev->hard_header_cache = NULL;
    dev->header_cache_update = NULL;
    dev->change_mtu = NULL;
    dev->do_ioctl = NULL;
    dev->get_stats = clip_get_stats;
    dev->type = ARPHRD_ATM;
    dev->hard_header_len = RFC1483LLC_LEN;
    dev->mtu = RFC1626_MTU;
    dev->addr_len = 0;
    dev->tx_queue_len = 100; /* "normal" queue (packets) */
    /* When using a "real" qdisc, the qdisc determines the queue */
    /* length. tx_queue_len is only used for the default case, */
    /* without any more elaborate queuing. 100 is a reasonable */
    /* compromise between decent burst-tolerance and protection */
    /* against memory hogs. */
    dev->flags = 0;
    return 0;
}

static int clip_destroy(int number)
{
    knet_netdev_t **dev, *tmp;

    for (dev = &clip_devs; *dev && PRIV(*dev)->number != number;
	dev = &PRIV(*dev)->next);

    tmp = *dev;
    if (!tmp)
	return -ENODEV;

    *dev = PRIV(*dev)->next;
    
    unregister_netdev(tmp);

    return 0;
}

static int clip_create(int number)
{
    knet_netdev_t *dev;
    struct clip_priv *clip_priv;
    int error;

    if (number != -1)
    {
	for (dev = clip_devs; dev; dev = PRIV(dev)->next)
	{
	    if (PRIV(dev)->number == number)
		return -EEXIST;
	}
    }
    else
    {
	number = 0;
	for (dev = clip_devs; dev; dev = PRIV(dev)->next)
	{
	    if (PRIV(dev)->number >= number)
		number = PRIV(dev)->number + 1;
	}
    }
    dev = kmalloc(sizeof(knet_netdev_t) + sizeof(struct clip_priv),
	GFP_KERNEL); 
    if (!dev)
	return -ENOMEM;
    memset(dev, 0, sizeof(knet_netdev_t) + sizeof(struct clip_priv));
    clip_priv = PRIV(dev);
#ifdef KOS_LINUX_22
    dev->name = PRIV(dev)->name;
#endif
    sprintf(dev->name, "clip%d", number);
    dev->init = clip_init;
    spin_lock_init(&clip_priv->xoff_lock);
    clip_priv->number = number;
    error = register_netdev(dev);
    if (error)
    {
	kfree(dev);
	return error;
    }
    clip_priv->next = clip_devs;
    clip_devs = dev;
    DPRINTK("registered (net:%s)\n",dev->name);
    return number;
}

int clip_ioctl(struct atm_vcc *vcc, unsigned int cmd, unsigned long arg)
{
    int num;
#define CHECK_CAPABLE_AND_ACT(action) \
    { \
	if (!capable(CAP_NET_ADMIN)) \
	    return -EPERM; \
	if (copy_from_user(&num, (void *)arg, sizeof(int))) \
	    return -EFAULT; \
	return (action);\
    }

    switch (cmd) {
    case SIOCCLIP_CREATE: CHECK_CAPABLE_AND_ACT(clip_create(num));
    case SIOCCLIP_DEL: CHECK_CAPABLE_AND_ACT(clip_destroy(num));
    case SIOCCLIP_MKIP: CHECK_CAPABLE_AND_ACT(clip_mkip(vcc, num));
    case SIOCCLIP_SETDEV: CHECK_CAPABLE_AND_ACT(clip_assign_device(vcc, num));
    case SIOCCLIP_ENCAP: CHECK_CAPABLE_AND_ACT(clip_encap(vcc, num));
    default: return -ENOIOCTLCMD;
    }
}

void atm_clip_init(void)
{
    clip_tbl.lock = RW_LOCK_UNLOCKED;
    clip_tbl.kmem_cachep = kmem_cache_create(clip_tbl.id,
	clip_tbl.entry_size, 0, SLAB_HWCACHE_ALIGN, NULL, NULL);

    init_timer(&neigh_expire_timer);
    neigh_expire_timer.expires = jiffies + CLIP_CHECK_INTERVAL * HZ;
    neigh_expire_timer.function = neigh_expire_timer_check;
    add_timer(&neigh_expire_timer);
}
