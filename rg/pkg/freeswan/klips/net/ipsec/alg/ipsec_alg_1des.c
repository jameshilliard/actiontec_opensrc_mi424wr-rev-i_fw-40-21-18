/*
 * ipsec_alg 1DES cipher 
 *
 * Author: JuanJo Ciarlante <jjo-ipsec@mendoza.gov.ar>
 *
 * $Id: ipsec_alg_1des.c,v 1.2 2004/01/18 16:36:59 sergey Exp $
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
#if CONFIG_IPSEC_MODULE && CONFIG_IPSEC_ALG_1DES
#undef MODULE
#endif

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h> /* printk() */
#include <ipsec_log.h>
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/string.h>

/* Check if __exit is defined, if not null it */
#ifndef __exit
#define __exit
#endif

#include <linux/socket.h>
#include <linux/in.h>
#include "ipsec_param.h"
#include "ipsec_sa.h"
#include "ipsec_alg.h"
#include "../libdes/des.h"

MODULE_AUTHOR("JuanJo Ciarlante <jjo-ipsec@mendoza.gov.ar>");
static int debug=0;
MODULE_PARM(debug, "i");
static int test=0;
MODULE_PARM(test, "i");
static int excl=0;
MODULE_PARM(excl, "i");
static int esp_id=0;
MODULE_PARM(esp_id, "i");

#define ESP_DES			2

#define ESP_DES_CBC_BLKLEN	8	/* 64 bit blocks */
#define ESP_DES_KEY_SZ		8	/* 56 bits keylen :P */

struct des1_eks{
	des_key_schedule ctx[1];
};
static int _1des_set_key(struct ipsec_alg_enc *alg,__u8 *key_e, const __u8 * key, size_t keysize) {
	des_key_schedule *ctx=((struct des1_eks*)key_e)->ctx;
	int error;
	if (debug > 0)
		ipsec_log(KERN_DEBUG "klips_debug: _1des_set_key: "
				"key_e=%p key=%p keysize=%d\n",
				key_e, key, keysize);
	des_set_odd_parity((des_cblock *)key);
	error = des_set_key((des_cblock *)key, ctx[0]);
	if (debug > 0)
		ipsec_log(KERN_DEBUG "klips_debug:des_set_key:"
			"ctx[%d]=%p, error=%d \n",
			0, ctx[0], error);
	if (error == -1)
		ipsec_log("klips_debug: _1des_set_key: "
				"parity error in 1des key\n");
	else if (error == -2)
		ipsec_log("klips_debug: _1des_set_key: "
				"illegal weak 1des key \n");
	if (error)
		return error;
	return 0;
}
void des_cbc_encrypt(des_cblock *input, des_cblock *output,
		long length, des_key_schedule ks, 
		des_cblock *ivec, int enc);

static int _1des_cbc_encrypt(struct ipsec_alg_enc *alg, __u8 * key_e, __u8 * in, int ilen, const __u8 * iv, int encrypt) {
	char iv_buf[ESP_DES_CBC_BLKLEN];
	des_key_schedule *ctx=((struct des1_eks*)key_e)->ctx;
	*((__u32*)&(iv_buf)) = ((__u32*)(iv))[0];
	*((__u32*)&(iv_buf)+1) = ((__u32*)(iv))[1];
	if (debug > 1) {
		ipsec_log(KERN_DEBUG "klips_debug:_1des_cbc_encrypt:"
				"key_e=%p in=%p ilen=%d iv=%p encrypt=%d\n",
				ctx, in, ilen, iv, encrypt);
	}
	des_cbc_encrypt((des_cblock*) in, (des_cblock*) in, ilen, ctx[0], (des_cblock *)iv_buf, encrypt);
	return ilen;
}
static struct ipsec_alg_enc ipsec_alg_1DES = {
	ixt_version:	IPSEC_ALG_VERSION,
	ixt_module:	THIS_MODULE,
	ixt_refcnt:	ATOMIC_INIT(0),
	ixt_name: 	"1des",
	ixt_alg_type:	IPSEC_ALG_TYPE_ENCRYPT,
	ixt_alg_id: 	ESP_DES,
	ixt_blocksize:	ESP_DES_CBC_BLKLEN,
	ixt_keyminbits:	ESP_DES_KEY_SZ*7, /*  7bits key+1bit parity  */
	ixt_keymaxbits:	ESP_DES_KEY_SZ*7, /*  7bits key+1bit parity  */
	ixt_e_keylen:	ESP_DES_KEY_SZ,
	ixt_e_ctx_size:	sizeof(struct des1_eks),
	ixt_e_set_key:	_1des_set_key,
	ixt_e_cbc_encrypt:_1des_cbc_encrypt,
};
	
IPSEC_ALG_MODULE_INIT(ipsec_1des_init)
{
	int ret, test_ret;
	if (esp_id)
		ipsec_alg_1DES.ixt_alg_id=esp_id;
	if (excl) ipsec_alg_1DES.ixt_state |= IPSEC_ALG_ST_EXCL;
	ret=register_ipsec_alg_enc(&ipsec_alg_1DES);
	ipsec_log("ipsec_1des_init(alg_type=%d alg_id=%d name=%s): ret=%d\n",
			ipsec_alg_1DES.ixt_alg_type, 
			ipsec_alg_1DES.ixt_alg_id, 
			ipsec_alg_1DES.ixt_name, 
			ret);
	if (ret==0 && test) {
		test_ret=ipsec_alg_test(
				ipsec_alg_1DES.ixt_alg_type,
				ipsec_alg_1DES.ixt_alg_id, 
				test);
		ipsec_log("ipsec_1des_init(alg_type=%d alg_id=%d): test_ret=%d\n", 
				ipsec_alg_1DES.ixt_alg_type, 
				ipsec_alg_1DES.ixt_alg_id, 
				test_ret);
	}
	return ret;
}
IPSEC_ALG_MODULE_EXIT(ipsec_1des_fini)
{
	unregister_ipsec_alg_enc(&ipsec_alg_1DES);
	return;
}
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
