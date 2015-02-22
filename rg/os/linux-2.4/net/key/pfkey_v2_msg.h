/* $USAGI: pfkey_v2_msg.h,v 1.3 2002/01/15 16:54:55 mk Exp $ */
/*
 * Copyright (C)2001 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PFKEY_V2_MSG_H
#define __PFKEY_V2_MSG_H

#include <net/pfkeyv2.h>
#include <net/sadb.h>

#define ESP_DES_KEY_BITS	64
#define ESP_3DES_KEY_BITS	192
#define ESP_NULL_KEY_BITS	0
#define ESP_AES_KEY_BITS	128

#define AUTH_MD5HMAC_KEY_BITS 128
#define AUTH_SHA1HMAC_KEY_BITS 160 

/* thease codes are derived from FreeS/WAN */
#define IPSEC_PFKEYv2_ALIGN (sizeof(uint64_t)/sizeof(uint8_t))
#define BITS_PER_OCTET 8
#define OCTETBITS 8
#define PFKEYBITS 64
#define DIVUP(x,y) ((x + y -1) / y) /* divide, rounding upwards */
#define ALIGN_N(x,y) (DIVUP(x,y) * y) /* align on y boundary */

#define PFKEYv2_MAX_MSGSIZE 4096


/* utils */
int sadb_msg_sanity_check(struct sadb_msg* msg);
int sadb_address_to_sockaddr(const struct sadb_address *ext_msg, struct sockaddr *addr);
int sadb_key_to_esp(const __u8 esp_algo, const struct sadb_key* ext_msg, struct ipsec_sa *sa_entry);
int sadb_key_to_auth(const __u8 auth_algo, const struct sadb_key *ext_msg, struct ipsec_sa *sa_entry);
int sadb_lifetime_to_lifetime(const struct sadb_lifetime* ext_msg, struct sa_lifetime *lifetime);
int sadb_msg_detect_ext(struct sadb_msg* msg, struct sadb_ext **ext_msgs);
int lifetime_to_sadb_lifetime(struct sa_lifetime *lifetime, struct sadb_lifetime *ext_msg, int type);

/* parsers */
int sadb_msg_getspi_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_update_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_add_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_delete_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_get_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_acquire_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_register_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_expire_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_flush_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_flush_sp_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_dump_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_addflow_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);
int sadb_msg_delflow_parse(struct sock* sk, struct sadb_msg* msg, struct sadb_msg **reply);

/* send message */
int sadb_msg_send_expire(struct ipsec_sa* sa);
int sadb_msg_send_acquire(struct ipsec_sa* sa);
#endif /* __PFKEY_V2_MSG_H */

