/*
 * IPSEC_HW interface code.
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

#ifndef _IPSEC_HWACCEL_H
#define _IPSEC_HWACCEL_H

/*
 * NOTE: This file captures the dependency between FreeSWAN and underlying HW
 * Acceleration functionality.
 */

#include <IxTypes.h>
#include <IxCryptoAcc.h> 
#include <IxOsBuffMgt.h> 
#include <IxOsBuffPoolMgt.h> 

typedef IxCryptoAccStatus (*HwAccelRegisterFunction) (
    IxCryptoAccCtx *pAccCtx,
    IX_MBUF *pMbufPrimaryChainVar,
    IX_MBUF *pMbufSecondaryChainVar,
    IxCryptoAccRegisterCompleteCallback registerCallbackFn,
    IxCryptoAccPerformCompleteCallback performCallbackFn,
    UINT32 *pCryptoCtxId);

typedef IxCryptoAccStatus (*HwAccelUnregisterFunction) (UINT32 cryptoCtxId);

typedef IxCryptoAccStatus (*HwAccelPerformFunction)  (
    UINT32 cryptoCtxId,
    IX_MBUF *pSrcMbuf,
    IX_MBUF *pDestMbuf,
    UINT16 authStartOffset,
    UINT16 authDataLen,    
    UINT16 cryptStartOffset,   
    UINT16 cryptDataLen,
    UINT16 icvOffset,
    UINT8  *pIV);

typedef IX_STATUS (*HwAccelOsBuffPoolInit) (
    IX_MBUF_POOL **poolPtrPtr,
    int count,
    int size,
    char *name);

typedef IX_STATUS (*HwAccelOsBuffPoolInitNoAlloc) (
    IX_MBUF_POOL **poolPtrPtr, 
    void *poolBufPtr, 
    void *poolDataPtr, 
    int count, 
    int size, 
    char *name);

typedef IX_STATUS (*HwAccelOsBuffPoolUnchainedBufGet) (
    IX_MBUF_POOL *poolPtr, 
    IX_MBUF **newBufPtrPtr);

typedef IX_MBUF * (*HwAccelOsBuffPoolBufFree) (IX_MBUF *bufPtr);

typedef UINT32 (*HwAccelOsBuffPoolBufAreaSizeAligned)(int count);

typedef void * (*HwAccelOsServCacheDmaAlloc) (UINT32 size);

typedef void (*HwAccelOsServCacheDmaFree) (void *ptr, UINT32 size);

/* ***************************************************************************
 *
 *                      EXTERNAL (NON-FREESWAN) FUNCTIONS
 *
 * ***************************************************************************/

/*
 * Register real HW Accel code with FreeSWAN stack.
 * 
 * Param: Function Pointers for register, unregister and perform functions
 * that must function as per description in IxCryptoAcc.h 
 */
void ipsec_hwaccel_add_service(HwAccelRegisterFunction registerFunc,
			       HwAccelUnregisterFunction unregisterFunc,
			       HwAccelPerformFunction performFunc,
			       HwAccelOsBuffPoolInit initFunc,
			       HwAccelOsBuffPoolInitNoAlloc initNoAllocFunc,
			       HwAccelOsBuffPoolUnchainedBufGet getFunc,
			       HwAccelOsBuffPoolBufFree freeFunc,
			       HwAccelOsBuffPoolBufAreaSizeAligned alignedFunc,
			       HwAccelOsServCacheDmaAlloc allocDmaFunc,
			       HwAccelOsServCacheDmaFree freeDmaFunc);


/* ***************************************************************************
 *
 *                      INTERNAL (FREESWAN) FUNCTIONS
 *
 * ***************************************************************************/

/*
 * Functions are mirrors of those specfied in IxCryptoAcc.h
 */
BOOL ipsec_hwaccel_ready(void);

IxCryptoAccStatus ipsec_hwaccel_register(
    IxCryptoAccCtx *pAccCtx,
    IX_MBUF *pMbufPrimaryChainVar,
    IX_MBUF *pMbufSecondaryChainVar,
    IxCryptoAccRegisterCompleteCallback registerCallbackFn,
    IxCryptoAccPerformCompleteCallback performCallbackFn,
    UINT32 *pCryptoCtxId);

IxCryptoAccStatus ipsec_hwaccel_unregister (UINT32 cryptoCtxId);

IxCryptoAccStatus ipsec_hwaccel_perform (
    UINT32 cryptoCtxId,
    IX_MBUF *pSrcMbuf,
    IX_MBUF *pDestMbuf,
    UINT16 authStartOffset,
    UINT16 authDataLen,    
    UINT16 cryptStartOffset,   
    UINT16 cryptDataLen,
    UINT16 icvOffset,
    UINT8  *pIV);

IX_STATUS ipsec_hwaccel_OsBuffPoolInit (
    IX_MBUF_POOL **poolPtrPtr,
    int count,
    int size,
    char *name);

IX_STATUS ipsec_hwaccel_OsBuffPoolInitNoAlloc (
    IX_MBUF_POOL **poolPtrPtr, 
    void *poolBufPtr, 
    void *poolDataPtr, 
    int count, 
    int size, 
    char *name);

IX_STATUS ipsec_hwaccel_OsBuffPoolUnchainedBufGet (
    IX_MBUF_POOL *poolPtr, 
    IX_MBUF **newBufPtrPtr);

IX_MBUF *ipsec_hwaccel_OsBuffPoolBufFree (IX_MBUF *bufPtr);


UINT32 ipsec_hwaccel_OsBuffPoolBufAreaSizeAligned(int count);

void *ipsec_hwaccel_OsServCacheDmaAlloc(UINT32 size);

void ipsec_hwaccel_OsServCacheDmaFree(void *ptr, UINT32 size);

#endif /* _IPSEC_HWACCEL_H */

