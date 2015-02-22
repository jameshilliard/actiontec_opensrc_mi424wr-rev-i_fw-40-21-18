/*
 * IPSEC <> netlink interface
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
 */

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

#include <linux/netdevice.h>   /* struct net_device, and other headers */
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
# define ip_chk_addr inet_addr_type
# define IS_MYADDR RTN_LOCAL
#endif
#include <asm/checksum.h>
#include <net/ip.h>
#ifdef NETLINK_SOCK
# include <linux/netlink.h>
#else
# include <net/netlink.h>
#endif

#include "radij.h"
#include "ipsec_encap.h"
#include "ipsec_radij.h"
#include "ipsec_netlink.h"
#include "ipsec_xform.h"

#include "ipsec_rcv.h"
#include "ipsec_ah.h"
#include "ipsec_esp.h"

#ifdef CONFIG_IPSEC_DEBUG
# include "ipsec_tunnel.h"
#endif /* CONFIG_IPSEC_DEBUG */

#include <pfkeyv2.h>
#include <pfkey.h>

#ifdef CONFIG_IPSEC_DEBUG
 int debug_netlink = 0;
#endif /* CONFIG_IPSEC_DEBUG */

#define SENDERR(_x) do { len = -(_x); goto errlab; } while (0)

#if 0
int 
#ifdef NETLINK_SOCK
ipsec_callback(int proto, struct sk_buff *skb)
#else /* NETLINK_SOCK */
ipsec_callback(struct sk_buff *skb)
#endif /* NETLINK_SOCK */
{
	/*
	 * this happens when we write to /dev/ipsec (c 36 10)
	 */
	int len = skb->len;
	u_char *dat = (u_char *)skb->data;
	struct encap_msghdr *em	= (struct encap_msghdr *)dat;
	struct tdb *tdbp, *tprev;
	int i, nspis, error = 0;
#ifdef CONFIG_IPSEC_DEBUG
	struct eroute *eret;
	char sa[SATOA_BUF];
	size_t sa_len;
	struct sk_buff *first, *last;


	sa_len = satoa(em->em_said, 0, sa, SATOA_BUF);

	if(debug_netlink) {
		printk("klips_debug:ipsec_callback: "
		       "skb=0x%p skblen=%ld em_magic=%d em_type=%d\n",
		       skb,
		       (unsigned long int)skb->len,
		       em->em_magic,
		       em->em_type);
		switch(em->em_type) {
		case EMT_SETDEBUG:
			printk("klips_debug:ipsec_callback: "
			       "set ipsec_debug level\n");
			break;
		case EMT_DELEROUTE:
		case EMT_CLREROUTE:
		case EMT_CLRSPIS:
			break;
		default:
			printk("klips_debug:ipsec_callback: "
			       "called for SA:%s\n",
			       sa_len ? sa : " (error)");
		}
	}
#endif /* CONFIG_IPSEC_DEBUG */

	/* XXXX Temporarily disable netlink I/F code until it gets permanantly
	   ripped out in favour of PF_KEYv2 I/F. */
	SENDERR(EPROTONOSUPPORT);

	/*	em = (struct encap_msghdr *)dat; */
	if (em->em_magic != EM_MAGIC) {
		printk("klips_debug:ipsec_callback: "
		       "bad magic=%d failed, should be %d\n",
		       em->em_magic,
		       EM_MAGIC);
		SENDERR(EINVAL);
	}
	switch (em->em_type) {
	case EMT_SETDEBUG:
#ifdef CONFIG_IPSEC_DEBUG
		if(em->em_db_nl >> (sizeof(em->em_db_nl) * 8 - 1)) {
			em->em_db_nl &= ~(1 << (sizeof(em->em_db_nl) * 8 -1));
			debug_tunnel  |= em->em_db_tn;
			debug_netlink |= em->em_db_nl;
			debug_xform   |= em->em_db_xf;
			debug_eroute  |= em->em_db_er;
			debug_spi     |= em->em_db_sp;
			debug_radij   |= em->em_db_rj;
			debug_esp     |= em->em_db_es;
			debug_ah      |= em->em_db_ah;
			debug_rcv     |= em->em_db_rx;
			debug_pfkey   |= em->em_db_ky;
			if(debug_netlink)
				printk("klips_debug:ipsec_callback: set\n");
		} else {
			if(debug_netlink)
				printk("klips_debug:ipsec_callback: unset\n");
			debug_tunnel  &= em->em_db_tn;
			debug_netlink &= em->em_db_nl;
			debug_xform   &= em->em_db_xf;
			debug_eroute  &= em->em_db_er;
			debug_spi     &= em->em_db_sp;
			debug_radij   &= em->em_db_rj;
			debug_esp     &= em->em_db_es;
			debug_ah      &= em->em_db_ah;
			debug_rcv     &= em->em_db_rx;
			debug_pfkey   &= em->em_db_ky;
		}
#else /* CONFIG_IPSEC_DEBUG */
		printk("klips_debug:ipsec_callback: "
		       "debugging not enabled\n");
		SENDERR(EINVAL);
#endif /* CONFIG_IPSEC_DEBUG */
		break;

	case EMT_SETEROUTE:
		if ((error = ipsec_makeroute(&(em->em_eaddr), &(em->em_emask), em->em_ersaid, 0, NULL, NULL, NULL)))
			SENDERR(-error);
		break;

	case EMT_REPLACEROUTE:
		if ((error = ipsec_breakroute(&(em->em_eaddr), &(em->em_emask), &first, &last)) == EINVAL) {
			kfree_skb(first);
			kfree_skb(last);
			SENDERR(-error);
		}
		if ((error = ipsec_makeroute(&(em->em_eaddr), &(em->em_emask), em->em_ersaid, NULL, NULL)))
			SENDERR(-error);
		break;

	case EMT_DELEROUTE:
		if ((error = ipsec_breakroute(&(em->em_eaddr), &(em->em_emask), &first, &last)))
			kfree_skb(first);
			kfree_skb(last);
			SENDERR(-error);
		break;

	case EMT_CLREROUTE:
		if ((error = ipsec_cleareroutes()))
			SENDERR(-error);
		break;

	case EMT_SETSPI:
		if (em->em_if >= 5)	/* XXX -- why 5? */
			SENDERR(ENODEV);
		
		tdbp = gettdb(&(em->em_said));
		if (tdbp == NULL) {
			tdbp = (struct tdb *)kmalloc(sizeof (*tdbp), GFP_ATOMIC);

			if (tdbp == NULL)
				SENDERR(ENOBUFS);
			
			memset((caddr_t)tdbp, 0, sizeof(*tdbp));
			
			tdbp->tdb_said = em->em_said;
			tdbp->tdb_flags = em->em_flags;
			
			if(ip_chk_addr((unsigned long)em->em_said.dst.s_addr) == IS_MYADDR) {
				tdbp->tdb_flags |= EMT_INBOUND;
			}
			KLIPS_PRINT(debug_netlink & DB_NL_TDBCB,
				    "klips_debug:ipsec_callback: "
				    "existing Tunnel Descriptor Block not found (this is good) for SA: %s, %s-bound, allocating.\n",
				    sa_len ? sa : " (error)",
				    (tdbp->tdb_flags & EMT_INBOUND) ? "in" : "out");

/* XXX  		tdbp->tdb_rcvif = &(enc_softc[em->em_if].enc_if);*/
			tdbp->tdb_rcvif = NULL;
		} else {
			KLIPS_PRINT(debug_netlink & DB_NL_TDBCB,
				    "klips_debug:ipsec_callback: "
				    "EMT_SETSPI found an old Tunnel Descriptor Block for SA: %s, delete it first.\n",
				    sa_len ? sa : " (error)");
			SENDERR(EEXIST);
		}
		
		if ((error = tdb_init(tdbp, em))) {
			KLIPS_PRINT(debug_netlink & DB_NL_TDBCB,
				    "klips_debug:ipsec_callback: "
				    "EMT_SETSPI not successful for SA: %s, deleting.\n",
				    sa_len ? sa : " (error)");
			ipsec_tdbwipe(tdbp);
			
			SENDERR(-error);
		}

		tdbp->tdb_lifetime_addtime_c = jiffies/HZ;
		tdbp->tdb_state = 1;
		if(!tdbp->tdb_lifetime_allocations_c) {
			tdbp->tdb_lifetime_allocations_c += 1;
		}

		puttdb(tdbp);
		KLIPS_PRINT(debug_netlink & DB_NL_TDBCB,
			    "klips_debug:ipsec_callback: "
			    "EMT_SETSPI successful for SA: %s\n",
			    sa_len ? sa : " (error)");
		break;
		
	case EMT_DELSPI:
		if (em->em_if >= 5)	/* XXX -- why 5? */
			SENDERR(ENODEV);
		
		spin_lock_bh(&tdb_lock);
		
		tdbp = gettdb(&(em->em_said));
		if (tdbp == NULL) {
			KLIPS_PRINT(debug_netlink & DB_NL_TDBCB,
				    "klips_debug:ipsec_callback: "
				    "EMT_DELSPI Tunnel Descriptor Block not found for SA%s, could not delete.\n",
				    sa_len ? sa : " (error)");
			spin_unlock_bh(&tdb_lock);
			SENDERR(ENXIO);  /* XXX -- wrong error message... */
		} else {
			if((error = deltdbchain(tdbp))) {
				spin_unlock_bh(&tdb_lock);
				SENDERR(-error);
			}
		}
		spin_unlock_bh(&tdb_lock);

		break;
		
	case EMT_GRPSPIS:
		nspis = (len - EMT_GRPSPIS_FLEN) / sizeof(em->em_rel[0]);
		if ((nspis * (sizeof(em->em_rel[0]))) != (len - EMT_GRPSPIS_FLEN)) {
			printk("klips_debug:ipsec_callback: "
			       "EMT_GRPSPI message size incorrect, expected nspis(%d)*%d, got %d.\n",
			       nspis,
			       sizeof(em->em_rel[0]),
			       (len - EMT_GRPSPIS_FLEN));
			SENDERR(EINVAL);
			break;
		}
		
		spin_lock_bh(&tdb_lock);

		for (i = 0; i < nspis; i++) {
			KLIPS_PRINT(debug_netlink,
				    "klips_debug:ipsec_callback: "
				    "EMT_GRPSPI for SA(%d) %s,\n",
				    i,
				    sa_len ? sa : " (error)");
			if ((tdbp = gettdb(&(em->em_rel[i].emr_said))) == NULL) {
				KLIPS_PRINT(debug_netlink,
					    "klips_debug:ipsec_callback: "
					    "EMT_GRPSPI Tunnel Descriptor Block not found for SA%s, could not group.\n",
					    sa_len ? sa : " (error)");
				spin_unlock_bh(&tdb_lock);
				SENDERR(ENXIO);
			} else {
				if(tdbp->tdb_inext || tdbp->tdb_onext) {
					KLIPS_PRINT(debug_netlink,
						    "klips_debug:ipsec_callback: "
						    "EMT_GRPSPI Tunnel Descriptor Block already grouped for SA: %s, can't regroup.\n",
						    sa_len ? sa : " (error)");
					spin_unlock_bh(&tdb_lock);
					SENDERR(EBUSY);
				}
				em->em_rel[i].emr_tdb = tdbp;
			}
		}
		tprev = em->em_rel[0].emr_tdb;
		tprev->tdb_inext = NULL;
		for (i = 1; i < nspis; i++) {
			tdbp = em->em_rel[i].emr_tdb;
			tprev->tdb_onext = tdbp;
			tdbp->tdb_inext = tprev;
			tprev = tdbp;
		}
		tprev->tdb_onext = NULL;

		spin_unlock_bh(&tdb_lock);

		error = 0;
		break;
		
	case EMT_UNGRPSPIS:
		if (len != (8 + (sizeof(struct sa_id) + sizeof(struct tdb *)) /* 12 */) ) {
			printk("klips_debug:ipsec_callback: "
			       "EMT_UNGRPSPIS message size incorrect, expected %d, got %d.\n",
			       8 + (sizeof(struct sa_id) + sizeof(struct tdb *)),
				    len);
			SENDERR(EINVAL);
			break;
		}
		
		spin_lock_bh(&tdb_lock);

		if ((tdbp = gettdb(&(em->em_rel[0].emr_said))) == NULL) {
			KLIPS_PRINT(debug_netlink,
				    "klips_debug:ipsec_callback: "
				    "EMT_UGRPSPI Tunnel Descriptor Block not found for SA%s, could not ungroup.\n",
				    sa_len ? sa : " (error)");
			spin_unlock_bh(&tdb_lock);
			SENDERR(ENXIO);
		}
		while(tdbp->tdb_onext) {
			tdbp = tdbp->tdb_onext;
		}
		while(tdbp->tdb_inext) {
			tprev = tdbp;
			tdbp = tdbp->tdb_inext;
			tprev->tdb_inext = NULL;
			tdbp->tdb_onext = NULL;
		}

		spin_unlock_bh(&tdb_lock);

		break;
		
	case EMT_CLRSPIS:
		KLIPS_PRINT(debug_netlink,
			    "klips_debug:ipsec_callback: "
			    "spi clear called.\n");
		if (em->em_if >= 5)	/* XXX -- why 5? */
			SENDERR(ENODEV);
		ipsec_tdbcleanup(0);
		break;
	default:
		KLIPS_PRINT(debug_netlink,
			    "klips_debug:ipsec_callback: "
			    "unknown message type\n");
		SENDERR(EINVAL);
	}
 errlab:
#ifdef NET_21
	kfree_skb(skb);
#else /* NET_21 */
	kfree_skb(skb, FREE_WRITE);
#endif /* NET_21 */
	return len;
}
#endif

