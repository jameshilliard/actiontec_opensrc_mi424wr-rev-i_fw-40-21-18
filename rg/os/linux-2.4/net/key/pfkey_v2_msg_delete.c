/* $USAGI: pfkey_v2_msg_delete.c,v 1.2.2.1 2002/09/03 08:35:31 mk Exp $ */
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
 * This is a parse routine for a message of SADB_DELETE.
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

int sadb_msg_delete_parse(struct sock *sk, struct sadb_msg *msg, struct sadb_msg **reply)
{
	int error = 0;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1], *reply_ext_msgs[SADB_EXT_MAX+1];
	struct sadb_sa reply_sa;
	struct sadb_sa *sa = NULL;
	struct sadb_address *src = NULL;
	struct sadb_address *dst = NULL;
	struct sockaddr_storage saddr, daddr;
	struct ipsec_sa *ipsec_sa = NULL;
	uint8_t prefixlen_s, prefixlen_d;

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	memset(reply_ext_msgs, 0, sizeof(reply_ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);
	if (error) {
		PFKEY_DEBUG("error in sadb_msg_detect_ext\n");
		goto err;
	}

	if (ext_msgs[SADB_EXT_SA] &&
		ext_msgs[SADB_EXT_ADDRESS_SRC] &&
		ext_msgs[SADB_EXT_ADDRESS_DST])
	{
		sa  = (struct sadb_sa*)ext_msgs[SADB_EXT_SA];

		src = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_SRC];
		dst = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_DST];

		memset(&saddr, 0, sizeof(struct sockaddr_storage));
		memset(&daddr, 0, sizeof(struct sockaddr_storage));

		error = sadb_address_to_sockaddr(src, (struct sockaddr*)&saddr);
		if (error) {
			PFKEY_DEBUG("src translate\n");
			goto err;
		}
		error = sadb_address_to_sockaddr(dst, (struct sockaddr*)&daddr);
		if (error) {
			PFKEY_DEBUG("dst translate\n");
			goto err;
		}

		prefixlen_s = src->sadb_address_prefixlen;
		prefixlen_d = dst->sadb_address_prefixlen;

		error = sadb_find_by_address_proto_spi( (struct sockaddr*)&saddr, prefixlen_s,
							(struct sockaddr*)&daddr, prefixlen_d,
							msg->sadb_msg_satype,
							sa->sadb_sa_spi,
							&ipsec_sa);

		if (error == -EEXIST) {
			ipsec_sa_put(ipsec_sa);
			sadb_remove(ipsec_sa);
			error = 0;
		}else{
			error = -ESRCH;
			goto err;
		}
	}

	memset(&reply_sa, 0, sizeof(struct sadb_sa));
	reply_sa.sadb_sa_len = sizeof(struct sadb_sa) / 8;
	reply_sa.sadb_sa_exttype = SADB_EXT_SA;
	reply_sa.sadb_sa_spi = sa->sadb_sa_spi;
	reply_ext_msgs[0] = (struct sadb_ext*) msg;
	reply_ext_msgs[SADB_EXT_SA] = (struct sadb_ext*)&reply_sa;
	reply_ext_msgs[SADB_EXT_ADDRESS_SRC] = ext_msgs[SADB_EXT_ADDRESS_SRC];
	reply_ext_msgs[SADB_EXT_ADDRESS_DST] = ext_msgs[SADB_EXT_ADDRESS_DST];
	error = pfkey_msg_build(reply, reply_ext_msgs, EXT_BITS_OUT);
err:
	return error;
}


