/* net/atm/atmipv6.h - RFC2492 IPv6 over ATM */
 
/*
 * Copyright (C) 2001 USAGI/WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
/* based on atmarp.h - ATM ARP protocol and kernel-demon interface definitions */
/* Written 1995-1999 by Werner Almesberger, EPFL LRC/ICA */
 
 
#ifndef _ATMIPV6_H
#define _ATMIPV6_H

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/atmarp.h>
#include <linux/spinlock.h>
#include <net/neighbour.h>


#define CLIP6_VCC(vcc) ((struct clip6_vcc *) ((vcc)->user_back))
//#define NEIGH2ENTRY(neigh) ((struct atmarp_entry *) (neigh)->primary_key)


struct clip6_vcc {
	struct atm_vcc	*vcc;		/* VCC descriptor */
	struct atmarp_entry *entry;	/* ATMARP table entry, NULL if IP addr.
					   isn't known yet */
	int		xoff;		/* 1 if send buffer is full */
	unsigned char	encap;		/* 0: NULL, 1: LLC/SNAP */
	unsigned long	last_use;	/* last send or receive operation */
	unsigned long	idle_timeout;	/* keep open idle for so many jiffies*/
	void (*old_push)(struct atm_vcc *vcc,struct sk_buff *skb);
					/* keep old push fn for chaining */
	void (*old_pop)(struct atm_vcc *vcc,struct sk_buff *skb);
					/* keep old pop fn for chaining */
	struct clip6_vcc	*next;		/* next VCC */
        struct net_device *dev;   	/* network interface. */
};

#if 0
struct atmarp_entry {
	u32		ip;		/* IP address */
	struct clip_vcc	*vccs;		/* active VCCs; NULL if resolution is
					   pending */
	unsigned long	expires;	/* entry expiration time */
	struct neighbour *neigh;	/* neighbour back-pointer */
};
#endif /* if 0 */

//TODO:
#define PRIV(dev) ((struct clip6_priv *) ((struct net_device *) (dev)+1))
#define PRIV6(dev) ((struct clip6_priv *) ((struct net_device *) (dev)+1))


struct clip6_priv {
	int number;			/* for convenience ... */
	spinlock_t xoff_lock;		/* ensures that pop is atomic (SMP) */
	struct net_device_stats stats;
	struct net_device *next;	/* next CLIP6 interface */
  	struct clip6_vcc	*vccs;		/* active VCCs; NULL if resolution is
					   pending */
//	struct atm_vcc vccc;		/* vcc ToDo: del*/
	struct atm_vcc *vcc;		/* vcc */
};


//extern struct atm_vcc *atmarpd; /* ugly */
//extern struct neigh_table clip6_tbl;

int clip6_create(int number);
int clip6_mkip(struct atm_vcc *vcc,int timeout);
int clip6_encap(struct atm_vcc *vcc,int mode);

int clip6_set_vcc_netif(struct socket *sock,int number);

void clip6_push(struct atm_vcc *vcc,struct sk_buff *skb);

void atm_clip6_init(void);

#endif
