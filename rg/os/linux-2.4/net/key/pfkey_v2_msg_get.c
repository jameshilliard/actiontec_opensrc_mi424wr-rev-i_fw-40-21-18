/* $USAGI: pfkey_v2_msg_get.c,v 1.6.2.1 2002/09/03 08:35:31 mk Exp $ */
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
/*
 * This is a parse routine for a message of SADB_GET.
 */
#ifdef MODULE
#include <linux/module.h>
#  ifdef MODVERSIONS
#  include <linux/modversions.h>
#  endif
#endif

#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/random.h>
#include <linux/ipsec.h>

#include <net/pfkeyv2.h>
#include <net/pfkey.h>
#include <net/sadb.h>

#include "pfkey_v2_msg.h"
#include "sadb_utils.h" /* sockaddrtoa ultoa */
#define BUFSIZE 64

int sadb_msg_get_parse(struct sock *sk, struct sadb_msg *msg, struct sadb_msg **reply)
{
	int error = 0;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1], *reply_ext_msgs[SADB_EXT_MAX+1];
        struct sadb_sa *sa_msg = NULL;
        struct sadb_address *src = NULL;
        struct sadb_address *dst = NULL;
        struct ipsec_sa *sa_entry = NULL;
	struct sockaddr_storage saddr, daddr;

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	memset(reply_ext_msgs, 0, sizeof(reply_ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);

	if (error) {
		PFKEY_DEBUG("sadb_msg_detect_ext faild\n");
		goto err;
	}

	if (ext_msgs[SADB_EXT_SA] &&
	    ext_msgs[SADB_EXT_ADDRESS_SRC] &&
	    ext_msgs[SADB_EXT_ADDRESS_DST])
	{
		sa_msg  = (struct sadb_sa*)ext_msgs[SADB_EXT_SA];

                src = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_SRC];
                dst = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_DST];

                error = sadb_address_to_sockaddr( src, (struct sockaddr*)&saddr);
                if (error) {
                        PFKEY_DEBUG("src translation failed\n");
                        goto err;
                }
                error = sadb_address_to_sockaddr( dst, (struct sockaddr*)&daddr);
                if (error) {
                        PFKEY_DEBUG("dst translation failed\n");
                        goto err;
                }

		error = sadb_find_by_address_proto_spi((struct sockaddr*)&saddr, src->sadb_address_prefixlen,
					 (struct sockaddr*)&daddr, dst->sadb_address_prefixlen,
					 msg->sadb_msg_type,
					 sa_msg->sadb_sa_spi,
					 &sa_entry); 


		if (error == -EEXIST) {

			read_lock_bh(&sa_entry->lock);

			error=pfkey_sa_build(&reply_ext_msgs[SADB_EXT_SA], SADB_EXT_SA,
						sa_entry->spi, sa_entry->replay_window.size,
						sa_entry->state, sa_entry->auth_algo.algo,
						sa_entry->esp_algo.algo, SADB_SAFLAGS_PFS );
			if (error) {
				PFKEY_DEBUG("pfkey_sa_build faild\n");
				goto err;
			}

			error = lifetime_to_sadb_lifetime(&sa_entry->lifetime_c,
							(struct sadb_lifetime*)reply_ext_msgs[SADB_EXT_LIFETIME_CURRENT],
							SADB_EXT_LIFETIME_CURRENT);
			if (error) {
				PFKEY_DEBUG("lifetime_to_sadb_lifetime failed\n");
				goto err;
			}

			error = lifetime_to_sadb_lifetime(&sa_entry->lifetime_h,
							(struct sadb_lifetime*)reply_ext_msgs[SADB_EXT_LIFETIME_HARD],
							SADB_EXT_LIFETIME_HARD);
			if (error) {
				PFKEY_DEBUG("lifetime_to_sadb_lifetime failed\n");
				goto err;
			}

			error = lifetime_to_sadb_lifetime(&sa_entry->lifetime_s,
							(struct sadb_lifetime*)reply_ext_msgs[SADB_EXT_LIFETIME_SOFT],
							SADB_EXT_LIFETIME_SOFT);
			if (error) {
				PFKEY_DEBUG("lifetime_to_sadb_lifetime failed\n");
				goto err;
			}

			error = pfkey_address_build((struct sadb_ext**)&reply_ext_msgs[SADB_EXT_ADDRESS_SRC],
						SADB_EXT_ADDRESS_SRC,
						sa_entry->proto,
						sa_entry->prefixlen_s,
						(struct sockaddr*)&sa_entry->src);
			if (error) {
				PFKEY_DEBUG("pfkey_address_build faild\n");
				goto err;
			}

			error = pfkey_address_build((struct sadb_ext**)&reply_ext_msgs[SADB_EXT_ADDRESS_DST],
						SADB_EXT_ADDRESS_DST,
						sa_entry->proto,
						sa_entry->prefixlen_d,
						(struct sockaddr*)&sa_entry->dst);
			if (error) {
				PFKEY_DEBUG("pfkey_address_build faild\n");
				goto err;
			}

			if (sa_entry->ipsec_proto == SADB_SATYPE_AH) {
				error = pfkey_key_build((struct sadb_ext**)&reply_ext_msgs[SADB_EXT_KEY_AUTH],
						SADB_EXT_KEY_AUTH,
						sa_entry->auth_algo.key_len*sizeof(__u8),
						sa_entry->auth_algo.key);
				if (error) {
					PFKEY_DEBUG("pfkey_key_build faild\n");
					goto err;
				}
			}

			if (sa_entry->ipsec_proto == SADB_SATYPE_ESP) {
#ifndef CONFIG_IPSEC_DEBUG
					error = pfkey_key_build((struct sadb_ext**)&reply_ext_msgs[SADB_EXT_KEY_ENCRYPT],
						SADB_EXT_KEY_ENCRYPT,
						sa_entry->esp_algo.key_len*sizeof(__u8),
						(char*)sa_entry->esp_algo.cx->ci);
#else /* CONFIG_IPSEC_DEBUG */
				if (sa_entry->esp_algo.algo != SADB_EALG_NULL) {
					error = pfkey_key_build((struct sadb_ext**)&reply_ext_msgs[SADB_EXT_KEY_ENCRYPT],
						SADB_EXT_KEY_ENCRYPT,
						sa_entry->esp_algo.key_len*sizeof(__u8),
						(char*)sa_entry->esp_algo.cx->ci);
				} else {
					struct sadb_key *tmp_key_msg = kmalloc(sizeof(struct sadb_key), GFP_KERNEL);
					if (tmp_key_msg) {
						PFKEY_DEBUG("could not allocat tmp_key_msg");
						error = -ENOMEM;
						goto err;
					}
					tmp_key_msg->sadb_key_len = sizeof(struct sadb_key)/8;
					tmp_key_msg->sadb_key_exttype = SADB_EXT_KEY_ENCRYPT;
					tmp_key_msg->sadb_key_bits = 0;
					tmp_key_msg->sadb_key_reserved = 0;
					reply_ext_msgs[SADB_EXT_KEY_ENCRYPT] = (struct sadb_ext *)tmp_key_msg;
				}
#endif /* CONFIG_IPSEC_DEBUG */
				if (error) {
					PFKEY_DEBUG("pfkey_key_build faild\n");
					goto err;
				}
			}
			
			read_unlock_bh(&sa_entry->lock);
			ipsec_sa_put(sa_entry);

		} else {
			PFKEY_DEBUG("sadb_find_by_address_spi faild\n");
			goto err;
		}
	}

	reply_ext_msgs[0] = (struct sadb_ext*) msg;
	pfkey_msg_build(reply, reply_ext_msgs, EXT_BITS_OUT);
	pfkey_extensions_free(reply_ext_msgs);
err:
	return error;
}

