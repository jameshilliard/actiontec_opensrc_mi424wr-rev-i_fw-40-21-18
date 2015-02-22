/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/freeswan/klips/net/ipsec/alg/ipsec_alg_md5_cadence.c
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

/* Check if __exit is defined, if not null it */
#ifndef __exit
#define __exit
#endif

#include "ipsec_alg.h"
#include "cadence_ipsec2_itf.h"

#define AH_MD5		2
#define MD5_BLOCKSIZE	64

static struct ipsec_alg_auth ipsec_alg_MD5_cadence = {
    ixt_version:	IPSEC_ALG_VERSION,
    ixt_module:		THIS_MODULE,
    ixt_refcnt:		ATOMIC_INIT(0),
    ixt_alg_type:	IPSEC_ALG_TYPE_AUTH,
    ixt_alg_id: 	AH_MD5,
    ixt_name:		"Cadence_md5",
    ixt_blocksize:	MD5_BLOCKSIZE,
    ixt_keyminbits:	128,
    ixt_keymaxbits:	128,
    ixt_a_keylen:	128/8,
    ixt_a_ctx_size:	sizeof(ipsec2_request),
    ixt_a_hmac_new_key:	_Cadence_hmac_new_key,
    ixt_a_hmac_destroy_key:	_Cadence_hmac_destroy_key,
    ixt_a_hmac_hash:	_Cadence_hmac_hash,
};

IPSEC_ALG_MODULE_INIT( ipsec_md5_init )
{
    printk(KERN_INFO "Cadence-ipsec2 driver : register MD5 hash algorithm\n");
    return register_ipsec_alg_auth(&ipsec_alg_MD5_cadence);
}

IPSEC_ALG_MODULE_EXIT( ipsec_md5_fini )
{
    unregister_ipsec_alg_auth(&ipsec_alg_MD5_cadence);
}

