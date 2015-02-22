/*
 * IPSEC_GLUE_MBUF interface code.
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

#include "IxOsBuffMgt.h"
#include "IxOsBuffPoolMgt.h"
#include "ipsec_glue_mbuf.h"
#include "ipsec_hwaccel.h"


IX_MBUF_POOL *pIpsecMbufHdrPool; /* mbuf header pool pointer */
IX_MBUF       *pIpsecMbufHdr;
UINT32 	      mbufHdrAreaMemSize;

IX_MBUF_POOL *pIpsecMbufPool;   /* Mbuf pool pointer */

/* Initialize mbuf header pool */
void ipsec_glue_mbuf_header_init (void)
{
    pIpsecMbufHdr = ipsec_hwaccel_OsServCacheDmaAlloc((mbufHdrAreaMemSize =
	ipsec_hwaccel_OsBuffPoolBufAreaSizeAligned(IPSEC_GLUE_MBUF_HEADER_COUNT)));

    /* initialize mbuf header pool */
    ipsec_hwaccel_OsBuffPoolInitNoAlloc (
        &pIpsecMbufHdrPool,
        pIpsecMbufHdr,
        NULL,
	    IPSEC_GLUE_MBUF_HEADER_COUNT,
        0,
        "IXP425 IPSec driver Mbuf Header Pool");
}

/* Release mbuf header pool */
void ipsec_glue_mbuf_header_relase (void)
{
	/* 
	 * TO DO: CSR does not implement to free the mbuf pool once the implement 
	 * are ready the functional is called here. The pool is only free when the ipsec 
	 * pplication is closed.  
	 */ 
}

/* Get mbuf from mbuf header pool */
int ipsec_glue_mbuf_header_get (IX_MBUF **pMbufPtr)
{
    if ((ipsec_hwaccel_OsBuffPoolUnchainedBufGet(pIpsecMbufHdrPool, pMbufPtr)) == IX_SUCCESS)
        return 0;
    else
        return 1;
}


/* Release mbuf back into mbuf header pool */
void ipsec_glue_mbuf_header_rel (IX_MBUF *pMbuf)
{
    ipsec_hwaccel_OsBuffPoolBufFree(pMbuf);
}


/* Initialize mbuf pool */
void ipsec_glue_mbuf_init (void)
{

    /* initialize mbuf pool */
    ipsec_hwaccel_OsBuffPoolInit(
        &pIpsecMbufPool,
	IPSEC_GLUE_MBUF_COUNT,
        IPSEC_GLUE_MBUF_DATA_SIZE,
        "IXP425 IPSec driver Mbuf Pool");
}

/* release mbuf pool */
void ipsec_glue_mbuf_release (void)
{
	/* 
	 * TO DO: CSR does not implement to free the mbuf pool once the implement 
	 * are ready the functional is called here. The pool is only free when the ipsec 
	 * pplication is closed.  
	 */ 
}

/* Get mbuf from mbuf pool */
int ipsec_glue_mbuf_get (IX_MBUF **pMbufPtr)
{
    if ((ipsec_hwaccel_OsBuffPoolUnchainedBufGet(pIpsecMbufPool, pMbufPtr)) == IX_SUCCESS)
        return 0;
    else
        return 1;
}


/* Release mbuf back into mbuf pool */
void ipsec_glue_mbuf_rel (IX_MBUF *pMbuf)
{
    ipsec_hwaccel_OsBuffPoolBufFree(pMbuf);
}
