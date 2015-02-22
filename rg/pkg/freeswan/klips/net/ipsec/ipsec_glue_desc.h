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


#ifndef _IPSEC_GLUE_DESC_H
#define _IPSEC_GLUE_DESC_H

#include "ipsec_sa.h"
#include "IxTypes.h"

/*
 * #defines for function return types, etc.
 */

#define MAX_IPSEC_RCV_DESCRIPTORS_NUM_IN_POOL   384
#define MAX_IPSEC_XMIT_DESCRIPTORS_NUM_IN_POOL   384


/* Descriptor size for ipsec_rcv */
#define IPSEC_RCV_DESC_SIZE   (((sizeof (IpsecRcvDesc) +              \
                                  (IX_XSCALE_CACHE_LINE_SIZE - 1)) /  \
                                  IX_XSCALE_CACHE_LINE_SIZE) *        \
                                  IX_XSCALE_CACHE_LINE_SIZE )


/* Descriptor size for ipsec_tunnel */
#define IPSEC_XMIT_DESC_SIZE   (((sizeof (IpsecXmitDesc) +            \
                                  (IX_XSCALE_CACHE_LINE_SIZE - 1)) /  \
                                  IX_XSCALE_CACHE_LINE_SIZE) *        \
                                  IX_XSCALE_CACHE_LINE_SIZE )





/* ipsec_rcv transform descriptor */
typedef struct
{
    struct ipsec_sa *tdbp;
    struct net_device_stats *stats; /* This device's statistics */
    int hard_header_len;
    __u32 icv_offset;
    __u16 ip_frag_off;
    __u8 ip_ttl;
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
    __u8 natt_type;
#endif
} IpsecRcvDesc;


/* ipsec_tunnel transform descriptor */
typedef struct
{
    struct ipsec_sa *tdbp;
    struct net_device *dev;
    struct sa_id outgoing_said;
    struct sockaddr_encap matcher;  /* eroute search key */
    struct ipsec_sa tdb;
    struct sk_buff *oskb;   /* Original skb pointer */
    __u8 *saved_header;     /* saved copy of the hard header */
    int hard_header_len;
    __u32 orgedst;
    int pass;
    int tot_headroom;       /* The total header space needed */
    int tot_tailroom;       /* The totalstuffing needed */
    __u32 eroute_pid;
    __u16 ip_frag_off;
    __u8 ip_ttl;
    __u8 ip_tos;
#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
    uint8_t natt_type;
    uint8_t natt_head;
    uint16_t natt_sport;
    uint16_t natt_dport;
#endif
} IpsecXmitDesc;



/**
 * ipsec_glue_rcv_desc_init
 *
 * Initialize ipsec_rcv descriptor management module.
 *
 * Param : None
 *
 * Return : None
 *
 */
int
ipsec_glue_rcv_desc_init (void);


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
ipsec_glue_rcv_desc_get (IpsecRcvDesc **pIpsecRcvDescPtr);


/**
 * ipsec_glue_rcv_desc_release
 *
 * Release descriptor previously allocated back to the ipsec_rcv
 * descriptor pool
 *
 * pIpsecRcvDesc [in] - Pointer to ipsec_rcv descriptor
 *
 * Return : 0 - Success
 *          1 - Fail
 *
 */
int 
ipsec_glue_rcv_desc_release (IpsecRcvDesc *pIpsecRcvDesc);



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
ipsec_rcv_desc_pool_free (void);


/**
 * ipsec_glue_xmit_desc_init
 *
 * Initialize ipsec_xmit descriptor management module.
 *
 * Param : None
 *
 * Return : None
 *
 */
int
ipsec_glue_xmit_desc_init (void);



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
ipsec_glue_xmit_desc_get (IpsecXmitDesc **pIpsecXmitDescPtr);



/**
 * ipsec_glue_xmit_desc_release
 *
 * Release descriptor previously allocated back to the ipsec_xmit
 * descriptor pool
 *
 * Param : pIpsecXmitDesc [in] - Pointer to ipsec_xmit descriptor
 *
 * Return : 0 - Success
 *          1 - Fail
 *
 */
int
ipsec_glue_xmit_desc_release (IpsecXmitDesc *pIpsecXmitDesc);


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
ipsec_xmit_desc_pool_free (void);



#endif /* _IPSEC_GLUE_DESC_H */
