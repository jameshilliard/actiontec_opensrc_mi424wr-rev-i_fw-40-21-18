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

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */

#include "ipsec_param.h"

#ifdef MALLOC_SLAB
# include <linux/slab.h> /* kmalloc() */
#else /* MALLOC_SLAB */
# include <linux/malloc.h> /* kmalloc() */
#endif /* MALLOC_SLAB */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/interrupt.h> /* mark_bh */

#include <linux/netdevice.h>   /* struct net_device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/in.h>          /* struct sockaddr_in */
#include <linux/skbuff.h>
#include <freeswan.h>

#ifdef NET_21
# include <asm/uaccess.h>
# include <linux/in6.h>
#endif /* NET_21 */

#include <asm/checksum.h>
#include <net/ip.h>

#include "ipsec_glue_mbuf.h" 	/* The interface to glue mbuf 				*/
#include "ipsec_glue.h"		/* The interface to glue sa 				*/
#include "ipsec_hwaccel.h"      /* The interface to actual hw accel                     */

#include <freeswan.h>
#include "ipsec_netlink.h"
#include "ipsec_xform.h"	/* The interface to ipsec transform		 	*/
#include "ipsec_ah.h"		/* The interface to ipsec authentication 		*/
#include "ipsec_esp.h"		/* The interface to ipsec esp		 		*/
#include <pfkeyv2.h>
#include <pfkey.h>
#include "ipsec_log.h"

#define INVALID_CRYPTO_STATE -1

struct ipsec_sa_record
{
    struct ipsec_sa *sa;
    int callbackState;
};

extern spinlock_t tdb_lock;

extern int debug_xform;
	
/* Perform the encrytion for hardware accelaration funtion */
extern void ipsec_tunnel_start_xmit_cb( UINT32, IX_MBUF *, IX_MBUF *, IxCryptoAccStatus);

/* Perform the dencrytion for hardware accelaration funtion */
extern void ipsec_rcv_cb( UINT32, IX_MBUF *, IX_MBUF *, IxCryptoAccStatus);

/* Callback funtion for crypto context registration */
IxCryptoAccPerformCompleteCallback RegCallbk = NULL; 

/* Forward declaration of the show funtion */
#ifdef SA_GLUE_DEBUG
  void print_show_algo(void);
#endif /* SA_GLUE_DEBUG */

/* Table to store that mapping between SA and Crypto context 			*/
struct ipsec_sa_record sa_crypto_context_map[IX_CRYPTO_ACC_MAX_ACTIVE_SA_TUNNELS];

/* Crypto context 	*/
IxCryptoAccCtx cryptoAccCtx;

/* Mbufs for registration */
IX_MBUF *pMbufPrimaryChainVar = NULL;    
IX_MBUF *pMbufSecondaryChainVar = NULL;

void ipsec_glue_sa_crypto_context_map_init(void)
{
    int i;
	
    for (i=0; i<IX_CRYPTO_ACC_MAX_ACTIVE_SA_TUNNELS; i++)
    {
	sa_crypto_context_map[i].sa = NULL;
	sa_crypto_context_map[i].callbackState = INVALID_CRYPTO_STATE;
    }
}

void ipsec_glue_crypto_ctx_init(void)
{
    cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_TYPE_OF_OPERATION;
    cryptoAccCtx.cipherCtx.cipherAlgo = IX_CRYPTO_ACC_CIPHER_NULL;
    cryptoAccCtx.cipherCtx.cipherMode = IX_CRYPTO_ACC_MODE_NULL ;
    cryptoAccCtx.cipherCtx.cipherKeyLen = 0;
    cryptoAccCtx.cipherCtx.key.desKey[0] = 0; 
    cryptoAccCtx.cipherCtx.cipherBlockLen = 0;
    cryptoAccCtx.cipherCtx.cipherInitialVectorLen = 0;
    cryptoAccCtx.authCtx.authAlgo =IX_CRYPTO_ACC_AUTH_NULL;
    cryptoAccCtx.authCtx.authDigestLen = 0;
    cryptoAccCtx.authCtx.authKeyLen = 0;
    cryptoAccCtx.authCtx.key.authKey[0] =  0;
    cryptoAccCtx.useDifferentSrcAndDestMbufs = FALSE;
}

void ipsec_glue_update_state(struct ipsec_sa *ips,
			     IxCryptoAccStatus state)
{
    /* If ips's state 'mature' then register callback already updates it. */
    if (ips->ips_state == SADB_SASTATE_MATURE)
	return;
    if (state == IX_SUCCESS) 
    {
	KLIPS_PRINT(debug_xform,
		    "klips_glue:update_state: "
		    "Changing State to Mature.!");
	/* update tdb to MATURE state */
	spin_lock_bh(&tdb_lock);
	ips->ips_state = SADB_SASTATE_MATURE;	
	spin_unlock_bh(&tdb_lock);

	/*  sa unlock */
    }
    else if (state == IX_FAIL)
    {
	KLIPS_PRINT(debug_xform,
		    "klips_glue:update_state: "
		    "Changing State to Dead.!");
	/* update tdb to DEAD state */
	spin_lock_bh(&tdb_lock);
	ips->ips_state = SADB_SASTATE_DEAD;	
	spin_unlock_bh(&tdb_lock);
    }
    else if (state ==IX_CRYPTO_ACC_STATUS_WAIT)
    {
	KLIPS_PRINT(debug_xform,
		    "klips_glue:update_state: "
		    "Registration not complete yet; wait for next completion indication.!");
	/* update tdb to LARVA state */
	spin_lock_bh(&tdb_lock);
	ips->ips_state = SADB_SASTATE_LARVAL;	
	spin_unlock_bh(&tdb_lock);
    }
    else
    {
	KLIPS_PRINT(debug_xform,
		    "klips_glue:update_state: "
		    "Error in status message.!");
	/* update tdb to DEAD state */
	spin_lock_bh(&tdb_lock);
	ips->ips_state = SADB_SASTATE_DEAD;	
	spin_unlock_bh(&tdb_lock);
    }	
}
 
UINT32 ipsec_glue_encapalgo(struct ipsec_sa *ips)
{
    UINT32 status = STATUS_SUCCESS;

    switch(ips->ips_encalg)
    {
#ifdef CONFIG_IPSEC_ENC_AES
	case ESP_AES:
	    cryptoAccCtx.cipherCtx.cipherAlgo = IX_CRYPTO_ACC_CIPHER_AES;

	    switch (DIVUP(ips->ips_key_bits_e, BITS))
	    {
	    case EMT_ESPAES128_KEY_SZ:
		cryptoAccCtx.cipherCtx.cipherKeyLen = IX_CRYPTO_ACC_AES_KEY_128;
		memcpy(cryptoAccCtx.cipherCtx.key.aesKey128,
		    (UINT8 *)(ips->ips_key_e), IX_CRYPTO_ACC_AES_KEY_128);
		break;
	    case EMT_ESPAES192_KEY_SZ:
		cryptoAccCtx.cipherCtx.cipherKeyLen = IX_CRYPTO_ACC_AES_KEY_192;
		memcpy(cryptoAccCtx.cipherCtx.key.aesKey192,
		    (UINT8 *)(ips->ips_key_e), IX_CRYPTO_ACC_AES_KEY_192);
		break;
	    case EMT_ESPAES256_KEY_SZ:
		cryptoAccCtx.cipherCtx.cipherKeyLen = IX_CRYPTO_ACC_AES_KEY_256;
		memcpy(cryptoAccCtx.cipherCtx.key.aesKey256,
		    (UINT8 *)(ips->ips_key_e), IX_CRYPTO_ACC_AES_KEY_256);
		break;
	    default:
		status = STATUS_FAIL;
		KLIPS_PRINT(debug_xform, "klips_error:glue_encapalgo: "
		    "Invalid AES length!\n");
	    }

	    cryptoAccCtx.cipherCtx.cipherBlockLen = IX_CRYPTO_ACC_AES_BLOCK_128;
	    cryptoAccCtx.cipherCtx.cipherMode = IX_CRYPTO_ACC_MODE_CBC;

	    if (EMT_ESPAES_IV_SZ == (DIVUP(ips->ips_iv_bits, BITS))) 
	    {
		cryptoAccCtx.cipherCtx.cipherInitialVectorLen =
		    IX_CRYPTO_ACC_AES_CBC_IV_128;
	    }
	    else
	    {
		status = STATUS_FAIL;
		KLIPS_PRINT(debug_xform, "klips_error:glue_encapalgo: "
		    "Invalid IV length!\n");
	    }

	    break;
#endif
#ifdef CONFIG_IPSEC_ENC_3DES
	case ESP_3DES:
	    /* The cipher algorith, 3DES */
	    cryptoAccCtx.cipherCtx.cipherAlgo = IX_CRYPTO_ACC_CIPHER_3DES;
		
	    /* The cipher key length	 		*/
	    /* check the cipher length, 3DES = 24 bytes	*/
	    if (EMT_ESP3DES_KEY_SZ == (DIVUP(ips->ips_key_bits_e, BITS))) 
	    {
		cryptoAccCtx.cipherCtx.cipherKeyLen = IX_CRYPTO_ACC_3DES_KEY_192;
	    }
	    else
	    {
		status = STATUS_FAIL;
		KLIPS_PRINT(debug_xform,
			    "klips_error:glue_encapalgo: "
			    "Invalid 3DES length!\n");
		break;
	    }
		
	    /* The cipher key  */
	    memcpy (cryptoAccCtx.cipherCtx.key.desKey, (UINT8 *)(ips->ips_key_e), 
		    IX_CRYPTO_ACC_3DES_KEY_192); 

	    /* The cipher block length */
	    cryptoAccCtx.cipherCtx.cipherBlockLen = IPSEC_DES_BLOCK_LENGTH;
		
	    /* The cipher mode, supported cipher mode: CBC	*/
	    cryptoAccCtx.cipherCtx.cipherMode = IX_CRYPTO_ACC_MODE_CBC;
 	
	    /* The cipher IV length */
	    if (EMT_ESPDES_IV_SZ == (DIVUP(ips->ips_iv_bits, BITS))) 
	    {
		cryptoAccCtx.cipherCtx.cipherInitialVectorLen = IX_CRYPTO_ACC_DES_IV_64;
	    }
	    else
	    {
		status = STATUS_FAIL;
		KLIPS_PRINT(debug_xform,
			   "klips_error:glue_encapalgo: "
			   "Invalid IV length!\n");
	    }
	
	    break;
#endif /* CONFIG_IPSEC_ENC_3DES */

#ifdef USE_SINGLE_DES
	case ESP_DES:
	    /* The cipher algorith, DES */
	    cryptoAccCtx.cipherCtx.cipherAlgo = IX_CRYPTO_ACC_CIPHER_DES;
		
	    /* The cipher key length, DES = 8 bytes */
	    if (EMT_ESPDES_KEY_SZ == (DIVUP(ips->ips_key_bits_e, BITS))) 
	    {
		cryptoAccCtx.cipherCtx.cipherKeyLen = IX_CRYPTO_ACC_DES_KEY_64;
	    }
	    else
	    {
		status = STATUS_FAIL;
		KLIPS_PRINT(debug_xform,
			    "klips_error:glue_encapalgo: "
			    "Invalid DES length!\n");
		break;
	    }
		
	    /* The cipher key  */
	    memcpy (cryptoAccCtx.cipherCtx.key.desKey, (UINT8 *)(ips->ips_key_e), 
		    IX_CRYPTO_ACC_DES_KEY_64);
		
	    /* The cipher block length */
	    cryptoAccCtx.cipherCtx.cipherBlockLen = IPSEC_DES_BLOCK_LENGTH;
		
	    /* The cipher mode, supported cipher mode: CBC	*/
	    cryptoAccCtx.cipherCtx.cipherMode = IX_CRYPTO_ACC_MODE_CBC;
 	
	    /* The cipher IV length */
	    if (EMT_ESPDES_IV_SZ == (DIVUP(ips->ips_iv_bits, BITS))) 
	    {
		cryptoAccCtx.cipherCtx.cipherInitialVectorLen = IX_CRYPTO_ACC_DES_IV_64;
	    }
	    else
	    {
		status = STATUS_FAIL;
		KLIPS_PRINT(debug_xform,
			   "klips_error:glue_encapalgo: "
			   "Invalid IV length!\n");
	    }
	
	    break;
#endif /* USE_SINGLE_DES */

	case ESP_NULL:
	    break;

	default:
	    /* Encryption not supported */
	    status = STATUS_FAIL;
	    KLIPS_PRINT(debug_xform,
			"klips_error:glue_encapalgo: "
			"Encap. Algorithm not supported!\n");
	    return status;
    }
	
    return status;
}

 
UINT32 ipsec_glue_authalg(struct ipsec_sa *ips)
{
    UINT32 status = STATUS_SUCCESS;

    switch(ips->ips_authalg) {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
	case AH_MD5:
	    /* Tne the authentication algorithm - MD5*/
	    cryptoAccCtx.authCtx.authAlgo = IX_CRYPTO_ACC_AUTH_MD5;
			
	    /* The digest length, in bytes */
	    cryptoAccCtx.authCtx.authDigestLen = AHHMAC_HASHLEN;
		
	    /* The authentication key length */
	    if (AHMD596_KLEN == (DIVUP(ips->ips_key_bits_a, BITS))) 
	    {
		cryptoAccCtx.authCtx.authKeyLen = IX_CRYPTO_ACC_MD5_KEY_128;
	    }
	    else
	    {
		status = STATUS_FAIL;
		KLIPS_PRINT(debug_xform,
			    "klips_error:glue_encapalgo: "
			    "Invalid MD5 length!\n");
		break;
	    }
	
	    /* The authentication key */
	    memcpy(cryptoAccCtx.authCtx.key.authKey, (UINT8 *)(ips->ips_key_a), 
		    IX_CRYPTO_ACC_MD5_KEY_128);
	    break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_MD5 */

#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
	case AH_SHA:
	    cryptoAccCtx.authCtx.authAlgo = IX_CRYPTO_ACC_AUTH_SHA1;
		
	    /* The digest length, in bytes */
	    cryptoAccCtx.authCtx.authDigestLen = AHHMAC_HASHLEN;
		
	    /* The authentication key length */
	    if (AHSHA196_KLEN == (DIVUP(ips->ips_key_bits_a, BITS))) 
	    {
		cryptoAccCtx.authCtx.authKeyLen = IX_CRYPTO_ACC_SHA1_KEY_160;
	    }
	    else
	    {
		status = STATUS_FAIL;
		KLIPS_PRINT(debug_xform,
			    "klips_error:glue_encapalgo: "
			    "Invalid SHA1 length!\n");
		break;
	    }
		
	    /* The authentication key, SHA1 */
	    memcpy(cryptoAccCtx.authCtx.key.authKey, (UINT8 *)(ips->ips_key_a), 
		    IX_CRYPTO_ACC_SHA1_KEY_160);
		
	    break;
#endif /* CONFIG_IPSEC_AUTH_HMAC_SHA1 */

	case AH_NONE:
	    break;
	
	default:
	    /* Authentication algo. not supported */
	    status = STATUS_FAIL;
	    KLIPS_PRINT(debug_xform,
			"klips_error:glue_authalgo: "
			"Authen. Algorithm not supported!\n");
	    return status;
    }
    return status;
}


UINT32 ipsec_compose_context(struct ipsec_sa *ips)
{
    UINT32 status = STATUS_SUCCESS;

    /* 
       Temporary structure to store the crypto context. Hardware 
       accelarator will copy the data into its own structure 
    */   
    ipsec_glue_crypto_ctx_init();	
	
    switch(ips->ips_said.proto) 
    {
	case IPPROTO_AH:
	    /* fill only in cryto authentication context */
	    if (STATUS_FAIL == (status = ipsec_glue_authalg(ips)))
	    {
		KLIPS_PRINT(debug_xform,
			    "klips_error:glue_compose_context: "
			    "Encapsulatio Algo error!\n");
		return status;
	    }
	    /* Determine the direction of the transformation */
	    if (ips->ips_flags & EMT_INBOUND)
	    {	/* Incoming direction */
		cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_AUTH_CHECK;
		RegCallbk = &ipsec_rcv_cb;
	    }
	    else
	    {	/* Outgoing direction */
		cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_AUTH_CALC;
		RegCallbk = &ipsec_tunnel_start_xmit_cb;
	    }
	    break;
		
	case IPPROTO_ESP:
	    if (STATUS_FAIL == (status = ipsec_glue_encapalgo(ips)))
	    {
		KLIPS_PRINT(debug_xform,
			    "klips_error:glue_compose_context: "
			    "Encapsulatio Algo error!\n");
		return status;
	    }
			
	    /* fill only in cryto authentication context */
	    if (STATUS_FAIL == (status = ipsec_glue_authalg(ips)))
	    {
		KLIPS_PRINT(debug_xform,
			    "klips_error:glue_compose_context: "
			    "Encapsulatio Algo error!\n");
		return status;
	    }
		
	    /* Determine the direction of the transformation */
	    if (ips->ips_flags & EMT_INBOUND)
	    {	/* Incoming direction */
		if (AH_NONE == ips->ips_authalg)
		{
		    if(ESP_NULL == ips->ips_encalg)
		    {
			KLIPS_PRINT(debug_xform,
				    "klips_error:glue_compose_context: "
				    "Per RFC2406, cannot have NULL Auth and Cipher in ESP!\n");
			return STATUS_FAIL;
		    }
		    else
		    {
			cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_DECRYPT;
		    }
		}
		else
		{
		    if(ESP_NULL == ips->ips_encalg)
		    {
			cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_AUTH_CHECK;
		    }
		    else
		    {
			cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_AUTH_DECRYPT;
		    }
		}
		RegCallbk = &ipsec_rcv_cb;
	    }
	    else
	    {	/* Outgoing direction */
		if (AH_NONE == ips->ips_authalg)
		{
		    if(ESP_NULL == ips->ips_encalg)
		    {
			KLIPS_PRINT(debug_xform,
				    "klips_error:glue_compose_context: "
				    "Per RFC2406, cannot have NULL Auth and Cipher in ESP!\n");
			return STATUS_FAIL;
		    }
		    else
		    {
			cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_ENCRYPT;
		    }
		}
		else
		{
		    if(ESP_NULL == ips->ips_encalg)
		    {
			cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_AUTH_CALC;
		    }
		    else
		    {
			cryptoAccCtx.operation = IX_CRYPTO_ACC_OP_ENCRYPT_AUTH;
		    }
		}
		RegCallbk = &ipsec_tunnel_start_xmit_cb;
	    }	
	    break;
		
	case IPPROTO_IPIP:
	    status = STATUS_NOT_SUPPORTED;
	    break;
		
#ifdef CONFIG_IPSEC_IPCOMP
	case IPPROTO_COMP:
	    status = STATUS_NOT_SUPPORTED;
	    break;
#endif /* CONFIG_IPSEC_IPCOMP */

	case IPPROTO_INT:
	    status = STATUS_NOT_SUPPORTED;
	    break;
	
	case 0:
	    status = STATUS_NOT_SUPPORTED;
	    break;
		
	default:
	    KLIPS_PRINT(debug_xform,
			"klips_error:compose_context: "
			"unknown proto=%d.\n",
			ips->ips_said.proto);
	    status = STATUS_FAIL;
	    break;
    }
	
    /* The data is read and write to the source */
    cryptoAccCtx.useDifferentSrcAndDestMbufs = FALSE;

#ifdef SA_GLUE_DEBUG
    ipsec_log("Context compose status:  %d\n", status);
    print_show_algo();	
#endif /* SA_GLUE_DEBUG */
	
    return status;
}


UINT32 
ipsec_glue_crypto_context_put(struct ipsec_sa *ips)
{
    UINT32 cryptoCtxId;
    UINT32 status = STATUS_SUCCESS;
    UINT32 ret_status;
    IxCryptoAccStatus reg_status;
    IxCryptoAccStatus cb_status;

    pMbufPrimaryChainVar = NULL;
    pMbufSecondaryChainVar = NULL;
    
    /* Refuse to register if no undelying HW Acceleration */
    if(ipsec_hwaccel_ready() == FALSE)
    {
	KLIPS_PRINT(debug_xform,
		    "klips_error:context_put: "
		    "Cannot register SAs until HW Accel present.\n");
	return (STATUS_FAIL);
    }

    /* Contruct the crypto context	*/
    ret_status = ipsec_compose_context(ips);

    if (STATUS_FAIL == ret_status)
    {
	KLIPS_PRINT(debug_xform,
		    "klips_error:context_put: "
		    "Composed crypto context failed \n");
	return (STATUS_FAIL);
    }
    else
	if (STATUS_NOT_SUPPORTED == ret_status)
	{
	    KLIPS_PRINT(debug_xform,
			"klips_debug:context_put: "
			"Composed crypto context not supported \n");

	    return status;
	}
	
    /*  allocate Mbuf for crypto registration */
    /* ESP with out authentication */
    if ((IX_CRYPTO_ACC_OP_ENCRYPT == cryptoAccCtx.operation) ||
	(IX_CRYPTO_ACC_OP_DECRYPT == cryptoAccCtx.operation))
    {
	/* Authentication not selected (SHA1/MD5 not selected) - 
	   Secondary Mbuf must be NULL */
	pMbufSecondaryChainVar = NULL;
    }
    else /* ESP with authentication or AH */
    {
	if (STATUS_FAIL == ipsec_glue_mbuf_get (&pMbufPrimaryChainVar))
	{
	    KLIPS_PRINT(debug_xform,
			"klips_error:context_put: "
			"Unable to allocate MBUF.\n");
	    return (STATUS_FAIL);
	}
	    
	if (STATUS_FAIL == ipsec_glue_mbuf_get (&pMbufSecondaryChainVar))
	{
	    if (pMbufPrimaryChainVar)
		ipsec_glue_mbuf_rel(pMbufPrimaryChainVar);
	    KLIPS_PRINT(debug_xform,
			"klips_error:context_put: "
			"Unable to allocate MBUF.\n");
	    return (STATUS_FAIL);
	}
    }
    
    if (RegCallbk == NULL)
    {
	KLIPS_PRINT(debug_xform,
		"klips_error:context_put: "
		"RegCallbk is NULL.\n");
	return STATUS_FAIL;
    }

    /*  The tdb table better *NOT* be locked before it is handed in, 
	or SMP locks will happen */
    spin_lock_bh(&tdb_lock);
    ips->ips_state = SADB_SASTATE_LARVAL;
    spin_unlock_bh(&tdb_lock);
		
    /* Register crypto context	*/
    reg_status = ipsec_hwaccel_register (&cryptoAccCtx, 
					 pMbufPrimaryChainVar, 
					 pMbufSecondaryChainVar, 
					 register_crypto_cb, 
					 RegCallbk, 
					 &cryptoCtxId);
	
    if (IX_CRYPTO_ACC_STATUS_SUCCESS == reg_status)
    {
	sa_crypto_context_map[cryptoCtxId].sa = ips;
	
	spin_lock_bh(&tdb_lock);
	ips->ips_crypto_context_id = cryptoCtxId;
	spin_unlock_bh(&tdb_lock);

	cb_status = sa_crypto_context_map[cryptoCtxId].callbackState;

	if(cb_status != INVALID_CRYPTO_STATE)
	{
	    /* Callback has already been called. Handle state */
	    ipsec_glue_update_state(ips,cb_status);
	}
    }
    else
    {
	sa_crypto_context_map[cryptoCtxId].callbackState = INVALID_CRYPTO_STATE;
	spin_lock_bh(&tdb_lock);
	ips->ips_state = SADB_SASTATE_DEAD;
	spin_unlock_bh(&tdb_lock);

	if (pMbufPrimaryChainVar)
            ipsec_glue_mbuf_rel (pMbufPrimaryChainVar);
	if (pMbufSecondaryChainVar)
    	    ipsec_glue_mbuf_rel (pMbufSecondaryChainVar);

	if (IX_CRYPTO_ACC_STATUS_FAIL == reg_status) 
	{
	    KLIPS_PRINT(debug_xform,
			"klips_error:glue_crypto_context_put: "
			"Registration failed for some unspecified internal reasons!\n");
	}
	else
	{
	    KLIPS_PRINT(debug_xform,
			"klips_error:glue_crypto_context_put: "
			"Registration failed - Invalid parameters!\n");
	}
		
	status = STATUS_FAIL;
    }
    return status;
}

void register_crypto_cb(UINT32 cryptoCtxId, IX_MBUF *empty_mbuf, IxCryptoAccStatus state)
{
    if (empty_mbuf != NULL)
    {
	/* free the mbuf */
	ipsec_glue_mbuf_rel (empty_mbuf);
    }

    /* prints the returned pointer to cryptoCtxId*/
    KLIPS_PRINT(debug_xform,
		"klips_glue:crypto_cb: "
		"cryptoCtxId is %d\n",
		cryptoCtxId); 

    if (sa_crypto_context_map[cryptoCtxId].sa == NULL)
    {
	/* 
	 * We must not have needed to send message to
	 * NPE and hence called callback before returning
	 * from Register(). Record state for caller of
	 * Register() to use 
	 */
	sa_crypto_context_map[cryptoCtxId].callbackState = state;
    }
    else
    {
	/* Otherwise we have ips so drive state now */
	ipsec_glue_update_state(sa_crypto_context_map[cryptoCtxId].sa,
				state);
    }
}


UINT32 
ipsec_glue_crypto_context_del (UINT32 cryptoCtxId) 
{
    UINT32 status = STATUS_SUCCESS;
    IxCryptoAccStatus unregister_status;
	
    unregister_status = ipsec_hwaccel_unregister (cryptoCtxId);
	
    if (IX_CRYPTO_ACC_STATUS_SUCCESS == unregister_status)
    {
	sa_crypto_context_map[cryptoCtxId].sa = NULL;
	sa_crypto_context_map[cryptoCtxId].callbackState 
	    = INVALID_CRYPTO_STATE;
    }
    else
    { 
	if (IX_CRYPTO_ACC_STATUS_FAIL == unregister_status)
	{
	    KLIPS_PRINT(debug_xform,
			"klips_error:glue_crtypto_context_del: "
			"Cannot unregister crypto context!");
	    status = STATUS_FAIL;
	}
	else
	    if (IX_CRYPTO_ACC_STATUS_CRYPTO_CTX_NOT_VALID == unregister_status)	
	    {	KLIPS_PRINT(debug_xform,
			    "klips_error:glue_crtypto_context_del: "
			    "invalid cryptoCtxId.!\n");
	    }
	    else
	    {
		KLIPS_PRINT(debug_xform,
			    "klips_error:glue_crtypto_context_del: "
			    "iretry the unregister operation.!");
	    }	
		
	status = STATUS_FAIL;
    }

    return status;
}

struct ipsec_sa *
ipsec_glue_sa_get (UINT32 cryptoCtxId)
{
	
    if (cryptoCtxId < IX_CRYPTO_ACC_MAX_ACTIVE_SA_TUNNELS)
    {
	return (sa_crypto_context_map[cryptoCtxId].sa); 
    }
    else
    {
	return (NULL);
    }
}

#ifdef SA_GLUE_DEBUG
void print_show_algo()
{
    if (!ipsec_rate_limit())
	return;

    printk("Cipher Operation : %d\n", cryptoAccCtx.operation);
    printk("Cipher Algo: %d\n", cryptoAccCtx.cipherCtx.cipherAlgo);
    printk("Cipher Mode: %d\n", cryptoAccCtx.cipherCtx.cipherMode);
    printk("Cipher Key Length: %d\n", cryptoAccCtx.cipherCtx.cipherKeyLen);
    printk("Cipher key : 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.cipherCtx.key.desKey)) + 0))); 
    printk("Cipher key : 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.cipherCtx.key.desKey)) + 1))); 
    printk("Cipher key : 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.cipherCtx.key.desKey)) + 2))); 
    printk("Cipher key : 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.cipherCtx.key.desKey)) + 3))); 
    printk("Cipher key : 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.cipherCtx.key.desKey)) + 4))); 
    printk("Cipher key : 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.cipherCtx.key.desKey)) + 5))); 
    printk("Cipher Block Len: %d\n", cryptoAccCtx.cipherCtx.cipherBlockLen);
    printk("Cipher IV Length: %d\n", cryptoAccCtx.cipherCtx.cipherInitialVectorLen);

    printk("Auth Algo: %d\n", cryptoAccCtx.authCtx.authAlgo);
    printk("Auth Digetst Len: %d\n", cryptoAccCtx.authCtx.authDigestLen);
    printk("Auth key Len: %d\n", cryptoAccCtx.authCtx.authKeyLen);
    printk("Auth Key: 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.authCtx.key.authKey)) + 0)));
    printk("Auth Key: 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.authCtx.key.authKey)) + 1)));
    printk("Auth Key: 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.authCtx.key.authKey)) + 2)));
    printk("Auth Key: 0x%x\n", (*(((UINT32 *)(cryptoAccCtx.authCtx.key.authKey)) + 3)));
};	
#endif /* SA_GLUE_DEBUG */ 
