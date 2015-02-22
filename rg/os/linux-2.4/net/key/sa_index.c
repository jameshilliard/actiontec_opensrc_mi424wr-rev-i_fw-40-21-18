/* $USAGI: sa_index.c,v 1.6 2002/08/13 10:51:41 miyazawa Exp $ */
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
 * struct ipsec_sa is connected with struct ipsec_sp by struct sa_index
 * The element sa of sa_index occasionlly NULL.
 * Note that struct ipsec_sa has a reference count. 
 */


#ifdef MODULE
#  include <linux/module.h>
#  ifdef MODVERSIONS
#    include <linux/modversions.h>
#  endif /* MODVERSIONS */
#endif /* MODULE */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/ipsec.h>

#include <net/sadb.h>

#include "sadb_utils.h"

struct sa_index* sa_index_kmalloc()
{
	struct sa_index* sa_idx = NULL;

	sa_idx = kmalloc(sizeof(struct sa_index), GFP_ATOMIC);

	if (!sa_idx) {
		SADB_DEBUG("kmalloc faild\n");
		return NULL;
	}

	sa_index_init(sa_idx);

	return sa_idx;
}

int sa_index_init(struct sa_index *sa)
{
	int error = 0;

	if (!sa) {
		SADB_DEBUG("sa is null\n");
		error = -EINVAL;
		goto err;
	}

	memset(sa, 0, sizeof(struct sa_index));
	INIT_LIST_HEAD(&sa->entry);

err:
	return error;
}

void sa_index_kfree(struct sa_index *sa_idx)
{
	if (!sa_idx) {
		SADB_DEBUG("sa is null\n");
		return;
	}

	if (sa_idx->sa) {
		ipsec_sa_put((sa_idx)->sa);
		SADB_DEBUG("ptr=%p,refcnt=%d\n",
			   sa_idx->sa, atomic_read(&sa_idx->sa->refcnt));
		sa_idx->sa = NULL;
	}

	kfree(sa_idx);
}

int sa_index_copy(struct sa_index *dst, struct sa_index *src)
{
	int error = 0;

	if (!dst || !src) {
		SADB_DEBUG("dst or src is null\n");
		error = -EINVAL;
		goto err;
	}

	dst->dst	 = src->dst;
	dst->prefixlen_d = src->prefixlen_d;
	dst->ipsec_proto = src->ipsec_proto;
	dst->spi 	 = src->spi;
	if (src->sa) {
		dst->sa		 = src->sa;
		atomic_inc(&dst->sa->refcnt);
		SADB_DEBUG("ptr=%p,refcnt=%d\n",
			   dst->sa, atomic_read(&dst->sa->refcnt));
	}

err:
	return error;
}

/*
	Currently sa_index_compare is used in ipsec6_input.c
	If ether sa_idx1 or sa_idx2 has SPI=0xFFFFFFFF, It ignores SPI.
	Please take care to use sa_index_compare in other codes.

	We use SPI=0xFFFFFFFF in policy as SPI is any.
*/
int sa_index_compare(struct sa_index *sa_idx1, struct sa_index *sa_idx2)
{
	if (!sa_idx1 || !sa_idx2) {
		SADB_DEBUG("sa_idx1 or sa_idx2 is null\n");
		return -EINVAL;
	}

	if (sa_idx1->spi != IPSEC_SPI_ANY && sa_idx2->spi != IPSEC_SPI_ANY) {
		if (sa_idx1->spi != sa_idx2->spi)
			return 1;
	}

	if (sa_idx1->ipsec_proto != sa_idx2->ipsec_proto)
		return 1;

	return compare_address_with_prefix((struct sockaddr*)&sa_idx1->dst, sa_idx1->prefixlen_d,
					   (struct sockaddr*)&sa_idx2->dst, sa_idx2->prefixlen_d); 
}
