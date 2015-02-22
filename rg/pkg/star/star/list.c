/* @(#)list.c	1.41 02/05/05 Copyright 1985, 1995, 2000-2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)list.c	1.41 02/05/05 Copyright 1985, 1995, 2000-2001 J. Schilling";
#endif
/*
 *	List the content of an archive
 *
 *	Copyright (c) 1985, 1995, 2000-2001 J. Schilling
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
#include "table.h"
#include <dirdefs.h>
#include <standard.h>
#include <strdefs.h>
#include <schily.h>
#include "starsubs.h"

extern	FILE	*tarf;
extern	FILE	*vpr;
extern	char	*listfile;
extern	Llong	curblockno;

extern	BOOL	havepat;
extern	BOOL	numeric;
extern	int	verbose;
extern	BOOL	prblockno;
extern	BOOL	tpath;
extern	BOOL	cflag;
extern	BOOL	xflag;
extern	BOOL	interactive;

extern	BOOL	acctime;
extern	BOOL	Ctime;

extern	BOOL	listnew;
extern	BOOL	listnewf;

EXPORT	void	list		__PR((void));
LOCAL	void	modstr		__PR((FINFO * info, char* s, Ulong  mode));
EXPORT	void	list_file	__PR((FINFO * info));
EXPORT	void	vprint		__PR((FINFO * info));

EXPORT void
list()
{
		FINFO	finfo;
		FINFO	newinfo;
		TCB	tb;
		TCB	newtb;
		char	name[PATH_MAX+1];
		char	newname[PATH_MAX+1];
		char	newlname[PATH_MAX+1];
	register TCB 	*ptb = &tb;

	fillbytes((char *)&finfo, sizeof(finfo), '\0');
	fillbytes((char *)&newinfo, sizeof(newinfo), '\0');

	finfo.f_tcb = ptb;
	for (;;) {
		if (get_tcb(ptb) == EOF)
			break;
		if (prblockno)
			(void)tblocks();		/* set curblockno */

		finfo.f_name = name;
		if (tcb_to_info(ptb, &finfo) == EOF)
			return;
		if (listnew || listnewf) {
			/*
			 * XXX nsec beachten wenn im Archiv!
			 */
			if (((finfo.f_mtime > newinfo.f_mtime) ||
			    ((finfo.f_xflags & XF_MTIME) && 
			     (newinfo.f_xflags & XF_MTIME) &&
			     (finfo.f_mtime == newinfo.f_mtime) &&
			     (finfo.f_mnsec > newinfo.f_mnsec))) &&
					(!listnewf || is_file(&finfo))) {
				movebytes(&finfo, &newinfo, sizeof(finfo));
				movetcb(&tb, &newtb);
				/*
				 * Paranoia.....
				 */
				strncpy(newname, name, PATH_MAX);
				newname[PATH_MAX] = '\0';
				newinfo.f_name = newname;
				if (newinfo.f_lname[0] != '\0') {
					/*
					 * Paranoia.....
					 */
					strncpy(newlname, newinfo.f_lname,
								PATH_MAX);
					newlname[PATH_MAX] = '\0';
					newinfo.f_lname = newlname;
				}
				newinfo.f_flags |= F_HAS_NAME;
			}
		} else if (listfile) {
			if (hash_lookup(finfo.f_name))
				list_file(&finfo);
		} else if (!havepat || match(finfo.f_name))
			list_file(&finfo);

		void_file(&finfo);
	}
	if ((listnew || listnewf) && newinfo.f_mtime != 0L) {
		/* XXX
		 * XXX Achtung!!! tcb_to_info zerstört t_name[NAMSIZ]
		 * XXX und t_linkname[NAMSIZ].
		 */
		tcb_to_info(&newtb, &newinfo);
		list_file(&newinfo);
	}
}

#ifdef	OLD
static char *typetab[] = 
{"S","-","l","d","","","","", };
#endif

LOCAL void
modstr(info, s, mode)
		FINFO	*info;
		 char	*s;
	register Ulong	mode;
{
	register char	*mstr = "xwrxwrxwr";
	register char	*str = s;
	register int	i;

	for (i=9; --i >= 0;) {
		if (mode & (1 << i))
			*str++ = mstr[i];
		else
			*str++ = '-';
	}
#ifdef	USE_ACL
	*str++ = ' ';
#endif
	*str = '\0';
	str = s;
	if (mode & 01000) {
		if (mode & 01)
			str[8] = 't';
		else
			str[8] = 'T';
	}
	if (mode & 02000) {
		if (mode & 010) {
			str[5] = 's';
		} else {
			if (is_dir(info))
				str[5] = 'S';
			else
				str[5] = 'l';
		}
	}
	if (mode & 04000) {
		if (mode & 0100)
			str[2] = 's';
		else
			str[2] = 'S';
	}
#ifdef	USE_ACL
	if ((info->f_xflags & (XF_ACL_ACCESS|XF_ACL_DEFAULT)) != 0)
		str[9] = '+';
#endif
}

EXPORT void
list_file(info)
	register FINFO	*info;
{
		FILE	*f;
		time_t	*tp;
		char	*tstr;
		char	mstr[11]; /* 9 UNIX chars + ACL '+' + nul */
	static	char	nuid[11]; /* XXXX 64 bit longs??? */
	static	char	ngid[11]; /* XXXX 64 bit longs??? */

	f = vpr;
	if (prblockno)
		fprintf(f, "block %9lld: ", curblockno);
	if (cflag)
		fprintf(f, "a ");
	else if (xflag)
		fprintf(f, "x ");

	if (verbose) {
		register Uint	xft = info->f_xftype;

/*		tp = (time_t *) (acctime ? &info->f_atime :*/
/*				(Ctime ? &info->f_ctime : &info->f_mtime));*/
		tp = acctime ? &info->f_atime :
				(Ctime ? &info->f_ctime : &info->f_mtime);
		tstr = ctime(tp);
		if (numeric || info->f_uname == NULL) {
			sprintf(nuid, "%lu", info->f_uid);
			info->f_uname = nuid;
			info->f_umaxlen = sizeof(nuid)-1;
		}
		if (numeric || info->f_gname == NULL) {
			sprintf(ngid, "%lu", info->f_gid);
			info->f_gname = ngid;
			info->f_gmaxlen = sizeof(ngid)-1;
		}

		if (is_special(info))
			fprintf(f, "%3lu %3lu",
				info->f_rdevmaj, info->f_rdevmin);
		else
			fprintf(f, "%7llu", (Llong)info->f_size);
		modstr(info, mstr, info->f_mode);

/*
 * XXX Übergangsweise, bis die neue Filetypenomenklatur sauber eingebaut ist.
 */
if (xft == 0 || xft == XT_BAD) {
	xft = info->f_xftype = IFTOXT(info->f_type);
	errmsgno(EX_BAD, "XXXXX xftype == 0 (typeflag = '%c' 0x%02X)\n",
				info->f_typeflag, info->f_typeflag);
}
		if (xft == XT_LINK)
			xft = info->f_rxftype;
		fprintf(f,
			" %s%s %3.*s/%-3.*s %.12s %4.4s ",
#ifdef	OLD
			typetab[info->f_filetype & 07],
#else
			XTTOSTR(xft),
#endif
			mstr,
			(int)info->f_umaxlen, info->f_uname,
			(int)info->f_gmaxlen, info->f_gname,
			&tstr[4], &tstr[20]);
	}
	fprintf(f, "%s", info->f_name);
	if (tpath) {
		fprintf(f, "\n");
		return;
	}
	if (is_link(info)) {
		if (is_dir(info))
			fprintf(f, " directory");
		fprintf(f, " link to %s", info->f_lname);
	}
	if (is_symlink(info))
		fprintf(f, " -> %s", info->f_lname);
	if (is_volhdr(info))
		fprintf(f, " --Volume Header--");
	if (is_multivol(info))
		fprintf(f, " --Continued at byte %lld--", (Llong)info->f_contoffset);
	fprintf(f, "\n");
	fflush(f);
}

EXPORT void
vprint(info)
	FINFO	*info;
{
		FILE	*f;
	char	*mode;

	if (verbose || interactive) {
		if (verbose > 1) {
			list_file(info);
			return;
		}

		f = vpr;

		if (prblockno)
			fprintf(f, "block %9lld: ", curblockno);
		if (cflag)
			mode = "a ";
		else if (xflag)
			mode = "x ";
		else
			mode = "";

		if (tpath) {
			fprintf(f, "%s%s\n", mode, info->f_name);
			return;
		}
		if (is_dir(info)) {
			if (is_link(info)) {
				fprintf(f, "%s%s directory link to %s\n",
					mode, info->f_name, info->f_lname);
			} else {
				fprintf(f, "%s%s directory\n", mode, info->f_name);
			}
		} else if (is_link(info)) {
			fprintf(f, "%s%s link to %s\n",
				mode, info->f_name, info->f_lname);
		} else if (is_symlink(info)) {
			fprintf(f, "%s%s symbolic link to %s\n",
				mode, info->f_name, info->f_lname);
		} else if (is_special(info)) {
			fprintf(f, "%s%s special\n", mode, info->f_name);
		} else {
			fprintf(f, "%s%s %lld bytes, %lld tape blocks\n",
				mode, info->f_name, (Llong)info->f_size,
				(Llong)tarblocks(info->f_rsize));
		}
	}
}
