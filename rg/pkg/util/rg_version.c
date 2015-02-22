/****************************************************************************
 *
 * rg/pkg/util/rg_version.c
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
/* SYNC: rg/pkg/util/rg_version.c <-> project/tools/util/rg_version.c */

#include <rg_version.h>
#define OS_INCLUDE_STD
#include <os_includes.h>
#include "str.h"

#define MAX_VERSION_BYTES 80

#define RG_OLD_VERSION_MAJOR_GET(ver) ((ver) / 10000)
#define RG_OLD_VERSION_MINOR_GET(ver) (((ver) / 100 ) % 100)
#define RG_OLD_VERSION_REVISION_GET(ver) ((ver) % 100)

char *rg_version = RG_VERSION;
char *external_version = EXTERNAL_VERSION;

char *rg_full_version_to_str(char *rg_ver, char *ext_ver)
{
    static char str[MAX_VERSION_BYTES + 1];
    int size = 0;
 
    size = snprintf(str, sizeof(str) - 1, "%s", rg_ver); 
    if (ext_ver && *ext_ver) 
	snprintf(str + size, sizeof(str) - size - 1 , ".%s", ext_ver);
    
    return str;
}

#define SKIP_DOT(p) \
    if (*p == '.') p++; \
    else if (*p) return -2
	
int rg_version_is_valid(char *p)
{
    int len = 0;
    
    while (*p)
    {
	int i;
	
	/* we do not allow leading zeros */
	if (*p == '0' && isdigit(*(p+1)))
	    return 0;
	if (!isdigit(*p))
	    return 0;
	i = strtol(p, &p, 10);
	if (i > 999)
	    return 0;
	len++;

	if (*p && *p++ != '.')
	    return 0;
    }
    return len >= 3;
}

static int version_is_nonzero(char *p)
{
    while (*p) 
    {
	if (strtol(p, &p, 10))
	    return 1;
	SKIP_DOT(p);
    }
    return 0;
}

static int rg_version_compare_int(char *p, char *q, 
    int *only_last_digit_differs)
{
    while (*p && *q)
    {
	int pn = strtol(p, &p, 10);
	int qn = strtol(q, &q, 10);

	*only_last_digit_differs = !*p && !*q;
	if (pn != qn) 
	    return (pn > qn) ? 1 : -1;
	SKIP_DOT(p);
	SKIP_DOT(q);
    }
    if (*p && version_is_nonzero(p))
	return 1;
    if (*q && version_is_nonzero(q))
	return -1;
    return 0;
}

int rg_version_compare(char *p, char *q)
{
    int dummy;

    return rg_version_compare_int(p, q, &dummy);
}

int rg_version_new2old(char *p)
{
    int i, r=0;

    for (i=0; i<3; i++)
    {
	r = 100 * r + strtol(p, &p, 10);
	SKIP_DOT(p);
    }
    return r;
}

char *rg_version_old2new(int v)
{
    static char buf[12];

    sprintf(buf, "%d.%d.%d", RG_OLD_VERSION_MAJOR_GET(v),
	RG_OLD_VERSION_MINOR_GET(v), RG_OLD_VERSION_REVISION_GET(v));
    return buf;
}

int rg_version_is_old(char *p)
{
    return RG_OLD_VERSION_MAJOR_GET(atoi(p));
}

/* return 1 iff v1 and v2 are the same, or only differ in the last digit */
int rg_version_is_same_branch(char *p, char *q)
{
    int last;

    return !rg_version_compare_int(p, q, &last) || last;
}

int rg_version_length(char *p)
{
    int len = 0;

    while (*p)
    {
	strtol(p, &p, 10);
	SKIP_DOT(p);
	len++;
    }
    return len;
}

