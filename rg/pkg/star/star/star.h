/* @(#)star.h	1.62 02/05/11 Copyright 1985, 1995-2001 J. Schilling */
/*
 *	Copyright (c) 1985, 1995-2001 J. Schilling
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef	_STAR_H
#define	_STAR_H

#include <utypes.h>
#include <timedefs.h>

#ifndef	_INCL_SYS_TYPES_H
#include <sys/types.h>
#define	_INCL_SYS_TYPES_H
#endif

/*
 * Be careful not to overflow off_t when computing tarblocks()
 */
#define tarblocks(s)	(((s) / TBLOCK) + (((s)%TBLOCK)?1:0))
#define tarsize(s)	(tarblocks(s) * TBLOCK)

/*
 * Defines for header type recognition
 * N.B. these must kept in sync with hdrtxt[] in header.c
 */
#define	H_SWAPPED(t)	((-1)*(t))
#define	H_ISSWAPPED(t)	((t) < H_UNDEF)
#define	H_TYPE(t)	((int)(H_ISSWAPPED(t) ? ((-1)*(t)):(t)))
#define	H_UNDEF		0
#define	H_TAR		1	/* tar unbekanntes format */
#define	H_OTAR		2	/* tar altes format (1978 ???) */
#define	H_STAR		3	/* altes star format (1985) */
#define	H_GNUTAR	4	/* gnu tar format (1989) */
#define	H_USTAR		5	/* ieee 1003.1-1988 format (1987 ff.) */
#define	H_XSTAR		6	/* extended 1003.1-1988 format (1994) */
#define	H_XUSTAR	7	/* ext 1003.1-1988 format without "tar" signature (1998) */
#define	H_EXUSTAR	8	/* ext 1003.1-2001 format without "tar" signature (2001) */
#define	H_PAX		9	/* ieee 1003.1-2001 extended ustar format called PAX */
#define	H_SUNTAR	10	/* Sun's tar implementaion from Solaris 7/8/9 */
#define	H_RES11		11	/* Reserved */
#define	H_RES12		12	/* Reserved */
#define	H_RES13		13	/* Reserved */
#define	H_RES14		14	/* Reserved */
#define	H_BAR		15	/* SUN bar format */
#define	H_CPIO_BASE	16	/* cpio Basis */
#define	H_CPIO_BIN	16	/* cpio Binär */
#define	H_CPIO_CHR	17	/* cpio -c format */
#define	H_CPIO_NBIN	18	/* cpio neu Binär */
#define	H_CPIO_CRC	19	/* cpio crc Binär */
#define	H_CPIO_ASC	20	/* cpio ascii expanded maj/min */
#define	H_CPIO_ACRC	21	/* cpio crc expanded maj/min */
#define	H_CPIO_MAX	21	/* cpio Ende */
#define	H_MAX_ARCH	21	/* Highest possible # */

/*
 * Return codes from compression type checker.
 */
#define	C_NONE		0	/* Not compressed or unknown compression    */
#define	C_GZIP		1	/* Compression may be unpacked with 'bzip'  */
#define	C_BZIP2		2	/* Compression may be unpacked with 'bzip2' */


/*
 * POSIX.1-1988 field size values and magic.
 */
#define TBLOCK		512
#define NAMSIZ		100
#define	PFXSIZ		155

#define	TMODLEN		8
#define	TUIDLEN		8
#define	TGIDLEN		8
#define	TSIZLEN		12
#define	TMTMLEN		12
#define	TCKSLEN		8

#define	TMAGIC		"ustar"	/* ustar magic */
#define	TMAGLEN		6	/* "ustar" including NULL byte */
#define	TVERSION	"00"
#define	TVERSLEN	2
#define	TUNMLEN		32
#define	TGNMLEN		32
#define	TDEVLEN		8

/*
 * The maximum number that we may handle with a 32 bit int
 */
#define	MAXINT32	0x7FFFFFFFL

/*
 * Large file summit: max size of a non-large file (2 GB - 2 Bytes) 
 */
#define	MAXNONLARGEFILE	(MAXINT32 - 1)

/*
 * Max POSIX.1-1988 limit for numeric 12 byte fields such as size/mtime
 */
#ifdef	USE_LONGLONG
#define	MAXOCTAL11	077777777777ULL
#else
#define	MAXOCTAL11	MAXINT32
#endif

/*
 * Max POSIX.1-1988 limit for numeric 8 byte fields such as uid/gid/dev
 */
#define	MAXOCTAL7	07777777

/*
 * Pre POSIX.1-1988 limit for numeric 8 byte fields such as uid/gid/dev
 */
#define	MAXOCTAL6	0777777

/*
 * Non POSIX.1-1988 limit used by HP-UX tar for 8 byte devmajor/devminor
 */
#define	MAXOCTAL8	077777777


/*
 * POSIX.1-1988 typeflag values
 */
#define	REGTYPE		'0'
#define	AREGTYPE	'\0'
#define	LNKTYPE		'1'
#define	SYMTYPE		'2'
#define	CHRTYPE		'3'
#define	BLKTYPE		'4'
#define	DIRTYPE		'5'
#define	FIFOTYPE	'6'
#define	CONTTYPE	'7'

/*
 * POSIX.1-2001 typeflag extensions.
 * POSIX.1-2001 calls the extended USTAR format PAX although it is definitely
 * derived from and based on USTAR. The reason may be that POSIX.1-2001
 * calls the tar program outdated and lists the pax program as the successor.
 */
#define	LF_GHDR		'g'	/* POSIX.1-2001 global extended header */
#define	LF_XHDR		'x'	/* POSIX.1-2001 extended header */

/*
 * star/gnu/Sun tar extensions:
 */
/* Note that the standards committee allows only capital A through
   capital Z for user-defined expansion.  This means that defining something
   as, say '8' is a *bad* idea. */

#define	LF_ACL		'A'	/* Solaris Access Control List	*/
#define LF_DUMPDIR	'D'	/* This is a dir entry that contains
					   the names of files that were in
					   the dir at the time the dump
					   was made */
#define	LF_EXTATTR	'E'	/* Solaris Extended Attribute File	*/
#define	LF_META		'I'	/* Inode (metadata only) no file content*/
#define LF_LONGLINK	'K'	/* Identifies the NEXT file on the tape
					   as having a long linkname */
#define LF_LONGNAME	'L'	/* Identifies the NEXT file on the tape
					   as having a long name. */
#define LF_MULTIVOL	'M'	/* This is the continuation
					   of a file that began on another
					   volume */
#define LF_NAMES	'N'	/* For storing filenames that didn't
					   fit in 100 characters */
#define LF_SPARSE	'S'	/* This is for sparse files */
#define LF_VOLHDR	'V'	/* This file is a tape/volume header */
				/* Ignore it on extraction */
#define	LF_VU_XHDR	'X'	/* POSIX.1-2001 xtended (VU version) */

/*
 * Definitions for the t_mode field
 */
#define	TSUID		04000	/* Set UID on execution */
#define	TSGID		02000	/* Set GID on execution */
#define	TSVTX		01000	/* On directories, restricted deletion flag */
#define	TUREAD		00400	/* Read by owner */
#define	TUWRITE		00200	/* Write by owner special */
#define	TUEXEC		00100	/* Execute/search by owner */
#define	TGREAD		00040	/* Read by group */
#define	TGWRITE		00020	/* Write by group */
#define	TGEXEC		00010	/* Execute/search by group */
#define	TOREAD		00004	/* Read by other */
#define	TOWRITE		00002	/* Write by other */
#define	TOEXEC		00001	/* Execute/search by other */

#define	TALLMODES	07777	/* The low 12 bits mentioned in the standard */


/*
 * This is the ustar (Posix 1003.1) header.
 */
struct header {
	char t_name[NAMSIZ];	/*   0	Dateiname	*/
	char t_mode[8];		/* 100	Zugriffsrechte 	*/
	char t_uid[8];		/* 108	Benutzernummer	*/
	char t_gid[8];		/* 116	Benutzergruppe	*/
	char t_size[12];	/* 124	Dateigroesze	*/
	char t_mtime[12];	/* 136	Zeit d. letzten Aenderung */
	char t_chksum[8];	/* 148	Checksumme	*/
	char t_typeflag;	/* 156	Typ der Datei	*/
	char t_linkname[NAMSIZ];/* 157	Zielname des Links */
	char t_magic[TMAGLEN];	/* 257	"ustar"		*/
	char t_version[TVERSLEN];/*263	Version v. star	*/
	char t_uname[TUNMLEN];	/* 265	Benutzername	*/
	char t_gname[TGNMLEN];	/* 297	Gruppenname	*/
	char t_devmajor[8];	/* 329	Major bei Geraeten */
	char t_devminor[8];	/* 337	Minor bei Geraeten */
	char t_prefix[PFXSIZ];	/* 345	Prefix fuer t_name */
				/* 500	Ende		*/
	char t_mfill[12];	/* 500	Filler bis 512	*/
};

/*
 * star header specific definitions
 */
#define	STMAGIC		"tar"	/* star magic */
#define	STMAGLEN	4	/* "tar" including NULL byte */
#define	STVERSION	'1'	/* current star version */

#define	STUNMLEN	16	/* star user name length */
#define	STGNMLEN	15	/* star group name length */

/*
 * This is the old (pre Posix 1003.1-1988) star header defined in 1985.
 */
struct star_header {
	char t_name[NAMSIZ];	/*   0	Dateiname	*/
	char t_mode[8];		/* 100	Zugriffsrechte 	*/
	char t_uid[8];		/* 108	Benutzernummer	*/
	char t_gid[8];		/* 116	Benutzergruppe	*/
	char t_size[12];	/* 124	Dateigroesze	*/
	char t_mtime[12];	/* 136	Zeit d. letzten Aenderung */
	char t_chksum[8];	/* 148	Checksumme	*/
	char t_linkflag;	/* 156	Linktyp der Datei */
	char t_linkname[NAMSIZ];/* 157	Zielname des Links */
				/* ---	Ende historisches TAR */
				/* ---	Anfang star Erweiterungen */
	char t_vers;		/* 257	Version v. star	*/
	char t_filetype[8];	/* 258	Interner Dateityp */
	char t_type[12];	/* 266	Dateityp (UNIX)	*/
#ifdef	no_minor_bits_in_rdev
	char t_rdev[12];	/* 278	Major/Minor bei Geraeten */
#else
	char t_rdev[11];	/* 278	Major/Minor bei Geraeten */
	char t_devminorbits;	/* 298	Anzahl d. Minor Bits in t_rdev */
#endif
	char t_atime[12];	/* 290	Zeit d. letzten Zugriffs */
	char t_ctime[12];	/* 302	Zeit d. letzten Statusaend. */
	char t_uname[STUNMLEN];	/* 314	Benutzername	*/
	char t_gname[STGNMLEN];	/* 330	Gruppenname	*/
	char t_prefix[PFXSIZ];	/* 345	Prefix fuer t_name */
	char t_mfill[8];	/* 500	Filler bis magic */
	char t_magic[4];	/* 508	"tar"		*/
};

/*
 * This is the new (post Posix 1003.1-1988) xstar header defined in 1994.
 *
 * t_prefix[130]	is garanteed to be '\0' to prevent ustar compliant
 *			implementations from failing.
 * t_mfill & t_xmagic	need to be zero for a 100% ustar compliant
 *			implementation, so setting t_xmagic to "tar" should
 *			be avoided in the future.
 *
 * A different method to recognise this format is to verify that
 * t_prefix[130]	is equal to '\0' and
 * t_atime[0]/t_ctime[0] is an octal number and
 * t_atime[11]		is equal to ' ' and
 * t_ctime[11]		is equal to ' '.
 *
 * Note that t_atime[11]/t_ctime[11] may be changed in future.
 */
struct xstar_header {
	char t_name[NAMSIZ];	/*   0	Dateiname	*/
	char t_mode[8];		/* 100	Zugriffsrechte 	*/
	char t_uid[8];		/* 108	Benutzernummer	*/
	char t_gid[8];		/* 116	Benutzergruppe	*/
	char t_size[12];	/* 124	Dateigroesze	*/
	char t_mtime[12];	/* 136	Zeit d. letzten Aenderung */
	char t_chksum[8];	/* 148	Checksumme	*/
	char t_typeflag;	/* 156	Typ der Datei	*/
	char t_linkname[NAMSIZ];/* 157	Zielname des Links */
	char t_magic[TMAGLEN];	/* 257	"ustar"		*/
	char t_version[TVERSLEN];/*263	Version v. star	*/
	char t_uname[TUNMLEN];	/* 265	Benutzername	*/
	char t_gname[TGNMLEN];	/* 297	Gruppenname	*/
	char t_devmajor[8];	/* 329	Major bei Geraeten */
	char t_devminor[8];	/* 337	Minor bei Geraeten */
	char t_prefix[131];	/* 345	Prefix fuer t_name */
	char t_atime[12];	/* 476	Zeit d. letzten Zugriffs */
	char t_ctime[12];	/* 488	Zeit d. letzten Statusaend. */
	char t_mfill[8];	/* 500	Filler bis magic */
	char t_xmagic[4];	/* 508	"tar"		*/
};

struct sparse {
	char t_offset[12];
	char t_numbytes[12];
};

#define SPARSE_EXT_HDR  21
#define SPARSE_IN_HDR	4
#define	SIH		SPARSE_IN_HDR
#define	SEH		SPARSE_EXT_HDR

struct xstar_in_header {
	char t_fill[345];	/*   0  Everything that is before t_prefix */
	char t_prefix[1];	/* 345	Prefix fuer t_name */
	char t_fill2;		/* 346  */
	char t_fill3[8];	/* 347  */
	char t_isextended;	/* 355	*/
	struct sparse t_sp[SIH];/* 356	8 x 12		*/
	char t_realsize[12];	/* 452	Echte Größe bei sparse Dateien */
	char t_offset[12];	/* 464	Offset für Multivol cont. Dateien */
	char t_atime[12];	/* 476	Zeit d. letzten Zugriffs */
	char t_ctime[12];	/* 488	Zeit d. letzten Statusaend. */
	char t_mfill[8];	/* 500	Filler bis magic */
	char t_xmagic[4];	/* 508	"tar"		*/
};

struct xstar_ext_header {
	struct sparse t_sp[SEH];
	char t_isextended;
};

typedef struct {
	off_t	sp_offset;
	off_t	sp_numbytes;
} sp_t;

/*
 * gnu tar header specific definitions
 */

#define	GMAGIC		"ustar  "/* gnu tar magic */
#define	GMAGLEN		8	/* "ustar" two blanks and a NULL */

/*
 * This is the GNUtar header defined in 1989.
 *
 * The nonstandard stuff could not be found in in the first pubslished versions
 * of the program. The first version I am aware of, is a program called SUGtar
 * published at the Sun User Group meeting in december 1987, a different
 * publishing of the same program which has been originally written by
 * John Gilmore was called PDtar. In 1987 PDtar/SUGtar was implementing a true
 * subset of the 1987 POSIX-1003 draft (missing only the long name splitting).
 *
 * FSF people then later added t_atime... making GNU tar non POSIX compliant.
 * When FSF added the sparse file handling stuff, this was done in a way that
 * even violates any tar document available since the late 1970's.
 *
 * GNU tar is not tar...
 */
struct gnu_header {
	char t_name[NAMSIZ];	/*   0	Dateiname	*/
	char t_mode[8];		/* 100	Zugriffsrechte	*/
	char t_uid[8];		/* 108	Benutzernummer	*/
	char t_gid[8];		/* 116	Benutzergruppe	*/
	char t_size[12];	/* 124	Dateigroesze	*/
	char t_mtime[12];	/* 136	Zeit d. letzten Aenderung */
	char t_chksum[8];	/* 148	Checksumme	*/
	char t_linkflag;	/* 156	Typ der Datei	*/
	char t_linkname[NAMSIZ];/* 157	Zielname des Links */
	char t_magic[8];	/* 257	"ustar"		*/
	char t_uname[TUNMLEN];	/* 265	Benutzername	*/
	char t_gname[TGNMLEN];	/* 297	Gruppenname	*/
	char t_devmajor[8];	/* 329	Major bei Geraeten */
	char t_devminor[8];	/* 337	Minor bei Geraeten */

/*	Jay Fenlason (hack@ai.mit.edu) */
	/* these following fields were added by JF for gnu */
	/* and are NOT standard */
	char t_atime[12];	/* 345	*/
	char t_ctime[12];	/* 357	*/
	char t_offset[12];	/* 369	*/
	char t_longnames[4];	/* 381	*/
	/*
	 * for the rest see struct gnu_in_header
	 */
};

struct gnu_in_header {
	char	t_fill[386];
	struct sparse t_sp[SIH];/* 386	4 sparse structures (2 x 12 bytes) */
	char t_isextended;	/* 482	an extended header follows	   */
	char t_realsize[12];	/* 483  true size of the sparse file	   */
};

struct gnu_extended_header {
	struct sparse t_sp[SEH];/*   0  21 sparse structures (2 x 12 bytes) */
	char t_isextended;	/* 504  another extended header follows	    */
};

#undef	SIH
#undef	SEH

#define	BAR_UNSPEC	'\0'	/* XXX Volheader ??? */
#define	BAR_REGTYPE	'0'
#define	BAR_LNKTYPE	'1'
#define	BAR_SYMTYPE	'2'
#define	BAR_SPECIAL	'3'

#define	BAR_VOLHEAD	"V"	/* The BAR Volume "magic" */

/*
 * The Sun BAR header format as introduced with the Roadrunner Intel machines
 *
 * All header parts marked with "*VH" are set only in the volheader
 * and zero on any other headers.
 */
struct bar_header {
	char mode[8];		/*   0	file type and mode (top bit masked) */
	char uid[8];		/*   8	Benutzernummer		*/
	char gid[8];		/*  16	Benutzergruppe		*/
	char size[12];		/*  24	Dateigroesze		*/
	char mtime[12];		/*  36	Zeit d. letzten Aenderung */
	char t_chksum[8];	/*  48	Checksumme		*/
	char rdev[8];		/*  56	Major/Minor bei Geraeten */
	char linkflag;		/*  64	Linktyp der Datei	*/
	char bar_magic[2];	/*  65	*VH xxx			*/
	char volume_num[4];	/*  67	*VH Volume Nummer	*/
	char compressed;	/*  71	*VH Compress Flag	*/
	char date[12];		/*  72	*VH Aktuelles Datum YYMMDDhhmm */
	char start_of_name[1];	/*  84	Dateiname		*/
};

typedef union hblock {
	char dummy[TBLOCK];
	long ldummy[TBLOCK/sizeof(long)];	/* force long alignement */
	struct star_header dbuf;
	struct star_header star_dbuf;
	struct xstar_header xstar_dbuf;
	struct xstar_in_header xstar_in_dbuf;
	struct xstar_ext_header xstar_ext_dbuf;
	struct header ustar_dbuf;
	struct gnu_header gnu_dbuf;
	struct gnu_in_header gnu_in_dbuf;
	struct gnu_extended_header gnu_ext_dbuf;
	struct bar_header bar_dbuf;
} TCB;

/*
 * Our internal OS independant structure to hold file metadata.
 *
 * Some remarks to the different file type structure members:
 *
 *	f_xftype	The new tar general (x-tended) file type.
 *			This includes values XT_LINK XT_SPARSE XT_LONGNAME ...
 *
 *	f_rxftype	The 'real' general file type.
 *			Doesn't include XT_LINK XT_SPARSE XT_LONGNAME ...
 *			This is the 'real' file type and close to what has been
 *			set up in getinfo().
 *
 *	f_filetype	The coarse file type classification (star 1985 header)
 *
 *	f_typeflag	The file type flag used in the POSIX.1-1988 TAR header
 *
 *	f_type		The OS specific file type (e.g. UNIX st_mode & S_IFMT)
 */
typedef	struct	{
	TCB	*f_tcb;
	char	*f_name;	/* Zeiger auf den langen Dateinamen */
	Ulong	f_namelen;	/* Länge des Dateinamens */
	char	*f_lname;	/* Zeiger auf den langen Linknamen */
	Ulong	f_lnamelen;	/* Länge des Linknamens */
	char	*f_uname;	/* User name oder NULL Pointer */
	Ulong	f_umaxlen;	/* Maximale Länge des Usernamens*/
	char	*f_gname;	/* Group name oder NULL Pointer */
	Ulong	f_gmaxlen;	/* Maximale Länge des Gruppennamens*/
	dev_t	f_dev;		/* Geraet auf dem sich d. Datei befindet */
	ino_t	f_ino;		/* Dateinummer			*/
	Ulong	f_nlink;	/* Anzahl der Links		*/
	Ulong	f_mode;		/* Zugriffsrechte 		*/
	Ulong	f_uid;		/* Benutzernummer		*/
	Ulong	f_gid;		/* Benutzergruppe		*/
	off_t	f_size;		/* Dateigroesze			*/
	off_t	f_rsize;	/* Dateigroesze auf Band	*/
	off_t	f_contoffset;	/* Offset für Multivol cont. Dateien */
	Ulong	f_flags;	/* Bearbeitungshinweise		*/
	Ulong	f_xflags;	/* Flags für x-header		*/
	Ulong	f_xftype;	/* Header Dateityp (neu generell)*/
	Ulong	f_rxftype;	/* Echter Dateityp (neu generell)*/
	Ulong	f_filetype;	/* Typ der Datei (star alt)	*/
	Uchar	f_typeflag;	/* Kopie aus TAR Header		*/
	Ulong	f_type;		/* Dateityp aus UNIX struct stat*/
	Ulong	f_rdev;		/* Major/Minor bei Geraeten	*/
	Ulong	f_rdevmaj;	/* Major bei Geraeten		*/
	Ulong	f_rdevmin;	/* Minor bei Geraeten		*/
	time_t	f_atime;	/* Zeit d. letzten Zugriffs	*/
	Ulong	f_ansec;	/* nsec Teil "			*/
	time_t	f_mtime;	/* Zeit d. letzten Aenderung	*/
	Ulong	f_mnsec;	/* nsec Teil "			*/
	time_t	f_ctime;	/* Zeit d. letzten Statusaend.	*/
	Ulong	f_cnsec;	/* nsec Teil "			*/
	Ulong	f_fflags;	/* File flags			*/
#ifdef	USE_ACL
	char	*f_acl_access;	/* POSIX Access Control List	*/
	char	*f_acl_default;	/* POSIX Default ACL		*/
#endif
} FINFO;

/*
 * Used with f_flags
 */
#define	F_LONGNAME	0x01	/* Langer Name passt nicht in Header	     */
#define	F_LONGLINK	0x02	/* Langer Linkname passt nicht in Header     */
#define	F_SPLIT_NAME	0x04	/* Langer Name wurde gesplitted		     */
#define	F_HAS_NAME	0x08	/* Langer Name in f_name/f_lname soll bleiben*/
#define	F_SPARSE	0x10	/* Datei enthält Löcher			     */
#define	F_TCB_BUF	0x20	/* TCB ist/war vom Buffer alloziert	     */
#define	F_ADDSLASH	0x40	/* Langer Name benötigt Slash am Ende	     */
#define	F_NSECS		0x80	/* stat() liefert Nanosekunden		     */
#define	F_NODUMP	0x100	/* Datei hat OS spezifisches NODUMP flag     */

/*
 * Used with f_xflags
 */
#define	XF_ATIME	0x0001	/* Zeit d. letzten Zugriffs	*/
#define	XF_CTIME	0x0002	/* Zeit d. letzten Statusaend.	*/
#define	XF_MTIME	0x0004	/* Zeit d. letzten Aenderung	*/
#define	XF_COMMENT	0x0008	/* Beliebiger Kommentar		*/
#define	XF_UID		0x0010	/* Benutzernummer		*/
#define	XF_UNAME	0x0020	/* Langer Benutzername		*/
#define	XF_GID		0x0040	/* Benutzergruppe		*/
#define	XF_GNAME	0x0080	/* Langer Benutzergruppenname	*/
#define	XF_PATH		0x0100	/* Langer Name			*/
#define	XF_LINKPATH	0x0200	/* Langer Link Name		*/
#define	XF_SIZE		0x0400	/* Dateigröße wenn > 8 GB	*/
#define	XF_CHARSET	0x0800	/* Zeichensatz für Dateiinhalte	*/

#define	XF_DEVMAJOR	0x1000	/* Major bei Geräten		*/
#define	XF_DEVMINOR	0x2000	/* Major bei Geräten		*/

#define XF_ACL_ACCESS	0x4000	/* POSIX Access Control List	*/
#define XF_ACL_DEFAULT	0x8000	/* POSIX Default ACL		*/

#define XF_FFLAGS	0x10000	/* File flags			*/

#define	XF_NOTIME    0x10000000	/* Keine extended Zeiten	*/

/*
 * Used with f_filetype
 *
 * This is optimised for the old star (1986) extensions that were the first
 * tar extensions which allowed to archive files different from regular files,
 * directories and symbolic links.
 */
#define	F_SPEC		0	/* Anything not mentioned below		     */
#define	F_FILE		1	/* A reguar file			     */
#define	F_SLINK		2	/* A symbolic link			     */
#define	F_DIR		3	/* A directory				     */

#define	is_special(i)	((i)->f_filetype == F_SPEC)
#define	is_file(i)	((i)->f_filetype == F_FILE)
#define	is_symlink(i)	((i)->f_filetype == F_SLINK)
#define	is_dir(i)	((i)->f_filetype == F_DIR)

#define	is_bdev(i)	((i)->f_xftype == XT_BLK)
#define	is_cdev(i)	((i)->f_xftype == XT_CHR)
#define	is_dev(i)	(is_bdev(i) || is_cdev(i))
#define	is_fifo(i)	((i)->f_xftype == XT_FIFO)
#define	is_door(i)	((i)->f_xftype == XT_DOOR)
#define	is_link(i)	((i)->f_xftype == XT_LINK)
#define	fis_link(i)	((i)->f_rxftype == XT_LINK)	/* Filetype unknown  */
#define	is_volhdr(i)	((i)->f_xftype == XT_VOLHDR)
#define	is_sparse(i)	((i)->f_xftype == XT_SPARSE)
#define	is_multivol(i)	((i)->f_xftype == XT_MULTIVOL)
#define	is_whiteout(i)	((i)->f_xftype == XT_WHT)
#define	is_meta(i)	((i)->f_xftype == XT_META)
#define	fis_meta(i)	((i)->f_rxftype == XT_META)	/* Really "regular"  */

#ifdef	isdigit
#undef	isdigit		/* Needed for HP-UX */
#endif
#define isdigit(c)	((c) >= '0' && (c) <= '9')
#define isoctal(c)	((c) >= '0' && (c) <= '7')
#ifdef	isupper
#undef	isupper		/* Needed for HP-UX */
#endif
#define	isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define	toupper(c)	(isupper(c) ? (c) : (c) - ('a' - 'A'))
/*
 * Needed for QNX
 */
#ifdef	max
#undef	max
#endif
#ifdef	min
#undef	min
#endif
#define	max(a,b)	((a) < (b) ? (b) : (a))
#define	min(a,b)	((a) < (b) ? (a) : (b))


struct star_stats {
	int	s_staterrs;	/* Could not stat(2) file	*/
#ifdef	USE_ACL
	int	s_getaclerrs;	/* Could not get ACL for file	*/
#endif
	int	s_openerrs;	/* Open/Create error for file	*/
	int	s_rwerrs;	/* Read/Write error from file	*/
	int	s_sizeerrs;	/* File changed size		*/
	int	s_misslinks;	/* Missing links to file	*/
	int	s_toolong;	/* File name too long		*/
	int	s_toobig;	/* File does not fit on one tape*/
	int	s_isspecial;	/* File is special - not dumped	*/
	/*
	 * Extract only....
	 */
	int	s_settime;	/* utimes() on file failed	*/
	int	s_setmodes;	/* chmod() on file failed	*/
#ifdef	USE_ACL
	int	s_badacl;	/* ACL could not be converted	*/
	int	s_setacl;	/* set ACL for file failed	*/
#endif
};

extern	struct	star_stats	xstats;


#ifdef	HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif

/*
 * NODEV may be in sys/param.h keep this definition past the include.
 */
#ifndef	NODEV
#define	NODEV	((dev_t)-1L)
#endif

#if !defined(PATH_MAX) && defined(MAXPATHLEN)
#define	PATH_MAX	MAXPATHLEN
#endif

#ifndef	PATH_MAX
#define	PATH_MAX	1024
#endif

/*
 * Make sure that regardless what the OS defines, star reserves
 * space for 1024 chars in filenames.
 */
#if	PATH_MAX < 1024
#undef	PATH_MAX
#define	PATH_MAX	1024
#endif

#ifdef	HAVE_LARGEFILES
/*
 * XXX Hack until fseeko()/ftello() are available everywhere or until
 * XXX we know a secure way to let autoconf ckeck for fseeko()/ftello()
 * XXX without defining FILE_OFFSETBITS to 64 in confdefs.h
 */
#	define	fseek	fseeko
#	define	ftell	ftello
#endif

#endif	/* _STAR_H */
