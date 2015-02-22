/*
 * IPSEC_GLUE interface code.
 * Copyright 2002 Intel Corporation All Rights Reserved.
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
 */

#ifndef _IPSEC_GLUE_H
#define _IPSEC_GLUE_H

#include "ipsec_sa.h"

#include <IxTypes.h>
#include <IxCryptoAcc.h> 

/*
 * Constants value for glue code
 */
#define STATUS_SUCCESS			0
#define STATUS_FAIL			1
#define STATUS_NOT_SUPPORTED		2
#define IPSEC_DES_BLOCK_LENGTH		8
#define BITS				8

/*
 * Initialize the freeswan security association and crypto context map table
 * 
 * Param: None 
 */
void ipsec_glue_sa_crypto_context_map_init(void);

/*
 * Initialize the crypto context
 * 
 * Param: None 
 */
void ipsec_glue_crypto_ctx_init(void);

/* 
 * Create crypto cipher context from the the freeswan security association 
 *
 * Param: ips [in] Pointer to ipsec security association. 
 *
 * Return: 
 *	STATUS_SUCCESS - The parameters to create crypto context are valid
 *	STATUS_FAIL	- Some of parameters to create crypto cihper context are invalid
 */
UINT32 ipsec_glue_encapalgo(struct ipsec_sa *ips);

/* 
 * Create crypto authentication context from the the freeswan security association  
 *
 * Param: ips [in] Pointer to ipsec security association. 
 *
 * Return: 
 *	STATUS_SUCCESS - The parameters to create authentication context are valid
 *	STATUS_FAIL	- Some of parameters to create crypto authentican 
 *			context are invalid
 */
UINT32 ipsec_glue_authalg(struct ipsec_sa *ips);

/*
 * Create crypto context from freeswan security association 
 *
 * Param: ips [in] Pointer to ipsec security association. 
 *
 * Return: 
 *	STATUS_SUCCESS - Successfully to create crypto context from ipsec 
 *			security association
 *	STATUS_FAIL	- Failed to create crypto context from ipsec 
 *			security association
 *	STATUS_NOT_SUPPORTED	- IPSEC protocol not supported (e.g.  IPPROTO_COMP)
 *
 */
UINT32 ipsec_compose_context(struct ipsec_sa *ips);

/*
 * Register crypto context with IXP4XX hardware accelarator
  * Param: ips [in] Pointer to ipsec security association. 
 *
 * Return: 
 *	STATUS_SUCCESS - Successfully to register crypto context to hardware accelarator
 *	STATUS_FAIL	- Failed to register crypto context to hardware accelarator
 *
 */
UINT32 ipsec_glue_crypto_context_put(struct ipsec_sa *ips);

/*
 * Unregister crypto context with hardware accelarator
 *
 * Param: cryptoCtxId [in] crypto context id. The id is given when registration to 
 * hardware accelarator 
 *
 * Return: 
 *	STATUS_SUCCESS - Ssuccessfully unregister the crypto context Id.
 *	STATUS_FAIL	-  Unregistration failed for some internal reasons 
 *			(detail refer to IxCryptoAcc.h)
 *	
 */
UINT32 ipsec_glue_crypto_context_del (UINT32 cryptoCtxId);

/*
 * Get the security assication from the mapping table 
 *
 * Param: cryptoCtxId [in] crypto context id. The id is given when registration to 
 * 	hardware accelarator 
 *
 *	ips [out] ipsec security association
 *
 * Return: 
 *	NONE
 *
 *   The tdb table better *NOT* be locked before it is handed in, 
 *   or semaphore locks will happen
 */
struct ipsec_sa * 
ipsec_glue_sa_get (UINT32 cryptoCtxId);

/*
 * Crypto context register callback function
 *
 * Refer to IxCryptoAcc.h for detail.
 *
 *   The tdb table better *NOT* be locked before it is handed in, 
 *   or semaphore locks will happen
 */
void register_crypto_cb(UINT32 cryptoCtxId, IX_MBUF *empty_mbuf, IxCryptoAccStatus state);

#endif /* _IPSEC_GLUE_H */

