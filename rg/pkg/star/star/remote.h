/* @(#)remote.h	1.7 01/08/02 Copyright 1996 J. Schilling */
/*
 *	Prototypes for rmt client subroutines
 *
 *	Copyright (c) 1996 J. Schilling
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

#ifndef	_REMOTE_H
#define	_REMOTE_H

#ifndef	_INCL_SYS_TYPES_H
#include <sys/types.h>
#define	_INCL_SYS_TYPES_H
#endif

/*
 * Definitions for the new RMT Protocol version 1
 *
 * The new Protocol version tries to make the use
 * of rmtioctl() more portable between different platforms.
 */
#define	RMTIVERSION	-1
#define	RMT_NOVERSION	-1
#define	RMT_VERSION	1

/*
 * Support for commands bejond MTWEOF..MTNOP (0..7)
 */
#define	RMTICACHE	0
#define	RMTINOCACHE	1
#define	RMTIRETEN	2
#define	RMTIERASE	3
#define	RMTIEOM		4
#define	RMTINBSF	5

/*
 * Old MTIOCGET copies a binary version of struct mtget back
 * over the wire. This is highly non portable.
 * MTS_* retrieves ascii versions (%d format) of a single
 * field in the struct mtget.
 * NOTE: MTS_ERREG may only be valid on the first call and
 *	 must be retrived first.
 */
#define	MTS_TYPE	'T'		/* mtget.mt_type */
#define	MTS_DSREG	'D'		/* mtget.mt_dsreg */
#define	MTS_ERREG	'E'		/* mtget.mt_erreg */
#define	MTS_RESID	'R'		/* mtget.mt_resid */
#define	MTS_FILENO	'F'		/* mtget.mt_fileno */
#define	MTS_BLKNO	'B'		/* mtget.mt_blkno */
#define	MTS_FLAGS	'f'		/* mtget.mt_flags */
#define	MTS_BF		'b'		/* mtget.mt_bf */

/*
 * remote.c
 */
extern	int		rmtdebug	__PR((int dlevel));
extern	char		*rmtfilename	__PR((char *name));
extern	char		*rmthostname	__PR((char *hostname, char *rmtspec, int size));
extern	int		rmtgetconn	__PR((char* host, int size));
extern	int		rmtopen		__PR((int fd, char* fname, int fmode));
extern	int		rmtclose	__PR((int fd));
extern	int		rmtread		__PR((int fd, char* buf, int count));
extern	int		rmtwrite	__PR((int fd, char* buf, int count));
extern	off_t		rmtseek		__PR((int fd, off_t offset, int whence));
extern	int		rmtioctl	__PR((int fd, int cmd, int count));
#ifdef	MTWEOF
extern	int		rmtstatus	__PR((int fd, struct mtget* mtp));
#endif

#endif	/* _REMOTE_H */
