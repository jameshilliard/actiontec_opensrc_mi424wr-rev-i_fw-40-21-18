/* @(#)movearch.h	1.1 01/08/12 Copyright 1993, 1995, 2001 J. Schilling */
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

typedef struct {
	char	*m_data;	/* Pointer to data to be moved ftom/to arch */
	int	m_size;		/* Size of data to be moved from/to arch    */
	int	m_flags;	/* Flags holding different move options	    */
} move_t;

#define	MF_ADDSLASH	0x01	/* Add a slash to the data on archive	    */


extern	int	move_from_arch	__PR((move_t * move, char* p, int amount));
extern	int	move_to_arch	__PR((move_t * move, char* p, int amount));

#define	vp_move_from_arch ((int(*)__PR((void *, char *, int)))move_from_arch)
#define	vp_move_to_arch	((int(*)__PR((void *, char *, int)))move_to_arch)
