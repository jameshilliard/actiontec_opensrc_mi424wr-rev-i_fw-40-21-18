/************************************************************************
 * Cadence Ipsec2 library
 *
 * Written by Fabien Marotte <fabien.marotte@mindspeed.com>
 ************************************************************************/

#ifndef __CADENCE_IPSEC2_H__
#define __CADENCE_IPSEC2_H__

#include <asm/io.h>
#include <asm/semaphore.h>
#include <linux/timer.h>

/************************************************************************
* Defines
* 
*************************************************************************/

	/*
	* Cadence ipsec2 register
	*/
	
	#define IPSEC2_MODE			0x0000
	#define IPSEC2_STATUS		0x0004
	#define IPSEC2_SRCADDR		0x0008
	#define IPSEC2_DSTADDR		0x000C
	#define IPSEC2_LEN			0x0010
	#define IPSEC2_IV0			0x0014
	#define IPSEC2_IV1			0x0018
	#define IPSEC2_IV2			0x001C
	#define IPSEC2_IV3			0x0020
	#define IPSEC2_CIPHER_KEY0		0x0024
	#define IPSEC2_CIPHER_KEY1		0x0028
	#define IPSEC2_CIPHER_KEY2		0x002C
	#define IPSEC2_CIPHER_KEY3		0x0030
	#define IPSEC2_CIPHER_KEY4		0x0034
	#define IPSEC2_CIPHER_KEY5		0x0038
	#define IPSEC2_CIPHER_KEY6		0x003C
	#define IPSEC2_CIPHER_KEY7		0x0040
	#define IPSEC2_HMAC_KEY0		0x0044
	#define IPSEC2_HMAC_KEY1		0x0048
	#define IPSEC2_HMAC_KEY2		0x004C
	#define IPSEC2_HMAC_KEY3		0x0050
	#define IPSEC2_HMAC_KEY4		0x0054
	#define IPSEC2_HMAC_KEY5		0x0058
	#define IPSEC2_HMAC_KEY6		0x005C
	#define IPSEC2_HMAC_KEY7		0x0060
	#define IPSEC2_HMAC_RES0		0x0064
	#define IPSEC2_HMAC_RES1		0x0068
	#define IPSEC2_HMAC_RES2		0x006C
	#define IPSEC2_HMAC_RES3		0x0070
	#define IPSEC2_HMAC_RES4		0x0074
	#define IPSEC2_HMAC_RES5		0x0078
	#define IPSEC2_HMAC_RES6		0x007C
	#define IPSEC2_HMAC_RES7		0x0080
	#define IPSEC2_CNT_LOAD_VAL0		0x0084
	#define IPSEC2_CNT_LOAD_VAL1		0x0088
	#define IPSEC2_CNT_LOAD_VAL2		0x008C
	#define IPSEC2_CNT_LOAD_VAL3		0x0090
	#define IPSEC2_CNT_MODE			0x0094
	#define IPSEC2_COUNTER0			0x0098
	#define IPSEC2_COUNTER1			0x009C
	#define IPSEC2_COUNTER2			0x00A0
	#define IPSEC2_COUNTER3			0x00A4
	
	/*
	* Mode register flags and masks
	*/
	
	/*Bit 0 : EN*/
	#define IPSEC2_MODE_EN				0x00000001
	/*Bit 1-2 : MODE*/
	#define IPSEC2_MODE_CIPHER			0x00000000
	#define IPSEC2_MODE_ESP				0x00000002		//This mode gives wrong result, do not use it
	#define IPSEC2_MODE_HASH			0x00000004
	#define IPSEC2_MODE_HMAC			0x00000006
	/*Bit 3 : CBC*/
	#define IPSEC2_MODE_CBC				0x00000008
	#define IPSEC2_MODE_ECB				0x00000000
	/*Bit 4 : CTS*/
	#define IPSEC2_MODE_CTS				0x00000010
	/*Bit 5 : TDES*/
	#define IPSEC2_MODE_TDES			0x00000020
	#define IPSEC2_MODE_DES				0x00000000
	/*Bit 6 : ENC*/
	#define IPSEC2_MODE_ENCRYPT		0x00000040
	#define IPSEC2_MODE_DECRYPT		0x00000000
	/*Bit 7 : KEY3*/
	#define IPSEC2_MODE_KEY3			0x00000080
	/*Bit 8 : HASH*/
	#define IPSEC2_MODE_MD5				0x00000100
	#define IPSEC2_MODE_SHA1			0x00000000
	/*Bit 9 : NDN*/
	#define IPSEC2_MODE_BIGEND			0x00000200
	/*Bit 10 : RST*/
	#define IPSEC2_MODE_RESET			0x00000400
	/*Bit 11 : SHA2*/
	#define IPSEC2_MODE_SHA256			0x00000800
	/*Bit 12 : CIPHER*/
	#define IPSEC2_MODE_AES				0x00001000
	/*Bit 13 : NEW*/
	#define IPSEC2_MODE_AES_NEW_KEY	0x00002000
	/*Bit 14-15 : AES Key*/
	#define IPSEC2_MODE_AES_KEY_128	0x00000000
	#define IPSEC2_MODE_AES_KEY_192	0x00004000
	#define IPSEC2_MODE_AES_KEY_256	0x00008000
	#define IPSEC2_MODE_AES_KEY_NA	0x0000C000
	/*Bit 16 : OFB*/
	#define IPSEC2_MODE_AES_OFB		0x00010000
	/*Bit 17 : CFB*/
	#define IPSEC2_MODE_AES_CFB		0x00020000
	/*Bit 18 : CTR*/
	#define IPSEC2_MODE_AES_CTR		0x00040000
	/*Bit 19-20 : WIDTH*/
	#define IPSEC2_MODE_AES_CFB_W1	0x00000000
	#define IPSEC2_MODE_AES_CFB_W8	0x00080000
	#define IPSEC2_MODE_AES_CFB_W64	0x00100000
	#define IPSEC2_MODE_AES_CFB_W128 0x00180000
	/* ??? */
	#define IPSEC2_MODE_HMAC_MASK		0x00000FF0
	#define IPSEC2_MODE_MASK			0x00000006
	
	
	/*
	* Status register mask and flags
	*/
	
	#define IPSEC2_STATUS_IRQ			0x00000001 /*Bit 0*/
	#define IPSEC2_STATUS_BUSY			0x00000002 /*Bit 1*/
	#define IPSEC2_STATUS_SMCERR		0x00000004 /*Bit 2*/
	
	/*
	* Counter mode mask and flags
	*/
	#define IPSEC2_COUNTER_MODE_BITLOAD128	0x00000001
	#define IPSEC2_COUNTER_MODE_WORD1		0x00008000
	#define IPSEC2_COUNTER_MODE_WORD2		0x0000A000
	#define IPSEC2_COUNTER_MODE_WORD3		0x0000C000
	#define IPSEC2_COUNTER_MODE_WORD4		0x0000E000
	
	
	
	
	/*Hmac encryption definitions*/
	#define IPSEC2_MD5		1
	#define IPSEC2_SHA1		2
	#define IPSEC2_SHA256	3
	
	/*Cipher encryption definitions*/
	#define IPSEC2_DES		1
	#define IPSEC2_3DES		2
	#define IPSEC2_AES128	3
	#define IPSEC2_AES192	4
	#define IPSEC2_AES256	5

	#define DATA_BUFFER_SIZE 1600 		/*1600 is large enough for Ipsec*/

	
/****************************************************************************
* Structures
****************************************************************************/

	/*
	 * struct ipsec2_stat : 
	 *
	 * Cadence ipsec2 hardware accelerator statistics
	*/
	struct ipsec2_stat {
		int allocatedHandler;
		int freedHandler;
		int handlerErrors;
		int performedDESReq;
		int performed3DESReq;
		int performedAES128Req;
		int performedAES192Req;
		int performedAES256Req;
		int performedMD5Req;
		int performedSHA1Req;
		int totalPerformedReq;
		int hardwareErrors;
		unsigned long workingTime;
		int usage;
	};

	/*
	 * struct cadence_ipsec2 : 
	*
	* Cadence ipsec2 hardware accelerator
	*/
	typedef struct ipsec2_device {
	
			unsigned int base_address;					/* virtual base address for device*/
			struct timer_list timer;
			unsigned int performedEncReqId; 		/* Allow to know the last Encryption request performed by the device */
			unsigned int performedAutReqId; 		/* Allow to know the last Authentication request performed by the device */
			char *srcBuffer;							/* Source buffer is always the same */
			char *dstBuffer;							/* Destination buffer is always the same */
			struct ipsec2_stat stats;				/* Statistics */
	} *ipsec2_device;



/************************************************************************
* Functions definition
*************************************************************************/

	void ipsec2_GetModeRegister(ipsec2_device device, int *value);
	void ipsec2_SetModeRegister(ipsec2_device device, int value);
	void ipsec2_GetStatusRegister(ipsec2_device device, int* value);
	void ipsec2_GetSrcAddrRegister(ipsec2_device device, int* value);
	void ipsec2_SetSrcAddrRegister(ipsec2_device device, int value);
	void ipsec2_GetDstAddrRegister(ipsec2_device device, int* value);
	void ipsec2_SetDstAddrRegister(ipsec2_device device, int value);
	void ipsec2_GetLengthRegister(ipsec2_device device, int* value);
	void ipsec2_SetLengthRegister(ipsec2_device device, int value);
	void ipsec2_GetIvNRegister(ipsec2_device device, int value[4]);
	void ipsec2_SetIvNRegister(ipsec2_device device, int value[4], int ivLen);
	void ipsec2_GetCipherKeyNRegister(ipsec2_device device, int value[8]);
	void ipsec2_SetCipherKeyNRegister(ipsec2_device device, int value[8], int keyLen, int algorithm);
	void ipsec2_GetHmacKeyNRegister(ipsec2_device device, int value[8]);
	void ipsec2_SetHmacKeyNRegister(ipsec2_device device, int value[8], int algorithm);
	void ipsec2_GetHmacResNRegister(ipsec2_device device, int value[8], int algorithm);
	void ipsec2_GetCNTLoadValNRegister(ipsec2_device device, int value[4]);
	void ipsec2_SetCNTLoadValNRegister(ipsec2_device device, int value[4]);
	void ipsec2_GetCounterModeRegister(ipsec2_device device, int value);
	void ipsec2_SetCounterModeRegister(ipsec2_device device, int value);
	void ipsec2_GetCounterNRegister(ipsec2_device device, int value[4]);
	

	void ipsec2_reverse64(int in[2], int out[2]);
	void ipsec2_reverse128(int in[4], int out[4]);
	void ipsec2_reverse160(int in[5], int out[5]);
	void ipsec2_reverse192(int in[6], int out[6]);
	void ipsec2_reverse256(int in[8], int out[8]);

/************************************************************************
 * Functions headers                                                    *
 ************************************************************************/
	__u8* _Cadence_cipher_new_key(struct ipsec_alg_enc *alg, const __u8 *key, size_t keysize);
	void _Cadence_cipher_destroy_key(struct ipsec_alg_enc *alg, __u8 *key_e);
	int _Cadence_cipher_cbc_encrypt(struct ipsec_alg_enc *alg, __u8 * key_e, __u8 * in, int ilen, const __u8 * iv, int encrypt);
	
	__u8* _Cadence_hmac_new_key(struct ipsec_alg_auth *alg, const __u8 * key, int keylen);
	void _Cadence_hmac_destroy_key(struct ipsec_alg_auth *alg, __u8 *key_a);
	int _Cadence_hmac_hash(struct ipsec_alg_auth *alg, __u8 * key_a, const __u8 * dat, int len, __u8 * hash, int hashlen);


#endif
