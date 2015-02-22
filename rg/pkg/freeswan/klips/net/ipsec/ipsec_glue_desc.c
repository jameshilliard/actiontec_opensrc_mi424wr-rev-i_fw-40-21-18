/*
 * IPSEC_GLUE_DESC interface code.
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
 
/*
 * Put the user defined include files required.
 */
#include "IxTypes.h"
#include "IxOsCacheMMU.h"
#include "IxOsServices.h"
#include <linux/in.h>
#include "ipsec_glue_desc.h"
#include "ipsec_hwaccel.h"
#include "ipsec_log.h"

#include <asm/system.h>

/*
 * Variable declarations global to this file only.  Externs are followed by
 * static variables.
 */
static IpsecRcvDesc* ipsecRcvDescList[MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL];  
                                 /**< array of ipsec_rcv descriptors pointers,
                                  * this array is used as a FILO push-pop 
                                  * list in descriptor get and release 
                                  * funciton
                                  */
static UINT32 ipsecRcvDescCount = 0;   /**< descriptor index to ipsecRcvDescList 
                                         * array (descriptor list head 
                                         * pointer)
                                         */
static INT32 ipsecRcvDescMgmtLock;       /**< Protect critical sections in this 
                                          * module
                                          */
static UINT8 *pIpsecRcvDescPool = NULL;  /**< ipsec_rcv descriptor pool pointer */

static IpsecXmitDesc* ipsecXmitDescList[MAX_IPSEC_XMIT_DESCRIPTORS_NUM_IN_POOL];  
                                 /**< array of ipsec_xmit descriptors pointers,
                                  * this array is used as a FILO push-pop 
                                  * list in descriptor get and release 
                                  * funciton
                                  */
static UINT32 ipsecXmitDescCount = 0;   /**< descriptor index to ipsecXmitDescList 
                                         * array (descriptor list head 
                                         * pointer)
                                         */
static INT32 ipsecXmitDescMgmtLock;       /**< Protect critical sections in this 
                                           * module
                                           */
static UINT8 *pIpsecXmitDescPool = NULL;  /**< ipsec_xmit descriptor pool pointer */


PUBLIC int
ipsec_glue_IntLock()
{
    int flags;
    save_flags(flags);
    cli();
    return flags;
}

PUBLIC void
ipsec_glue_IntUnlock(int lockKey)
{
    restore_flags(lockKey);
}


/**
 * ipsec_glue_rcv_desc_init
 *
 * Initialize ipsec_rcv descriptor management module. 
 *
 */
int
ipsec_glue_rcv_desc_init (void)
{
    UINT8 *pRcvDescPool = NULL;
    UINT32 i;

    /* Allocate memory to Rcv Descriptors Pool */
    pRcvDescPool = ipsec_hwaccel_OsServCacheDmaAlloc(
                      MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL
                      * IPSEC_RCV_DESC_SIZE);

    if (NULL == pRcvDescPool) /* if NULL */
    {
        /* Memory allocation failed */
        return 1;
    } /* end of if (pRcvDescPool) */

    pIpsecRcvDescPool = pRcvDescPool;

    for (i = 0;
        i < MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL;
        i++)
    {
        /* Allocate a descriptor from the Rcv Descriptors Pool
         * Assign Rcv descriptor to ipsecRcvDescList * Assign Q descriptor to qDescList
         */
        ipsecRcvDescList[i] = (IpsecRcvDesc *) pRcvDescPool;

        /* Move descriptor pool pointer to point to next
         * descriptor element
         */
        pRcvDescPool += IPSEC_RCV_DESC_SIZE;
    } /* end of for (i) */

    /* Set ipsecRcvDescList head pointer to last element in the list */
    ipsecRcvDescCount = MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL;

    return 0;

} /* end of ipsec_glue_rcv_desc_init () function */


/**
 * ipsec_glue_rcv_desc_get
 *
 * Get descriptor from the ipsec_rcv descriptor pool.
 *
 * Param : pIpsecRcvDescPtr [out] - Pointer to ipsec_rcv descriptor pointer
 *
 * Return : 0 - Success
 *          1 - Fail
 *
 */
int
ipsec_glue_rcv_desc_get (IpsecRcvDesc **pIpsecRcvDescPtr)
{
    int status;

    /* Lock ipsecRcvDescList to protect critical section */
    ipsecRcvDescMgmtLock = ipsec_glue_IntLock();

    if (ipsecRcvDescCount > 0) /* Unused descriptor available */
    {
        /* Get a descriptor from the pool */
        *pIpsecRcvDescPtr = ipsecRcvDescList[--ipsecRcvDescCount];

        (*pIpsecRcvDescPtr)->stats = NULL;

        status = 0;
    }
    else /* ipsecRcvDescCount <= 0 */
    {
        /* Run out of descriptor in the pool */
        status = 1;
    } /* end of if-else (ipsecRcvDescCount) */

    /* Unlock given mutex */
    ipsec_glue_IntUnlock(ipsecRcvDescMgmtLock);

    return status;
}


/**
 * ipsec_glue_rcv_desc_release
 *
 * Release descriptor previously allocated back to the ipsec_rcv
 * descriptor pool
 *
 */
int
ipsec_glue_rcv_desc_release (IpsecRcvDesc *pIpsecRcvDesc)
{
    int status;

    /* Lock ipsecRcvDescList to protect critical section */
    ipsecRcvDescMgmtLock = ipsec_glue_IntLock();

    if (ipsecRcvDescCount < MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL)
    {
        /* Push the  descriptor back to the pool */
        ipsecRcvDescList[ipsecRcvDescCount++] = pIpsecRcvDesc;

        status = 0;
    }
    else /* ipsecRcvDescCount >= MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL */
    {
        /* Descriptor Pool overflow */
        status = 1;
    } /* end of if-else (ipsecRcvDescCount) */

    /* Unlock given mutex */
    ipsec_glue_IntUnlock(ipsecRcvDescMgmtLock);

    return status;
}




/**
 * ipsec_rcv_desc_pool_free
 *
 * To free the memory allocated to descriptor pool through malloc
 *        function.
 *
 * Param : None
 * Return :  None
 *
 */
void
ipsec_rcv_desc_pool_free (void)
{
    /* Check if the pool has been allocated */
    if (NULL != pIpsecRcvDescPool)
    {
        /* free memory allocated */
       ipsec_hwaccel_OsServCacheDmaFree(pIpsecRcvDescPool,
				    MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL
                      * IPSEC_RCV_DESC_SIZE);

        ipsec_log("IPsec Xmit Descriptor pool has been freed.\n");
    } /* end of (pXmitDescPool) */
}



/**
 * ipsec_glue_xmit_desc_init
 *
 * Initialize ipsec_xmit descriptor management module.
 *
 */
int
ipsec_glue_xmit_desc_init (void)
{
    UINT8 *pXmitDescPool = NULL;
    UINT32 i;

    /* Allocate memory to Xmit Descriptors Pool */
    pXmitDescPool = ipsec_hwaccel_OsServCacheDmaAlloc(
                      MAX_IPSEC_XMIT_DESCRIPTORS_NUM_IN_POOL
                      * IPSEC_XMIT_DESC_SIZE);

    pIpsecXmitDescPool = pXmitDescPool;

    if (NULL == pXmitDescPool) /* if NULL */
    {
        /* Memory allocation failed */
        return 1;
    } /* end of if (pXmitDescPool) */

    for (i = 0;
        i < MAX_IPSEC_XMIT_DESCRIPTORS_NUM_IN_POOL;
        i++)
    {
        /* Allocate a descriptor from the Xmit Descriptors Pool
         * Assign Xmit descriptor to ipsecXmitDescList
         */
        ipsecXmitDescList[i] = (IpsecXmitDesc *) pXmitDescPool;

        /* Move descriptor pool pointer to point to next
         * descriptor element
         */
        pXmitDescPool += IPSEC_XMIT_DESC_SIZE;
    } /* end of for (i) */

    /* Set ipsecRcvDescList head pointer to last element in the list */
    ipsecXmitDescCount = MAX_IPSEC_XMIT_DESCRIPTORS_NUM_IN_POOL;

    return 0;
}



/**
 * ipsec_glue_xmit_desc_get
 *
 * Get descriptor from the ipsec_xmit descriptor pool.
 *
 * Param : pIpsecXmitDescPtr [out] - Pointer to ipsec_xmit descriptor pointer
 *
 * Return : 0 - Success
 *          1 - Fail
 *
 */
int
ipsec_glue_xmit_desc_get (IpsecXmitDesc **pIpsecXmitDescPtr)
{
    int status;

    /* Lock ipsecRcvDescList to protect critical section */
    ipsecXmitDescMgmtLock = ipsec_glue_IntLock();

    if (ipsecXmitDescCount > 0) /* Unused descriptor available */
    {
        /* Get a descriptor from the pool */
        *pIpsecXmitDescPtr = ipsecXmitDescList[--ipsecXmitDescCount];

        (*pIpsecXmitDescPtr)->tot_headroom = 0;
        (*pIpsecXmitDescPtr)->tot_tailroom = 0;
        (*pIpsecXmitDescPtr)->saved_header = NULL;
        (*pIpsecXmitDescPtr)->oskb = NULL;
        (*pIpsecXmitDescPtr)->pass = 0;
        memset((char*)&((*pIpsecXmitDescPtr)->tdb), 0, sizeof(struct ipsec_sa));

        status = 0;
    }
    else /* ipsecXmitDescCount <= 0 */
    {
        /* Run out of descriptor in the pool */
        status = 1;
    } /* end of if-else (ipsecXmitDescCount) */

    /* Unlock given mutex */
    ipsec_glue_IntUnlock(ipsecXmitDescMgmtLock);

    return status;
}



/**
 * ipsec_glue_xmit_desc_release
 *
 * Release descriptor previously allocated back to the ipsec_xmit
 * descriptor pool
 *
 * pIpsecXmitDesc [in] - Pointer to ipsec_xmit descriptor
 *
 * Return : 0 - Success
 *          1 - Fail
 *
 */
int
ipsec_glue_xmit_desc_release (IpsecXmitDesc *pIpsecXmitDesc)
{
    int status;

    /* Lock ipsecXmitDescList to protect critical section */
    ipsecXmitDescMgmtLock = ipsec_glue_IntLock();

    if (ipsecXmitDescCount < MAX_IPSEC_XMIT_DESCRIPTORS_NUM_IN_POOL)
    {
        /* Push the  descriptor back to the pool */
        ipsecXmitDescList[ipsecXmitDescCount++] = pIpsecXmitDesc;

        status = 0;
    }
    else /* ipsecXmitDescCount >= MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL */
    {
        /* Descriptor Pool overflow */
        status = 1;
    } /* end of if-else (ipsecXmitDescCount) */

    /* Unlock given mutex */
    ipsec_glue_IntUnlock(ipsecXmitDescMgmtLock);

    return status;
}



/**
 * ipsec_xmit_desc_pool_free
 *
 * To free the memory allocated to descriptor pool through malloc
 *        function.
 *
 * Param : None
 * Return :  None
 *
 */
void
ipsec_xmit_desc_pool_free (void)
{
    /* Check if the pool has been allocated */
    if (NULL != pIpsecXmitDescPool)
    {
        /* free memory allocated */
        ipsec_hwaccel_OsServCacheDmaFree(pIpsecXmitDescPool,
				    MAX_IPSEC_XMIT_DESCRIPTORS_NUM_IN_POOL
                      * IPSEC_XMIT_DESC_SIZE);

        ipsec_log("IPsec Xmit Descriptor pool has been freed.\n");
    } /* end of (pXmitDescPool) */
}
