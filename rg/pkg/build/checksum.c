/****************************************************************************
 *
 * rg/pkg/build/checksum.c
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

int big_endian = 0;
static unsigned long htocl(unsigned long x)
{
       if (big_endian)
	       return ((x << 24) & 0xff000000) |
                      ((x <<  8) & 0x00ff0000) |
                      ((x >>  8) & 0x0000ff00) |
                      ((x >> 24) & 0x000000ff);
       return x;
}

void usage(int i)
{
    printf("usage: checksum filename b/l\n");
    exit(i);
}

int main(int argc, char **argv)
{
    FILE *f = NULL;
    unsigned int val = 0;
    int i;
    
    if (argc != 3)
	usage(-1);

    switch (argv[2][0])
    {
	case 'b':
	    big_endian = 1;
	    break;
	case 'l':
	    big_endian = 0;
	    break;
	default:
	    usage(-2);
	    break;
    }
    
    f = fopen(argv[1], "r");
    if (!f)
    {
	printf("can't open file %s\n", argv[1]);
	exit(-1);
    }

    while ((i=fgetc(f)) != -1)
	val += i;

    val = htocl(val);
    write(1, &val, sizeof(val));
    
    return 0;
}
