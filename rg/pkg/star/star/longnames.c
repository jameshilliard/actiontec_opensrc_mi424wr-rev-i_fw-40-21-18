/* @(#)longnames.c	1.34 02/05/09 Copyright 1993, 1995, 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)longnames.c	1.34 02/05/09 Copyright 1993, 1995, 2001 J. Schilling";
#endif
/*
 *	Handle filenames that cannot fit into a single
 *	string of 100 charecters
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

LOCAL	void	enametoolong	__PR((char* name, int len, int maxlen));
LOCAL	char*	split_posix_name __PR((char* name, int namlen, int add));
EXPORT	BOOL	name_to_tcb	__PR((FINFO * info, TCB * ptb));
EXPORT	void	tcb_to_name	__PR((TCB * ptb, FINFO * info));
EXPORT	void	tcb_undo_split	__PR((TCB * ptb, FINFO * info));
EXPORT	int	tcb_to_longname	__PR((TCB * ptb, FINFO * info));
EXPORT	void	write_longnames	__PR((FINFO * info));
LOCAL	void	put_longname	__PR((FINFO * info,
					char* name, int namelen, char* tname,
							Ulong  xftype));

LOCAL void
enametoolong(name, len, maxlen)
	char	*name;
	int	len;
	int	maxlen;
{
	xstats.s_toolong++;
	errmsgno(EX_BAD, "%s: Name too long (%d > %d chars)\n",
							name, len, maxlen);
}


LOCAL char *
split_posix_name(name, namlen, add)
	char	*name;
	int	namlen;
	int	add;
{
	register char	*low;
	register char	*high;

	if (namlen+add > props.pr_maxprefix+1+props.pr_maxsname) {
		/*
		 * Cannot split
		 */
		if (props.pr_maxnamelen <= props.pr_maxsname) /* No longnames*/
			enametoolong(name, namlen+add,
				props.pr_maxprefix+1+props.pr_maxsname);
		return (NULL);
	}
	low = &name[namlen+add - props.pr_maxsname];
	if (--low < name)
		low = name;
	high = &name[props.pr_maxprefix>namlen ? namlen:props.pr_maxprefix];

#ifdef	DEBUG
error("low: %d:%s high: %d:'%c',%s\n",
			strlen(low), low, strlen(high), *high, high);
#endif
	high++;
	while (--high >= low)
		if (*high == '/')
			break;
	if (high < low) {
		if (props.pr_maxnamelen <= props.pr_maxsname) {
			xstats.s_toolong++;
			errmsgno(EX_BAD, "%s: Name too long (cannot split)\n",
									name);
		}
		return (NULL);
	}
#ifdef	DEBUG
error("solved: add: %d prefix: %d suffix: %d\n",
			add, high-name, strlen(high+1)+add);
#endif
	return (high);
}

/*
 * Es ist sichergestelt, daß namelen >= 1 ist.
 */
EXPORT BOOL
name_to_tcb(info, ptb)
	FINFO	*info;
	TCB	*ptb;
{
	char	*name = info->f_name;
	int	namelen = info->f_namelen;
	int	add = 0;
	char	*np = NULL;

	if (namelen == 0)
		raisecond("name_to_tcb: namelen", 0L);

	if (is_dir(info) && name[namelen-1] != '/')
		add++;

	if ((namelen+add) <= props.pr_maxsname) {	/* Fits in shortname */
		if (add)
			strcatl(ptb->dbuf.t_name, name, "/", (char *)NULL);
		else
			strcpy(ptb->dbuf.t_name, name);
		return (TRUE);
	}

	if (props.pr_nflags & PR_POSIX_SPLIT)
		np = split_posix_name(name, namelen, add);
	if (np == NULL) {
		/*
		 * cannot split
		 */
		if (namelen+add <= props.pr_maxnamelen) {
			if (props.pr_flags & PR_XHDR)
				info->f_xflags |= XF_PATH;
			else
				info->f_flags |= F_LONGNAME;
			if (add)
				info->f_flags |= F_ADDSLASH;
			strncpy(ptb->dbuf.t_name, name, props.pr_maxsname);
			return (TRUE);
		} else {
			enametoolong(name, namelen+add, props.pr_maxnamelen);
			return (FALSE);
		}
	}

	if (add)
		strcatl(ptb->dbuf.t_name, &np[1], "/", (char *)NULL);
	else
		strcpy(ptb->dbuf.t_name, &np[1]);
	strncpy(ptb->dbuf.t_prefix, name, np - name);
	info->f_flags |= F_SPLIT_NAME;
	return (TRUE);
}

/*
 * This function is only called by tcb_to_info().
 * If we ever decide to call it from somewhere else check if the linkname
 * kludge for 100 char linknames does not make problems.
 */
EXPORT void
tcb_to_name(ptb, info)
	TCB	*ptb;
	FINFO	*info;
{
	if ((info->f_flags & F_LONGLINK) == 0 &&	/* name from 'K' head*/
	    (info->f_xflags & XF_LINKPATH) == 0 &&	/* name from 'x' head*/
	    ptb->dbuf.t_linkname[NAMSIZ-1] != '\0') {
		extern	char	longlinkname[];

		/*
		 * Our caller has set ptb->dbuf.t_linkname[NAMSIZ] to '\0'
		 * if the link name len is exactly 100 chars.
		 */
		info->f_lname = longlinkname;
		strcpy(info->f_lname, ptb->dbuf.t_linkname);
	}

	/*
	 * Name has already been set up because it is a very long name or
	 * because it has been setup from somwhere else.
	 * We have nothing to do.
	 */
	if (info->f_flags & (F_LONGNAME|F_HAS_NAME))
		return;
	/*
	 * Name has already been set up from a POSIX.1-2001 extended header.
	 */
	if (info->f_xflags & XF_PATH)
		return;

	if (props.pr_nflags & PR_POSIX_SPLIT) {
		strcatl(info->f_name, ptb->dbuf.t_prefix,
						*ptb->dbuf.t_prefix?"/":"",
						ptb->dbuf.t_name, NULL);
	} else {
		strcpy(info->f_name, ptb->dbuf.t_name);
	}
}

EXPORT void
tcb_undo_split(ptb, info)
	TCB	*ptb;
	FINFO	*info;
{
	fillbytes(ptb->dbuf.t_name, NAMSIZ, '\0');
	fillbytes(ptb->dbuf.t_prefix, props.pr_maxprefix, '\0');

	info->f_flags &= ~F_SPLIT_NAME;

	if (props.pr_flags & PR_XHDR)
		info->f_xflags |= XF_PATH;
	else
		info->f_flags |= F_LONGNAME;

	strncpy(ptb->dbuf.t_name, info->f_name, props.pr_maxsname);
}

/*
 * A bad idea to do this here!
 * We have to set up a more generalized pool of namebuffers wich are allocated
 * on an actual MAX_PATH base or even better allocated on demand.
 *
 * XXX If we change the code to allocate the data, we need to make sure that
 * XXX the allocated data holds one byte more than needed as movearch.c
 * XXX adds a second null byte to the buffer to enforce null-termination.
 */
char	longlinkname[PATH_MAX+1];

EXPORT int
tcb_to_longname(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	move_t	move;
	Ullong	ull;

	/*
	 * File size is strlen of name + 1
	 */
	stolli(ptb->dbuf.t_size, &ull);
	info->f_size = ull;
	info->f_rsize = info->f_size;
	if (info->f_size > PATH_MAX) {
		xstats.s_toolong++;
		errmsgno(EX_BAD, "Long name too long (%lld) ignored.\n",
							(Llong)info->f_size);
		void_file(info);
		return (get_tcb(ptb));
	}
	if (ptb->dbuf.t_linkflag == LF_LONGNAME) {
		if ((info->f_xflags & XF_PATH) != 0) {
			/*
			 * Ignore old star/gnutar extended headers for very
			 * long filenames if we already found a POSIX.1-2001
			 * compliant long PATH name.
			 */
			void_file(info);
			return (get_tcb(ptb));
		}
		info->f_namelen = info->f_size -1;
		info->f_flags |= F_LONGNAME;
		move.m_data = info->f_name;
	} else {
		if ((info->f_xflags & XF_LINKPATH) != 0) {
			/*
			 * Ignore old star/gnutar extended headers for very
			 * long linknames if we already found a POSIX.1-2001
			 * compliant long LINKPATH name.
			 */
			void_file(info);
			return (get_tcb(ptb));
		}
		info->f_lname = longlinkname;
		info->f_lnamelen = info->f_size -1;
		info->f_flags |= F_LONGLINK;
		move.m_data = info->f_lname;
	}
	move.m_flags = 0;
	if (xt_file(info, vp_move_from_arch, &move, 0, "moving long name") < 0)
		die(EX_BAD);

	return (get_tcb(ptb));
}

EXPORT void
write_longnames(info)
	register FINFO	*info;
{
	/*
	 * XXX Should test for F_LONGNAME & F_FLONGLINK
	 */
	if ((info->f_flags & F_LONGNAME) ||
	    (info->f_namelen > props.pr_maxsname)) {
		put_longname(info, info->f_name, info->f_namelen+1,
						"././@LongName", XT_LONGNAME);
	}
	if ((info->f_flags & F_LONGLINK) ||
	    (info->f_lnamelen > props.pr_maxslname)) {
		put_longname(info, info->f_lname, info->f_lnamelen+1,
						"././@LongLink", XT_LONGLINK);
	}
}

LOCAL void
put_longname(info, name, namelen, tname, xftype)
	FINFO	*info;
	char	*name;
	int	namelen;
	char	*tname;
	Ulong	xftype;
{
	FINFO	finfo;
	TCB	*ptb;
	move_t	move;

	fillbytes((char *)&finfo, sizeof(finfo), '\0');

	ptb = (TCB *)get_block();
	finfo.f_flags |= F_TCB_BUF;
	filltcb(ptb);

	strcpy(ptb->dbuf.t_name, tname);

	move.m_flags = 0;
	if ((info->f_flags & F_ADDSLASH) != 0 && xftype == XT_LONGNAME) {
		/*
		 * A slash is only added to the filename and not to the
		 * linkname.
		 */
		move.m_flags |= MF_ADDSLASH;
		namelen++;
	}
	finfo.f_rsize = finfo.f_size = namelen;
	finfo.f_xftype = xftype;
	info_to_tcb(&finfo, ptb);
	write_tcb(ptb, &finfo);

	move.m_data = name;
	move.m_size = finfo.f_size;
	cr_file(&finfo, vp_move_to_arch, &move, 0, "moving long name");
}
