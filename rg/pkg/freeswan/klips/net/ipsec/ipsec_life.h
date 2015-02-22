/*
 * Definitions relevant to IPSEC lifetimes
 * Copyright (C) 2001  Richard Guy Briggs  <rgb@freeswan.org>
 *                 and Michael Richardson  <mcr@freeswan.org>
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
 * This file derived from ipsec_xform.h on 2001/9/18 by mcr.
 *
 */

/* 
 * This file describes the book keeping fields for the 
 *   IPsec Security Association Structure. ("ipsec_sa")
 *
 * This structure is never allocated directly by kernel code,
 * (it is always a static/auto or is part of a structure)
 * so it does not have a reference count.
 *
 */

#ifndef _IPSEC_LIFE_H_

/*
 *  _count is total count.
 *  _hard is hard limit (kill SA after this number)
 *  _soft is soft limit (try to renew SA after this number)
 *  _last is used in some special cases.
 *
 */

struct ipsec_lifetime64
{
	__u64           ipl_count;
	__u64           ipl_soft;
	__u64           ipl_hard;
	__u64           ipl_last;  
};

struct ipsec_lifetimes
{
	/* number of bytes processed */
	struct ipsec_lifetime64 ipl_bytes;

	/* number of packets processed */
	struct ipsec_lifetime64 ipl_packets;

	/* time since SA was added */
	struct ipsec_lifetime64 ipl_addtime;

	/* time since SA was first used */
	struct ipsec_lifetime64 ipl_usetime;

	/* from rfc2367:  
         *         For CURRENT, the number of different connections,
         *         endpoints, or flows that the association has been
         *          allocated towards. For HARD and SOFT, the number of
         *          these the association may be allocated towards
         *          before it expires. The concept of a connection,
         *          flow, or endpoint is system specific.
	 *
	 * mcr(2001-9-18) it is unclear what purpose these serve for FreeSWAN.
	 *          They are maintained for PF_KEY compatibility. 
	 */
	struct ipsec_lifetime64 ipl_allocations;
};

enum ipsec_life_alive {
	ipsec_life_harddied = -1,
	ipsec_life_softdied = 0,
	ipsec_life_okay     = 1
};

enum ipsec_life_type {
	ipsec_life_timebased = 1,
	ipsec_life_countbased= 0
};

#define _IPSEC_LIFE_H_
#endif /* _IPSEC_LIFE_H_ */


/*
 * $Log: ipsec_life.h,v $
 * Revision 1.5  2007/06/11 11:46:11  guyn
 * B43992 B45894 rgload failed on malindi2_4_7 at dev. Revert the work done on
 * B43992 (Intel Hamoa reference platform porting), in order for MALINDI2 to work.
 *
 * Revision 1.3  2003/09/21 20:23:15  igork
 * merge branch-dev-2421 into dev
 *
 * Revision 1.1.1.1.34.1  2003/09/16 13:34:20  ron
 * merge from branch-3_1 to merge-dev-branch-dev-2421 into branch-dev-2421
 *
 * Revision 1.2  2003/09/11 11:37:56  yoavp
 * AUTO MERGE: 1 <- branch-3_2
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 * NOTIFY: automerge
 *
 * Revision 1.1.1.1.36.1  2003/09/11 11:34:41  yoavp
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 *
 * Revision 1.1.1.1  2003/02/19 11:46:31  sergey
 * upgrading freeswan to ver. 1.99.
 *
 * Revision 1.2  2001/11/26 09:16:14  rgb
 * Merge MCR's ipsec_sa, eroute, proc and struct lifetime changes.
 *
 * Revision 1.1.2.1  2001/09/25 02:25:58  mcr
 * 	lifetime structure created and common functions created.
 *
 *
 * Local variables:
 * c-file-style: "linux"
 * End:
 *
 */
