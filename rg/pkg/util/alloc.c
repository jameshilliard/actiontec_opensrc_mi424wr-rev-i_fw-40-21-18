/****************************************************************************
 *
 * rg/pkg/util/alloc.c
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
/* SYNC: rg/pkg/util/alloc.c <-> project/tools/util/alloc.c */

#define OS_INCLUDE_STD
#define OS_INCLUDE_IO
#include <os_includes.h>

#include "alloc.h"
#include "rg_error.h"

void *malloc_log(rg_error_level_t level, size_t size)
{
    void *mem;

    /* uClibc crashes on malloc(0) */
    if (!size)
	size = 4;

    mem = malloc(size);
    if (!mem)
	rg_error(level, "malloc_e(%zd): failed", size);
    return mem;
}

void *zalloc_log(rg_error_level_t level, size_t size)
{
    void *mem;

    if (!(mem = zalloc(size)))
	rg_error(level, "zalloc_e(%zd): failed", size);
    return mem;
}

void *realloc_log(rg_error_level_t level, void *p, size_t size)
{
    void *mem;

    /* uClibc crashes on realloc(p, 0) */
    if (!size)
	size = 4;

    if (!(mem = realloc(p, size)))
	rg_error(level, "realloc_e(%zd): failed", size);
    return mem;
}

