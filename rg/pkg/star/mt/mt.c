/* @(#)mt.c	1.11 02/04/20 Copyright 2000 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)mt.c	1.11 02/04/20 Copyright 2000 J. Schilling";
#endif
/*
 *	Magnetic tape manipulation program
 *
 *	Copyright (c) 2000 J. Schilling
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

#include <stdio.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <strdefs.h>
#include <utypes.h>
#include <fctldefs.h>
#include <sys/ioctl.h>
/*#undef	HAVE_SYS_MTIO_H*/
#ifdef	HAVE_SYS_MTIO_H
#include <sys/mtio.h>
#else
#include "mtio.h"
#endif
#include <errno.h>

#include <schily.h>
#include <standard.h>
#include "remote.h"

LOCAL BOOL	help;
LOCAL BOOL	prvers;
LOCAL int	debug;

LOCAL struct mtop	mt_op;
LOCAL struct mtget	mt_status;

#define	NO_ASF		1000
#define	NO_NBSF		1001
#ifndef	MTASF
#	define	MTASF	NO_ASF
#endif
#ifndef	MTNBSF
#	define	MTNBSF	NO_NBSF
#endif

#define	MTC_NONE	0	/* No flags defined			*/
#define	MTC_RW		0	/* This command writes to the tape	*/
#define	MTC_RDO		1	/* This command does not write		*/
#define	MTC_CNT		2	/* This command uses the count arg	*/


LOCAL struct mt_cmds {
	char *mtc_name;		/* The name of the command		*/
	char *mtc_text;		/* Description of the command		*/
	int mtc_opcode;		/* The opcode for mtio			*/
	int mtc_flags;		/* Flags for this command		*/
} cmds[] = {
#ifdef	MTWEOF
	{ "weof",	"write EOF mark",		MTWEOF,		MTC_RW|MTC_CNT },
	{ "eof",	"write EOF mark",		MTWEOF,		MTC_RW|MTC_CNT },
#endif
#ifdef	MTFSF
	{ "fsf",	"forward skip FILE mark",	MTFSF,		MTC_RDO|MTC_CNT },
#endif
#ifdef	MTBSF
	{ "bsf",	"backward skip FILE mark",	MTBSF,		MTC_RDO|MTC_CNT },
#endif
	{ "asf",	"absolute FILE mark pos",	MTASF,		MTC_RDO|MTC_CNT },
#ifdef	MTFSR
	{ "fsr",	"forward skip record",		MTFSR,		MTC_RDO|MTC_CNT },
#endif
#ifdef	MTBSR
	{ "bsr",	"backward skip record",		MTBSR,		MTC_RDO|MTC_CNT },
#endif
#ifdef	MTREW
	{ "rewind",	"rewind tape",			MTREW,		MTC_RDO },
#endif
#ifdef	MTOFFL
	{ "offline",	"rewind and unload",		MTOFFL,		MTC_RDO },
	{ "rewoffl",	"rewind and unload",		MTOFFL,		MTC_RDO },
#endif
#ifdef	MTNOP
	{ "status",	"get tape status",		MTNOP,		MTC_RDO },
#endif
#ifdef	MTRETEN
	{ "retension",	"retension tape cartridge",	MTRETEN,	MTC_RDO },
#endif
#ifdef	MTERASE
	{ "erase",	"erase tape",			MTERASE,	MTC_RW },
#endif
#ifdef	MTEOM
	{ "eom",	"position to EOM",		MTEOM,		MTC_RDO },
#endif

#if	MTNBSF != NO_NBSF
	{ "nbsf",	"backward skip FILE mark",	MTNBSF,		MTC_RDO|MTC_CNT },
#endif

#ifdef	MTLOAD
	{ "load",	"load tape",			MTLOAD,		MTC_RDO },
#endif

	{ NULL, 	NULL,				0,		MTC_NONE }
};

LOCAL	void	usage		__PR((int ex));
EXPORT	int	main		__PR((int ac, char *av[]));
LOCAL	void	mtstatus	__PR((struct mtget *sp));
LOCAL	char 	*print_key	__PR((int key));
LOCAL	int	openremote	__PR((char *tape));
LOCAL	int	opentape	__PR((char *tape, struct mt_cmds *cp));
LOCAL	int	mtioctl		__PR((int cmd, caddr_t arg));

LOCAL void
usage(ex)
	int	ex;
{
	struct mt_cmds	*cp;
	int		i;

	error("Usage: mt [ -f device ] command [ count ]\n");
	error("Commands are:\n");
	for (cp = cmds; cp->mtc_name != NULL; cp++) {
		error("%s%n", cp->mtc_name, &i);
		error("%*s%s\n", 14-i, "", cp->mtc_text);
	}
	exit(ex);
}

LOCAL char	opts[] = "f*,t*,version,help,h";

int
main(ac, av)
	int	ac;
	char	*av[];
{
	int	cac;
	char	* const *cav;
	char	*tape = NULL;
	char	*cmd = "BADCMD";
	int	count = 1;
	struct mt_cmds	*cp;

	save_args(ac, av);
	cac = --ac;
	cav = ++av;
	
	if (getallargs(&cac, &cav, opts,
			&tape, &tape,
			&prvers,
			&help, &help) < 0) {
		errmsgno(EX_BAD, "Bad Option: '%s'.\n", cav[0]);
		usage(EX_BAD);
	}
	if (help) usage(0);
	if (prvers) {
		printf("mt %s (%s-%s-%s)\n\n", "1.11", HOST_CPU, HOST_VENDOR, HOST_OS);
		printf("Copyright (C) 2000 Jörg Schilling\n");
		printf("This is free software; see the source for copying conditions.  There is NO\n");
		printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
		exit(0);
	}

	if (tape == NULL && (tape = getenv("TAPE")) == NULL) {
#ifdef	DEFTAPE
		tape = DEFTAPE;
#else
		errmsgno(EX_BAD, "No default tape defined.\n");
		usage(EX_BAD);
		/* NOTREACHED */
#endif
	}

	cac = ac;
	cav = av;
	if (getfiles(&cac, &cav, opts) == 0) {
		errmsgno(EX_BAD, "Missing args.\n");
		usage(EX_BAD);
	} else {
		cmd = cav[0];
		cav++;
		cac--;	
	}
	if (getfiles(&cac, &cav, opts) > 0) {
		if (*astoi(cav[0], &count) != '\0') {
			errmsgno(EX_BAD, "Not a number: '%s'.\n", cav[0]);
			usage(EX_BAD);
		}
		if (count < 0) {
			comerrno(EX_BAD, "negative file number or repeat count\n");
			/* NOTREACHED */
		}
		cav++;
		cac--;	
	}
	if (getfiles(&cac, &cav, opts) > 0) {
		errmsgno(EX_BAD, "Too many args.\n");
		usage(EX_BAD);
	}

	for (cp = cmds; cp->mtc_name != NULL; cp++) {
		if (strncmp(cmd, cp->mtc_name, strlen(cmd)) == 0)
			break;
	}
	if (cp->mtc_name == NULL) {
		comerrno(EX_BAD, "Unknown command: %s\n", cmd);
		/* NOTREACHED */
	}
#ifdef	DEBUG
	error("cmd: %s opcode: %d %s %s\n",
		cp->mtc_name, cp->mtc_opcode,
		(cp->mtc_flags & MTC_RDO) != 0 ? "RO":"RW",
		(cp->mtc_flags & MTC_CNT) != 0 ? "usecount":"");
#endif

	if ((cp->mtc_flags & MTC_CNT) == 0)
		count = 1;

#ifdef	USE_REMOTE
	rmtdebug(debug);
	(void)openremote(tape);		/* This needs super user privilleges */
#endif
#ifdef	HAVE_SETREUID
	if (setreuid(-1, getuid()) < 0)
#else
#ifdef	HAVE_SETEUID
	if (seteuid(getuid()) < 0)
#else
	if (setuid(getuid()) < 0)
#endif
#endif
		comerr("Panic cannot set back efective uid.\n");

	if (opentape(tape, cp) < 0) {
		if (geterrno() == EIO) {
			comerrno(EX_BAD, "'%s': no tape loaded or drive offline.\n",
				tape);
		} else if (geterrno() == EACCES) {
			comerrno(EX_BAD, "'%s': tape is write protected.\n", tape);
		} else {
			comerr("Cannot open '%s'.\n", tape); 
		}
		/* NOTREACHED */
	}

#ifdef	DEBUG
	error("Tape: %s cmd : %s (%s) count: %d\n", tape, cmd, cp->mtc_name, count);
#endif

	if (cp->mtc_opcode == MTNOP) {
		/*
		 * Status ioctl
		 */
		if (mtioctl(MTIOCGET, (caddr_t)&mt_status) < 0) {
			comerr("Cannot get mt status from '%s'.\n", tape); 
			/* NOTREACHED */
		}
		mtstatus(&mt_status);
#if	MTASF == NO_ASF
	} else if (cp->mtc_opcode == MTASF) {
		if (mtioctl(MTIOCGET, (caddr_t)&mt_status) < 0) {
			comerr("Cannot get mt status from '%s'.\n", tape); 
			/* NOTREACHED */
		}
		/*
		 * If the device does not support to report the current file
		 * tape file position - rewind the tape, and space forward.
		 */
#ifndef	MTF_ASF
		if (1) {
#else
		if (!(mt_status.mt_flags & MTF_ASF) || MTNBSF == NO_NBSF) {
#endif
			mt_status.mt_fileno = 0;
			mt_op.mt_count = 1;
			mt_op.mt_op = MTREW;
			if (mtioctl(MTIOCTOP, (caddr_t)&mt_op) < 0) {
				comerr("%s %s %d failed\n", tape, cp->mtc_name,
							count);
				/* NOTREACHED */
			}
		}
		if (count < mt_status.mt_fileno) {
			mt_op.mt_op = MTNBSF;
			mt_op.mt_count =  mt_status.mt_fileno - count;
			/*printf("mt: bsf= %d\n", mt_op.mt_count);*/
		} else {
			mt_op.mt_op = MTFSF;
			mt_op.mt_count =  count - mt_status.mt_fileno;
			/*printf("mt: fsf= %d\n", mt_op.mt_count);*/
		}
		if (mtioctl(MTIOCTOP, (caddr_t)&mt_op) < 0) {
			if (mtioctl(MTIOCTOP, (caddr_t)&mt_op) < 0) {
				comerr("%s %s %d failed\n", tape, cp->mtc_name,
							count);
				/* NOTREACHED */
			}
		}
#endif
	} else {
		/*
		 * Regular magnetic tape ioctl
		 */
		mt_op.mt_op = cp->mtc_opcode;
		mt_op.mt_count = count;
		if (mtioctl(MTIOCTOP, (caddr_t)&mt_op) < 0) {
			comerr("%s %s %ld failed\n", tape, cp->mtc_name,
						(long)mt_op.mt_count);
			/* NOTREACHED */
		}
	}
	return (0);
}

/*
 * If we try to make this portable, we need a way to initialize it
 * in an OS independant way.
 * Don't use it for now.
 */
LOCAL struct tape_info {
	short	t_type;		/* type of magnetic tape device	*/
	char	*t_name;	/* name for prining		*/
	char	*t_dsbits;	/* "drive status" register	*/
	char	*t_erbits;	/* "error" register		*/
} tapes[] = {
#ifdef	XXX
	{ MT_ISTS,	"ts11",		0,		TSXS0_BITS },
#endif
	{ 0 }
};

/*
 * Interpret the status buffer returned
 */
LOCAL void
mtstatus(sp)
	register struct mtget *sp;
{
	register struct tape_info *mt = NULL;

#ifdef	XXX
#ifdef	HAVE_MTGET_TYPE
	for (mt = tapes; mt->t_type; mt++)
		if (mt->t_type == sp->mt_type)
			break;
#endif
#endif

#if	defined(HAVE_MTGET_FLAGS) && defined(MTF_SCSI)

	if ((sp->mt_flags & MTF_SCSI)) {
		/*
		 * Handle SCSI tape drives specially.
		 */
#ifdef	HAVE_MTGET_TYPE
		if (mt != NULL && mt->t_type == sp->mt_type)
			printf("%s tape drive:\n", mt->t_name);
		else
			printf("%s tape drive:\n", "SCSI");
#else
		printf("unknown SCSI tape drive:\n");
#endif

		printf("   sense key(0x%x)= %s   residual= %ld   ",
			sp->mt_erreg, print_key(sp->mt_erreg), (long)sp->mt_resid);
		printf("retries= %ld\n", (long)sp->mt_dsreg);
	} else
#endif	/* HAVE_MTGET_FLAGS */
		{
		/*
		 * Handle other drives below.
		 */
#ifdef	HAVE_MTGET_TYPE
		if (mt == NULL || mt->t_type == 0) {
			printf("unknown tape drive type (0x%lx)\n", (long)sp->mt_type);
		} else {
			printf("%s tape drive:\n", mt->t_name);
		}
#else
		printf("unknown tape drive:\n");
#endif
#ifdef	HAVE_MTGET_RESID
		printf("   residual= %ld", (long)sp->mt_resid);
#endif
		/*
		 * If we implement better support for specific OS,
		 * then we may want to implement something like the
		 * *BSD kernel %b printf format (e.g. printreg).
		 */
#ifdef	HAVE_MTGET_DSREG
		printf  ("   ds = %lx", (unsigned long)sp->mt_dsreg);
#endif
#ifdef	HAVE_MTGET_ERREG
		printf  ("   er = %lx", (unsigned long)sp->mt_erreg);
#endif
		putchar('\n');
	}
	printf("   file no= %lld   block no= %lld\n",
#ifdef	HAVE_MTGET_FILENO
			(Llong)sp->mt_fileno,
#else
			(Llong)-1,
#endif
#ifdef	HAVE_MTGET_BLKNO
			(Llong)sp->mt_blkno);
#else
			(Llong)-1);
#endif
#ifdef	 HAVE_MTGET_BF
	printf("   optimum blocking factor= %ld\n", (long)sp->mt_bf);
#endif
#ifdef	 HAVE_MTGET_FLAGS
	printf("   flags= 0x%lX\n", (long)sp->mt_flags);
#endif
}

static char *sense_keys[] = {
	"No Additional Sense",		/* 0x00 */
	"Recovered Error",		/* 0x01 */
	"Not Ready",			/* 0x02 */
	"Medium Error",			/* 0x03 */
	"Hardware Error",		/* 0x04 */
	"Illegal Request",		/* 0x05 */
	"Unit Attention",		/* 0x06 */
	"Data Protect",			/* 0x07 */
	"Blank Check",			/* 0x08 */
	"Vendor Unique",		/* 0x09 */
	"Copy Aborted",			/* 0x0a */
	"Aborted Command",		/* 0x0b */
	"Equal",			/* 0x0c */
	"Volume Overflow",		/* 0x0d */
	"Miscompare",			/* 0x0e */
	"Reserved"			/* 0x0f */
};

LOCAL char *
print_key(key)
	int	key;
{
	static	char keys[32];

	if (key >= 0 && key < (sizeof(sense_keys)/sizeof(sense_keys[0])))
		return (sense_keys[key]);
	js_snprintf(keys, sizeof(keys), "Unknown Key: %d", key);
	return (keys);
}

/*--------------------------------------------------------------------------*/
LOCAL int	isremote;
LOCAL int	remfd	= -1;
LOCAL int	mtfd;
LOCAL char	*remfn;

#ifdef	USE_REMOTE
LOCAL int
openremote(tape)
	char	*tape;
{
	char	host[128];

	if ((remfn = rmtfilename(tape)) != NULL) {
		rmthostname(host, tape, sizeof(host));
		isremote++;

		if (debug)
			errmsgno(EX_BAD, "Remote: %s Host: %s file: %s\n",
							tape, host, remfn);

		if ((remfd = rmtgetconn(host, 4096)) < 0)
			comerrno(EX_BAD, "Cannot get connection to '%s'.\n",
				/* errno not valid !! */		host);
	}
	return (isremote);
}
#endif

LOCAL int
opentape(tape, cp)
		char		*tape;
	register struct mt_cmds *cp;
{
	if (isremote) {
#ifdef	USE_REMOTE
		if (rmtopen(remfd, remfn, (cp->mtc_flags&MTC_RDO) ? 0 : 2) < 0)
			return (-1);
#else
		comerrno(EX_BAD, "Remote tape support not present.\n");
#endif
	} else if ((mtfd = open(tape, (cp->mtc_flags&MTC_RDO) ? 0 : 2)) < 0) {
			return (-1);
	}
	return (0);
}

LOCAL int
mtioctl(cmd, arg)
	int	cmd;
	caddr_t	arg;
{
	int	ret = -1;
	struct mtget *mtp;
	struct mtop *mop;

	if (isremote) {
#ifdef	USE_REMOTE
		switch (cmd) {

		case MTIOCGET:
			ret = rmtstatus(remfd, (struct mtget *)arg);
			if (ret < 0)
				return (ret);

			mtp = (struct mtget *)arg;
#ifdef	DEBUG
error("type: %X ds: %X er: %X resid: %d fileno: %d blkno: %d flags: %X bf: %d\n",
mtp->mt_type, mtp->mt_dsreg, mtp->mt_erreg, mtp->mt_resid, mtp->mt_fileno,
mtp->mt_blkno, mtp->mt_flags, mtp->mt_bf);
#endif
			break;
		case MTIOCTOP:
			mop = (struct mtop *)arg;
			ret = rmtioctl(remfd, mop->mt_op, mop->mt_count);
			break;
		default:
			comerrno(ENOTTY, "Invalid mtioctl.\n");
			/* NOTREACHED */
		}
#else
		comerrno(EX_BAD, "Remote tape support not present.\n");
#endif
	} else {
		ret = ioctl(mtfd, cmd, arg);
	}
	return (ret);
}
