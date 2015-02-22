/****************************************************************************
 *
 * rg/pkg/include/rg_types.h
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

#ifndef _RG_TYPES_H_
#define _RG_TYPES_H_

#include <rg_os.h>

#if !(defined(__OS_LINUX__) && defined(__KERNEL__))
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
#ifndef HAVE_S64_TYPEDEF
#define HAVE_S64_TYPEDEF
typedef signed long long s64;
#endif
#ifndef HAVE_U64_TYPEDEF
#define HAVE_U64_TYPEDEF
typedef unsigned long long u64;
#endif
#endif

#ifdef __OS_VXWORKS__
typedef	unsigned char u_int8_t;
typedef	unsigned short int u_int16_t;
typedef	unsigned int u_int32_t;
typedef unsigned long int ulong;
typedef unsigned long long uint64_t;
#endif

/* Integer type which is big enough to hold pointer.
 * Note: u32 or int are not big enough for 64bit platforms.
 *
 * Use it for the following cases:
 * - variables which may be assigned to both pointer and numeric values
 * - for numeric operations on pointers.
 *
 * Use %ld/%lx for formatted input/output.
 *
 * TODO: Reconsider when adding 128bit platform support 
 */
typedef long addr_t;

#endif
