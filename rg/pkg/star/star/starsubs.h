
/* @(#)starsubs.h	1.24 02/05/20 Copyright 1996 J. Schilling */
/*
 *	Prototypes for star subroutines
 *
 *	Copyright (c) 1996 J. Schilling
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

#include <ccomdefs.h>

#ifndef	_INCL_SYS_TYPES_H
#include <sys/types.h>
#define	_INCL_SYS_TYPES_H
#endif

/*
 * star.c
 */
extern	int	main		__PR((int ac, char** av));
extern	const char *filename	__PR((const char* name));
extern	BOOL	match		__PR((const char* name));
extern	void	*__malloc	__PR((size_t size, char *msg));
extern	void	*__realloc	__PR((void *ptr, size_t size, char *msg));
extern	char	*__savestr	__PR((char *s));

/*
 * buffer.c
 */
extern	BOOL	openremote	__PR((void));
extern	void	opentape	__PR((void));
extern	void	closetape	__PR((void));
extern	void	changetape	__PR((void));
extern	void	nexttape	__PR((void));
extern	void	initbuf		__PR((int nblocks));
extern	void	markeof		__PR((void));
extern	void	syncbuf		__PR((void));
extern	int	readblock	__PR((char* buf));
extern	int	readtape	__PR((char* buf, int amount));
extern	void	filltcb		__PR((TCB *ptb));
extern	void	movetcb		__PR((TCB *from_ptb, TCB *to_ptb));
extern	void	*get_block	__PR((void));
extern	void	put_block	__PR((void));
extern	void	writeblock	__PR((char* buf));
extern	int	writetape	__PR((char* buf, int amount));
extern	void	writeempty	__PR((void));
extern	void	weof		__PR((void));
extern	void	buf_sync	__PR((void));
extern	void	buf_drain	__PR((void));
extern	int	buf_wait	__PR((int amount));
extern	void	buf_wake	__PR((int amount));
extern	int	buf_rwait	__PR((int amount));
extern	void	buf_rwake	__PR((int amount));
extern	void	buf_resume	__PR((void));
extern	void	backtape	__PR((void));
extern	int	mtioctl		__PR((int cmd, int count));
extern	off_t	mtseek		__PR((off_t offset, int whence));
extern	Llong	tblocks		__PR((void));
extern	void	prstats		__PR((void));
extern	BOOL	checkerrs	__PR((void));
extern	void	exprstats	__PR((int ret));
extern	void	excomerrno	__PR((int err, char* fmt, ...)) __printflike__(2, 3);
extern	void	excomerr	__PR((char* fmt, ...)) __printflike__(1, 2);
extern	void	die		__PR((int err));

/*
 * append.c
 */
extern	void	skipall		__PR((void));
extern	BOOL	update_newer	__PR((FINFO *info));

/*
 * create.c
 */
extern	void	checklinks	__PR((void));
extern	int	_fileopen	__PR((char *name, char *smode));
extern	int	_fileread	__PR((int *fp, void *buf, int len));
extern	void	create		__PR((char* name));
extern	void	createlist	__PR((void));
extern	BOOL	read_symlink	__PR((char* name, FINFO * info, TCB * ptb));
#ifdef	EOF
extern	void	put_file	__PR((int *fp, FINFO * info));
#endif
extern	void	cr_file		__PR((FINFO * info, int (*)(void *, char *, int), void *arg, int amt, char* text));

/*
 * diff.c
 */
extern	void	diff		__PR((void));
#ifdef	EOF
extern	void	prdiffopts	__PR((FILE * f, char* label, int flags));
#endif

/*
 * dirtime.c
 */
extern	void	sdirtimes	__PR((char* name, FINFO* info));
/*extern	void	dirtimes	__PR((char* name, struct timeval* tp));*/

/*
 * extract.c
 */
extern	void	extract		__PR((char *vhname));
extern	BOOL	newer		__PR((FINFO * info));
extern	BOOL	void_file	__PR((FINFO * info));
extern	int	xt_file		__PR((FINFO * info, int (*)(void *, char *, int), void *arg, int amt, char* text));
extern	void	skip_slash	__PR((FINFO * info));

/*
 * fifo.c
 */
#ifdef	FIFO
extern	void	initfifo	__PR((void));
extern	void	fifo_ibs_shrink	__PR((int newsize));
extern	void	runfifo		__PR((void));
extern	void	fifo_stats	__PR((void));
extern	int	fifo_amount	__PR((void));
extern	int	fifo_iwait	__PR((int amount));
extern	void	fifo_owake	__PR((int amount));
extern	void	fifo_oflush	__PR((void));
extern	int	fifo_owait	__PR((int amount));
extern	void	fifo_iwake	__PR((int amt));
extern	void	fifo_resume	__PR((void));
extern	void	fifo_sync	__PR((void));
extern	void	fifo_exit	__PR((void));
extern	void	fifo_chtape	__PR((void));
#endif

/*
 * header.c
 */
extern	int	get_hdrtype	__PR((TCB * ptb, BOOL isrecurse));
extern	int	get_compression	__PR((TCB * ptb));
extern	int	get_tcb		__PR((TCB * ptb));
extern	void	put_tcb		__PR((TCB * ptb, FINFO * info));
extern	void	write_tcb	__PR((TCB * ptb, FINFO * info));
extern	void	put_volhdr	__PR((char* name));
extern	BOOL	get_volhdr	__PR((FINFO * info, char *vhname));
extern	void	info_to_tcb	__PR((register FINFO * info, register TCB * ptb));
extern	int	tcb_to_info	__PR((register TCB * ptb, register FINFO * info));
extern	BOOL	ia_change	__PR((TCB * ptb, FINFO * info));
extern	void	stolli		__PR((register char* s, Ullong * ull));
extern	void	llitos		__PR((char* s, Ullong ull, int fieldw));

/*
 * xheader.c
 */
extern	void	xbinit		__PR((void));
extern	void	info_to_xhdr	__PR((FINFO * info, TCB * ptb));
extern	int	tcb_to_xhdr	__PR((TCB * ptb, FINFO * info));

/*
 * hole.c
 */
#ifdef	EOF
extern	int	get_forced_hole	__PR((FILE * f, FINFO * info));
extern	int	get_sparse	__PR((FILE * f, FINFO * info));
extern	BOOL	cmp_sparse	__PR((FILE * f, FINFO * info));
extern	void	put_sparse	__PR((int *fp, FINFO * info));
#endif
extern	int	gnu_skip_extended	__PR((TCB * ptb));

/*
 * lhash.c
 */
#ifdef	EOF
extern	void	hash_build	__PR((FILE * fp, size_t size));
#endif
extern	BOOL	hash_lookup	__PR((char* str));

/*
 * list.c
 */
extern	void	list	__PR((void));
extern	void	list_file __PR((register FINFO * info));
extern	void	vprint	__PR((FINFO * info));

/*
 * longnames.c
 */
extern	BOOL	name_to_tcb	__PR((FINFO * info, TCB * ptb));
extern	void	tcb_to_name	__PR((TCB * ptb, FINFO * info));
extern	void	tcb_undo_split	__PR((TCB * ptb, FINFO * info));
extern	int	tcb_to_longname	__PR((register TCB * ptb, register FINFO * info));
extern	void	write_longnames	__PR((register FINFO * info));

/*
 * names.c
 */
extern	BOOL	nameuid	__PR((char* name, int namelen, Ulong uid));
extern	BOOL	uidname	__PR((char* name, int namelen, Ulong* uidp));
extern	BOOL	namegid	__PR((char* name, int namelen, Ulong gid));
extern	BOOL 	gidname	__PR((char* name, int namelen, Ulong* gidp));

/*
 * props.c
 */
extern	void	setprops	__PR((long htype));
extern	void	printprops	__PR((void));

/*
 * remove.c
 */
extern	BOOL	remove_file	__PR((char* name, BOOL isfirst));

/*
 * star_unix.c
 */
extern	BOOL	getinfo		__PR((char* name, FINFO * info));
#ifdef	EOF
extern	void	checkarch	__PR((FILE *f));
#endif
extern	void	setmodes	__PR((FINFO * info));
extern	int	snulltimes	__PR((char* name, FINFO * info));
extern	int	sxsymlink	__PR((FINFO * info));
extern	int	rs_acctime	__PR((int fd, FINFO * info));

/*
 * acl_unix.c
 */
extern	BOOL	get_acls	__PR((FINFO *info));
extern	void	set_acls	__PR((FINFO *info));

/*
 * unicode.c
 */
extern	void	to_utf8		__PR((Uchar *to, Uchar *from));
extern	BOOL	from_utf8	__PR((Uchar *to, Uchar *from));

/*
 * fflags.c
 */
extern	void	get_fflags	__PR((FINFO *info));
extern	void	set_fflags	__PR((FINFO *info));
extern	char	*textfromflags	__PR((FINFO *info, char *buf));
extern	int	texttoflags	__PR((FINFO *info, char *buf));
