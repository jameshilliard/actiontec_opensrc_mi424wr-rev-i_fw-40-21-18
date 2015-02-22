/* @(#)unicode.c	1.2 01/08/17 Copyright 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)unicode.c	1.2 01/08/17 Copyright 2001 J. Schilling";
#endif
/*
 *	Routines to convert from/to UNICODE
 *
 *	This is currently a very simple implementation that only
 *	handles ISO-8859-1 coding. There should be a better solution
 *	in the future.
 *
 *	Copyright (c) 2001 J. Schilling
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
#include "star.h"
#include <standard.h>
#include <schily.h>
#include "starsubs.h"

EXPORT	void	to_utf8		__PR((Uchar *to, Uchar *from));
EXPORT	BOOL	from_utf8	__PR((Uchar *to, Uchar *from));

EXPORT void
to_utf8(to, from)
	register Uchar	*to;
	register Uchar	*from;
{
	register Uchar	c;

	while ((c = *from++) != '\0') {
		if (c <= 0x7F) {
			*to++ = c;
		} else if (c <= 0xBF) {
			*to++ = 0xC2;
			*to++ = c;
		} else { /*c <= 0xFF */
			*to++ = 0xC3;
			*to++ = c & 0xBF;
		}
	}
	*to = '\0';
}

EXPORT BOOL
from_utf8(to, from)
	register Uchar	*to;
	register Uchar	*from;
{
	register Uchar	c;
	register BOOL	ret = TRUE;

	while ((c = *from++) != '\0') {
		if (c <= 0x7F) {
			*to++ = c;
		} else if (c == 0xC0) {
			*to++ = *from++ & 0x7F;
		} else if (c == 0xC1) {
			*to++ = (*from++ | 0x40) & 0x7F;
		} else if (c == 0xC2) {
			*to++ = *from++;
		} else if (c == 0xC3) {
			*to++ = *from++ | 0x40;
		} else {
			ret = FALSE;		/* unknown/illegal UTF-8 char*/
			*to++ = '_';		/* use default character     */
			if (c < 0xE0) {
				from++;		/* 2 bytes in total */
			} else if (c < 0xF0) {
				from += 2;	/* 3 bytes in total */
			} else if (c < 0xF8) {
				from += 3;	/* 4 bytes in total */
			} else if (c < 0xFC) {
				from += 4;	/* 5 bytes in total */
			} else if (c < 0xFE) {
				from += 5;	/* 6 bytes in total */
			} else {
				while ((c = *from) != '\0') {
					/*
					 * Test for 7 bit ASCII + non prefix
					 */
					if (c <= 0xBF)
						break;
					from++;
				}
			}
		}
	}
	*to = '\0';
	return (ret);
}
