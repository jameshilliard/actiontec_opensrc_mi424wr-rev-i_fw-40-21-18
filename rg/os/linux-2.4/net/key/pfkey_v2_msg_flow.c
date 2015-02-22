/* $USAGI: pfkey_v2_msg_flow.c,v 1.16.2.1 2002/09/10 07:54:05 yoshfuji Exp $ */
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
 * This is a parse routine for messages SADB_EXT_ADDFLOW and SADB_EXT_DELFLOW.
 * We use FreeS/WAN's user land routine for manipulating SA and Policy.
 * These are FreeS/WAN specific.
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
#include <net/spd.h>

#include "pfkey_v2_msg.h"
#include "sadb_utils.h" /* sockaddrtoa ultoa */
#include "spd_utils.h"
#define BUFSIZE 64

static int sadb_mask_to_prefixlen(const struct sadb_address *ext_msg)
{
	int rtn = 0;
#ifdef CONFIG_IPSEC_DEBUG
	int len;
	char buf[BUFSIZE];
#endif

	struct sockaddr* tmp_addr = NULL;

	if (!ext_msg) {
		PFKEY_DEBUG("ext_msg is null\n");
		return -EINVAL;
	}

#ifdef CONFIG_IPSEC_DEBUG
	len = ext_msg->sadb_address_len - sizeof(struct sadb_address);
	if (len < sizeof(struct sockaddr)) {
		PFKEY_DEBUG("sadb_address_len is small len=%d\n", len);
		return -EINVAL;
	}
#endif

	tmp_addr = (struct sockaddr*)((char*)ext_msg + sizeof(struct sadb_address));

	switch (tmp_addr->sa_family) {

	case AF_INET:
	{
		int i;
		__u32 tmp;
#ifdef CONFIG_IPSEC_DEBUG
		PFKEY_DEBUG("address family is AF_INET\n");
		sockaddrtoa(tmp_addr, buf, BUFSIZE);
		PFKEY_DEBUG("address %s\n", buf);
#endif
		tmp = ntohl(((struct sockaddr_in*)tmp_addr)->sin_addr.s_addr);
		for (i=0; i<32; i++) {
			if ((tmp>>i)&0x01)
				break;
		}
		rtn = 32 - i;
	}
	break;

	case AF_INET6:
	{
		int i,j;
		__u16 tmp;
#ifdef CONFIG_IPSEC_DEBUG
		PFKEY_DEBUG("address family is AF_INET6\n");
		sockaddrtoa(tmp_addr, buf, BUFSIZE);
		PFKEY_DEBUG("address %s\n", buf);
#endif
		for (i=0; i<8; i++ ) {
			tmp = ntohs(((struct sockaddr_in6*)tmp_addr)->sin6_addr.s6_addr16[7-i]);
			for (j=0; j<16; j++) {
				if ((tmp>>j)&0x01) {
					rtn = 128 - i*16 - j;
					PFKEY_DEBUG("result i=%d, j=%d\n", i, j);
					goto err;
				}
			}
		}
	}
	break;
	default:
		PFKEY_DEBUG("address family is unknown\n");
		rtn = -EINVAL;
		goto err;
	}

err:
#ifdef CONFIG_IPSEC_DEBUG
	if (rtn)
		PFKEY_DEBUG("prefixlen=%d\n", rtn);
#endif
	return rtn;
}


int sadb_msg_addflow_parse(struct sock *sk, struct sadb_msg* msg, struct sadb_msg **reply)
{
	int error = 0;
	int tmp = 0;
	char buf[BUFSIZE];
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1], *reply_ext_msgs[SADB_EXT_MAX+1];
	struct sadb_sa *sa = NULL;
//	struct sadb_address *src = NULL;
	struct sadb_address *dst = NULL;
	struct sadb_address *sflow = NULL;
	struct sadb_address *dflow = NULL;
	struct ipsec_sp *policy = NULL;
	struct sa_index sa_idx;
	struct selector selector;


	if (!msg) {
		PFKEY_DEBUG("msg is null\n");
		error = -EINVAL;
		goto err;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	memset(reply_ext_msgs, 0, sizeof(reply_ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);

	if (ext_msgs[SADB_EXT_SA] &&
//		ext_msgs[SADB_EXT_ADDRESS_SRC] &&
		ext_msgs[SADB_EXT_ADDRESS_DST] &&
		ext_msgs[SADB_X_EXT_ADDRESS_SRC_FLOW] &&
		ext_msgs[SADB_X_EXT_ADDRESS_DST_FLOW])
	{
		sa  = (struct sadb_sa*)ext_msgs[SADB_EXT_SA];
		PFKEY_DEBUG("sa.spi=0x%x\n", ntohl(sa->sadb_sa_spi));
		PFKEY_DEBUG("sa.auth=%u\n", ntohs(sa->sadb_sa_auth));
		PFKEY_DEBUG("sa.encrypt=%u\n", ntohs(sa->sadb_sa_encrypt));
#ifdef CONFIG_IPSEC_TUNNEL
		PFKEY_DEBUG("sa mode=%d\n", sa->sadb_sa_flags & SADB_X_SAFLAGS_TUNNEL);
#endif
//		src = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_SRC];
		dst = (struct sadb_address*)ext_msgs[SADB_EXT_ADDRESS_DST];
		sflow = (struct sadb_address*)ext_msgs[SADB_X_EXT_ADDRESS_SRC_FLOW];
		dflow = (struct sadb_address*)ext_msgs[SADB_X_EXT_ADDRESS_DST_FLOW];

#ifdef CONFIG_IPSEC_TUNNEL
		selector.mode = sa->sadb_sa_flags & SADB_X_SAFLAGS_TUNNEL
				? IPSEC_MODE_TUNNEL
				: IPSEC_MODE_TRANSPORT;

		if (selector.mode == IPSEC_MODE_TRANSPORT) {
#endif
			if (sflow->sadb_address_proto == dflow->sadb_address_proto) {
				selector.proto = sflow->sadb_address_proto;
			} else {
				error = -EINVAL;
				goto err;
			}
#ifdef CONFIG_IPSEC_TUNNEL
		}
#endif
		/* assuming an SA is not needed if policy is 
		 * bypass or discard the packet. */
		if ((ntohl(sa->sadb_sa_spi) != IPSEC_SPI_DROP)
				&& (ntohl (sa->sadb_sa_spi) != IPSEC_SPI_BYPASS)) {
			sa_index_init(&sa_idx);

			sa_idx.ipsec_proto = msg->sadb_msg_satype;
			sa_idx.spi = sa->sadb_sa_spi;

			sadb_address_to_sockaddr(dst, (struct sockaddr*)&sa_idx.dst);
			sa_idx.prefixlen_d = dst->sadb_address_prefixlen;
#ifdef CONFIG_IPSEC_DEBUG
			memset(buf, 0, BUFSIZE);
			sockaddrtoa((struct sockaddr*)&sa_idx.dst, buf, BUFSIZE);
			PFKEY_DEBUG("dst=%s\n", buf);
#endif /* CONFIG_IPSEC_DEBUG */
		}

		memset(buf, 0, BUFSIZE);
		sadb_address_to_sockaddr( sflow, (struct sockaddr*)&selector.src);
		sockaddrtoa((struct sockaddr*)&selector.src, buf, BUFSIZE);
		PFKEY_DEBUG("sflow=%s\n", buf);

		memset(buf, 0, BUFSIZE);
		sadb_address_to_sockaddr( dflow, (struct sockaddr*)&selector.dst);
		sockaddrtoa((struct sockaddr*)&selector.dst, buf, BUFSIZE);
		PFKEY_DEBUG("dflow=%s\n", buf);

		selector.prefixlen_s = sflow->sadb_address_prefixlen;
		PFKEY_DEBUG("prefixlen_s=%d\n", selector.prefixlen_s);
		selector.prefixlen_d = dflow->sadb_address_prefixlen;
		PFKEY_DEBUG("prefixlen_d=%d\n", selector.prefixlen_d);

		if (ext_msgs[SADB_X_EXT_ADDRESS_SRC_MASK]) {
			tmp = sadb_mask_to_prefixlen((struct sadb_address*)ext_msgs[SADB_X_EXT_ADDRESS_SRC_MASK]);
			if (tmp<0) {
				error = tmp;
				goto err;
			} else {
				selector.prefixlen_s = tmp;
			}
		}

		if (ext_msgs[SADB_X_EXT_ADDRESS_DST_MASK]) {
			tmp = sadb_mask_to_prefixlen((struct sadb_address*)ext_msgs[SADB_X_EXT_ADDRESS_DST_MASK]);
			if (tmp<0) {
				error = tmp;
				goto err;
			} else {
				selector.prefixlen_d = tmp;
			}
		}

		error = spd_find_by_selector(&selector, &policy);

		PFKEY_DEBUG("spd_find error = %d\n", error);

		if (error == -ESRCH) {

			/* It's new one. I append it into spd_list */

			policy = ipsec_sp_kmalloc();
			if (!policy) {
				PFKEY_DEBUG("ipsec_sp_kmalloc faild\n");
				error = -ENOMEM;
				goto err;
			}
			memcpy(&policy->selector, &selector, sizeof(struct selector));

			if (ntohl(sa->sadb_sa_spi) == IPSEC_SPI_DROP) {
				policy->policy_action = IPSEC_POLICY_DROP;
				error = 0;
			} else if (ntohl(sa->sadb_sa_spi) == IPSEC_SPI_BYPASS) {
				policy->policy_action = IPSEC_POLICY_BYPASS;
				error = 0;
			} else {
				policy->policy_action = IPSEC_POLICY_APPLY;
				switch (sa_idx.ipsec_proto) {
				case SADB_SATYPE_AH:
					policy->auth_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->auth_sa_idx, &sa_idx);
					break;
				case SADB_SATYPE_ESP:
					policy->esp_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->esp_sa_idx, &sa_idx);
					break;
				case SADB_X_SATYPE_COMP: /* Currently, just set as SA related with ESP which you specify by SPI. */
					policy->comp_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->comp_sa_idx, &sa_idx);
					printk(KERN_DEBUG "set IPComp policy.\n");
					break;
				default:
					error = -EINVAL;
					ipsec_sp_put(policy);
					goto err;
				}
			}

			if (!error) {
				error = spd_append(policy);
			}

			ipsec_sp_put(policy);
			error = 0;

		} else if (error == -EEXIST) {

			/* It has already been in spd_list, I append sa_index into it's sa_list */
			write_lock_bh(&policy->lock);
			PFKEY_DEBUG("policy=%p\n", policy);

			switch(sa_idx.ipsec_proto) {

			case SADB_SATYPE_AH:
				if (!policy->auth_sa_idx) {
					policy->auth_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->auth_sa_idx, &sa_idx);

				} else if (sa->sadb_sa_flags & SADB_X_SAFLAGS_REPLACEFLOW) {
					sa_index_kfree(policy->auth_sa_idx);
					policy->auth_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->auth_sa_idx, &sa_idx);
				}

				break;

			case SADB_SATYPE_ESP:
				if (!policy->esp_sa_idx) {
					policy->esp_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->esp_sa_idx, &sa_idx);

				} else if (sa->sadb_sa_flags & SADB_X_SAFLAGS_REPLACEFLOW) {
					sa_index_kfree(policy->esp_sa_idx);
					policy->esp_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->esp_sa_idx, &sa_idx);
				}

				break;

			case SADB_X_SATYPE_COMP:
				if (!policy->comp_sa_idx) {
					policy->comp_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->comp_sa_idx, &sa_idx);
				} else if (sa->sadb_sa_flags & SADB_X_SAFLAGS_REPLACEFLOW) {
					sa_index_kfree(policy->comp_sa_idx);
					policy->comp_sa_idx = sa_index_kmalloc();
					error = sa_index_copy(policy->comp_sa_idx, &sa_idx);
				}

				break;

			default:
				error = -EINVAL;
				write_unlock_bh(&policy->lock);
				ipsec_sp_put(policy);
				goto err;
			}

			if (error) {
				write_unlock_bh(&policy->lock);
				ipsec_sp_put(policy);
				goto err;
			}

			write_unlock_bh(&policy->lock);
			ipsec_sp_put(policy);
			error = 0;

		} else {
			/* Something is wrong */
			PFKEY_DEBUG("spd_find_by_selector faild\n");
			error = -EINVAL;
			goto err;
		}

	}
	
err:
	reply_ext_msgs[0] = (struct sadb_ext*) msg;
	reply_ext_msgs[SADB_EXT_SA] = ext_msgs[SADB_EXT_SA];
	reply_ext_msgs[SADB_EXT_ADDRESS_SRC] = ext_msgs[SADB_EXT_ADDRESS_SRC];
	reply_ext_msgs[SADB_EXT_ADDRESS_DST] = ext_msgs[SADB_EXT_ADDRESS_DST];
	reply_ext_msgs[SADB_X_EXT_ADDRESS_SRC_FLOW] = ext_msgs[SADB_X_EXT_ADDRESS_SRC_FLOW];
	reply_ext_msgs[SADB_X_EXT_ADDRESS_DST_FLOW] = ext_msgs[SADB_X_EXT_ADDRESS_DST_FLOW];
	reply_ext_msgs[SADB_X_EXT_ADDRESS_SRC_MASK] = ext_msgs[SADB_X_EXT_ADDRESS_SRC_MASK];
	reply_ext_msgs[SADB_X_EXT_ADDRESS_DST_MASK] = ext_msgs[SADB_X_EXT_ADDRESS_DST_MASK];
	error = pfkey_msg_build(reply, reply_ext_msgs, EXT_BITS_OUT);

	return error;

}

int sadb_msg_delflow_parse(struct sock *sk, struct sadb_msg* msg, struct sadb_msg **reply)
{
	int error = 0;
	int tmp = 0;
	struct sadb_ext *ext_msgs[SADB_EXT_MAX+1], *reply_ext_msgs[SADB_EXT_MAX+1];
	struct sadb_sa *sa = NULL;
	struct sadb_address *sflow = NULL;
	struct sadb_address *dflow = NULL;
	//struct sa_index sa_idx;
	struct selector selector;


	if (!msg) {
		PFKEY_DEBUG("msg is null\n");
		error = -EINVAL;
		goto err;
	}

	memset(ext_msgs, 0, sizeof(ext_msgs));
	memset(reply_ext_msgs, 0, sizeof(reply_ext_msgs));
	error = sadb_msg_detect_ext(msg, ext_msgs);

	if (ext_msgs[SADB_EXT_SA] &&
		ext_msgs[SADB_X_EXT_ADDRESS_SRC_FLOW] &&
		ext_msgs[SADB_X_EXT_ADDRESS_DST_FLOW])
	{
		sa  = (struct sadb_sa*)ext_msgs[SADB_EXT_SA];

		sflow = (struct sadb_address*)ext_msgs[SADB_X_EXT_ADDRESS_SRC_FLOW];
		dflow = (struct sadb_address*)ext_msgs[SADB_X_EXT_ADDRESS_DST_FLOW];
#ifdef CONFIG_IPSEC_TUNNEL
		selector.mode = sa->sadb_sa_flags & SADB_X_SAFLAGS_TUNNEL
				? IPSEC_MODE_TUNNEL
				: IPSEC_MODE_TRANSPORT;

		if (selector.mode == IPSEC_MODE_TRANSPORT) {
#endif
			if (sflow->sadb_address_proto == dflow->sadb_address_proto) {
				selector.proto = sflow->sadb_address_proto;
			} else {
				error = -EINVAL;
				goto err;
			}	
#ifdef CONFIG_IPSEC_TUNNEL
		}
#endif
		sadb_address_to_sockaddr( sflow, (struct sockaddr*)&selector.src);
		sadb_address_to_sockaddr( dflow, (struct sockaddr*)&selector.dst);
		selector.prefixlen_s = sflow->sadb_address_prefixlen;
		selector.prefixlen_d = dflow->sadb_address_prefixlen;

		if (ext_msgs[SADB_X_EXT_ADDRESS_SRC_MASK]) {
			tmp = sadb_mask_to_prefixlen(
				(struct sadb_address*)ext_msgs[SADB_X_EXT_ADDRESS_SRC_MASK]);
			if (tmp<0) {
				error = tmp;
				goto err;
			} else {
				selector.prefixlen_s = tmp;
			}
		}

		if (ext_msgs[SADB_X_EXT_ADDRESS_DST_MASK]) {
			tmp = sadb_mask_to_prefixlen(
				(struct sadb_address*)ext_msgs[SADB_X_EXT_ADDRESS_DST_MASK]);
			if (tmp<0) {
				error = tmp;
				goto err;
			} else {
				selector.prefixlen_d = tmp;
			}
		}

		error = spd_remove(&selector);

		if (error) {
			PFKEY_DEBUG("spd_remove failed\n");
			goto err;
		}

	}

	reply_ext_msgs[0] = (struct sadb_ext*) msg;
	reply_ext_msgs[SADB_EXT_SA] = ext_msgs[SADB_EXT_SA];
	reply_ext_msgs[SADB_X_EXT_ADDRESS_SRC_FLOW] = ext_msgs[SADB_X_EXT_ADDRESS_SRC_FLOW];
	reply_ext_msgs[SADB_X_EXT_ADDRESS_DST_FLOW] = ext_msgs[SADB_X_EXT_ADDRESS_DST_FLOW];
	reply_ext_msgs[SADB_X_EXT_ADDRESS_SRC_MASK] = ext_msgs[SADB_X_EXT_ADDRESS_SRC_MASK];
	reply_ext_msgs[SADB_X_EXT_ADDRESS_DST_MASK] = ext_msgs[SADB_X_EXT_ADDRESS_DST_MASK];
	error = pfkey_msg_build(reply, reply_ext_msgs, EXT_BITS_OUT);

err:
	return error;

}



