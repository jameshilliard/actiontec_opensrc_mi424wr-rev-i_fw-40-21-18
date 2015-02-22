/* @(#)fflags.c	1.4 02/01/19 Copyright 2001-2002 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)fflags.c	1.4 02/01/19 Copyright 2001-2002 J. Schilling";
#endif
/*
 *	Routines to handle extended file flags
 *
 *	Copyright (c) 2001-2002 J. Schilling
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

#include <mconfig.h>

#ifdef	USE_FFLAGS
#include <stdio.h>
#include <errno.h>
#include "star.h"
#include "props.h"
#include "table.h"
#include <standard.h>
#include <unixstd.h>
#include <dirdefs.h>
#include <strdefs.h>
#include <statdefs.h>
#include <schily.h>
#include "starsubs.h"
#ifdef	__linux__
#include <fctldefs.h>
#include <linux/ext2_fs.h>
#include <sys/ioctl.h>
#endif

#ifndef	HAVE_ERRNO_DEF
extern	int	errno;
#endif

EXPORT	void	get_fflags	__PR((FINFO *info));
EXPORT	void	set_fflags	__PR((FINFO *info));
EXPORT	char	*textfromflags	__PR((FINFO *info, char *buf));
EXPORT	int	texttoflags	__PR((FINFO *info, char *buf));

EXPORT void
get_fflags(info)
	register FINFO	*info;
{
#ifdef	__linux__
	int	f;
	long	l = 0L;

	if ((f = open(info->f_name, O_RDONLY|O_NDELAY)) >= 0) {
		if (ioctl(f, EXT2_IOC_GETFLAGS, &l) >= 0) {
			info->f_fflags = l;
			if ((l & EXT2_NODUMP_FL) != 0)
				info->f_flags |= F_NODUMP;
			if (info->f_fflags != 0)
				info->f_xflags |= XF_FFLAGS;
		} else {
			info->f_fflags = 0L;
		}
		close(f);
	}
#else	/* !__linux__ */
	info->f_fflags = 0L;
#endif
}

EXPORT void
set_fflags(info)
	register FINFO	*info;
{
/*
 * Be careful: True64 includes a chflags() stub but no #defines for st_flags
 */
#if	defined(HAVE_CHFLAGS) && defined(UF_SETTABLE)
	char	buf[512];

	/*
	 * As for 14.2.2002 the man page of chflags() is wrong, the following
	 * code is a result of kernel source study.
	 * If we are not allowed to set the flags, try to only set the user
	 * settable flags.
	 */
	if ((chflags(info->f_name, info->f_fflags) < 0 && geterrno() == EPERM) ||
	     chflags(info->f_name, info->f_fflags & UF_SETTABLE) < 0)
		errmsg("Cannot set file flags '%s' for '%s'.\n",
				textfromflags(info, buf), info->f_name);
#else
#ifdef	__linux__
	char	buf[512];
	int	f;
	Ulong	flags;
	Ulong	oldflags;
	BOOL	err = TRUE;
	/*
	 * Linux bites again! There is no define for the flags that are only
	 * settable by the root user.
	 */
#define	SF_MASK			(EXT2_IMMUTABLE_FL|EXT2_APPEND_FL)

	if ((f = open(info->f_name, O_RDONLY|O_NONBLOCK)) >= 0) {
		if (ioctl(f, EXT2_IOC_SETFLAGS, &info->f_fflags) >= 0) {
			err = FALSE;

		} else if (geterrno() == EPERM) {
			if (ioctl(f, EXT2_IOC_GETFLAGS, &oldflags) >= 0) {

				flags	 =  info->f_fflags & ~SF_MASK;
				oldflags &= SF_MASK;
				flags	 |= oldflags;
				if (ioctl(f, EXT2_IOC_SETFLAGS, &flags) >= 0)
					err = FALSE;	
			}
		}
		close(f);
	}
	if (err)
		errmsg("Cannot set file flags '%s' for '%s'.\n",
				textfromflags(info, buf), info->f_name);
#endif

#endif
}


LOCAL struct {
	char	*name;
	Ulong	flag;
} flagnames[] = {
	/* shorter names per flag first, all prefixed by "no" */
#ifdef	SF_APPEND
	{ "sappnd",		SF_APPEND },
	{ "sappend",		SF_APPEND },
#endif

#ifdef	EXT2_APPEND_FL				/* 'a' */
	{ "sappnd",		EXT2_APPEND_FL },
	{ "sappend",		EXT2_APPEND_FL },
#endif

#ifdef	SF_ARCHIVED
	{ "arch",		SF_ARCHIVED },
	{ "archived",		SF_ARCHIVED },
#endif

#ifdef	SF_IMMUTABLE
	{ "schg",		SF_IMMUTABLE },
	{ "schange",		SF_IMMUTABLE },
	{ "simmutable",		SF_IMMUTABLE },
#endif
#ifdef	EXT2_IMMUTABLE_FL			/* 'i' */
	{ "schg",		EXT2_IMMUTABLE_FL },
	{ "schange",		EXT2_IMMUTABLE_FL },
	{ "simmutable",		EXT2_IMMUTABLE_FL },
#endif

#ifdef	SF_NOUNLINK
	{ "sunlnk",		SF_NOUNLINK },
	{ "sunlink",		SF_NOUNLINK },
#endif

/*--------------------------------------------------------------------------*/

#ifdef	UF_APPEND
	{ "uappnd",		UF_APPEND },
	{ "uappend",		UF_APPEND },
#endif

#ifdef	UF_IMMUTABLE
	{ "uchg",		UF_IMMUTABLE },
	{ "uchange",		UF_IMMUTABLE },
	{ "uimmutable",		UF_IMMUTABLE },
#endif

#ifdef	EXT2_COMPR_FL				/* 'c' */
	{ "compress",		EXT2_COMPR_FL },
#endif

#ifdef	EXT2_NOATIME_FL				/* 'A' */
	{ "noatime",		EXT2_NOATIME_FL },
#endif

#ifdef	UF_NODUMP
	{ "nodump",		UF_NODUMP },
#endif
#ifdef	EXT2_NODUMP_FL				/* 'd' */
	{ "nodump",		EXT2_NODUMP_FL },
#endif

#ifdef	UF_OPAQUE
	{ "opaque",		UF_OPAQUE },
#endif

#ifdef	EXT2_SECRM_FL				/* 's' Purge before unlink */
	{ "secdel",		EXT2_SECRM_FL },
#endif

#ifdef	EXT2_SYNC_FL				/* 'S' */
	{ "sync",		EXT2_SYNC_FL },
#endif

#ifdef	EXT2_UNRM_FL				/* 'u' Allow to 'unrm' file the */
	{ "undel",		EXT2_UNRM_FL },
#endif

#ifdef	UF_NOUNLINK
	{ "uunlnk",		UF_NOUNLINK },
	{ "uunlink",		UF_NOUNLINK },
#endif
	{ NULL,			0 }
};
#define nflagnames	((sizeof(flagnames) / sizeof(flagnames[0])) -1)

/*
 * With 32 bits for flags and 512 bytes for the text buffer any name
 * for a single flag may be <= 16 bytes.
 */
EXPORT char *
textfromflags(info, buf)
	register FINFO	*info;
	register char	*buf;
{
	register Ulong	flags = info->f_fflags;
	register char	*n;
	register char	*p;
	register int	i;

	buf[0] = '\0';
	p = buf;

	for (i = 0; i < nflagnames; i++) {
		if (flags & flagnames[i].flag) {
			flags &= ~flagnames[i].flag;
			if (p != buf)
				*p++ = ',';
			for (n = flagnames[i].name; *n; *p++ = *n++)
				;
		}
	}
	*p = '\0';
	return (buf);
}

EXPORT int
texttoflags(info, buf)
	register FINFO	*info;
	register char	*buf;
{
	register char	*p;
	register char	*sep;
	register int	i;
	register Ulong	flags = 0;

	p = buf;

	while (*p) {
		if ((sep = strchr(p, ',')) != NULL)
			*sep = '\0';

		for (i = 0; i < nflagnames; i++) {
			if (streql(flagnames[i].name, p)) {
				flags |= flagnames[i].flag;
				break;
			}
		}
#ifdef	nonono
		if (i == nflagnames) {
			not found!
		}
#endif
		if (sep != NULL) {
			*sep++ = ',';
			p = sep;
		} else {
			break;
		}
	}
	info->f_fflags = flags;
	return (0);
}

#endif  /* USE_FFLAGS */
