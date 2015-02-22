/*
 * ipsec_alg SHA1 hash stubs
 *
 * Author: JuanJo Ciarlante <jjo-ipsec@mendoza.gov.ar>
 * 
 * $Id: ipsec_alg_sha1.c,v 1.2 2004/01/18 16:36:59 sergey Exp $
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

/*	
 *	special case: ipsec core modular with this static algo inside:
 *	must avoid MODULE magic for this file
 */
#if CONFIG_IPSEC_MODULE && CONFIG_IPSEC_ALG_SHA1
#undef MODULE
#endif

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h> /* printk() */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/string.h>

/* Check if __exit is defined, if not null it */
#ifndef __exit
#define __exit
#endif

/*	Low freeswan header coupling	*/
#include "ipsec_alg.h"
#include "libsha1/sha.h"
#include "libsha1/hmac_sha1.h"
#include <ipsec_log.h>

MODULE_AUTHOR("JuanJo Ciarlante <jjo-ipsec@mendoza.gov.ar>");
static int debug=0;
MODULE_PARM(debug, "i");
static int test=0;
MODULE_PARM(test, "i");
static int excl=0;
MODULE_PARM(excl, "i");

#define AH_SHA		3

static int _sha1_hmac_set_key(struct ipsec_alg_auth *alg, __u8 * key_a, const __u8 * key, int keylen) {
	sha1_hmac_context *hctx=(sha1_hmac_context*)(key_a);
	sha1_hmac_set_key(hctx, key, keylen);
	if (debug > 0)
		ipsec_log(KERN_DEBUG "klips_debug: _sha1_hmac_set_key(): "
				"key_a=%p key=%p keysize=%d\n",
				key_a, key, keylen);
	return 0;
}
static int _sha1_hmac_hash(struct ipsec_alg_auth *alg, __u8 * key_a, const __u8 * dat, int len, __u8 * hash, int hashlen) {
	sha1_hmac_context *hctx=(sha1_hmac_context*)(key_a);
	if (debug > 0)
		ipsec_log(KERN_DEBUG "klips_debug: _sha1_hmac_hash(): "
				"key_a=%p dat=%p len=%d hash=%p hashlen=%d\n",
				key_a, dat, len, hash, hashlen);
	sha1_hmac_hash(hctx, dat, len, hash, hashlen);
	return 0;
}
static struct ipsec_alg_auth ipsec_alg_SHA1 = {
	ixt_version:	IPSEC_ALG_VERSION,
	ixt_module:	THIS_MODULE,
	ixt_refcnt:	ATOMIC_INIT(0),
	ixt_alg_type:	IPSEC_ALG_TYPE_AUTH,
	ixt_alg_id: 	AH_SHA,
	ixt_name: 	"sha1",
	ixt_blocksize:	SHA1_BLOCKSIZE,
	ixt_keyminbits:	160,
	ixt_keymaxbits:	160,
	ixt_a_keylen:	160/8,
	ixt_a_ctx_size:	sizeof(sha1_hmac_context),
	ixt_a_hmac_set_key:	_sha1_hmac_set_key,
	ixt_a_hmac_hash:	_sha1_hmac_hash,
};
IPSEC_ALG_MODULE_INIT( ipsec_sha1_init )
{
	int ret, test_ret;
	if (excl) ipsec_alg_SHA1.ixt_state |= IPSEC_ALG_ST_EXCL;
	ret=register_ipsec_alg_auth(&ipsec_alg_SHA1);
	ipsec_log("ipsec_sha1_init(alg_type=%d alg_id=%d name=%s): ret=%d\n", 
			ipsec_alg_SHA1.ixt_alg_type, 
			ipsec_alg_SHA1.ixt_alg_id, 
			ipsec_alg_SHA1.ixt_name, 
			ret);
	if (ret==0 && test) {
		test_ret=ipsec_alg_test(
				ipsec_alg_SHA1.ixt_alg_type,
				ipsec_alg_SHA1.ixt_alg_id, 
				test);
		ipsec_log("ipsec_sha1_init(alg_type=%d alg_id=%d): test_ret=%d\n", 
				ipsec_alg_SHA1.ixt_alg_type, 
				ipsec_alg_SHA1.ixt_alg_id, 
				test_ret);
	}
	return ret;
}
IPSEC_ALG_MODULE_EXIT( ipsec_sha1_fini )
{
	unregister_ipsec_alg_auth(&ipsec_alg_SHA1);
	return;
}
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

EXPORT_NO_SYMBOLS;
