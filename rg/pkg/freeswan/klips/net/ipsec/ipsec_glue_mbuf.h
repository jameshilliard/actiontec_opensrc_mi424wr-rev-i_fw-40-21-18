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

#ifndef _IPSEC_GLUE_MBUF_H
#define _IPSEC_GLUE_MBUF_H

#include "IxOsBuffMgt.h"
#include "IxOsBuffPoolMgt.h"

/* Maximum mbuf header allocate for IPSec driver */
#define IPSEC_GLUE_MBUF_HEADER_COUNT    384

/* Maximum mbufs allocate for IPSec driver */
#define IPSEC_GLUE_MBUF_COUNT           256

/* Size of mdata in mbuf */
#define IPSEC_GLUE_MBUF_DATA_SIZE       128


/* IPSec cryptoAcc return status */
#define IPSEC_BUSY              0
#define IPSEC_SUCCESS           1
#define IPSEC_FAIL              2
#define IPSEC_AUTH_FAIL         3

/* ICV location in AH */
#define AUTH_DATA_IN_AH_OFFSET  12

/*
 * Initialize mbufs header pool
 * The mbuf pool will have maximum IPSEC_GLUE_MBUF_HEADER_COUNT of mbufs. The mbuf header do not have
 * the mdata pointer attached to it.
 *
 * Param: None
 *
 * Return:None
 *
 */
void ipsec_glue_mbuf_header_init (void);

/*
 * Release mbufs header pool
 *
 * Param: None
 *
 * Return:None
 *
 */
void ipsec_glue_mbuf_header_release (void);

/*
 * Get mbuf header from buf pool
 *
 * Param: pMbufPtr [out] pointer to the mbuf pointer
 *
 * Return: 0 - success
 *         1 - fail
 *
 */
int ipsec_glue_mbuf_header_get (IX_MBUF **pMbufPtr);


/*
 * Release mbuf header back into mbuf pool
 *
 * Param: pMbuf [in] mbuf pointer to be release back to the pool
 *
 * Return: None
 *
 */
void ipsec_glue_mbuf_header_rel (IX_MBUF *pMbuf);


/*
 * Initialize mbufs pool
 * The mbuf pool will have maximum IPSEC_GLUE_MBUF_COUNT of mbufs with mdata pointer attached to it.
 *
 * Param: None
 *
 * Return:None
 *
 */
void ipsec_glue_mbuf_init (void);

/*
 * Release mbufs pool
 *
 * Param: None
 *
 * Return:None
 *
 */
void ipsec_glue_mbuf_release (void);


/*
 * Get mbuf header from buf pool
 *
 * Param: pMbufPtr [out] pointer to the mbuf pointer
 *
 * Return: 0 - success
 *         1 - fail
 *
 */
int ipsec_glue_mbuf_get (IX_MBUF **pMbufPtr);


/*
 * Release mbuf back into mbuf pool
 *
 * Param: pMbuf [in] mbuf pointer to be release back to the pool
 *
 * Return: None
 *
 */
void ipsec_glue_mbuf_rel (IX_MBUF *pMbuf);



#endif /*_IPSEC_GLUE_MBUF_H */
