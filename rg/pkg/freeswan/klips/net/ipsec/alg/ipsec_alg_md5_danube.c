/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/freeswan/klips/net/ipsec/alg/ipsec_alg_md5_danube.c
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
 *	special case: ipsec core modular with this static algo inside:
 *	must avoid MODULE magic for this file
 */
#if CONFIG_IPSEC_MODULE && CONFIG_IPSEC_ALG_MD5
#undef MODULE
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <string.h>

/* Check if __exit is defined, if not null it */
#ifndef __exit
#define __exit
#endif

#include "ipsec_alg.h"

#define AH_MD5					2
#define MD5_HMAC_DIGEST_SIZE			16	/* 128 bit digest */
#define MD5_HMAC_BLOCK_SIZE			64	/* 512 bit data block */

/* saving the key xored with ipad and opad */
typedef struct {
    u8 key_ipad[MD5_HMAC_BLOCK_SIZE];
    u8 key_opad[MD5_HMAC_BLOCK_SIZE];
} md5_hmac_danube_context_t;

/* from the driver */
struct md5_ctx {
    u32 hash[MD5_HMAC_DIGEST_SIZE / 4];
    u32 block[MD5_HMAC_BLOCK_SIZE / 4];
    u64 byte_count;
};

extern void danube_md5_init(void *ctx);
extern void danube_md5_update(void *ctx, const u8 *data, unsigned int len);
extern void danube_md5_final(void *ctx, u8 *out);

#ifdef DANUBE_AUTH_DEBUG
#define DEBUG_PRINT(fmt, p...) \
    ipsec_log("auth debug(md5): %s:" fmt "\n", __FUNCTION__, ##p)
#else
#define DEBUG_PRINT(fmt, p...)
#endif

static int _md5_hmac_danube_set_key(struct ipsec_alg_auth *alg, __u8 *key_a,
    const __u8 *key, int key_len)
{
    md5_hmac_danube_context_t *ctx = (md5_hmac_danube_context_t *)key_a;
    int i;

    DEBUG_PRINT("alg=%p, key_a=%p, key=%p, key_len=%d", alg, key_a, key,
	key_len);

    /* if the key is too big we use the digest of the key */
    if (key_len > MD5_HMAC_BLOCK_SIZE)
    {
	struct md5_ctx tmp;

	danube_md5_init(&tmp);
	danube_md5_update(&tmp, key, key_len);
	danube_md5_final(&tmp, key);

	key_len = MD5_HMAC_DIGEST_SIZE;
    }

    /* copy the key into padding vectors and pad with zeros */
    memcpy(ctx->key_ipad, key, key_len);
    memcpy(ctx->key_opad, key, key_len);
    memset(ctx->key_ipad + key_len, 0, sizeof(ctx->key_ipad) - key_len);
    memset(ctx->key_opad + key_len, 0, sizeof(ctx->key_opad) - key_len);

    /* now we XOR with padding vectors
     * ref. RFC2104 (key XOR ipad, key XOR opad) */
    for (i = 0; i < MD5_HMAC_BLOCK_SIZE; i++)
    {
	ctx->key_ipad[i] ^= 0x36;
	ctx->key_opad[i] ^= 0x5c;
    }

    return 0;
}

/* HMAC transform defined as: HASH((key XOR opad) HASH((key XOR ipad) text)) */
static int _md5_hmac_danube_hash(struct ipsec_alg_auth *alg, __u8 *key_a,
    const __u8 *data, int data_len, __u8 *hash, int hash_len) 
{
    md5_hmac_danube_context_t *ctx = (md5_hmac_danube_context_t *)key_a;
    struct md5_ctx tmp;
    u8 digest[MD5_HMAC_DIGEST_SIZE];
    int len;

    DEBUG_PRINT("alg=%p, key_a=%p, data=%p, data_len=%d, hash=%p, hash_len=%d",
	alg, key_a, data, data_len, hash, hash_len);

    /* HASH((key XOR ipad) text) */
    danube_md5_init(&tmp);
    danube_md5_update(&tmp, ctx->key_ipad, sizeof(ctx->key_ipad));
    danube_md5_update(&tmp, data, data_len);
    danube_md5_final(&tmp, digest);

    /* HASH((key XOR opad) HASH((key XOR ipad) text)) */
    danube_md5_init(&tmp);
    danube_md5_update(&tmp, ctx->key_opad, sizeof(ctx->key_opad));
    danube_md5_update(&tmp, digest, sizeof(digest));
    danube_md5_final(&tmp, digest);

    /* clip if needed */
    if (hash_len >= MD5_HMAC_DIGEST_SIZE)
	len = MD5_HMAC_DIGEST_SIZE;
    else
	len = hash_len;

    memcpy(hash, digest, len);

    return 0;
}

static struct ipsec_alg_auth ipsec_alg_MD5_danube = {
    ixt_version:	IPSEC_ALG_VERSION,
    ixt_module:		THIS_MODULE,
    ixt_refcnt:		ATOMIC_INIT(0),
    ixt_name:		"md5_danube",
    ixt_alg_type:	IPSEC_ALG_TYPE_AUTH,
    ixt_alg_id: 	AH_MD5,
    ixt_blocksize:	MD5_HMAC_BLOCK_SIZE,
    ixt_keyminbits:	128,
    ixt_keymaxbits:	128,
    ixt_a_keylen:	128/8,
    ixt_a_ctx_size:	sizeof(md5_hmac_danube_context_t),
    ixt_a_hmac_set_key:	_md5_hmac_danube_set_key,
    ixt_a_hmac_hash:	_md5_hmac_danube_hash,
};

IPSEC_ALG_MODULE_INIT(ipsec_md5_init)
{
    ipsec_log("Danube-ipsec driver : register MD5 hash algorithm\n");
    return register_ipsec_alg_auth(&ipsec_alg_MD5_danube);
}

IPSEC_ALG_MODULE_EXIT(ipsec_md5_fini)
{
    unregister_ipsec_alg_auth(&ipsec_alg_MD5_danube);
}

