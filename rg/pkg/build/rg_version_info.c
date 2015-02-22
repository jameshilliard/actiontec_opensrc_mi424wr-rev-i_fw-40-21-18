/****************************************************************************
 *
 * rg/pkg/build/rg_version_info.c
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

#include <rg_version.h>
#define OS_INCLUDE_STD
#include <os_includes.h>

void usage(char *name)
{
    printf("USAGE:\n"
	"%s <-c|e|f>"
	"\n\nWhere:\n"
	"  -e: prints external version\n" 
	"  -f: prints RG version + external version\n" 
	"  -c: prints RG version\n", name);
    exit(1);
}

int main(int argc, char *argv[])
{
    int c;

    if (argc < 2)
	usage(argv[0]);

    while ((c = getopt(argc, argv, "cef")) != -1)
    {
	switch (c)
	{
	case 'c':
	    printf("%s", rg_version);
	    break;
	case 'e':
	    printf("%s", external_version);
	    break;
	case 'f':
	    printf("%s%s%s", rg_version,
		strlen(external_version) ? "."  : "", external_version);
	    break;
	default:
	    usage(argv[0]);
	    break;
	}
    }
    return 0;
}
