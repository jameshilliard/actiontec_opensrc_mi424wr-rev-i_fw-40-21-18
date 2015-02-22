/* $USAGI: pfkey_v2_msg_update.c,v 1.2.2.1 2002/09/03 08:35:31 mk Exp $ */
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
 * This is a parse routine for a message of SADB_UPDATE.
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
#include <net/sadb.h>

#include "pfkey_v2_msg.h"
#include "sadb_utils.h" /* sockaddrtoa ultoa */
#define BUFSIZE 64

int sadb_msg_update_parse(struct sock *sk, struct sadb_msg* msg, struct sadb_msg **reply)
{
	int error = 0;
	__u32 spi = 0;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1];
	struct sadb_sa *sa = NULL;
	struct sadb_address *src = NULL;
	struct sadb_address *dst = NULL;
	struct ipsec_sa *sa_entry = NULL;
	struct sockaddr_storage saddr, daddr, paddr;

	if (!msg) {
		PFKEY_DEBUG("msg==null\n");
		error = -EINVAL;
		goto err;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);
	
	if (ext_msgs[SADB_EXT_SA] &&
	    ext_msgs[SADB_EXT_ADDRESS_SRC] &&
	    ext_msgs[SADB_EXT_ADDRESS_DST] &&
	   (ext_msgs[SADB_EXT_KEY_AUTH] ||
	    ext_msgs[SADB_EXT_KEY_ENCRYPT]))
	{
		
		memset(&saddr, 0, sizeof(struct sockaddr_storage));
		memset(&daddr, 0, sizeof(struct sockaddr_storage));
		memset(&paddr, 0, sizeof(struct sockaddr_storage));

		src = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_SRC];
		dst = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_DST];

		spi = sa->sadb_sa_spi;

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

		if (ext_msgs[SADB_EXT_ADDRESS_PROXY]) {
			printk(KERN_WARNING "PFKEY proxy translation is not supported.\n");
		}

		if (sa->sadb_sa_auth && !(ext_msgs[SADB_EXT_KEY_AUTH])) {
			PFKEY_DEBUG("sa has auth algo but there is no key for auth\n");
			error = -EINVAL;
			goto err;
		}

		if (sa->sadb_sa_encrypt && !(ext_msgs[SADB_EXT_KEY_ENCRYPT])) {
			PFKEY_DEBUG("sa has esp algo but there is no key for esp\n");
			error = -EINVAL;
			goto err;
		}

		if (ext_msgs[SADB_EXT_ADDRESS_PROXY]) { 
			PFKEY_DEBUG("PFKEY proxy translation not supported.\n");
			error = -EINVAL;
			goto err;
		}

		if (error) {
			PFKEY_DEBUG("could not find sa\n");
			goto err;
		}

		switch (sa_entry->state) {
		case SADB_SASTATE_LARVAL:
		case SADB_SASTATE_MATURE:
		case SADB_SASTATE_DYING:
			break;
		case SADB_SASTATE_DEAD:
		default:
		}

	}
	/* mask auth key and esp key */
err:
	return error;
}

