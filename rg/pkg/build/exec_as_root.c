/****************************************************************************
 *
 * rg/pkg/build/exec_as_root.c
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
#include <unistd.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
	printf("Need one parameter at least\n");
	return -1;
    }

#ifndef __CYGWIN__
    if (setuid(0))
    {
	perror("Cannot setuid()");
	return 1;
    }

    if (setgid(0))
    {
	perror("Cannot setgid()");
	return 2;
    }
#endif

    execvp(argv[1], argv+1);
    perror(argv[1]);
    return 3;
}

