/* @(#)remove.c	1.44 02/04/25 Copyright 1985 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)remove.c	1.44 02/04/25 Copyright 1985 J. Schilling";
#endif
/*
 *	remove files an file trees
 *
 *	Copyright (c) 1985 J. Schilling
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
#include <stdio.h>
#include <standard.h>
#include "star.h"
#include "table.h"
#include <dirdefs.h>	/*XXX Wegen S_IFLNK */
#include <unixstd.h>
#include <strdefs.h>
#include <schily.h>

#ifdef	JOS
#	include	<error.h>
#else
#	include	<errno.h>
#	define	EMISSDIR	ENOENT
#endif
#include "starsubs.h"


extern	FILE	*tty;
extern	FILE	*vpr;
extern	BOOL	interactive;
extern	BOOL	force_remove;
extern	BOOL	ask_remove;
extern	BOOL	remove_first;
extern	BOOL	remove_recursive;

EXPORT	BOOL	remove_file	__PR((char* name, BOOL isfirst));
LOCAL	BOOL	_remove_file	__PR((char* name, BOOL isfirst, int depth));
LOCAL	BOOL	remove_tree	__PR((char* name, BOOL isfirst, int depth));

EXPORT BOOL
remove_file(name, isfirst)
	register char	*name;
		 BOOL	isfirst;
{
	static	int	depth	= -10;
	static	int	dinit	= 0;

	if (!dinit) {
#ifdef	_SC_OPEN_MAX
		depth += sysconf(_SC_OPEN_MAX);
#else
		depth += getdtablesize();
#endif
		dinit = 1;
	}
	return (_remove_file(name, isfirst, depth));
}

LOCAL BOOL
_remove_file(name, isfirst, depth)
	register char	*name;
		 BOOL	isfirst;
		 int	depth;
{
	char	buf[32];
	char	ans = '\0';
	int	err = EX_BAD;
	BOOL	fr_save = force_remove;
	BOOL	rr_save = remove_recursive;
	BOOL	ret;

	if (remove_first && !isfirst)
		return (FALSE);
	if (!force_remove && (interactive || ask_remove)) {
		fprintf(vpr, "remove '%s' ? Y(es)/N(o) :", name);fflush(vpr);
		fgetline(tty, buf, 2);
	}
	if (force_remove ||
	    ((interactive || ask_remove) && (ans = toupper(buf[0])) == 'Y')) {

		/*
		 * only unlink non directories or empty directories
		 */
		if (rmdir(name) < 0) {
			err = geterrno();
			if (err == ENOTDIR) {
				if (unlink(name) < 0) {
					err = geterrno();
					goto cannot;
				}
				return (TRUE);
			}
#if defined(ENOTEMPTY) && ENOTEMPTY != EEXIST
			if (err == EEXIST || err == ENOTEMPTY) {
#else
			if (err == EEXIST) {
#endif
				if (!remove_recursive) {
					if (ans == 'Y') {
						fprintf(vpr,
						"Recursive remove nonempty '%s' ? Y(es)/N(o) :",
							name);
						fflush(vpr);
						fgetline(tty, buf, 2);
						if (toupper(buf[0]) == 'Y') {
							force_remove = TRUE;
							remove_recursive = TRUE;
						} else {
							goto nonempty;
						}
					} else {
				nonempty:
						errmsgno(err,
						"Nonempty directory '%s' not removed.\n",
						name);
						return (FALSE);
					}
				}
				ret = remove_tree(name, isfirst, depth);

				force_remove = fr_save;
				remove_recursive = rr_save;
				return (ret);
			}
			goto cannot;
		}
		return (TRUE);
	}
cannot:
	errmsgno(err, "File '%s' not removed.\n", name);
	return (FALSE);
}

LOCAL BOOL
remove_tree(name, isfirst, depth)
	register char	*name;
		 BOOL	isfirst;
		 int	depth;
{
	DIR		*d;
	struct dirent	*dir;
	BOOL		ret = TRUE;
	char		xn[PATH_MAX];	/* XXX A bad idea for a final solution */
	char		*p;

	if ((d = opendir(name)) == NULL) {
		return (FALSE);
	}
	depth--;

	strcpy(xn, name);
	p = &xn[strlen(name)];
	*p++ = '/';

	while ((dir = readdir(d)) != NULL) {

		if (streql(dir->d_name, ".") ||
				streql(dir->d_name, ".."))
			continue;

		strcpy(p, dir->d_name);

		if (depth <= 0) {
			closedir(d);
		}
		if (!_remove_file(xn, isfirst, depth))
			ret = FALSE;
		if (depth <= 0 && (d = opendir(name)) == NULL) {
			return (FALSE);
		}
	}
		
	closedir(d);

	if (ret == FALSE)
		return (ret);

	if (rmdir(name) >= 0)
		return (ret);

	errmsg("Directory '%s' not removed.\n", name);
	return (FALSE);
}
