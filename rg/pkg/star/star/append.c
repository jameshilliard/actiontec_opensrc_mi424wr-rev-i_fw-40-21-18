/* @(#)append.c	1.17 02/05/17 Copyright 1992, 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)append.c	1.17 02/05/17 Copyright 1992, 2001 J. Schilling";
#endif
/*
 *	Routines used to append files to an existing 
 *	tape archive
 *
 *	Copyright (c) 1992, 2001 J. Schilling
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
#include <unixstd.h>
#include <standard.h>
#include "star.h"
#include <schily.h>
#include "starsubs.h"

extern	FILE	*vpr;

extern	BOOL	debug;
extern	BOOL	cflag;
extern	BOOL	uflag;
extern	BOOL	rflag;

/*
 * XXX We should try to share the hash code with lhash.c
 */
static struct h_elem {
	struct h_elem *h_next;
	time_t		h_time;
	Ulong		h_nsec;
	short		h_len;
	char		h_flags;
	char  	       h_data[1];			/* Variable size. */
} **h_tab;

#define	HF_NSEC		0x01				/* have nsecs	*/

static	unsigned	h_size;
LOCAL	int		cachesize;

EXPORT	void	skipall		__PR((void));
LOCAL	void	hash_new	__PR((size_t size));
LOCAL	struct h_elem *	uhash_lookup	__PR((FINFO *info));
LOCAL	void	hash_add	__PR((FINFO *info));
EXPORT	BOOL	update_newer	__PR((FINFO *info));
LOCAL	int	hashval		__PR((Uchar *str, Uint maxsize));

EXPORT void
skipall()
{
		FINFO	finfo;
		TCB	tb;
		char	name[PATH_MAX+1];
	register TCB 	*ptb = &tb;

	if (uflag)
		hash_new(100);

	fillbytes((char *)&finfo, sizeof(finfo), '\0');

	finfo.f_tcb = ptb;
	for (;;) {
		if (get_tcb(ptb) == EOF) {
			if (debug)
				error("used %d bytes for update cache.\n",
							cachesize);
			return;
		}
		finfo.f_name = name;
		if (tcb_to_info(ptb, &finfo) == EOF)
			return;

		if (debug)
			fprintf(vpr, "R %s\n", finfo.f_name);
		if (uflag)
			hash_add(&finfo);

		void_file(&finfo);
	}
}

#include <strdefs.h>

LOCAL void
hash_new(size)
	size_t	size;
{
	register	int	i;

	h_size = size;
	h_tab = (struct h_elem **)__malloc(size * sizeof (struct h_elem *), "new hash");
	for (i=0; i<size; i++) h_tab[i] = 0;

	cachesize += size * sizeof (struct h_elem *);
}

LOCAL struct h_elem *
uhash_lookup(info)
	FINFO	*info;
{
	register char		*name = info->f_name;
	register struct h_elem *hp;
	register int		hv;

	hv = hashval((unsigned char *)name, h_size);
	for (hp = h_tab[hv]; hp; hp=hp->h_next) {
		if (streql(name, hp->h_data))
			return (hp);
	}
	return (0);
}

LOCAL void
hash_add(info)
	FINFO	*info;
{
	register	int	len;
	register struct h_elem	*hp;
	register	int	hv;

	/*
	 * XXX nsec korrekt implementiert?
	 */
	if ((hp = uhash_lookup(info)) != 0) {
		if (hp->h_time < info->f_mtime) {
			hp->h_time = info->f_mtime;
			hp->h_nsec = info->f_mnsec;
		} else if (hp->h_time == info->f_mtime) {
			/*
			 * If the current archive entry holds extended
			 * time imformation, honor it.
			 */
			if (info->f_xflags & XF_MTIME)
				hp->h_flags |= HF_NSEC;
			else
				hp->h_flags &= ~HF_NSEC;

			if ((hp->h_flags & HF_NSEC) &&
			    (hp->h_nsec < info->f_mnsec))
				hp->h_nsec = info->f_mnsec;
		}
		return;
	}

	len = strlen(info->f_name);
	hp = __malloc((size_t)len + sizeof (struct h_elem), "add hash");
	cachesize += len + sizeof (struct h_elem);
	strcpy(hp->h_data, info->f_name);
	hp->h_time = info->f_mtime;
	hp->h_nsec = info->f_mnsec;
	hp->h_flags = 0;
	if (info->f_xflags & XF_MTIME)
		hp->h_flags |= HF_NSEC;
	hv = hashval((unsigned char *)info->f_name, h_size);
	hp->h_next = h_tab[hv];
	h_tab[hv] = hp;
}

EXPORT BOOL
update_newer(info)
	FINFO	*info;
{
	register struct h_elem *hp;

	/*
	 * XXX nsec korrekt implementiert?
	 */
	if ((hp = uhash_lookup(info)) != 0) {
		if (info->f_mtime > hp->h_time)
			return (TRUE);
		if ((hp->h_flags & HF_NSEC) && (info->f_flags & F_NSECS) &&
		    info->f_mtime == hp->h_time && info->f_mnsec > hp->h_nsec)
			return (TRUE);
		return (FALSE);
	}
	return (TRUE);
}

LOCAL int
hashval(str, maxsize)
	register Uchar *str;
		 Uint	maxsize;
{
	register int	sum = 0;
	register int	i;
	register int	c;

	for (i=0; (c = *str++) != '\0'; i++)
		sum ^= (c << (i&7));
	return sum % maxsize;
}
