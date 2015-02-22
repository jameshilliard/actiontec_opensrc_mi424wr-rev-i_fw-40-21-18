/* @(#)movearch.c	1.27 01/12/07 Copyright 1993, 1995, 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)movearch.c	1.27 01/12/07 Copyright 1993, 1995, 2001 J. Schilling";
#endif
/*
 *	Handle non-file type data that needs to be moved from/to the archive.
 *
 *	Copyright (c) 1993, 1995, 2001 J. Schilling
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
#include "star.h"
#include "props.h"
#include "table.h"
#include <standard.h>
#include <strdefs.h>
#include <schily.h>
#include "starsubs.h"
#include "movearch.h"

EXPORT	int	move_from_arch	__PR((move_t * move, char* p, int amount));
EXPORT	int	move_to_arch	__PR((move_t * move, char* p, int amount));


/*
 * Move data from archive.
 */
EXPORT int
move_from_arch(move, p, amount)
	move_t	*move;
	char	*p;
	int	amount;
{
	movebytes(p, move->m_data, amount);
	move->m_data += amount;
	/*
	 * If we make sure that the buffer holds at least one character more
	 * than needed, then we may safely add another null byte at the end of
	 * the extracted buffer.
	 * This makes sure, that a buggy tar implementation which wight archive
	 * non null-terminated long filenames with 'L' or 'K' filetype may
	 * not cause us to core dump.
	 * It is needed when extracting extended attribute buffers from
	 * POSIX.1-2001 archives as POSIX.1-2001 makes the buffer '\n' but not
	 * null-terminated.
	 */
	move->m_data[0] = '\0';
	return (amount);
}

/*
 * Move data to archive.
 */
EXPORT int
move_to_arch(move, p, amount)
	move_t	*move;
	char	*p;
	int	amount;
{
	if (amount > move->m_size)
		amount = move->m_size;
	movebytes(move->m_data, p, amount);
	move->m_data += amount;
	move->m_size -= amount;
	if (move->m_flags & MF_ADDSLASH) {
		if (move->m_size == 1) {
			p[amount-1] = '/';
		} else if (move->m_size == 0) {
			if (amount > 1)
				p[amount-2] = '/';
			p[amount-1] = '\0';
		}
	}
	return (amount);
}
