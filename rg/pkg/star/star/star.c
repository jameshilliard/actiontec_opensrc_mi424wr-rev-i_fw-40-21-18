/* @(#)star.c	1.122 02/05/17 Copyright 1985, 88-90, 92-96, 98, 99, 2000-2002 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)star.c	1.122 02/05/17 Copyright 1985, 88-90, 92-96, 98, 99, 2000-2002 J. Schilling";
#endif
/*
 *	Copyright (c) 1985, 88-90, 92-96, 98, 99, 2000-2002 J. Schilling
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
#include <unixstd.h>
#include <signal.h>
#include <strdefs.h>
#include "star.h"
#include "diff.h"
#include <waitdefs.h>
#include <standard.h>
#include <patmatch.h>
#define	__XDEV__	/* Needed to activate _dev_init() */
#include <device.h>
#include <getargs.h>
#include <schily.h>
#include "starsubs.h"
#include "fifo.h"

EXPORT	int	main		__PR((int ac, char** av));
LOCAL	void	getdir		__PR((int *acp, char *const **avp,
						const char **dirp));
LOCAL	char	*dogetwdir	__PR((void));
LOCAL	BOOL	dochdir		__PR((const char *dir, BOOL doexit));
LOCAL	void	openlist	__PR((void));
LOCAL	void	susage		__PR((int ret));
LOCAL	void	usage		__PR((int ret));
LOCAL	void	xusage		__PR((int ret));
LOCAL	void	dusage		__PR((int ret));
LOCAL	void	husage		__PR((int ret));
LOCAL	void	gargs		__PR((int ac, char *const* av));
LOCAL	Llong	number		__PR((char* arg, int* retp));
LOCAL	int	getnum		__PR((char* arg, long* valp));
/*LOCAL	int	getllnum	__PR((char* arg, Llong* valp));*/
LOCAL	int	getlldefault	__PR((char* arg, Llong* valp, int mult));
LOCAL	int	getbnum		__PR((char* arg, Llong* valp));
LOCAL	int	getknum		__PR((char* arg, Llong* valp));
LOCAL	int	addtarfile	__PR((const char *tarfile));
EXPORT	const char *filename	__PR((const char *name));
LOCAL	BOOL	nameprefix	__PR((const char *patp, const char *name));
LOCAL	int	namefound	__PR((const char* name));
EXPORT	BOOL	match		__PR((const char* name));
LOCAL	int	addpattern	__PR((const char* pattern));
LOCAL	int	addarg		__PR((const char* pattern));
LOCAL	void	closepattern	__PR((void));
LOCAL	void	printpattern	__PR((void));
LOCAL	int	add_diffopt	__PR((char* optstr, long* flagp));
LOCAL	int	gethdr		__PR((char* optstr, long* typep));
#ifdef	USED
LOCAL	int	addfile		__PR((char* optstr, long* dummy));
#endif
LOCAL	void	exsig		__PR((int sig));
LOCAL	void	sighup		__PR((int sig));
LOCAL	void	sigintr		__PR((int sig));
LOCAL	void	sigquit		__PR((int sig));
LOCAL	void	getstamp	__PR((void));
EXPORT	void	*__malloc	__PR((size_t size, char *msg));
EXPORT	void	*__realloc	__PR((void *ptr, size_t size, char *msg));
EXPORT	char	*__savestr	__PR((char *s));
LOCAL	void	docompat	__PR((int *pac, char *const **pav));

#if	defined(SIGDEFER) || defined(SVR4)
#define	signal	sigset
#endif

#define	QIC_24_TSIZE	122880		/*  61440 kBytes */
#define	QIC_120_TSIZE	256000		/* 128000 kBytes */
#define	QIC_150_TSIZE	307200		/* 153600 kBytes */
#define	QIC_250_TSIZE	512000		/* 256000 kBytes (XXX not verified)*/
#define	TSIZE(s)	((s)*TBLOCK)

#define	SECOND		(1)
#define	MINUTE		(60 * SECOND)
#define	HOUR		(60 * MINUTE)
#define DAY		(24 * HOUR)
#define YEAR		(365 * DAY)
#define LEAPYEAR	(366 * DAY)

char	strvers[] = "1.4";

struct star_stats	xstats;

#define	NPAT	100

EXPORT	BOOL		havepat = FALSE;
LOCAL	int		npat	= 0;
LOCAL	int		narg	= 0;
LOCAL	int		maxplen	= 0;
LOCAL	int		*aux[NPAT];
LOCAL	int		alt[NPAT];
LOCAL	int		*state;
LOCAL	const	Uchar	*pat[NPAT];
LOCAL	const	char	*dirs[NPAT];

#define	NTARFILE	100

FILE	*tarf;
FILE	*listf;
FILE	*tty;
FILE	*vpr;
const	char	*tarfiles[NTARFILE];
int	ntarfiles;
int	tarfindex;
char	*newvol_script;
char	*listfile;
char	*stampfile;
const	char	*wdir;
const	char	*currdir;
const	char	*dir_flags = NULL;
char	*volhdr;
dev_t	tape_dev;
ino_t	tape_ino;
BOOL	tape_isreg = FALSE;
#ifdef	FIFO
BOOL	use_fifo = TRUE;
#else
BOOL	use_fifo = FALSE;
#endif
BOOL	shmflag	= FALSE;
long	fs;
long	bs;
int	nblocks = 20;
int	uid;
dev_t	curfs = NODEV;
/*
 * Change default header format into XUSTAR in 2004 (see below in gargs())
 */
long	hdrtype	= H_XSTAR;	/* default header format */
long	chdrtype= H_UNDEF;	/* command line hdrtype	 */
int	version	= 0;
int	swapflg	= -1;
BOOL	debug	= FALSE;
BOOL	showtime= FALSE;
BOOL	no_stats= FALSE;
BOOL	do_fifostats= FALSE;
BOOL	numeric	= FALSE;
int	verbose = 0;
BOOL	silent = FALSE;
BOOL	prblockno = FALSE;
BOOL	tpath	= FALSE;
BOOL	cflag	= FALSE;
BOOL	uflag	= FALSE;
BOOL	rflag	= FALSE;
BOOL	xflag	= FALSE;
BOOL	tflag	= FALSE;
BOOL	nflag	= FALSE;
BOOL	diff_flag = FALSE;
BOOL	zflag	= FALSE;
BOOL	bzflag	= FALSE;
BOOL	multblk	= FALSE;
BOOL	ignoreerr = FALSE;
BOOL	nodir	= FALSE;
BOOL	nomtime	= FALSE;
BOOL	nochown	= FALSE;
BOOL	acctime	= FALSE;
BOOL	pflag	= FALSE;
BOOL	dirmode	= FALSE;
BOOL	nolinkerr = FALSE;
BOOL	follow	= FALSE;
BOOL	nodesc	= FALSE;
BOOL	nomount	= FALSE;
BOOL	interactive = FALSE;
BOOL	signedcksum = FALSE;
BOOL	partial	= FALSE;
BOOL	nospec	= FALSE;
int	Fflag	= 0;
BOOL	uncond	= FALSE;
BOOL	xdir	= FALSE;
BOOL	keep_old= FALSE;
BOOL	refresh_old= FALSE;
BOOL	abs_path= FALSE;
BOOL	notpat	= FALSE;
BOOL	force_hole = FALSE;
BOOL	sparse	= FALSE;
BOOL	to_stdout = FALSE;
BOOL	wready = FALSE;
BOOL	force_remove = FALSE;
BOOL	ask_remove = FALSE;
BOOL	remove_first = FALSE;
BOOL	remove_recursive = FALSE;
BOOL	nullout = FALSE;

Ullong	maxsize	= 0;
time_t	Newer	= 0;
Ullong	tsize	= 0;
long	diffopts= 0L;
BOOL	nowarn	= FALSE;
BOOL	Ctime	= FALSE;
BOOL	nodump	= FALSE;

BOOL	listnew	= FALSE;
BOOL	listnewf= FALSE;
BOOL	hpdev	= FALSE;
BOOL	modebits= FALSE;
BOOL	copylinks= FALSE;
BOOL	hardlinks= FALSE;
BOOL	symlinks= FALSE;
BOOL	doacl	= FALSE;
BOOL	dofflags= FALSE;
BOOL	link_dirs= FALSE;
BOOL	dodump	= FALSE;
BOOL	dometa	= FALSE;

BOOL	tcompat	= FALSE;	/* Tar compatibility (av[0] is tar/ustar)   */
BOOL	fcompat	= FALSE;	/* Archive file compatibility was requested */

int	intr	= 0;

/*
 * Achtung: Optionen wie f= sind problematisch denn dadurch dass -ffilename geht,
 * werden wird bei Falschschreibung von -fifo evt. eine Datei angelegt wird.
 */
char	*opts = "C*,help,xhelp,version,debug,time,no_statistics,no-statistics,fifostats,numeric,v+,block-number,tpath,c,u,r,x,t,n,diff,diffopts&,H&,force_hole,force-hole,sparse,to_stdout,to-stdout,wready,force_remove,force-remove,ask_remove,ask-remove,remove_first,remove-first,remove_recursive,remove-recursive,nullout,onull,fifo,no_fifo,no-fifo,shm,fs&,VOLHDR*,list*,new-volume-script*,file&,f&,T,z,bz,bs&,blocks&,b&,B,pattern&,pat&,i,d,m,o,nochown,a,atime,p,dirmode,l,h,L,D,dodesc,M,I,w,O,signed_checksum,signed-checksum,P,S,F+,U,xdir,k,keep_old_files,keep-old-files,refresh_old_files,refresh-old-files,refresh,/,not,V,maxsize&,newer*,ctime,nodump,tsize&,qic24,qic120,qic150,qic250,nowarn,newest_file,newest-file,newest,hpdev,modebits,copylinks,hardlinks,symlinks,acl,xfflags,link-dirs,dump,meta,silent";

EXPORT int
main(ac, av)
	int	ac;
	char	**av;
{
	int		cac  = ac;
	char *const	*cav = av;

	save_args(ac, av);

	docompat(&cac, &cav);

	gargs(cac, cav);
	--cac,cav++;

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		(void) signal(SIGHUP, sighup);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) signal(SIGINT, sigintr);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void) signal(SIGQUIT, sigquit);

	file_raise((FILE *)NULL, FALSE);

	initbuf(nblocks);

	(void)openremote();		/* This needs super user privilleges */

	if (geteuid() != getuid()) {	/* AIX does not like to do this */
					/* If we are not root		*/
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
	}
	/*
	 * WARNING: We now are no more able to open a new remote connection
	 * unless we have been called by root.
	 * It you like to do a remote multi-tape backup to different hosts
	 * and do not call star from root, you are lost.
	 */

	opentape();

	uid = geteuid();

	if (stampfile)
		getstamp();

	setprops(chdrtype);	/* Set up properties for archive format */
	dev_init(debug);	/* Init device macro handling */
	xbinit();		/* Initialize buffer for extended headers */

#ifdef	FIFO
	if (use_fifo)
		runfifo();
#endif

	if (dir_flags)
		wdir = dogetwdir();

	if (xflag || tflag || diff_flag) {
		if (listfile) {
			openlist();
			hash_build(listf, 1000);
			if((currdir = dir_flags) != NULL)
				dochdir(currdir, TRUE);
		} else {
			for (;;--cac,cav++) {
				if (dir_flags)
					getdir(&cac, &cav, &currdir);
				if (getfiles(&cac, &cav, opts) == 0)
					break;
				addarg(cav[0]);
			}
			closepattern();
		}
		if (tflag) {
			list();
		} else {
			/*
			 * xflag || diff_flag
			 * First change dir to the one or last -C arg
			 * in case there is no pattern in list.
			 */
			if((currdir = dir_flags) != NULL)
				dochdir(currdir, TRUE);
			if (xflag)
				extract(volhdr);
			else
				diff();
		}
	}
	closepattern();
	if (uflag || rflag) {
		skipall();
		syncbuf();
		backtape();
	}
	if (cflag) {
		put_volhdr(volhdr);
		if (listfile) {
			openlist();
			if((currdir = dir_flags) != NULL)
				dochdir(currdir, TRUE);
			createlist();
		} else {
			const char	*cdir = NULL;

			for (;;--cac,cav++) {
				if (dir_flags)
					getdir(&cac, &cav, &currdir);
				if (currdir && cdir != currdir) {
					if (!(dochdir(wdir, FALSE) &&
					      dochdir(currdir, FALSE)))
						break;
					cdir = currdir;
				}

				if (getfiles(&cac, &cav, opts) == 0)
					break;
				if (intr)
					break;
				curfs = NODEV;
				create(cav[0]);
			}
		}
		weof();
		buf_drain();
	}

	if (!nolinkerr)
		checklinks();
	if (!use_fifo)
		closetape();
#ifdef	FIFO
	if (use_fifo)
		fifo_exit();
#endif

	while (wait(0) >= 0) {
		;
	}
	prstats();
	if (checkerrs()) {
		if (!nowarn && !no_stats) {
			errmsgno(EX_BAD,
			"Processed all possible files, despite earlier errors.\n");
		}
		exit(-2);
	}
	exit(0);
	/* NOTREACHED */
	return(0);	/* keep lint happy */
}

LOCAL void
getdir(acp, avp, dirp)
	int		*acp;
	char *const	**avp;
	const char	**dirp;
{
	/*
	 * Skip all other flags.
	 */
	getfiles(acp, avp, &opts[3]);

	if (debug) /* temporary */
		errmsgno(EX_BAD, "Flag/File: '%s'.\n", *avp[0]);

	if (getargs(acp, avp, "C*", dirp) < 0) {
		/*
		 * Skip all other flags.
		 */
		if (getfiles(acp, avp, &opts[3]) < 0) {
			errmsgno(EX_BAD, "Badly placed Option: %s.\n", *avp[0]);
			susage(EX_BAD);
		}
	}
	if (debug) /* temporary */
		errmsgno(EX_BAD, "Dir: '%s'.\n", *dirp);
}

#include <dirdefs.h>
#include <maxpath.h>
#include <getcwd.h>

LOCAL char *
dogetwdir()
{
	char	dir[PATH_MAX+1];
	char	*ndir;

/* XXX MAXPATHNAME vs. PATH_MAX ??? */

	if (getcwd(dir, PATH_MAX) == NULL)
		comerr("Cannot get working directory\n");
	ndir = __malloc(strlen(dir)+1, "working dir");
	strcpy(ndir, dir);
	return (ndir);
}

LOCAL BOOL
dochdir(dir, doexit)
	const char	*dir;
	BOOL		doexit;
{
	if (debug) /* temporary */
		error("dochdir(%s) = ", dir);

	if (chdir(dir) < 0) {
		int	ex = geterrno();

		if (debug) /* temporary */
			error("%d\n", ex);

		errmsg("Cannot change directory to '%s'.\n", dir);
		if (doexit)
			exit(ex);
		return (FALSE);
	}
	if (debug) /* temporary */
		error("%d\n", 0);

	return (TRUE);
}

LOCAL void
openlist()
{
	if (streql(listfile, "-")) {
		listf = stdin;
		listfile = "stdin";
	} else if ((listf = fileopen(listfile, "r")) == (FILE *)NULL)
		comerr("Cannot open '%s'.\n", listfile);
}

/*
 * Short usage
 */
LOCAL void
susage(ret)
	int	ret;
{
	error("Usage:\t%s cmd [options] file1 ... filen\n", get_progname());
	error("\nUse\t%s -help\n", get_progname());
	error("and\t%s -xhelp\n", get_progname());
	error("to get a list of valid cmds and options.\n");
	error("\nUse\t%s H=help\n", get_progname());
	error("to get a list of valid archive header formats.\n");
	error("\nUse\t%s diffopts=help\n", get_progname());
	error("to get a list of valid diff options.\n");
	exit(ret);
	/* NOTREACHED */
}

LOCAL void
usage(ret)
	int	ret;
{
	error("Usage:\tstar cmd [options] file1 ... filen\n");
	error("Cmd:\n");
	error("\t-c/-u/-r\tcreate/update/replace named files to tape\n");
	error("\t-x/-t/-n\textract/list/trace named files from tape\n");
	error("\t-diff\t\tdiff archive against file system (see -xhelp)\n");
	error("Options:\n");
	error("\t-help\t\tprint this help\n");
	error("\t-xhelp\t\tprint extended help\n");
	error("\t-version\tprint version information and exit\n");
	error("\tblocks=#,b=#\tset blocking factor to #x512 Bytes (default 20)\n"); 
	error("\tfile=nm,f=nm\tuse 'nm' as tape instead of stdin/stdout\n");
	error("\t-T\t\tuse $TAPE as tape instead of stdin/stdout\n");
#ifdef	FIFO
	error("\t-fifo/-no-fifo\tuse/don't use a fifo to optimize data flow from/to tape\n");
#if defined(USE_MMAP) && defined(USE_USGSHM)
	error("\t-shm\t\tuse SysV shared memory for fifo\n");
#endif
#endif
	error("\t-v\t\tincrement verbose level\n");
	error("\t-block-number\tprint the block numbers where the TAR headers start\n");
	error("\t-tpath\t\tuse with -t to list path names only\n");
	error("\tH=header\tgenerate 'header' type archive (see H=help)\n");
	error("\tC=dir\t\tperform a chdir to 'dir' before storing next file\n");
	error("\t-z\t\tpipe input/output through gzip, does not work on tapes\n");
	error("\t-bz\t\tpipe input/output through bzip2, does not work on tapes\n");
	error("\t-B\t\tperform multiple reads (needed on pipes)\n");
	error("\t-i\t\tignore checksum errors\n");
	error("\t-d\t\tdo not store/create directories\n");
	error("\t-m\t\tdo not restore access and modification time\n");
	error("\t-o,-nochown\tdo not restore owner and group\n");
	error("\t-a,-atime\treset access time after storing file\n");
	error("\t-p\t\trestore filemodes of directories\n");
	error("\t-l\t\tdo not print a message if not all links are dumped\n");
	error("\t-h,-L\t\tfollow symbolic links as if they were files\n");
	error("\t-D\t\tdo not descend directories\n");
	error("\t-M\t\tdo not descend mounting points\n");
	error("\t-I,-w\t\tdo interactive creation/extraction/renaming\n");
	error("\t-O\t\tbe compatible to old tar (except for checksum bug)\n");
	error("\t-P\t\tlast record may be partial (useful on cartridge tapes)\n");
	error("\t-S\t\tdo not store/create special files\n");
	error("\t-F,-FF,-FFF,...\tdo not store/create SCCS/RCS, core and object files\n");
	error("\t-U\t\trestore files unconditionally\n");
	exit(ret);
	/* NOTREACHED */
}

LOCAL void
xusage(ret)
	int	ret;
{
	error("Usage:\tstar cmd [options] file1 ... filen\n");
	error("Extended options:\n");
	error("\tdiffopts=optlst\tcomma separated list of diffopts (see diffopts=help)\n");
	error("\t-debug\t\tprint additional debug messages\n");
	error("\t-silent\t\tno not print informational messages\n");
	error("\t-not,-V\t\tuse those files which do not match pattern\n");
	error("\tVOLHDR=name\tuse name to generate a volume header\n");
	error("\t-xdir\t\textract dir even if the current is never\n");
	error("\t-dirmode\t\twrite directories after the files they contain\n");
	error("\t-link-dirs\tlook for hard linked directories in create mode\n");
	error("\t-dump\t\texperimental option for incremental dumps (more ino metadata)\n");
	error("\t-meta\t\texperimental option to use inode metadata only\n");
	error("\t-keep-old-files,-k\tkeep existing files\n");
	error("\t-refresh-old-files\trefresh existing files, don't create new files\n");
	error("\t-refresh\trefresh existing files, don't create new files\n");
	error("\t-/\t\tdon't strip leading '/'s from file names\n");
	error("\tlist=name\tread filenames from named file\n");
	error("\t-dodesc\t\tdo descend directories found in a list= file\n");
	error("\tpattern=p,pat=p\tset matching pattern\n");
	error("\tmaxsize=#\tdo not store file if bigger than # (default mult is kB)\n");
	error("\tnewer=name\tstore only files which are newer than 'name'\n");
	error("\tnew-volume-script=script\tcall 'scipt' at end of each volume\n");
	error("\t-ctime\t\tuse ctime for newer= option\n");
	error("\t-nodump\t\tdo not dump files that have the nodump flag set\n");
	error("\t-acl\t\thandle access control lists\n");
	error("\t-xfflags\t\thandle extended file flags\n");
	error("\tbs=#\t\tset (output) block size to #\n");
#ifdef	FIFO
	error("\tfs=#\t\tset fifo size to #\n");
#endif
	error("\ttsize=#\t\tset tape volume size to # (default multiplier is 512)\n");
	error("\t-qic24\t\tset tape volume size to %d kBytes\n",
						TSIZE(QIC_24_TSIZE)/1024);
	error("\t-qic120\t\tset tape volume size to %d kBytes\n",
						TSIZE(QIC_120_TSIZE)/1024);
	error("\t-qic150\t\tset tape volume size to %d kBytes\n",
						TSIZE(QIC_150_TSIZE)/1024);
	error("\t-qic250\t\tset tape volume size to %d kBytes\n",
						TSIZE(QIC_250_TSIZE)/1024);
	error("\t-nowarn\t\tdo not print warning messages\n");
	error("\t-time\t\tprint timing info\n");
	error("\t-no-statistics\tdo not print statistics\n");
#ifdef	FIFO
	error("\t-fifostats\tprint fifo statistics\n");
#endif
	error("\t-numeric\tdon't use user/group name from tape\n");
	error("\t-newest\t\tfind newest file on tape\n");
	error("\t-newest-file\tfind newest regular file on tape\n");
	error("\t-hpdev\t\tuse HP's non POSIX compliant method to store dev numbers\n");
	error("\t-modebits\tinclude all 16 bits from stat.st_mode, this violates POSIX-1003.1\n");
	error("\t-copylinks\tCopy hard and symlinks rather than linking\n");
	error("\t-hardlinks\tExtract symlinks as hardlinks\n");
	error("\t-symlinks\tExtract hardlinks as symlinks\n");
	error("\t-signed-checksum\tuse signed chars to calculate checksum\n");
	error("\t-sparse\t\thandle file with holes effectively on store/create\n");
	error("\t-force-hole\ttry to extract all files with holes\n");
	error("\t-to-stdout\textract files to stdout\n");
	error("\t-wready\t\twait for tape drive to become ready\n");
	error("\t-force-remove\tforce to remove non writable files on extraction\n");
	error("\t-ask-remove\task to remove non writable files on extraction\n");
	error("\t-remove-first\tremove files before extraction\n");
	error("\t-remove-recursive\tremove files recursive\n");
	error("\t-onull,-nullout\tsimulate creating an achive to compute the size\n");
	exit(ret);
	/* NOTREACHED */
}

LOCAL void
dusage(ret)
	int	ret;
{
	error("Diff options:\n");
	error("\tnot\t\tif this option is present, exclude listed options\n");
	error("\tperm\t\tcompare file permissions\n");
	error("\tmode\t\tcompare file permissions\n");
	error("\ttype\t\tcompare file type\n");
	error("\tnlink\t\tcompare linkcount (not supported)\n");
	error("\tuid\t\tcompare owner of file\n");
	error("\tgid\t\tcompare group of file\n");
	error("\tuname\t\tcompare name of owner of file\n");
	error("\tgname\t\tcompare name of group of file\n");
	error("\tid\t\tcompare owner, group, ownername and groupname of file\n");
	error("\tsize\t\tcompare file size\n");
	error("\tdata\t\tcompare content of file\n");
	error("\tcont\t\tcompare content of file\n");
	error("\trdev\t\tcompare rdev of device node\n");
	error("\thardlink\tcompare target of hardlink\n");
	error("\tsymlink\t\tcompare target of symlink\n");
	error("\tatime\t\tcompare access time of file (only star)\n");
	error("\tmtime\t\tcompare modification time of file\n");
	error("\tctime\t\tcompare creation time of file (only star)\n");
	error("\ttimes\t\tcompare all times of file\n");
	exit(ret);
	/* NOTREACHED */
}

LOCAL void
husage(ret)
	int	ret;
{
	error("Header types:\n");
	error("\ttar\t\told tar format\n");
	error("\tstar\t\told star format from 1985\n");
	error("\tgnutar\t\tgnu tar format (violates POSIX, use with care)\n");
	error("\tustar\t\tstandard tar (ieee POSIX 1003.1-1988) format\n");
	error("\txstar\t\textended standard tar format (star 1994)\n");
	error("\txustar\t\textended standard tar format without tar signature\n");
	error("\texustar\t\textended standard tar format without tar signature (always x-header)\n");
	error("\tpax\t\textended (ieee POSIX 1003.1-2001) standard tar format\n");
	error("\tsuntar\t\tSun's extended pre-POSIX.1-2001 Solaris 7/8 tar format\n");
	exit(ret);
	/* NOTREACHED */
}

LOCAL void
gargs(ac, av)
	int		ac;
	char	*const *av;
{
	BOOL	help	= FALSE;
	BOOL	xhelp	= FALSE;
	BOOL	prvers	= FALSE;
	BOOL	oldtar	= FALSE;
	BOOL	no_fifo	= FALSE;
	BOOL	usetape	= FALSE;
	BOOL	xlinkerr= FALSE;
	BOOL	dodesc	= FALSE;
	BOOL	qic24	= FALSE;
	BOOL	qic120	= FALSE;
	BOOL	qic150	= FALSE;
	BOOL	qic250	= FALSE;
	const	char	*p;
	Llong	llbs	= 0;

/*char	*opts = "C*,help,xhelp,version,debug,time,no_statistics,no-statistics,fifostats,numeric,v+,block-number,tpath,c,u,r,x,t,n,diff,diffopts&,H&,force_hole,force-hole,sparse,to_stdout,to-stdout,wready,force_remove,force-remove,ask_remove,ask-remove,remove_first,remove-first,remove_recursive,remove-recursive,nullout,onull,fifo,no_fifo,no-fifo,shm,fs&,VOLHDR*,list*,new-volume-script*,file&,f&,T,z,bz,bs&,blocks&,b&,B,pattern&,pat&,i,d,m,o,nochown,a,atime,p,dirmode,l,h,L,D,dodesc,M,I,w,O,signed_checksum,signed-checksum,P,S,F+,U,xdir,k,keep_old_files,keep-old-files,refresh_old_files,refresh-old-files,refresh,/,not,V,maxsize&,newer*,ctime,nodump,tsize&,qic24,qic120,qic150,qic250,nowarn,newest_file,newest-file,newest,hpdev,modebits,copylinks,hardlinks,symlinks,acl,xfflags,link-dirs,dump,meta,silent";*/

	p = filename(av[0]);
	if (streql(p, "ustar")) {
		/*
		 * If we are called as "ustar" we are as POSIX-1003.1-1988
		 * compliant as possible. There are no enhancements at all.
		 */
		hdrtype = H_USTAR;
	} else if (streql(p, "tar")) {
		/*
		 * If we are called as "tar" we are mostly POSIX compliant
		 * and use POSIX-1003.1-2001 extensions. The differences of the
		 * base format compared to POSIX-1003.1-1988 can only be
		 * regocnised by star. Even the checsum bug of the "pax"
		 * reference implementation is not hit by the fingerprint
		 * used to allow star to discriminate XUSTAR from USTAR.
		 */
		hdrtype = H_XUSTAR;
	}
	/*
	 * Current default archive format in all other cases is XSTAR (see
	 * above). This will not change until 2004 (then the new XUSTAR format
	 * is recognised by star for at least 5 years and we may asume that
	 * all star installations will properly handle it.
	 * XSTAR is USTAR with extensions similar to GNU tar.
	 */

	--ac,++av;
	if (getallargs(&ac, &av, opts,
				&dir_flags,
				&help, &xhelp, &prvers, &debug,
				&showtime, &no_stats, &no_stats, &do_fifostats,
				&numeric, &verbose, &prblockno, &tpath,
#ifndef	lint
				&cflag,
				&uflag,
				&rflag,
				&xflag,
				&tflag,
				&nflag,
				&diff_flag, add_diffopt, &diffopts,
				gethdr, &chdrtype,
				&force_hole, &force_hole, &sparse, &to_stdout, &to_stdout, &wready,
				&force_remove, &force_remove, &ask_remove, &ask_remove,
				&remove_first, &remove_first, &remove_recursive, &remove_recursive,
				&nullout, &nullout,
				&use_fifo, &no_fifo, &no_fifo, &shmflag,
				getnum, &fs,
				&volhdr,
				&listfile,
				&newvol_script,
				addtarfile, NULL,
				addtarfile, NULL,
				&usetape,
				&zflag, &bzflag,
				getnum, &bs,
				getbnum, &llbs,
				getbnum, &llbs,
				&multblk,
				addpattern, NULL,
				addpattern, NULL,
				&ignoreerr,
				&nodir,
				&nomtime, &nochown, &nochown,
				&acctime, &acctime,
				&pflag, &dirmode,
				&xlinkerr,
				&follow, &follow,
				&nodesc,
				&dodesc,
				&nomount,
				&interactive, &interactive,
				&oldtar, &signedcksum, &signedcksum,
				&partial,
				&nospec, &Fflag,
				&uncond, &xdir,
				&keep_old, &keep_old, &keep_old,
				&refresh_old, &refresh_old, &refresh_old,
				&abs_path,
				&notpat, &notpat,
				getknum, &maxsize,
				&stampfile,
				&Ctime,
				&nodump,
				getbnum, &tsize,
				&qic24,
				&qic120,
				&qic150,
				&qic250,
				&nowarn,
#endif /* lint */
				&listnewf, &listnewf,
				&listnew,
				&hpdev, &modebits,
				&copylinks, &hardlinks, &symlinks,
				&doacl, &dofflags,
				&link_dirs,
				&dodump,		/* Experimental */
				&dometa,		/* Experimental */
				&silent) < 0) {
		errmsgno(EX_BAD, "Bad Option: %s.\n", av[0]);
		susage(EX_BAD);
	}
	if (help)
		usage(0);
	if (xhelp)
		xusage(0);
	if (prvers) {
		printf("star %s\n\n", strvers);
		printf("Copyright (C) 1985, 88-90, 92-96, 98, 99, 2000-2002 Jörg Schilling\n");
		printf("This is free software; see the source for copying conditions.  There is NO\n");
		printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
		exit(0);
	}

	if ((xflag + cflag + uflag + rflag + tflag + nflag + diff_flag) > 1) {
		errmsgno(EX_BAD, "Too many commaands, only one of -x -c -u -r -t -n or -diff is allowed.\n");
		susage(EX_BAD);
	}
	if (!(xflag | cflag | uflag | rflag | tflag | nflag | diff_flag)) {
		errmsgno(EX_BAD, "Missing command, must specify -x -c -u -r -t -n or -diff.\n");
		susage(EX_BAD);
	}
	if (uflag || rflag) {
		cflag = TRUE;
		no_fifo = TRUE;	/* Until we are able to reverse the FIFO */
	}
	if (nullout && !cflag) {
		errmsgno(EX_BAD, "-nullout only makes sense in create mode.\n");
		susage(EX_BAD);
	}
	if (no_fifo || nullout)
		use_fifo = FALSE;
#ifndef	FIFO
	if (use_fifo) {
		errmsgno(EX_BAD, "Fifo not configured in.\n");
		susage(EX_BAD);
	}
#endif

/*#define	TAR_COMPAT*/
#ifdef	TAR_COMPAT
	nolinkerr = xlinkerr ^ tcompat;
#else
	nolinkerr = xlinkerr;
#endif

	if (dodump)
		chdrtype = H_EXUSTAR;
	if (oldtar)
		chdrtype = H_OTAR;
	if (chdrtype != H_UNDEF) {
		if (H_TYPE(chdrtype) == H_OTAR)
			oldtar = TRUE;	/* XXX hack */
	}
	if (cflag) {
		if (chdrtype != H_UNDEF)
			hdrtype = chdrtype;
		chdrtype = hdrtype;	/* wegen setprops in main() */

		/*
		 * hdrtype und chdrtype
		 * bei uflag, rflag sowie xflag, tflag, nflag, diff_flag
		 * in get_tcb vergleichen !
		 */
	}
	if (diff_flag) {
		if (diffopts == 0)
			diffopts = D_DEFLT;
	} else if (diffopts != 0) {
		errmsgno(EX_BAD, "diffopts= only makes sense with -diff\n");
		susage(EX_BAD);
	}
	if (fs == 0L) {
		char	*ep = getenv("STAR_FIFO_SIZE");

		if (ep) {
			if (getnum(ep, &fs) != 1) {
				comerrno(EX_BAD,
					"Bad fifo size environment '%s'.\n",
									ep);
			}
		}
	}
	if (llbs != 0 && bs != 0) {
		errmsgno(EX_BAD, "Only one of blocks= b= bs=.\n");
		susage(EX_BAD);
	}
	if (llbs != 0) {
		bs = llbs;
		if (bs != llbs) {
			errmsgno(EX_BAD, "Blocksize used with blocks= or b= too large.\n");
			susage(EX_BAD);
		}		
	}
	if (bs % TBLOCK) {
		errmsgno(EX_BAD, "Invalid block size %ld.\n", bs);
		susage(EX_BAD);
	}
	if (bs)
		nblocks = bs / TBLOCK;
	if (nblocks <= 0) {
		errmsgno(EX_BAD, "Invalid block size %d blocks.\n", nblocks);
		susage(EX_BAD);
	}
	bs = nblocks * TBLOCK;
	if (debug) {
		errmsgno(EX_BAD, "Block size %d blocks (%ld bytes).\n", nblocks, bs);
	}
	if (tsize > 0) {
		if (tsize % TBLOCK) {
			errmsgno(EX_BAD, "Invalid tape size %llu.\n", tsize);
			susage(EX_BAD);
		}
		tsize /= TBLOCK;
	}
	if (tsize > 0 && tsize < 3) {
		errmsgno(EX_BAD, "Tape size must be at least 3 blocks.\n");
		susage(EX_BAD);
	}
	if (tsize == 0) {
		if (qic24)  tsize = QIC_24_TSIZE;
		if (qic120) tsize = QIC_120_TSIZE;
		if (qic150) tsize = QIC_150_TSIZE;
		if (qic250) tsize = QIC_250_TSIZE;
	}
	if (listfile != NULL && !dodesc)
		nodesc = TRUE;
	if (oldtar)
		nospec = TRUE;
	if (!tarfiles[0]) {
		if (usetape) {
			tarfiles[0] = getenv("TAPE");
		}
		if (!tarfiles[0])
			tarfiles[0] = "-";
		ntarfiles++;
	}
	if (interactive || ask_remove || tsize > 0) {
#ifdef	JOS
		tty = stderr;
#else
		if ((tty = fileopen("/dev/tty", "r")) == (FILE *)NULL)
			comerr("Cannot open '/dev/tty'.\n");
#endif
	}
	if (nflag) {
		xflag = TRUE;
		interactive = TRUE;
		if (verbose == 0)
			verbose = 1;
	}
	if (to_stdout) {
		force_hole = FALSE;
	}
	if (keep_old && refresh_old) {
		errmsgno(EX_BAD, "Cannot use -keep-old-files and -refresh-old-files together.\n");
		susage(EX_BAD);
	}
	if ((copylinks + hardlinks + symlinks) > 1) {
		errmsgno(EX_BAD, "Only one of -copylinks -hardlinks -symlinks.\n");
		susage(EX_BAD);
	}

	/*
	 * keep compatibility for some time.
	 */
	if (pflag)
		dirmode = TRUE;

	/*
	 * -acl includes -p
	 */
	if (doacl)
		pflag = TRUE;
}

LOCAL Llong
number(arg, retp)
	register char	*arg;
		int	*retp;
{
	Llong	val	= (Llong)0;

	if (*retp != 1)
		return (val);
	if (*arg == '\0') {
		*retp = -1;
	} else if (*(arg = astoll(arg, &val))) {
		if (*arg == 'p' || *arg == 'P') {
			val *= (1024*1024);
			val *= (1024*1024*1024);
			arg++;

		} else if (*arg == 't' || *arg == 'T') {
			val *= (1024*1024);
			val *= (1024*1024);
			arg++;

		} else if (*arg == 'g' || *arg == 'G') {
			val *= (1024*1024*1024);
			arg++;

		} else if (*arg == 'm' || *arg == 'M') {
			val *= (1024*1024);
			arg++;

		} else if (*arg == 'k' || *arg == 'K') {
			val *= 1024;
			arg++;

		} else if (*arg == 'b' || *arg == 'B') {
			val *= 512;
			arg++;

		} else if (*arg == 'w' || *arg == 'W') {
			val *= 2;
			arg++;
		} else if (*arg == '.') {	/* 1x multiplier */
			arg++;
		}
		if (*arg == '*' || *arg == 'x')
			val *= number(++arg, retp);
		else if (*arg != '\0')
			*retp = -1;
	}
	return (val);
}

LOCAL int
getnum(arg, valp)
	char	*arg;
	long	*valp;
{
	Llong	llval;
	int	ret = 1;

	llval = number(arg, &ret);
	*valp = llval;
	if (*valp != llval) {
		errmsgno(EX_BAD,
			"Value %lld is too large for data type 'long'.\n",
									llval);
		ret = -1;
	}
	return (ret);
}

/*
 * not yet needed
LOCAL int
getllnum(arg, valp)
	char	*arg;
	Llong	*valp;
{
	int	ret = 1;

	*valp = number(arg, &ret);
	return (ret);
}
*/

LOCAL int
getlldefault(arg, valp, mult)
	char	*arg;
	Llong	*valp;
	int	mult;
{
	int	ret = 1;
	int	len = strlen(arg);

	if (len > 0) {
		len = (Uchar)arg[len-1];
		if (!isdigit(len))
			mult = 1;
	}
	*valp = number(arg, &ret);
	if (ret == 1)
		*valp *= mult;
	return (ret);
}

LOCAL int
getbnum(arg, valp)
	char	*arg;
	Llong	*valp;
{
	return (getlldefault(arg, valp, 512));
}

LOCAL int
getknum(arg, valp)
	char	*arg;
	Llong	*valp;
{
	return (getlldefault(arg, valp, 1024));
}

LOCAL int
addtarfile(tarfile)
	const char	*tarfile;
{
/*	if (debug)*/
/*		error("Add tar file '%s'.\n", tarfile);*/

	if (ntarfiles >= NTARFILE)
		comerrno(EX_BAD, "Too many tar files (max is %d).\n", NTARFILE);

	if (streql(tarfile, "-") || (ntarfiles > 0 && streql(tarfiles[0], "-")))
		comerrno(EX_BAD, "Cannot handle multi volume archives from/to stdin/stdout.\n");

	tarfiles[ntarfiles] = tarfile;
	ntarfiles++;
	return (TRUE);
}

EXPORT const char *
filename(name)
	const char	*name;
{
	char	*p;

	if ((p = strrchr(name, '/')) == NULL)
		return (name);
	return (++p);
}

LOCAL BOOL
nameprefix(patp, name)
	register const char	*patp;
	register const char	*name;
{
	while (*patp) {
		if (*patp++ != *name++)
			return (FALSE);
	}
	if (*name) {
		return (*name == '/');	/* Directory tree match	*/
	}
	return (TRUE);			/* Names are equal	*/
}

LOCAL int
namefound(name)
	const	char	*name;
{
	register int	i;

	for (i=npat; i < narg; i++) {
		if (nameprefix((const char *)pat[i], name)) {
			return (i);
		}
	}
	return (-1);
}

EXPORT BOOL
match(name)
	const	char	*name;
{
	register int	i;
		char	*ret = NULL;

	if (!cflag && narg > 0) {
		if ((i = namefound(name)) < 0)
			return (FALSE);
		if (npat == 0)
			goto found;
	}

	for (i=0; i < npat; i++) {
		ret = (char *)patmatch(pat[i], aux[i],
					(const Uchar *)name, 0,
					strlen(name), alt[i], state);
		if (ret != NULL && *ret == '\0')
			break;
	}
	if (notpat ^ (ret != NULL && *ret == '\0')) {
found:
		if (!(xflag || diff_flag))	/* Chdir only on -x or -diff */
			return (TRUE);
		if (dirs[i] != NULL && currdir != dirs[i]) {
			currdir = dirs[i];
			dochdir(wdir, TRUE);
			dochdir(currdir, TRUE);
		}
		return TRUE;
	}
	return FALSE;
}

LOCAL int
addpattern(pattern)
	const char	*pattern;
{
	int	plen;

/*	if (debug)*/
/*		error("Add pattern '%s'.\n", pattern);*/

	if (npat >= NPAT)
		comerrno(EX_BAD, "Too many patterns (max is %d).\n", NPAT);
	plen = strlen(pattern);
	pat[npat] = (const Uchar *)pattern;

	if (plen > maxplen)
		maxplen = plen;

	aux[npat] = __malloc(plen*sizeof(int), "compiled pattern");
	if ((alt[npat] = patcompile((const Uchar *)pattern,
							plen, aux[npat])) == 0)
		comerrno(EX_BAD, "Bad pattern: '%s'.\n", pattern);
	dirs[npat] = currdir;
	npat++;
	return (TRUE);
}

LOCAL int
addarg(pattern)
	const char	*pattern;
{
	if (narg == 0)
		narg = npat;

/*	if (debug)*/
/*		error("Add arg '%s'.\n", pattern);*/

	if (narg >= NPAT)
		comerrno(EX_BAD, "Too many patterns (max is %d).\n", NPAT);

	pat[narg] = (const Uchar *)pattern;
	dirs[narg] = currdir;
	narg++;
	return (TRUE);
}

/*
 * Close pattern list: insert useful default directories.
 */
LOCAL void
closepattern()
{
	register int	i;

	if (debug) /* temporary */
		printpattern();

	for (i=0; i < npat; i++) {
		if (dirs[i] != NULL)
			break;
	}
	while (--i >= 0)
		dirs[i] = wdir;

	if (debug) /* temporary */
		printpattern();

	if (npat > 0 || narg > 0)
		havepat = TRUE;

	if (npat > 0) {
		state = __malloc((maxplen+1)*sizeof(int), "pattern state");
	}
}

LOCAL void
printpattern()
{
	register int	i;

	error("npat: %d narg: %d\n", npat, narg);
	for (i=0; i < npat; i++) {
		error("pat %s dir %s\n", pat[i], dirs[i]);
	}
	for (i=npat; i < narg; i++) {
		error("arg %s dir %s\n", pat[i], dirs[i]);
	}
}

LOCAL int
add_diffopt(optstr, flagp)
	char	*optstr;
	long	*flagp;
{
	char	*ep;
	char	*np;
	int	optlen;
	long	optflags = 0;
	BOOL	not = FALSE;

	while (*optstr) {
		if ((ep = strchr(optstr, ',')) != NULL) {
			optlen = ep - optstr;
			np = &ep[1];
		} else {
			optlen = strlen(optstr);
			np = &optstr[optlen];
		}
		if (optstr[0] == '!') {
			optstr++;
			optlen--;
			not = TRUE;
		}
		if (strncmp(optstr, "not", optlen) == 0 ||
				strncmp(optstr, "!", optlen) == 0) {
			not = TRUE;
		} else if (strncmp(optstr, "all", optlen) == 0) {
			optflags |= D_ALL;
		} else if (strncmp(optstr, "perm", optlen) == 0) {
			optflags |= D_PERM;
		} else if (strncmp(optstr, "mode", optlen) == 0) {
			optflags |= D_PERM;
		} else if (strncmp(optstr, "type", optlen) == 0) {
			optflags |= D_TYPE;
		} else if (strncmp(optstr, "nlink", optlen) == 0) {
			optflags |= D_NLINK;
			errmsgno(EX_BAD, "nlink not supported\n");
			dusage(EX_BAD);
		} else if (strncmp(optstr, "uid", optlen) == 0) {
			optflags |= D_UID;
		} else if (strncmp(optstr, "gid", optlen) == 0) {
			optflags |= D_GID;
		} else if (strncmp(optstr, "uname", optlen) == 0) {
			optflags |= D_UNAME;
		} else if (strncmp(optstr, "gname", optlen) == 0) {
			optflags |= D_GNAME;
		} else if (strncmp(optstr, "id", optlen) == 0) {
			optflags |= D_ID;
		} else if (strncmp(optstr, "size", optlen) == 0) {
			optflags |= D_SIZE;
		} else if (strncmp(optstr, "data", optlen) == 0) {
			optflags |= D_DATA;
		} else if (strncmp(optstr, "cont", optlen) == 0) {
			optflags |= D_DATA;
		} else if (strncmp(optstr, "rdev", optlen) == 0) {
			optflags |= D_RDEV;
		} else if (strncmp(optstr, "hardlink", optlen) == 0) {
			optflags |= D_HLINK;
		} else if (strncmp(optstr, "symlink", optlen) == 0) {
			optflags |= D_SLINK;
		} else if (strncmp(optstr, "sparse", optlen) == 0) {
			optflags |= D_SPARS;
		} else if (strncmp(optstr, "atime", optlen) == 0) {
			optflags |= D_ATIME;
		} else if (strncmp(optstr, "mtime", optlen) == 0) {
			optflags |= D_MTIME;
		} else if (strncmp(optstr, "ctime", optlen) == 0) {
			optflags |= D_CTIME;
		} else if (strncmp(optstr, "times", optlen) == 0) {
			optflags |= D_TIMES;
		} else if (strncmp(optstr, "help", optlen) == 0) {
			dusage(0);
		} else {
			error("Illegal diffopt.\n");
			dusage(EX_BAD);
			return (-1);
		}
		optstr = np;
	}
	if (not) {
		*flagp = ~optflags;
	} else {
		*flagp = optflags;
	}
	return (TRUE);
}

LOCAL int
gethdr(optstr, typep)
	char	*optstr;
	long	*typep;
{
	BOOL	swapped = FALSE;
	long	type	= H_UNDEF;

	if (*optstr == 'S') {
		swapped = TRUE;
		optstr++;
	}
	if (streql(optstr, "tar")) {
		type = H_OTAR;
	} else if (streql(optstr, "star")) {
		type = H_STAR;
	} else if (streql(optstr, "gnutar")) {
		type = H_GNUTAR;
	} else if (streql(optstr, "ustar")) {
		type = H_USTAR;
	} else if (streql(optstr, "xstar")) {
		type = H_XSTAR;
	} else if (streql(optstr, "xustar")) {
		type = H_XUSTAR;
	} else if (streql(optstr, "exustar")) {
		type = H_EXUSTAR;
	} else if (streql(optstr, "pax")) {
		type = H_PAX;
	} else if (streql(optstr, "suntar")) {
		type = H_SUNTAR;
	} else if (streql(optstr, "help")) {
		husage(0);
	} else {
		error("Illegal header type '%s'.\n", optstr);
		husage(EX_BAD);
		return (-1);
	}
	if (swapped)
		*typep = H_SWAPPED(type);
	else
		*typep = type;
	return (TRUE);
}

#ifdef	USED
/*
 * Add archive file.
 * May currently not be activated:
 *	If the option string ends with ",&", the -C option will not work
 *	anymore.
 */
LOCAL int
addfile(optstr, dummy)
	char	*optstr;
	long	*dummy;
{
	char	*p;

/*	error("got_it: %s\n", optstr);*/

	if (!strchr("01234567", optstr[0]))
		return (NOTAFILE);/* Tell getargs that this may be a flag */

	for (p = &optstr[1]; *p; p++) {
		if (*p != 'l' && *p != 'm' && *p != 'h')
			return (BADFLAG);
	}
/*	error("is_tape: %s\n", optstr);*/

	comerrno(EX_BAD, "Options [0-7][lmh] currently not supported.\n");
	/*
	 * The tape device should be determined from the defaults file
	 * in the near future.
	 * Search for /etc/opt/schily/star, /etc/default/star, /etc/default/tar
	 */

	return (1);		/* Success */
}
#endif

LOCAL void
exsig(sig)
	int	sig;
{
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

/* ARGSUSED */
LOCAL void
sighup(sig)
	int	sig;
{
	signal(SIGHUP, sighup);
	prstats();
	intr++;
	if (!cflag)
		exsig(sig);
}

/* ARGSUSED */
LOCAL void
sigintr(sig)
	int	sig;
{
	signal(SIGINT, sigintr);
	prstats();
	intr++;
	if (!cflag)
		exsig(sig);
}

/* ARGSUSED */
LOCAL void
sigquit(sig)
	int	sig;
{
	signal(SIGQUIT, sigquit);
	prstats();
}

LOCAL void
getstamp()
{
	FINFO	finfo;
	BOOL	ofollow = follow;

	follow = TRUE;
	if (!getinfo(stampfile, &finfo))
		comerr("Cannot stat '%s'.\n", stampfile);
	follow = ofollow;

	Newer = finfo.f_mtime;
}

EXPORT void *
__malloc(size, msg)
	size_t	size;
	char	*msg;
{
	void	*ret;

	ret = malloc(size);
	if (ret == NULL) {
		comerr("Cannot allocate memory for %s.\n", msg);
		/* NOTREACHED */
	}
	return (ret);
}

EXPORT void *
__realloc(ptr, size, msg)
	void	*ptr;
	size_t	size;
	char	*msg;
{
	void	*ret;

	ret = realloc(ptr, size);
	if (ret == NULL) {
		comerr("Cannot realloc memory for %s.\n", msg);
		/* NOTREACHED */
	}
	return (ret);
}

EXPORT char *
__savestr(s)
	char	*s;
{
	char	*ret = __malloc(strlen(s)+1, "saved string");

	strcpy(ret, s);
	return (ret);
}

/*
 * Convert old tar type syntax into the new UNIX option syntax.
 * Allow only a limited subset of the single character options to avoid
 * collisions between interpretation of options in different
 * tar implementations. The old syntax has a risk to damage files
 * which is avoided with the 'fcompat' flag (see opentape()).
 *
 * The UNIX-98 documentation lists the following tar options:
 *	Function Key:	crtux
 *			c	Create
 *			r	Append
 *			t	List
 *			u	Update
 *			x	Extract
 *	Additional Key:	vwfblmo
 *			v	Verbose
 *			w	Wait for confirmation
 *			f	Archive file
 *			b	Blocking factor
 *			l	Report missing links
 *			m	Do not restore mtime from archive
 *			o	Do not restore owner/group from archive
 *
 *	Incompatibilities with UNIX-98 tar:
 *			l	works the oposite way round as with star, but
 *				if TAR_COMPAT is defined, star will behaves
 * 				as documented in UNIX-98 if av[0] is either
 *				"tar" or "ustar".
 *
 *	Additional functions from historic UNIX tar vesions:
 *			0..7	magtape_0 .. magtape_7
 *
 *	Additional functions from historic BSD tar vesions:
 *			p	Extract dir permissions too
 *			h	Follow symbolic links
 *			i	ignore directory checksum errors
 *			B	do multiple reads to reblock pipes
 *			F	Ommit unwanted files (e.g. core)
 *			
 *	Additional functions from historic Star vesions:
 *			T	Use $TAPE environment as archive
 *			L	Follow symbolic links
 *			d	do not archive/extract directories
 *			k	keep old files
 *			n	do not extract but tell what to do
 *
 *	Additional functions from historic SVr4 tar vesions:
 *			e	Exit on unexpected errors
 *			X	Arg is File with unwanted filenames
 *
 *	Additional functions from historic GNU tar vesions:
 *			z	do inline compression
 *
 *	Missing in star (from SVr4/Solaris tar):
 *			E	Extended headers
 *			P	Supress '/' at beginning of filenames
 *			q	quit after extracting first file
 *	Incompatibilities with SVr4/Solaris tar:
 *			I	Arg is File with filenames to be included
 *			P	SVr4/solaris: Supress '/', star: last partial
 *			k	set tape size for multi volume archives
 *			n	non tape device (do seeks)
 *
 *	Incompatibilities with GNU tar:
 *		There are many. GNU programs in many cases make smooth
 *		coexistence hard.
 *
 * Problems:
 *	The 'e' and 'X' option are currently not implemented.
 *	The 'h' option can only be implemented if the -help XXX DONE
 *	shortcut in star is removed.
 *	There is a collision between the BSD -I (include) and
 *	star's -I (interactive) which may be solved by using -w instead.
 */
LOCAL void
docompat(pac, pav)
	int	*pac;
	char	*const **pav;
{
	int	ac		= *pac;
	char	*const *av	= *pav;
	int	nac;
	char	**nav;
	char	nopt[3];
	char	*copt = "crtuxbfXBFTLdehiklmnopvwz01234567";
const	char	*p;
	char	c;
	char	*const *oa;
	char	**na;

	p = filename(av[0]);
	if (streql(p, "tar") || streql(p, "ustar"))
		tcompat = TRUE;

	if (ac <= 1)
		return;

	if (av[1][0] == '-')			/* Do not convert new syntax */
		return;

	if (strchr(av[1], '=') != NULL)		/* Do not try to convert bs= */
		return;

	nac = ac + strlen(av[1]);
	nav = __malloc(nac-- * sizeof(char *),	/* keep space for NULL ptr */
				"compat argv");
	oa = av;
	na = nav;
	*na++ = *oa++;
	oa++;					/* Skip over av[1] */

	nopt[0] = '-';
	nopt[2] = '\0';

	for (p=av[1]; (c = *p) != '\0'; p++) {
		if (strchr(copt, c) == NULL) {
			errmsgno(EX_BAD, "Illegal option '%c' for compat mode.\n", c);
			susage(EX_BAD);
		}
		nopt[1] = c;
		*na++ = __savestr(nopt);
		if (c == 'f' || c == 'b' || c == 'X') {
			*na++ = *oa++;
			/*
			 * The old syntax has a high risk of corrupting
			 * files if the user disorders the args.
			 */
			if (c == 'f')
				fcompat = TRUE;
		}
	}

	/*
	 * Now copy over the rest...
	 */
	while ((av + ac) > oa)
		*na++ = *oa++;
	*na = NULL;

	*pac = nac;
	*pav = nav;
}
