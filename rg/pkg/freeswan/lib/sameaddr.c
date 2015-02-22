/*
 * comparisons
 * Copyright (C) 2000  Henry Spencer.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/lgpl.txt>.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 */
#include "internal.h"
#include "freeswan.h"

static int samenbits(const ip_address *a, const ip_address *b, int n);

/*
 - addrcmp - compare two addresses
 * Caution, the order of the tests is subtle:  doing type test before
 * size test can yield cases where a<b, b<c, but a>c.
 */
int				/* like memcmp */
addrcmp(a, b)
const ip_address *a;
const ip_address *b;
{
	int at = addrtypeof(a);
	int bt = addrtypeof(b);
	const unsigned char *ap;
	const unsigned char *bp;
	size_t as = addrbytesptr(a, &ap);
	size_t bs = addrbytesptr(b, &bp);
	size_t n = (as < bs) ? as : bs;		/* min(as, bs) */
	int c = memcmp(ap, bp, n);

	if (c != 0)		/* bytes differ */
		return (c < 0) ? -1 : 1;
	if (as != bs)		/* comparison incomplete:  lexical order */
		return (as < bs) ? -1 : 1;
	if (at != bt)		/* bytes same but not same type:  break tie */
		return (at < bt) ? -1 : 1;
	return 0;
}

/*
 - sameaddr - are two addresses the same?
 */
int
sameaddr(a, b)
const ip_address *a;
const ip_address *b;
{
	return (addrcmp(a, b) == 0) ? 1 : 0;
}

/*
 - samesubnet - are two subnets the same?
 */
int
samesubnet(a, b)
const ip_subnet *a;
const ip_subnet *b;
{
	unsigned int a_from, a_to, b_from, b_to;
	/* XXX doesn't support AF_INET6 */
#define NET_TO_RANGE(x, dst_from, dst_to) \
	do { \
	    if ((x)->is_subnet) \
	    { \
		ip_address mask; \
		\
		maskof(x, &mask); \
		dst_from = (x)->v.net.addr.u.v4.sin_addr.s_addr; \
		dst_to = dst_from | ~mask.u.v4.sin_addr.s_addr; \
	    } \
	    else \
	    { \
		dst_from = (x)->v.range.from.u.v4.sin_addr.s_addr; \
		dst_to = (x)->v.range.to.u.v4.sin_addr.s_addr; \
	    } \
	} while (0)

	NET_TO_RANGE(a, a_from, a_to);
	NET_TO_RANGE(b, b_from, b_to);
	return a_from == b_from && a_to == b_to;
}

/*
 - subnetishost - is a subnet in fact a single host?
 */
int
subnetishost(a)
const ip_subnet *a;
{
	if (a->is_subnet)
		return a->v.net.maskbits == addrlenof(&a->v.net.addr)*8;
	else
		return sameaddr(&a->v.range.from, &a->v.range.to);
}

/*
 - samesaid - are two SA IDs the same?
 */
int
samesaid(a, b)
const ip_said *a;
const ip_said *b;
{
	if (a->spi != b->spi)	/* test first, most likely to be different */
		return 0;
	if (!sameaddr(&a->dst, &b->dst))
		return 0;
	if (a->proto != b->proto)
		return 0;
	return 1;
}

/*
 - sameaddrtype - do two addresses have the same type?
 */
int
sameaddrtype(a, b)
const ip_address *a;
const ip_address *b;
{
	return (addrtypeof(a) == addrtypeof(b)) ? 1 : 0;
}

/*
 - samesubnettype - do two subnets have the same type?
 */
int
samesubnettype(a, b)
const ip_subnet *a;
const ip_subnet *b;
{
	return (subnettypeof(a) == subnettypeof(b)) ? 1 : 0;
}

/*
 - addrinsubnet - is this address in this subnet?
 */
int
addrinsubnet(a, s)
const ip_address *a;
const ip_subnet *s;
{
	if (addrtypeof(a) != subnettypeof(s))
		return 0;
	if (s->is_subnet && !samenbits(a, &s->v.net.addr, s->v.net.maskbits))
		return 0;
	if (!s->is_subnet && addrtypeof(a)==AF_INET)
	{
		unsigned int from = ntohl(s->v.range.from.u.v4.sin_addr.s_addr);
		unsigned int to = ntohl(s->v.range.to.u.v4.sin_addr.s_addr);
		unsigned int addr = ntohl(a->u.v4.sin_addr.s_addr);

		return addr >= from && addr <= to;
	}
	/* XXX Add support for AF_INET6. */
	return 1;
}

static unsigned int subnet_end_get(const ip_subnet *s)
{
    if (s->is_subnet)
    {
	ip_address mask;

	maskof(s, &mask);
	return ntohl(s->v.net.addr.u.v4.sin_addr.s_addr) |
	    ~ntohl(mask.u.v4.sin_addr.s_addr);
    }
    else
	return ntohl(s->v.range.to.u.v4.sin_addr.s_addr);
}

/*
 - subnetinsubnet - is a within b?
 */
int
subnetinsubnet(a, b)
const ip_subnet *a;
const ip_subnet *b;
{
	unsigned int b_from, b_to, a_from, a_to;

	if (subnettypeof(a) != subnettypeof(b))
		return 0;

	/* XXX Add support for AF_INET6. */
	a_from = ntohl(SUBNET_ADDR_GET(a)->u.v4.sin_addr.s_addr);
	b_from = ntohl(SUBNET_ADDR_GET(b)->u.v4.sin_addr.s_addr);
	a_to = subnet_end_get(a);
	b_to = subnet_end_get(b);

	return a_from >= b_from && a_to <= b_to;
}

/*
 - samenbits - do two addresses have the same first n bits?
 */
static int
samenbits(a, b, nbits)
const ip_address *a;
const ip_address *b;
int nbits;
{
	const unsigned char *ap;
	const unsigned char *bp;
	size_t n;
	int m;

	if (addrtypeof(a) != addrtypeof(b))
		return 0;	/* arbitrary */
	n = addrbytesptr(a, &ap);
	if (n == 0)
		return 0;	/* arbitrary */
	(void) addrbytesptr(b, &bp);
	if (nbits > n*8)
		return 0;	/* "can't happen" */

	for (; nbits >= 8 && *ap == *bp; nbits -= 8, ap++, bp++)
		continue;
	if (nbits >= 8)
		return 0;
	if (nbits > 0) {	/* partial byte */
		m = ~(0xff >> nbits);
		if ((*ap & m) != (*bp & m))
			return 0;
	}
	return 1;
}
