/****************************************************************************
 *
 * rg/pkg/util/rg_memory.c
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
/* SYNC: rg/pkg/util/rg_memory.c <-> project/tools/util/memory.c */

#define OS_INCLUDE_STD
#define OS_INCLUDE_IO
#include <os_includes.h>

#include "rg_memory.h"
#include "rg_error.h"

/* Note this code is GPL Licence, so do not add any new 
 * openrg incude files here. Talk to TL or SA before any change
 */

void _zfree(void **p)
{
    if (!*p)
	return;
    free(*p);
    *p = NULL;
}
	
void *memdup_log(rg_error_level_t severity, void *ptr, int len)
{
    void *dst;

    if (!(dst = memdup(ptr, len)))
	rg_error(severity, "memdup(buf %p len %d) failed", ptr, len);
    return dst;
}

void *memdup_l(void *ptr, size_t size)
{
    return memdup_log(LERR, ptr, size);
}

void *memdup_e(void *ptr, size_t size)
{
    return memdup_log(LEXIT, ptr, size);
}

void *memdup(void *ptr, size_t size)
{
    void *dst;
    if (!(dst = malloc(size)))
	return NULL;
    memcpy(dst, ptr, size);
    return dst;
}

