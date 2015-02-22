/******************************************************
 * Cadence Ipsec2 library
 *
 * Written by Fabien Marotte <fabien.marotte@mindspeed.com>
 *******************************************************/
#ifndef __CADENCE_IPSEC2_ITF_H__
#define __CADENCE_IPSEC2_ITF_H__

#include <linux/mm.h> 
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>

	
	#include "cadence_ipsec2.h"

	#define IPSEC2_USAGE_INTERVAL 5				/*Hardware usage refresh time, value in second*/

	/*Error code definitions*/
	#define IPSEC2_ERR_NO_ERROR 					 0
	#define IPSEC2_ERR_BAD_PARAMETER				-1
	#define IPSEC2_ERR_PERFORMING_REQUEST		-2
	#define IPSEC2_ERR_NO_MORE_MEMORY			-3
	#define IPSEC2_ERR_UNKNOWN_MODE				-4
	#define IPSEC2_ERR_UNKNOWN_CIPHER_ENC		-5
	#define IPSEC2_ERR_UNKNOWN_HMAC_ENC			-6
	#define IPSEC2_ERR_PARAMETER_MISSING		-7
	#define IPSEC2_ERR_HARDWARE_ERROR			-8

	
	typedef struct ipsec2_request{
		int id;							/*Unique Identifier. Must be the first variable of the structure*/

		ipsec2_device device;		/*Hardware device associated to the request*/

		int modeRegister;

		char* srcAddr;			/*Data to encrypt/decrypt*/
		int srcLen;				/*Data size*/

		char* dstAddr;			/*Address where data result will be stored*/
		int dstLen;				/*Buffer size*/

		int hmacKey[8];		/*8*4 bytes = 256 bits*/
		int cipherKey[8];		/*8*4 bytes = 256 bits*/
		int cipherIV[4];		/*4*4 bytes = 128 bits*/

		/*
		* These parameters are redundant
		* Info can be found analysing modeRegister
		*/
		int cipherAlgorithm;
		int hmacAlgorithm;
		int cipherKeyLen;
		int ivLen;
		int blockSize;

	}*ipsec2_request;

	
	void ipsec2DriverInit(void);
	void ipsec2DriverUnInit(void);
	
	int ipsec2NewRequest(ipsec2_request* request);
	int ipsec2FreeRequest(ipsec2_request* request);
	int ipsec2ResetRequest(ipsec2_request request);
	
	int ipsec2SetMode(ipsec2_request request, int mode);
	
	int ipsec2GetOperation(ipsec2_request request);
	int ipsec2SetOperation(ipsec2_request request, int op);
	
	int ipsec2SetCipherEncryption(ipsec2_request request, int cyphEnc, int BlockSize);
	
	int ipsec2SetHmacEncryption(ipsec2_request request, int hmacEnc);
	
	int ipsec2SetCipherKey(ipsec2_request request, char* cypherKey, int cipherKeyLen);
	
	int ipsec2SetHmacKey(ipsec2_request request, char* hmacKey, int hmacKeyLen);
	
	int ipsec2SetCipherIV(ipsec2_request request, char* cipherIV, int cipherIVLen);
	
	int ipsec2SetCipherOptions(ipsec2_request request, int options);
	int ipsec2UnsetCipherOptions(ipsec2_request request, int options);
	
	int ipsec2SetSrcAddr(ipsec2_request request, void* srcAddr, int Len);
	
	int ipsec2SetDstAddr(ipsec2_request request, void* dstAddr, int Len);
	
	int ipsec2ExecuteRequest(ipsec2_request request);
	
#ifdef CADENCE_IPSEC2_DEBUG
	void ipsec2PrintRequest(ipsec2_request request);
#endif

	void memswap64(void *to,const void *from,unsigned long n);
	void memswap128(void *to,const void *from,unsigned long n);

	int ipsec2_proc_info(char* page, char** start, off_t off, int count, int *eof, void* data);
	void ipsec2Usage(unsigned long data);
	
#endif
