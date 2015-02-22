/* @(#)star_unix.c	1.51 02/04/28 Copyright 1985, 1995, 2001 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)star_unix.c	1.51 02/04/28 Copyright 1985, 1995, 2001 J. Schilling";
#endif
/*
 *	Stat / mode / owner routines for unix like
 *	operating systems
 *
 *	Copyright (c) 1985, 1995, 2001 J. Schilling
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
#ifndef	HAVE_UTIMES
#define	utimes	__nothing__	/* BeOS has no utimes() but wrong prototype */
#endif
#include <stdio.h>
#include <errno.h>
#include "star.h"
#include "props.h"
#include "table.h"
#include <standard.h>
#include <unixstd.h>
#include <dirdefs.h>
#include <statdefs.h>
#include <device.h>
#include <schily.h>
#include "dirtime.h"
#include "xutimes.h"
#ifdef	__linux__
#include <fctldefs.h>
#include <linux/ext2_fs.h>
#include <sys/ioctl.h>
#endif
#ifndef	HAVE_UTIMES
#undef	utimes
#endif
#include "starsubs.h"

#ifndef	HAVE_LSTAT
#define	lstat	stat
#endif
#ifndef	HAVE_LCHOWN
#define	lchown	chown
#endif

#if   S_ISUID == TSUID && S_ISGID == TSGID && S_ISVTX == TSVTX \
   && S_IRUSR == TUREAD && S_IWUSR == TUWRITE && S_IXUSR == TUEXEC \
   && S_IRGRP == TGREAD && S_IWGRP == TGWRITE && S_IXGRP == TGEXEC \
   && S_IROTH == TOREAD && S_IWOTH == TOWRITE && S_IXOTH == TOEXEC

#define	HAVE_POSIX_MODE_BITS	/* st_mode bits are equal to TAR mode bits */
#endif
/*#undef	HAVE_POSIX_MODE_BITS*/

#define	ROOT_UID	0

extern	int	uid;
extern	dev_t	curfs;
extern	BOOL	nomtime;
extern	BOOL	nochown;
extern	BOOL	pflag;
extern	BOOL	follow;
extern	BOOL	nodump;
extern	BOOL	doacl;
extern	BOOL	dofflags;

EXPORT	BOOL	getinfo		__PR((char* name, FINFO * info));
EXPORT	void	checkarch	__PR((FILE *f));
EXPORT	void	setmodes	__PR((FINFO * info));
LOCAL	int	sutimes		__PR((char* name, FINFO *info));
EXPORT	int	snulltimes	__PR((char* name, FINFO *info));
EXPORT	int	sxsymlink	__PR((FINFO *info));
EXPORT	int	rs_acctime	__PR((int fd, FINFO * info));
#ifndef	HAVE_UTIMES
EXPORT	int	utimes		__PR((char *name, struct timeval *tp));
#endif
#ifdef	HAVE_POSIX_MODE_BITS	/* st_mode bits are equal to TAR mode bits */
#else
LOCAL	int	dochmod		__PR((const char *name, mode_t tarmode));
#define	chmod	dochmod
#endif

#ifdef	USE_ACL
/*
 * HAVE_ANY_ACL currently includes HAVE_POSIX_ACL and HAVE_SUN_ACL.
 * This definition must be in sync with the definition in acl_unix.c
 * As USE_ACL is used in star.h, we are not allowed to change the
 * value of USE_ACL before we did include star.h or we may not include
 * star.h at all.
 * HAVE_HP_ACL is currently not included in HAVE_ANY_ACL.
 */
#	ifndef	HAVE_ANY_ACL
#	undef	USE_ACL		/* Do not try to get or set ACLs */
#	endif
#endif


EXPORT BOOL
getinfo(name, info)
	char	*name;
	register FINFO	*info;
{
	struct stat	stbuf;

	if (follow?stat(name, &stbuf):lstat(name, &stbuf) < 0)
		return (FALSE);
	info->f_name = name;
	info->f_uname = info->f_gname = NULL;
	info->f_umaxlen = info->f_gmaxlen = 0L;
	info->f_dev	= stbuf.st_dev;
	if (curfs == NODEV)
		curfs = info->f_dev;
	info->f_ino	= stbuf.st_ino;
	info->f_nlink	= stbuf.st_nlink;
#ifdef	HAVE_POSIX_MODE_BITS	/* st_mode bits are equal to TAR mode bits */
	info->f_mode	= stbuf.st_mode & 07777;
#else
	/*
	 * The unexpected case when the S_I* OS mode bits do not match
	 * the T* mode bits from tar.
	 */
	{ register mode_t m = stbuf.st_mode;

	info->f_mode	= ((m & S_ISUID ? TSUID   : 0)
			 | (m & S_ISGID ? TSGID   : 0)
			 | (m & S_ISVTX ? TSVTX   : 0)
			 | (m & S_IRUSR ? TUREAD  : 0)
			 | (m & S_IWUSR ? TUWRITE : 0)
			 | (m & S_IXUSR ? TUEXEC  : 0)
			 | (m & S_IRGRP ? TGREAD  : 0)
			 | (m & S_IWGRP ? TGWRITE : 0)
			 | (m & S_IXGRP ? TGEXEC  : 0)
			 | (m & S_IROTH ? TOREAD  : 0)
			 | (m & S_IWOTH ? TOWRITE : 0)
			 | (m & S_IXOTH ? TOEXEC  : 0));
	}
#endif
	info->f_uid	= stbuf.st_uid;
	info->f_gid	= stbuf.st_gid;
	info->f_size	= (off_t)0;	/* Size of file */
	info->f_rsize	= (off_t)0;	/* Size on tape */
	info->f_flags	= 0L;
	info->f_xflags	= props.pr_xdflags;
	info->f_typeflag= 0;
	info->f_type	= stbuf.st_mode & ~07777;

	if (sizeof(stbuf.st_rdev) == sizeof(short)) {
		info->f_rdev = (Ushort) stbuf.st_rdev;
	} else if ((sizeof(int) != sizeof(long)) &&
			(sizeof(stbuf.st_rdev) == sizeof(int))) {
		info->f_rdev = (Uint) stbuf.st_rdev;
	} else {
		info->f_rdev = (Ulong) stbuf.st_rdev;
	}
	info->f_rdevmaj	= major(info->f_rdev);
	info->f_rdevmin	= minor(info->f_rdev);
	info->f_atime	= stbuf.st_atime;
	info->f_mtime	= stbuf.st_mtime;
	info->f_ctime	= stbuf.st_ctime;
#ifdef	HAVE_ST_SPARE1		/* if struct stat contains st_spare1 (usecs) */
	info->f_flags  |= F_NSECS;
	info->f_ansec	= stbuf.st_spare1*1000;
	info->f_mnsec	= stbuf.st_spare2*1000;
	info->f_cnsec	= stbuf.st_spare3*1000;
#else

#ifdef	HAVE_ST_NSEC		/* if struct stat contains st_atim.st_nsec (nanosecs */ 
	info->f_flags  |= F_NSECS;
	info->f_ansec	= stbuf.st_atim.tv_nsec;
	info->f_mnsec	= stbuf.st_mtim.tv_nsec;
	info->f_cnsec	= stbuf.st_ctim.tv_nsec;
#else
	info->f_ansec = info->f_mnsec = info->f_cnsec = 0L;
#endif	/* HAVE_ST_NSEC */
#endif	/* HAVE_ST_SPARE1 */

#ifdef	HAVE_ST_FLAGS
	/*
	 * The *BSD based method is easy to handle.
	 */
	if (dofflags)
		info->f_fflags = stbuf.st_flags;
	else
		info->f_fflags = 0L;
	if (info->f_fflags != 0)
		info->f_xflags |= XF_FFLAGS;
#ifdef	UF_NODUMP				/* The *BSD 'nodump' flag */
	if ((stbuf.st_flags & UF_NODUMP) != 0)
		info->f_flags |= F_NODUMP;	/* Convert it to star flag */
#endif
#else	/* !HAVE_ST_FLAGS */
	/*
	 * The non *BSD case...
	 */
#ifdef	USE_FFLAGS
	if ((nodump || dofflags) && !S_ISLNK(stbuf.st_mode)) {
		get_fflags(info);
		if (!dofflags)
			info->f_xflags &= ~XF_FFLAGS;
	} else {
		info->f_fflags = 0L;
	}
#else
	info->f_fflags = 0L;
#endif
#endif

	switch ((int)(stbuf.st_mode & S_IFMT)) {

	case S_IFREG:	/* regular file */
			info->f_filetype = F_FILE;
			info->f_xftype = XT_FILE;
			info->f_rsize = info->f_size = stbuf.st_size;
			info->f_rdev = 0;
			info->f_rdevmaj	= 0;
			info->f_rdevmin	= 0;
			break;
#ifdef	S_IFCNT
	case S_IFCNT:	/* contiguous file */
			info->f_filetype = F_FILE;
			info->f_xftype = XT_CONT;
			info->f_rsize = info->f_size = stbuf.st_size;
			info->f_rdev = 0;
			info->f_rdevmaj	= 0;
			info->f_rdevmin	= 0;
			break;
#endif
	case S_IFDIR:	/* directory */
			info->f_filetype = F_DIR;
			info->f_xftype = XT_DIR;
			info->f_rdev = 0;
			info->f_rdevmaj	= 0;
			info->f_rdevmin	= 0;
			break;
#ifdef	S_IFLNK
	case S_IFLNK:	/* symbolic link */
			info->f_filetype = F_SLINK;
			info->f_xftype = XT_SLINK;
			info->f_rdev = 0;
			info->f_rdevmaj	= 0;
			info->f_rdevmin	= 0;
			break;
#endif
#ifdef	S_IFCHR
	case S_IFCHR:	/* character special */
			info->f_filetype = F_SPEC;
			info->f_xftype = XT_CHR;
			break;
#endif
#ifdef	S_IFBLK
	case S_IFBLK:	/* block special */
			info->f_filetype = F_SPEC;
			info->f_xftype = XT_BLK;
			break;
#endif
#ifdef	S_IFIFO
	case S_IFIFO:	/* fifo */
			info->f_filetype = F_SPEC;
			info->f_xftype = XT_FIFO;
			break;
#endif
#ifdef	S_IFSOCK
	case S_IFSOCK:	/* socket */
			info->f_filetype = F_SPEC;
			info->f_xftype = XT_SOCK;
			break;
#endif
#ifdef	S_IFNAM
	case S_IFNAM:	/* Xenix named file */
			info->f_filetype = F_SPEC;

			/*
			 * 'st_rdev' field is really the subtype
			 * As S_INSEM & S_INSHD we may safely cast
			 * stbuf.st_rdev to int.
			 */
			switch ((int)stbuf.st_rdev) {
			case S_INSEM:
				info->f_xftype = XT_NSEM;
				break;
			case S_INSHD:
				info->f_xftype = XT_NSHD;
				break;
			default:
				info->f_xftype = XT_BAD;
				break;
			}
			break;
#endif
#ifdef	S_IFMPC
	case S_IFMPC:	/* multiplexed character special */
			info->f_filetype = F_SPEC;
			info->f_xftype = XT_MPC;
			break;
#endif
#ifdef	S_IFMPB
	case S_IFMPB:	/* multiplexed block special */
			info->f_filetype = F_SPEC;
			info->f_xftype = XT_MPB;
			break;
#endif
#ifdef	S_IFDOOR
	case S_IFDOOR:	/* door */
			info->f_filetype = F_SPEC;
			info->f_xftype = XT_DOOR;
			break;
#endif
#ifdef	S_IFWHT
	case S_IFWHT:	/* whiteout */
			info->f_filetype = F_SPEC;
			info->f_xftype = XT_WHT;
			break;
#endif

	default:	/* any other unknown file type */
			if ((stbuf.st_mode & S_IFMT) == 0) {
				/*
				 * If we come here, we either did not yet
				 * realize that somebody created a new file
				 * type with a value of 0 or the system did
				 * return an "unallocated file" with lstat().
				 * The latter happens if we are on old Solaris
				 * systems that did not yet add SOCKETS again.
				 * if somebody mounted a filesystem that
				 * has been written with a *BSD system like
				 * SunOS 4.x and this FS holds a socket we get
				 * a pseudo unallocated file...
				 */
				info->f_filetype = F_SPEC;	/* ??? */
				info->f_xftype = XT_NONE;
			} else {
				/*
				 * We don't know this file type!
				 */
				info->f_filetype = F_SPEC;
				info->f_xftype = XT_BAD;
			}
	}
	info->f_rxftype = info->f_xftype;

#ifdef	HAVE_ST_BLOCKS
#if	defined(hpux) || defined(__hpux)
	if (info->f_size > (stbuf.st_blocks * 1024 + 1024))
#else
	if (info->f_size > (stbuf.st_blocks * 512 + 512))
#endif
		info->f_flags |= F_SPARSE;
#endif

#ifdef	USE_ACL
	/*
	 * Only look for ACL's if extended headers are allowed with the current
	 * archive format. Also don't include ACL's if we are creating a Sun
	 * vendor unique variant of the extended headers. Sun's tar will not
	 * grok access control lists.
	 */
	if ((props.pr_flags & PR_XHDR) == 0 || (props.pr_xc != 'x'))
		return (TRUE);

	/*
	 * If we return (FALSE) here, the file would not be archived at all.
	 * This is not what we want, so ignore return code from get_acls().
	 */
	if (doacl)
		(void) get_acls(info);
#endif  /* USE_ACL */

	return (TRUE);
}

EXPORT void
checkarch(f)
	FILE	*f;
{
	struct stat	stbuf;
	extern	dev_t	tape_dev;
	extern	ino_t	tape_ino;
	extern	BOOL	tape_isreg;

	tape_isreg = FALSE;
	tape_dev = (dev_t)0;
	tape_ino = (ino_t)0;

	if (fstat(fdown(f), &stbuf) < 0)
		return;
	
	if (S_ISREG(stbuf.st_mode)) {
		tape_dev = stbuf.st_dev;
		tape_ino = stbuf.st_ino;
		tape_isreg = TRUE;
	} else if (((stbuf.st_mode & S_IFMT) == 0) ||
			S_ISFIFO(stbuf.st_mode) ||
			S_ISSOCK(stbuf.st_mode)) {
		/*
		 * This is a pipe or similar on different UNIX implementations.
		 * (stbuf.st_mode & S_IFMT) == 0 may happen in stange cases.
		 */
		tape_dev = NODEV;
		tape_ino = (ino_t)-1;
	}
}

EXPORT void
setmodes(info)
	register FINFO	*info;
{
	BOOL	didutimes = FALSE;

	if (!nomtime && !is_symlink(info)) {
		/*
		 * With Cygwin32,
		 * DOS will not allow us to set file times on read-only files.
		 * We set the time before we change the access modes to
		 * overcode this problem.
		 */
		if (!is_dir(info)) {
			didutimes = TRUE;
			if (sutimes(info->f_name, info) < 0)
				xstats.s_settime++;
		}
	}

#ifndef	NEW_P_FLAG
	if ((!is_dir(info) || pflag) && !is_symlink(info)) {
		if (chmod(info->f_name, (int)info->f_mode) < 0) {
			xstats.s_setmodes++;
		}
	}
#ifdef	USE_ACL
	if (pflag && !is_symlink(info)) {
		/*
		 * If there are no additional ACLs, set_acls() will
		 * simply do a fast return.
		 */
		if (doacl)
			set_acls(info);
	}
#endif
	if (dofflags && !is_symlink(info))
		set_fflags(info);
#else
/* XXX Checken! macht die Aenderung des Verhaltens von -p Sinn? */

	if (pflag && !is_symlink(info)) {
		if (chmod(info->f_name, (int)info->f_mode) < 0) {
			xstats.s_setmodes++;
		}
#ifdef	USE_ACL
		/*
		 * If there are no additional ACLs, set_acls() will
		 * simply do a fast return.
		 */
		if (doacl)
			set_acls(info);
#endif
	}
	if (dofflags && !is_symlink(info))
		set_fflags(info);
#endif
	if (!nochown && uid == ROOT_UID) {
		/*
		 * Star will not allow non root users to give away files.
		 */
		lchown(info->f_name, (int)info->f_uid, (int)info->f_gid);

#ifndef	NEW_P_FLAG
		if ((!is_dir(info) || pflag) && !is_symlink(info) &&
#else
/* XXX Checken! macht die Aenderung des Verhaltens von -p Sinn? */
		if (pflag && !is_symlink(info) &&
#endif
				(info->f_mode & 07000) != 0) {
			/*
			 * On a few systems, seeting the owner of a file
			 * destroys the suid and sgid bits.
			 * We repeat the chmod() in this case.
			 */
			if (chmod(info->f_name, (int)info->f_mode) < 0) {
				/*
				 * Do not increment chmod() errors here,
				 * it did already fail above.
				 */
				/* EMPTY */
				;
			}
		}
	}
	if (!nomtime && !is_symlink(info)) {
		if (is_dir(info)) {
			sdirtimes(info->f_name, info);
		} else {
			if (sutimes(info->f_name, info) < 0 && !didutimes)
				xstats.s_settime++;
		}
	}
}

EXPORT	int	xutimes		__PR((char* name, struct timeval *tp));

LOCAL int
sutimes(name, info)
	char	*name;
	FINFO	*info;
{
	struct	timeval tp[3];

	tp[0].tv_sec = info->f_atime;
	tp[0].tv_usec = info->f_ansec/1000;

	tp[1].tv_sec = info->f_mtime;
	tp[1].tv_usec = info->f_mnsec/1000;
#ifdef	SET_CTIME
	tp[2].tv_sec = info->f_ctime;
	tp[2].tv_usec = info->f_cnsec/1000;
#else
	tp[2].tv_sec = 0;
	tp[2].tv_usec = 0;
#endif
	return (xutimes(name, tp));
}

EXPORT int
snulltimes(name, info)
	char	*name;
	FINFO	*info;
{
	struct	timeval tp[3];

	fillbytes((char *)tp, sizeof(tp), '\0');
	return (xutimes(name, tp));
}

EXPORT int
xutimes(name, tp)
	char	*name;
	struct	timeval tp[3];
{
	struct  timeval curtime;
	struct  timeval pasttime;
	extern int Ctime;
	int	ret;
	int	errsav;

#ifndef	HAVE_SETTIMEOFDAY
#undef	SET_CTIME
#endif

#ifdef	SET_CTIME
	if (Ctime) {
		gettimeofday(&curtime, 0);
		settimeofday(&tp[2], 0);
	}
#endif
	ret = utimes(name, tp);
	errsav = geterrno();

#ifdef	SET_CTIME
	if (Ctime) {
		gettimeofday(&pasttime, 0);
		/* XXX Hack: f_ctime.tv_usec ist immer 0! */
		curtime.tv_usec += pasttime.tv_usec;
		if (curtime.tv_usec > 1000000) {
			curtime.tv_sec += 1;
			curtime.tv_usec -= 1000000;
		}
		settimeofday(&curtime, 0);
/*		error("pasttime.usec: %d\n", pasttime.tv_usec);*/
	}
#endif
	seterrno(errsav);
	return (ret);
}

EXPORT int
sxsymlink(info)
	FINFO	*info;
{
	struct	timeval tp[3];
	struct  timeval curtime;
	struct  timeval pasttime;
	char	*linkname;
	char	*name;
	extern int Ctime;
	int	ret;
	int	errsav;

	tp[0].tv_sec = info->f_atime;
	tp[0].tv_usec = info->f_ansec/1000;

	tp[1].tv_sec = info->f_mtime;
	tp[1].tv_usec = info->f_mnsec/1000;
#ifdef	SET_CTIME
	tp[2].tv_sec = info->f_ctime;
	tp[2].tv_usec = info->f_cnsec/1000;
#endif
	linkname = info->f_lname;
	name = info->f_name;

#ifdef	SET_CTIME
	if (Ctime) {
		gettimeofday(&curtime, 0);
		settimeofday(&tp[2], 0);
	}
#endif
	ret = symlink(linkname, name);
	errsav = geterrno();

#ifdef	SET_CTIME
	if (Ctime) {
		gettimeofday(&pasttime, 0);
		/* XXX Hack: f_ctime.tv_usec ist immer 0! */
		curtime.tv_usec += pasttime.tv_usec;
		if (curtime.tv_usec > 1000000) {
			curtime.tv_sec += 1;
			curtime.tv_usec -= 1000000;
		}
		settimeofday(&curtime, 0);
/*		error("pasttime.usec: %d\n", pasttime.tv_usec);*/
	}
#endif
	seterrno(errsav);
	return (ret);
}

#ifdef	HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

EXPORT int
rs_acctime(fd, info)
		 int	fd;
	register FINFO	*info;
{
#ifdef	_FIOSATIME
	struct timeval	atv;
#endif

	if (is_symlink(info))
		return (0);

#ifdef	_FIOSATIME
	/*
	 * On Solaris 2.x root may reset accesstime without changing ctime.
	 */
	if (uid == ROOT_UID) {
		atv.tv_sec = info->f_atime;
		atv.tv_usec = info->f_ansec/1000;
		return (ioctl(fd, _FIOSATIME, &atv));
	}
#endif
	return (sutimes(info->f_name, info));
}

#ifndef	HAVE_UTIMES

#define	utimes	__nothing__	/* BeOS has no utimes() but wrong prototype */
#include <utimdefs.h>
#undef	utimes

EXPORT int
utimes(name, tp)
	char		*name;
	struct timeval	*tp;
{
	struct utimbuf	ut;

	ut.actime = tp->tv_sec;
	tp++;
	ut.modtime = tp->tv_sec;
	
	return (utime(name, &ut));
}
#endif

#ifdef	HAVE_POSIX_MODE_BITS	/* st_mode bits are equal to TAR mode bits */
#else
#undef	chmod
LOCAL int
dochmod(name, tarmode)
	register const char	*name;
	register mode_t		tarmode;
{
	register mode_t		osmode;

	osmode	= ((tarmode & TSUID   ? S_ISUID : 0)
		 | (tarmode & TSGID   ? S_ISGID : 0)
		 | (tarmode & TSVTX   ? S_ISVTX : 0)
		 | (tarmode & TUREAD  ? S_IRUSR : 0)
		 | (tarmode & TUWRITE ? S_IWUSR : 0)
		 | (tarmode & TUEXEC  ? S_IXUSR : 0)
		 | (tarmode & TGREAD  ? S_IRGRP : 0)
		 | (tarmode & TGWRITE ? S_IWGRP : 0)
		 | (tarmode & TGEXEC  ? S_IXGRP : 0)
		 | (tarmode & TOREAD  ? S_IROTH : 0)
		 | (tarmode & TOWRITE ? S_IWOTH : 0)
		 | (tarmode & TOEXEC  ? S_IXOTH : 0));

	return (chmod(name, osmode));
}
#endif
