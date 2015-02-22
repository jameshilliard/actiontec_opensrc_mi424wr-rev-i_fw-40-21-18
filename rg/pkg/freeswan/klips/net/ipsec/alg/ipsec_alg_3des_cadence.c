/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/freeswan/klips/net/ipsec/alg/ipsec_alg_3des_cadence.c
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

#include "ipsec_alg.h"
#include "cadence_ipsec2_itf.h"

#define ESP_3DES            3
#define ESP_3DES_CBC_BLKLEN 8  /* 64 bits blocks */
#define ESP_3DES_KEY_SZ     24 /* 192 bits keylen */

static struct ipsec_alg_enc ipsec_alg_3DES_cadence = {
    ixt_version:	IPSEC_ALG_VERSION,
    ixt_module:		THIS_MODULE,
    ixt_refcnt:		ATOMIC_INIT(0),
    ixt_name:		"Cadence_3des",
    ixt_alg_type:	IPSEC_ALG_TYPE_ENCRYPT,
    ixt_alg_id: 	ESP_3DES,
    ixt_blocksize:	ESP_3DES_CBC_BLKLEN,
    ixt_keyminbits:	ESP_3DES_KEY_SZ*7,
    ixt_keymaxbits:	ESP_3DES_KEY_SZ*7,
    ixt_e_keylen:	ESP_3DES_KEY_SZ,
    ixt_e_ctx_size:	sizeof(ipsec2_request),
    ixt_e_cbc_encrypt:  _Cadence_cipher_cbc_encrypt,
    ixt_e_new_key:	_Cadence_cipher_new_key,
    ixt_e_destroy_key:	_Cadence_cipher_destroy_key
};

IPSEC_ALG_MODULE_INIT( ipsec_3des_init )
{
    printk(KERN_INFO "Cadence-ipsec2 driver : register 3DES cipher algorithm\n");
    return register_ipsec_alg_enc(&ipsec_alg_3DES_cadence);
}

IPSEC_ALG_MODULE_EXIT( ipsec_3des_fini )
{
    unregister_ipsec_alg_enc(&ipsec_alg_3DES_cadence);
}

