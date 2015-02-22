/* @(#)props.h	1.12 01/12/07 Copyright 1994 J. Schilling */
/*
 *	Properties definitions to handle different
 *	archive types
 *
 *	Copyright (c) 1994 J. Schilling
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

#include <utypes.h>

/*
 *	Properties to describe the different archive formats.
 *
 *	if pr_maxnamelen id == pr_maxsname, we cannot have long names
 *	besides file name splitting.
 *
 *	NOTE that part of the information in struct propertiesis available more
 *	than once. This is needed as different parts of the source need the
 *	information in different ways. Partly for performance reasons, partly
 *	because one method of storing the information is inappropriate for all
 *	places in the source.
 *
 *	If you add new features or information related to the fields
 *	pr_flags/pr_nflags or the fields pr_xftypetab[]/pr_typeflagtab[]
 *	take care of possible problems due to this fact.
 *
 */
struct properties {
	Ullong	pr_maxsize;		/* max file size */
	int	pr_flags;		/* gerneral flags */
	int	pr_xdflags;		/* default extended header flags */
	char	pr_fillc;		/* fill prefix for numbers in TCB */
	char	pr_xc;			/* typeflag used for extended headers */
	long	pr_diffmask;		/* diffopts not supported */
	int	pr_nflags;		/* name related flags */
	int	pr_maxnamelen;		/* max length for filename */
	int	pr_maxlnamelen;		/* max length for linkname */
	int	pr_maxsname;		/* max length for short filename */
	int	pr_maxslname;		/* max length for short linkname */
	int	pr_maxprefix;		/* max length of prefix if splitting */
	int	pr_sparse_in_hdr;	/* # of sparse entries in header */
	char	pr_xftypetab[32];	/* (*1) list of supported file types */
	char	pr_typeflagtab[256];	/* (*2) list of supported TCB typeflags */
};

/*
 * 1) pr_xftypetab is used when creating archives only.
 * 2) pr_typeflagtab is used when extracting archives only.
 */

/*
 * general flags
 */
#define	PR_POSIX_OCTAL		0x0001	/* left fill octal number with '0' */
#define	PR_LOCAL_STAR		0x0002	/* can handle local star filetypes */
#define	PR_LOCAL_GNU		0x0004	/* can handle local gnu filetypes */
#define	PR_SPARSE		0x0010	/* can handle sparse files	*/
#define	PR_GNU_SPARSE_BUG	0x0020	/* size does not contain ext. headers*/
#define	PR_VOLHDR		0x0100	/* can handle volume headers	*/
#define	PR_XHDR			0x0200	/* POSIX.1-2001 extended headers */

/*
 * name related flags
 */
#define	PR_POSIX_SPLIT		0x01	/* can do posix filename splitting */
#define	PR_PREFIX_REUSED	0x02	/* prefix space used by other option */
#define	PR_LONG_NAMES		0x04	/* can handle very long names	*/
#define	PR_DUMB_EOF		0x10	/* handle t_name[0] == '\0' as EOF */

/*
 * Macro to make pr_xftypetab easier to use. See also table.h/table.c.
 */
#define	pr_unsuptype(i)		(props.pr_xftypetab[(i)->f_xftype] == 0)

/*
 * typeflagtab related flags
 */
#define	TF_VALIDTYPE		0x01	/* A valid typeflag for extraction */
#define	TF_XHEADERS		0x02	/* This is a valid extended header */

#define	_pr_typeflags(c)	(props.pr_typeflagtab[(Uchar)(c)])
#define	pr_validtype(c)		((_pr_typeflags(c) & TF_VALIDTYPE) != 0)
#define	pr_isxheader(c)		((_pr_typeflags(c) & TF_XHEADERS) != 0)

extern	struct properties	props;
