/* @(#)avoffset.c	1.15 00/05/07 Copyright 1987 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)avoffset.c	1.15 00/05/07 Copyright 1987 J. Schilling";
#endif
/*
 * This program is a tool to generate the file "avoffset.h".
 * It is used by functions that trace the stack to get to the top of the stack.
 *
 * It generates two defines:
 *	AV_OFFSET	- offset of argv relative to the main() frame pointer
 *	FP_INDIR	- number of stack frames above main()
 *			  before encountering a NULL pointer.
 *
 *	Copyright (c) 1987 J. Schilling
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
#include <schily.h>
#include <stdxlib.h>
#include <signal.h>

#ifdef	NO_SCANSTACK
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif

#ifdef	HAVE_SCANSTACK
#	include <stkframe.h>
#endif

LOCAL	RETSIGTYPE handler __PR((int signo));
EXPORT	int	main	__PR((int ac, char** av));

LOCAL	RETSIGTYPE
handler(signo)
	int	signo;
{
	fprintf(stderr, "Warning: Cannot scan stack on this environment.\n");
	exit(0);
}


int main(ac, av)
	int	ac;
	char	**av;
{
#ifdef	HAVE_SCANSTACK
	register struct frame *fp = (struct frame *)getfp();
	register int	i = 0;
#endif

	signal(SIGBUS, handler);
	signal(SIGSEGV, handler);

	printf("/*\n");
	printf(" * This file has been generated automatically\n");
	printf(" * by %s\n", sccsid);
	printf(" * do not edit by hand.\n");
	printf(" *\n");
	printf(" * This file includes definitions for AV_OFFSET and FP_INDIR.\n");
	printf(" * FP_INDIR is the number of fp chain elements above 'main'.\n");
	printf(" * AV_OFFSET is the offset of &av[0] relative to the frame pointer in 'main'.\n");
	printf(" *\n");
	printf(" * If getav0() does not work on a specific architecture\n");
	printf(" * the program which generated this include file may dump core.\n");
	printf(" * In this case, the generated include file does not include\n");
	printf(" * definitions for AV_OFFSET and FP_INDIR but ends after this comment.\n");
	printf(" * If AV_OFFSET or FP_INDIR are missing in this file, all programs\n");
	printf(" * which use the definitions are automatically disabled.\n");
	printf(" */\n");
	fflush(stdout);

#ifdef	HAVE_SCANSTACK
	while (fp->fr_savfp) {
		fp = (struct frame *)fp->fr_savfp;
		if (fp->fr_savpc == 0)
			break;
		i++;
	}
	/*
	 * Do not add any printf()'s before this line to allow avoffset
	 * to abort without printing more than the comment above.
	 */
	fp = (struct frame *)getfp();
	printf("#define	AV_OFFSET	%d\n", (int)(av-(char **)fp));
	printf("#define	FP_INDIR	%d\n", i);
#endif
	exit(0);
	return (0);	/* keep lint happy */
}
