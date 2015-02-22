/****************************************************************************
 *
 * rg/pkg/util/rg_memory.h
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
/* SYNC: rg/pkg/util/rg_memory.h <-> project/tools/util/memory.h */

#ifndef _RG_MEMORY_H_
#define _RG_MEMORY_H_

#define OS_INCLUDE_STD
#include <os_includes.h>

void *memdup(void *ptr, size_t size);
void *memdup_e(void *ptr, size_t size);
void *memdup_l(void *ptr, size_t size);
void _zfree(void **p);

#define zfree(x) _zfree((void **) &(x))

#endif

