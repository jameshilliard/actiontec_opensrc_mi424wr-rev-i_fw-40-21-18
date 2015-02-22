/****************************************************************************
 *
 * rg/pkg/freeswan/pluto/rg_utils.c
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

#include <freeswan.h>
#include "rg_utils.h"

rg_ipsec_prop_t esp_enc[] = {
#ifdef CONFIG_IPSEC_ENC_NULL
    {OPENRG_ENC_NULL, ESP_NULL, 0, 0},
#endif
#ifdef CONFIG_IPSEC_ENC_1DES
    {OPENRG_ENC_1DES, ESP_DES, 0, 0},
#endif
#ifdef CONFIG_IPSEC_ENC_3DES
    {OPENRG_ENC_3DES, ESP_3DES, 0, 0},
#endif
#ifdef CONFIG_IPSEC_ENC_AES
    {OPENRG_ENC_AES128, ESP_AES, KEY_LENGTH, 128},
    {OPENRG_ENC_AES192, ESP_AES, KEY_LENGTH, 192},
    {OPENRG_ENC_AES256, ESP_AES, KEY_LENGTH, 256},
#endif
    {0, 0, 0, 0}
};

rg_ipsec_prop_t esp_auth[] = {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
    {OPENRG_AUTH_MD5, 0, AUTH_ALGORITHM, AUTH_ALGORITHM_HMAC_MD5},
#endif
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
    {OPENRG_AUTH_SHA, 0, AUTH_ALGORITHM, AUTH_ALGORITHM_HMAC_SHA1},
#endif
    {0, 0, 0, 0}
};

rg_ipsec_prop_t ah_auth[] = {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
    {OPENRG_HASH_MD5, AH_MD5, AUTH_ALGORITHM, AUTH_ALGORITHM_HMAC_MD5},
#endif
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
    {OPENRG_HASH_SHA1, AH_SHA, AUTH_ALGORITHM, AUTH_ALGORITHM_HMAC_SHA1},
#endif
    {0, 0, 0, 0}
};

static openrg_policy_t openrg_policy_get(rg_ipsec_prop_t *list,
    u_int8_t policy, u_int16_t attr)
{
    int i;

    for (i=0; list[i].openrg_policy; i++)
    {
	if ((!policy || list[i].transid==policy) &&
	    (!list[i].attr || list[i].attr==attr))
	{
	    break;
	}
    }
    return list[i].openrg_policy;
}

static openrg_policy_t openrg_enc_policy_get(u_int8_t transid,
    u_int16_t key_len)
{
    return openrg_policy_get(esp_enc, transid, key_len);
}

static openrg_policy_t openrg_auth_policy_get(u_int16_t pluto_auth)
{
    return openrg_policy_get(esp_auth, 0, pluto_auth);
}

int check_openrg_esp_policy(u_int8_t transid, u_int16_t key_len,
    u_int16_t auth, lset_t rg_policy)
{
    openrg_policy_t openrg_enc = openrg_enc_policy_get(transid, key_len);
    openrg_policy_t openrg_auth = openrg_auth_policy_get(auth);

    return (rg_policy&openrg_enc) && (rg_policy&openrg_auth);
}

int check_openrg_ah_policy(u_int16_t auth, lset_t rg_policy)
{
    return openrg_policy_get(ah_auth, 0, auth) & rg_policy;
}

