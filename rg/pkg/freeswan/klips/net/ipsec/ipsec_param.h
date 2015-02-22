/*
 * @(#) FreeSWAN tunable paramaters
 *
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
 */

/* 
 * This file provides a set of #define's which may be tuned by various
 * people/configurations. It keeps all compile-time tunables in one place.
 *
 * This file should be included before all other IPsec kernel-only files.
 *
 */

#ifndef _IPSEC_PARAM_H_

#include "ipsec_kversion.h"

#ifdef CONFIG_IPSEC_BIGGATE
#define SADB_HASHMOD   8069
#else
#define SADB_HASHMOD	257
#endif

#ifndef PROC_NO_DUMMY
#define IPSEC_PROC_LAST_ARG , int dummy
#else
#define IPSEC_PROC_LAST_ARG
#endif /* !PROC_NO_DUMMY */

#ifdef NETDEV_23

#define ipsec_dev_get __dev_get_by_name

#else

#define ipsec_dev_get dev_get

#endif /* NETDEV_23 */

#ifndef PROC_FS_2325
#define IPSEC_PROCFS_DEBUG_NO_STATIC DEBUG_NO_STATIC
#else
#define IPSEC_PROCFS_DEBUG_NO_STATIC
#endif /* PROC_FS_2325 */

#if !defined(LINUX_KERNEL_HAS_SNPRINTF)
/* GNU CPP specific! */
#define snprintf(buf, len, fmt...) sprintf(buf, ##fmt)
#endif

#ifdef SPINLOCK
 #ifdef SPINLOCK_23
  #include <linux/spinlock.h> /* *lock* */
 #else /* SPINLOCK_23 */
  #include <asm/spinlock.h> /* *lock* */
 #endif /* SPINLOCK_23 */
#endif /* SPINLOCK */

#ifndef KLIPS_FIXES_DES_PARITY
#define KLIPS_FIXES_DES_PARITY 1
#endif

#ifndef KLIPS_DIVULGE_CYPHER_KEY
#define KLIPS_DIVULGE_CYPHER_KEY 0
#endif

/* extra toggles for regression testing */
#ifdef CONFIG_IPSEC_REGRESS

/* 
 * should pfkey_acquire() become 100% lossy?
 *
 */
extern int sysctl_ipsec_regress_pfkey_lossage;
#ifndef KLIPS_PFKEY_ACQUIRE_LOSSAGE
#ifdef CONFIG_IPSEC_PFKEY_ACQUIRE_LOSSAGE
#define KLIPS_PFKEY_ACQUIRE_LOSSAGE 100
#else
/* not by default! */
#define KLIPS_PFKEY_ACQUIRE_LOSSAGE 0
#endif
#endif

#endif

/* IP_FRAGMENT_LINEARIZE is set in freeswan.h if Kernel > 2.4.4 */
#ifndef IP_FRAGMENT_LINEARIZE
#define IP_FRAGMENT_LINEARIZE 0
#endif

#ifdef CONFIG_IPSEC_NAT_TRAVERSAL
#define NAT_TRAVERSAL 1
#endif

#define _IPSEC_PARAM_H_
#endif

/*
 * $Log: ipsec_param.h,v $
 * Revision 1.5  2006/02/23 17:36:43  sergey
 * AUTO MERGE: 1 <- branch-4_2
 * B6059: implement (OpenSWAN based) IPSec NAT-Traversal.
 * NOTIFY: automerge
 *
 * Revision 1.4.2.1  2006/02/23 17:32:13  sergey
 * B6059: implement (OpenSWAN based) IPSec NAT-Traversal.
 *
 * Revision 1.4  2005/11/14 10:54:29  sergey
 * AUTO MERGE: 1 <- branch-4_1
 * B26997: enable IPSec with linux 2.6.
 * NOTIFY: automerge
 *
 * Revision 1.3.172.1  2005/11/14 10:47:20  sergey
 * B26997: enable IPSec with linux 2.6.
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
 * Revision 1.6  2002/01/29 02:11:42  mcr
 * 	removal of kversions.h - sources that needed it now use ipsec_param.h.
 * 	updating of IPv6 structures to match latest in6.h version.
 * 	removed dead code from freeswan.h that also duplicated kversions.h
 * 	code.
 *
 * Revision 1.5  2002/01/28 19:22:01  mcr
 * 	by default, turn off LINEARIZE option
 * 	(let kversions.h turn it on)
 *
 * Revision 1.4  2002/01/20 20:19:36  mcr
 * 	renamed option to IP_FRAGMENT_LINEARIZE.
 *
 * Revision 1.3  2002/01/12 02:57:25  mcr
 * 	first regression test causes acquire messages to be lost
 * 	100% of the time. This is to help testing of pluto.
 *
 * Revision 1.2  2001/11/26 09:16:14  rgb
 * Merge MCR's ipsec_sa, eroute, proc and struct lifetime changes.
 *
 * Revision 1.1.2.3  2001/10/23 04:40:16  mcr
 * 	added #define for DIVULGING session keys in debug output.
 *
 * Revision 1.1.2.2  2001/10/22 20:53:25  mcr
 * 	added a define to control forcing of DES parity.
 *
 * Revision 1.1.2.1  2001/09/25 02:20:19  mcr
 * 	many common kernel configuration questions centralized.
 * 	more things remain that should be moved from freeswan.h.
 *
 *
 * Local variables:
 * c-file-style: "linux"
 * End:
 *
 */
