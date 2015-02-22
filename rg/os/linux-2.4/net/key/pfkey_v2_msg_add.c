/* $USAGI: pfkey_v2_msg_add.c,v 1.9.2.2 2002/09/03 11:11:20 mk Exp $ */
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
 * This is a parse routine for a message of SADB_ADD.
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

int sadb_msg_add_parse(struct sock *sk, struct sadb_msg *msg, struct sadb_msg **reply)
{
	int error = 0;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1], *reply_ext_msgs[SADB_EXT_MAX+1];
	struct sadb_sa *sa;
	struct sadb_address *src;
	struct sadb_address *dst;
	struct ipsec_sa *sa_entry = NULL;

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto rtn;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	memset(reply_ext_msgs, 0, sizeof(reply_ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);

	if (ext_msgs[SADB_EXT_SA] &&
		ext_msgs[SADB_EXT_ADDRESS_SRC] &&
		ext_msgs[SADB_EXT_ADDRESS_DST])
	{
		sa  = (struct sadb_sa*)ext_msgs[SADB_EXT_SA];

		if ( ((!sa->sadb_sa_auth && !sa->sadb_sa_encrypt) && msg->sadb_msg_satype != SADB_X_SATYPE_COMP) || 
				sa->sadb_sa_auth > SADB_AALG_MAX || 
				sa->sadb_sa_encrypt > SADB_EALG_MAX ) {
			PFKEY_DEBUG("sa has no transform or invalid transform value\n");
			error = -EINVAL;
			goto rtn;
		}

		switch (msg->sadb_msg_satype) {

		case SADB_SATYPE_AH:
		case SADB_SATYPE_ESP:
		case SADB_X_SATYPE_COMP:
			break;
		case SADB_SATYPE_RSVP:
		case SADB_SATYPE_OSPFV2:
		case SADB_SATYPE_RIPV2:
		case SADB_SATYPE_MIP:
		default:
			PFKEY_DEBUG("invalid sa type\n");
			error = -EINVAL;
			goto rtn;
		}

		if (msg->sadb_msg_satype == SADB_SATYPE_ESP && !(sa->sadb_sa_encrypt)) {
			PFKEY_DEBUG("sa type is esp but no algorithm\n");
			error = -EINVAL;
			goto rtn;
		}

		sa_entry = ipsec_sa_kmalloc();
		if (!sa_entry) {
			PFKEY_DEBUG("sa_entry: null\n");
			error = -ENOMEM;
			goto err;
		}

		src = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_SRC];
		dst = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_DST];

		sa_entry->ipsec_proto = msg->sadb_msg_satype;

		/* SPI which is under 255 is reserved by IANA. 
		 * Additionally, 256 and 257 reserved for our internal use.  */
		if (ntohl(sa->sadb_sa_spi) < 258 && sa_entry->ipsec_proto != SADB_X_SATYPE_COMP) {
			PFKEY_DEBUG("SPI value is reserved.(SPI<258)\n");
			goto err;
		}

		sa_entry->spi = sa->sadb_sa_spi;

		error = sadb_address_to_sockaddr( src,
				(struct sockaddr*)&sa_entry->src);
		if (error) {
			PFKEY_DEBUG("src translation failed\n");
			goto err;
		}
		error = sadb_address_to_sockaddr( dst,
				(struct sockaddr*)&sa_entry->dst);
		if (error) {
			PFKEY_DEBUG("dst translation failed\n");
			goto err;
		}

		if (src->sadb_address_proto == dst->sadb_address_proto) {
			sa_entry->proto = src->sadb_address_proto;
		} else {
			error = -EINVAL;
			goto err;
		} 

		sa_entry->prefixlen_s = src->sadb_address_prefixlen;
		sa_entry->prefixlen_d = dst->sadb_address_prefixlen;

		if (sa->sadb_sa_auth) {
			if( (ext_msgs[SADB_EXT_KEY_AUTH]) ) {
				error = sadb_key_to_auth(sa->sadb_sa_auth,
					(struct sadb_key*) ext_msgs[SADB_EXT_KEY_AUTH], sa_entry);
				if (error)
					goto err;
			} else {
				PFKEY_DEBUG("SA has auth algo but there is no key for auth\n");
				error = -EINVAL;
				goto err;
			}
		}

		if (sa->sadb_sa_encrypt) {
			if (sa->sadb_sa_encrypt == SADB_EALG_NULL && 
					sa->sadb_sa_auth == SADB_AALG_NONE) {
				error = -EINVAL;
				goto err;
			}
			if (msg->sadb_msg_satype != SADB_X_SATYPE_COMP) {
				error = sadb_key_to_esp(sa->sadb_sa_encrypt, 
					(struct sadb_key*) ext_msgs[SADB_EXT_KEY_ENCRYPT], sa_entry);
				if (error)
					goto err;
			}
		}

		if (ext_msgs[SADB_EXT_ADDRESS_PROXY]) {
			printk(KERN_WARNING "PFKEY proxy translation is not supported.\n");
			goto err;
		}

		if (ext_msgs[SADB_EXT_LIFETIME_HARD]) {
			error = sadb_lifetime_to_lifetime((struct sadb_lifetime*)ext_msgs[SADB_EXT_LIFETIME_HARD],
						&sa_entry->lifetime_h);
			if (error) goto err;
		}

		if (ext_msgs[SADB_EXT_LIFETIME_SOFT]) {
			error = sadb_lifetime_to_lifetime((struct sadb_lifetime*)ext_msgs[SADB_EXT_LIFETIME_SOFT],
						&sa_entry->lifetime_s);
			if (error) goto err;
		}

		sa_entry->state = SADB_SASTATE_MATURE;

		error = sadb_append(sa_entry);	
		if (error) goto err;	

		reply_ext_msgs[0] = (struct sadb_ext*) msg;
		reply_ext_msgs[SADB_EXT_SA] = ext_msgs[SADB_EXT_SA];
		reply_ext_msgs[SADB_EXT_ADDRESS_SRC] = ext_msgs[SADB_EXT_ADDRESS_SRC];
		reply_ext_msgs[SADB_EXT_ADDRESS_DST] = ext_msgs[SADB_EXT_ADDRESS_DST];

		if (ext_msgs[SADB_EXT_LIFETIME_HARD])
			reply_ext_msgs[SADB_EXT_LIFETIME_HARD] = ext_msgs[SADB_EXT_LIFETIME_HARD];

		if (ext_msgs[SADB_EXT_LIFETIME_SOFT])
			reply_ext_msgs[SADB_EXT_LIFETIME_SOFT] = ext_msgs[SADB_EXT_LIFETIME_SOFT];

		error = pfkey_msg_build(reply, reply_ext_msgs, EXT_BITS_OUT);
		goto rtn;
	} else {
		PFKEY_DEBUG("extensions not enough\n");
		error = -EINVAL;
		goto err;
	}

err:
	ipsec_sa_kfree(sa_entry);
rtn:
	return error;
}

