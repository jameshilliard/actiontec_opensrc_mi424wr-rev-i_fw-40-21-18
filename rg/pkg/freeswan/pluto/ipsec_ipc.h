/****************************************************************************
 *
 * rg/pkg/freeswan/pluto/ipsec_ipc.h
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

#ifndef _IPSEC_IPC_H_
#define _IPSEC_IPC_H_

#include <net/if.h>
#include <rg_types.h>
#include <ipsec_types.h>

/* Active (not connected) IPSec connection with any remote IP 
 * have name in format:
 * <ipsec_templ_name>_<wan_dev_name>
 */
#define IPSEC_CONN_NAME_SIZE (2*IFNAMSIZ+2)

typedef enum {
    PUBKEY = 0,
    MODULUS = 1,
    PUBLICEXPONENT = 2,
    PRIVATEEXPONENT = 3,
    PRIME1 = 4,
    PRIME2 = 5,
    EXPONENT1 = 6,
    EXPONENT2 = 7,
    COEFFICIENT = 8,
    RSA_SEC_ALL,
} rsa_secr_fields_t;

typedef enum {
    IPSEC_CMD_RSASIGKEY = 1,
    IPSEC_CMD_UP = 2,
    IPSEC_CMD_DOWN = 3,
    IPSEC_CMD_SECRETS_REQUEST = 4,
    IPSEC_CMD_CTRL_SK_OPEN = 5,
    IPSEC_CMD_DELETE_INSTANCE = 6,
} ipsec_proc_cmd_t;

typedef enum {
    IPSEC_PPK_NONE = 0,
    IPSEC_PPK_PSK = 1,
    IPSEC_PPK_RSA = 2,
    IPSEC_PPK_RSA_SECRET = 3,
    IPSEC_PPK_RSA_PRIVATE_KEY_FILE = 4,
} ipsec_secret_kind_t;

typedef struct {
    char conn_name[IPSEC_CONN_NAME_SIZE];
    char ipsec_dev[IFNAMSIZ];
    struct in_addr peer;
    rg_ip_range_t peer_client;
    struct in_addr next_hop;
    u32 instance;
} ipsec_conn_data_t;

#endif
