/* @(#)lhash.c	1.9 02/05/17 Copyright 1988 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)lhash.c	1.9 02/05/17 Copyright 1988 J. Schilling";
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
 * Hash table name lookup.
 *
 *	Implemented 1988 with help from B. Mueller-Zimmermann
 *
 *	hash_build(fp, nqueue) FILE *fp;
 *
 *		Liest das File `fp' zeilenweise. Jede Zeile enthaelt genau
 *		einen Namen. Blanks werden nicht entfernt. Alle so
 *		gefundenen Namen werden in der Hashtabelle gespeichert.
 *		`nqueue' ist ein tuning parameter und gibt die Zahl der
 *		Hash-queues an. Pro Hashqueue werden 4 Bytes benoetigt.
 *
 *	hash_lookup(str) char *str;
 *
 *		Liefert TRUE, wenn der angegebene String in der
 *		Hashtabelle vorkommt.
 *
 * Scheitert malloc(), gibt es eine Fehlermeldung und exit().
 */

#include <mconfig.h>
#include <stdio.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <standard.h>
#include "star.h"
#include <strdefs.h>
#include <schily.h>
#include "starsubs.h"

extern	BOOL	notpat;

static struct h_elem {
	struct h_elem *h_next;
	char  	       h_data[1];			/* Variable size. */
} **h_tab;

static unsigned	h_size;

EXPORT	void	hash_build	__PR((FILE * fp, size_t size));
EXPORT	BOOL	hash_lookup	__PR((char* str));
LOCAL	int	hashval		__PR((unsigned char* str, unsigned int maxsize));

EXPORT void
hash_build(fp, size)
	FILE	*fp;
	size_t	size;
{
	register struct h_elem	*hp;
	register	int	i;
	register	int	len;
	register	int	hv;
			char	buf[PATH_MAX+1];

	h_size = size;
	h_tab = __malloc(size * sizeof (struct h_elem *), "list option");
	for (i=0; i<size; i++) h_tab[i] = 0;
	while ((len = fgetline(fp, buf, sizeof buf)) >= 0) {
		if (len == 0)
			continue;
		if (len >= PATH_MAX) {
			errmsgno(EX_BAD, "%s: Name too long (%d > %d).\n",
							buf, len, PATH_MAX);
			continue;
		}
		hp = __malloc((size_t)len + 1 + sizeof (struct h_elem *), "list option");
		strcpy(hp->h_data, buf);
		hv = hashval((unsigned char *)buf, size);
		hp->h_next = h_tab[hv];
		h_tab[hv] = hp;
	}
}

EXPORT BOOL
hash_lookup(str)
	char	*str;
{
	register struct h_elem *hp;
	register int		hv;

	hv = hashval((unsigned char *)str, h_size);
	for (hp = h_tab[hv]; hp; hp=hp->h_next)
	    if (streql(str, hp->h_data))
		return (!notpat);
	return (notpat);
}

LOCAL int
hashval(str, maxsize)
	register unsigned char *str;
		 unsigned	maxsize;
{
	register int	sum = 0;
	register int	i;
	register int	c;

	for (i=0; (c = *str++) != '\0'; i++)
		sum ^= (c << (i&7));
	return sum % maxsize;
}
