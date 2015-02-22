/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/freeswan/klips/net/ipsec/alg/ipsec_alg_1des_mpc.c
 *
 *  Developed by Jungo LTD.
 *  Residential Gateway Software Division
 *  www.jungo.com
 *  info@jungo.com
 *
 *  This file is part of the OpenRG Software and may not be distributed,
 *  sold, reproduced or copied in any way.
 *
 *  This copyright notice should not be removed
 *
 */

#include <linux/config.h>
#include <linux/version.h>

/*	
 * special case: ipsec core modular with this static algo inside:
 * must avoid MODULE magic for this file
 */
#if CONFIG_IPSEC_MODULE && CONFIG_IPSEC_ALG_1DES
#undef MODULE
#endif

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/string.h>
#include <ipsec_log.h>

/* Check if __exit is defined, if not null it */
#ifndef __exit
#define __exit
#endif

#include "ipsec_alg.h"

extern int _des_mpc_set_key(struct ipsec_alg_enc *alg,__u8 *key_e,
    const __u8 * key, size_t keysize);
extern int _1des_mpc_cbc_encrypt(struct ipsec_alg_enc *alg, __u8 * key_e,
    __u8 * in, int ilen, const __u8 * iv, int encrypt);

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

static struct ipsec_alg_enc ipsec_alg_1DES_mpc = {
    ixt_version:	IPSEC_ALG_VERSION,
    ixt_module:		THIS_MODULE,
    ixt_refcnt:		ATOMIC_INIT(0),
    ixt_name:		"1des_mpc",
    ixt_alg_type:	IPSEC_ALG_TYPE_ENCRYPT,
    ixt_alg_id: 	ESP_DES,
    ixt_blocksize:	ESP_DES_CBC_BLKLEN,
    ixt_keyminbits:	ESP_DES_KEY_SZ*7,
    ixt_keymaxbits:	ESP_DES_KEY_SZ*7,
    ixt_e_keylen:	ESP_DES_KEY_SZ,
    ixt_e_ctx_size:	sizeof(__u8)*ESP_DES_KEY_SZ,
    ixt_e_set_key:	_des_mpc_set_key,
    ixt_e_cbc_encrypt:	_1des_mpc_cbc_encrypt,
};
	
IPSEC_ALG_MODULE_INIT(ipsec_1des_init)
{
    int ret, test_ret;

    if (esp_id)
	ipsec_alg_1DES_mpc.ixt_alg_id = esp_id;
    if (excl)
	ipsec_alg_1DES_mpc.ixt_state |= IPSEC_ALG_ST_EXCL;
    ret = register_ipsec_alg_enc(&ipsec_alg_1DES_mpc);
    if (debug)
    {
	ipsec_log("ipsec_1des_init(alg_type=%d alg_id=%d name=%s): ret=%d\n",
	    ipsec_alg_1DES_mpc.ixt_alg_type,
	    ipsec_alg_1DES_mpc.ixt_alg_id,
	    ipsec_alg_1DES_mpc.ixt_name,
	    ret);
    }
    if (ret==0 && test)
    {
	test_ret = ipsec_alg_test(
	    ipsec_alg_1DES_mpc.ixt_alg_type,
	    ipsec_alg_1DES_mpc.ixt_alg_id, 
	    test);
	ipsec_log("ipsec_1des_init(alg_type=%d alg_id=%d): test_ret=%d\n", 
	    ipsec_alg_1DES_mpc.ixt_alg_type, 
	    ipsec_alg_1DES_mpc.ixt_alg_id, 
	    test_ret);
    }
    return ret;
}

IPSEC_ALG_MODULE_EXIT(ipsec_1des_fini)
{
    unregister_ipsec_alg_enc(&ipsec_alg_1DES_mpc);
    return;
}

