/*
 * ipsec_alg NULL cipher stubs
 *
 * Author: JuanJo Ciarlante <jjo-ipsec@mendoza.gov.ar>
 * 
 * $Id: ipsec_alg_null.c,v 1.2 2004/01/18 16:36:59 sergey Exp $
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
 * Fixes by:
 * 	DDR:	David De Reu <DeReu@tComLabs.com> 
 * Fixes:
 * 	DDR:	comply to RFC2410 and make it interop with other impl.
 */
#include <linux/config.h>
#include <linux/version.h>

/*	
 *	special case: ipsec core modular with this static algo inside:
 *	must avoid MODULE magic for this file
 */
#if CONFIG_IPSEC_MODULE && CONFIG_IPSEC_ALG_NULL
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
#include <ipsec_log.h>

#define ESP_NULL		11	/* from ipsec drafts */
#define ESP_NULL_BLK_LEN	1	/* from RFC 2410 */
#define ESP_NULL_IV_LEN		0	/* from RFC 2410 */

MODULE_AUTHOR("JuanJo Ciarlante <jjo-ipsec@mendoza.gov.ar>");
static int debug=0;
MODULE_PARM(debug, "i");
static int test=0;
MODULE_PARM(test, "i");
static int excl=0;
MODULE_PARM(excl, "i");

typedef int null_context;

struct null_eks{
	null_context null_ctx;
};
static int _null_set_key(struct ipsec_alg_enc *alg, __u8 * key_e, const __u8 * key, size_t keysize) {
	null_context *ctx=&((struct null_eks*)key_e)->null_ctx;
	if (debug > 0)
		ipsec_log(KERN_DEBUG "klips_debug:_null_set_key:"
				"key_e=%p key=%p keysize=%d\n",
				key_e, key, keysize);
	*ctx = 1;
	return 0;
}
static int _null_cbc_encrypt(struct ipsec_alg_enc *alg, __u8 * key_e, __u8 * in, int ilen, const __u8 * iv, int encrypt) {
	null_context *ctx=&((struct null_eks*)key_e)->null_ctx;
	if (debug > 0)
		ipsec_log(KERN_DEBUG "klips_debug:_null_cbc_encrypt:"
				"key_e=%p in=%p ilen=%d iv=%p encrypt=%d\n",
				key_e, in, ilen, iv, encrypt);
	(*ctx)++;
	return ilen;
}
static struct ipsec_alg_enc ipsec_alg_NULL = {
	ixt_version:	IPSEC_ALG_VERSION,
	ixt_module:	THIS_MODULE,
	ixt_refcnt:	ATOMIC_INIT(0),
	ixt_alg_type:	IPSEC_ALG_TYPE_ENCRYPT,
	ixt_alg_id: 	ESP_NULL,
	ixt_name: 	"null",
	ixt_blocksize:	ESP_NULL_BLK_LEN, 
	ixt_ivlen:	ESP_NULL_IV_LEN,
	ixt_keyminbits:	0,
	ixt_keymaxbits:	0,
	ixt_e_keylen:	0,
	ixt_e_ctx_size:	sizeof(null_context),
	ixt_e_set_key:	_null_set_key,
	ixt_e_cbc_encrypt:_null_cbc_encrypt,
};
	
IPSEC_ALG_MODULE_INIT(ipsec_null_init)
{
	int ret, test_ret;
	if (excl) ipsec_alg_NULL.ixt_state |= IPSEC_ALG_ST_EXCL;
	ret=register_ipsec_alg_enc(&ipsec_alg_NULL);
	ipsec_log("ipsec_null_init(alg_type=%d alg_id=%d name=%s): ret=%d\n", 
			ipsec_alg_NULL.ixt_alg_type, 
			ipsec_alg_NULL.ixt_alg_id, 
			ipsec_alg_NULL.ixt_name, 
			ret);
	if (ret==0 && test) {
		test_ret=ipsec_alg_test(
				ipsec_alg_NULL.ixt_alg_type,
				ipsec_alg_NULL.ixt_alg_id, 
				test);
		ipsec_log("ipsec_null_init(alg_type=%d alg_id=%d): test_ret=%d\n", 
				ipsec_alg_NULL.ixt_alg_type, 
				ipsec_alg_NULL.ixt_alg_id, 
				test_ret);
	}
	return ret;
}
IPSEC_ALG_MODULE_EXIT(ipsec_null_fini)
{
	unregister_ipsec_alg_enc(&ipsec_alg_NULL);
	return;
}
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

EXPORT_NO_SYMBOLS;
