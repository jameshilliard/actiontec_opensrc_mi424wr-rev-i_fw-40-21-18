/*
 * @(#) declarations of eroute structures
 *
 * Copyright (C) 1996, 1997  John Ioannidis.
 * Copyright (C) 1998, 1999, 2000, 2001  Richard Guy Briggs <rgb@freeswan.org>
 * Copyright (C) 2001                    Michael Richardson <mcr@freeswan.org>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * derived from ipsec_encap.h 1.15 on 2001/9/18 by mcr.
 *
 */

#ifndef _IPSEC_EROUTE_H_

#include "radij.h"
#include "ipsec_encap.h"
#include "ipsec_radij.h"

/*
 * The "type" is really part of the address as far as the routing
 * system is concerned. By using only one bit in the type field
 * for each type, we sort-of make sure that different types of
 * encapsulation addresses won't be matched against the wrong type.
 */

/*
 * An entry in the radix tree 
 */

struct rjtentry
{
	struct	radij_node rd_nodes[2];	/* tree glue, and other values */
#define	rd_key(r)	((struct sockaddr_encap *)((r)->rd_nodes->rj_key))
#define	rd_mask(r)	((struct sockaddr_encap *)((r)->rd_nodes->rj_mask))
#define rd_range_end(r)	((struct sockaddr_encap *)((r)->rd_nodes->rj_range_end))
	short	rd_flags;
	short	rd_count;
};

struct ident
{
	__u16	type;	/* identity type */
	__u64	id;	/* identity id */
	__u8	len;	/* identity len */
	caddr_t	data;	/* identity data */
};

/*
 * An encapsulation route consists of a pointer to a 
 * radix tree entry and a SAID (a destination_address/SPI/protocol triple).
 */

struct eroute
{
	struct rjtentry er_rjt;
	struct sa_id er_said;
	uint32_t er_pid;
	uint32_t er_count;
	uint64_t er_lasttime;
	struct sockaddr_encap er_eaddr; /* MCR get rid of _encap, it is silly*/
	struct sockaddr_encap er_emask;
	/* If er_erange_to is used, then er_eaddr is range-from. */
	struct sockaddr_encap er_erange_to;
        struct ident er_ident_s;
        struct ident er_ident_d;
	struct sk_buff* er_first;
	struct sk_buff* er_last;
};

#define er_dst er_said.dst
#define er_spi er_said.spi

#define _IPSEC_EROUTE_H_
#endif /* _IPSEC_EROUTE_H_ */

/*
 * $Log: ipsec_eroute.h,v $
 * Revision 1.4  2003/09/21 20:23:15  igork
 * merge branch-dev-2421 into dev
 *
 * Revision 1.1.1.2.34.1  2003/09/16 13:34:20  ron
 * merge from branch-3_1 to merge-dev-branch-dev-2421 into branch-dev-2421
 *
 * Revision 1.3  2003/09/11 11:37:56  yoavp
 * AUTO MERGE: 1 <- branch-3_2
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 * NOTIFY: automerge
 *
 * Revision 1.2.2.1  2003/09/11 11:34:41  yoavp
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 *
 * Revision 1.2  2003/09/01 13:24:19  yoavp
 * fix B5435, B5436: support ranges in local and remote lans
 *
 * Revision 1.1.1.2  2003/02/20 12:10:20  sergey
 * Import to ver. 1.99 with x.509 support.
 *
 * Revision 1.2  2001/11/26 09:16:13  rgb
 * Merge MCR's ipsec_sa, eroute, proc and struct lifetime changes.
 *
 * Revision 1.1.2.1  2001/09/25 02:18:54  mcr
 * 	struct eroute moved to ipsec_eroute.h
 *
 *
 * Local variables:
 * c-file-style: "linux"
 * End:
 *
 */
