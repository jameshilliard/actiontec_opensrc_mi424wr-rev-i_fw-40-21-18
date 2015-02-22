/*
 * Common routines for IPSEC transformations.
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

#include <linux/config.h>
#include <linux/version.h>

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
#include <linux/random.h>	/* get_random_bytes() */
#include <freeswan.h>
#ifdef SPINLOCK
# ifdef SPINLOCK_23
#  include <linux/spinlock.h> /* *lock* */
# else /* SPINLOCK_23 */
#  include <asm/spinlock.h> /* *lock* */
# endif /* SPINLOCK_23 */
#endif /* SPINLOCK */
#ifdef NET_21
# include <asm/uaccess.h>
# include <linux/in6.h>
#endif
#include <asm/checksum.h>
#include <net/ip.h>

#include "radij.h"
#include "ipsec_encap.h"
#include "ipsec_radij.h"
#include "ipsec_netlink.h"
#include "ipsec_xform.h"
#include "ipsec_ipe4.h"
#include "ipsec_ah.h"
#include "ipsec_esp.h"

#include <pfkeyv2.h>
#include <pfkey.h>

#ifdef CONFIG_IPSEC_DEBUG
int debug_xform = 0;
#endif /* CONFIG_IPSEC_DEBUG */

#define SENDERR(_x) do { error = -(_x); goto errlab; } while (0)

extern int des_set_key(caddr_t, caddr_t);

struct xformsw xformsw[] = {
{ XF_IP4,		0,		"IPv4_Encapsulation"},
{ XF_AHHMACMD5,		XFT_AUTH,	"HMAC_MD5_Authentication"},
{ XF_AHHMACSHA1,	XFT_AUTH,	"HMAC_SHA-1_Authentication"},
{ XF_ESPDES,      	XFT_CONF,	"DES_Encryption"},
{ XF_ESPDESMD596, 	XFT_CONF,	"DES-MD5-96_Encryption"},
{ XF_ESPDESSHA196,   	XFT_CONF,	"DES-SHA1-96_Encryption"},
{ XF_ESP3DES,		XFT_CONF,	"3DES_Encryption"},
{ XF_ESP3DESMD596,	XFT_CONF,	"3DES-MD5-96_Encryption"},
{ XF_ESP3DESSHA196,	XFT_CONF,	"3DES-SHA1-96_Encryption"},
{ XF_ESPNULLMD596,	XFT_CONF,	"NULL-MD5-96_ESP_*Plaintext*"},
{ XF_ESPNULLSHA196,	XFT_CONF,	"NULL-SHA1-96_ESP_*Plaintext*"},
};

struct tdb *tdbh[TDB_HASHMOD];
#ifdef SPINLOCK
spinlock_t tdb_lock = SPIN_LOCK_UNLOCKED;
#else /* SPINLOCK */
spinlock_t tdb_lock;
#endif /* SPINLOCK */
struct xformsw *xformswNXFORMSW = &xformsw[sizeof(xformsw)/sizeof(xformsw[0])];

int
ipsec_tdbinit(void)
{
	int i;

	for(i = 1; i < TDB_HASHMOD; i++) {
		tdbh[i] = NULL;
	}
	return 0;
}

