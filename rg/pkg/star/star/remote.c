/*#define	USE_REMOTE*/
/* @(#)remote.c	1.39 02/05/20 Copyright 1990 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)remote.c	1.39 02/05/20 Copyright 1990 J. Schilling";
#endif
/*
 *	Remote tape interface
 *
 *	Copyright (c) 1990 J. Schilling
 *
 *	TOTO:
 *		Signal handler for SIGPIPE
 *		check rmtaborted for exit() / clean abort of connection
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

#if !defined(HAVE_NETDB_H) || !defined(HAVE_RCMD)
#undef	USE_REMOTE				/* There is no rcmd() */
#endif

#ifdef	USE_REMOTE
#include <stdio.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <fctldefs.h>
#ifdef	HAVE_SYS_MTIO_H
#include <sys/mtio.h>
#else
#include "mtio.h"
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <pwd.h>
#include <standard.h>
#include <strdefs.h>
#include <utypes.h>
#include <schily.h>
#include "remote.h"

#if	defined(SIGDEFER) || defined(SVR4)
#define	signal	sigset
#endif

#define	CMD_SIZE	80

LOCAL	BOOL	rmt_debug;

LOCAL	void	rmtabrt			__PR((int sig));
EXPORT	int	rmtdebug		__PR((int dlevel));
EXPORT	char	*rmthostname		__PR((char *hostname, char *rmtspec, int size));
EXPORT	int	rmtgetconn		__PR((char* host, int size));
LOCAL	void	rmtoflags		__PR((int fmode, char *cmode));
EXPORT	int	rmtopen			__PR((int fd, char* fname, int fmode));
EXPORT	int	rmtclose		__PR((int fd));
EXPORT	int	rmtread			__PR((int fd, char* buf, int count));
EXPORT	int	rmtwrite		__PR((int fd, char* buf, int count));
EXPORT	off_t	rmtseek			__PR((int fd, off_t offset, int whence));
EXPORT	int	rmtioctl		__PR((int fd, int cmd, int count));
LOCAL	int	rmtmapold		__PR((int cmd));
LOCAL	int	rmtmapnew		__PR((int cmd));
LOCAL	Llong	rmtxstatus		__PR((int fd, int cmd));
LOCAL	int	rmt_v1_status		__PR((int fd, struct mtget* mtp));
EXPORT	int	rmtstatus		__PR((int fd, struct mtget* mtp));
LOCAL	Llong	rmtcmd			__PR((int fd, char* name, char* cbuf));
LOCAL	void	rmtsendcmd		__PR((int fd, char* name, char* cbuf));
LOCAL	int	rmtgetline		__PR((int fd, char* line, int count));
LOCAL	Llong	rmtgetstatus		__PR((int fd, char* name));
LOCAL	int	rmtaborted		__PR((int fd));
EXPORT	char	*rmtfilename		__PR((char *name));

LOCAL void
rmtabrt(sig)
	int	sig;
{
	rmtaborted(-1);
}

EXPORT int
rmtdebug(dlevel)
	int	dlevel;
{
	int	odebug = rmt_debug;

	rmt_debug = dlevel;
	return (odebug);
}

EXPORT char *
rmthostname(hostname, rmtspec, size)
		 char	*hostname;
		 char	*rmtspec;
	register int	size;
{
	register int	i;
	register char	*hp;
	register char	*fp;
	register char	*remfn;

	if ((remfn = rmtfilename(rmtspec)) == NULL) {
		hostname[0] = '\0';
		return (NULL);
	}
	remfn--;
	for (fp = rmtspec, hp = hostname, i = 1;
			fp < remfn && i < size; i++) {
		*hp++ = *fp++;
	}
	*hp = '\0';
	return (hostname);
}

EXPORT int
rmtgetconn(host, size)
	char	*host;
	int	size;
{
	static	struct servent	*sp = 0;
	static	struct passwd	*pw = 0;
		char		*name = "root";
		char		*p;
		int		rmtsock;
		char		*rmtpeer;
		char		rmtuser[128];


	signal(SIGPIPE, rmtabrt);
	if (sp == 0) {
		sp = getservbyname("shell", "tcp");
		if (sp == 0) {
			comerrno(EX_BAD, "shell/tcp: unknown service\n");
			/* NOTREACHED */
		}
		pw = getpwuid(getuid());
		if (pw == 0) {
			comerrno(EX_BAD, "who are you? No passwd entry found.\n");
			/* NOTREACHED */
		}
	}
	if ((p = strchr(host, '@')) != NULL) {
		size_t d = p - host;

		if (d > sizeof(rmtuser))
			d = sizeof(rmtuser);
		js_snprintf(rmtuser, sizeof(rmtuser), "%.*s",
							(int)d, host);
		name = rmtuser;
		host = &p[1];
	} else {
		name = pw->pw_name;
	}
	if (rmt_debug)
		errmsgno(EX_BAD, "locuser: '%s' rmtuser: '%s' host: '%s'\n",
						pw->pw_name, name, host);
	rmtpeer = host;
	rmtsock = rcmd(&rmtpeer, (unsigned short)sp->s_port,
					pw->pw_name, name, "/etc/rmt", 0);
/*					pw->pw_name, name, "/etc/xrmt", 0);*/

	if (rmtsock < 0)
		return (-1);


#ifdef	SO_SNDBUF
	while (size > 512 &&
		setsockopt(rmtsock, SOL_SOCKET, SO_SNDBUF,
					(char *)&size, sizeof (size)) < 0) {
		size -= 512;
	}
	if (rmt_debug)
		errmsgno(EX_BAD, "sndsize: %d\n", size);
#endif
#ifdef	SO_RCVBUF
	while (size > 512 &&
		setsockopt(rmtsock, SOL_SOCKET, SO_RCVBUF,
					(char *)&size, sizeof (size)) < 0) {
		size -= 512;
	}
	if (rmt_debug)
		errmsgno(EX_BAD, "rcvsize: %d\n", size);
#endif

	return (rmtsock);
}

LOCAL void
rmtoflags(fmode, cmode)
	int	fmode;
	char	*cmode;
{
	register char	*p;
	register int	amt;
	register int	maxcnt = CMD_SIZE;

	switch (fmode & O_ACCMODE) {

	case O_RDONLY:	p = "O_RDONLY";	break;
	case O_RDWR:	p = "O_RDWR";	break;
	case O_WRONLY:	p = "O_WRONLY";	break;

	default:	p = "Cannot Happen";
	}
	amt = js_snprintf(cmode, maxcnt, p); if (amt < 0) return;
	p = cmode;
	p += amt;
	maxcnt -= amt;
#ifdef	O_TEXT
	if (fmode & O_TEXT) {
		amt = js_snprintf(p, maxcnt, "|O_TEXT"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_NDELAY
	if (fmode & O_NDELAY) {
		amt = js_snprintf(p, maxcnt, "|O_NDELAY"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_APPEND
	if (fmode & O_APPEND) {
		amt = js_snprintf(p, maxcnt, "|O_APPEND"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_SYNC
	if (fmode & O_SYNC) {
		amt = js_snprintf(p, maxcnt, "|O_SYNC"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_DSYNC
	if (fmode & O_DSYNC) {
		amt = js_snprintf(p, maxcnt, "|O_DSYNC"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_RSYNC
	if (fmode & O_RSYNC) {
		amt = js_snprintf(p, maxcnt, "|O_RSYNC"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_NONBLOCK
	if (fmode & O_NONBLOCK) {
		amt = js_snprintf(p, maxcnt, "|O_NONBLOCK"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_PRIV
	if (fmode & O_PRIV) {
		amt = js_snprintf(p, maxcnt, "|O_PRIV"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_LARGEFILE
	if (fmode & O_LARGEFILE) {
		amt = js_snprintf(p, maxcnt, "|O_LARGEFILE"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_CREAT
	if (fmode & O_CREAT) {
		amt = js_snprintf(p, maxcnt, "|O_CREAT"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_TRUNC
	if (fmode & O_TRUNC) {
		amt = js_snprintf(p, maxcnt, "|O_TRUNC"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_EXCL
	if (fmode & O_EXCL) {
		amt = js_snprintf(p, maxcnt, "|O_EXCL"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
#ifdef	O_NOCTTY
	if (fmode & O_NOCTTY) {
		amt = js_snprintf(p, maxcnt, "|O_NOCTTY"); if (amt < 0) return;
		p += amt;
		maxcnt -= amt;
	}
#endif
}

EXPORT int
rmtopen(fd, fname, fmode)
	int	fd;
	char	*fname;
	int	fmode;
{
	char	cbuf[CMD_SIZE];
	char	cmode[CMD_SIZE];
	int	ret;

	/*
	 * Convert all fmode bits into the symbolic fmode.
	 * only send the lowest 2 bits in numeric mode as it would be too
	 * dangerous because the apropriate bits differ between different
	 * operating systems.
	 */
	rmtoflags(fmode, cmode);
	js_snprintf(cbuf, CMD_SIZE, "O%s\n%d %s\n", fname, fmode & O_ACCMODE, cmode);
	ret = rmtcmd(fd, "open", cbuf);
	if (ret < 0)
		return (ret);

	/*
	 * Tell the rmt server that we are aware of Version 1 commands.
	 */
/*	(void)rmtioctl(fd, RMTIVERSION, 0);*/
	(void)rmtioctl(fd, -1, 0);

	return (ret);
}

EXPORT int
rmtclose(fd)
	int	fd;
{
	return (rmtcmd(fd, "close", "C\n"));
}

EXPORT int
rmtread(fd, buf, count)
	int	fd;
	char	*buf;
	int	count;
{
	char	cbuf[CMD_SIZE];
	int	n;
	int	amt = 0;
	int	cnt;

	js_snprintf(cbuf, CMD_SIZE, "R%d\n", count);
	n = rmtcmd(fd, "read", cbuf);
	if (n < 0)
		return (-1);

	/*
	 * Nice idea from disassembling Solaris ufsdump...
	 */
	if (n > count) {
		errmsgno(EX_BAD,
			"rmtread: expected response size %d, got %d\n",
			count, n);
		errmsgno(EX_BAD,
			"This means the remote rmt daemon is not compatible.\n");
		return (rmtaborted(fd));
		/*
		 * XXX Should we better abort (exit) here?
		 */
	}
	while (amt < n) {
		if ((cnt = _niread(fd, &buf[amt], n - amt)) <= 0) {
			return (rmtaborted(fd));
		}
		amt += cnt;
	}

	return (amt);
}

EXPORT int
rmtwrite(fd, buf, count)
	int	fd;
	char	*buf;
	int	count;
{
	char	cbuf[CMD_SIZE];

	js_snprintf(cbuf, CMD_SIZE, "W%d\n", count);
	rmtsendcmd(fd, "write", cbuf);
	if (_niwrite(fd, buf, count) != count)
		rmtaborted(fd);
	return (rmtgetstatus(fd, "write"));
}

EXPORT off_t
rmtseek(fd, offset, whence)
	int	fd;
	off_t	offset;
	int	whence;
{
	char	cbuf[CMD_SIZE];

	switch (whence) {

	case 0: whence = SEEK_SET; break;
	case 1: whence = SEEK_CUR; break;
	case 2: whence = SEEK_END; break;

	default:
		seterrno(EINVAL);
		return (-1);
	}

	js_snprintf(cbuf, CMD_SIZE, "L%lld\n%d\n", (Llong)offset, whence);
	return ((off_t)rmtcmd(fd, "seek", cbuf));
}

EXPORT int
rmtioctl(fd, cmd, count)
	int	fd;
	int	cmd;
	int	count;
{
	char	cbuf[CMD_SIZE];
	char	c = 'I';
	int	rmtversion = RMT_NOVERSION;
	int	i;

	if (cmd != RMTIVERSION)
		rmtversion = rmtioctl(fd, RMTIVERSION, 0);

	if (cmd >= 0 && (rmtversion == RMT_VERSION)) {
		/*
		 * Opcodes 0..7 are unique across different architectures.
		 * But as in many cases Linux does not even follow this rule.
		 * If we know that we are calling a VERSION 1 client, we may 
		 * safely assume that the client is not using Linux mapping
		 * but the standard mapping.
		 */
		i = rmtmapold(cmd);
		if (cmd <= 7 && i  < 0) {
			/*
			 * We cannot map the current command but it's value is
			 * within the range 0..7. Do not send it over the wire.
			 */
			seterrno(EINVAL);
			return (-1);
		}
		if (i >= 0)
			cmd = i;
	}
	if (cmd > 7 && (rmtversion == RMT_VERSION)) {
		i = rmtmapnew(cmd);
		if (i >= 0) {
			cmd = i;
			c = 'i';
		}
	}

	js_snprintf(cbuf, CMD_SIZE, "%c%d\n%d\n", c, cmd, count);
	return (rmtcmd(fd, "ioctl", cbuf));
}

/*
 * Map all old opcodes that should be in range 0..7 to numbers /etc/rmt expects
 * This is needed because Linux does not follow the UNIX conventions.
 */
LOCAL int
rmtmapold(cmd)
	int	cmd;
{
	switch (cmd) {

#ifdef	MTWEOF
	case  MTWEOF:	return (0);
#endif

#ifdef	MTFSF
	case MTFSF:	return (1);
#endif

#ifdef	MTBSF
	case MTBSF:	return (2);
#endif

#ifdef	MTFSR
	case MTFSR:	return (3);
#endif

#ifdef	MTBSR
	case MTBSR:	return (4);
#endif

#ifdef	MTREW
	case MTREW:	return (5);
#endif

#ifdef	MTOFFL
	case MTOFFL:	return (6);
#endif

#ifdef	MTNOP
	case MTNOP:	return (7);
#endif
	}
	return (-1);
}

/*
 * Map all new opcodes that should be in range above 7 to the 
 * values expected by the 'i' command of /etc/rmt.
 */
LOCAL int
rmtmapnew(cmd)
	int	cmd;
{
	switch (cmd) {

#ifdef	MTCACHE
	case MTCACHE:	return (RMTICACHE);
#endif

#ifdef	MTNOCACHE
	case MTNOCACHE:	return (RMTINOCACHE);
#endif

#ifdef	MTRETEN
	case MTRETEN:	return (RMTIRETEN);
#endif

#ifdef	MTERASE
	case MTERASE:	return (RMTIERASE);
#endif

#ifdef	MTEOM
	case MTEOM:	return (RMTIEOM);
#endif

#ifdef	MTNBSF
	case MTNBSF:	return (RMTINBSF);
#endif
	}
	return (-1);
}

LOCAL Llong
rmtxstatus(fd, cmd)
	int	fd;
	char	cmd;
{
	char	cbuf[CMD_SIZE];

			/* No newline */
	js_snprintf(cbuf, CMD_SIZE, "s%c", cmd);
	return (rmtcmd(fd, "extended status", cbuf));
}

LOCAL int
rmt_v1_status(fd, mtp)
	int		fd;
	struct  mtget	*mtp;
{
	mtp->mt_erreg = mtp->mt_type = 0;

#ifdef	HAVE_MTGET_ERREG
	mtp->mt_erreg  = rmtxstatus(fd, MTS_ERREG); /* must be first */
#endif
#ifdef	HAVE_MTGET_TYPE
	mtp->mt_type   = rmtxstatus(fd, MTS_TYPE);
#endif
	if (mtp->mt_erreg == -1 || mtp->mt_type == -1)
		return (-1);

#ifdef	HAVE_MTGET_DSREG	/* doch immer vorhanden ??? */
	mtp->mt_dsreg  = rmtxstatus(fd, MTS_DSREG);
#endif
#ifdef	HAVE_MTGET_RESID
	mtp->mt_resid  = rmtxstatus(fd, MTS_RESID);
#endif
#ifdef	HAVE_MTGET_FILENO
	mtp->mt_fileno = rmtxstatus(fd, MTS_FILENO);
#endif
#ifdef	HAVE_MTGET_BLKNO
	mtp->mt_blkno  = rmtxstatus(fd, MTS_BLKNO);
#endif
#ifdef	HAVE_MTGET_FLAGS
	mtp->mt_flags  = rmtxstatus(fd, MTS_FLAGS);
#endif
#ifdef	HAVE_MTGET_BF
	mtp->mt_bf     = rmtxstatus(fd, MTS_BF);
#endif
	return (0);
}

EXPORT int
rmtstatus(fd, mtp)
	int		fd;
	struct  mtget	*mtp;
{
	register int i;
	register char *cp;
	char	c;
	int	n;

	if (rmtioctl(fd, RMTIVERSION, 0) == RMT_VERSION)
		return (rmt_v1_status(fd, mtp));

				/* No newline */
	if ((n = rmtcmd(fd, "status", "S")) < 0)
		return (-1);

	/*
	 * From disassembling Solaris ufsdump, they seem to check
	 * only if (n > sizeof(mts)).
	 */
	if (n != sizeof(struct mtget)) {
		errmsgno(EX_BAD,
			"rmtstatus: expected response size %d, got %d\n",
			(int)sizeof(struct mtget), n);
		errmsgno(EX_BAD,
			"This means the remote rmt daemon is not compatible.\n");
		/*
		 * XXX should we better abort here?
		 */
	}

	for (i = 0, cp = (char *)mtp; i < sizeof(struct mtget); i++)
		*cp++ = 0;
	for (i = 0, cp = (char *)mtp; i < n; i++) {
		/*
		 * Make sure to read all bytes because we otherwise
		 * would confuse the protocol. Do not copy more
		 * than the size of our local struct mtget.
		 */
		if (_niread(fd, &c, 1) != 1)
			return (rmtaborted(fd));

		if (i < sizeof(struct mtget))
			*cp++ = c;
	}
	/*
	 * The GNU remote tape lib tries to swap the structure based on the
	 * value of mt_type. While this makes sense for UNIX, it will not
	 * work if one system is running Linux. The Linux mtget structure
	 * is completely incompatible (mt_type is long instead of short).
	 */
	return (n);
}

LOCAL Llong
rmtcmd(fd, name, cbuf)
	int	fd;
	char	*name;
	char	*cbuf;
{
	rmtsendcmd(fd, name, cbuf);
	return (rmtgetstatus(fd, name));
}

LOCAL void
rmtsendcmd(fd, name, cbuf)
	int	fd;
	char	*name;
	char	*cbuf;
{
	int	buflen = strlen(cbuf);

	seterrno(0);
	if (_niwrite(fd, cbuf, buflen) != buflen)
		rmtaborted(fd);
}

LOCAL int
rmtgetline(fd, line, count)
	int	fd;
	char	*line;
	int	count;
{
	register char	*cp;

	for (cp = line; cp < &line[count]; cp++) {
		if (_niread(fd, cp, 1) != 1)
			return (rmtaborted(fd));

		if (*cp == '\n') {
			*cp = '\0';
			return (cp - line);
		}
	}
	if (rmt_debug)
		errmsgno(EX_BAD, "Protocol error (in rmtgetline).\n");
	return (rmtaborted(fd));
}

LOCAL Llong
rmtgetstatus(fd, name)
	int	fd;
	char	*name;
{
	char	cbuf[CMD_SIZE];
	char	code;
	Llong	number;

	rmtgetline(fd, cbuf, sizeof(cbuf));
	code = cbuf[0];
	astoll(&cbuf[1], &number);

	if (code == 'E' || code == 'F') {
		rmtgetline(fd, cbuf, sizeof(cbuf));
		if (code == 'F')	/* should close file ??? */
			rmtaborted(fd);
		if (rmt_debug)
			errmsgno(number, "Remote status(%s): %lld '%s'.\n",
							name, number, cbuf);
		seterrno(number);
		return ((Llong)-1);
	}
	if (code != 'A') {
		/* XXX Hier kommt evt Command not found ... */
		if (rmt_debug)
			errmsgno(EX_BAD, "Protocol error (got %s).\n", cbuf);
		return (rmtaborted(fd));
	}
	return (number);
}

LOCAL int
rmtaborted(fd)
	int	fd;
{
	if (rmt_debug)
		errmsgno(EX_BAD, "Lost connection to remote host ??\n");
	/* if fd >= 0 */
	/* close file */
	if (geterrno() == 0) {
/*		errno = EIO;*/
		seterrno(EPIPE);
	}
	/*
	 * BSD used EIO but EPIPE is better for something like sdd -noerror
	 */
	return (-1);
}

#endif	/* USE_REMOTE */

#ifndef	USE_REMOTE
#include <standard.h>
#include <strdefs.h>

EXPORT	char	*rmtfilename		__PR((char *name));
#endif

EXPORT char *
rmtfilename(name)
	char	*name;
{
	char	*ret;

	if (name[0] == '/')
		return (NULL);		/* Absolut pathname cannot be remote */
	if (name[0] == '.') {
		if (name[1] == '/' || (name[1] == '.' && name[2] == '/'))
			return (NULL);	/* Relative pathname cannot be remote*/
	}
	if ((ret = strchr(name, ':')) != NULL) {
		if (name[0] == ':') {
			/*
			 * This cannot be a remote filename as the host part
			 * has zero length.
			 */
			return (NULL);
		}
		ret++;	/* Skip the colon. */
	}
	return (ret);
}
