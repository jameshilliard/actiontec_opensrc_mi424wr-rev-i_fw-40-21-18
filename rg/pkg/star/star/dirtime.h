/* @(#)dirtime.h	1.1 96/06/13 Copyright 1996 J. Schilling */
/*
 *	Prototypes for dirtime users
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

/*
 * dirtime.c
 */
extern	void	sdirtimes	__PR((char* name, FINFO* info));
extern	void	dirtimes	__PR((char* name, struct timeval* tp));
