/* net/atm/atmarp.h - RFC1577 ATM ARP */
 
/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */
 
 
#ifndef _ATMCLIP_H
#define _ATMCLIP_H

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/spinlock.h>
#include <net/neighbour.h>


#define CLIP_VCC(vcc) ((struct clip_vcc *) ((vcc)->user_back))
#define NEIGH2ENTRY(neigh) ((struct atmarp_entry *) (neigh)->primary_key)


struct clip_vcc {
	struct atm_vcc	*vcc;		/* VCC descriptor */
	struct atmarp_entry *entry;	/* ATMARP table entry, NULL if IP addr.
					   isn't known yet */
	int		xoff;		/* 1 if send buffer is full */
	unsigned char	encap;		/* 0: NULL, 1: LLC/SNAP */
	unsigned long	last_use;	/* last send or receive operation */
	unsigned long	idle_timeout;	/* keep open idle for so many jiffies*/
	unsigned long	resolve_timeout;
	void (*old_push)(struct atm_vcc *vcc,struct sk_buff *skb);
					/* keep old push fn for chaining */
	void (*old_pop)(struct atm_vcc *vcc,struct sk_buff *skb);
					/* keep old pop fn for chaining */
	struct net_device *dev;		/* device that this vcc is associated
					 * with. */
	struct clip_vcc	*next;		/* next VCC in an atmarp_entry */
	struct clip_vcc *global_next;	/* next VCC in the global list of CLIP
					 * VCCs. */
	struct timer_list resolve_timer; 
};


struct atmarp_entry {
	u32		ip;		/* IP address */
	struct clip_vcc	*vccs;		/* active VCCs; NULL if resolution is
					   pending */
	unsigned long	expires;	/* entry expiration time */
	struct neighbour *neigh;	/* neighbour back-pointer */
};

extern struct neigh_table clip_tbl;

void atm_clip_init(void);
int clip_ioctl(struct atm_vcc *vcc, unsigned int cmd, unsigned long arg);

#endif
