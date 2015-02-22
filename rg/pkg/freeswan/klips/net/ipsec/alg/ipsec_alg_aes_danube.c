/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/freeswan/klips/net/ipsec/alg/ipsec_alg_aes_danube.c
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
#if CONFIG_IPSEC_MODULE && CONFIG_IPSEC_ALG_AES
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

static int debug=0;
MODULE_PARM(debug, "i");
static int test=0;
MODULE_PARM(test, "i");
static int excl=0;
MODULE_PARM(excl, "i");
static int keyminbits=0;
MODULE_PARM(keyminbits, "i");
static int keymaxbits=0;
MODULE_PARM(keymaxbits, "i");

#define ESP_AES			12	/* truely _constant_  :)  */

/* 128, 192 or 256 */
#define ESP_AES_KEY_SZ_MIN	16 	/* 128 bit secret key */
#define ESP_AES_KEY_SZ_MAX	32 	/* 256 bit secret key */
#define ESP_AES_CBC_BLKLEN	16	/* AES-CBC block size */

/* from danube driver */
struct aes_ctx {
	int key_length;
	u32 E[60];
	u32 D[60];
};

#ifdef DANUBE_CRYPTO_DEBUG
#define DEBUG_PRINT(fmt, p...) \
    ipsec_log("crypto debug(aes): %s:" fmt "\n", __FUNCTION__, ##p)
#else
#define DEBUG_PRINT(fmt, p...)
#endif

/* danube driver functions */
extern int danube_aes_set_key(void *ctx, const u8 *key,
    unsigned int keylen, u32 *flags);
extern void aes_ifxdeu_cbc(void *ctx, uint8_t *dst, const uint8_t *src,
    uint8_t *iv, size_t nbytes, int encdec, int inplace);

static int _aes_danube_set_key(struct ipsec_alg_enc *alg, __u8 *key_e,
    const __u8 *key, size_t keysize)
{
    u32 dummy_flags = 0;
    struct aes_ctx *ctx = (struct aes_ctx *)key_e;

    DEBUG_PRINT("aes_danube_set_key(alg=%p, key_e=%p, key=%p keysize=%d)\n",
	alg, key_e, key, keysize);

    return danube_aes_set_key(ctx, key, keysize, &dummy_flags);
}

static int _aes_danube_cbc_encrypt(struct ipsec_alg_enc *alg, __u8 *key_e,
    __u8 *in, int ilen, const __u8 *iv, int encrypt)
{
    struct aes_cnx *ctx = (struct aes_ctx *)key_e;

    DEBUG_PRINT("aes_danube_cbc_encrypt(alg=%p, key_e=%p, in=%p ilen=%d "
	"iv=%p encrypt=%d)\n", alg, key_e, in, ilen, iv, encrypt);

    aes_ifxdeu_cbc(ctx, in, in, iv, ilen, encrypt, 1);
    return ilen;
}

static struct ipsec_alg_enc ipsec_alg_AES = {
    ixt_version:	IPSEC_ALG_VERSION,
    ixt_module:		THIS_MODULE,
    ixt_refcnt:		ATOMIC_INIT(0),
    ixt_name:		"aes_danube",
    ixt_alg_type:	IPSEC_ALG_TYPE_ENCRYPT,
    ixt_alg_id: 	ESP_AES,
    ixt_blocksize:	ESP_AES_CBC_BLKLEN,
    ixt_keyminbits:	ESP_AES_KEY_SZ_MIN*8,
    ixt_keymaxbits:	ESP_AES_KEY_SZ_MAX*8,
    ixt_e_keylen:	ESP_AES_KEY_SZ_MAX,
    ixt_e_ctx_size:	sizeof(struct aes_ctx),
    ixt_e_set_key:	_aes_danube_set_key,
    ixt_e_cbc_encrypt:	_aes_danube_cbc_encrypt,
};
	
IPSEC_ALG_MODULE_INIT(ipsec_aes_init)
{
    int ret, test_ret;

    if (keyminbits)
	ipsec_alg_AES.ixt_keyminbits = keyminbits;
    if (keymaxbits)
    {
	ipsec_alg_AES.ixt_keymaxbits = keymaxbits;
	if (keymaxbits*8 > ipsec_alg_AES.ixt_keymaxbits)
	    ipsec_alg_AES.ixt_e_keylen = keymaxbits*8;
    }
    if (excl)
	ipsec_alg_AES.ixt_state |= IPSEC_ALG_ST_EXCL;
    ret = register_ipsec_alg_enc(&ipsec_alg_AES);
    if (debug)
    {
	ipsec_log("ipsec_aes_init(alg_type=%d alg_id=%d name=%s): ret=%d\n",
	    ipsec_alg_AES.ixt_alg_type,
	    ipsec_alg_AES.ixt_alg_id,
	    ipsec_alg_AES.ixt_name,
	    ret);
    }
    if (ret==0 && test)
    {
	test_ret=ipsec_alg_test(
	    ipsec_alg_AES.ixt_alg_type,
	    ipsec_alg_AES.ixt_alg_id,
	    test);
	ipsec_log("ipsec_aes_init(alg_type=%d alg_id=%d): test_ret=%d\n",
	    ipsec_alg_AES.ixt_alg_type,
	    ipsec_alg_AES.ixt_alg_id,
	    test_ret);
    }
    return ret;
}

IPSEC_ALG_MODULE_EXIT(ipsec_aes_fini)
{
    unregister_ipsec_alg_enc(&ipsec_alg_AES);
    return;
}

