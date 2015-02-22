/* @(#)xheader.c	1.12 02/05/17 Copyright 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)xheader.c	1.12 02/05/17 Copyright 2001 J. Schilling";
#endif
/*
 *	Handling routines to read/write, parse/create
 *	POSIX.1-2001 extended archive headers
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
#include <stdxlib.h>
#include "star.h"
#include "props.h"
#include "table.h"
#include <dirdefs.h>
#include <standard.h>
#include <strdefs.h>
#define	__XDEV__	/* Needed to activate _dev_major()/_dev_minor() */
#include <device.h>
#include <schily.h>
#include "starsubs.h"
#include "movearch.h"

#define	MAX_UNAME	64	/* The maximum length of a user/group name */

typedef	void (*function)	__PR((FINFO *, char *, char *));

typedef struct {
	char		*x_name;
	function	x_func;
	int		x_flag;
} xtab_t;

EXPORT	void	xbinit		__PR((void));
LOCAL	void	xbgrow		__PR((int newsize));
EXPORT	void	info_to_xhdr	__PR((FINFO * info, TCB * ptb));
LOCAL	void	gen_xtime	__PR((char *keyword, time_t sec, Ulong nsec));
LOCAL	void	gen_number	__PR((char *keyword, Llong arg));
LOCAL	void	gen_text	__PR((char *keyword, char *arg, BOOL addslash));
LOCAL	xtab_t	*lookup		__PR((char *cmd, xtab_t *cp));
EXPORT	int	tcb_to_xhdr	__PR((TCB * ptb, FINFO * info));
LOCAL	BOOL	get_xtime	__PR((char *keyword, char *arg, time_t *secp,
								Ulong *nsecp));
LOCAL	void	get_atime	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_ctime	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_mtime	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	BOOL	get_number	__PR((char *keyword, char *arg, Llong *llp));
LOCAL	void	get_uid		__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_gid		__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_uname	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_gname	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_path	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_lpath	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_size	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_major	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_minor	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_dev		__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_ino		__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_nlink	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_filetype	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_acl_access	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_acl_default	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_xfflags	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	get_dummy	__PR((FINFO *info, char *keyword, char *arg));
LOCAL	void	bad_utf8	__PR((char *keyword, char *arg));

LOCAL	char	*xbuf;	/* Space used to prepare I/O from/to extended headers */
LOCAL	int	xblen;	/* the length of the buffer for the extended headers */
LOCAL	int	xbidx;	/* The index where we start to prepare next entry    */

LOCAL xtab_t xtab[] = {
			{ "atime",		get_atime,	0	},
			{ "ctime",		get_ctime,	0	},
			{ "mtime",		get_mtime,	0	},

			{ "uid",		get_uid,	0	},
			{ "uname",		get_uname,	0	},
			{ "gid",		get_gid,	0	},
			{ "gname",		get_gname,	0	},

			{ "path",		get_path,	0	},
			{ "linkpath",		get_lpath,	0	},

			{ "size",		get_size,	0	},

			{ "charset",		get_dummy,	0	},
			{ "comment",		get_dummy,	0	},

			{ "SCHILY.devmajor",	get_major,	0	},
			{ "SCHILY.devminor",	get_minor,	0	},

#ifdef	USE_ACL
			{ "SCHILY.acl.access",	get_acl_access,	0	},
			{ "SCHILY.acl.default",	get_acl_default,0	},
#else
/*
 * We don't want star to complain about unknown extended headers when it
 * has been compiled without ACL support.
 */
			{ "SCHILY.acl.access",	get_dummy,	0	},
			{ "SCHILY.acl.default",	get_dummy,	0	},
#endif

#ifdef	USE_FFLAGS
			{ "SCHILY.fflags",	get_xfflags,	0	},
#else
/*
 * We don't want star to complain about unknown extended headers when it
 * has been compiled without extended file flag support.
 */
			{ "SCHILY.fflags",	get_dummy,	0	},
#endif
			{ "SCHILY.dev",		get_dev,	0	},
			{ "SCHILY.ino",		get_ino,	0	},
			{ "SCHILY.nlink",	get_nlink,	0	},
			{ "SCHILY.filetype",	get_filetype,	0	},
			{ "SCHILY.tarfiletype",	get_filetype,	0	},

			{ "SUN.devmajor",	get_major,	0	},
			{ "SUN.devminor",	get_minor,	0	},

			{ NULL,			NULL,		0	}};

/*
 * Initialize the growable buffer used for reading the extended headers
 */
EXPORT void
xbinit()
{
	xbuf = __malloc(1, "growable xheader");
	xblen = 1;
}

/*
 * Grow the growable buffer used for reading the extended headers
 */
LOCAL void
xbgrow(newsize)
	int	newsize;
{
	char	*newbuf;
	int	i;

	/*
	 * grow in 1k units
	 */
	for (i = 0; i < newsize; i += 1024)
		;
	newsize = i + xblen;
	newbuf = __realloc(xbuf, newsize, "growable xheader");
	xbuf = newbuf;
	xblen = newsize;
}

/*
 * Prepare and write out the extended header
 */
EXPORT void
info_to_xhdr(info, ptb)
	register FINFO	*info;
	register TCB	*ptb;
{
	FINFO	finfo;
	TCB	*xptb;
	move_t	move;
	char	name[MAX_UNAME+1];
extern	long	hdrtype;
extern	BOOL	dodump;

	fillbytes((char *)&finfo, sizeof(finfo), '\0');

	/*
	 * Unless we really don't want extended sub-second resolution
	 * timestamps or a specific selection of timestams has been set up,
	 * include all times (atime/ctime/mtime) if we need to include extended
	 * headers at all.
	 */
	if ((info->f_xflags & (XF_ATIME|XF_CTIME|XF_MTIME|XF_NOTIME)) == 0)
		info->f_xflags |= (XF_ATIME|XF_CTIME|XF_MTIME);

/*#define	DEBUG_XHDR*/
#ifdef	DEBUG_XHDR
	info->f_xflags = 0xffffffff;
#endif

	xbidx = 0;	/* Reset xbuffer index to start of buffer. */

	if (info->f_xflags & XF_ATIME)
		gen_xtime("atime", info->f_atime, info->f_ansec);
	if (info->f_xflags & XF_CTIME)
		gen_xtime("ctime", info->f_ctime, info->f_cnsec);
	if (info->f_xflags & XF_MTIME)
		gen_xtime("mtime", info->f_mtime, info->f_mnsec);

	if (info->f_xflags & XF_UID)
		gen_number("uid", (Llong)info->f_uid);
	if (info->f_xflags & XF_GID)
		gen_number("gid", (Llong)info->f_gid);

	if (info->f_xflags & XF_UNAME) {
		nameuid(name, sizeof(name)-1, info->f_uid);
		gen_text("uname", name, FALSE);
	}
	if (info->f_xflags & XF_GNAME) {
		namegid(name, sizeof(name)-1, info->f_gid);
		gen_text("gname", name, FALSE);
	}

	if (info->f_xflags & XF_PATH)
		gen_text("path", info->f_name,
				(info->f_flags & F_ADDSLASH) != 0);
	if (info->f_xflags & XF_LINKPATH && info->f_lnamelen)
		gen_text("linkpath", info->f_lname, FALSE);

	if (info->f_xflags & XF_SIZE)
		gen_number("size", (Llong)info->f_size);

	if (H_TYPE(hdrtype) == H_SUNTAR) {
		if (info->f_xflags & XF_DEVMAJOR)
			gen_number("SUN.devmajor", (Llong)info->f_rdevmaj);
		if (info->f_xflags & XF_DEVMINOR)
			gen_number("SUN.devminor", (Llong)info->f_rdevmin);
	} else {
		if (info->f_xflags & XF_DEVMAJOR)
			gen_number("SCHILY.devmajor", (Llong)info->f_rdevmaj);
		if (info->f_xflags & XF_DEVMINOR)
			gen_number("SCHILY.devminor", (Llong)info->f_rdevmin);
	}

#ifdef	USE_ACL
	/*
	 * POSIX Access Control Lists, currently supported e.g. by Linux.
	 * Solaris ACLs have been converted to POSIX before.
	 */
	if (info->f_xflags & XF_ACL_ACCESS) {
		gen_text("SCHILY.acl.access", info->f_acl_access, FALSE);
	}

	if (info->f_xflags & XF_ACL_DEFAULT) {
		gen_text("SCHILY.acl.default", info->f_acl_default, FALSE);
	}
#endif  /* USE_ACL */

#ifdef	USE_FFLAGS
	if (info->f_xflags & XF_FFLAGS) {
extern char	*textfromflags	__PR((FINFO *, char *));

		char	fbuf[512];
		gen_text("SCHILY.fflags", textfromflags(info, fbuf), FALSE);
	}
#endif

	if (dodump) {
		gen_number("SCHILY.dev", (Llong)info->f_dev);
		gen_number("SCHILY.ino", (Llong)info->f_ino);
		gen_number("SCHILY.nlink", (Llong)info->f_nlink);
		gen_text("SCHILY.filetype", XTTONAME(info->f_rxftype), FALSE);
#ifdef	__needed__
		if (info->f_rxftype != info->f_xftype)
			gen_text("SCHILY.tarfiletype", XTTONAME(info->f_xftype), FALSE);
#endif
	}

	xptb = (TCB *)get_block();
	finfo.f_flags |= F_TCB_BUF;
	filltcb(xptb);
/*	strcpy(xptb->dbuf.t_name, ptb->dbuf.t_name);*/
	strcpy(xptb->dbuf.t_name, "././@PaxHeader");
	finfo.f_rsize = finfo.f_size = xbidx;
	finfo.f_xftype = props.pr_xc;
	info_to_tcb(&finfo, xptb);
	xptb->dbuf.t_linkflag = props.pr_xc;
	write_tcb(xptb, &finfo);

	move.m_data = xbuf;
	move.m_size = finfo.f_size;
	move.m_flags = 0;
	cr_file(&finfo, vp_move_to_arch, &move, 0, "moving extended header");
}

/*
 * Create a time string with sub-second granularity.
 *
 * <length> <time-name-spec>=<seconds>.<nano-seconds>\n
 *
 * As we always emmit 9 digits for the sub-second part, the
 * length of this entry is always more then 10 and less than 100.
 * We may safely fill in the two digit <length> later when we know the value.
 */
LOCAL void
gen_xtime(keyword, sec, nsec)
	char	*keyword;
	time_t	sec;
	Ulong	nsec;
{
	char	*p;
	int	len;

	if ((xbidx + 100) > xblen)
		xbgrow(100);

	p = &xbuf[xbidx+3];
	len = js_sprintf(p, "%s=%lld.%9.9ld\n", keyword, (Llong)sec, nsec);
	len += 3;
	js_sprintf(&xbuf[xbidx], "%d", len);
	xbuf[xbidx+2] = ' ';
	xbidx += len;
}

/*
 * Create a generic number string.
 *
 * <length> <name-spec>=<value>\n
 *
 * The length of this entry is always less than 100 chars if the length of the
 * 'name-spec' is less than 75 chars (the maximum length of a 64 bit number in
 * decimal is 20 digits). In the rare case when the number is short and
 * 'name-spec' only uses a few characters (e.g. number == 0 and 
 * strlen(name-spec) < 4) the length of the entry will be less than 10 and
 * we have to correct the string by moving it backwards later.
 */
LOCAL void
gen_number(keyword, arg)
	char	*keyword;
	Llong	arg;
{
	char	*p;
	int	len;

	if ((xbidx + 100) > xblen)
		xbgrow(100);

	p = &xbuf[xbidx+3];
	len = js_sprintf(p, "%s=%lld\n", keyword, arg);
	len += 3;
	js_sprintf(&xbuf[xbidx], "%d", len);
	xbuf[xbidx+2] = ' ';
	if (len < 10) {
		/*
		 * Rare case: the total length is less than 10 and we have to
		 * move the remainder of the string backwards by one.
		 */
		len -= 1;
		xbuf[xbidx] = len + '0';
		strcpy(&xbuf[xbidx+1], &xbuf[xbidx+2]);
	}
	xbidx += len;
}

/*
 * Create a generic text string in UTF-8 coding.
 *
 * <length> <name-spec>=<value>\n
 *
 * This function will have to carefully check for the resultant length
 * and thus compute the total length in advance.
 */
LOCAL void
gen_text(keyword, arg, addslash)
	char	*keyword;
	char	*arg;
	BOOL	addslash;
{
	Uchar	uuu[10000];
	int	len;

	to_utf8(uuu, (Uchar *)arg);

	len  = strlen((char *)uuu);
	if (addslash) {			/* only used if 'path' is a dir */
		uuu[len++] = '/';
		uuu[len] = '\0';
	}
	len += strlen(keyword) + 4;	/* one digit + ' ' + '=' + '\n' */

	if (len > 9996) {
		comerrno(EX_BAD,
			"Fatal: extended header for keyword '%s' too long.\n",
			keyword);
	}
	if (len > 997)
		len += 3;
	else if (len > 98)
		len += 2;
	else if (len > 9)
		len += 1;

	if ((xbidx + len) > xblen)
		xbgrow(len);

	js_sprintf(&xbuf[xbidx], "%d %s=%s\n", len, keyword, uuu);
	xbidx += len;
}

/*
 * Lookup command in command tab
 */
LOCAL xtab_t *
lookup(cmd, cp)
	register char	*cmd;
	register xtab_t	*cp;
{
	for (; cp->x_name; cp++) {
		if ((*cmd == *cp->x_name) &&
		    strcmp(cmd, cp->x_name) == 0) {
			return (cp);
		}
	}
	return ((xtab_t *)NULL);
}

/*
 * Read extended POSIX.1-2001 header and parse the content.
 */
EXPORT int
tcb_to_xhdr(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	register xtab_t	*cp;
	register char	*keyword;
	register char 	*arg;
	register char	*p;
	register char 	*ep;
		 move_t	move;
		 Ullong	ull;
		 long	length;

#ifdef	XH_DEBUG
error("Block: %lld\n", tblocks());
#endif
	/*
	 * File size is strlen of extended header
	 */
	stolli(ptb->dbuf.t_size, &ull);
	info->f_size = ull;
	info->f_rsize = (off_t)info->f_size;
	/*
	 * Reset xbidx to make xbgrow() work correctly for our case.
	 */
	xbidx = 0;
	if ((info->f_size+1) > xblen)
		xbgrow(info->f_size+1);

	move.m_data = xbuf;
	move.m_flags = 0;
	if (xt_file(info, vp_move_from_arch, &move, 0,
						"moving extended header") < 0) {
		die(EX_BAD);
	}

#ifdef	XH_DEBUG
error("Block: %lld\n", tblocks());
error("xbuf: '%s'\n", xbuf);
#endif

	p = xbuf;
	ep = p + ull;
	while (p < ep) {
		keyword = astolb(p, &length, 10);
		if (*keyword != ' ') {
			errmsgno(EX_BAD, "Syntax error in extended header.\n");
			break;
		}
		keyword++;
		arg = strchr(keyword, '=');
		*arg++ = '\0';
		*(p + length -1) = '\0';	/* Kill new-line character */

		if ((cp = lookup(keyword, xtab)) != NULL) {
			(*cp->x_func)(info, keyword, arg);
		} else {
			errmsgno(EX_BAD,
			    "Unknown extended header keyword '%s' ignored.\n",
				keyword);
		}
		p += length;
	}
	return (get_tcb(ptb));
}

/*
 * generic function to read args that hold times
 *
 * The time may either be in second resolution or in sub-second resolution.
 * In the latter case the second fraction and the sub second fraction
 * is separated by a dot ('.').
 */
LOCAL BOOL
get_xtime(keyword, arg, secp, nsecp)
	char	*keyword;
	char	*arg;
	time_t	*secp;
	Ulong	*nsecp;
{
	Llong	ll;
	long	l;
	char	*p;

	p = astollb(arg, &ll, 10);
	if (*p == '\0') {		/* time has second resolution only */
		*secp = ll;
		*nsecp = 0;
		return (TRUE);
	} else if (*p == '.') {		/* time has sub-second resolution */
		*secp = ll;
		if (strlen(++p) > 9)	/* if resolution is better then nano- */ 
			p[9] = '\0';	/* seconds kill rest of line as we    */
		p = astolb(p, &l, 10);	/* don't understand more.	      */
		if (*p == '\0') {	/* number read correctly	      */
			if (l >= 0) {
				*nsecp = l;
				return (TRUE);
			}
		}
	}
	errmsgno(EX_BAD, "Bad timespec '%s' for '%s' in extended header.\n",
		arg, keyword);
	return (FALSE);
}

/*
 * get read access time from extended header
 */
LOCAL void
get_atime(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	if (get_xtime(keyword, arg, &info->f_atime, &info->f_ansec))
		info->f_xflags |= XF_ATIME;
}

/*
 * get inode status change time from extended header
 */
LOCAL void
get_ctime(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	if (get_xtime(keyword, arg, &info->f_ctime, &info->f_cnsec))
		info->f_xflags |= XF_CTIME;
}

/*
 * get modification time from extended header
 */
LOCAL void
get_mtime(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	if (get_xtime(keyword, arg, &info->f_mtime, &info->f_mnsec))
		info->f_xflags |= XF_MTIME;
}

/*
 * generic function to read args that hold decimal numbers
 */
LOCAL BOOL
get_number(keyword, arg, llp)
	char	*keyword;
	char	*arg;
	Llong	*llp;
{
	Llong	ll;
	char	*p;

	p = astollb(arg, &ll, 10);
	if (*p == '\0') {		/* number read correctly */
		*llp = ll;
		return (TRUE);
	}
	errmsgno(EX_BAD, "Bad number '%s' for '%s' in extended header.\n",
		arg, keyword);
	return (FALSE);
}

/*
 * get user id (if > 2097151)
 */
LOCAL void
get_uid(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	Llong	ll;

	if (get_number(keyword, arg, &ll)) {
		info->f_xflags |= XF_UID;
		info->f_uid = ll;
	}
}

/*
 * get group id (if > 2097151)
 */
LOCAL void
get_gid(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	Llong	ll;

	if (get_number(keyword, arg, &ll)) {
		info->f_xflags |= XF_GID;
		info->f_gid = ll;
	}
}

/*
 * Space for returning user/group names.
 */
LOCAL	Uchar	_uname[MAX_UNAME+1];
LOCAL	Uchar	_gname[MAX_UNAME+1];

/*
 * get user name (if name length is > 32 chars or if contains non ASCII chars)
 */
LOCAL void
get_uname(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	if (strlen(arg) > MAX_UNAME)
		return;
	if (from_utf8(_uname, (Uchar *)arg)) {
		info->f_xflags |= XF_UNAME;
		info->f_uname = (char *)_uname;
		info->f_umaxlen = strlen((char *)_uname);
	} else {
		bad_utf8(keyword, arg);
	}
}

/*
 * get group name (if name length is > 32 chars or if contains non ASCII chars)
 */
LOCAL void
get_gname(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	if (strlen(arg) > MAX_UNAME)
		return;
	if (from_utf8(_gname, (Uchar *)arg)) {
		info->f_xflags |= XF_GNAME;
		info->f_gname = (char *)_gname;
		info->f_gmaxlen = strlen((char *)_gname);
	} else {
		bad_utf8(keyword, arg);
	}
}

/*
 * get path (if name length is > 100-255 chars or if contains non ASCII chars)
 */
LOCAL void
get_path(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	if (strlen(arg) > PATH_MAX)
		return;
	if (from_utf8((Uchar *)info->f_name, (Uchar *)arg)) {
		info->f_xflags |= XF_PATH;
	} else {
		bad_utf8(keyword, arg);
	}
}

/*
 * get link path (if name length is > 100 chars or if contains non ASCII chars)
 */
LOCAL void
get_lpath(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
extern	char	longlinkname[];

	if (strlen(arg) > PATH_MAX)
		return;
	if (from_utf8((Uchar *)longlinkname, (Uchar *)arg)) {
		info->f_xflags |= XF_LINKPATH;
		info->f_lname = longlinkname;
	} else {
		bad_utf8(keyword, arg);
	}
}

/*
 * get size (usually when size is > 8 GB)
 */
LOCAL void
get_size(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	Llong	ll;

	if (get_number(keyword, arg, &ll)) {
		info->f_xflags |= XF_SIZE;
		info->f_size = (off_t)ll;
	}
}

/*
 * get major device number (always vendor unique)
 */
LOCAL void
get_major(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	Llong	ll;

	if (get_number(keyword, arg, &ll)) {
		info->f_xflags |= XF_DEVMAJOR;
		info->f_rdevmaj = ll;
	}
}

/*
 * get minor device number (always vendor unique)
 */
LOCAL void
get_minor(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	Llong	ll;

	if (get_number(keyword, arg, &ll)) {
		info->f_xflags |= XF_DEVMINOR;
		info->f_rdevmin = ll;
	}
}

/*
 * get device number of device containing FS (vendor unique)
 */
LOCAL void
get_dev(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	Llong	ll;

	if (get_number(keyword, arg, &ll)) {
		info->f_dev = ll;
	}
}

/*
 * get inode number for this file (vendor unique)
 */
LOCAL void
get_ino(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	Llong	ll;

	if (get_number(keyword, arg, &ll)) {
		info->f_ino = ll;
	}
}

/*
 * get link count for this file (vendor unique)
 */
LOCAL void
get_nlink(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	Llong	ll;

	if (get_number(keyword, arg, &ll)) {
		info->f_nlink = ll;
	}
}

/*
 * get tar file type or real file type for this file (vendor unique)
 */
LOCAL void
get_filetype(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	int	i;

	for (i=0; i < XT_BAD; i++) {
		if (streql(xttoname_tab[i], arg))
			break;
	}
	if (i >= XT_BAD)
		return;

	if (keyword[7] == 'f') {		/* "SCHILY.filetype" */
		info->f_rxftype = i;
		info->f_filetype = XTTOST(info->f_rxftype);
		info->f_type = XTTOIF(info->f_rxftype);	
	} else {				/* "SCHILY.tarfiletype" */
		info->f_xftype = i;
	}
}

#ifdef	USE_ACL

/*
 * XXX acl_access_text/acl_default_text are a bad idea. (see acl_unix.c)
 */
LOCAL char acl_access_text[PATH_MAX+1];
LOCAL char acl_default_text[PATH_MAX+1];

LOCAL void
get_acl_access(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	if (strlen(arg) > PATH_MAX)	/* XXX We should use dynamic strings */
		return;
	if (from_utf8((Uchar *)acl_access_text, (Uchar *)arg)) {
		info->f_xflags |= XF_ACL_ACCESS;
		info->f_acl_access = acl_access_text;
	} else {
		bad_utf8(keyword, arg);
	}
}

LOCAL void
get_acl_default(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	if (strlen(arg) > PATH_MAX)	/* XXX We should use dynamic strings */
		return;
	if (from_utf8((Uchar *)acl_default_text, (Uchar *)arg)) {
		info->f_xflags |= XF_ACL_DEFAULT;
		info->f_acl_default = acl_default_text;
	} else {
		bad_utf8(keyword, arg);
	}
}

#endif  /* USE_ACL */

#ifdef	USE_FFLAGS

LOCAL void
get_xfflags(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
	texttoflags(info, arg);
	info->f_xflags |= XF_FFLAGS;
}

#endif	/* USE_FFLAGS */


/*
 * Dummy 'get' function used for all fields that we don't yet understand or
 * fields that we indent to ignore.
 */
LOCAL void
get_dummy(info, keyword, arg)
	FINFO	*info;
	char	*keyword;
	char	*arg;
{
}

LOCAL void
bad_utf8(keyword, arg)
	char	*keyword;
	char	*arg;
{
	errmsgno(EX_BAD, "Bad UTF-8 arg '%s' for '%s' in extended header.\n",
		arg, keyword);
}
