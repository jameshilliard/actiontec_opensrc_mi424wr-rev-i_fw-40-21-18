/*
 * IPSEC_GLUE interface code.
 * Copyright 2003 Intel Corporation All Rights Reserved.
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

#include <linux/socket.h>
#include <linux/in.h>
#include "ipsec_hwaccel.h"
#include "ipsec_glue_mbuf.h"
#include "ipsec_glue_desc.h"
#include "ipsec_log.h"

#include <linux/module.h>
EXPORT_SYMBOL(ipsec_hwaccel_add_service);

static HwAccelRegisterFunction registerFunctionPtr = NULL;
static HwAccelUnregisterFunction unregisterFunctionPtr = NULL;
static HwAccelPerformFunction performFunctionPtr = NULL;
static HwAccelOsBuffPoolInit initFunctionPtr = NULL;
static HwAccelOsBuffPoolInitNoAlloc initNoAllocFunctionPtr = NULL;
static HwAccelOsBuffPoolUnchainedBufGet getFunctionPtr = NULL;
static HwAccelOsBuffPoolBufFree freeFunctionPtr = NULL;
static HwAccelOsBuffPoolBufAreaSizeAligned alignedFunction = NULL;
static HwAccelOsServCacheDmaAlloc allocDmaFunction = NULL;
static HwAccelOsServCacheDmaFree freeDmaFunction = NULL;

void ipsec_hwaccel_add_service(HwAccelRegisterFunction registerFunc,
			       HwAccelUnregisterFunction unregisterFunc,
			       HwAccelPerformFunction performFunc,
			       HwAccelOsBuffPoolInit initFunc,
			       HwAccelOsBuffPoolInitNoAlloc initNoAllocFunc,
			       HwAccelOsBuffPoolUnchainedBufGet getFunc,
			       HwAccelOsBuffPoolBufFree freeFunc,
			       HwAccelOsBuffPoolBufAreaSizeAligned alignedFunc,
			       HwAccelOsServCacheDmaAlloc allocDmaFunc,
			       HwAccelOsServCacheDmaFree freeDmaFunc)
{
    if((registerFunc == NULL)||
       (unregisterFunc==NULL)||
       (performFunc==NULL)||
       (initFunc==NULL)||
       (initNoAllocFunc==NULL)||
       (getFunc==NULL)||
       (freeFunc==NULL)||
       (alignedFunc==NULL)||
       (allocDmaFunc==NULL)||
       (freeDmaFunc==NULL))
    {
	ipsec_log("ERROR: ipsec_hwaccel_add_service: "
	       "Invalid function pointers!\n");
    }
    
    registerFunctionPtr = registerFunc;
    unregisterFunctionPtr = unregisterFunc;
    performFunctionPtr = performFunc;
    initFunctionPtr = initFunc;
    initNoAllocFunctionPtr = initNoAllocFunc;
    getFunctionPtr = getFunc;
    freeFunctionPtr = freeFunc;
    alignedFunction = alignedFunc;
    allocDmaFunction = allocDmaFunc;
    freeDmaFunction = freeDmaFunc;

    /*  Now we have function pointers init mbuf pools */
    ipsec_glue_mbuf_header_init();    
    ipsec_glue_mbuf_init();
    ipsec_glue_rcv_desc_init();
    ipsec_glue_xmit_desc_init();
}


BOOL ipsec_hwaccel_ready(void)
{
    return (registerFunctionPtr!=NULL);
}

/*
 * NOTE: None of the functions below check non-Nullness of function pointers.
 * This is for performance reasons. It is assumed that calling client will yse
 * above ipsec_hwaccel_ready() function before calling these.
 */

IxCryptoAccStatus ipsec_hwaccel_register(
    IxCryptoAccCtx *pAccCtx,
    IX_MBUF *pMbufPrimaryChainVar,
    IX_MBUF *pMbufSecondaryChainVar,
    IxCryptoAccRegisterCompleteCallback registerCallbackFn,
    IxCryptoAccPerformCompleteCallback performCallbackFn,
    UINT32 *pCryptoCtxId)
{
    return registerFunctionPtr(pAccCtx, 
			       pMbufPrimaryChainVar,
			       pMbufSecondaryChainVar,
			       registerCallbackFn,
			       performCallbackFn,
			       pCryptoCtxId);
}

IxCryptoAccStatus ipsec_hwaccel_unregister (UINT32 cryptoCtxId)
{
    return unregisterFunctionPtr(cryptoCtxId);
}

IxCryptoAccStatus ipsec_hwaccel_perform (
    UINT32 cryptoCtxId,
    IX_MBUF *pSrcMbuf,
    IX_MBUF *pDestMbuf,
    UINT16 authStartOffset,
    UINT16 authDataLen,    
    UINT16 cryptStartOffset,   
    UINT16 cryptDataLen,
    UINT16 icvOffset,
    UINT8  *pIV)
{
    return performFunctionPtr(cryptoCtxId,
			      pSrcMbuf,
			      pDestMbuf,
			      authStartOffset,
			      authDataLen,    
			      cryptStartOffset,   
			      cryptDataLen,
			      icvOffset,
			      pIV);
}


IX_STATUS ipsec_hwaccel_OsBuffPoolInit (
    IX_MBUF_POOL **poolPtrPtr,
    int count,
    int size,
    char *name)
{
    return initFunctionPtr(poolPtrPtr,count,size,name);
}

IX_STATUS ipsec_hwaccel_OsBuffPoolInitNoAlloc (
    IX_MBUF_POOL **poolPtrPtr, 
    void *poolBufPtr, 
    void *poolDataPtr, 
    int count, 
    int size, 
    char *name)
{
    return initNoAllocFunctionPtr(poolPtrPtr,poolBufPtr,poolDataPtr,count,size,name);
}

IX_STATUS ipsec_hwaccel_OsBuffPoolUnchainedBufGet (
    IX_MBUF_POOL *poolPtr, 
    IX_MBUF **newBufPtrPtr)
{
    return getFunctionPtr(poolPtr,
			  newBufPtrPtr);
}

IX_MBUF *ipsec_hwaccel_OsBuffPoolBufFree (IX_MBUF *bufPtr)
{
    return freeFunctionPtr(bufPtr);
}

UINT32 ipsec_hwaccel_OsBuffPoolBufAreaSizeAligned(int count)
{
    return alignedFunction(count);
}

void *ipsec_hwaccel_OsServCacheDmaAlloc(UINT32 size)
{
    return allocDmaFunction(size);
}

void ipsec_hwaccel_OsServCacheDmaFree(void *ptr, UINT32 size)
{
    return freeDmaFunction(ptr, size);
}

