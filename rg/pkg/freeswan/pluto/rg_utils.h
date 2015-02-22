/****************************************************************************
 *
 * rg/pkg/freeswan/pluto/rg_utils.h
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#ifndef __RG_EXT_H__
#define __RG_EXT_H__

#include <freeswan.h>
#include "constants.h"

#define RG_LELEM(opt) (1UL << (opt))

typedef enum {
    IKE_DEBUG_RAW = DBG_RAW,
    IKE_DEBUG_CRYPT = DBG_CRYPT,
    IKE_DEBUG_PARSING = DBG_PARSING,
    IKE_DEBUG_EMITTING = DBG_EMITTING,
    IKE_DEBUG_CONTROL = DBG_CONTROL,
    IKE_DEBUG_KLIPS = DBG_KLIPS,
    IKE_DEBUG_DPD = DBG_DPD,
    IKE_DEBUG_NATT = DBG_NATT,
    IKE_DEBUG_IKE_REJECT = DBG_IKE_REJECT,
    IKE_DEBUG_PRIVATE = DBG_PRIVATE,
    IKE_DEBUG_LOG_ALL = DBG_LOG_ALL,
} ike_debug_t;

typedef enum {
    KLIPS_DEBUG_TUNNEL = RG_LELEM(0),
    KLIPS_DEBUG_TUNNEL_XMIT = RG_LELEM(1),
    KLIPS_DEBUG_PFKEY = RG_LELEM(2),
    KLIPS_DEBUG_XFORM = RG_LELEM(3),
    KLIPS_DEBUG_EROUTE = RG_LELEM(4),
    KLIPS_DEBUG_SPI = RG_LELEM(5),
    KLIPS_DEBUG_RADIJ = RG_LELEM(6),
    KLIPS_DEBUG_ESP = RG_LELEM(7),
    KLIPS_DEBUG_AH = RG_LELEM(8),
    KLIPS_DEBUG_RCV = RG_LELEM(9),
    KLIPS_DEBUG_IPCOMP = RG_LELEM(10),
    KLIPS_DEBUG_VERBOSE = RG_LELEM(11),
    KLIPS_DEBUG_REJECT = RG_LELEM(12),
    KLIPS_DEBUG_LOG_ALL = RG_LELEM(13),
} klips_debug_t;

typedef enum {
    OPENRG_AUTH_MD5 = RG_LELEM(0),
    OPENRG_AUTH_SHA = RG_LELEM(1),
    OPENRG_ENC_NULL = RG_LELEM(2),
    OPENRG_ENC_1DES = RG_LELEM(3),
    OPENRG_ENC_3DES = RG_LELEM(4),
    OPENRG_ENC_AES128 = RG_LELEM(5),
    OPENRG_ENC_AES192 = RG_LELEM(6),
    OPENRG_ENC_AES256 = RG_LELEM(7),
    OPENRG_MODP_768 = RG_LELEM(8),
    OPENRG_MODP_1024 = RG_LELEM(9),
    OPENRG_MODP_1536 = RG_LELEM(10),
    OPENRG_HASH_MD5 = RG_LELEM(11),
    OPENRG_HASH_SHA1 = RG_LELEM(12),
} openrg_policy_t;

typedef struct rg_ipsec_prop_t {
    lset_t openrg_policy;
    u_int8_t transid;
    u_int16_t attr_type;
    u_int16_t attr;
} rg_ipsec_prop_t;

extern rg_ipsec_prop_t esp_enc[];
extern rg_ipsec_prop_t esp_auth[];
extern rg_ipsec_prop_t ah_auth[];

int check_openrg_esp_policy(u_int8_t transid, u_int16_t key_len, u_int16_t auth,
    lset_t policy);
int check_openrg_ah_policy(u_int16_t auth, lset_t rg_policy);
int esp_is_valid_openrg_prop(u_int8_t transid, u_int16_t auth,
    u_int16_t key_len, lset_t openrg_quick_policy);

#endif

