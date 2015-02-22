/****************************************************************************
 *
 * rg/pkg/include/rg_os.h
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
/* SYNC: rg/pkg/include/rg_os.h <-> project/tools/util/rg_os.h */

#ifndef _RG_OS_H_
#define _RG_OS_H_

#if USE_BASIC_HOST_CONFIG
#include "host_config.h"
#else
#include "rg_config.h"
#endif

#ifdef CONFIG_CC_FMTCHECK_ESC_SQL
#define __format_sql__(ARGS...) __format__(ARGS)
#else
#define __format_sql__(ARGS...)
#endif

#ifdef __TARGET__
  #if defined(CONFIG_RG_TARGET_LINUX)
    #define __OS_LINUX__
  #elif defined(CONFIG_RG_TARGET_VXWORKS)
    #define __OS_VXWORKS__
    #define __OS_BSD__ /* VxWorks is BSD compliant */
  #elif defined(CONFIG_RG_TARGET_ECOS)
    #define __OS_ECOS__
  #endif
#elif defined(__HOST__)
  #if defined(CONFIG_RG_HOST_LINUX)
    #define __OS_LINUX__
  #elif defined(CONFIG_RG_HOST_CYGWIN)
    #define __OS_CYGWIN__
  #endif
#else
  #error "__HOST__/__TARGET__ not defined"
#endif

#ifdef CONFIG_RG_TARGET_VXWORKS
#define CONFIG_RG_TARGET_BSD
#endif

#if defined(__OS_VXWORKS__) || defined(__OS_ECOS__)
#define __USERMODE_IN_KERNEL__
#endif

#if defined(__OS_VXWORKS__)
  #define RG_OS_CONSOLE "/tyCo/0"
#elif defined(__OS_LINUX__)
  #define RG_OS_CONSOLE "/dev/console"
#endif

#define RG_OS_O_BINARY 0

/* This file contains header files which should be included always for an OS.
 * For example, in case of VxWorks, it's always needed to include vxWorks.h
 */
#if defined(__OS_VXWORKS__)
  #include <vxWorks.h>
  #define __BYTE_ORDER _BYTE_ORDER
  #define __BIG_ENDIAN _BIG_ENDIAN
  #define __LITTLE_ENDIAN _LITTLE_ENDIAN
#elif defined(__OS_LINUX__)
  #ifdef __KERNEL__
    #include <asm/byteorder.h>
    /* Linux user-mode has __BYTE_ORDER defined, but the kernel does not */
    #ifdef CONFIG_CPU_LITTLE_ENDIAN
      #define __BYTE_ORDER __LITTLE_ENDIAN
    #elif defined(CONFIG_CPU_BIG_ENDIAN)
      #define __BYTE_ORDER __BIG_ENDIAN
    #else
      #error "Unable to define __BYTE_ORDER"
    #endif
  #else
    #include <endian.h>
  #endif
#endif

/* Quick hack: Currently, we only support x86 hosts, which are little endian.
 * This defined either LITTLE_ENDIAN_HOST or BIG_ENDIAN_HOST (not supported).
 */
#if defined(CONFIG_RG_HOST_LINUX) || defined(CONFIG_RG_HOST_CYGWIN)
  #define LITTLE_ENDIAN_HOST 1
#endif

#ifdef __TARGET__
  /* This defined either LITTLE_ENDIAN_TARGET or BIG_ENDIAN_TARGET */
  #if defined(CONFIG_CPU_LITTLE_ENDIAN)
    #define LITTLE_ENDIAN_TARGET 1
  #elif defined(CONFIG_CPU_BIG_ENDIAN)
    #define BIG_ENDIAN_TARGET 1
  #else
    #error "No endianess defined"
  #endif
#endif

#endif
