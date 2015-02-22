/* @(#)fifo.h	1.8 01/08/14 Copyright 1989 J. Schilling */
/*
 *	Definitions for a "fifo" that uses
 *	shared memory between two processes
 *
 *	Copyright (c) 1989 J. Schilling
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

typedef	struct	{
	BOOL	reading;	/* true if currently reading from tape	*/
	int	swapflg;	/* -1: init, 0: FALSE, 1: TRUE		*/
	int	volno;		/* Volume #				*/
	int	nblocks;	/* Blocksize for each transfer in TBLOCK*/
	long	blocksize;	/* Blocksize for each transfer in bytes	*/
	long	lastsize;	/* Size of last transfer (for backtape)	*/
	Llong	blocks;		/* Full blocks transfered on Volume	*/
	Llong	parts;		/* Bytes fom partial transferes on Volume */
	Llong	Tblocks;	/* Total blocks transfered		*/
	Llong	Tparts;		/* Total Bytes fom parttial transferes	*/
} m_stats;

typedef struct {
	char	*putptr;	/* put pointer within shared memory */
	char	*getptr;	/* get pointer within shared memory */
	char	*base;		/* base of fifo within shared memory segment*/
	char	*end;		/* end of real shared memory segment */
	int	size;		/* size of fifo within shared memory segment*/
	int	ibs;		/* input transfer size	*/
	int	obs;		/* output transfer size	*/
	unsigned long	icnt;	/* input count (incremented on each put) */
	unsigned long	ocnt;	/* output count (incremented on each get) */
	int	hiw;		/* highwater mark */
	int	low;		/* lowwater mark */
	int	flags;		/* fifo flags */
	int	gp[2];		/* sync pipe for get process */
	int	pp[2];		/* sync pipe for put process */
	int	puts;		/* fifo put count statistic */
	int	gets;		/* fifo get get statistic */
	int	empty;		/* fifo was empty count statistic */
	int	full;		/* fifo was full count statistic */
	int	maxfill;	/* max # of bytes in fifo */
	m_stats	stats;		/* statistics			*/
} m_head;

#define	gpin	gp[0]		/* get pipe in  */
#define	gpout	gp[1]		/* get pipe out */
#define	ppin	pp[0]		/* put pipe in  */
#define	ppout	pp[1]		/* put pipe out */

#define	FIFO_AMOUNT(p)	((p)->icnt - (p)->ocnt)

#define	FIFO_IBLOCKED	0x001	/* input  (put side) is blocked	*/
#define	FIFO_OBLOCKED	0x002	/* output (get side) is blocked	*/
#define	FIFO_FULL	0x004	/* fifo is full			*/
#define	FIFO_MEOF	0x008	/* EOF on input (put side)	*/
#define	FIFO_MERROR	0x010	/* error on input (put side)	*/
#define	FIFO_EXIT	0x020	/* exit() on non tape side	*/

#define	FIFO_IWAIT	0x200	/* input (put side) waits after first record */
#define	FIFO_I_CHREEL	0x400	/* change input tape reel if fifo gets empty */
#define	FIFO_O_CHREEL	0x800	/* change output tape reel if fifo gets empty*/

#if	!defined(HAVE_SMMAP) && !defined(HAVE_USGSHM) && !defined(HAVE_DOSALLOCSHAREDMEM)
#undef	FIFO			/* We cannot have a FIFO on this platform */
#endif
