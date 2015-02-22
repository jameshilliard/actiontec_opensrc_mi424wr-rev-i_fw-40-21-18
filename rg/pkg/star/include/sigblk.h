/* @(#)sigblk.h	1.7 00/11/08 Copyright 1985 J. Schilling */
/*
 *	software signal block definition
 *
 *	Copyright (c) 1985,1995 J. Schilling
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

#ifndef	_SIGBLK_H
#define	_SIGBLK_H

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif
#ifndef _STANDARD_H
#include <standard.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct sigblk {
	long		**sb_savfp;
	struct sigblk	*sb_signext;
	short		sb_siglen;
	const char	*sb_signame;
	int		(*sb_sigfun)	__PR((const char *, long, long));
	long		sb_sigarg;
} SIGBLK;

typedef	int	(*handlefunc_t)		__PR((const char *, long, long));

extern	void	handlecond		__PR((const char *, SIGBLK *,
					      int(*)(const char *, long, long),
									long));
extern	void	raisecond		__PR((const char *, long));
extern	void	unhandlecond		__PR((void));

#ifdef	__cplusplus
}
#endif

#endif	/* _SIGBLK_H */
