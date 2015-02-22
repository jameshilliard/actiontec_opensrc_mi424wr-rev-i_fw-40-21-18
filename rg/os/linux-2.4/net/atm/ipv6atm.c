/* net/atm/ipv6atm.c - RFC2492 IPv6 over ATM */

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

/* based on clip.c - RFC1577 Classical IP over ATM */
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
#include <linux/atmipv6.h>	/* IPv6 over ATM */
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

#include "common.h"
#include "resources.h"
#include "ipcommon.h"
#include <net/atmipv6.h>

#if 0
#define DPRINTK(format,args...) printk(format,##args)
#else
#define DPRINTK(format,args...)
#endif


struct net_device *clip6_devs = NULL;


void clip6_push(struct atm_vcc *vcc,struct sk_buff *skb)
{
	struct clip6_vcc *clip6_vcc = CLIP6_VCC(vcc);

	DPRINTK("clip6 push\n");
	if (!skb) {
		DPRINTK("removing VCC %p\n",clip6_vcc);
		clip6_vcc->old_push(vcc,NULL); /* pass on the bad news */
		kfree(clip6_vcc);
		return;
	}
	atm_return(vcc,skb->truesize);
	skb->dev = clip6_vcc->dev;
		/* clip6_vcc->entry == NULL if we don't have an IP address yet */
	if (!skb->dev) {
		dev_kfree_skb_any(skb);
		return;
	}
	ATM_SKB(skb)->vcc = vcc;
	skb->mac.raw = skb->data;
	if (!clip6_vcc->encap || skb->len < RFC1483LLC_LEN || memcmp(skb->data,
	    llc_oui,sizeof(llc_oui))) {
		skb->protocol = ((u16 *) skb->data)[3];
				/* ??? Should I validate skb->protocol is
				   either ETH_P_IPV6 or ETH_P_IP? */
	}
	else {
		skb->protocol = ((u16 *) skb->data)[3];
		skb_pull(skb,RFC1483LLC_LEN);
				/* TODO: check */
#if 0
		if (skb->protocol == htons(ETH_P_ARP)) {
			PRIV(skb->dev)->stats.rx_packets++;
			PRIV(skb->dev)->stats.rx_bytes += skb->len;
			clip6_arp_rcv(skb);
			return;
		}
#endif /* if 0 */
	}
	clip6_vcc->last_use = jiffies;
	PRIV(skb->dev)->stats.rx_packets++;
	PRIV(skb->dev)->stats.rx_bytes += skb->len;
	netif_rx(skb);
}


/*
 * Note: these spinlocks _must_not_ block on non-SMP. The only goal is that
 * clip6_pop is atomic with respect to the critical section in clip6_start_xmit.
 */


static void clip6_pop(struct atm_vcc *vcc,struct sk_buff *skb)
{
	struct clip6_vcc *clip6_vcc = CLIP6_VCC(vcc);
	struct net_device *dev = skb->dev;
	int old;
	unsigned long flags;

	DPRINTK("clip6_pop(vcc %p)\n",vcc);
	clip6_vcc->old_pop(vcc,skb);
	/* skb->dev == NULL in outbound ARP packets */
	if (!dev) return;
	spin_lock_irqsave(&PRIV(dev)->xoff_lock,flags);
	if (atm_may_send(vcc,0)) {
		old = xchg(&clip6_vcc->xoff,0);
		if (old) netif_wake_queue(dev);
	}
	spin_unlock_irqrestore(&PRIV(dev)->xoff_lock,flags);
}


/* @@@ copy bh locking from arp.c -- need to bh-enable atm code before */

/*
 * We play with the resolve flag: 0 and 1 have the usual meaning, but -1 means
 * to allocate the neighbour entry but not to ask atmarpd for resolution. Also,
 * don't increment the usage count. This is used to create entries in
 * clip6_setentry.
 */


int clip6_encap(struct atm_vcc *vcc,int mode)
{
	if (mode != 1) return -EINVAL; /* Only LLC/SNAP is supported */
	CLIP6_VCC(vcc)->encap = mode;
	return 0;
}


static int clip6_start_xmit(struct sk_buff *skb,struct net_device *dev)
{
	struct clip6_priv *clip6_priv = PRIV(dev);
	struct atm_vcc *vcc;
	int old;
	unsigned long flags;

	DPRINTK("clip6_start_xmit (skb %p)\n",skb);

	if (clip6_priv->vccs) {
		ATM_SKB(skb)->vcc = vcc = clip6_priv->vccs->vcc;
		if (vcc) {
			DPRINTK("using p2p vcc %p\n",vcc);
			if (clip6_priv->vccs->encap) {
				void *here;

				here = skb_push(skb,RFC1483LLC_LEN);
				memcpy(here,llc_oui,sizeof(llc_oui));
				((u16 *) here)[3] = skb->protocol;
			}
			atomic_add(skb->truesize,&vcc->tx_inuse);
			ATM_SKB(skb)->iovcnt = 0;
			ATM_SKB(skb)->atm_options = vcc->atm_options;
				/* TODO: check */
			clip6_priv->vccs->last_use = jiffies;
			DPRINTK("atm_skb(%p)->vcc(%p)->dev(%p)\n",skb,vcc,vcc->dev);
			old = xchg(&clip6_priv->vccs->xoff,1); /* assume XOFF ... */
			if (old) {
				printk(KERN_WARNING "clip6_start_xmit: XOFF->XOFF transition\n");
				return 0;
			}
			clip6_priv->stats.tx_packets++;
			clip6_priv->stats.tx_bytes += skb->len;
			(void) vcc->send(vcc,skb);
			if (atm_may_send(vcc,0)) {
				clip6_priv->vccs->xoff = 0;
				return 0;
			}
			spin_lock_irqsave(&clip6_priv->xoff_lock,flags);
			netif_stop_queue(dev); /* XOFF -> throttle immediately */
			barrier();
			if (!clip6_priv->vccs->xoff)
				netif_start_queue(dev);
		/* Oh, we just raced with clip6_pop. netif_start_queue should be
		   good enough, because nothing should really be asleep because
		   of the brief netif_stop_queue. If this isn't true or if it
		   changes, use netif_wake_queue instead. */
			spin_unlock_irqrestore(&clip6_priv->xoff_lock,flags);
			return 0;
		}
	}
	
				/* Bellows are for SVC, not supported yet. */
	printk(KERN_ERR "clip6_start_xmit: vccs == NULL\n");
	dev_kfree_skb(skb);
	clip6_priv->stats.tx_dropped++;
	return 0;
}


static struct net_device_stats *clip6_get_stats(struct net_device *dev)
{
	return &PRIV(dev)->stats;
}


int clip6_mkip(struct atm_vcc *vcc,int timeout)
{
	struct clip6_vcc *clip6_vcc;
	struct sk_buff_head copy;
	struct sk_buff *skb;

	if (!vcc->push) return -EBADFD;
	clip6_vcc = kmalloc(sizeof(struct clip6_vcc),GFP_KERNEL);
	if (!clip6_vcc) return -ENOMEM;
	DPRINTK("mkip6 clip6_vcc %p vcc %p\n",clip6_vcc,vcc);
	clip6_vcc->vcc = vcc;
	vcc->user_back = clip6_vcc;
	clip6_vcc->entry = NULL;
	clip6_vcc->dev = NULL;
	clip6_vcc->xoff = 0;
	clip6_vcc->encap = 1;
	clip6_vcc->last_use = jiffies;
	clip6_vcc->idle_timeout = timeout*HZ;
	clip6_vcc->old_push = vcc->push;
	clip6_vcc->old_pop = vcc->pop;
	vcc->push = clip6_push;
	vcc->pop = clip6_pop;
	skb_queue_head_init(&copy);
	skb_migrate(&vcc->recvq,&copy);
	/* re-process everything received between connection setup and MKIP */
	while ((skb = skb_dequeue(&copy)))
		if (!clip6_devs) {
			atm_return(vcc,skb->truesize);
			kfree_skb(skb);
		}
		else {
			unsigned int len = skb->len;

			clip6_push(vcc,skb);
			PRIV(skb->dev)->stats.rx_packets--;
			PRIV(skb->dev)->stats.rx_bytes -= len;
		}
	return 0;
}


int clip6_set_vcc_netif(struct socket *sock,int number)
{
	struct clip6_vcc *clip6_vcc;
	struct sock *sk = NULL;
	struct net_device *dev;
	struct atm_vcc *vcc = ATM_SD(sock);
	
	DPRINTK("clip6_set_vcc_netif 0x%08x\n", (unsigned int)vcc);
	if (vcc->push != clip6_push) {
		printk(KERN_WARNING "clip6_set_vcc_netif: non-CLIP VCC\n");
		return -EBADF;
	}
		/* allocate a scapegoat sk and vcc */
	if (NULL == (sk = alloc_atm_vcc_sk(sock->sk->family))) {
		printk(KERN_WARNING "clip6_set_vcc_netif: can not allocate VCC(scapegoat).\n");
		return -ENOMEM;
	}

	clip6_vcc = CLIP6_VCC(vcc);

	for (dev = clip6_devs; dev; dev = PRIV(dev)->next) {
	    if (PRIV(dev)->number == number) {
	        PRIV(dev)->vccs = clip6_vcc;
		clip6_vcc->dev = dev;
		if (vcc->dev) {
				/* copy MAC address */
				/* TODO: This will cause address duplication
				     in case loop back. 
				     To avoid this, dev_addr should include
				     the number of interface, or such. */
			dev->addr_len = ESI_LEN;
			memcpy(dev->dev_addr, vcc->dev->esi, dev->addr_len);
		}
			/* detach vcc from a soket */
		sk->rcvbuf = vcc->sk->rcvbuf;
		sk->sndbuf = vcc->sk->sndbuf;

		PRIV(dev)->vcc = vcc;
		PRIV(dev)->vcc->sk = sk;
		
		*(&ATM_SD(sock)) = sk->protinfo.af_atm;
		sk->protinfo.af_atm->sk = sock->sk;

				/* TODO: ininialize lists, 	vcc->prev,next, nodev_vccs */
		return 0;
	    }
	}
	return -ENODEV;
}


static int clip6_init(struct net_device *dev)
{
	DPRINTK("clip6_init %s\n",dev->name);
	dev->hard_start_xmit = clip6_start_xmit;
	/* sg_xmit ... */
	dev->hard_header = NULL;
	dev->rebuild_header = NULL;
	dev->set_mac_address = NULL;
	dev->hard_header_parse = NULL;
	dev->hard_header_cache = NULL;
	dev->header_cache_update = NULL;
	dev->change_mtu = NULL;
	dev->do_ioctl = NULL;
	dev->get_stats = clip6_get_stats;
	dev->type = ARPHRD_ATM;
	dev->hard_header_len = RFC1483LLC_LEN;
				/* TODO: check */
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


int clip6_create(int number)
{
	struct net_device *dev;
	struct clip6_priv *clip6_priv;
	int error;

	DPRINTK("clip6_create\n");
	if (number != -1) {
		for (dev = clip6_devs; dev; dev = PRIV(dev)->next)
			if (PRIV(dev)->number == number) return -EEXIST;
	}
	else {
		number = 0;
		for (dev = clip6_devs; dev; dev = PRIV(dev)->next)
			if (PRIV(dev)->number >= number)
				number = PRIV(dev)->number+1;
		DPRINTK("clip6_create number %d\n", number);
	}
	dev = kmalloc(sizeof(struct net_device)+sizeof(struct clip6_priv),
	    GFP_KERNEL); 
	DPRINTK("clip6_create mem\n");
	if (!dev) return -ENOMEM;
	DPRINTK("clip6_create mem ok\n");
	memset(dev,0,sizeof(struct net_device)+sizeof(struct clip6_priv));
	clip6_priv = PRIV(dev);
	sprintf(dev->name,"atm%d",number);
	dev->init = clip6_init;
	spin_lock_init(&clip6_priv->xoff_lock);
	clip6_priv->number = number;
	error = register_netdev(dev);
	if (error) {
		kfree(dev);
		DPRINTK("clip6_create error %d\n",error);
		return error;
	}
	clip6_priv->next = clip6_devs;
	clip6_devs = dev;
	clip6_priv->vccs = NULL;
	clip6_priv->vcc = NULL;
	DPRINTK("registered (net:%s)\n",dev->name);
	return number;
}


static int clip6_device_event(struct notifier_block *this,unsigned long event,
    void *dev)
{
	/* ignore non-CLIP devices */
	if (((struct net_device *) dev)->type != ARPHRD_ATM ||
	    ((struct net_device *) dev)->init != clip6_init)
		return NOTIFY_DONE;
	switch (event) {
		case NETDEV_UP:
			DPRINTK("clip6_device_event NETDEV_UP\n");
			/* ignore */
			break;
		case NETDEV_GOING_DOWN:
			DPRINTK("clip6_device_event NETDEV_DOWN\n");
			/* ignore */
			break;
		case NETDEV_CHANGE:
		case NETDEV_CHANGEMTU:
			DPRINTK("clip6_device_event NETDEV_CHANGE*\n");
			/* ignore */
			break;
		case NETDEV_REBOOT:
		case NETDEV_REGISTER:
		case NETDEV_DOWN:
			DPRINTK("clip6_device_event %ld\n",event);
			/* ignore */
			break;
		default:
			printk(KERN_WARNING "clip6_device_event: unknown event "
			    "%ld\n",event);
			break;
	}
	return NOTIFY_DONE;
}
