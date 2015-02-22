/*#define	DEBUG*/
/* @(#)hole.c	1.29 02/05/11 Copyright 1990 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)hole.c	1.29 02/05/11 Copyright 1990 J. Schilling";
#endif
/*
 *	Handle files with holes (sparse files)
 *
 *	Copyright (c) 1990 J. Schilling
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
#include <standard.h>
#include "star.h"
#include <schily.h>
#include "props.h"
#include "table.h"
#include "starsubs.h"
#ifdef	sun
#	include <sys/filio.h>
#	if	_FIOAI == _FIOOBSOLETE67
#	undef	_FIOAI
#	endif
#	ifdef	_FIOAI
#	include <sys/fs/ufs_filio.h>
#	endif
#endif	/* sun */

#define DEBUG
#ifdef DEBUG
#define	EDEBUG(a)	if (debug) error a
#else
#define	EDEBUG(a)
#endif

extern	long	bigcnt;
extern	char	*bigptr;

extern	BOOL	debug;
extern	BOOL	nullout;

/*
 * XXX If we have really big files, there could in theory be more than
 * XXX 2 billions of hole/nohole pairs in a file.
 * XXX But if this happens, we would need > 32 GB of RAM for the hole list.
 */
typedef	struct {
	FILE	*fh_file;
	char	*fh_name;
	off_t	fh_size;
	off_t	fh_newpos;
	sp_t	*fh_sparse;
	int	fh_nsparse;
	int	fh_spindex;
	int	fh_diffs;
} fh_t;

LOCAL	int	force_hole_func	__PR((fh_t * fh, char* p, int amount));
EXPORT	int	get_forced_hole	__PR((FILE * f, FINFO * info));
LOCAL	int	get_sparse_func	__PR((fh_t * fh, char* p, int amount));
LOCAL	int	cmp_sparse_func	__PR((fh_t * fh, char* p, int amount));
LOCAL	int	put_sparse_func	__PR((fh_t * fh, char* p, int amount));
LOCAL	sp_t*	grow_sp_list	__PR((sp_t * sparse, int* nspp));
LOCAL	sp_t*	get_sp_list	__PR((FINFO * info));
LOCAL	int	mk_sp_list	__PR((int *fp, FINFO * info, sp_t ** spp));
EXPORT	int	gnu_skip_extended __PR((TCB * ptb));
EXPORT	int	get_sparse	__PR((FILE * f, FINFO * info));
EXPORT	BOOL	cmp_sparse	__PR((FILE * f, FINFO * info));
EXPORT	void	put_sparse	__PR((int *fp, FINFO * info));
LOCAL	void	put_sp_list	__PR((FINFO * info, sp_t * sparse, int nsparse));

#define	vp_force_hole_func ((int(*)__PR((void *, char *, int)))force_hole_func)

LOCAL int
force_hole_func(fh, p, amount)
	register fh_t	*fh;
	register char	*p;
		 int	amount;
{
	register int	cnt;

	fh->fh_newpos += amount;
	if (amount < fh->fh_size &&
				cmpnullbytes(bigptr, amount) >= amount) {
		if (lseek(fdown(fh->fh_file), fh->fh_newpos, SEEK_SET) == (off_t)-1) {
			xstats.s_rwerrs++;
			errmsg("Error seeking '%s'.\n", fh->fh_name);
		}

		fh->fh_size -= amount;
		return (amount);
	}
	cnt = ffilewrite(fh->fh_file, p, amount);
	fh->fh_size -= amount;
	return (cnt);
}

EXPORT BOOL
get_forced_hole(f, info)
	FILE	*f;
	FINFO	*info;
{
	fh_t	fh;

	fh.fh_file = f;
	fh.fh_name = info->f_name;
	fh.fh_size = info->f_rsize;
	fh.fh_newpos = (off_t)0;
	return (xt_file(info, vp_force_hole_func, &fh, TBLOCK, "writing"));
}

#define	vp_get_sparse_func ((int(*)__PR((void *, char *, int)))get_sparse_func)

LOCAL int
get_sparse_func(fh, p, amount)
	register fh_t	*fh;
	register char	*p;
		 int	amount;
{
	register int	cnt;

	EDEBUG(("amount: %d newpos: %lld index: %d\n",
					amount, (Llong)fh->fh_newpos, fh->fh_spindex));

	if (fh->fh_sparse[fh->fh_spindex].sp_offset > fh->fh_newpos) {

		EDEBUG(("seek to: %lld\n",
				(Llong)fh->fh_sparse[fh->fh_spindex].sp_offset));

		if (lseek(fdown(fh->fh_file),
				fh->fh_sparse[fh->fh_spindex].sp_offset, SEEK_SET) < 0) {
			xstats.s_rwerrs++;
			errmsg("Error seeking '%s'.\n", fh->fh_name);
		}

		fh->fh_newpos = fh->fh_sparse[fh->fh_spindex].sp_offset;
	}
	EDEBUG(("write %d at: %lld\n", amount, (Llong)fh->fh_newpos));

	cnt = ffilewrite(fh->fh_file, p, amount);
	fh->fh_size -= cnt;
	fh->fh_newpos += cnt;

	EDEBUG(("off: %lld numb: %lld cnt: %d off+numb: %lld newpos: %lld index: %d\n", 
		(Llong)fh->fh_sparse[fh->fh_spindex].sp_offset,
		(Llong)fh->fh_sparse[fh->fh_spindex].sp_numbytes, cnt,
		(Llong)(fh->fh_sparse[fh->fh_spindex].sp_offset +
			fh->fh_sparse[fh->fh_spindex].sp_numbytes),
		(Llong)fh->fh_newpos,
		fh->fh_spindex));

	if ((fh->fh_sparse[fh->fh_spindex].sp_offset +
		fh->fh_sparse[fh->fh_spindex].sp_numbytes)
				 <= fh->fh_newpos) {
		fh->fh_spindex++;

		EDEBUG(("new index: %d\n", fh->fh_spindex));
	}
	EDEBUG(("return (%d)\n", cnt));
	return (cnt);
}

#define	vp_cmp_sparse_func ((int(*)__PR((void *, char *, int)))cmp_sparse_func)

LOCAL int
cmp_sparse_func(fh, p, amount)
	register fh_t	*fh;
	register char	*p;
		 int	amount;
{
	register int	cnt;
		 char	*cmp_buf[TBLOCK];

	EDEBUG(("amount: %d newpos: %lld index: %d\n",
					amount,
					(Llong)fh->fh_newpos, fh->fh_spindex));
	/*
	 * If we already found diffs we save time and only pass tape ...
	 */
	if (fh->fh_diffs)
		return (amount);

	if (fh->fh_sparse[fh->fh_spindex].sp_offset > fh->fh_newpos) {
		EDEBUG(("seek to: %lld\n",
				(Llong)fh->fh_sparse[fh->fh_spindex].sp_offset));

		while (fh->fh_newpos < fh->fh_sparse[fh->fh_spindex].sp_offset){
			register int	amt;

			amt = min(TBLOCK,
				fh->fh_sparse[fh->fh_spindex].sp_offset -
				fh->fh_newpos);

			cnt = ffileread(fh->fh_file, cmp_buf, amt);
			if (cnt != amt)
				fh->fh_diffs++;

			if (cmpnullbytes(cmp_buf, amt) < cnt)
				fh->fh_diffs++;

			fh->fh_newpos += cnt;

			if (fh->fh_diffs)
				return (amount);
		}
	}
	EDEBUG(("read %d at: %lld\n", amount, (Llong)fh->fh_newpos));

	cnt = ffileread(fh->fh_file, cmp_buf, amount);
	if (cnt != amount)
		fh->fh_diffs++;

	if (cmpbytes(cmp_buf, p, cnt) < cnt)
		fh->fh_diffs++;

	fh->fh_size -= cnt;
	fh->fh_newpos += cnt;

	EDEBUG(("off: %lld numb: %lld cnt: %d off+numb: %lld newpos: %lld index: %d\n", 
		(Llong)fh->fh_sparse[fh->fh_spindex].sp_offset,
		(Llong)fh->fh_sparse[fh->fh_spindex].sp_numbytes, cnt,
		(Llong)(fh->fh_sparse[fh->fh_spindex].sp_offset +
			fh->fh_sparse[fh->fh_spindex].sp_numbytes),
		(Llong)fh->fh_newpos,
		fh->fh_spindex));

	if ((fh->fh_sparse[fh->fh_spindex].sp_offset +
		fh->fh_sparse[fh->fh_spindex].sp_numbytes)
				 <= fh->fh_newpos) {
		fh->fh_spindex++;

		EDEBUG(("new index: %d\n", fh->fh_spindex));
	}
	EDEBUG(("return (%d) diffs: %d\n", cnt, fh->fh_diffs));
	return (cnt);
}

#define	vp_put_sparse_func ((int(*)__PR((void *, char *, int)))put_sparse_func)

LOCAL int
put_sparse_func(fh, p, amount)
	register fh_t	*fh;
	register char	*p;
		 int	amount;
{
	register int	cnt;

	EDEBUG(("amount: %d newpos: %lld index: %d\n",
					amount,
					(Llong)fh->fh_newpos, fh->fh_spindex));

	if (fh->fh_spindex < fh->fh_nsparse &&
		fh->fh_sparse[fh->fh_spindex].sp_offset > fh->fh_newpos) {

		EDEBUG(("seek to: %lld\n",
				(Llong)fh->fh_sparse[fh->fh_spindex].sp_offset));

		if (lseek(*(int *)(fh->fh_file),
				fh->fh_sparse[fh->fh_spindex].sp_offset, SEEK_SET) < 0) {
			xstats.s_rwerrs++;
			errmsg("Error seeking '%s'.\n", fh->fh_name);
		}

		fh->fh_newpos = fh->fh_sparse[fh->fh_spindex].sp_offset;
	}
	EDEBUG(("read %d at: %lld\n", amount, (Llong)fh->fh_newpos));

	if (nullout) {
		cnt = amount;
		if (cnt > fh->fh_size)
			cnt = fh->fh_size;
	} else {
		cnt = _fileread((int *)fh->fh_file, p, amount);
	}
	fh->fh_size -= cnt;
	fh->fh_newpos += cnt;
/*	if (cnt < TBLOCK)*/
/*		fillbytes(&p[cnt], TBLOCK-cnt, '\0');*/

	EDEBUG(("off: %lld numb: %lld cnt: %d off+numb: %lld newpos: %lld index: %d\n", 
		(Llong)fh->fh_sparse[fh->fh_spindex].sp_offset,
		(Llong)fh->fh_sparse[fh->fh_spindex].sp_numbytes, cnt,
		(Llong)(fh->fh_sparse[fh->fh_spindex].sp_offset +
			fh->fh_sparse[fh->fh_spindex].sp_numbytes),
		(Llong)fh->fh_newpos,
		fh->fh_spindex));

	if ((fh->fh_sparse[fh->fh_spindex].sp_offset +
		fh->fh_sparse[fh->fh_spindex].sp_numbytes)
				 <= fh->fh_newpos) {
		fh->fh_spindex++;

		EDEBUG(("new index: %d\n", fh->fh_spindex));
	}
	EDEBUG(("return (%d)\n", cnt));
	return (cnt);
}

LOCAL sp_t *
grow_sp_list(sparse, nspp)
	sp_t	*sparse;
	int	*nspp;
{
	sp_t	*new;

	if (*nspp < 512)
		*nspp *= 2;
	else
		*nspp += 512;
	new = (sp_t *)realloc(sparse, *nspp*sizeof(sp_t));
	if (new == 0) {
		errmsg("Cannot grow sparse buf.\n");
		free(sparse);
	}
	return (new);
}

LOCAL sp_t *
get_sp_list(info)
	FINFO	*info;
{
	TCB	tb;
	TCB	*ptb = info->f_tcb;
	sp_t	*sparse;
	int	nsparse = 25;
	int	extended;
	register int	i;
	register int	sparse_in_hdr = props.pr_sparse_in_hdr;
	register int	ind;
		char	*p;
/*XXX*/extern long hdrtype;

	EDEBUG(("rsize: %lld\n" , (Llong)info->f_rsize));

	sparse = (sp_t *)malloc(nsparse*sizeof(sp_t));
	if (sparse == 0) {
		errmsg("Cannot alloc sparse buf.\n");
		return (sparse);
	}

	if (H_TYPE(hdrtype) == H_GNUTAR) {
		p = (char *)ptb->gnu_in_dbuf.t_sp;
	} else {
		p = (char *)ptb->xstar_in_dbuf.t_sp;

		if (ptb->xstar_dbuf.t_prefix[0] == '\0' &&
		    ptb->xstar_in_dbuf.t_sp[0].t_offset[10] != '\0') {
			static	BOOL	warned;
			extern	BOOL	nowarn;

			if (!nowarn && !warned) {
				errmsgno(EX_BAD, "WARNING: Archive uses old sparse format. Please upgrade!\n");
				warned = TRUE;
			}
			sparse_in_hdr = SPARSE_IN_HDR;
		}
	}

	for (i= 0; i < sparse_in_hdr; i++) {
		Ullong	ull;

		stolli(p, &ull); p += 12;
		sparse[i].sp_offset = ull;
		stolli(p, &ull);   p += 12;
		sparse[i].sp_numbytes = ull;
		if (sparse[i].sp_numbytes == 0)
			break;
	}
#ifdef	DEBUG
	if (debug) for (i = 0; i < sparse_in_hdr; i++) {
		error("i: %d offset: %lld numbytes: %lld\n", i,
				(Llong)sparse[i].sp_offset,
				(Llong)sparse[i].sp_numbytes);
		if (sparse[i].sp_numbytes == 0)
			break;
	}
#endif
	ind = sparse_in_hdr-SPARSE_EXT_HDR;

	if (H_TYPE(hdrtype) == H_GNUTAR)
		extended = ptb->gnu_in_dbuf.t_isextended;
	else
		extended = ptb->xstar_in_dbuf.t_isextended;

	extended |= (sparse_in_hdr == 0);

	EDEBUG(("isextended: %d\n", extended));

	ptb = &tb;	/* don't destroy orig TCB */
	while (extended) {
		if (readblock((char *)ptb) == EOF) {
			free(sparse);
			return (0);
		}
		if ((props.pr_flags & PR_GNU_SPARSE_BUG) == 0)
			info->f_rsize -= TBLOCK;

		EDEBUG(("rsize: %lld\n" , (Llong)info->f_rsize));

		ind += SPARSE_EXT_HDR;

		EDEBUG(("ind: %d\n", ind));

		if (i+ind > nsparse+1) {
			if ((sparse = grow_sp_list(sparse, &nsparse)) == 0)
				return ((sp_t *)0);
		}
		p = (char *)ptb;
		for (i = 0; i < SPARSE_EXT_HDR; i++) {
			Ullong	ull;

			stolli(p, &ull);     p += 12;
			sparse[i+ind].sp_offset = ull;
			stolli(p, &ull);       p += 12;
			sparse[i+ind].sp_numbytes = ull;

			EDEBUG(("i: %d offset: %lld numbytes: %lld\n", i,
				(Llong)sparse[i+ind].sp_offset,
				(Llong)sparse[i+ind].sp_numbytes));

			if (sparse[i+ind].sp_numbytes == 0)
				break;
		}
		extended = ptb->gnu_ext_dbuf.t_isextended;
	}
#ifdef	DEBUG
	ind += i;
	EDEBUG(("ind: %d\n", ind));
	if (debug) for (i = 0; i < ind; i++) {
		error("i: %d offset: %lld numbytes: %lld\n", i,
				(Llong)sparse[i].sp_offset,
				(Llong)sparse[i].sp_numbytes);
		if (sparse[i].sp_numbytes == 0)
			break;
	}
	EDEBUG(("rsize: %lld\n" , (Llong)info->f_rsize));
#endif
	return (sparse);
}

LOCAL int
mk_sp_list(fp, info, spp)
	int	*fp;
	FINFO	*info;
	sp_t	**spp;
{
	long	rbuf[32*1024/sizeof(long)];	/* Force it be long aligned */
	sp_t	*sparse;
	int	nsparse = 25;
	register int	amount = 0;
	register int	cnt = 0;
	register char	*rbp = (char *)rbuf;
	register off_t	pos = (off_t)0;
	register int	i = 0;
	register BOOL	data = FALSE;
	register BOOL	use_ai = FALSE;
	register BOOL	is_hole = FALSE;
#ifdef	_FIOAI
		int	fai_idx;
	struct fioai	fai;
	struct fioai	*faip;
#	define	NFAI	1024
	daddr_t		fai_arr[NFAI];
#endif	/* _FIOAI */

#ifdef	_FIOAI
	fai.fai_off = 0;
	fai.fai_size = 512;
	fai.fai_num = 1;
	fai.fai_daddr = fai_arr;
	use_ai = ioctl(*fp, _FIOAI, &fai) >= 0;	/* Use if operational*/
	fai_idx = 0;
	fai.fai_num = 0;			/* start at 0 again  */
#endif	/* _FIOAI */

	*spp = (sp_t *)0;
	info->f_rsize = 0;
	sparse = (sp_t *)malloc(nsparse*sizeof(sp_t));
	if (sparse == 0) {
		errmsg("Cannot alloc sparse buf.\n");
		return (i);
	}
	for (;;) {
		if (use_ai) {
#ifdef	_FIOAI
			if (fai_idx >= fai.fai_num) {
				fai.fai_off = pos;
				fai.fai_size = 512 * NFAI;
				fai.fai_num = NFAI;
			retry:
				ioctl(*fp, _FIOAI, &fai);
				if (fai.fai_num == 0) {
					/*
					 * Check whether we crossed the end of
					 * the file or off_t wrapps.
					 */
					if (pos < lseek(*fp, (off_t)0, SEEK_END)) {
						off_t	di;

						di = lseek(*fp, (off_t)0, SEEK_END);
						di -= pos;
						fai.fai_off = pos;
						fai.fai_size = (di/512) * 512;
						fai.fai_num = di/512;
						if (fai.fai_num != 0)
							goto retry;
						if (di > 0 && di < 512) {
							/*
							 * The last entry
							 * cannot be checked
							 * via the ooctl.
							 */
							fai.fai_size = 512;
							fai.fai_num = 1;
							fai.fai_daddr[0] = !_FIOAI_HOLE;
							goto fake;
						}
					}
					break;
				}
			fake:
				fai_idx = 0;
			}
			is_hole = fai.fai_daddr[fai_idx++] == _FIOAI_HOLE;

			amount = 512;
			if (pos + amount < 0)
				amount = info->f_size - pos;
			else if (pos + amount > info->f_size)
				amount = info->f_size - pos;
#else
			/* EMPTY */
#endif	/* _FIOAI */
		} else {
			if (cnt <= 0) {
				if ((cnt = _fileread(fp, rbuf, sizeof(rbuf))) == 0)
					break;
				if (cnt < 0) {
					errmsg(
					"Error scanning for holes in '%s'.\n",
					info->f_name);

					errmsg("Current pos: %lld\n",
					(Llong)lseek(*fp, (off_t)0, SEEK_CUR));

					free(sparse);
					return (0);
				}
				rbp = (char *)rbuf;
			}
			if ((amount = cmpnullbytes(rbp, cnt)) >= cnt) {
				is_hole = TRUE;
				cnt = 0;
			} else {
				amount = (amount / TBLOCK) * TBLOCK;
				if ((is_hole = amount != 0) == FALSE) {
					amount += TBLOCK;
					if (amount > cnt)
						amount = cnt;
				}
				rbp += amount;
				cnt -= amount;
			}
		}

		if (is_hole) {
			if (data) {
				sparse[i].sp_numbytes =
						pos - sparse[i].sp_offset;
				info->f_rsize += sparse[i].sp_numbytes;

				EDEBUG(("i: %d offset: %lld numbytes: %lld\n", i,
					(Llong)sparse[i].sp_offset,
					(Llong)sparse[i].sp_numbytes));

				data = FALSE;
				i++;
				if (i >= nsparse) {
					if ((sparse = grow_sp_list(sparse,
							&nsparse)) == 0) {
						lseek(*fp, (off_t)0, SEEK_SET);
						return (0);
					}
				}
			}
		} else {
			if (!data) {
				sparse[i].sp_offset = pos;
				data = TRUE;
			}
		}
		pos += amount;
	}
	EDEBUG(("data: %d\n", data));

	if (data) {
		sparse[i].sp_numbytes = pos - sparse[i].sp_offset;
		info->f_rsize += sparse[i].sp_numbytes;

		EDEBUG(("i: %d offset: %lld numbytes: %lld\n", i,
				(Llong)sparse[i].sp_offset,
				(Llong)sparse[i].sp_numbytes));
	} else {
		sparse[i].sp_offset = pos -1;
		sparse[i].sp_numbytes = 1;
		info->f_rsize += 1;

		EDEBUG(("i: %d offset: %lld numbytes: %lld\n", i,
				(Llong)sparse[i].sp_offset,
				(Llong)sparse[i].sp_numbytes));
	}
	lseek(*fp, (off_t)0, SEEK_SET);
	*spp = sparse;
	return (++i);
}

EXPORT int
gnu_skip_extended(ptb)
	TCB	*ptb;
{
	if (ptb->gnu_in_dbuf.t_isextended) do {
		if (readblock((char *)ptb) == EOF)
			return (EOF);
	} while(ptb->gnu_ext_dbuf.t_isextended);
	return (0);
}

EXPORT int
get_sparse(f, info)
	FILE	*f;
	FINFO	*info;
{
	fh_t	fh;
	sp_t	*sparse = get_sp_list(info);
	int	ret;

	if (sparse == 0) {
		errmsgno(EX_BAD, "Skipping '%s' sorry ...\n", info->f_name);
		errmsgno(EX_BAD, "WARNING: '%s' is damaged\n", info->f_name);
		void_file(info);
		return (FALSE);
	}
	fh.fh_file = f;
	fh.fh_name = info->f_name;
	fh.fh_size = info->f_rsize;
	fh.fh_newpos = (off_t)0;
	fh.fh_sparse = sparse;
	fh.fh_spindex = 0;
	ret = xt_file(info, vp_get_sparse_func, &fh, TBLOCK, "writing");
	free(sparse);
	return (ret);
}

EXPORT BOOL
cmp_sparse(f, info)
	FILE	*f;
	FINFO	*info;
{
	fh_t	fh;
	sp_t	*sparse = get_sp_list(info);

	if (sparse == 0) {
		errmsgno(EX_BAD, "Skipping '%s' sorry ...\n", info->f_name);
		void_file(info);
		fclose(f);
		return (FALSE);
	}
	fh.fh_file = f;
	fh.fh_name = info->f_name;
	fh.fh_size = info->f_rsize;
	fh.fh_newpos = (off_t)0;
	fh.fh_sparse = sparse;
	fh.fh_spindex = 0;
	fh.fh_diffs = 0;
	if (xt_file(info, vp_cmp_sparse_func, &fh, TBLOCK, "reading") < 0)
		die(EX_BAD);
	if (fclose(f) != 0)
		xstats.s_rwerrs++;
	free(sparse);
	return (fh.fh_diffs == 0);
}

EXPORT void
put_sparse(fp, info)
	int	*fp;
	FINFO	*info;
{
	fh_t	fh;
	sp_t	*sparse;
	int	nsparse;
	off_t	rsize;

	nsparse = mk_sp_list(fp, info, &sparse);
	if (nsparse == 0) {
		info->f_rsize = info->f_size;
		put_tcb(info->f_tcb, info);
		vprint(info);
		errmsgno(EX_BAD, "Dumping SPARSE '%s' as file\n", info->f_name);
		put_file(fp, info);
		return;
	}
	rsize = info->f_rsize;

	EDEBUG(("rsize: %lld\n", (Llong)rsize));

	put_sp_list(info, sparse, nsparse);
	fh.fh_file = (FILE *)fp;
	fh.fh_name = info->f_name;
	fh.fh_size = info->f_rsize = rsize;
	fh.fh_newpos = (off_t)0;
	fh.fh_sparse = sparse;
	fh.fh_nsparse = nsparse;
	fh.fh_spindex = 0;
	cr_file(info, vp_put_sparse_func, &fh, TBLOCK, "reading");
	free(sparse);
}

LOCAL void
put_sp_list(info, sparse, nsparse)
	FINFO	*info;
	sp_t	*sparse;
	int	nsparse;
{
	register int	i;
	register int	sparse_in_hdr = props.pr_sparse_in_hdr;
	register char	*p;
		 TCB	tb;
		 TCB	*ptb = info->f_tcb;
/*XXX*/extern long hdrtype;

	EDEBUG(("1nsparse: %d rsize: %lld\n", nsparse, (Llong)info->f_rsize));

	if (nsparse > sparse_in_hdr) {
		if ((props.pr_flags & PR_GNU_SPARSE_BUG) == 0)
			info->f_rsize +=
				(((nsparse-sparse_in_hdr)+SPARSE_EXT_HDR-1)/
				SPARSE_EXT_HDR)*TBLOCK;
	}
	EDEBUG(("2nsparse: %d rsize: %lld added: %d\n", nsparse, (Llong)info->f_rsize,
				(((nsparse-sparse_in_hdr)+SPARSE_EXT_HDR-1)/
				SPARSE_EXT_HDR)*TBLOCK));
	EDEBUG(("addr sp: %d\n", (int)((TCB *)0)->xstar_in_dbuf.t_sp));
	EDEBUG(("addr rs: %d\n", (int)((TCB *)0)->xstar_in_dbuf.t_realsize));
	EDEBUG(("flags: 0x%lX\n", info->f_flags));

	info->f_rxftype = info->f_xftype = XT_SPARSE;
	if (info->f_flags & F_SPLIT_NAME && props.pr_nflags & PR_PREFIX_REUSED)
		tcb_undo_split(ptb, info);
	info_to_tcb(info, ptb);

	if (H_TYPE(hdrtype) == H_GNUTAR)
		p = (char *)ptb->gnu_in_dbuf.t_sp;
	else
		p = (char *)ptb->xstar_in_dbuf.t_sp;
	for (i=0; i < sparse_in_hdr && i < nsparse; i++) {
		llitos(p, (Ullong)sparse[i].sp_offset, 11);   p += 12;
		llitos(p, (Ullong)sparse[i].sp_numbytes, 11); p += 12;
	}
	if (sparse_in_hdr > 0 && nsparse > sparse_in_hdr) {
		if (H_TYPE(hdrtype) == H_GNUTAR)
			ptb->gnu_in_dbuf.t_isextended = 1;
		else
			ptb->xstar_in_dbuf.t_isextended = '1';
	}

	put_tcb(ptb, info);
	vprint(info);

	nsparse -= sparse_in_hdr;
	sparse += sparse_in_hdr;
	ptb = &tb;
	while (nsparse > 0) {
		filltcb(ptb);
		p = (char *)ptb;
		for (i=0; i < SPARSE_EXT_HDR && i < nsparse; i++) {
			llitos(p, (Ullong)sparse[i].sp_offset, 11);   p += 12;
			llitos(p, (Ullong)sparse[i].sp_numbytes, 11); p += 12;
		}
		nsparse -= SPARSE_EXT_HDR;
		sparse += SPARSE_EXT_HDR;
		if (nsparse > 0)
			ptb->gnu_ext_dbuf.t_isextended = '1';
		writeblock((char *)ptb);
	}
}
