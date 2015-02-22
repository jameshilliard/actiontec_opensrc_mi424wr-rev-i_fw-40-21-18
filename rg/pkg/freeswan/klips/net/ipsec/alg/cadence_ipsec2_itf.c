/******************************************************
 * Cadence Ipsec2 library
 *
 * Written by Fabien Marotte <fabien.marotte@mindspeed.com>
 *******************************************************/
#include "cadence_ipsec2_itf.h"

struct ipsec2_device hardwareDevice;
unsigned int cptId = 1; /*Variable only incremented */


/**************************************************************************************************
 *
 *
 **************************************************************************************************/
void ipsec2DriverInit(void)
{
	/* Structure initialization*/
	hardwareDevice.base_address = IO_ADDRESS(M828XX_IPSEC_BASE);
	init_timer(&hardwareDevice.timer);
	hardwareDevice.performedEncReqId = 0;
	hardwareDevice.performedAutReqId = 0;
	hardwareDevice.srcBuffer = kmalloc(DATA_BUFFER_SIZE, GFP_ATOMIC);
	hardwareDevice.dstBuffer = kmalloc(DATA_BUFFER_SIZE, GFP_ATOMIC);
	memset(&hardwareDevice.stats, 0, sizeof(struct ipsec2_stat));

	/*Set timer for statistics*/
	hardwareDevice.timer.function = ipsec2Usage;
	hardwareDevice.timer.expires = jiffies + (IPSEC2_USAGE_INTERVAL * HZ);
	hardwareDevice.timer.data = (long) &hardwareDevice;
	add_timer(&(hardwareDevice.timer));

	/*Perform a hardware reset*/
	ipsec2_SetModeRegister(&hardwareDevice, IPSEC2_MODE_RESET);

	/*Set hardware registers with source and destination buffers address*/
	ipsec2_SetSrcAddrRegister(&hardwareDevice, virt_to_phys(hardwareDevice.srcBuffer));
	ipsec2_SetDstAddrRegister(&hardwareDevice, virt_to_phys(hardwareDevice.dstBuffer));

	/* Create /proc/cadence-ipsec2 entry */
	create_proc_read_entry("cadence-ipsec2", 0, 0, ipsec2_proc_info, NULL);
}

/**************************************************************************************************
 *
 *
 **************************************************************************************************/
void ipsec2DriverUnInit(void)
{
	/* Remove /proc/cadence-ipsec2 entry */
	remove_proc_entry("cadence-ipsec2", NULL);
	
	/*Delete timer*/
	del_timer(&(hardwareDevice.timer));

	/*Perform a hardware reset*/
	ipsec2_SetModeRegister(&hardwareDevice, IPSEC2_MODE_RESET);

	/* Free source and destination buffers */
	kfree(hardwareDevice.srcBuffer);
	kfree(hardwareDevice.dstBuffer);
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2NewRequest(ipsec2_request* request)
{
	int ret;
	
	if (!request)
		return IPSEC2_ERR_BAD_PARAMETER;

	*request = (ipsec2_request) kmalloc(sizeof (struct ipsec2_request), GFP_KERNEL);

	if (!(*request))
		return IPSEC2_ERR_NO_MORE_MEMORY;
	
	(*request)->id = cptId;
	cptId++;
	
	(*request)->device = &hardwareDevice;
	
	ret = ipsec2ResetRequest(*request);
	
	(*request)->device->stats.allocatedHandler++;
	
	return ret;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2FreeRequest(ipsec2_request* request)
{
	(*request)->device->stats.freedHandler++;
	
	kfree(*request);
	(*request) = NULL;
	
	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2ResetRequest(ipsec2_request request)
{
	int temp;
	ipsec2_device devtemp; 
	
	temp = request->id;
	devtemp = request->device;
	
	memset(request, 0, sizeof(struct ipsec2_request));

	request->id = temp;
	request->device = devtemp;

	return IPSEC2_ERR_NO_ERROR;
}

/*************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetMode(ipsec2_request request, int mode)
{
	request->modeRegister = (request->modeRegister& (~IPSEC2_MODE_HMAC) ) | mode;
	
	return IPSEC2_ERR_NO_ERROR;
}

/*************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2GetOperation(ipsec2_request request)
{
	return (request->modeRegister & IPSEC2_MODE_ENCRYPT);
}
/**************************************************************************************************
*	 ipsec2SetOperation
* 		Set Encryption or decryption
***************************************************************************************************/
int ipsec2SetOperation(ipsec2_request request, int op) 
{
	request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_ENCRYPT)) |op;
	
	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetCipherEncryption(ipsec2_request request, int cyphEnc, int BlockSize)
{
	switch (cyphEnc)
	{
		case IPSEC2_DES :
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_TDES) ) | IPSEC2_MODE_DES;
			break;

		case IPSEC2_3DES : 
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_TDES) ) | IPSEC2_MODE_TDES;
			break;

		case IPSEC2_AES128 :
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_AES)) | IPSEC2_MODE_AES;
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_AES_KEY_NA)) | IPSEC2_MODE_AES_KEY_128;
			break;

		case IPSEC2_AES192 :
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_AES)) | IPSEC2_MODE_AES;
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_AES_KEY_NA)) | IPSEC2_MODE_AES_KEY_192;
			break;

		case IPSEC2_AES256 :
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_AES)) | IPSEC2_MODE_AES;
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_AES_KEY_NA)) | IPSEC2_MODE_AES_KEY_256;
			break;

		default :
			return IPSEC2_ERR_UNKNOWN_CIPHER_ENC;
	}

	request->cipherAlgorithm = cyphEnc;
	request->blockSize = BlockSize;
	
	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetHmacEncryption(ipsec2_request request, int hmacEnc)
{
	switch (hmacEnc)
	{
		case IPSEC2_MD5 : 
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_MD5)) | IPSEC2_MODE_MD5;
			break;

		case IPSEC2_SHA1 : 
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_MD5)) | IPSEC2_MODE_SHA1;
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_SHA256)) | IPSEC2_MODE_SHA1;
			break;

		case IPSEC2_SHA256 :
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_MD5)) | IPSEC2_MODE_SHA1;
			request->modeRegister = (request->modeRegister & (~IPSEC2_MODE_SHA256)) | IPSEC2_MODE_SHA256;
			break;

		default :
			return IPSEC2_ERR_UNKNOWN_HMAC_ENC;
	}

	request->hmacAlgorithm = hmacEnc;

	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetCipherKey(ipsec2_request request, char* cypherKey, int cipherKeyLen)
{
	request->cipherKeyLen = cipherKeyLen;
	memcpy(request->cipherKey, cypherKey, cipherKeyLen);

	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetHmacKey(ipsec2_request request, char* hmacKey, int hmacKeyLen)
{
	memcpy(request->hmacKey, hmacKey, hmacKeyLen);
	
	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetCipherIV(ipsec2_request request, char* cipherIV, int cipherIVLen)
{
	memcpy(request->cipherIV, cipherIV, cipherIVLen);
	request->ivLen = cipherIVLen;
	
	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetCipherOptions(ipsec2_request request, int options)
{
	request->modeRegister |= options;
	
	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2UnsetCipherOptions(ipsec2_request request, int options)
{
	request->modeRegister &= ~options;

	return IPSEC2_ERR_NO_ERROR;
}
/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetSrcAddr(ipsec2_request request, void* srcAddr, int Len)
{
	request->srcAddr = srcAddr;
	request->srcLen = Len;
	
	return IPSEC2_ERR_NO_ERROR;
}
/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2SetDstAddr(ipsec2_request request, void* dstAddr, int Len)
{
	request->dstAddr = dstAddr;
	request->dstLen = Len;
	
	return IPSEC2_ERR_NO_ERROR;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2ExecuteRequest(ipsec2_request request)
{
	int status, waiting, new_key = 0;
	int ret = IPSEC2_ERR_NO_ERROR;
	int encType;
	int hmacTmp[8];
	struct timeval timeBefore, timeAfter;
	
	if ( !request->srcAddr)
	{
		printk(KERN_ERR "ipsec2ExecuteRequest : Request->srcAddr parameter missing\n");
		request->device->stats.handlerErrors++;		/*Statistic*/
		ret = IPSEC2_ERR_PARAMETER_MISSING;
		goto err;
	}
	if (!request->dstAddr)
	{
		printk(KERN_ERR "ipsec2ExecuteRequest : Request->dstAddr parameter missing\n");
		request->device->stats.handlerErrors++;		/*Statistic*/
		ret = IPSEC2_ERR_PARAMETER_MISSING;
		goto err;
	}

	/*
	* 1. Write key if needed
	* 2. Write Initialisation Vector
	* 3. Write source data
	* 4. Write source data length
	* 5. Write mode register (execute the request)
	* 6. Wait for hardware
	* 7. Read encrypted/decrypted data
	*/

	/*Stats*/
	do_gettimeofday(&timeBefore);
		
	/*encType : 0 => CIPHER, >0 => HMAC*/
	encType = request->modeRegister & IPSEC2_MODE_HMAC;

	/*
	 * 1. Write key if needed
	 *    We don't need to re-write HMAC and Cipher Keys if the last executed request was with same keys
	 */
	/*Hmac Key*/
	if (encType)
	{
		if (request->id != request->device->performedAutReqId)
		{
			ipsec2_SetHmacKeyNRegister(request->device , request->hmacKey, request->hmacAlgorithm);
			request->device->performedAutReqId = request->id;
		}
		/*
		 * 3. Write source data
		 */
		memcpy(request->device->srcBuffer, request->srcAddr, request->srcLen);
	}
	/* CipherKey*/
	else
	{
		if (request->id != request->device->performedEncReqId)
		{
			ipsec2_SetCipherKeyNRegister(request->device, request->cipherKey, request->cipherKeyLen, request->cipherAlgorithm);
			request->device->performedEncReqId = request->id;

			/*If we write a new AES key, hardware needs to know it*/
			if (request->modeRegister & IPSEC2_MODE_AES)
			{
				new_key = IPSEC2_MODE_AES_NEW_KEY;
			}
		}
		/*
		 * 2. Write Initialisation Vector when Cipher
		 */
		ipsec2_SetIvNRegister(request->device, request->cipherIV, request->ivLen);
		
		/*
		 * 3. Write source data
		 *    Cadence processor has a bug, need to swap some bytes
		 */
	
		if (request->blockSize == 8)
			memswap64(request->device->srcBuffer, request->srcAddr, request->srcLen);
		else
			memswap128(request->device->srcBuffer, request->srcAddr, request->srcLen);
	}

	/*
	 * 4. Write source data length
	 */
	ipsec2_SetLengthRegister(request->device, request->srcLen);
	
	consistent_sync(request->device->srcBuffer, DATA_BUFFER_SIZE, DMA_TO_DEVICE); /* DMA Synchronization  */

	/*Statistics*/
	switch(request->cipherAlgorithm)
	{
		case IPSEC2_DES:
			request->device->stats.performedDESReq++;
			break;
		case IPSEC2_3DES:
			request->device->stats.performed3DESReq++;
			break;
		case IPSEC2_AES128:
			request->device->stats.performedAES128Req++;
			break;
		case IPSEC2_AES192:
			request->device->stats.performedAES192Req++;
			break;
		case IPSEC2_AES256:
			request->device->stats.performedAES256Req++;
			break;
		default:
			break;
	}
	switch(request->hmacAlgorithm)
	{
		case IPSEC2_MD5:
			request->device->stats.performedMD5Req++;
			break;
		case IPSEC2_SHA1:
			request->device->stats.performedSHA1Req++;
			break;
		default:
			break;
	}	
	request->device->stats.totalPerformedReq++;
	
	/*
	 * 5. Write mode register (execute the request)
	 */
	ipsec2_SetModeRegister(request->device, request->modeRegister | IPSEC2_MODE_EN | new_key);

	/*
	 * 6. Wait for hardware
	 */
	
	waiting =1;
	while(waiting)
	{
		/*Hardware has finished*/
		ipsec2_GetStatusRegister(request->device, &status);
		if (status & IPSEC2_STATUS_SMCERR)
		{
			printk(KERN_ERR "ipsec2ExecuteRequest : Hardware Error\n");
			request->device->stats.hardwareErrors++;		/*Statistic*/
			ret = IPSEC2_ERR_HARDWARE_ERROR;
			goto err;
		}
		else
			waiting  = status & IPSEC2_STATUS_BUSY;
	}
	
	
		
	if (encType)
	{
		/*HMAC mode*/
		/* We get the HMAC result*/
		/* We read 32 bytes from registers, if destination buffer isn't bigger enough, we trunk hmac*/
		
		ipsec2_GetHmacResNRegister(request->device, hmacTmp, request->hmacAlgorithm);
		memcpy(request->dstAddr, hmacTmp, request->dstLen);
	}
	else
	{
		/*Cipher mode*/
		consistent_sync(hardwareDevice.dstBuffer, DATA_BUFFER_SIZE, DMA_FROM_DEVICE); /* DMA and Cache Synchronization*/

		if (request->blockSize == 8)
		{
			memswap64(request->dstAddr, request->device->dstBuffer, request->srcLen);
		}
		else
		{
			memswap128(request->dstAddr, request->device->dstBuffer, request->srcLen);
		}
	}

err:
	/*Stats*/
	do_gettimeofday(&timeAfter);
	request->device->stats.workingTime += (timeAfter.tv_usec - timeBefore.tv_usec);

	return ret;
}

#ifdef CADENCE_IPSEC2_DEBUG
/************************************************************************
*
*
*************************************************************************/
void ipsec2PrintRequest(ipsec2_request request)
{
	int temp;
	/* 
		mode 
		algo
		operation
		options
		cipher key
		hmac key
		IV 
		src buffer
	*/
	printk(KERN_INFO "Info for request %d\n", request->id);
	
	printk(KERN_INFO"\tModeRegister : 0x%08x\n", request->modeRegister);
	printk(KERN_INFO"\tMode : ");
	temp = request->modeRegister & IPSEC2_MODE_HMAC;
	switch (temp)
	{
		case IPSEC2_MODE_CIPHER :
			printk("CIPHER\n");
			printk(KERN_INFO "\tCipher algorithm : ");
			temp = request->modeRegister & IPSEC2_MODE_AES;
			if(temp)
			{
				/*AES*/
				temp = request->modeRegister & IPSEC2_MODE_AES_KEY_NA;
				switch(temp)
				{
					case IPSEC2_MODE_AES_KEY_128 :
						printk("AES 128\n");
						break;
				
					case IPSEC2_MODE_AES_KEY_192 :
						printk("AES 192\n");
						break;
			
					case IPSEC2_MODE_AES_KEY_256 :
						printk("AES 256\n");
						break;
			
					default :
						printk("ERROR\n");
				}
			}
			else
			{
				/*DES or 3DES*/
				temp = request->modeRegister & IPSEC2_MODE_TDES;
				if(temp)
					printk("3DES\n");
				else
					printk("DES\n");
			}
			printk(KERN_INFO "\tOperation : ");
			temp = request->modeRegister & IPSEC2_MODE_ENCRYPT;
			if (temp)
				printk("ENCRYPT\n");
			else
				printk("DECRYPT\n");

			printk(KERN_INFO "\tOptions : ");
			temp = request->modeRegister & IPSEC2_MODE_CBC;
			if (temp)
				printk("CBC");
			else
				printk("ECB");
	
			temp = request->modeRegister & IPSEC2_MODE_CTS;
			if (temp)
				printk(", CTS");
	
			temp = request->modeRegister & IPSEC2_MODE_KEY3;
			if (temp)
				printk(", KEY3");
	
			printk("\n");

			break;

		case IPSEC2_MODE_HMAC :
			printk("HMAC\n");
			printk(KERN_INFO "\tHmac algorithm : ");
			temp = request->modeRegister & IPSEC2_MODE_MD5;
			if(temp)
				printk("MD5\n");
			else
			{
				temp = request->modeRegister & IPSEC2_MODE_SHA256;
				if (temp)
					printk("SHA2\n");
				else
					printk("SHA1\n");
			}
			break;
		
		default :
			printk("ERROR\n");
	}

	
	printk(KERN_INFO "\tSrc data : %x and %d bytes length\n", (int) request->srcAddr, request->srcLen);
	printk(KERN_INFO "\tDst data : %x and %d bytes length\n", (int) request->dstAddr, request->dstLen);
}
#endif
/**************************************************************************************************
*
*
**************************************************************************************************/
int ipsec2_proc_info(char* page, char** start, off_t off, int count, int *eof, void* data)
{
	/* 
	 * Harware usage in %
	 * Number of queued handler, waiting for hardware
	 * Number of allocated handler
	 * Number of freed handler
	 * Number of performed request :
	 *      DES : 
	 *      3DES :
	 *      AES 128 :
	 *      AES 192 :
	 *      AES 256 :
	 *      MD5 :
	 *      SHA1 :
	 */
	int len =0;
	
	len += sprintf(page+len, "Hardware usage :\t%d\n", hardwareDevice.stats.usage);
	len += sprintf(page+len, "Hardware errors :\t%d\n", hardwareDevice.stats.hardwareErrors);
	
	len += sprintf(page+len, "Handler allocated :\t%d\n", hardwareDevice.stats.allocatedHandler);
	len += sprintf(page+len, "Handler freed :\t\t%d\n", hardwareDevice.stats.freedHandler);
	len += sprintf(page+len, "Handler errors :\t%d\n", hardwareDevice.stats.handlerErrors);
	
	len += sprintf(page+len, "Performed request :\t%d\n", hardwareDevice.stats.totalPerformedReq);
	len += sprintf(page+len, "\tDES :\t\t%d\n", hardwareDevice.stats.performedDESReq);
	len += sprintf(page+len, "\t3DES :\t\t%d\n", hardwareDevice.stats.performed3DESReq);
	len += sprintf(page+len, "\tAES 128 :\t%d\n", hardwareDevice.stats.performedAES128Req);
	len += sprintf(page+len, "\tAES 192 :\t%d\n", hardwareDevice.stats.performedAES192Req);
	len += sprintf(page+len, "\tAES 256 :\t%d\n", hardwareDevice.stats.performedAES256Req);
	len += sprintf(page+len, "\tMD5 :\t\t%d\n", hardwareDevice.stats.performedMD5Req);
	len += sprintf(page+len, "\tSHA1 :\t\t%d\n", hardwareDevice.stats.performedSHA1Req);
	

	return len;
}

/**************************************************************************************************
*
*
**************************************************************************************************/
void ipsec2Usage(unsigned long data)
{
	struct ipsec2_device * device;

	device = (struct ipsec2_device *)data;

	device->stats.usage = (device->stats.workingTime* 100) / (IPSEC2_USAGE_INTERVAL*(1000000)) ;
	device->stats.workingTime = 0;

	device->timer.expires = jiffies + (IPSEC2_USAGE_INTERVAL * HZ);
	add_timer(&(device->timer));
}

