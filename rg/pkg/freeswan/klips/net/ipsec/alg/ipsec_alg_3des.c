/*
 * ipsec_alg 3DES cipher
 *
 * Author: JuanJo Ciarlante <jjo-ipsec@mendoza.gov.ar>
 *
 * $Id: ipsec_alg_3des.c,v 1.2 2004/01/18 16:36:59 sergey Exp $
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
#if CONFIG_IPSEC_MODULE && CONFIG_IPSEC_ALG_3DES
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

#include <linux/socket.h>
#include <linux/in.h>
#include "ipsec_param.h"
#include "ipsec_sa.h"
#include "ipsec_alg.h"
#include "../libdes/des.h"
#include <ipsec_log.h>

MODULE_AUTHOR("JuanJo Ciarlante <jjo-ipsec@mendoza.gov.ar>");
static int debug=0;
MODULE_PARM(debug, "i");
static int test=0;
MODULE_PARM(test, "i");
static int excl=0;
MODULE_PARM(excl, "i");
static int esp_id=0;
MODULE_PARM(esp_id, "i");

#define ESP_3DES		3

#define ESP_3DES_CBC_BLKLEN	8	/* 64 bit blocks */
#define ESP_DES_KEY_SZ		8
#define ESP_3DES_KEY_SZ		8*3 	/* 3DES */

struct des3_eks{
	des_key_schedule ctx[3];
};
static int _3des_set_key(struct ipsec_alg_enc *alg,__u8 *key_e, const __u8 * key, size_t keysize) {
	des_key_schedule *ctx=((struct des3_eks*)key_e)->ctx;
	int i, error;
	if (debug > 0)
		ipsec_log(KERN_DEBUG "klips_debug: _3des_set_key: "
				"key_e=%p key=%p keysize=%d\n",
				key_e, key, keysize);
	for(i = 0; i < 3; i++) {
		des_set_odd_parity((des_cblock *)(key+ESP_DES_KEY_SZ * i));
		error = des_set_key((des_cblock *)(key+ESP_DES_KEY_SZ * i),
				    ctx[i]);
		if (debug > 0)
			ipsec_log(KERN_DEBUG "klips_debug:des_set_key:"
				"ctx[%d]=%p, error=%d \n",
				i, ctx[i], error);
		if (error == -1)
			ipsec_log("klips_debug: _3des_set_key: "
					"parity error in des key %d/3\n",
					i + 1);
		else if (error == -2)
			ipsec_log("klips_debug: _3des_set_key: "
					"illegal weak des key %d/3\n", i + 1);
		if (error)
			return error;
	}
	return 0;
}
void des_ede3_cbc_encrypt(des_cblock *input, des_cblock *output,
		long length, des_key_schedule ks1, des_key_schedule ks2,
		des_key_schedule ks3, des_cblock *ivec, int enc);

static int _3des_cbc_encrypt(struct ipsec_alg_enc *alg, __u8 * key_e, __u8 * in, int ilen, const __u8 * iv, int encrypt) {
	char iv_buf[ESP_3DES_CBC_BLKLEN];
	des_key_schedule *ctx=((struct des3_eks*)key_e)->ctx;
	*((__u32*)&(iv_buf)) = ((__u32*)(iv))[0];
	*((__u32*)&(iv_buf)+1) = ((__u32*)(iv))[1];
	if (debug > 1) {
		ipsec_log(KERN_DEBUG "klips_debug:_3des_cbc_encrypt:"
				"key_e=%p in=%p ilen=%d iv=%p encrypt=%d\n",
				ctx, in, ilen, iv, encrypt);
	}
	des_ede3_cbc_encrypt((des_cblock*) in, (des_cblock*) in, ilen, ctx[0], ctx[1], ctx[2], (des_cblock *)iv_buf, encrypt);
	return ilen;
}
static struct ipsec_alg_enc ipsec_alg_3DES = {
	ixt_version:	IPSEC_ALG_VERSION,
	ixt_module:	THIS_MODULE,
	ixt_refcnt:	ATOMIC_INIT(0),
	ixt_name: 	"3des",
	ixt_alg_type:	IPSEC_ALG_TYPE_ENCRYPT,
	ixt_alg_id: 	ESP_3DES,
	ixt_blocksize:	ESP_3DES_CBC_BLKLEN,
	ixt_keyminbits:	ESP_3DES_KEY_SZ*7, /*  7bits key+1bit parity  */
	ixt_keymaxbits:	ESP_3DES_KEY_SZ*7, /*  7bits key+1bit parity  */
	ixt_e_keylen:	ESP_3DES_KEY_SZ,
	ixt_e_ctx_size:	sizeof(struct des3_eks),
	ixt_e_set_key:	_3des_set_key,
	ixt_e_cbc_encrypt:_3des_cbc_encrypt,
};
	
IPSEC_ALG_MODULE_INIT(ipsec_3des_init)
{
	int ret, test_ret;
	if (esp_id)
		ipsec_alg_3DES.ixt_alg_id=esp_id;
	if (excl) ipsec_alg_3DES.ixt_state |= IPSEC_ALG_ST_EXCL;
	ret=register_ipsec_alg_enc(&ipsec_alg_3DES);
	ipsec_log("ipsec_3des_init(alg_type=%d alg_id=%d name=%s): ret=%d\n", 
			ipsec_alg_3DES.ixt_alg_type, 
			ipsec_alg_3DES.ixt_alg_id, 
			ipsec_alg_3DES.ixt_name, 
			ret);
	if (ret==0 && test) {
		test_ret=ipsec_alg_test(
				ipsec_alg_3DES.ixt_alg_type,
				ipsec_alg_3DES.ixt_alg_id, 
				test);
		ipsec_log("ipsec_3des_init(alg_type=%d alg_id=%d): test_ret=%d\n", 
				ipsec_alg_3DES.ixt_alg_type, 
				ipsec_alg_3DES.ixt_alg_id, 
				test_ret);
	}
	return ret;
}
IPSEC_ALG_MODULE_EXIT(ipsec_3des_fini)
{
	unregister_ipsec_alg_enc(&ipsec_alg_3DES);
	return;
}
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
