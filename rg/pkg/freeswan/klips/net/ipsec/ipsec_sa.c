/*
 * Common routines for IPsec SA maintenance routines.
 *
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
 * This is the file formerly known as "ipsec_xform.h"
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
#ifdef SPINLOCK_23
#include <linux/spinlock.h> /* *lock* */
#else /* SPINLOCK_23 */
#include <asm/spinlock.h> /* *lock* */
#endif /* SPINLOCK_23 */
#endif /* SPINLOCK */
#ifdef NET_21
#include <asm/uaccess.h>
#include <linux/in6.h>
#endif
#include <asm/checksum.h>
#include <net/ip.h>

#ifdef CONFIG_IPSEC_USE_IXP4XX_CRYPTO
#include "ipsec_glue.h"		/* glue code */
#endif /* CONFIG_IPSEC_USE_IXP4XX_CRYPTO */

#include "radij.h"

#include "ipsec_stats.h"
#include "ipsec_life.h"
#include "ipsec_sa.h"
#include "ipsec_xform.h"

#include "ipsec_encap.h"
#include "ipsec_radij.h"
#include "ipsec_netlink.h"
#include "ipsec_xform.h"
#include "ipsec_ipe4.h"
#include "ipsec_ah.h"
#include "ipsec_esp.h"

#include <pfkeyv2.h>
#include <pfkey.h>

#include "ipsec_proto.h"
#include "ipsec_alg.h"
#include "ipsec_log.h"


#ifdef CONFIG_IPSEC_DEBUG
int debug_xform = 0;
#endif /* CONFIG_IPSEC_DEBUG */

#define SENDERR(_x) do { error = -(_x); goto errlab; } while (0)

struct ipsec_sa *ipsec_sadb_hash[SADB_HASHMOD];
#ifdef SPINLOCK
spinlock_t tdb_lock = SPIN_LOCK_UNLOCKED;
#else /* SPINLOCK */
spinlock_t tdb_lock;
#endif /* SPINLOCK */

#ifdef CONFIG_IPSEC_USE_IXP4XX_CRYPTO
UINT32 ipsec_glue_crypto_context_del (UINT32);
UINT32 ipsec_glue_crypto_context_put(struct ipsec_sa *);
#endif

int
ipsec_sadb_init(void)
{
	int i;

	for(i = 1; i < SADB_HASHMOD; i++) {
		ipsec_sadb_hash[i] = NULL;
	}
	return 0;
}

struct ipsec_sa *
ipsec_sa_getbyid(struct sa_id *said)
{
	int hashval;
	struct ipsec_sa *ips;
        char sa[SATOA_BUF];
	size_t sa_len;

	if(!said) {
		KLIPS_PRINT(debug_xform,
			    "klips_error:gettdb: "
			    "null pointer passed in!\n");
		return NULL;
	}

	sa_len = satoa(*said, 0, sa, SATOA_BUF);

	hashval = (said->spi+said->dst.s_addr+said->proto) % SADB_HASHMOD;
	
	KLIPS_PRINT(debug_xform,
		    "klips_debug:gettdb: "
		    "linked entry in tdb table for hash=%d of SA:%s requested.\n",
		    hashval,
		    sa_len ? sa : " (error)");

	if(!(ips = ipsec_sadb_hash[hashval])) {
		KLIPS_PRINT(debug_xform,
			    "klips_debug:gettdb: "
			    "no entries in tdb table for hash=%d of SA:%s.\n",
			    hashval,
			    sa_len ? sa : " (error)");
		return NULL;
	}

	for (; ips; ips = ips->ips_hnext) {
		if ((ips->ips_said.spi == said->spi) &&
		    (ips->ips_said.dst.s_addr == said->dst.s_addr) &&
		    (ips->ips_said.proto == said->proto)) {
			return ips;
		}
	}
	
	KLIPS_PRINT(debug_xform,
		    "klips_debug:gettdb: "
		    "no entry in linked list for hash=%d of SA:%s.\n",
		    hashval,
		    sa_len ? sa : " (error)");
	return NULL;
}

/*
  The tdb table better *NOT* be locked before it is handed in, or SMP locks will happen
*/
int
ipsec_sa_put(struct ipsec_sa *ips)
{
	int error = 0;
	unsigned int hashval;

	if(!ips) {
		KLIPS_PRINT(debug_xform,
			    "klips_error:puttdb: "
			    "null pointer passed in!\n");
		return -ENODATA;
	}
	hashval = ((ips->ips_said.spi + ips->ips_said.dst.s_addr + ips->ips_said.proto) % SADB_HASHMOD);

	spin_lock_bh(&tdb_lock);
	
	ips->ips_hnext = ipsec_sadb_hash[hashval];
	ipsec_sadb_hash[hashval] = ips;
	
	spin_unlock_bh(&tdb_lock);

#ifdef CONFIG_IPSEC_USE_IXP4XX_CRYPTO
	/* Add correspond crypto context for hardware accelarator */
	if (STATUS_FAIL == ipsec_glue_crypto_context_put (ips))
	{
		KLIPS_PRINT(debug_xform,
			    "klips_error:puttdb: "
			    "Cannot add crypto context!\n");
	}	
#endif /* CONFIG_IPSEC_USE_IXP4XX_CRYPTO */

	return error;
}

/*
  The tdb table better be locked before it is handed in, or races might happen
*/
int
ipsec_sa_del(struct ipsec_sa *ips)
{
	unsigned int hashval;
	struct ipsec_sa *tdbtp;
        char sa[SATOA_BUF];
	size_t sa_len;

	if(!ips) {
		KLIPS_PRINT(debug_xform,
			    "klips_error:deltdb: "
			    "null pointer passed in!\n");
		return -ENODATA;
	}
	
	sa_len = satoa(ips->ips_said, 0, sa, SATOA_BUF);
	if(ips->ips_inext || ips->ips_onext) {
		KLIPS_PRINT(debug_xform,
			    "klips_error:deltdb: "
			    "SA:%s still linked!\n",
			    sa_len ? sa : " (error)");
		return -EMLINK;
	}
	
	hashval = ((ips->ips_said.spi + ips->ips_said.dst.s_addr + ips->ips_said.proto) % SADB_HASHMOD);
	
	KLIPS_PRINT(debug_xform,
		    "klips_debug:deltdb: "
		    "deleting SA:%s, hashval=%d.\n",
		    sa_len ? sa : " (error)",
		    hashval);
	if(!ipsec_sadb_hash[hashval]) {
		KLIPS_PRINT(debug_xform,
			    "klips_debug:deltdb: "
			    "no entries in tdb table for hash=%d of SA:%s.\n",
			    hashval,
			    sa_len ? sa : " (error)");
		return -ENOENT;
	}
	
	if (ips == ipsec_sadb_hash[hashval]) {
		ipsec_sadb_hash[hashval] = ipsec_sadb_hash[hashval]->ips_hnext;
		ips->ips_hnext = NULL;
		KLIPS_PRINT(debug_xform,
			    "klips_debug:deltdb: "
			    "successfully deleted first tdb in chain.\n");
#ifdef CONFIG_IPSEC_USE_IXP4XX_CRYPTO
         	/* Delete cryto context associate with this SA */
	        if (STATUS_FAIL == 
	        	ipsec_glue_crypto_context_del(ips->ips_crypto_context_id))
	        {
	    	 	KLIPS_PRINT(debug_xform,
				    "klips_error:deltdb: "
			    	    "Cannot delete crypto context!\n");
	    	}		
#endif /* CONFIG_IPSEC_USE_IXP4XX_CRYPTO */

		return 0;
	} else {
		for (tdbtp = ipsec_sadb_hash[hashval];
		     tdbtp;
		     tdbtp = tdbtp->ips_hnext) {
			if (tdbtp->ips_hnext == ips) {
				tdbtp->ips_hnext = ips->ips_hnext;
				ips->ips_hnext = NULL;
				KLIPS_PRINT(debug_xform,
					    "klips_debug:deltdb: "
					    "successfully deleted link in tdb chain.\n");
#ifdef CONFIG_IPSEC_USE_IXP4XX_CRYPTO					 
				/* Delete cryto context associate with this SA -in the chain */
	        		if (STATUS_FAIL == 
	        			ipsec_glue_crypto_context_del(ips->ips_crypto_context_id))
	        		{
	    	 			KLIPS_PRINT(debug_xform,
				    		"klips_error:deltdb: "
			    	    		"Cannot delete crypto context!\n");
	    			}
#endif /* CONFIG_IPSEC_USE_IXP4XX_CRYPTO */
				return 0;
			}
		}
	}
	
	KLIPS_PRINT(debug_xform,
		    "klips_debug:deltdb: "
		    "no entries in linked list for hash=%d of SA:%s.\n",
		    hashval,
		    sa_len ? sa : " (error)");
	return -ENOENT;
}

/*
  The tdb table better be locked before it is handed in, or races might happen
*/
int
ipsec_sa_delchain(struct ipsec_sa *ips)
{
	struct ipsec_sa *tdbdel;
	int error = 0;
        char sa[SATOA_BUF];
	size_t sa_len;

	if(!ips) {
		KLIPS_PRINT(debug_xform,
			    "klips_error:deltdbchain: "
			    "null pointer passed in!\n");
		return -ENODATA;
	}

	sa_len = satoa(ips->ips_said, 0, sa, SATOA_BUF);
	KLIPS_PRINT(debug_xform,
		    "klips_debug:deltdbchain: "
		    "passed SA:%s\n",
		    sa_len ? sa : " (error)");
	while(ips->ips_onext) {
		ips = ips->ips_onext;
	}

	while(ips) {
		/* XXX send a pfkey message up to advise of deleted TDB */
		sa_len = satoa(ips->ips_said, 0, sa, SATOA_BUF);
		KLIPS_PRINT(debug_xform,
			    "klips_debug:deltdbchain: "
			    "unlinking and delting SA:%s",
			    sa_len ? sa : " (error)");
		tdbdel = ips;
		ips = ips->ips_inext;
		if(ips) {
			sa_len = satoa(ips->ips_said, 0, sa, SATOA_BUF);
			KLIPS_PRINT(debug_xform,
				    ", inext=%s",
				    sa_len ? sa : " (error)");
			tdbdel->ips_inext = NULL;
			ips->ips_onext = NULL;
		}
		KLIPS_PRINT(debug_xform,
			    ".\n");
		if((error = ipsec_sa_del(tdbdel))) {
			KLIPS_PRINT(debug_xform,
				    "klips_debug:deltdbchain: "
				    "deltdb returned error %d.\n", -error);
			return error;
		}
		if((error = ipsec_sa_wipe(tdbdel))) {
			KLIPS_PRINT(debug_xform,
				    "klips_debug:deltdbchain: "
				    "ipsec_tdbwipe returned error %d.\n", -error);
			return error;
		}
	}
	return error;
}

int 
ipsec_sadb_cleanup(__u8 proto)
{
	int i;
	int error = 0;
	struct ipsec_sa *ips, **ipsprev, *tdbdel;
        char sa[SATOA_BUF];
	size_t sa_len;

	KLIPS_PRINT(debug_xform,
		    "klips_debug:ipsec_tdbcleanup: "
		    "cleaning up proto=%d.\n",
		    proto);

	spin_lock_bh(&tdb_lock);

	for (i = 0; i < SADB_HASHMOD; i++) {
		ipsprev = &(ipsec_sadb_hash[i]);
		ips = ipsec_sadb_hash[i];
		for(; ips;) {
			sa_len = satoa(ips->ips_said, 0, sa, SATOA_BUF);
			KLIPS_PRINT(debug_xform,
				    "klips_debug:ipsec_tdbcleanup: "
				    "checking SA:%s, hash=%d",
				    sa_len ? sa : " (error)",
				    i);
			tdbdel = ips;
			ips = tdbdel->ips_hnext;
			if(ips) {
				sa_len = satoa(ips->ips_said, 0, sa, SATOA_BUF);
				KLIPS_PRINT(debug_xform,
					    ", hnext=%s",
					    sa_len ? sa : " (error)");
			}
			if(*ipsprev) {
				sa_len = satoa((*ipsprev)->ips_said, 0, sa, SATOA_BUF);
				KLIPS_PRINT(debug_xform,
					    ", *ipsprev=%s",
					    sa_len ? sa : " (error)");
				if((*ipsprev)->ips_hnext) {
					sa_len = satoa((*ipsprev)->ips_hnext->ips_said, 0, sa, SATOA_BUF);
					KLIPS_PRINT(debug_xform,
						    ", *ipsprev->ips_hnext=%s",
						    sa_len ? sa : " (error)");
				}
			}
			KLIPS_PRINT(debug_xform,
				    ".\n");
			if(!proto || (proto == tdbdel->ips_said.proto)) {
				sa_len = satoa(tdbdel->ips_said, 0, sa, SATOA_BUF);
				KLIPS_PRINT(debug_xform,
					    "klips_debug:ipsec_tdbcleanup: "
					    "deleting SA chain:%s.\n",
					    sa_len ? sa : " (error)");
				if((error = ipsec_sa_delchain(tdbdel))) {
					SENDERR(-error);
				}
				ipsprev = &(ipsec_sadb_hash[i]);
				ips = ipsec_sadb_hash[i];

				KLIPS_PRINT(debug_xform,
					    "klips_debug:ipsec_tdbcleanup: "
					    "deleted SA chain:%s",
					    sa_len ? sa : " (error)");
				if(ips) {
					sa_len = satoa(ips->ips_said, 0, sa, SATOA_BUF);
					KLIPS_PRINT(debug_xform,
						    ", tdbh[%d]=%s",
						    i,
						    sa_len ? sa : " (error)");
				}
				if(*ipsprev) {
					sa_len = satoa((*ipsprev)->ips_said, 0, sa, SATOA_BUF);
					KLIPS_PRINT(debug_xform,
						    ", *ipsprev=%s",
						    sa_len ? sa : " (error)");
					if((*ipsprev)->ips_hnext) {
						sa_len = satoa((*ipsprev)->ips_hnext->ips_said, 0, sa, SATOA_BUF);
						KLIPS_PRINT(debug_xform,
							    ", *ipsprev->ips_hnext=%s",
							    sa_len ? sa : " (error)");
					}
				}
				KLIPS_PRINT(debug_xform,
					    ".\n");
			} else {
				ipsprev = &tdbdel;
			}
		}
	}
 errlab:

	spin_unlock_bh(&tdb_lock);

	return(error);
}

int
ipsec_sa_wipe(struct ipsec_sa *ips)
{
	if(!ips) {
		return -ENODATA;
	}

	if(ips->ips_addr_s) {
		memset((caddr_t)(ips->ips_addr_s), 0, ips->ips_addr_s_size);
		kfree(ips->ips_addr_s);
	}
	ips->ips_addr_s = NULL;

	if(ips->ips_addr_d) {
		memset((caddr_t)(ips->ips_addr_d), 0, ips->ips_addr_d_size);
		kfree(ips->ips_addr_d);
	}
	ips->ips_addr_d = NULL;

	if(ips->ips_addr_p) {
		memset((caddr_t)(ips->ips_addr_p), 0, ips->ips_addr_p_size);
		kfree(ips->ips_addr_p);
	}
	ips->ips_addr_p = NULL;

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
	if(ips->ips_natt_oa) {
		memset((caddr_t)(ips->ips_natt_oa), 0, ips->ips_natt_oa_size);
		kfree(ips->ips_natt_oa);
	}
	ips->ips_natt_oa = NULL;
#endif

	if(ips->ips_key_a) {
#ifdef CONFIG_IPSEC_ALG
		if (ips->ips_alg_auth && ips->ips_alg_auth->ixt_a_hmac_destroy_key) {
			ips->ips_alg_auth->ixt_a_hmac_destroy_key(ips->ips_alg_auth, ips->ips_key_a);
		} else {
#endif
			memset((caddr_t)(ips->ips_key_a), 0, ips->ips_key_a_size);
			kfree(ips->ips_key_a);
#ifdef CONFIG_IPSEC_ALG
		}
#endif
	}
	ips->ips_key_a = NULL;

	if(ips->ips_key_e) {
#ifdef CONFIG_IPSEC_ALG
		if (ips->ips_alg_enc&&ips->ips_alg_enc->ixt_e_destroy_key) {
			ips->ips_alg_enc->ixt_e_destroy_key(ips->ips_alg_enc, 
					ips->ips_key_e);
		} else {
#endif /* CONFIG_IPSEC_ALG */
		memset((caddr_t)(ips->ips_key_e), 0, ips->ips_key_e_size);
		kfree(ips->ips_key_e);
#ifdef CONFIG_IPSEC_ALG
		}
#endif /* CONFIG_IPSEC_ALG */
	}
	ips->ips_key_e = NULL;

	if(ips->ips_iv) {
		memset((caddr_t)(ips->ips_iv), 0, ips->ips_iv_size);
		kfree(ips->ips_iv);
	}
	ips->ips_iv = NULL;

	if(ips->ips_ident_s.data) {
		memset((caddr_t)(ips->ips_ident_s.data),
                       0,
		       ips->ips_ident_s.len * IPSEC_PFKEYv2_ALIGN - sizeof(struct sadb_ident));
		kfree(ips->ips_ident_s.data);
        }
	ips->ips_ident_s.data = NULL;
	
	if(ips->ips_ident_d.data) {
		memset((caddr_t)(ips->ips_ident_d.data),
                       0,
		       ips->ips_ident_d.len * IPSEC_PFKEYv2_ALIGN - sizeof(struct sadb_ident));
		kfree(ips->ips_ident_d.data);
        }
	ips->ips_ident_d.data = NULL;

#ifdef CONFIG_IPSEC_ALG
	if (ips->ips_alg_enc||ips->ips_alg_auth) {
		ipsec_alg_sa_wipe(ips);
	}
#endif /* CONFIG_IPSEC_ALG */
	
	memset((caddr_t)ips, 0, sizeof(*ips));
	kfree(ips);
	ips = NULL;

	return 0;
}

