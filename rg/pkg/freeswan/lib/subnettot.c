/*
 * convert binary form of subnet description to text
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

/*
 - subnettot - convert subnet to text "addr/bitcount", range to text "addr-addr"
 */
size_t				/* space needed for full conversion */
subnettot(sub, format, dst, dstlen)
const ip_subnet *sub;
int format;			/* character */
char *dst;			/* need not be valid if dstlen is 0 */
size_t dstlen;
{
	size_t len;
	size_t rest;
	char *p;

	switch (format) {
	case 0:
		break;
	default:
		return 0;
		break;
	}

	if (sub->is_subnet)
		len = addrtot(&sub->v.net.addr, format, dst, dstlen);
	else
		len = addrtot(&sub->v.range.from, format, dst, dstlen);
	if (len < dstlen) {
		dst[len - 1] = sub->is_subnet ? '/' : '-';
		p = dst + len;
		rest = dstlen - len;
	} else {
		p = NULL;
		rest = 0;
	}
	if (sub->is_subnet)
		len += ultoa((unsigned long)sub->v.net.maskbits, 10, p, rest);
	else
		len += addrtot(&sub->v.range.to, format, p, rest);

	return len;
}
