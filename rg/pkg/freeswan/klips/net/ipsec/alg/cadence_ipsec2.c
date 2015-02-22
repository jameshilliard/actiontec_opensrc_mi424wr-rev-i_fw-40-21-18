/************************************************************************
 * Cadence Ipsec2 library
 *
 * Written by Fabien Marotte <fabien.marotte@mindspeed.com>
 ************************************************************************/

#include "cadence_ipsec2.h"

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetModeRegister(ipsec2_device device, int* value)
{
	*value = readl(device->base_address + IPSEC2_MODE);
}

void ipsec2_SetModeRegister(ipsec2_device device, int value)
{
	writel(value, (void*)(device->base_address + IPSEC2_MODE));
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetStatusRegister(ipsec2_device device, int* value)
{
	*value = readl(device->base_address + IPSEC2_STATUS);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetSrcAddrRegister(ipsec2_device device, int* value)
{	
	*value = readl(device->base_address + IPSEC2_SRCADDR);
}

void ipsec2_SetSrcAddrRegister(ipsec2_device device, int value)
{
	writel(value, device->base_address + IPSEC2_SRCADDR);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetDstAddrRegister(ipsec2_device device, int* value)
{
	*value = readl(device->base_address + IPSEC2_DSTADDR);
}

void ipsec2_SetDstAddrRegister(ipsec2_device device, int value)
{
	writel(value, device->base_address + IPSEC2_DSTADDR);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetLengthRegister(ipsec2_device device, int* value)
{
	*value = readl(device->base_address + IPSEC2_LEN);
}

void ipsec2_SetLengthRegister(ipsec2_device device, int value)
{
	writel(value, device->base_address + IPSEC2_LEN);
}

/************************************************************************
 * Get IvN Register
 *
 ************************************************************************/
void ipsec2_GetIvNRegister(ipsec2_device device, int value[4])
{
	/*Work around TO DO*/
	value[0] = readl(device->base_address + IPSEC2_IV0);
	value[1] = readl(device->base_address + IPSEC2_IV1);
	value[2] = readl(device->base_address + IPSEC2_IV2);
	value[3] = readl(device->base_address + IPSEC2_IV3);
}

void ipsec2_SetIvNRegister(ipsec2_device device, int value[4], int ivLen)
{
	int buffer[4] = {0,0,0,0};
	
	if (ivLen == 8)
		ipsec2_reverse64(value, buffer);
	else
		ipsec2_reverse128(value, buffer);

	writel(buffer[0], device->base_address + IPSEC2_IV0);
	writel(buffer[1], device->base_address + IPSEC2_IV1);
	writel(buffer[2], device->base_address + IPSEC2_IV2);
	writel(buffer[3], device->base_address + IPSEC2_IV3);
}

/************************************************************************
 * Cipher key register
 *
 ************************************************************************/
void ipsec2_GetCipherKeyNRegister(ipsec2_device device, int value[8])
{
	/*Work around TO DO*/
	value[0] = readl(device->base_address + IPSEC2_CIPHER_KEY0);
	value[1] = readl(device->base_address + IPSEC2_CIPHER_KEY1);
	value[2] = readl(device->base_address + IPSEC2_CIPHER_KEY2);
	value[3] = readl(device->base_address + IPSEC2_CIPHER_KEY3);
	value[4] = readl(device->base_address + IPSEC2_CIPHER_KEY4);
	value[5] = readl(device->base_address + IPSEC2_CIPHER_KEY5);
	value[6] = readl(device->base_address + IPSEC2_CIPHER_KEY6);
	value[7] = readl(device->base_address + IPSEC2_CIPHER_KEY7);
}

void ipsec2_SetCipherKeyNRegister(ipsec2_device device, int value[8], int keyLen, int algorithm)
{
	int buffer[8] = {0,0,0,0,0,0,0,0};
	
	switch (algorithm)
	{
		case IPSEC2_AES128 :
			ipsec2_reverse128(value, buffer);
			break;
			
		case IPSEC2_AES192 :
			ipsec2_reverse192(value, buffer);
			break;
			
		case IPSEC2_AES256 :	
			ipsec2_reverse256(value, buffer);
			break;

		case IPSEC2_DES :
			ipsec2_reverse64(value, buffer);
			break;

		case IPSEC2_3DES :
			ipsec2_reverse64(value, buffer);
			ipsec2_reverse64(value+2, buffer+2);
			if (keyLen == 24)
				ipsec2_reverse64(value+4, buffer+4);

			break;
		
		default :
			return;
	}

	writel(buffer[0], device->base_address + IPSEC2_CIPHER_KEY0);
	writel(buffer[1], device->base_address + IPSEC2_CIPHER_KEY1);
	writel(buffer[2], device->base_address + IPSEC2_CIPHER_KEY2);
	writel(buffer[3], device->base_address + IPSEC2_CIPHER_KEY3);
	writel(buffer[4], device->base_address + IPSEC2_CIPHER_KEY4);
	writel(buffer[5], device->base_address + IPSEC2_CIPHER_KEY5);
	writel(buffer[6], device->base_address + IPSEC2_CIPHER_KEY6);
	writel(buffer[7], device->base_address + IPSEC2_CIPHER_KEY7);
}

/************************************************************************
 * Hmac key register
 *
 ************************************************************************/
void ipsec2_GetHmacKeyNRegister(ipsec2_device device, int *value)
{
	/*Work around TO DO*/
	value[0] = readl(device->base_address + IPSEC2_HMAC_KEY0);
	value[1] = readl(device->base_address + IPSEC2_HMAC_KEY1);
	value[2] = readl(device->base_address + IPSEC2_HMAC_KEY2);
	value[3] = readl(device->base_address + IPSEC2_HMAC_KEY3);
	value[4] = readl(device->base_address + IPSEC2_HMAC_KEY4);
	value[5] = readl(device->base_address + IPSEC2_HMAC_KEY5);
	value[6] = readl(device->base_address + IPSEC2_HMAC_KEY6);
	value[7] = readl(device->base_address + IPSEC2_HMAC_KEY7);
}

void ipsec2_SetHmacKeyNRegister(ipsec2_device device, int value[8], int algorithm)
{
	int buffer[8] = {0,0,0,0,0,0,0,0};

	/*How to bypass a hardware bug*/
	switch (algorithm)
	{
		case IPSEC2_MD5 :
		case IPSEC2_SHA1:

			buffer[0] = value[4];
			buffer[1] = value[3];
			buffer[2] = value[2];
			buffer[3] = value[1];
			buffer[4] = value[0];

			break;
						
		case IPSEC2_SHA256:

			ipsec2_reverse256(value, buffer);
			break;
			
		default :
			return;
	}
	 
	writel(buffer[0], device->base_address + IPSEC2_HMAC_KEY0);
	writel(buffer[1], device->base_address + IPSEC2_HMAC_KEY1);
	writel(buffer[2], device->base_address + IPSEC2_HMAC_KEY2);
	writel(buffer[3], device->base_address + IPSEC2_HMAC_KEY3);
	writel(buffer[4], device->base_address + IPSEC2_HMAC_KEY4);
	writel(buffer[5], device->base_address + IPSEC2_HMAC_KEY5);
	writel(buffer[6], device->base_address + IPSEC2_HMAC_KEY6);
	writel(buffer[7], device->base_address + IPSEC2_HMAC_KEY7);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetHmacResNRegister(ipsec2_device device, int value[8], int algorithm)
{
	int buffer[8];
	
	buffer[0] = readl(device->base_address + IPSEC2_HMAC_RES0);
	buffer[1] = readl(device->base_address + IPSEC2_HMAC_RES1);
	buffer[2] = readl(device->base_address + IPSEC2_HMAC_RES2);
	buffer[3] = readl(device->base_address + IPSEC2_HMAC_RES3);
	buffer[4] = readl(device->base_address + IPSEC2_HMAC_RES4);
	buffer[5] = readl(device->base_address + IPSEC2_HMAC_RES5);
	buffer[6] = readl(device->base_address + IPSEC2_HMAC_RES6);
	buffer[7] = readl(device->base_address + IPSEC2_HMAC_RES7);

	/*How to bypass a hardware bug*/
	switch (algorithm)
	{
		case IPSEC2_MD5 :
		case IPSEC2_SHA1:

			value[0] = buffer[4];
			value[1] = buffer[3];
			value[2] = buffer[2];
			value[3] = buffer[1];
			value[4] = buffer[0];
			value[5] = 0;
			value[6] = 0;
			value[7] = 0;

			break;
						
		case IPSEC2_SHA256:

			ipsec2_reverse256(buffer, value);
			break;
			
		default :
			break;
	}
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetCNTLoadValNRegister(ipsec2_device device, int value[4])
{
	value[0] = readl(device->base_address + IPSEC2_CNT_LOAD_VAL0);
	value[1] = readl(device->base_address + IPSEC2_CNT_LOAD_VAL1);
	value[2] = readl(device->base_address + IPSEC2_CNT_LOAD_VAL2);
	value[3] = readl(device->base_address + IPSEC2_CNT_LOAD_VAL3);
}

void ipsec2_SetCNTLoadValNRegister(ipsec2_device device, int value[4])
{
	writel(value[0], device->base_address + IPSEC2_CNT_LOAD_VAL0);
	writel(value[1], device->base_address + IPSEC2_CNT_LOAD_VAL1);
	writel(value[2], device->base_address + IPSEC2_CNT_LOAD_VAL2);
	writel(value[3], device->base_address + IPSEC2_CNT_LOAD_VAL3);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetCounterModeRegister(ipsec2_device device, int value)
{
	value = readl(device->base_address + IPSEC2_CNT_MODE);
}

void ipsec2_SetCounterModeRegister(ipsec2_device device, int value)
{
	writel(value, device->base_address + IPSEC2_CNT_MODE);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_GetCounterNRegister(ipsec2_device device, int value[4])
{
	value[0] = readl(device->base_address + IPSEC2_COUNTER0);
	value[1] = readl(device->base_address + IPSEC2_COUNTER1);
	value[2] = readl(device->base_address + IPSEC2_COUNTER2);
	value[3] = readl(device->base_address + IPSEC2_COUNTER3);
}


/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_reverse64(int in[2], int out[2])
{
	out[0] = ntohl(in[1]);
	out[1] = ntohl(in[0]);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_reverse128(int in[4], int out[4])
{
	out[0] = ntohl(in[3]);
	out[1] = ntohl(in[2]);
	out[2] = ntohl(in[1]);
	out[3] = ntohl(in[0]);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_reverse160(int in[5], int out[5])
{
	out[0] = ntohl(in[4]);
	out[1] = ntohl(in[3]);
	out[2] = ntohl(in[2]);
	out[3] = ntohl(in[1]);
	out[4] = ntohl(in[0]);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_reverse192(int in[6], int out[6])
{
	out[0] = ntohl(in[5]);
	out[1] = ntohl(in[4]);
	out[2] = ntohl(in[3]);
	out[3] = ntohl(in[2]);
	out[4] = ntohl(in[1]);
	out[5] = ntohl(in[0]);
}

/************************************************************************
 *
 *
 ************************************************************************/
void ipsec2_reverse256(int in[8], int out[8])
{
	out[0] = ntohl(in[7]);
	out[1] = ntohl(in[6]);
	out[2] = ntohl(in[5]);
	out[3] = ntohl(in[4]);
	out[4] = ntohl(in[3]);
	out[5] = ntohl(in[2]);
	out[6] = ntohl(in[1]);
	out[7] = ntohl(in[0]);
}
