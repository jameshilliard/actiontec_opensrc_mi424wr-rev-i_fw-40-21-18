/*
 * @(#) Initialization code.
 * Copyright (C) 1996, 1997  John Ioannidis.
 * Copyright (C) 1998, 1999, 2000, 2001  Richard Guy Briggs <rgb@freeswan.org>
 *                                 2001  Michael Richardson <mcr@freeswan.org>
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
 * /proc system code was split out into ipsec_proc.c after rev. 1.70.
 *
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
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
#include <linux/in.h>          /* struct sockaddr_in */
#include <linux/skbuff.h>
#include <net/protocol.h>
#include <freeswan.h>

#ifdef NET_21
#include <asm/uaccess.h>
#include <linux/in6.h>
#endif /* NET_21 */

#include <asm/checksum.h>
#include <net/ip.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif /* CONFIG_PROC_FS */

#ifdef NETLINK_SOCK
#include <linux/netlink.h>
#else
#include <net/netlink.h>
#endif

#include "radij.h"

#include "ipsec_life.h"
#include "ipsec_stats.h"
#include "ipsec_sa.h"

#include "ipsec_encap.h"
#include "ipsec_radij.h"
#include "ipsec_netlink.h"
#include "ipsec_xform.h"
#include "ipsec_tunnel.h"

#include "ipsec_rcv.h"
#include "ipsec_rcv_common.h"
#include "ipsec_ah.h"
#include "ipsec_esp.h"

#ifdef CONFIG_IPSEC_IPCOMP
#include "ipcomp.h"
#endif /* CONFIG_IPSEC_IPCOMP */

#include "ipsec_proto.h"
#include "ipsec_alg.h"
#include "ipsec_log.h"

#include <pfkeyv2.h>
#include <pfkey.h>

#if defined(NET_26) && defined(CONFIG_IPSEC_NAT_TRAVERSAL)
#include <net/xfrmudp.h>
#endif

#ifdef CONFIG_IPSEC_USE_IXP4XX_CRYPTO
#include "ipsec_glue.h"
#endif

#if !defined(CONFIG_IPSEC_ESP) && !defined(CONFIG_IPSEC_AH)
#error "kernel configuration must include ESP or AH"
#endif

/*
 * seems to be present in 2.4.10 (Linus), but also in some RH and other
 * distro kernels of a lower number.
 */
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

#ifdef CONFIG_IPSEC_DEBUG
int debug_eroute = 0;
int debug_spi = 0;
int debug_netlink = 0;
#endif /* CONFIG_IPSEC_DEBUG */

#if !defined(NET_26) && defined(CONFIG_IPSEC_NAT_TRAVERSAL)
extern void udp_espinudp_encap_set(
	int (*func)(struct sk_buff *skb, struct sock *sk));
#endif

#if defined(NET_26) && defined(CONFIG_IPSEC_NAT_TRAVERSAL)
xfrm4_rcv_encap_t klips_old_encap = NULL;
#endif

int ipsec_device_event(struct notifier_block *dnot, unsigned long event, void *ptr);
/*
 * the following structure is required so that we receive
 * event notifications when network devices are enabled and
 * disabled (ifconfig up and down).
 */
static struct notifier_block ipsec_dev_notifier={
	ipsec_device_event,
	NULL,
	0
};

#ifdef CONFIG_SYSCTL
extern int ipsec_sysctl_register(void);
extern void ipsec_sysctl_unregister(void);
#endif

#ifdef NET_26
static inline int
rg_inet_add_protocol(struct inet_protocol *prot, unsigned protocol)
{
	return inet_add_protocol(prot, protocol);
}

static inline int
rg_inet_del_protocol(struct inet_protocol *prot, unsigned protocol)
{
	return inet_del_protocol(prot, protocol);
}

#else
static inline int
rg_inet_add_protocol(struct inet_protocol *prot, unsigned protocol)
{
	inet_add_protocol(prot);
	return 0;
}

static inline int
rg_inet_del_protocol(struct inet_protocol *prot, unsigned protocol)
{
	inet_del_protocol(prot);
	return 0;
}

#endif

/* void */
int
ipsec_init(void)
{
	int error = 0;
#ifdef CONFIG_IPSEC_ENC_3DES
	extern int des_check_key;

	/* turn off checking of keys */
	des_check_key=0;
#endif /* CONFIG_IPSEC_ENC_3DES */

	KLIPS_PRINT(1, "klips_info:ipsec_init: "
		    "KLIPS startup, FreeS/WAN IPSec version: %s\n",
		    ipsec_version_code());

	error |= ipsec_proc_init();

#ifndef SPINLOCK
	tdb_lock.lock = 0;
	eroute_lock.lock = 0;
#endif /* !SPINLOCK */

	error |= ipsec_sadb_init();
	error |= ipsec_radijinit();

	error |= pfkey_init();

	error |= register_netdevice_notifier(&ipsec_dev_notifier);

#ifdef CONFIG_IPSEC_ESP
	rg_inet_add_protocol(&esp_protocol, IPPROTO_ESP);
#endif /* CONFIG_IPSEC_ESP */

#ifdef CONFIG_IPSEC_AH
	rg_inet_add_protocol(&ah_protocol, IPPROTO_AH);
#endif /* CONFIG_IPSEC_AH */

#if 0
#ifdef CONFIG_IPSEC_IPCOMP
	rg_inet_add_protocol(&comp_protocol, IPPROTO_COMP);
#endif /* CONFIG_IPSEC_IPCOMP */
#endif

	error |= ipsec_tunnel_init_devices();

#if !defined(NET_26) && defined(CONFIG_IPSEC_NAT_TRAVERSAL)
	udp_espinudp_encap_set(ipsec_espinudp_encap);
#endif

#if defined(NET_26) && defined(CONFIG_IPSEC_NAT_TRAVERSAL)
	/* register our ESP-UDP handler */
	if (udp4_register_esp_rcvencap(klips26_rcv_encap, &klips_old_encap)) {
	    printk(KERN_ERR "KLIPS: can not register klips_rcv_encap function\n");
	}
#endif	

#ifdef CONFIG_SYSCTL
        error |= ipsec_sysctl_register();
#endif                                                                          

#ifdef CONFIG_IPSEC_ALG
	ipsec_alg_init();
#endif

#ifdef CONFIG_IPSEC_USE_IXP4XX_CRYPTO
	ipsec_glue_crypto_ctx_init();
	ipsec_glue_sa_crypto_context_map_init();
#endif
	return error;
}	


/* void */
int
ipsec_cleanup(void)
{
	int error = 0;

#ifdef CONFIG_SYSCTL
        ipsec_sysctl_unregister();
#endif
#if defined(NET_26) && defined(CONFIG_IPSEC_NAT_TRAVERSAL)
	if (udp4_unregister_esp_rcvencap(klips26_rcv_encap, klips_old_encap) < 0) {
		printk(KERN_ERR "KLIPS: can not unregister klips_rcv_encap function\n");
	}
#endif
#if !defined(NET_26) && defined(CONFIG_IPSEC_NAT_TRAVERSAL)
	udp_espinudp_encap_set(NULL);
#endif

	KLIPS_PRINT(debug_netlink, /* debug_tunnel & DB_TN_INIT, */
		    "klips_debug:ipsec_cleanup: "
		    "calling ipsec_tunnel_cleanup_devices.\n");
	error |= ipsec_tunnel_cleanup_devices();

#if 0
#ifdef CONFIG_IPSEC_IPCOMP
	if (rg_inet_del_protocol(&comp_protocol, IPPROTO_COMP) < 0)
		ipsec_log(KERN_INFO "klips_debug:ipsec_cleanup: "
		       "comp close: can't remove protocol\n");
#endif /* CONFIG_IPSEC_IPCOMP */
#endif /* 0 */
#ifdef CONFIG_IPSEC_AH
	if (rg_inet_del_protocol(&ah_protocol, IPPROTO_AH) < 0)
		ipsec_log(KERN_INFO "klips_debug:ipsec_cleanup: "
		       "ah close: can't remove protocol\n");
#endif /* CONFIG_IPSEC_AH */
#ifdef CONFIG_IPSEC_ESP
	if (rg_inet_del_protocol(&esp_protocol, IPPROTO_ESP) < 0)
		ipsec_log(KERN_INFO "klips_debug:ipsec_cleanup: "
		       "esp close: can't remove protocol\n");
#endif /* CONFIG_IPSEC_ESP */

	error |= unregister_netdevice_notifier(&ipsec_dev_notifier);

	KLIPS_PRINT(debug_netlink, /* debug_tunnel & DB_TN_INIT, */
		    "klips_debug:ipsec_cleanup: "
		    "calling ipsec_tdbcleanup.\n");
	error |= ipsec_sadb_cleanup(0);
	KLIPS_PRINT(debug_netlink, /* debug_tunnel & DB_TN_INIT, */
		    "klips_debug:ipsec_cleanup: "
		    "calling ipsec_radijcleanup.\n");
	error |= ipsec_radijcleanup();
	
	KLIPS_PRINT(debug_pfkey, /* debug_tunnel & DB_TN_INIT, */
		    "klips_debug:ipsec_cleanup: "
		    "calling pfkey_cleanup.\n");
	error |= pfkey_cleanup();

	ipsec_proc_cleanup();

	return error;
}

#ifdef MODULE
int
init_module(void)
{
	int error = 0;

	error |= ipsec_init();

	return error;
}

void
cleanup_module(void)
{
	KLIPS_PRINT(debug_netlink, /* debug_tunnel & DB_TN_INIT, */
		    "klips_debug:cleanup_module: "
		    "calling ipsec_cleanup.\n");

	ipsec_cleanup();

	KLIPS_PRINT(1, "klips_info:cleanup_module: "
		    "ipsec module unloaded.\n");
}
#endif /* MODULE */

