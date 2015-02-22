/*
 * IPSEC <> netlink interface
 * Copyright (C) 1996, 1997  John Ioannidis.
 * Copyright (C) 1998, 1999, 2000, 2001  Richard Guy Briggs.
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
 */

#include <linux/stddef.h>

#ifndef NETLINK_IPSEC
#define NETLINK_IPSEC          10      /* IPSEC */ 
#endif /* !NETLINK_IPSEC */

#define EM_MAXRELSPIS	4		/* at most five chained xforms */
#define EM_MAGIC	0x5377616e	/* "Swan" */

#define EMT_IFADDR	1	/* set enc if addr */
#define EMT_SETSPI	2	/* Set SPI properties */
#define EMT_DELSPI	3	/* Delete an SPI */
#define EMT_GRPSPIS	4	/* Group SPIs (output order)  */
#define EMT_SETEROUTE	5	/* set an extended route */
#define EMT_DELEROUTE	6	/* del an extended route */
#define EMT_TESTROUTE	7	/* try to find route, print to console */
#define EMT_SETDEBUG	8	/* set debug level if active */
#define EMT_UNGRPSPIS	9	/* UnGroup SPIs (output order)  */
#define EMT_CLREROUTE	10	/* clear the extended route table */
#define EMT_CLRSPIS	11	/* clear the spi table */
#define EMT_REPLACEROUTE	12	/* set an extended route */
#define EMT_GETDEBUG	13	/* get debug level if active */
#define EMT_INEROUTE	14	/* set incoming policy for IPIP on a chain */

#ifdef CONFIG_IPSEC_DEBUG
#define DB_NL_TDBCB	0x0001
#endif /* CONFIG_IPSEC_DEBUG */

/* em_flags constants */
/* be mindful that this flag conflicts with SADB_SAFLAGS_PFS in pfkeyv2 */
/* perhaps it should be moved... */
#define EMT_INBOUND	0x01	/* SA direction, 1=inbound */

struct encap_msghdr
{
	__u32	em_magic;		/* EM_MAGIC */
#if 0
	__u16	em_msglen;		/* message length */
#endif
	__u8	em_msglen;		/* message length */
	__u8	em_flags;		/* message flags */
	__u8	em_version;		/* for future expansion */
	__u8	em_type;		/* message type */
	union
	{
		__u8	C;		/* Free-text */
		
		struct 
		{
			struct sa_id Said; /* SA ID */
			struct sockaddr_encap Eaddr;
			struct sockaddr_encap Emask;
		} Ert;

		struct
		{
			struct in_addr Ia;
			__u8	Ifn;
			__u8  xxx[3];	/* makes life a lot easier */
		} Ifa;

		struct
		{
			struct sa_id Said; /* SA ID */
			int If;		/* enc i/f for input */
			int Alg;	/* Algorithm to use */

                        /* The following union is a surrogate for
                         * algorithm-specific data.  To insure
                         * proper alignment, worst-case fields
                         * should be included.  It would be even
                         * better to include the types that will
                         * actually be used, but they may not be
                         * defined for each use of this header.
                         * The actual length is expected to be longer
                         * than is declared here.  References are normally
                         * made using the em_dat macro, as if it were a
                         * field name.
                         */
                        union { /* Data */
                                __u8 Dat[1];
                                __u64 Datq[1];  /* maximal alignment (?) */
                        } u;
		} Xfm;
		
		struct
		{
			struct sa_id emr_said; /* SA ID */
			struct ipsec_sa * emr_tdb; /* used internally! */
			
		} Rel[EM_MAXRELSPIS];
		
#ifdef CONFIG_IPSEC_DEBUG
		struct
		{
			int debug_tunnel;
			int debug_netlink;
			int debug_xform;
			int debug_eroute;
			int debug_spi;
			int debug_radij;
			int debug_esp;
			int debug_ah;
			int debug_rcv;
			int debug_pfkey;
			int debug_ipcomp;
			int debug_verbose;
			int debug_reject;
			int debug_log_all;
		} Dbg;
#endif /* CONFIG_IPSEC_DEBUG */
	} Eu;
};

#define EM_MINLEN	offsetof(struct encap_msghdr, Eu)
#define EMT_SETSPI_FLEN	offsetof(struct encap_msghdr, em_dat)
#define EMT_GRPSPIS_FLEN offsetof(struct encap_msghdr, Eu.Rel)
#define EMT_SETDEBUG_FLEN (offsetof(struct encap_msghdr, Eu.Dbg + \
			sizeof(((struct encap_msghdr*)0)->Eu.Dbg)))

#define em_c	Eu.C
#define em_eaddr Eu.Ert.Eaddr
#define em_emask Eu.Ert.Emask
#define em_ersaid Eu.Ert.Said
#define em_erdst Eu.Ert.Said.dst
#define em_erspi Eu.Ert.Said.spi
#define em_erproto Eu.Ert.Said.proto

#define em_ifa	Eu.Ifa.Ia
#define em_ifn	Eu.Ifa.Ifn

#define em_said	Eu.Xfm.Said
#define em_spi	Eu.Xfm.Said.spi
#define em_dst	Eu.Xfm.Said.dst
#define em_proto	Eu.Xfm.Said.proto
#define em_if	Eu.Xfm.If
#define em_alg	Eu.Xfm.Alg
#define em_dat	Eu.Xfm.u.Dat

#define em_rel	Eu.Rel
#define emr_dst emr_said.dst
#define emr_spi emr_said.spi
#define emr_proto emr_said.proto

#ifdef CONFIG_IPSEC_DEBUG
#define em_db_tn Eu.Dbg.debug_tunnel
#define em_db_nl Eu.Dbg.debug_netlink
#define em_db_xf Eu.Dbg.debug_xform
#define em_db_er Eu.Dbg.debug_eroute
#define em_db_sp Eu.Dbg.debug_spi
#define em_db_rj Eu.Dbg.debug_radij
#define em_db_es Eu.Dbg.debug_esp
#define em_db_ah Eu.Dbg.debug_ah
#define em_db_rx Eu.Dbg.debug_rcv
#define em_db_ky Eu.Dbg.debug_pfkey
#define em_db_gz Eu.Dbg.debug_ipcomp
#define em_db_vb Eu.Dbg.debug_verbose
#define em_db_rt Eu.Dbg.debug_reject
#define em_db_la Eu.Dbg.debug_log_all
#endif /* CONFIG_IPSEC_DEBUG */

#ifdef __KERNEL__
extern char ipsec_netlink_c_version[];
#ifndef KERNEL_VERSION
#  include <linux/version.h>
#endif
#ifdef NETLINK_SOCK
extern int ipsec_callback(int proto, struct sk_buff *skb);
#else /* NETLINK_SOCK */
extern int ipsec_callback(struct sk_buff *skb);
#endif /* NETLINK_SOCK */

#ifdef CONFIG_IPSEC_DEBUG
extern int debug_netlink;
#endif /* CONFIG_IPSEC_DEBUG */
#endif /* __KERNEL__ */

