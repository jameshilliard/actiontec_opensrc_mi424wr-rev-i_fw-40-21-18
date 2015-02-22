/****************************************************************************
 *
 * rg/pkg/build/rg_gcc.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <rg_config.h>

#define MAX_LINE 256

#ifndef RG_GCC
#error RG_GCC must be define to a working compiler (including path if needed)
#endif

#ifndef GCC_PREFIX
#error GCC_PREFIX must be define to the gcc inlcude dir
#endif

#ifndef RGSRC
#error RGSRC must be define to rg_include dir
#endif

#ifndef BUILDDIR
#error BUILDDIR must be define to rg_include dir
#endif

void add_libc_includes(char **gcc_argv, int *i, int use_ulibc, int use_stdinc)
{
#if defined (CONFIG_ULIBC)
    if (use_ulibc)
    {
	if (use_stdinc)
	{
	    gcc_argv[(*i)++] = "-I" BUILDDIR "/pkg/build/ulibc/include";
	    gcc_argv[(*i)++] = "-I" BUILDDIR "/pkg/ulibc/include";
	}
	else
	{
	    /* The include path must lead to libc_config_int.h */
	    gcc_argv[(*i)++] = "-I" BUILDDIR "/pkg/build/ulibc/include/libc_rg";
	}
    }
#endif

#if defined (CONFIG_GLIBC)
    if (!use_ulibc)
    {
	if (use_stdinc)
	    gcc_argv[(*i)++] = "-I" BUILDDIR "/pkg/build/glibc/include";
	else
	{
	    /* The include path must lead to libc_config_int.h */
	    gcc_argv[(*i)++] = "-I" BUILDDIR "/pkg/build/glibc/include/libc_rg";
	}
    }
#endif
}


int main(int argc, char* argv[])
{
    char **gcc_argv, gcc_exec_cmd[] = RG_GCC;
    int i = 0, j = 0, retval;
	
    int debug = 0, linking = 1, use_static_linking = 0,
        use_stdinc = 1, use_start = 1, use_stdlib = 1, 
        remove_includes = 0, shared = 0,
	explicit_libc = 0, use_ulibc = 
/* default libc (if not specified in the command line) is ulibc (providing
 * CONFIG_ULIBC is set)
 */
#if defined(CONFIG_ULIBC)
	1;
#else
        0;
#endif

    int source_count = 0;
    char *a;

    if (argc==1)
    {
	fprintf(stderr, "gcc: No input files\n");
	exit(0);
    }

    gcc_argv = (char**)malloc(sizeof(char*)*(argc+100));
    
    /* since ccache may be in the gcc exec name - we need to split it */
    for (gcc_argv[j++] = strtok(gcc_exec_cmd, " ");
	(gcc_argv[j] = strtok(NULL, " ")); j++);

    for (i = 1; i < argc; i++)
    {
	a = argv[i];
	if (a[0] == '-')
	{
	    if (!strcmp(a, "-c") || !strcmp(a, "-S") || !strcmp(a, "-E") ||
		!strcmp(a, "-M") || !strcmp(a, "-r") || 
		!strncmp(a, "-print", 6))
	    {
		linking = 0;
	    }
	    else if (!strcmp(a, "-nostdinc"))
	    {
		use_stdinc = 0;
	    }
	    else if (!strcmp(a, "-nostartfiles"))
	    {
		use_start = 0;
	    }
	    else if (!strcmp(a, "-nodefaultlibs"))
	    {
		use_stdlib = 0;
	    }
	    else if (!strcmp(a, "-nostdlib"))
	    {
		use_start = 0;
		use_stdlib = 0;
	    }
	    /* It seems that for glibc we don't need to do anythig different 
	     * for static or dynamic linking. the flag use_static_linking is
	     * from ulibc gcc but it is not used. */
	    else if (!strcmp(a, "-static"))
	    {
		use_static_linking = 1;
	    }
	    else if (!strcmp(a, "-shared"))
	    {
		shared = 1;
	    }
	    else if (!strncmp(a, "-Wl,", 4))
	    {
		if (!strstr(a+4, "static"))
		{
		    use_static_linking = 1;
		}
	    }
	    else if (!strcmp(a, "-I-"))
	    {
		remove_includes = 1;
	    }
	    else if (!strcmp(a, "-v"))
	    {
		debug = 1;
	    }
	    else if (!strcmp(a, "--rg-use-ulibc"))
	    {
		use_ulibc = 1;
		explicit_libc = 1;
	    }
	    else if (!strcmp(a, "--rg-use-glibc"))
	    {
		use_ulibc = 0;
		explicit_libc = 1;
	    }
	}
	else
	    source_count++;
    }

    i = j;
    
    if (remove_includes)
    {
	fprintf(stderr, "*********************************************************\n");
	fprintf(stderr, "openrg gcc does not support -I- as a parameter\n");
	fprintf(stderr, "due to a use of -I to create the compiler defult includes\n");
	fprintf(stderr, "*********************************************************\n");
	exit(-1);
    }
    
    if (linking && explicit_libc)
    {
	fprintf(stderr, "%s: internal error, ulibc/glibc is irrelevant when linking\n",
	    argv[0]);
	exit(-1);
    }

    if (linking && source_count)
    {
	if (use_stdlib)
	    gcc_argv[i++] = "-nostdlib";

#if defined (CONFIG_ULIBC) && !defined(LIBC_IN_TOOLCHAIN)
	gcc_argv[i++] = "-Wl,--dynamic-linker,/lib/ld-uClibc.so.0";
	gcc_argv[i++] = "-Wl,-rpath-link," BUILDDIR "/pkg/build/lib";
#endif

	if (use_start)
	{
	    if (!shared)
		gcc_argv[i++] = BUILDDIR "/pkg/build/lib/crt1.o";
#if defined (CONFIG_GLIBC) || defined(LIBC_IN_TOOLCHAIN) || defined(CONFIG_UCLIBCXX)
	    gcc_argv[i++] = BUILDDIR "/pkg/build/lib/crti.o";
	    gcc_argv[i++] = GCC_PREFIX "/crtbegin.o";
#endif
	}
    }   

    if (explicit_libc)
	add_libc_includes(gcc_argv, &i, use_ulibc, use_stdinc);

    for (j=1; j<argc; j++)
    {
	/* flags starting with "--rg-" are internal and not passed to gcc */
	if (strncmp(argv[j], "--rg-", 5))
	    gcc_argv[i++] = argv[j];
    }
    
    if (linking && source_count)
    {
	if (use_stdlib)	
	{
	    gcc_argv[i++] = "-L" BUILDDIR "/pkg/build/lib/";
	    gcc_argv[i++] = "-lgcc";
	    gcc_argv[i++] = "-lc";
	    gcc_argv[i++] = "-lgcc";
#if defined (CONFIG_FEROCEON) && defined (CONFIG_AEABI)
	    gcc_argv[i++] = "-lgcc_eh";
#endif
	}
	if (use_start)
	{
#if defined (CONFIG_GLIBC) || defined(LIBC_IN_TOOLCHAIN) || defined(CONFIG_UCLIBCXX)
	    gcc_argv[i++] = GCC_PREFIX "/crtend.o";
	    gcc_argv[i++] = BUILDDIR "/pkg/build/lib/crtn.o";
#endif
	}
    }    

    if (use_stdinc)
    {	    
	gcc_argv[i++] = "-nostdinc";
	gcc_argv[i++] = "-I" GCC_PREFIX "/include/";
	gcc_argv[i++] = "-I" RGSRC "/pkg/include/";
	gcc_argv[i++] = "-I" BUILDDIR "/pkg/include/";
    }

    if (!explicit_libc)
	/* use libc as the last resort */
	add_libc_includes(gcc_argv, &i, use_ulibc, use_stdinc);


    if (debug)
    {
	for (j=0; j<i; j++)
	    fprintf(stderr, "argv[%d]=%s\n", j, gcc_argv[j]);
    }
	    
    gcc_argv[i] = NULL;
    retval = execvp(gcc_argv[0], gcc_argv);

    /* We should never get to here */
    free(gcc_argv); /* Just to make every one happy */

    perror("execvp");
    fprintf(stderr, "can't execute %s. execvp return %d\n", RG_GCC, retval);
    exit(retval);
}
