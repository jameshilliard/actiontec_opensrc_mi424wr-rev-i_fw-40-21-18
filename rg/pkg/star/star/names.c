/* @(#)names.c	1.7 98/07/05 Copyright 1993 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)names.c	1.7 98/07/05 Copyright 1993 J. Schilling";
#endif
/*
 *	Handle user/group names for archive header
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

#include <mconfig.h>
#include <stdio.h>
#include <standard.h>
#include "star.h"
#include <pwd.h>
#include <grp.h>
#include <strdefs.h>
#include "starsubs.h"

#define	C_SIZE	16

typedef struct id {
	Ulong	id;
	char	name[TUNMLEN];		/* TUNMLEN == TGNMLEN	    */
	char	valid;
} idc_t;

LOCAL	idc_t	uidcache[C_SIZE];
LOCAL	int	lastuidx;		/* Last index for new entry */

LOCAL	idc_t	gidcache[C_SIZE];
LOCAL	int	lastgidx;		/* Last index for new entry */

EXPORT	BOOL	nameuid	__PR((char* name, int namelen, Ulong uid));
EXPORT	BOOL	uidname	__PR((char* name, int namelen, Ulong* uidp));
EXPORT	BOOL	namegid	__PR((char* name, int namelen, Ulong gid));
EXPORT	BOOL 	gidname	__PR((char* name, int namelen, Ulong* gidp));

/*
 * Get name from uid
 */
EXPORT BOOL
nameuid(name, namelen, uid)
	char	*name;
	int	namelen;
	Ulong	uid;
{
	struct passwd	*pw;
	register int	i;
	register idc_t	*idp;

	for (i=0, idp = uidcache; i < C_SIZE; i++, idp++) {
		if (idp->valid == 0)		/* Entry not yet filled */
			break;
		if (idp->id == uid)
			goto out;
	}
	idp = &uidcache[lastuidx++];		/* Round robin fill next ent */
	if (lastuidx >= C_SIZE)
		lastuidx = 0;

	idp->id = uid;
	idp->name[0] = '\0';
	idp->valid = 1;
	if ((pw = getpwuid(uid)) != NULL) {
		strncpy(idp->name, pw->pw_name, TUNMLEN);
		idp->name[namelen-1] = 0;
	}
out:
	strcpy(name, idp->name);
	return (name[0] != '\0');
}

/*
 * Get uid from name
 */
EXPORT BOOL
uidname(name, namelen, uidp)
	char	*name;
	int	namelen;
	Ulong	*uidp;
{
	struct passwd	*pw;
	register int	len = namelen>TUNMLEN?TUNMLEN:namelen;
	register int	i;
	register idc_t	*idp;

	if (name[0] == '\0')
		return (FALSE);

	for (i=0, idp = uidcache; i < C_SIZE; i++, idp++) {
		if (idp->valid == 0)		/* Entry not yet filled */
			break;
		if (name[0] == idp->name[0] && 
					strncmp(name, idp->name, len) == 0) {
			*uidp = idp->id;
			if (idp->valid == 2)	/* Name not found */
				return (FALSE);
			return (TRUE);
		}
	}
	idp = &uidcache[lastuidx++];		/* Round robin fill next ent */
	if (lastuidx >= C_SIZE)
		lastuidx = 0;

	idp->id = 0;
	idp->name[0] = '\0';
	strncpy(idp->name, name, len);
	idp->name[len] = '\0';
	idp->valid = 1;
	if ((pw = getpwnam(idp->name)) != NULL) {
		idp->id = pw->pw_uid;
		*uidp = idp->id;
		return (TRUE);
	} else {
		idp->valid = 2;			/* Mark name as not found */
		*uidp = 0;			/* XXX ??? */
		return (FALSE);
	}
}

/*
 * Get name from gid
 */
EXPORT BOOL
namegid(name, namelen, gid)
	char	*name;
	int	namelen;
	Ulong	gid;
{
	struct group	*gr;
	register int	i;
	register idc_t	*idp;

	for (i=0, idp = gidcache; i < C_SIZE; i++, idp++) {
		if (idp->valid == 0)		/* Entry not yet filled */
			break;
		if (idp->id == gid)
			goto out;
	}
	idp = &gidcache[lastgidx++];		/* Round robin fill next ent */
	if (lastgidx >= C_SIZE)
		lastgidx = 0;

	idp->id = gid;
	idp->name[0] = '\0';
	idp->valid = 1;
	if ((gr = getgrgid(gid)) != NULL) {
		strncpy(idp->name, gr->gr_name, TUNMLEN);
		idp->name[namelen-1] = 0;
	}
out:
	strcpy(name, idp->name);
	return (name[0] != '\0');
}

/*
 * Get gid from name
 */
EXPORT BOOL
gidname(name, namelen, gidp)
	char	*name;
	int	namelen;
	Ulong	*gidp;
{
	struct group	*gr;
	register int	len = namelen>TGNMLEN?TGNMLEN:namelen;
	register int	i;
	register idc_t	*idp;

	if (name[0] == '\0')
		return (FALSE);

	for (i=0, idp = gidcache; i < C_SIZE; i++, idp++) {
		if (idp->valid == 0)		/* Entry not yet filled */
			break;
		if (name[0] == idp->name[0] && 
					strncmp(name, idp->name, len) == 0) {
			*gidp = idp->id;
			if (idp->valid == 2)	/* Name not found */
				return (FALSE);
			return (TRUE);
		}
	}
	idp = &gidcache[lastgidx++];		/* Round robin fill next ent */
	if (lastgidx >= C_SIZE)
		lastgidx = 0;

	idp->id = 0;
	idp->name[0] = '\0';
	strncpy(idp->name, name, len);
	idp->name[len] = '\0';
	idp->valid = 1;
	if ((gr = getgrnam(idp->name)) != NULL) {
		idp->id = gr->gr_gid;
		*gidp = idp->id;
		return (TRUE);
	} else {
		idp->valid = 2;			/* Mark name as not found */
		*gidp = 0;			/* XXX ??? */
		return (FALSE);
	}
}
