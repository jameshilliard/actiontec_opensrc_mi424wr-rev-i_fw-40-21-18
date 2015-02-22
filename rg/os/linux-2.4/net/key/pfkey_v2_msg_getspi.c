/* $USAGI: pfkey_v2_msg_getspi.c,v 1.4.2.1 2002/09/03 08:35:31 mk Exp $ */
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
 * This is a parse routine for a message of SADB_GETSPI.
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

int sadb_msg_getspi_parse(struct sock *sk, struct sadb_msg* msg, struct sadb_msg **reply) 
{
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1], *reply_ext_msgs[SADB_EXT_MAX+1];
	int error = 0, found_avail = 0;
	__u32 newspi = 0;
	__u32 spi_max = 0, spi_min = 0;
	struct ipsec_sa sadb_entry;
	struct sadb_address *src, *dst;

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

	memset(reply_ext_msgs, 0, sizeof(reply_ext_msgs));

	if (ext_msgs[SADB_EXT_ADDRESS_SRC] &&
	    ext_msgs[SADB_EXT_ADDRESS_DST] &&
	    ext_msgs[SADB_EXT_SPIRANGE])
	{
		src = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_SRC];
		dst = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_DST];

		memset(&sadb_entry, 0, sizeof(struct ipsec_sa));
		error = sadb_address_to_sockaddr(src, (struct sockaddr*)&sadb_entry.src);
		if (error) {
			PFKEY_DEBUG("error in translate src address\n");
			goto err;
		}
		sadb_entry.prefixlen_s = src->sadb_address_prefixlen;

		error = sadb_address_to_sockaddr(dst, (struct sockaddr*)&sadb_entry.dst);
		if (error) {
			PFKEY_DEBUG("error in translate dst address\n");
		}
		sadb_entry.prefixlen_d = dst->sadb_address_prefixlen;

		spi_min = ((struct sadb_spirange*)ext_msgs[SADB_EXT_SPIRANGE])->sadb_spirange_min;
		spi_max = ((struct sadb_spirange*)ext_msgs[SADB_EXT_SPIRANGE])->sadb_spirange_max;

		/* SPI which is under 255 is reserved by IANA. 
		 * Additionally, 256 and 257 reserved ofr internal use.  */
		if (spi_min < 258) {
			PFKEY_DEBUG("SPI value is reserved.(SPI<258)\n");
			goto err;
		}

		if (spi_min == spi_max) {
			PFKEY_DEBUG("spi_min and spi_max are equal\n");

			error = sadb_find_by_address_proto_spi((struct sockaddr*)&sadb_entry.src, sadb_entry.prefixlen_s, 
								(struct sockaddr*)&sadb_entry.dst, sadb_entry.prefixlen_d, 
								spi_min, msg->sadb_msg_type, NULL /*only check*/);
				
			if (error == -ESRCH) {
				newspi = spi_min;
				found_avail = 1;
			} else {
				PFKEY_DEBUG("sadb_find_by_address_proto_spi return %d\n", error);
				goto err;
			}

		} else if (ntohl(spi_min) < ntohl(spi_max)) {
			/* This codes are derived from FreeS/WAN */
			int i = 0;
                	__u32 rand_val;
			__u32 spi_diff;

			PFKEY_DEBUG("spi_min and spi_max are defference\n");

			while ( ( i < (spi_diff = (ntohl(spi_max) - ntohl(spi_min)))) && !found_avail ) {
				get_random_bytes((void*) &rand_val,
				/* sizeof(extr->tdb->tdb_said.spi) */
                                         ( (spi_diff < (2^8))  ? 1 :
                                           ( (spi_diff < (2^16)) ? 2 :
                                             ( (spi_diff < (2^24)) ? 3 :
                                           4 ) ) ) );
				newspi = htonl(ntohl(spi_min) +
					(rand_val % (spi_diff + 1)));
				PFKEY_DEBUG("new spi is %d\n", ntohl(newspi));

				i++;
				error = sadb_find_by_address_proto_spi( (struct sockaddr*)&sadb_entry.src, sadb_entry.prefixlen_s, 
									(struct sockaddr*)&sadb_entry.dst, sadb_entry.prefixlen_d, 
									newspi, msg->sadb_msg_type, NULL /* only check */);
				if (error == -ESRCH) {
					found_avail = 1;
					break;
				} else {
					PFKEY_DEBUG("sadb_find_by_address_proto_spi return %d\n", error);
					goto err;
				}
			}

		} else {
			PFKEY_DEBUG("invalid spi range\n");
			error = -EINVAL;
			goto err;
		}

		if (found_avail) {
			sadb_entry.spi = newspi;
			sadb_entry.state = SADB_SASTATE_LARVAL;
			error = sadb_append(&sadb_entry);
			if (error) {
				PFKEY_DEBUG("sadb_append return %d\n", error);
				goto err;
			}
		} else {
			PFKEY_DEBUG("could not find available spi\n");
			goto err;
		}

	} else {
		PFKEY_DEBUG("necessary ext messages are not available\n");
		error = -EINVAL;
		goto err;
	}

	error = pfkey_sa_build(&reply_ext_msgs[SADB_EXT_SA], SADB_EXT_SA,
			newspi, 0, 0, 0, 0, SADB_SAFLAGS_PFS);
	if (error) {
		PFKEY_DEBUG("pfkey_address_build faild\n");
		goto err;
	}

	reply_ext_msgs[0] = (struct sadb_ext*) msg;
	reply_ext_msgs[SADB_EXT_ADDRESS_SRC] = ext_msgs[SADB_EXT_ADDRESS_SRC];
	reply_ext_msgs[SADB_EXT_ADDRESS_DST] = ext_msgs[SADB_EXT_ADDRESS_DST];
	error = pfkey_msg_build(reply, reply_ext_msgs, EXT_BITS_OUT);
err:
	return error;
}

