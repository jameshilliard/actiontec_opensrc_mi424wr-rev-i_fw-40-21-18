/* @(#)header.c	1.67 02/05/10 Copyright 1985, 1995 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)header.c	1.67 02/05/10 Copyright 1985, 1995 J. Schilling";
#endif
/*
 *	Handling routines to read/write, parse/create
 *	archive headers
 *
 *	Copyright (c) 1985, 1995 J. Schilling
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

	/* ustar */
LOCAL	char	magic[TMAGLEN] = TMAGIC;
	/* star */
LOCAL	char	stmagic[STMAGLEN] = STMAGIC;
	/* gnu tar */
LOCAL	char	gmagic[GMAGLEN] = GMAGIC;

LOCAL	char	*hdrtxt[] = {
	/* 0 */	"UNDEFINED",
	/* 1 */	"unknown tar",
	/* 2 */	"old tar",
	/* 3 */	"star",
	/* 4 */	"gnu tar",
	/* 5 */	"ustar",
	/* 6 */	"xstar",
	/* 7 */	"xustar",
	/* 8 */	"exustar",
	/* 9 */	"pax",		/* USTAR POSIX.1-2001 */
	/*10 */	"suntar",
	/*11 */	"res11",	/* Reserved */
	/*12 */	"res12",	/* Reserved */
	/*13 */	"res13",	/* Reserved */
	/*14 */	"res14",	/* Reserved */
	/*15 */	"bar",
	/*16 */	"cpio binary",
	/*17 */	"cpio -c",
	/*18 */	"cpio",
	/*19 */	"cpio crc",
	/*20 */	"cpio ascii",
	/*21 */	"cpio ascii crc",
};

extern	FILE	*tty;
extern	FILE	*vpr;
extern	long	hdrtype;
extern	long	chdrtype;
extern	int	version;
extern	int	swapflg;
extern	BOOL	debug;
extern	BOOL	numeric;
extern	int	verbose;
extern	BOOL	xflag;
extern	BOOL	nflag;
extern	BOOL	ignoreerr;
extern	BOOL	signedcksum;
extern	BOOL	nowarn;
extern	BOOL	nullout;
extern	BOOL	modebits;

extern	Ullong	tsize;

extern	char	*bigbuf;
extern	int	bigsize;

LOCAL	Ulong	checksum	__PR((TCB * ptb));
LOCAL	Ulong	bar_checksum	__PR((TCB * ptb));
LOCAL	BOOL	signedtarsum	__PR((TCB *ptb, Ulong ocheck));
LOCAL	BOOL	isstmagic	__PR((char* s));
LOCAL	BOOL	isxmagic	__PR((TCB *ptb));
LOCAL	BOOL	ismagic		__PR((char* s));
LOCAL	BOOL	isgnumagic	__PR((char* s));
LOCAL	BOOL	strxneql	__PR((char* s1, char* s2, int l));
LOCAL	BOOL	ustmagcheck	__PR((TCB * ptb));
LOCAL	void	print_hdrtype	__PR((int type));
EXPORT	int	get_hdrtype	__PR((TCB * ptb, BOOL isrecurse));
EXPORT	int	get_compression	__PR((TCB * ptb));
EXPORT	int	get_tcb		__PR((TCB * ptb));
EXPORT	void	put_tcb		__PR((TCB * ptb, FINFO * info));
EXPORT	void	write_tcb	__PR((TCB * ptb, FINFO * info));
EXPORT	void	put_volhdr	__PR((char* name));
EXPORT	BOOL	get_volhdr	__PR((FINFO * info, char* vhname));
EXPORT	void	info_to_tcb	__PR((FINFO * info, TCB * ptb));
LOCAL	void	info_to_star	__PR((FINFO * info, TCB * ptb));
LOCAL	void	info_to_ustar	__PR((FINFO * info, TCB * ptb));
LOCAL	void	info_to_xstar	__PR((FINFO * info, TCB * ptb));
LOCAL	void	info_to_gnutar	__PR((FINFO * info, TCB * ptb));
EXPORT	int	tcb_to_info	__PR((TCB * ptb, FINFO * info));
LOCAL	void	tar_to_info	__PR((TCB * ptb, FINFO * info));
LOCAL	void	star_to_info	__PR((TCB * ptb, FINFO * info));
LOCAL	void	ustar_to_info	__PR((TCB * ptb, FINFO * info));
LOCAL	void	xstar_to_info	__PR((TCB * ptb, FINFO * info));
LOCAL	void	gnutar_to_info	__PR((TCB * ptb, FINFO * info));
LOCAL	void	cpiotcb_to_info	__PR((TCB * ptb, FINFO * info));
LOCAL	int	ustoxt		__PR((int ustype));
EXPORT	BOOL	ia_change	__PR((TCB * ptb, FINFO * info));
LOCAL	BOOL	checkeof	__PR((TCB * ptb));
LOCAL	BOOL	eofblock	__PR((TCB * ptb));
LOCAL	void	astoo_cpio	__PR((char* s, Ulong * l, int cnt));
LOCAL	void	stoli		__PR((char* s, Ulong * l));
EXPORT	void	stolli		__PR((char* s, Ullong * ull));
LOCAL	void	litos		__PR((char* s, Ulong l, int fieldw));
EXPORT	void	llitos		__PR((char* s, Ullong ull, int fieldw));
LOCAL	void	stob		__PR((char* s, Ulong * l, int fieldw));
LOCAL	void	stollb		__PR((char* s, Ullong * ull, int fieldw));
LOCAL	void	btos		__PR((char* s, Ulong l, int fieldw));
LOCAL	void	llbtos		__PR((char* s, Ullong ull, int fieldw));

/*
 * XXX Hier sollte eine tar/bar universelle Checksummenfunktion sein!
 */
#define	CHECKS	sizeof(ptb->ustar_dbuf.t_chksum)
/*
 * We know, that sizeof(TCP) is 512 and therefore has no
 * reminder when dividing by 8
 *
 * CHECKS is known to be 8 too, use loop unrolling.
 */
#define	DO8(a)	a;a;a;a;a;a;a;a;

LOCAL Ulong
checksum(ptb)
	register	TCB	*ptb;
{
	register	int	i;
	register	Ulong	sum = 0;
	register	Uchar	*us;

	if (signedcksum) {
		register	char	*ss;

		ss = (char *)ptb;
		for (i=sizeof(*ptb)/8; --i >= 0;) {
			DO8(sum += *ss++);
		}
		if (sum == 0L)		/* Block containing 512 nul's */
			return(sum);

		ss=(char *)ptb->ustar_dbuf.t_chksum;
		DO8(sum -= *ss++);
		sum += CHECKS*' ';
	} else {
		us = (Uchar *)ptb;
		for (i=sizeof(*ptb)/8; --i >= 0;) {
			DO8(sum += *us++);
		}
		if (sum == 0L)		/* Block containing 512 nul's */
			return(sum);

		us=(Uchar *)ptb->ustar_dbuf.t_chksum;
		DO8(sum -= *us++);
		sum += CHECKS*' ';
	}
	return sum;
}
#undef	CHECKS

#define	CHECKS	sizeof(ptb->bar_dbuf.t_chksum)

LOCAL Ulong
bar_checksum(ptb)
	register	TCB	*ptb;
{
	register	int	i;
	register	Ulong	sum = 0;
	register	Uchar	*us;

	if (signedcksum) {
		register	char	*ss;

		ss = (char *)ptb;
		for (i=sizeof(*ptb); --i >= 0;)
			sum += *ss++;
		if (sum == 0L)		/* Block containing 512 nul's */
			return(sum);

		for (i=CHECKS, ss=(char *)ptb->bar_dbuf.t_chksum; --i >= 0;)
			sum -= *ss++;
		sum += CHECKS*' ';
	} else {
		us = (Uchar *)ptb;
		for (i=sizeof(*ptb); --i >= 0;)
			sum += *us++;
		if (sum == 0L)		/* Block containing 512 nul's */
			return(sum);

		for (i=CHECKS, us=(Uchar *)ptb->bar_dbuf.t_chksum; --i >= 0;)
			sum -= *us++;
		sum += CHECKS*' ';
	}
	return sum;
}
#undef	CHECKS

LOCAL BOOL
signedtarsum(ptb, ocheck)
	TCB	*ptb;
	Ulong	ocheck;
{
	BOOL	osigned = signedcksum;
	Ulong	check;

	signedcksum = !signedcksum;
	check = checksum(ptb);
	if (ocheck == check) {
		errmsgno(EX_BAD, "WARNING: archive uses %s checksums.\n",
				signedcksum?"signed":"unsigned");
		return (TRUE);
	}
	signedcksum = osigned;
	return (FALSE);
}

LOCAL BOOL
isstmagic(s)
	char	*s;
{
	return (strxneql(s, stmagic, STMAGLEN));
}

/*
 * Check for XUSTAR format.
 *
 * This is star's upcoming new standard format. This format understands star's
 * old extended POSIX format and in future will write POSIX.1-2001 extensions
 * using 'x' headers.
 */
LOCAL BOOL
isxmagic(ptb)
	TCB	*ptb;
{
	register int	i;

	/*
	 * prefix[130] is is granted to be '\0' with 'xstar'.
	 */
	if (ptb->xstar_dbuf.t_prefix[130] != '\0')
		return (FALSE);
	/*
	 * If atime[0]...atime[10] or ctime[0]...ctime[10]
	 * is not a POSIX octal number it cannot be 'xstar'.
	 * With the octal representation we may store any date
	 * for 1970 +- 136 years (1834 ... 2106). After 2106
	 * we will most likely always use POSIX.1-2001 'x'
	 * headers and thus don't need to check for base 256
	 * numbers.
	 */
	for (i = 0; i < 11; i++) {
		if (ptb->xstar_dbuf.t_atime[i] < '0' ||
		    ptb->xstar_dbuf.t_atime[i] > '7')
			return (FALSE);
		if (ptb->xstar_dbuf.t_ctime[i] < '0' ||
		    ptb->xstar_dbuf.t_ctime[i] > '7')
			return (FALSE);
	}

	/*
	 * Check for both POSIX compliant end of number characters.
	 */
	if ((ptb->xstar_dbuf.t_atime[11] != ' ' &&
	     ptb->xstar_dbuf.t_atime[11] != '\0') ||
	    (ptb->xstar_dbuf.t_ctime[11] != ' ' &&
	     ptb->xstar_dbuf.t_ctime[11] != '\0'))
		return (FALSE);

	return (TRUE);
}

LOCAL BOOL
ismagic(s)
	char	*s;
{
	return (strxneql(s, magic, TMAGLEN));
}

LOCAL BOOL
isgnumagic(s)
	char	*s;
{
	return (strxneql(s, gmagic, GMAGLEN));
}

LOCAL BOOL
strxneql(s1, s2, l)
	register char	*s1;
	register char	*s2;
	register int	l;
{
	while (--l >= 0)
		if (*s1++ != *s2++)
			return (FALSE);
	return (TRUE);
}

LOCAL BOOL
ustmagcheck(ptb)
	TCB	*ptb;
{
	if (ismagic(ptb->xstar_dbuf.t_magic) &&
				strxneql(ptb->xstar_dbuf.t_version, "00", 2))
		return (TRUE);
	return (FALSE);
}

LOCAL void
print_hdrtype(type)
	int	type;
{
	BOOL	isswapped = H_ISSWAPPED(type);

	if (H_TYPE(type) > H_MAX_ARCH)
		type = H_UNDEF;
	type = H_TYPE(type);

	error("%s%s archive.\n", isswapped?"swapped ":"", hdrtxt[type]);
}

EXPORT int
get_hdrtype(ptb, isrecurse)
	TCB	*ptb;
	BOOL	isrecurse;
{
	Ulong	check;
	Ulong	ocheck;
	int	ret = H_UNDEF;

	stoli(ptb->dbuf.t_chksum, &ocheck);
	check = checksum(ptb);
	if (ocheck != check && !signedtarsum(ptb, ocheck)) {
		if (debug && !isrecurse) {
			errmsgno(EX_BAD,
				"Bad tar checksum at: %lld: 0%lo should be 0%lo.\n",
							tblocks(),
							ocheck, check);
		}
		goto nottar;
	}

	if (isstmagic(ptb->dbuf.t_magic)) {	/* Check for 'tar\0' at end */
		if (ustmagcheck(ptb))
			ret = H_XSTAR;
		else
			ret = H_STAR;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if (ustmagcheck(ptb)) {			/* 'ustar\000' POSIX magic */
		if (isxmagic(ptb)) {
			if (ptb->ustar_dbuf.t_typeflag == 'g' ||
			    ptb->ustar_dbuf.t_typeflag == 'x')
				ret = H_EXUSTAR;
			else
				ret = H_XUSTAR;
		} else {
			if (ptb->ustar_dbuf.t_typeflag == 'g' ||
			    ptb->ustar_dbuf.t_typeflag == 'x')
				ret = H_PAX;
			else if (ptb->ustar_dbuf.t_typeflag == 'X')
				ret = H_SUNTAR;
			else
				ret = H_USTAR;
		}
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if (isgnumagic(&ptb->dbuf.t_vers)) {	/* 'ustar  ' GNU magic */
		ret = H_GNUTAR;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if ((ptb->dbuf.t_mode[6] == ' ' && ptb->dbuf.t_mode[7] == '\0')) {
		ret = H_OTAR;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if (ptb->ustar_dbuf.t_typeflag == LF_VOLHDR ||
			    ptb->ustar_dbuf.t_typeflag == LF_MULTIVOL) {
		/*
		 * Gnu volume headers & multi volume headers
		 * are no real tar headers.
		 */
		if (debug) error("gnutar buggy archive.\n");
		ret = H_GNUTAR;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	/*
	 * The only thing we know here is:
	 * we found a header with a correct tar checksum.
	 */
	ret = H_TAR;
	if (debug) print_hdrtype(ret);
	return (ret);

nottar:
	if (ptb->bar_dbuf.bar_magic[0] == 'V') {
		stoli(ptb->bar_dbuf.t_chksum, &ocheck);
		check = bar_checksum(ptb);

		if (ocheck == check) {
			ret = H_BAR;
			if (debug) print_hdrtype(ret);
			return (ret);
		} else if (debug && !isrecurse) {
			errmsgno(EX_BAD,
				"Bad bar checksum at: %lld: 0%lo should be 0%lo.\n",
							tblocks(),
							ocheck, check);
		}

	}
	if (strxneql((char *)ptb, "070701", 6)) {
		ret = H_CPIO_ASC;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if (strxneql((char *)ptb, "070702", 6)) {
		ret = H_CPIO_ACRC;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if (strxneql((char *)ptb, "070707", 6)) {
		ret = H_CPIO_CHR;
		if (debug) print_hdrtype(ret);
		return (ret);

	}
	if (strxneql((char *)ptb, "\161\301", 2)) {
		ret = H_CPIO_NBIN;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if (strxneql((char *)ptb, "\161\302", 2)) {
		ret = H_CPIO_CRC;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if (strxneql((char *)ptb, "\161\307", 2)) {
		ret = H_CPIO_BIN;
		if (debug) print_hdrtype(ret);
		return (ret);
	}
	if (debug) error("no tar archive??\n");

	if (!isrecurse) {
		int	rret;
		swabbytes((char *)ptb, TBLOCK);
		rret = get_hdrtype(ptb, TRUE);
		swabbytes((char *)ptb, TBLOCK);
		rret = H_SWAPPED(rret);
		if (debug) print_hdrtype(rret);
		return (rret);
	}

	if (debug) print_hdrtype(ret);
	return (ret);
}

EXPORT int
get_compression(ptb)
	TCB	*ptb;
{
	char	*p = (char *)ptb;

	if (p[0] == '\037') {
		if ((p[1] == '\037') ||	/* Packed	     */
		    (p[1] == '\213') ||	/* Gzip compressed   */
		    (p[1] == '\235') ||	/* LZW compressed    */
		    (p[1] == '\236') ||	/* Freezed 	     */
		    (p[1] == '\240'))	/* SCO LZH compressed*/
		return (C_GZIP);
	}
	if (p[0] == 'B' && p[1] == 'Z' && p[2] == 'h')
		return (C_BZIP2);
	return (C_NONE);
}

EXPORT int
get_tcb(ptb)
	TCB	*ptb;
{
	Ulong	check;
	Ulong	ocheck;
	BOOL	eof;

	do {
		/*
		 * bei der Option -i wird ein genulltes File
		 * fehlerhaft als EOF Block erkannt !
		 * wenn nicht t_magic gesetzt ist.
		 */
		if (readblock((char *)ptb) == EOF) {
			errmsgno(EX_BAD, "Hard EOF on input, first EOF block is missing.\n");
			return (EOF);
		}
		/*
		 * First tar control block
		 */
		if (swapflg < 0) {
			BOOL	swapped;

			hdrtype = get_hdrtype(ptb, FALSE);
			swapped = H_ISSWAPPED(hdrtype);
			if (chdrtype != H_UNDEF &&
					swapped != H_ISSWAPPED(chdrtype)) {

				swapped = H_ISSWAPPED(chdrtype);
			}
			if (swapped) {
				swapflg = 1;
				swabbytes((char *)ptb, TBLOCK);	/* copy of TCB*/
				swabbytes(bigbuf, bigsize);	/* io buffer */
			} else {
				swapflg = 0;
			}
			/*
			 * wake up fifo (first block ist swapped)
			 */
			buf_resume();
			if (H_TYPE(hdrtype) == H_BAR) {
				comerrno(EX_BAD, "Can't handle bar archives (yet).\n");
			}
			if (H_TYPE(hdrtype) >= H_CPIO_BASE) {
/* XXX JS Test */if (H_TYPE(hdrtype) == H_CPIO_CHR) {
/* XXX JS Test */FINFO info;
/* XXX JS Test */tcb_to_info(ptb, &info);
/* XXX JS Test */}
				comerrno(EX_BAD, "Can't handle cpio archives (yet).\n");
			}
			if (H_TYPE(hdrtype) == H_UNDEF) {
				switch (get_compression(ptb)) {

				case C_GZIP:
					comerrno(EX_BAD, "Archive is compressed, try to use the -z option.\n");
					break;
				case C_BZIP2:
					comerrno(EX_BAD, "Archive is bzip2 compressed, try to use the -bz option.\n");
					break;
				}
				if (!ignoreerr) {
					comerrno(EX_BAD,
					"Unknown archive type (neither tar, nor bar/cpio).\n");
				}
			}
			if (chdrtype != H_UNDEF && chdrtype != hdrtype) {
				errmsgno(EX_BAD, "Found: ");
				print_hdrtype(hdrtype);
				errmsgno(EX_BAD, "WARNING: extracting as ");
				print_hdrtype(chdrtype);
				hdrtype = chdrtype;
			}
			setprops(hdrtype);
		}
		eof = (ptb->dbuf.t_name[0] == '\0') && checkeof(ptb);
		if (eof && !ignoreerr) {
			return (EOF);
		}
		/*
		 * XXX Hier muß eine Universalchecksummenüberprüfung hin
		 */
		stoli(ptb->dbuf.t_chksum, &ocheck);
		check = checksum(ptb);
		/*
		 * check == 0 : genullter Block.
		 */
		if (check != 0 && ocheck == check) {
			char	*tmagic = ptb->ustar_dbuf.t_magic;

			switch (H_TYPE(hdrtype)) {

			case H_XUSTAR:
			case H_EXUSTAR:
				if (ismagic(tmagic) && isxmagic(ptb))
					return (0);
				/*
				 * Both formats are equivalent.
				 * Acept XSTAR too.
				 */
				/* FALLTHROUGH */
			case H_XSTAR:
				if (ismagic(tmagic) &&
				    isstmagic(ptb->xstar_dbuf.t_xmagic))
					return (0);
				break;
			case H_PAX:
			case H_USTAR:
			case H_SUNTAR:
				if (ismagic(tmagic))
					return (0);
				break;
			case H_GNUTAR:
				if (isgnumagic(tmagic))
					return (0);
				break;
			case H_STAR: 
				tmagic = ptb->star_dbuf.t_magic;
				if (ptb->dbuf.t_vers < STVERSION ||
				    isstmagic(tmagic))
				return (0);
				break;
			default:
			case H_TAR:
			case H_OTAR:
				return (0);
			}
			errmsgno(EX_BAD, "Wrong magic at: %lld: '%.8s'.\n",
							tblocks(), tmagic);
			/*
			 * Allow buggy gnu Volheaders & Multivolheaders to work
			 */
			if (H_TYPE(hdrtype) == H_GNUTAR)
				return (0);

		} else if (eof) {
			errmsgno(EX_BAD, "EOF Block at: %lld ignored.\n",
							tblocks());
		} else {
			if (signedtarsum(ptb, ocheck))
				return (0);
			errmsgno(EX_BAD, "Checksum error at: %lld: 0%lo should be 0%lo.\n",
							tblocks(),
							ocheck, check);
		}
	} while (ignoreerr);
	exprstats(EX_BAD);
	/* NOTREACHED */
	return (EOF);		/* Keep lint happy */
}

EXPORT void
put_tcb(ptb, info)
	TCB	*ptb;
	register FINFO	*info;
{
	TCB	tb;
	int	x1 = 0;
	int	x2 = 0;

	if (info->f_flags & (F_LONGNAME|F_LONGLINK))
		x1++;
	if (info->f_xflags & (XF_PATH|XF_LINKPATH))
		x1++;

/*XXX start alter code und Test */
	if (( (info->f_flags & F_ADDSLASH) ? 1:0 +
	    info->f_namelen > props.pr_maxsname &&
	    (ptb->dbuf.t_prefix[0] == '\0' || H_TYPE(hdrtype) == H_GNUTAR)) ||
		    info->f_lnamelen > props.pr_maxslname)
		x2++;

	if ((x1 != x2) && info->f_xftype != XT_META) {
error("type: %ld name: '%s' x1 %d x2 %d namelen: %ld prefix: '%s' lnamelen: %ld\n",
info->f_filetype, info->f_name, x1, x2,
info->f_namelen, ptb->dbuf.t_prefix, info->f_lnamelen);
	}
/*XXX ende alter code und Test */

	if (x1 || x2 || (info->f_xflags != 0)) {
		if ((info->f_flags & F_TCB_BUF) != 0) {	/* TCB is on buffer */
			movetcb(ptb, &tb);
			ptb = &tb;
			info->f_flags &= ~F_TCB_BUF;
		}
		if (info->f_xflags != 0)
			info_to_xhdr(info, ptb);
		else
			write_longnames(info);
	}
	write_tcb(ptb, info);
}

EXPORT void
write_tcb(ptb, info)
	TCB	*ptb;
	register FINFO	*info;
{
	if (tsize > 0) {
		TCB	tb;
		Llong	left;
		off_t	size = info->f_rsize;

		left = tsize - tblocks();

		if (is_link(info))
			size = 0L;
						/* file + tcb + EOF */
		if (left < (tarblocks(size)+1+2)) {
			if ((info->f_flags & F_TCB_BUF) != 0) {
				movetcb(ptb, &tb);
				ptb = &tb;
				info->f_flags &= ~F_TCB_BUF;
			}
			nexttape();
		}
	}
	if (!nullout) {				/* 17 (> 16) Bit !!! */
		if (props.pr_fillc == '0')
			litos(ptb->dbuf.t_chksum, checksum(ptb) & 0x1FFFF, 7);
		else
			litos(ptb->dbuf.t_chksum, checksum(ptb) & 0x1FFFF, 6);
	}
	if ((info->f_flags & F_TCB_BUF) != 0)	/* TCB is on buffer */
		put_block();
	else
		writeblock((char *)ptb);
}

EXPORT void
put_volhdr(name)
	char	*name;
{
	FINFO	finfo;
	TCB	tb;
	struct timeval tv;

	if (name == 0)
		return;
	if ((props.pr_flags & PR_VOLHDR) == 0)
		return;

	gettimeofday(&tv, (struct timezone *)0);

	fillbytes((char *)&finfo, sizeof (FINFO), '\0');
	filltcb(&tb);
	finfo.f_name = name;
	finfo.f_namelen = strlen(name);
	finfo.f_xftype = XT_VOLHDR;
	finfo.f_mtime = tv.tv_sec;
	finfo.f_mnsec = tv.tv_usec*1000;
	finfo.f_tcb = &tb;

	if (!name_to_tcb(&finfo, &tb))	/* Name too long */
		return;

	info_to_tcb(&finfo, &tb);
	put_tcb(&tb, &finfo);
	vprint(&finfo);
}

EXPORT BOOL
get_volhdr(info, vhname)
	FINFO	*info;
	char	*vhname;
{
	error("Volhdr: %s\n", info->f_name);

	if (vhname) {
		return (streql(info->f_name, vhname));
	} else { 
		return (TRUE);
	}
}

EXPORT void
info_to_tcb(info, ptb)
	register FINFO	*info;
	register TCB	*ptb;
{
	if (nullout)
		return;

	if (props.pr_fillc == '0') {
		/*
		 * This is a POSIX compliant header, it is allowed to use
		 * 7 bytes from 8 byte headers as POSIX only requires a ' ' or
		 * '\0' as last char.
		 */
		if (modebits)
			litos(ptb->dbuf.t_mode, (info->f_mode|info->f_type) & 0xFFFF, 7);
		else
			litos(ptb->dbuf.t_mode, info->f_mode & 0xFFFF, 7);

		if (info->f_uid > MAXOCTAL7 && (props.pr_flags & PR_XHDR)) {
			info->f_xflags |= XF_UID;
		}
		litos(ptb->dbuf.t_uid, info->f_uid & MAXOCTAL7, 7);

		if (info->f_gid > MAXOCTAL7 && (props.pr_flags & PR_XHDR)) {
			info->f_xflags |= XF_GID;
		}
		litos(ptb->dbuf.t_gid, info->f_gid & MAXOCTAL7, 7);
	} else {
		/*
		 * This is a pre POSIX header, it is only allowed to use
		 * 6 bytes from 8 byte headers as historic TAR requires a ' '
		 * and a '\0' as last char.
		 */
		if (modebits)
			litos(ptb->dbuf.t_mode, (info->f_mode|info->f_type) & 0xFFFF, 6);
		else
			litos(ptb->dbuf.t_mode, info->f_mode & 0xFFFF, 6);

		if (info->f_uid > MAXOCTAL6 && (props.pr_flags & PR_XHDR)) {
			info->f_xflags |= XF_UID;
		}
		litos(ptb->dbuf.t_uid, info->f_uid & MAXOCTAL6, 6);

		if (info->f_gid > MAXOCTAL7 && (props.pr_flags & PR_XHDR)) {
			info->f_xflags |= XF_GID;
		}
		litos(ptb->dbuf.t_gid, info->f_gid & MAXOCTAL6, 6);
	}

	if (info->f_rsize > MAXOCTAL11 && (props.pr_flags & PR_XHDR)) {
		info->f_xflags |= XF_SIZE;
	}
	if (info->f_rsize <= MAXINT32) {
		litos(ptb->dbuf.t_size, (Ulong)info->f_rsize, 11);
	} else {
		llitos(ptb->dbuf.t_size, (Ullong)info->f_rsize, 11);
	}
	litos(ptb->dbuf.t_mtime, (Ulong)info->f_mtime, 11);
	ptb->dbuf.t_linkflag = XTTOUS(info->f_xftype);

	if (H_TYPE(hdrtype) == H_USTAR) {
		info_to_ustar(info, ptb);
	} else if (H_TYPE(hdrtype) == H_PAX) {
		info_to_ustar(info, ptb);
	} else if (H_TYPE(hdrtype) == H_SUNTAR) {
		info_to_ustar(info, ptb);
	} else if (H_TYPE(hdrtype) == H_XSTAR) {
		info_to_xstar(info, ptb);
	} else if (H_TYPE(hdrtype) == H_XUSTAR) {
		info_to_xstar(info, ptb);
	} else if (H_TYPE(hdrtype) == H_EXUSTAR) {
		info_to_xstar(info, ptb);
	} else if (H_TYPE(hdrtype) == H_GNUTAR) {
		info_to_gnutar(info, ptb);
	} else if (H_TYPE(hdrtype) == H_STAR) {
		info_to_star(info, ptb);
	}
}

LOCAL void
info_to_star(info, ptb)
	register FINFO	*info;
	register TCB	*ptb;
{
	ptb->dbuf.t_vers = STVERSION;
	litos(ptb->dbuf.t_filetype, info->f_filetype & 0xFFFF, 6);	/* XXX -> 7 ??? */
	litos(ptb->dbuf.t_type, info->f_type & 0xFFFF, 11);
#ifdef	needed
	/* XXX we need to do something if st_rdev is > 32 bits */
	if ((info->f_rdevmaj > MAXOCTAL7 || info->f_rdevmin > MAXOCTAL7) &&
	    (props.pr_flags & PR_XHDR)) {
		info->f_xflags |= XF_DEVMAJOR|XF_DEVMINOR;
	}
#endif
	litos(ptb->dbuf.t_rdev, info->f_rdev, 11);
#ifdef	DEV_MINOR_NONCONTIG
	ptb->dbuf.t_devminorbits = '@';
	if (props.pr_flags & PR_XHDR) {
		info->f_xflags |= XF_DEVMAJOR|XF_DEVMINOR;
	}
#else
	ptb->dbuf.t_devminorbits = '@' + minorbits;
#endif

	litos(ptb->dbuf.t_atime, (Ulong)info->f_atime, 11);
	litos(ptb->dbuf.t_ctime, (Ulong)info->f_ctime, 11);
/*	strcpy(ptb->dbuf.t_magic, stmagic);*/
	ptb->dbuf.t_magic[0] = 't';
	ptb->dbuf.t_magic[1] = 'a';
	ptb->dbuf.t_magic[2] = 'r';
	if (!numeric) {
		nameuid(ptb->dbuf.t_uname, STUNMLEN, info->f_uid);
		/* XXX Korrektes overflowchecking */
		if (ptb->dbuf.t_uname[STUNMLEN-1] != '\0' &&
		    props.pr_flags & PR_XHDR) {
			info->f_xflags |= XF_UNAME;
		}
		namegid(ptb->dbuf.t_gname, STGNMLEN, info->f_gid);
		/* XXX Korrektes overflowchecking */
		if (ptb->dbuf.t_gname[STGNMLEN-1] != '\0' &&
		    props.pr_flags & PR_XHDR) {
			info->f_xflags |= XF_GNAME;
		}
		if (*ptb->dbuf.t_uname) {
			info->f_uname = ptb->dbuf.t_uname;
			info->f_umaxlen = STUNMLEN;
		}
		if (*ptb->dbuf.t_gname) {
			info->f_gname = ptb->dbuf.t_gname;
			info->f_gmaxlen = STGNMLEN;
		}
	}

	if (is_sparse(info)) {
		/* XXX Korrektes overflowchecking fuer xhdr */
		if (info->f_size <= MAXINT32) {
			litos(ptb->xstar_in_dbuf.t_realsize, (Ulong)info->f_size, 11);
		} else {
			llitos(ptb->xstar_in_dbuf.t_realsize, (Ullong)info->f_size, 11);
		}
	}
}

LOCAL void
info_to_ustar(info, ptb)
	register FINFO	*info;
	register TCB	*ptb;
{
/*XXX solaris hat illegalerweise mehr als 12 Bit in t_mode !!!
 *	litos(ptb->dbuf.t_mode, info->f_mode|info->f_type & 0xFFFF, 6);	XXX -> 7 ???
*/
/*	strcpy(ptb->ustar_dbuf.t_magic, magic);*/
	ptb->ustar_dbuf.t_magic[0] = 'u';
	ptb->ustar_dbuf.t_magic[1] = 's';
	ptb->ustar_dbuf.t_magic[2] = 't';
	ptb->ustar_dbuf.t_magic[3] = 'a';
	ptb->ustar_dbuf.t_magic[4] = 'r';
/*	strncpy(ptb->ustar_dbuf.t_version, TVERSION, TVERSLEN);*/
	/*
	 * strncpy is slow: use handcrafted replacement.
	 */
	ptb->ustar_dbuf.t_version[0] = '0';
	ptb->ustar_dbuf.t_version[1] = '0';

	if (!numeric) {
		/* XXX Korrektes overflowchecking fuer xhdr */
		nameuid(ptb->ustar_dbuf.t_uname, TUNMLEN, info->f_uid);
		/* XXX Korrektes overflowchecking fuer xhdr */
		namegid(ptb->ustar_dbuf.t_gname, TGNMLEN, info->f_gid);
		if (*ptb->ustar_dbuf.t_uname) {
			info->f_uname = ptb->ustar_dbuf.t_uname;
			info->f_umaxlen = TUNMLEN;
		}
		if (*ptb->ustar_dbuf.t_gname) {
			info->f_gname = ptb->ustar_dbuf.t_gname;
			info->f_gmaxlen = TGNMLEN;
		}
	}
	if (info->f_rdevmaj > MAXOCTAL7 && (props.pr_flags & PR_XHDR)) {
		info->f_xflags |= XF_DEVMAJOR;
	}
	litos(ptb->ustar_dbuf.t_devmajor, info->f_rdevmaj, 7);
#if	DEV_MINOR_BITS > 21		/* XXX */
	/*
	 * XXX The DEV_MINOR_BITS autoconf macro is only tested with 32 bit
	 * XXX ints but this does not matter as it is sufficient to know that
	 * XXX it will not fit into a 7 digit octal number.
	 */
	if (info->f_rdevmin > MAXOCTAL7) {
		extern	BOOL	hpdev;

		if (props.pr_flags & PR_XHDR) {
			info->f_xflags |= XF_DEVMINOR;
		}
		if (!is_special(info)) {
			/*
			 * Until we know how to deal with this, we reduce
			 * the number of files that get non POSIX headers.
			 */
			info->f_rdevmin = 0;
			goto doposix;
		}
		if ((info->f_rdevmin <= MAXOCTAL8) && hpdev) {
			char	c;

			/*
			 * Implement the method from HP-UX that allows 24 bit
			 * for the device minor number. Note that this method
			 * violates the POSIX specs.
			 */
			c = ptb->ustar_dbuf.t_prefix[0];
			litos(ptb->ustar_dbuf.t_devminor, info->f_rdevmin, 8);
			ptb->ustar_dbuf.t_prefix[0] = c;
		} else {
			/*
			 * XXX If we ever need to write more than a long into
			 * XXX devmajor, we need to change llitos() to check
			 * XXX for 7 char limits too.
			 */
			btos(ptb->ustar_dbuf.t_devminor, info->f_rdevmin, 7);
		}
	} else
#endif
		{
#if	DEV_MINOR_BITS > 21
doposix:
#endif
		litos(ptb->ustar_dbuf.t_devminor, info->f_rdevmin, 7);
	}
}

LOCAL void
info_to_xstar(info, ptb)
	register FINFO	*info;
	register TCB	*ptb;
{
	info_to_ustar(info, ptb);
	litos(ptb->xstar_dbuf.t_atime, (Ulong)info->f_atime, 11);
	litos(ptb->xstar_dbuf.t_ctime, (Ulong)info->f_ctime, 11);

	/*
	 * Help recognition in isxmagic(), make sure that prefix[130] is null.
	 */
	ptb->xstar_dbuf.t_prefix[130] = '\0';

	if (H_TYPE(hdrtype) == H_XSTAR) {
/*		strcpy(ptb->xstar_dbuf.t_xmagic, stmagic);*/
		ptb->xstar_dbuf.t_xmagic[0] = 't';
		ptb->xstar_dbuf.t_xmagic[1] = 'a';
		ptb->xstar_dbuf.t_xmagic[2] = 'r';
	}
	if (is_sparse(info)) {
		/* XXX Korrektes overflowchecking fuer xhdr */
		if (info->f_size <= MAXINT32) {
			litos(ptb->xstar_in_dbuf.t_realsize, (Ulong)info->f_size, 11);
		} else {
			llitos(ptb->xstar_in_dbuf.t_realsize, (Ullong)info->f_size, 11);
		}
	}
}

LOCAL void
info_to_gnutar(info, ptb)
	register FINFO	*info;
	register TCB	*ptb;
{
	strcpy(ptb->gnu_dbuf.t_magic, gmagic);

	if (!numeric) {
		nameuid(ptb->ustar_dbuf.t_uname, TUNMLEN, info->f_uid);
		namegid(ptb->ustar_dbuf.t_gname, TGNMLEN, info->f_gid);
		if (*ptb->ustar_dbuf.t_uname) {
			info->f_uname = ptb->ustar_dbuf.t_uname;
			info->f_umaxlen = TUNMLEN;
		}
		if (*ptb->ustar_dbuf.t_gname) {
			info->f_gname = ptb->ustar_dbuf.t_gname;
			info->f_gmaxlen = TGNMLEN;
		}
	}
	if (info->f_xftype == XT_CHR || info->f_xftype == XT_BLK) {
		litos(ptb->ustar_dbuf.t_devmajor, info->f_rdevmaj, 6);	/* XXX -> 7 ??? */
		litos(ptb->ustar_dbuf.t_devminor, info->f_rdevmin, 6);	/* XXX -> 7 ??? */
	}

	/*
	 * XXX GNU tar only fill this if doing a gnudump.
	 */
	litos(ptb->gnu_dbuf.t_atime, (Ulong)info->f_atime, 11);
	litos(ptb->gnu_dbuf.t_ctime, (Ulong)info->f_ctime, 11);

	if (is_sparse(info)) {
		if (info->f_size <= MAXINT32) {
			litos(ptb->gnu_in_dbuf.t_realsize, (Ulong)info->f_size, 11);
		} else {
			llitos(ptb->gnu_in_dbuf.t_realsize, (Ullong)info->f_size, 11);
		}
	}
}

EXPORT int
tcb_to_info(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	int	ret = 0;
	char	xname;
	char	xlink;
	Ulong	ul;
	Ullong	ull;
	int	xt = XT_BAD;
	int	rxt = XT_BAD;
static	BOOL	posixwarn = FALSE;
static	BOOL	namewarn = FALSE;
static	BOOL	modewarn = FALSE;

	/*
	 * F_HAS_NAME is only used from list.c when the -listnew option is
	 * present. Keep f_lname and f_name, don't read LF_LONGLINK/LF_LONGNAME
	 * in this case.
	 */
	if ((info->f_flags & F_HAS_NAME) == 0)
		info->f_lname = ptb->dbuf.t_linkname;
	info->f_uname = info->f_gname = NULL;
	info->f_umaxlen = info->f_gmaxlen = 0L;
	info->f_xftype = XT_BAD;
	info->f_rxftype = XT_BAD;
	info->f_xflags = 0;
	info->f_contoffset = (off_t)0;
	info->f_flags &= F_HAS_NAME;
	info->f_fflags = 0L;

/* XXX JS Test */if (H_TYPE(hdrtype) >= H_CPIO_BASE) {
/* XXX JS Test */cpiotcb_to_info(ptb, info);
/* XXX JS Test */list_file(info);
/* XXX JS Test */return (ret);
/* XXX JS Test */}

	while (pr_isxheader(ptb->dbuf.t_linkflag)) {
		/*
		 * Handle POSIX.1-2001 extensions.
		 */
		if ((ptb->dbuf.t_linkflag == LF_XHDR ||
				    ptb->dbuf.t_linkflag == LF_VU_XHDR)) {
			ret = tcb_to_xhdr(ptb, info);
			if (ret != 0)
				return (ret);

			xt  = info->f_xftype;
			rxt = info->f_rxftype;
		}
		/*
		 * Handle very long names the old (star & gnutar) way.
		 */
		if ((info->f_flags & F_HAS_NAME) == 0 &&
					props.pr_nflags & PR_LONG_NAMES) {
			while (ptb->dbuf.t_linkflag == LF_LONGLINK ||
				    ptb->dbuf.t_linkflag == LF_LONGNAME) {
				ret = tcb_to_longname(ptb, info);
			}
		}
	}
	if (!pr_validtype(ptb->dbuf.t_linkflag)) {
		errmsgno(EX_BAD,
		"WARNING: Archive contains unknown typeflag '%c' (0x%02X).\n",
			ptb->dbuf.t_linkflag, ptb->dbuf.t_linkflag);
	}

	if (ptb->dbuf.t_name[NAMSIZ] == '\0') {
		if (ptb->dbuf.t_name[NAMSIZ-1] == '\0') {
			if (!nowarn && !modewarn) {
				errmsgno(EX_BAD,
				"WARNING: Archive violates POSIX 1003.1 (mode field starts with null byte).\n");
				modewarn = TRUE;
			}
		} else if (!nowarn && !namewarn) {
			errmsgno(EX_BAD,
			"WARNING: Archive violates POSIX 1003.1 (100 char filename is null terminated).\n");
			namewarn = TRUE;
		}
		ptb->dbuf.t_name[NAMSIZ] = ' ';
	}
	stoli(ptb->dbuf.t_mode, &info->f_mode);
	if (info->f_mode & ~07777) {
		if (!nowarn && !modebits && H_TYPE(hdrtype) == H_USTAR && !posixwarn) {
			errmsgno(EX_BAD,
			"WARNING: Archive violates POSIX 1003.1 (too many bits in mode field).\n");
			posixwarn = TRUE;
		}
		info->f_mode &= 07777;
	}
	if ((info->f_xflags & XF_UID) == 0)
		stoli(ptb->dbuf.t_uid, &info->f_uid);
	if ((info->f_xflags & XF_UID) == 0)
		stoli(ptb->dbuf.t_gid, &info->f_gid);
	if ((info->f_xflags & XF_SIZE) == 0) {
		stolli(ptb->dbuf.t_size, &ull);
		info->f_size = ull;
	}
	info->f_rsize = 0L;

#ifdef	OLD
/*XXX	if (ptb->dbuf.t_linkflag < LNKTYPE)*/	/* Alte star Version!!! */
	if (ptb->dbuf.t_linkflag != LNKTYPE &&
					ptb->dbuf.t_linkflag != DIRTYPE) {
		/* XXX
		 * XXX Ist das die richtige Stelle um f_rsize zu setzen ??
		 */
		info->f_rsize = info->f_size;
	}
#else
	switch (ptb->dbuf.t_linkflag) {

	case LNKTYPE:
	case DIRTYPE:
	case CHRTYPE:
	case BLKTYPE:
	case FIFOTYPE:
	case LF_META:
		break;

	default:
		info->f_rsize = info->f_size;
		break;
	}
#endif
	if ((info->f_xflags & XF_MTIME) == 0) {
		stoli(ptb->dbuf.t_mtime, &ul);
		info->f_mtime = (time_t)ul;
		info->f_mnsec = 0L;
	}

	info->f_typeflag = ptb->ustar_dbuf.t_typeflag;

	switch (H_TYPE(hdrtype)) {

	default:
	case H_TAR:
	case H_OTAR:
		tar_to_info(ptb, info);
		break;
	case H_PAX:
	case H_USTAR:
	case H_SUNTAR:
		ustar_to_info(ptb, info);
		break;
	case H_XSTAR:
	case H_XUSTAR:
	case H_EXUSTAR:
		xstar_to_info(ptb, info);
		break;
	case H_GNUTAR:
		gnutar_to_info(ptb, info);
		break;
	case H_STAR:
		star_to_info(ptb, info);
		break;
	}
	info->f_rxftype = info->f_xftype;
	if (rxt != XT_BAD) {
		info->f_rxftype = rxt;
		info->f_filetype = XTTOST(info->f_rxftype);
		info->f_type = XTTOIF(info->f_rxftype);
		/*
		 * XT_LINK may be any 'real' file type,
		 * XT_META may be either a regular file or a contigouos file.
		 */
		if (info->f_xftype != XT_LINK && info->f_xftype != XT_META)
			info->f_xftype = info->f_rxftype;
	}
	if (xt != XT_BAD) {
		info->f_xftype = xt;
	}

	/*
	 * Hack for list module (option -newest) ...
	 * Save and restore t_name[NAMSIZ] & t_linkname[NAMSIZ]
	 */
	xname = ptb->dbuf.t_name[NAMSIZ];
	ptb->dbuf.t_name[NAMSIZ] = '\0';	/* allow 100 chars in name */
	xlink = ptb->dbuf.t_linkname[NAMSIZ];
	ptb->dbuf.t_linkname[NAMSIZ] = '\0';/* allow 100 chars in linkname */

	/*
	 * Handle long name in posix split form now.
	 * Also copy ptb->dbuf.t_linkname[] if namelen is == 100.
	 */
	tcb_to_name(ptb, info);

	ptb->dbuf.t_name[NAMSIZ] = xname;	/* restore remembered value */
	ptb->dbuf.t_linkname[NAMSIZ] = xlink;	/* restore remembered value */

	return (ret);
}

LOCAL void
tar_to_info(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	register int	typeflag = ptb->ustar_dbuf.t_typeflag;

	if (ptb->dbuf.t_name[strlen(ptb->dbuf.t_name) - 1] == '/') {
		typeflag = DIRTYPE;
		info->f_filetype = F_DIR;
		info->f_rsize = (off_t)0;	/* XXX hier?? siehe oben */
	} else if (typeflag == SYMTYPE) {
		info->f_filetype = F_SLINK;
	} else if (typeflag != DIRTYPE) {
		info->f_filetype = F_FILE;
	}
	info->f_xftype = USTOXT(typeflag);
	info->f_type = XTTOIF(info->f_xftype);
	info->f_rdevmaj = info->f_rdevmin = info->f_rdev = 0;
	info->f_ctime = info->f_atime = info->f_mtime;
	info->f_cnsec = info->f_ansec = 0L;
}

LOCAL void
star_to_info(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	Ulong	id;
	Ullong	ull;
	int	mbits;

	version = ptb->dbuf.t_vers;
	if (ptb->dbuf.t_vers < STVERSION) {
		tar_to_info(ptb, info);
		return;
	}
	stoli(ptb->dbuf.t_filetype, &info->f_filetype);
	stoli(ptb->dbuf.t_type, &info->f_type);
	/*
	 * star Erweiterungen sind wieder ANSI kompatibel, d.h. linkflag
	 * hält den echten Dateityp (LONKLINK, LONGNAME, SPARSE ...)
	 */
	if(ptb->dbuf.t_linkflag < '1')
		info->f_xftype = IFTOXT(info->f_type);
	else
		info->f_xftype = USTOXT(ptb->ustar_dbuf.t_typeflag);

	stoli(ptb->dbuf.t_rdev, &info->f_rdev);
	if ((info->f_xflags & (XF_DEVMAJOR|XF_DEVMINOR)) !=
						(XF_DEVMAJOR|XF_DEVMINOR)) {
		mbits = ptb->dbuf.t_devminorbits - '@';
		if (mbits == 0) {
			static	BOOL	dwarned = FALSE;
			if (!dwarned) {
				errmsgno(EX_BAD,
#ifdef	DEV_MINOR_NONCONTIG
				"WARNING: Minor device numbers are non contiguous, devices may not be extracted correctly.\n");
#else
				"WARNING: The archiving system used non contiguous minor numbers, cannot extract devices correctly.\n");
#endif
				dwarned = TRUE;
			}
			/*
			 * Let us hope that both, the archiving and the
			 * extracting system use the same major()/minor()
			 * mapping.
			 */
			info->f_rdevmaj	= major(info->f_rdev);
			info->f_rdevmin	= minor(info->f_rdev);
		} else {
			/*
			 * Convert from remote major()/minor() mapping to
			 * local major()/minor() mapping.
			 */
			if (mbits < 0)		/* Old star format */
				mbits = 8;
			info->f_rdevmaj	= _dev_major(mbits, info->f_rdev);
			info->f_rdevmin	= _dev_minor(mbits, info->f_rdev);
			info->f_rdev = makedev(info->f_rdevmaj, info->f_rdevmin);
		}
	}

	if ((info->f_xflags & XF_ATIME) == 0) {
		stoli(ptb->dbuf.t_atime, &id);
		info->f_atime = (time_t)id;
		info->f_ansec = 0L;
	}
	if ((info->f_xflags & XF_CTIME) == 0) {
		stoli(ptb->dbuf.t_ctime, &id);
		info->f_ctime = (time_t)id;
		info->f_cnsec = 0L;
	}

	if ((info->f_xflags & XF_UNAME) == 0) {
		if (*ptb->dbuf.t_uname) {
			info->f_uname = ptb->dbuf.t_uname;
			info->f_umaxlen = STUNMLEN;
		}
	}
	if (info->f_uname) {
		if (!numeric && uidname(info->f_uname, info->f_umaxlen, &id))
			info->f_uid = id;
	}
	if ((info->f_xflags & XF_GNAME) == 0) {
		if (*ptb->dbuf.t_gname) {
			info->f_gname = ptb->dbuf.t_gname;
			info->f_gmaxlen = STGNMLEN;
		}
	}
	if (info->f_gname) {
		if (!numeric && gidname(info->f_gname, info->f_gmaxlen, &id))
			info->f_gid = id;
	}

	if (is_sparse(info)) {
		stolli(ptb->xstar_in_dbuf.t_realsize, &ull);
		info->f_size = ull;
	}
	if (is_multivol(info)) {
		stolli(ptb->xstar_in_dbuf.t_offset, &ull);
		info->f_contoffset = ull;
	}
}

LOCAL void
ustar_to_info(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	Ulong	id;
	char	c;

	info->f_xftype = USTOXT(ptb->ustar_dbuf.t_typeflag);
	info->f_filetype = XTTOST(info->f_xftype);
	info->f_type = XTTOIF(info->f_xftype);

	if ((info->f_xflags & XF_UNAME) == 0) {
		if (*ptb->ustar_dbuf.t_uname) {
			info->f_uname = ptb->ustar_dbuf.t_uname;
			info->f_umaxlen = TUNMLEN;
		}
	}
	if (info->f_uname) {
		if (!numeric && uidname(info->f_uname, info->f_umaxlen, &id))
			info->f_uid = id;
	}
	if ((info->f_xflags & XF_GNAME) == 0) {
		if (*ptb->ustar_dbuf.t_gname) {
			info->f_gname = ptb->ustar_dbuf.t_gname;
			info->f_gmaxlen = TGNMLEN;
		}
	}
	if (info->f_gname) {
		if (!numeric && gidname(info->f_gname, info->f_gmaxlen, &id))
			info->f_gid = id;
	}

	if ((info->f_xflags & XF_DEVMAJOR) == 0)
		stoli(ptb->ustar_dbuf.t_devmajor, &info->f_rdevmaj);

	if ((info->f_xflags & XF_DEVMINOR) == 0) {
		if (ptb->ustar_dbuf.t_devminor[0] & 0x80) {
			stob(ptb->ustar_dbuf.t_devminor, &info->f_rdevmin, 7);
		} else {
			/*
			 * The 'tar' that comes with HP-UX writes illegal tar
			 * archives. It includes 8 characters in the minor
			 * field and allows to archive 24 bits for the minor
			 * device which are used by HP-UX. As we like to be
			 * able to read these archives, we need to convert
			 * the number carefully by temporarily writing a NULL
			 * to the next character and restoring the right
			 * content afterwards.
			 */
			c = ptb->ustar_dbuf.t_prefix[0];
			ptb->ustar_dbuf.t_prefix[0] = '\0';
			stoli(ptb->ustar_dbuf.t_devminor, &info->f_rdevmin);
			ptb->ustar_dbuf.t_prefix[0] = c;
		}
	}

	info->f_rdev = makedev(info->f_rdevmaj, info->f_rdevmin);

	/*
	 * ANSI Tar hat keine atime & ctime im Header!
	 */
	if ((info->f_xflags & XF_ATIME) == 0) {
		info->f_atime = info->f_mtime;
		info->f_ansec = 0L;
	}
	if ((info->f_xflags & XF_CTIME) == 0) {
		info->f_ctime = info->f_mtime;
		info->f_cnsec = 0L;
	}
}

LOCAL void
xstar_to_info(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	Ulong	ul;
	Ullong	ull;

	ustar_to_info(ptb, info);

	if ((info->f_xflags & XF_ATIME) == 0) {
		stoli(ptb->xstar_dbuf.t_atime, &ul);
		info->f_atime = (time_t)ul;
		info->f_ansec = 0L;
	}
	if ((info->f_xflags & XF_CTIME) == 0) {
		stoli(ptb->xstar_dbuf.t_ctime, &ul);
		info->f_ctime = (time_t)ul;
		info->f_cnsec = 0L;
	}

	if (is_sparse(info)) {
		stolli(ptb->xstar_in_dbuf.t_realsize, &ull);
		info->f_size = ull;
	}
	if (is_multivol(info)) {
		stolli(ptb->xstar_in_dbuf.t_offset, &ull);
		info->f_contoffset = ull;
	}
}

LOCAL void
gnutar_to_info(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	Ulong	ul;
	Ullong	ull;

	ustar_to_info(ptb, info);

	if ((info->f_xflags & XF_ATIME) == 0) {
		stoli(ptb->gnu_dbuf.t_atime, &ul);
		info->f_atime = (time_t)ul;
		info->f_ansec = 0L;
		if (info->f_atime == 0 && ptb->gnu_dbuf.t_atime[0] == '\0')
			info->f_atime = info->f_mtime;
	}

	if ((info->f_xflags & XF_CTIME) == 0) {
		stoli(ptb->gnu_dbuf.t_ctime, &ul);
		info->f_ctime = (time_t)ul;
		info->f_cnsec = 0L;
		if (info->f_ctime == 0 && ptb->gnu_dbuf.t_ctime[0] == '\0')
			info->f_ctime = info->f_mtime;
	}

	if (is_sparse(info)) {
		stolli(ptb->gnu_in_dbuf.t_realsize, &ull);
		info->f_size = ull;
	}
	if (is_multivol(info)) {
		stolli(ptb->gnu_dbuf.t_offset, &ull);
		info->f_contoffset = ull;
	}
}

/*
 * XXX vorerst nur zum Test!
 */
LOCAL void
cpiotcb_to_info(ptb, info)
	register TCB	*ptb;
	register FINFO	*info;
{
	Ulong	ul;

	astoo_cpio(&((char *)ptb)[6], &ul, 6);
	info->f_dev = ul;
	astoo_cpio(&((char *)ptb)[12], &ul, 6);
	info->f_ino = ul;
error("ino: %lld\n", (Llong)info->f_ino);
	astoo_cpio(&((char *)ptb)[18], &info->f_mode, 6);
error("mode: %lo\n", info->f_mode);
	info->f_type = info->f_mode & S_IFMT;
	info->f_mode = info->f_mode & 07777;
	info->f_xftype = IFTOXT(info->f_type);
	info->f_filetype = XTTOST(info->f_xftype);
	astoo_cpio(&((char *)ptb)[24], &info->f_uid, 6);
	astoo_cpio(&((char *)ptb)[30], &info->f_gid, 6);
	astoo_cpio(&((char *)ptb)[36], &info->f_nlink, 6);
	astoo_cpio(&((char *)ptb)[42], &info->f_rdev, 6);

	astoo_cpio(&((char *)ptb)[48], &ul, 11);
	info->f_atime = (time_t)ul;

	astoo_cpio(&((char *)ptb)[59], &info->f_namelen, 6);

	astoo_cpio(&((char *)ptb)[65], &ul, 11);
	info->f_size = ul;
info->f_rsize = info->f_size;
	info->f_name = &((char *)ptb)[76];
}

LOCAL int
ustoxt(ustype)
	char	ustype;
{
	/*
	 * Map ANSI types
	 */
	if (ustype >= REGTYPE && ustype <= CONTTYPE)
		return _USTOXT(ustype);

	/*
	 * Map Vendor Unique (Gnu tar & Star) types ANSI: "local enhancements"
	 */
	if ((props.pr_flags & (PR_LOCAL_STAR|PR_LOCAL_GNU)) &&
					ustype >= 'A' && ustype <= 'Z')
		return _VTTOXT(ustype);

	/*
	 * treat unknown types as regular files conforming to standard
	 */
	return (XT_FILE);
}

/* ARGSUSED */
EXPORT BOOL
ia_change(ptb, info)
	TCB	*ptb;
	FINFO	*info;
{
	char	buf[NAMSIZ+1];	/* XXX nur 100 chars ?? */
	char	ans;
	int	len;

	if (verbose)
		list_file(info);
	else
		vprint(info);
	if (nflag)
		return (FALSE);
	fprintf(vpr, "get/put ? Y(es)/N(o)/C(hange name) :");fflush(vpr);
	fgetline(tty, buf, 2);
	if ((ans = toupper(buf[0])) == 'Y')
		return (TRUE);
	else if (ans == 'C') {
		for(;;) {
			fprintf(vpr, "Enter new name:");
			fflush(vpr);
			if ((len = fgetline(tty, buf, sizeof buf)) == 0)
				continue;
			if (len > sizeof(buf) - 1)
				errmsgno(EX_BAD, "Name too long.\n");
			else
				break;
		}
		strcpy(info->f_name, buf);	/* XXX nur 100 chars ?? */
		if (xflag && newer(info))
			return (FALSE);
		return (TRUE);
	}
	return (FALSE);
}

LOCAL BOOL
checkeof(ptb)
	TCB	*ptb;
{
	if (!eofblock(ptb))
		return (FALSE);
	if (debug)
		errmsgno(EX_BAD, "First  EOF Block OK\n");
	markeof();

	if (readblock((char *)ptb) == EOF) {
		errmsgno(EX_BAD, "Incorrect EOF, second EOF block is missing.\n");
		return (TRUE);
	}
	if (!eofblock(ptb)) {
		if (!nowarn)
			errmsgno(EX_BAD, "WARNING: Partial (single block) EOF detected.\n");
		return (FALSE);
	}
	if (debug)
		errmsgno(EX_BAD, "Second EOF Block OK\n");
	return (TRUE);
}

LOCAL BOOL
eofblock(ptb)
	TCB	*ptb;
{
	register short	i;
	register char	*s = (char *) ptb;

	if (props.pr_nflags & PR_DUMB_EOF)
		return (ptb->dbuf.t_name[0] == '\0');

	for (i=0; i < TBLOCK; i++)
		if (*s++ != '\0')
			return (FALSE);
	return (TRUE);
}

/*
 * Convert octal string -> long int
 */
LOCAL void /*char **/
astoo_cpio(s,l, cnt)
	register char	*s;
		 Ulong	*l;
	register int	cnt;
{
	register Ulong	ret = 0L;
	register char	c;
	register int	t;
	
	for(;cnt > 0; cnt--) {
		c = *s++;
		if(isoctal(c))
			t = c - '0';
		else
			break;
		ret *= 8;
		ret += t;
	}
	*l = ret;
	/*return(s);*/
}

/*
 * Convert string -> long int
 */
LOCAL void /*char **/
stoli(s,l)
	register char	*s;
		 Ulong	*l;
{
	register Ulong	ret = 0L;
	register char	c;
	register int	t;
	
	while(*s == ' ')
		s++;

	for(;;) {
		c = *s++;
		if(isoctal(c))
			t = c - '0';
		else
			break;
		ret *= 8;
		ret += t;
	}
	*l = ret;
	/*return(s);*/
}

/*
 * Convert string -> long long int
 */
EXPORT void /*char **/
stolli(s,ull)
	register char	*s;
		 Ullong	*ull;
{
	register Ullong	ret = (Ullong)0;
	register char	c;
	register int	t;
	
	if (*((Uchar*)s) & 0x80) {
		stollb(s, ull, 11);
		return;
	}

	while(*s == ' ')
		s++;

	for(;;) {
		c = *s++;
		if(isoctal(c))
			t = c - '0';
		else
			break;
		ret *= 8;
		ret += t;
	}
	*ull = ret;
	/*return(s);*/
}

/*
 * Convert long int -> string.
 */
LOCAL void
litos(s, l, fieldw)
		 char	*s;
	register Ulong	l;
	register int	fieldw;
{
	register char	*p	= &s[fieldw+1];
	register char	fill	= props.pr_fillc;

	/*
	 * Bei 12 Byte Feldern würde hier das Nächste Feld überschrieben, wenn
	 * entgegen der normalen Reihenfolge geschrieben wird!
	 * Da der TCB sowieso vorher genullt wird ist es aber kein Problem
	 * das bei 8 Bytes Feldern notwendige Nullbyte wegzulassen.
	 */
/*XXX	*p = '\0';*/
	/*
	 * Das Zeichen nach einer Zahl.
	 * XXX Soll hier besser ein NULL Byte bei POSIX Tar hin?
	 * XXX Wuerde das Probleme mit einer automatischen Erkennung geben?
	 */
	*--p = ' ';
/*???	*--p = '\0';*/

	do {
		*--p = (l%8) + '0';	/* Compiler optimiert */

	} while (--fieldw > 0 && (l /= 8) > 0);

	switch (fieldw) {

	default:
		break;
	case 11:	*--p = fill;	/* FALLTHROUGH */
	case 10:	*--p = fill;	/* FALLTHROUGH */
	case 9:		*--p = fill;	/* FALLTHROUGH */
	case 8:		*--p = fill;	/* FALLTHROUGH */
	case 7:		*--p = fill;	/* FALLTHROUGH */
	case 6:		*--p = fill;	/* FALLTHROUGH */
	case 5:		*--p = fill;	/* FALLTHROUGH */
	case 4:		*--p = fill;	/* FALLTHROUGH */
	case 3:		*--p = fill;	/* FALLTHROUGH */
	case 2:		*--p = fill;	/* FALLTHROUGH */
	case 1:		*--p = fill;	/* FALLTHROUGH */
	case 0:		;
	}
}

/*
 * Convert long long int -> string.
 */
EXPORT void
llitos(s, ull, fieldw)
		 char	*s;
	register Ullong	ull;
	register int	fieldw;
{
	register char	*p	= &s[fieldw+1];
	register char	fill	= props.pr_fillc;

	/*
	 * Currently only used with fieldwidth == 11.
	 * XXX Large 8 byte fields are handled separately.
	 */
	if (/*fieldw == 11 &&*/ ull > MAXOCTAL11) {
		llbtos(s, ull, fieldw);
		return;
	}

	/*
	 * Bei 12 Byte Feldern würde hier das Nächste Feld überschrieben, wenn
	 * entgegen der normalen Reihenfolge geschrieben wird!
	 * Da der TCB sowieso vorher genullt wird ist es aber kein Problem
	 * das bei 8 Bytes Feldern notwendige Nullbyte wegzulassen.
	 */
/*XXX	*p = '\0';*/
	/*
	 * Das Zeichen nach einer Zahl.
	 * XXX Soll hier besser ein NULL Byte bei POSIX Tar hin?
	 * XXX Wuerde das Probleme mit einer automatischen Erkennung geben?
	 */
	*--p = ' ';
/*???	*--p = '\0';*/

	do {
		*--p = (ull%8) + '0';	/* Compiler optimiert */

	} while (--fieldw > 0 && (ull /= 8) > 0);

	switch (fieldw) {

	default:
		break;
	case 11:	*--p = fill;	/* FALLTHROUGH */
	case 10:	*--p = fill;	/* FALLTHROUGH */
	case 9:		*--p = fill;	/* FALLTHROUGH */
	case 8:		*--p = fill;	/* FALLTHROUGH */
	case 7:		*--p = fill;	/* FALLTHROUGH */
	case 6:		*--p = fill;	/* FALLTHROUGH */
	case 5:		*--p = fill;	/* FALLTHROUGH */
	case 4:		*--p = fill;	/* FALLTHROUGH */
	case 3:		*--p = fill;	/* FALLTHROUGH */
	case 2:		*--p = fill;	/* FALLTHROUGH */
	case 1:		*--p = fill;	/* FALLTHROUGH */
	case 0:		;
	}
}

/*
 * Convert binary (base 256) string -> long int.
 */
LOCAL void /*char **/
stob(s, l, fieldw)
	register char	*s;
		 Ulong	*l;
	register int	fieldw;
{
	register Ulong	ret = 0L;
	register Uchar	c;
	
	c = *s++ & 0x7F;
	ret = c * 256;

	while (--fieldw >= 0) {
		c = *s++;
		ret *= 256;
		ret += c;
	}
	*l = ret;
	/*return(s);*/
}

/*
 * Convert binary (base 256) string -> long long int.
 */
LOCAL void /*char **/
stollb(s, ull, fieldw)
	register char	*s;
		 Ullong	*ull;
	register int	fieldw;
{
	register Ullong	ret = 0L;
	register Uchar	c;
	
	c = *s++ & 0x7F;
	ret = c * 256;

	while (--fieldw >= 0) {
		c = *s++;
		ret *= 256;
		ret += c;
	}
	*ull = ret;
	/*return(s);*/
}

/*
 * Convert long int -> binary (base 256) string.
 */
LOCAL void
btos(s, l, fieldw)
		 char	*s;
	register Ulong	l;
	register int	fieldw;
{
	register char	*p	= &s[fieldw+1];

	do {
		*--p = l%256;	/* Compiler optimiert */

	} while (--fieldw > 0 && (l /= 256) > 0);

	s[0] |= 0x80;
}

/*
 * Convert long long int -> binary (base 256) string.
 */
LOCAL void
llbtos(s, ull, fieldw)
		 char	*s;
	register Ullong	ull;
	register int	fieldw;
{
	register char	*p	= &s[fieldw+1];

	do {
		*--p = ull%256;	/* Compiler optimiert */

	} while (--fieldw > 0 && (ull /= 256) > 0);

	s[0] |= 0x80;
}
