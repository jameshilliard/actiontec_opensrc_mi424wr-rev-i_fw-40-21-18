/****************************************************************************
 *
 * rg/pkg/util/util.h
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
/* SYNC: rg/pkg/util/util.h <-> project/tools/util/util.h */

#ifndef _UTIL_H_
#define _UTIL_H_

#define OS_INCLUDE_STRING /* for memset */
#define OS_INCLUDE_PARAM /* for MIN/MAX */
#include <os_includes.h>

#define MAX_PATH 1024
#define MAX_LINE 1024

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#endif

#ifndef MZERO
#define MZERO(x) memset(&(x), 0, sizeof(x))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*(x)))
#endif

#define DEFAULT_PASSWORD "{[(+-)]}"

#endif

