/* @(#)seterrno.c	1.7 01/03/04 Copyright 1985 J. Schilling */
/*
 *	Set error number
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

#include <standard.h>
#include <errno.h>
#include <schily.h>

#ifndef	HAVE_ERRNO_DEF
extern	int	errno;
#endif

#ifdef	seterrno
#undef	seterrno
#endif

int seterrno(err)
	int	err;
{
	int	old = errno;

	errno = err;
	return (old);
}
