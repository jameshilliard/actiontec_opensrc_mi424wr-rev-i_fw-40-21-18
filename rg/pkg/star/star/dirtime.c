/* @(#)dirtime.c	1.11 02/05/20 Copyright 1988 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)dirtime.c	1.11 02/05/20 Copyright 1988 J. Schilling";
#endif
/*
 *	Copyright (c) 1988 J. Schilling
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

/*
 * Save directories and its times on a stack and set the times, if the new name
 * will not increase the depth of the directory stack.
 * The final flush of the stack is caused by a zero length filename.
 *
 * A string will be sufficient for the names of the directory stack because
 * all directories in a tree have a common prefix.  A counter for each
 * occurence of a slash '/' is the index into the array of times for the
 * directory stack. Directories with unknown times have atime == -1.
 *
 * If the order of the files on tape is not in an order that find(1) will
 * produce, this algorithm is not guaranteed to work. This is the case with
 * tapes that have been created with the -r option or with the list= option.
 *
 * The only alternative would be saving all directory times and setting them
 * at the end of an extract.
 *
 * NOTE: I am not shure if degenerate filenames will fool this algorithm.
 */
#include <mconfig.h>
#include "star.h"
#include <standard.h>
#include <schily.h>
#include "xutimes.h"

#ifdef DEBUG
#define	EDBG(a)	if (debug) error a
#else
#define	EDBG(a)
#endif

/*
 * Maximum depth of directory nesting
 * will be reached if name has the form x/y/z/...
 *
 * NOTE: If PATH_MAX is 1024, sizeof(dtimes) will be 12 kBytes.
 */
#define NTIMES (PATH_MAX/2+1)

LOCAL	char dirstack[PATH_MAX];
#ifdef	SET_CTIME
#define	NT	3
LOCAL	struct timeval dtimes[NTIMES][NT];
LOCAL	struct timeval dottimes[NT] = { {-1, -1}, {-1, -1}, {-1, -1}};
#else
#define	NT	2
LOCAL	struct timeval dtimes[NTIMES][NT];
LOCAL	struct timeval dottimes[NT] = { -1, -1, -1, -1};
#endif

EXPORT	void	sdirtimes	__PR((char* name, FINFO* info));
EXPORT	void	dirtimes	__PR((char* name, struct timeval* tp));
LOCAL	void	flushdirstack	__PR((char *, int));
LOCAL	void	setdirtime	__PR((char *, struct timeval *));

EXPORT void
sdirtimes(name, info)
	char	*name;
	FINFO	*info;
{
	struct timeval	tp[NT];

	tp[0].tv_sec = info->f_atime;
	tp[0].tv_usec = info->f_ansec/1000;

	tp[1].tv_sec = info->f_mtime;
	tp[1].tv_usec = info->f_mnsec/1000;
#ifdef	SET_CTIME
	tp[2].tv_sec = info->f_ctime;
	tp[2].tv_usec = info->f_cnsec/1000;
#endif

	dirtimes(name, tp);
}

EXPORT void
dirtimes(name, tp)
	char		*name;
	struct timeval	tp[NT];
{
	register char	*dp = dirstack;
	register char	*np = name;
	register int	idx = -1;

	EDBG(("dirtimes('%s', %s", name, tp ? ctime(&tp[1].tv_sec):"NULL\n"));

	if (np[0] == '\0') {				/* final flush */
		if (dottimes[0].tv_sec >= 0)
			setdirtime(".", dottimes);
		flushdirstack(dp, -1);
		return;
	}

	if ((np[0] == '.' && np[1] == '/' && np[2] == '\0') ||
				(np[0] == '.' && np[1] == '\0')) {
		dottimes[0] = tp[0];
		dottimes[1] = tp[1];
#ifdef	SET_CTIME
		dottimes[2] = tp[2];
#endif
	} else {
		/*
		 * Find end of common part
		 */
		while (*dp == *np) {
			if (*dp++ == '/')
				++idx;
			np++;
		}
		EDBG(("DIR: '%.*s' DP: '%s' NP: '%s' idx: %d\n",
				/* XXX Should not be > int */
				(int)(dp - dirstack), dirstack, dp, np, idx));

		if (*dp) {
			/*
			 * New directory does not increase the depth of the
			 * directory stack. Flush all dirs below idx.
			 */
			flushdirstack(dp, idx);
		}

		/*
		 * Put the new dir on the directory stack.
		 * First append the name component, then
		 * store times of "this" dir.
		 */
		while ((*dp = *np++) != '\0') {
			if (*dp++ == '/') {
				/*
				 * Disable times of unknown dirs.
				 */
				EDBG(("zapping idx: %d\n", idx+1));
				dtimes[++idx][0].tv_sec = -1;
			} else if (*np == '\0') {
				*dp++ = '/';
				idx++;
			}
		}
		if (tp) {
			EDBG(("set idx %d '%s'\n", idx, name));
			dtimes[idx][0] = tp[0];	/* overwrite last atime */
			dtimes[idx][1] = tp[1];	/* overwrite last mtime */
#ifdef	SET_CTIME
			dtimes[idx][2] = tp[2];	/* overwrite last ctime */
#endif
		}
	}
}

LOCAL void
flushdirstack(dp, depth)
	register char	*dp;
	register int	depth;
{
	if (depth == -1 && dp[0] == '/' && dirstack[0] == '/') {
		/*
		 * Flush the root dir, avoid flushing "".
		 */
		while (*dp == '/')
			dp++;
		if (dtimes[++depth][0].tv_sec >= 0) {
			EDBG(("depth: %d ", depth));
			setdirtime("/", dtimes[depth]);
		}
	}
	while (*dp) {
		if (*dp++ == '/')
			if (dtimes[++depth][0].tv_sec >= 0) {
				EDBG(("depth: %d ", depth));
				*--dp = '\0';	/* temporarily delete '/' */
				setdirtime(dirstack, dtimes[depth]);
				*dp++ = '/';	/* restore '/' */
			}
	}
}

LOCAL void
setdirtime(name, tp)
	char	*name;
	struct timeval	tp[NT];
{
	EDBG(("settime: '%s' to %s", name, ctime(&tp[1].tv_sec)));
#ifdef	SET_CTIME
	if (xutimes(name, tp) < 0) {
#else
	if (utimes(name, tp) < 0) {
#endif
		errmsg("Can't set time on '%s'.\n", name);
		xstats.s_settime++;
	}
}
