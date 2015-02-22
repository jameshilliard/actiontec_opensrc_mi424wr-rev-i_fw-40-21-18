/* @(#)extract.c	1.52 02/05/02 Copyright 1985 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)extract.c	1.52 02/05/02 Copyright 1985 J. Schilling";
#endif
/*
 *	extract files from archive
 *
 *	Copyright (c) 1985 J. Schilling
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
#include "props.h"
#include "table.h"
#include <dirdefs.h>	/*XXX Wegen S_IFLNK */
#include <unixstd.h>
#include <strdefs.h>
#include <schily.h>

#ifdef	JOS
#	include	<error.h>
#	define	mkdir	makedir
#else
#	include	<errno.h>
#	define	EMISSDIR	ENOENT
#endif
#include "dirtime.h"
#include "starsubs.h"

extern	FILE	*vpr;

extern	char	*listfile;

extern	int	bufsize;
extern	char	*bigptr;

extern	BOOL	havepat;
extern	BOOL	prblockno;
extern	BOOL	nflag;
extern	BOOL	interactive;
extern	BOOL	nodir;
extern	BOOL	nospec;
extern	BOOL	xdir;
extern	BOOL	uncond;
extern	BOOL	keep_old;
extern	BOOL	refresh_old;
extern	BOOL	abs_path;
extern	BOOL	nowarn;
extern	BOOL	force_hole;
extern	BOOL	to_stdout;
extern	BOOL	remove_first;
extern	BOOL	copylinks;
extern	BOOL	hardlinks;
extern	BOOL	symlinks;
extern	BOOL	dometa;

EXPORT	void	extract		__PR((char *vhname));
EXPORT	BOOL	newer		__PR((FINFO * info));
LOCAL	BOOL	same_symlink	__PR((FINFO * info));
LOCAL	BOOL	create_dirs	__PR((char* name));
LOCAL	BOOL	make_dir	__PR((FINFO * info));
LOCAL	BOOL	make_link	__PR((FINFO * info));
LOCAL	BOOL	make_symlink	__PR((FINFO * info));
LOCAL	BOOL	emul_symlink	__PR((FINFO * info));
LOCAL	BOOL	emul_link	__PR((FINFO * info));
LOCAL	BOOL	make_copy	__PR((FINFO * info, BOOL do_symlink));
LOCAL	int	copy_file	__PR((char *from, char *to, BOOL do_symlink));
LOCAL	BOOL	make_fifo	__PR((FINFO * info));
LOCAL	BOOL	make_special	__PR((FINFO * info));
LOCAL	BOOL	get_file	__PR((FINFO * info));
LOCAL	int	void_func	__PR((void *vp, char* p, int amount));
EXPORT	BOOL	void_file	__PR((FINFO * info));
EXPORT	int	xt_file		__PR((FINFO * info,
					int (*)(void *, char *, int),
					void *arg, int amt, char* text));
EXPORT	void	skip_slash	__PR((FINFO * info));

EXPORT void
extract(vhname)
	char	*vhname;
{
		FINFO	finfo;
		TCB	tb;
		char	name[PATH_MAX+1];
	register TCB 	*ptb = &tb;

	fillbytes((char *)&finfo, sizeof(finfo), '\0');

	finfo.f_tcb = ptb;
	for (;;) {
		if (get_tcb(ptb) == EOF)
			break;
		finfo.f_name = name;
		if (tcb_to_info(ptb, &finfo) == EOF)
			return;
		if (prblockno)
			(void)tblocks();		/* set curblockno */

		if (is_volhdr(&finfo)) {
			if (!get_volhdr(&finfo, vhname)) {
				excomerrno(EX_BAD,
				"Volume Header '%s' does not match '%s'.\n",
							finfo.f_name, vhname);
			}
			void_file(&finfo);
			continue;
		}
		if (!abs_path &&	/* XXX VVV siehe skip_slash() */
		    (finfo.f_name[0] == '/'/* || finfo.f_lname[0] == '/'*/))
			skip_slash(&finfo);
		if (listfile) {
			if (!hash_lookup(finfo.f_name)) {
				void_file(&finfo);
				continue;
			}	
		} else if (havepat && !match(finfo.f_name)) {
			void_file(&finfo);
			continue;
		}
		if (!is_file(&finfo) && to_stdout) {
			void_file(&finfo);
			continue;
		}
		if (is_special(&finfo) && nospec) {
			xstats.s_isspecial++;
			errmsgno(EX_BAD, "'%s' is not a file. Not created.\n",
							finfo.f_name) ;
			continue;
		}
		if (newer(&finfo) && !(xdir && is_dir(&finfo))) {
			void_file(&finfo);
			continue;
		}
		if (is_symlink(&finfo) && same_symlink(&finfo)) {
			continue;
		}
		if (interactive && !ia_change(ptb, &finfo)) {
			if (!nflag)
				fprintf(vpr, "Skipping ...\n");
			void_file(&finfo);
			continue;
		}
		vprint(&finfo);
		if (remove_first) {
			/*
			 * With keep_old we do not come here.
			 */ 
			(void)remove_file(finfo.f_name, TRUE);
		}
		if (is_dir(&finfo)) {
			if (!make_dir(&finfo))
				continue;
		} else if (is_link(&finfo)) {
			if (!make_link(&finfo))
				continue;
		} else if (is_symlink(&finfo)) {
			if (!make_symlink(&finfo))
				continue;
		} else if (is_special(&finfo)) {
			if (is_door(&finfo)) {
				if (!nowarn) {
					errmsgno(EX_BAD,
					"WARNING: Extracting door '%s' as plain file.\n",
						finfo.f_name);
				}
				if (!get_file(&finfo))
					continue;
			} else if (is_fifo(&finfo)) {
				if (!make_fifo(&finfo))
					continue;
			} else {
				if (!make_special(&finfo))
					continue;
			}
		} else if (is_meta(&finfo)) {
			void_file(&finfo);
		} else if (!get_file(&finfo))
				continue;
		if (!to_stdout)
			setmodes(&finfo);
	}
	dirtimes("", (struct timeval *)0);
}

EXPORT BOOL
newer(info)
	FINFO	*info;
{
	FINFO	cinfo;

	if (uncond)
		return (FALSE);
	if (!getinfo(info->f_name, &cinfo)) {
		if (refresh_old) {
			errmsgno(EX_BAD, "file '%s' does not exists.\n", info->f_name);
			return (TRUE);
		}
		return (FALSE);
	}
	if (keep_old) {
		if (!nowarn)
			errmsgno(EX_BAD, "file '%s' exists.\n", info->f_name);
		return (TRUE);
	}
	/*
	 * XXX nsec beachten wenn im Archiv!
	 */
	if (cinfo.f_mtime >= info->f_mtime) {

	isnewer:
		if (!nowarn)
			errmsgno(EX_BAD, "current '%s' newer.\n", info->f_name);
		return (TRUE);
	} else if ((cinfo.f_mtime % 2) == 0 && (cinfo.f_mtime + 1) == info->f_mtime) {
		/*
		 * The DOS FAT filestem does only support a time granularity
		 * of 2 seconds. So we need to be a bit more generous.
		 * XXX We should be able to test the filesytem type.
		 */
		goto isnewer;
	}
	return (FALSE);
}

LOCAL BOOL
same_symlink(info)
	FINFO	*info;
{
	FINFO	finfo;
	char	lname[PATH_MAX+1];
	TCB	tb;

	finfo.f_lname = lname;
	finfo.f_lnamelen = 0;

	if (uncond || !getinfo(info->f_name, &finfo))
		return (FALSE);

	/*
	 * Bei symlinks gehen nicht: lchmod lchtime & teilweise lchown
	 */
#ifdef	S_IFLNK
	if (!is_symlink(&finfo))	/* File on disk */
		return (FALSE);

	fillbytes(&tb, sizeof(TCB), '\0');
	info_to_tcb(&finfo, &tb);	/* XXX ist das noch nötig ??? */
					/* z.Zt. wegen linkflag/uname/gname */

	if (read_symlink(info->f_name, &finfo, &tb)) {
		if (streql(info->f_lname, finfo.f_lname)) {
			if (!nowarn)
				errmsgno(EX_BAD, "current '%s' is same symlink.\n",
								info->f_name);
			return (TRUE);
		}
	}
#ifdef	XXX
	/*
	 * XXX nsec beachten wenn im Archiv!
	 */
	if (finfo.f_mtime >= info->f_mtime) {
		if (!nowarn)
			errmsgno(EX_BAD, "current '%s' newer.\n", info->f_name);
		return (TRUE);
	}
#endif	/* XXX*/

#endif
	return (FALSE);
}

LOCAL BOOL
create_dirs(name)
	register char	*name;
{
	register char	*np;
	register char	*dp;
		 int	err;

	if (nodir) {
		errmsgno(EX_BAD, "Directories not created.\n");
		return (FALSE);
	}
	np = dp = name;
	do {
		if (*np == '/')
			dp = np;
	} while (*np++);
	if (dp == name)
		return TRUE;
	*dp = '\0';
	if (access(name, 0) < 0) {
		if (mkdir(name, 0777) < 0) {
			if (!create_dirs(name) || mkdir(name, 0777) < 0) {
				err = geterrno();
				if ((err == EACCES || err == EEXIST))
					goto exists;

				*dp = '/';
				return FALSE;
			}
		}
	} else {
		FINFO	dinfo;

	exists:
		if (getinfo(name, &dinfo) && !is_dir(&dinfo) &&
		    remove_file(name, FALSE) && mkdir(name, 0777) < 0) {
			if (!create_dirs(name) || mkdir(name, 0777) < 0) {
				*dp = '/';
				return FALSE;
			}
		}
	}
	*dp = '/';
	return TRUE;
}

LOCAL BOOL
make_dir(info)
	FINFO	*info;
{
	FINFO	dinfo;
	int	err;

	if (dometa)
		return (TRUE);

	if (create_dirs(info->f_name)) {
		if (getinfo(info->f_name, &dinfo) && is_dir(&dinfo))
			return (TRUE);
		if (uncond)
			unlink(info->f_name);
		if (mkdir(info->f_name, 0777) >= 0)
			return (TRUE);
		err = geterrno();
		if ((err == EACCES || err == EEXIST) &&
				remove_file(info->f_name, FALSE)) {
			if (mkdir(info->f_name, 0777) >= 0)
				return (TRUE);
		}
	}
	xstats.s_openerrs++;
	errmsg("Cannot make dir '%s'.\n", info->f_name);
	return FALSE;
}

LOCAL BOOL
make_link(info)
	FINFO	*info;
{
	int	err;

	if (dometa)
		return (TRUE);

	if (copylinks)
		return (make_copy(info, FALSE));
	else if (hardlinks)
		return (emul_link(info));

#ifdef	HAVE_LINK
	if (uncond)
		unlink(info->f_name);
	if (link(info->f_lname, info->f_name) >= 0)
		return (TRUE);
	err = geterrno();
	if (create_dirs(info->f_name)) {
		if (link(info->f_lname, info->f_name) >= 0)
			return (TRUE);
		err = geterrno();
	}
	if ((err == EACCES || err == EEXIST) &&
			remove_file(info->f_name, FALSE)) {
		if (link(info->f_lname, info->f_name) >= 0)
			return (TRUE);
	}
	xstats.s_openerrs++;
	errmsg("Cannot link '%s' to '%s'.\n", info->f_name, info->f_lname);
	return(FALSE);
#else	/* HAVE_LINK */
	xstats.s_isspecial++;
	errmsgno(EX_BAD, "Not supported. Cannot link '%s' to '%s'.\n",
						info->f_name, info->f_lname);
	return (FALSE);
#endif	/* HAVE_LINK */
}

LOCAL BOOL
make_symlink(info)
	FINFO	*info;
{
	int	err;

	if (dometa)
		return (TRUE);

	if (copylinks)
		return (make_copy(info, TRUE));
	else if (symlinks)
		return (emul_symlink(info));

#ifdef	S_IFLNK
	if (uncond)
		unlink(info->f_name);
	if (sxsymlink(info) >= 0)
		return (TRUE);
	err = geterrno();
	if (create_dirs(info->f_name)) {
		if (sxsymlink(info) >= 0)
			return (TRUE);
		err = geterrno();
	}
	if ((err == EACCES || err == EEXIST) &&
			remove_file(info->f_name, FALSE)) {
		if (sxsymlink(info) >= 0)
			return (TRUE);
	}
	xstats.s_openerrs++;
	errmsg("Cannot create symbolic link '%s' to '%s'.\n",
						info->f_name, info->f_lname);
	return (FALSE);
#else	/* S_IFLNK */
	xstats.s_isspecial++;
	errmsgno(EX_BAD, "Not supported. Cannot create symbolic link '%s' to '%s'.\n",
						info->f_name, info->f_lname);
	return (FALSE);
#endif	/* S_IFLNK */
}

LOCAL BOOL
emul_symlink(info)
	FINFO	*info;
{
	errmsgno(EX_BAD, "Option -symlinks not yet implemented.\n");
	errmsgno(EX_BAD, "Cannot create symbolic link '%s' to '%s'.\n",
						info->f_name, info->f_lname);
	return (FALSE);
}

LOCAL BOOL
emul_link(info)
	FINFO	*info;
{
	errmsgno(EX_BAD, "Option -hardlinks not yet implemented.\n");
	errmsgno(EX_BAD, "Cannot link '%s' to '%s'.\n", info->f_name, info->f_lname);
#ifdef	HAVE_LINK
	return (FALSE);
#else
	return (FALSE);
#endif	/* S_IFLNK */
}

LOCAL BOOL
make_copy(info, do_symlink)
	FINFO	*info;
	BOOL	do_symlink;
{
	int	ret;
	int	err;

	if (uncond)
		unlink(info->f_name);

	if ((ret = copy_file(info->f_lname, info->f_name, do_symlink)) >= 0)
		return (TRUE);
	err = geterrno();
	if (ret != -2 && create_dirs(info->f_name)) {
		if (copy_file(info->f_lname, info->f_name, do_symlink) >= 0)
			return (TRUE);
		err = geterrno();
	}
	if ((err == EACCES || err == EEXIST || err == EISDIR) &&
			remove_file(info->f_name, FALSE)) {
		if (copy_file(info->f_lname, info->f_name, do_symlink) >= 0)
			return (TRUE);
	}
	xstats.s_openerrs++;
	errmsg("Cannot create link copy '%s' from '%s'.\n",
					info->f_name, info->f_lname);
	return(FALSE);
}

LOCAL int
copy_file(from, to, do_symlink)
	char	*from;
	char	*to;
	BOOL	do_symlink;
{
	FINFO	finfo;
	FILE	*fin;
	FILE	*fout;
	int	cnt = -1;
	char	buf[8192];
	char	nbuf[PATH_MAX+1];

	/*
	 * When tar archives hard links, both names (from/to) are relative to
	 * the current directory. With symlinks this does not work. Symlinks
	 * are always evaluated relative to the directory they reside in.
	 * For this reason, we cannot simply open the from/to files if we
	 * like to emulate a symbolic link. To emulate the behavior of a
	 * symbolic link, we concat the the directory part of the 'to' name
	 * (which is the path that becomes the sombolic link) to the complete
	 * 'from' name (which is the path the symbolic linkc pints to) in case
	 * the 'from' name is a relative path name.
	 */
	if (do_symlink && from[0] != '/') {
		char	*p = strrchr(to, '/');
		int	len;

		if (p) {
			len = p - to + 1;
			strncpy(nbuf, to, len);
			if ((len + strlen(from)) > PATH_MAX) {
				xstats.s_toolong++;
				errmsgno(EX_BAD,
				"Name too long. Cannot copy from '%s'.\n", from);
				return (-2);
			}
			strcpy(&nbuf[len], from);
			from = nbuf;
		}
	}
	if (!getinfo(from, &finfo)) {
		xstats.s_staterrs++;
		errmsg("Cannot stat '%s'.\n", from);
		return (-2);
	}
	if (!is_file(&finfo)) {
		errmsgno(EX_BAD, "Not a file. Cannot copy from '%s'.\n", from);
		seterrno(EINVAL);
		return (-2);
	}

	if ((fin = fileopen(from, "rub")) == 0) {
		errmsg("Cannot open '%s'.\n", from);
	} else {
		if ((fout = fileopen(to, "wtcub")) == 0) {
/*			errmsg("Cannot create '%s'.\n", to);*/
			return (-1);
		} else {
			while ((cnt = ffileread(fin, buf, sizeof(buf))) > 0)
				ffilewrite(fout, buf, cnt);
			fclose(fout);
		}
		fclose(fin);
	}
	return (cnt);
}

LOCAL BOOL
make_fifo(info)
	FINFO	*info;
{
	int	mode;
	int	err;

	if (dometa)
		return (TRUE);

#ifdef	HAVE_MKFIFO
	mode = info->f_mode | info->f_type;
	if (uncond)
		unlink(info->f_name);
	if (mkfifo(info->f_name, mode) >= 0)
		return (TRUE);
	err = geterrno();
	if (create_dirs(info->f_name)) {
		if (mkfifo(info->f_name, mode) >= 0)
			return (TRUE);
		err = geterrno();
	}
	if ((err == EACCES || err == EEXIST) &&
			remove_file(info->f_name, FALSE)) {
		if (mkfifo(info->f_name, mode) >= 0)
			return (TRUE);
	}
	xstats.s_openerrs++;
	errmsg("Cannot make fifo '%s'.\n", info->f_name);
	return (FALSE);
#else
#ifdef	HAVE_MKNOD
	return (make_special(info));
#endif
	xstats.s_isspecial++;
	errmsgno(EX_BAD, "Not supported. Cannot make fifo '%s'.\n",
							info->f_name);
	return (FALSE);
#endif
}

LOCAL BOOL
make_special(info)
	FINFO	*info;
{
	int	mode;
	int	dev;
	int	err;

	if (dometa)
		return (TRUE);

#ifdef	HAVE_MKNOD
	mode = info->f_mode | info->f_type;
	dev = info->f_rdev;
	if (uncond)
		unlink(info->f_name);
	if (mknod(info->f_name, mode, dev) >= 0)
		return (TRUE);
	err = geterrno();
	if (create_dirs(info->f_name)) {
		if (mknod(info->f_name, mode, dev) >= 0)
			return (TRUE);
		err = geterrno();
	}
	if ((err == EACCES || err == EEXIST) &&
			remove_file(info->f_name, FALSE)) {
		if (mknod(info->f_name, mode, dev) >= 0)
			return (TRUE);
	}
	xstats.s_openerrs++;
	errmsg("Cannot make %s '%s'.\n",
					is_fifo(info)?"fifo":"special",
							info->f_name);
	return (FALSE);
#else
	xstats.s_isspecial++;
	errmsgno(EX_BAD, "Not supported. Cannot make %s '%s'.\n",
					is_fifo(info)?"fifo":"special",
							info->f_name);
	return (FALSE);
#endif
}

LOCAL BOOL
get_file(info)
		FINFO	*info;
{
		FILE	*f;
		int	err;
		int	ret;

	if (dometa)
		return (TRUE);

	if (to_stdout) {
		f = stdout;
	} else if ((f = fileopen(info->f_name, "wctub")) == (FILE *)NULL) {
		err = geterrno();
		if (err == EMISSDIR && create_dirs(info->f_name))
			return get_file(info);
		if ((err == EACCES || err == EEXIST || err == EISDIR) &&
					remove_file(info->f_name, FALSE)) {
			return get_file(info);
		}

		xstats.s_openerrs++;
		errmsg("Cannot create '%s'.\n", info->f_name);
		void_file(info);
		return (FALSE);
	}
	file_raise(f, FALSE);

	if (is_sparse(info)) {
		ret = get_sparse(f, info);
	} else if (force_hole) {
		ret = get_forced_hole(f, info);
	} else {
		ret = xt_file(info, (int(*)__PR((void *, char *, int)))ffilewrite,
						f, 0, "writing");
	}
	if (ret < 0) {
		snulltimes(info->f_name, info);
		die(EX_BAD);
	}
	if (!to_stdout) {
#ifdef	HAVE_FSYNC
		int	cnt;
#endif
		if (ret == FALSE)
			xstats.s_rwerrs--;	/* Compensate overshoot below */

		if (fflush(f) != 0)
			ret = FALSE;
#ifdef	HAVE_FSYNC
		err = 0;
		cnt = 0;
		do {
			if (fsync(fdown(f)) != 0)
				err = geterrno();

			if (err == EINVAL)
				err = 0;
		} while (err == EINTR && ++cnt < 10);
		if (err != 0)
			ret = FALSE;
#endif
		if (fclose(f) != 0)
			ret = FALSE;
		if (ret == FALSE) {
			xstats.s_rwerrs++;
			snulltimes(info->f_name, info);
		}
	}
	return (ret);
}

/* ARGSUSED */
LOCAL int
void_func(vp, p, amount)
	void	*vp;
	char	*p;
	int	amount;
{
	return (amount);
}

EXPORT BOOL
void_file(info)
		FINFO	*info;
{
	int	ret;

	/*
	 * handle botch in gnu sparse file definitions
	 */
	if (props.pr_flags & PR_GNU_SPARSE_BUG)
		if (gnu_skip_extended(info->f_tcb) < 0)
			die(EX_BAD);

	ret = xt_file(info, void_func, 0, 0, "void");
	if (ret < 0)
		die(EX_BAD);
	return (ret);
}

EXPORT int
xt_file(info, func, arg, amt, text)
		FINFO	*info;
		int	(*func) __PR((void *, char *, int));
		void	*arg;
		int	amt;
		char	*text;
{
	register int	amount; /* XXX ??? */
	register off_t	size;
	register int	tasize;
		 BOOL	ret = TRUE;

	size = info->f_rsize;
	if (amt == 0)
		amt = bufsize;
	while (size > 0) {
		/*
		 * Replace TBLOCK by 1 for cpio.
		 */
		amount = buf_rwait(TBLOCK);
		if (amount < TBLOCK) {
			errmsgno(EX_BAD, "Tar file too small (amount: %d bytes).\n", amount);
			errmsgno(EX_BAD, "Unexpected EOF on input.\n");
			return (-1);
		}
		amount = (amount / TBLOCK) * TBLOCK;
		amount = min(size, amount);
		amount = min(amount, amt);
		tasize = tarsize(amount);

		if ((*func)(arg, bigptr, amount) != amount) {
			ret = FALSE;
			xstats.s_rwerrs++;
			errmsg("Error %s '%s'.\n", text, info->f_name);
			/* func -> void_func() setzen ???? */
		}

		size -= amount;
		buf_rwake(tasize);
	}
	return (ret);
}

EXPORT void
skip_slash(info)
	FINFO	*info;
{
	static	BOOL	warned = FALSE;

	if (!warned && !nowarn) {
		errmsgno(EX_BAD, "WARNING: skipping leading '/' on filenames.\n");
		warned = TRUE;
	}
	/* XXX
	 * XXX ACHTUNG: ia_change kann es nötig machen, den String umzukopieren
	 * XXX denn sonst ist die Länge des Speicherplatzes unbestimmt!
	 *
	 * XXX ACHTUNG: mir ist noch unklar, ob es richtig ist, auch in jedem
	 * XXX Fall Führende slashes vom Linknamen zu entfernen.
	 * XXX Bei Hard-Link ist das sicher richtig und ergibt sich auch
	 * XXX automatisch, wenn man nur vor dem Aufruf von skip_slash()
	 * XXX auf f_name[0] == '/' abfragt.
	 */
	while (info->f_name[0] == '/')
		info->f_name++;

	/*
	 * Don't strip leading '/' from targets of symlinks.
	 */
	if (is_symlink(info))
		return;

	while (info->f_lname[0] == '/')
		info->f_lname++;
}
