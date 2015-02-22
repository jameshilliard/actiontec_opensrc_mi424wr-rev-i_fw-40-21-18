/************************************************************************
 * Includes and defines                                                 *
 ************************************************************************/

#include <linux/config.h>
#include <linux/version.h>

#if CONFIG_IPSEC_MODULE
#undef MODULE
#endif

#include <linux/module.h>
#include <linux/init.h>

#ifndef __exit
#define __exit
#endif

#include <linux/socket.h>
#include <linux/in.h>

#include <ipsec_log.h>
#include "ipsec_param.h"
#include "ipsec_sa.h"
#include "ipsec_alg.h"
#include "ipsec_xform.h"

#include "cadence_ipsec2_itf.h"

MODULE_AUTHOR("Fabien Marotte <fabien.marotte@mindspeed.com>");
 
#define ESP_DES_KEY_SZ				8            /* 64 bits keylen */
#define ESP_3DES_KEY_SZ				24           /* 192 bits keylen */
#define ESP_AES128_KEY_SZ			16           /* 128 bits keylen*/         
#define ESP_AES192_KEY_SZ			24           /* 192 bits keylen */
#define ESP_AES256_KEY_SZ			32           /* 256 bits keylen */

/************************************************************************
 * _Cadence_cipher_new_key                                                     *
 *                                                                      *
 ************************************************************************/
__u8* _Cadence_cipher_new_key(struct ipsec_alg_enc *alg, const __u8 *key, size_t keysize)
{
	ipsec2_request newRequest;
	int ret;
	int algo;
	int options=0;
	

	/*Make new request*/
	ret = ipsec2NewRequest(&newRequest);
	if (ret)
		goto err;
	
	/*Set mode*/
	ret = ipsec2SetMode(newRequest, IPSEC2_MODE_CIPHER);
	if (ret)
		goto err;
	
	/*Set Encryption Algorithm*/
	options = IPSEC2_MODE_CBC;
	switch(alg->ixt_alg_id)
	{
		case ESP_DES :
			algo = IPSEC2_DES;
			break;
		
		case ESP_3DES :
			algo = IPSEC2_3DES;
			if (keysize == ESP_3DES_KEY_SZ)
				options |= IPSEC2_MODE_KEY3;
			break;
		
		case ESP_AES :
			switch(keysize)
			{
				case ESP_AES128_KEY_SZ :
					algo = IPSEC2_AES128;
					break;
					
				case ESP_AES192_KEY_SZ :
					algo = IPSEC2_AES192;
					break;
					
				case ESP_AES256_KEY_SZ :
					algo = IPSEC2_AES256;
					break;
				
				default :
					goto err;
			}
			break;
			
		default :
			goto err;	
	}
	
	ret = ipsec2SetCipherEncryption(newRequest, algo, alg->ixt_blocksize);
	if (ret)
		goto err;

	/*Options*/
	ret = ipsec2SetCipherOptions(newRequest, options);
	if (ret)
		goto err;
	
	/*Set key*/
	ret = ipsec2SetCipherKey(newRequest, (char*) key, (int) keysize);
	if (ret)
		goto err;

	/*Return the request*/
	return (__u8*)newRequest;
	
err:
	return NULL;
}

/************************************************************************
 * _Cadence_cipher_destroy_key                                                 *
 *                                                                      *
 ************************************************************************/
void _Cadence_cipher_destroy_key(struct ipsec_alg_enc *alg, __u8 *key_e)
{
	ipsec2FreeRequest((ipsec2_request*) &key_e);
}


/************************************************************************
 *_Cadence_cipher_cbc_encrypt                                           *         
 *                                                                      *
 * Parameters:                                                          *
 *       - alg: A pointer to the ipsec_alg_enc structure that contains  *
 *                 information regarding algorithm properties           *
 *      - key_e: A pointer to the key context structure                 *
 *       - in: A pointer to the encrypt/decrypt buffer                  *
 *       - ilen: Buffer length                                          *
 *       - iv: IV value                                                 * 
 *       - encrypt: Select the action to be performed                   *
 *                 1 - Encryption                                       *
 *                 0 - Decryption                                       *                    
 ************************************************************************/
int _Cadence_cipher_cbc_encrypt(struct ipsec_alg_enc *alg, __u8 * key_e, __u8 * in, int ilen, const __u8 * iv, int encrypt)
{
	ipsec2_request Request;
	int ret;
	
	Request = (ipsec2_request) key_e;
	
	/*Set Encrypt or decrypt operation*/
	if (encrypt == 0)
	{
		/*Decrypt*/
		ipsec2SetOperation(Request, IPSEC2_MODE_DECRYPT);
	}
	else
	{
		/*Encrypt*/
		ipsec2SetOperation(Request, IPSEC2_MODE_ENCRYPT);
	}

	
	/*Set Initialisation Vector*/
	if(alg->ixt_alg_id == ESP_AES)
		ret = ipsec2SetCipherIV(Request, (char*)iv, 16);
	else
		ret = ipsec2SetCipherIV(Request, (char*)iv, 8);

	if (ret)
		goto err;
	
	/*Set source buffer*/
	ret = ipsec2SetSrcAddr(Request, in, ilen);
	if (ret)
		goto err;

	/*Set destination buffer*/
	ret = ipsec2SetDstAddr(Request, in, ilen);
	if (ret)
		goto err;

	/*Execute the request*/
	ret = ipsec2ExecuteRequest(Request);
	if (ret)
		goto err;
	
	/*Everything have been OK, we return encrypted data size that is the same than non encrypted data size*/
	ret = ilen;

err:
	return ret;
}


/************************************************************************
 * _Cadence_hmac_new_key                                                *
 *                                                                      *
 ***********************************************************************/
__u8* _Cadence_hmac_new_key(struct ipsec_alg_auth *alg, const __u8 * key, int keylen)
{
	ipsec2_request newRequest;
	int ret;
	int algo;
	__u8* tmp = NULL;

	/*Make new request*/
	ret = ipsec2NewRequest(&newRequest);
	if (ret)
		goto err;
	
	/*Set mode*/
	ret = ipsec2SetMode(newRequest, IPSEC2_MODE_HMAC);
	if (ret)
		goto err;

	/*Set Hmac Algorithm*/
	switch(alg->ixt_alg_id)
	{
		case AH_SHA :
			algo = IPSEC2_SHA1;
			break;

		case AH_MD5 :
			algo = IPSEC2_MD5;
			break;

		/*SHA256 and others are not yet supported by Jungo*/
		default :
			printk(KERN_ERR "_Cadence_hmac_new_key : Error, incorrect alg->ixt_alg_id");
			goto err;	
	}
	
	ret = ipsec2SetHmacEncryption(newRequest, algo);
	if (ret)
	{
		printk(KERN_ERR "_Cadence_hmac_new_key : Error, ipsec2SetHmacEncryption\n");
		goto err;
	}
	
	/*Set key*/
	ret = ipsec2SetHmacKey(newRequest, (char*) key, (int) keylen);
	if (ret)
		goto err;

	/*Return the request*/
	tmp = (__u8*)newRequest;
	
err:
	return tmp;
}

/************************************************************************
 * _Cadence_hmac_destroy_key                                            *
 *                                                                      *
 ***********************************************************************/
void _Cadence_hmac_destroy_key(struct ipsec_alg_auth *alg, __u8 *key_a)
{
	ipsec2FreeRequest((ipsec2_request*) &key_a);
}

/************************************************************************
 * _Cadence_hmac_hash                                                   *
 *                                                                      *
 ***********************************************************************/
int _Cadence_hmac_hash(struct ipsec_alg_auth *alg, __u8 * key_a, const __u8 * dat, int len, __u8 * hash, int hashlen)
{
	ipsec2_request Request;
	int ret=0;

	Request = (ipsec2_request) key_a;

	/*Set source buffer*/
	ret = ipsec2SetSrcAddr(Request, (__u8 *)dat, len);
	if (ret)
		goto err;

	/*Set destination buffer*/
	ret = ipsec2SetDstAddr(Request, hash, hashlen);
	if (ret)
		goto err;

	/*Execute the request*/
	ret = ipsec2ExecuteRequest(Request);

err:
	return ret;
}



/************************************************************************
 * IPSEC_ALG_MODULE_INIT                                                *
 ************************************************************************/
IPSEC_ALG_MODULE_INIT(ipsec_cadence_init)
{
	int ret = 0;

	/*Initialize the cadence_ipsec2 driver*/
	printk(KERN_INFO "Cadence_ipsec2 driver : hardware initialization\n");
	ipsec2DriverInit();

	return ret;
}

/************************************************************************
 * IPSEC_ALG_MODULE_EXIT                                                *
 ************************************************************************/
IPSEC_ALG_MODULE_EXIT(ipsec_cadence_exit)
{
	printk("Cadence_ipsec2 driver : exit\n");

	ipsec2DriverUnInit();
	return;
}

EXPORT_SYMBOL(_Cadence_cipher_new_key);
EXPORT_SYMBOL(_Cadence_cipher_destroy_key);
EXPORT_SYMBOL(_Cadence_cipher_cbc_encrypt);
EXPORT_SYMBOL(_Cadence_hmac_new_key);
EXPORT_SYMBOL(_Cadence_hmac_destroy_key);
EXPORT_SYMBOL(_Cadence_hmac_hash);

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

