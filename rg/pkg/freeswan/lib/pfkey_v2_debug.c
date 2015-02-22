/*
 * @(#) pfkey version 2 debugging messages
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

#ifdef __KERNEL__

# include <linux/kernel.h>  /* for printk */

# include "ipsec_kversion.h" /* for malloc switch */
# ifdef MALLOC_SLAB
#  include <linux/slab.h> /* kmalloc() */
# else /* MALLOC_SLAB */
#  include <linux/malloc.h> /* kmalloc() */
# endif /* MALLOC_SLAB */
# include <linux/errno.h>  /* error codes */
# include <linux/types.h>  /* size_t */
# include <linux/interrupt.h> /* mark_bh */

# include <linux/netdevice.h>   /* struct device, and other headers */
# include <linux/etherdevice.h> /* eth_type_trans */
extern int debug_pfkey;

#else /* __KERNEL__ */

# include <sys/types.h>
# include <linux/types.h>
# include <linux/errno.h>

#endif /* __KERNEL__ */

#include "freeswan.h"
#include "pfkeyv2.h"
#include "pfkey.h"

/* 
 * This file provides ASCII translations of PF_KEY magic numbers.
 *
 */

static char *pfkey_sadb_ext_strings[]={
  "reserved",                     /* SADB_EXT_RESERVED             0 */
  "security-association",         /* SADB_EXT_SA                   1 */
  "lifetime-current",             /* SADB_EXT_LIFETIME_CURRENT     2 */
  "lifetime-hard",                /* SADB_EXT_LIFETIME_HARD        3 */
  "lifetime-soft",                /* SADB_EXT_LIFETIME_SOFT        4 */
  "source-address",               /* SADB_EXT_ADDRESS_SRC          5 */
  "destination-address",          /* SADB_EXT_ADDRESS_DST          6 */
  "proxy-address",                /* SADB_EXT_ADDRESS_PROXY        7 */
  "authentication-key",           /* SADB_EXT_KEY_AUTH             8 */
  "cipher-key",                   /* SADB_EXT_KEY_ENCRYPT          9 */
  "source-identity",              /* SADB_EXT_IDENTITY_SRC         10 */
  "destination-identity",         /* SADB_EXT_IDENTITY_DST         11 */
  "sensitivity-label",            /* SADB_EXT_SENSITIVITY          12 */
  "proposal",                     /* SADB_EXT_PROPOSAL             13 */
  "supported-auth",               /* SADB_EXT_SUPPORTED_AUTH       14 */
  "supported-cipher",             /* SADB_EXT_SUPPORTED_ENCRYPT    15 */
  "spi-range",                    /* SADB_EXT_SPIRANGE             16 */
  "X-kmpprivate",                 /* SADB_X_EXT_KMPRIVATE          17 */
  "X-satype2",                    /* SADB_X_EXT_SATYPE2            18 */
  "X-security-association",       /* SADB_X_EXT_SA2                19 */
  "X-destination-address2",       /* SADB_X_EXT_ADDRESS_DST2       20 */
  "X-source-flow-address",        /* SADB_X_EXT_ADDRESS_SRC_FLOW   21 */
  "X-dest-flow-address",          /* SADB_X_EXT_ADDRESS_DST_FLOW   22 */
  "X-source-mask",                /* SADB_X_EXT_ADDRESS_SRC_MASK   23 */
  "X-dest-mask",                  /* SADB_X_EXT_ADDRESS_DST_MASK   24 */
  "X-source-to",                  /* SADB_X_EXT_ADDRESS_SRC_TO     25 */
  "X-dest-to",                    /* SADB_X_EXT_ADDRESS_DST_TO     26 */
  "X-set-debug",                  /* SADB_X_EXT_DEBUG              27 */
  /* NAT_TRAVERSAL */
  "X-NAT-T-type",                 /* SADB_X_EXT_NAT_T_TYPE         26 */
  "X-NAT-T-sport",                /* SADB_X_EXT_NAT_T_SPORT        27 */
  "X-NAT-T-dport",                /* SADB_X_EXT_NAT_T_DPORT        28 */
  "X-NAT-T-OA",                   /* SADB_X_EXT_NAT_T_OA           29 */
};

const char *
pfkey_v2_sadb_ext_string(int ext)
{
  if(ext < SADB_EXT_MAX) {
    return pfkey_sadb_ext_strings[ext];
  } else {
    return "unknown-ext";
  }
}


static char *pfkey_sadb_type_strings[]={
	"reserved",                     /* SADB_RESERVED      */
	"getspi",                       /* SADB_GETSPI        */
	"update",                       /* SADB_UPDATE        */
	"add",                          /* SADB_ADD           */
	"delete",                       /* SADB_DELETE        */
	"get",                          /* SADB_GET           */
	"acquire",                      /* SADB_ACQUIRE       */
	"register",                     /* SADB_REGISTER      */
	"expire",                       /* SADB_EXPIRE        */
	"flush",                        /* SADB_FLUSH         */
	"dump",                         /* SADB_DUMP          */
	"x-promisc",                    /* SADB_X_PROMISC     */
	"x-pchange",                    /* SADB_X_PCHANGE     */
	"x-groupsa",                    /* SADB_X_GRPSA       */
	"x-addflow(eroute)",            /* SADB_X_ADDFLOW     */
	"x-delflow(eroute)",            /* SADB_X_DELFLOW     */
	"x-debug",                      /* SADB_X_DEBUG       */
};

const char *
pfkey_v2_sadb_type_string(int sadb_type)
{
  if(sadb_type < SADB_MAX) {
    return pfkey_sadb_type_strings[sadb_type];
  } else {
    return "unknown-sadb-type";
  }
}




/*
 * $Log: pfkey_v2_debug.c,v $
 * Revision 1.6  2006/02/23 17:36:58  sergey
 * AUTO MERGE: 1 <- branch-4_2
 * B6059: implement (OpenSWAN based) IPSec NAT-Traversal.
 * NOTIFY: automerge
 *
 * Revision 1.5.8.1  2006/02/23 17:32:28  sergey
 * B6059: implement (OpenSWAN based) IPSec NAT-Traversal.
 *
 * Revision 1.5  2004/11/14 11:18:31  noams
 * B16557 Merge multi_dist compilation from 4_0 to dev.
 *
 * Revision 1.4.64.1  2004/11/11 18:04:40  noams
 * B16557 Merge from tag-3_14_25 to branch-3_14-multi_dist-end into branch-4_0.
 *
 * Revision 1.4.58.1  2004/09/26 14:45:56  noams
 * OpenRG/UML builds and runs OK.
 *
 * Revision 1.4  2003/09/21 20:24:03  igork
 * merge branch-dev-2421 into dev
 *
 * Revision 1.1.1.1.34.1  2003/09/16 13:34:34  ron
 * merge from branch-3_1 to merge-dev-branch-dev-2421 into branch-dev-2421
 *
 * Revision 1.3  2003/09/11 11:38:03  yoavp
 * AUTO MERGE: 1 <- branch-3_2
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 * NOTIFY: automerge
 *
 * Revision 1.2.2.1  2003/09/11 11:34:48  yoavp
 * R7234: add NONE to range-types, remove RCSIDs from freeswan and some cosmetics
 *
 * Revision 1.2  2003/09/01 13:24:26  yoavp
 * fix B5435, B5436: support ranges in local and remote lans
 *
 * Revision 1.1.1.1  2003/02/19 11:46:31  sergey
 * upgrading freeswan to ver. 1.99.
 *
 * Revision 1.4  2002/01/29 22:25:36  rgb
 * Re-add ipsec_kversion.h to keep MALLOC happy.
 *
 * Revision 1.3  2002/01/29 01:59:09  mcr
 * 	removal of kversions.h - sources that needed it now use ipsec_param.h.
 * 	updating of IPv6 structures to match latest in6.h version.
 * 	removed dead code from freeswan.h that also duplicated kversions.h
 * 	code.
 *
 * Revision 1.2  2002/01/20 20:34:50  mcr
 * 	added pfkey_v2_sadb_type_string to decode sadb_type to string.
 *
 * Revision 1.1  2001/11/27 05:30:06  mcr
 * 	initial set of debug strings for pfkey debugging.
 * 	this will eventually only be included for debug builds.
 *
 * Revision 1.1  2001/09/21 04:12:03  mcr
 * 	first compilable version.
 *
 *
 * Local variables:
 * c-file-style: "linux"
 * End:
 *
 */
