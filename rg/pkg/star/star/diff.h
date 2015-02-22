/* @(#)diff.h	1.6 99/06/15 Copyright 1993 J. Schilling */
/*
 *	Definitions for the taylorable diff command
 *
 *	Copyright (c) 1993 J. Schilling
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

#define	D_PERM	0x00001
#define	D_TYPE	0x00002
#define	D_NLINK	0x00004
#define	D_UID	0x00010
#define	D_GID	0x00020
#define	D_UNAME	0x00040
#define	D_GNAME	0x00080
#define	D_ID	(D_UID|D_GID|D_UNAME|D_GNAME)
#define	D_SIZE	0x00100
#define	D_DATA	0x00200
#define	D_RDEV	0x00400
#define	D_HLINK	0x01000
#define	D_SLINK	0x02000
#define	D_SPARS	0x04000
#define	D_ATIME	0x10000
#define	D_MTIME	0x20000
#define	D_CTIME	0x40000
#define	D_TIMES	(D_ATIME|D_MTIME|D_CTIME)

#define	D_DEFLT	(~(D_NLINK|D_ATIME))
#define	D_ALL	(~0L);

extern	long	diffopts;
