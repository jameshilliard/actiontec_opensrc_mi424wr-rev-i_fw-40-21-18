/****************************************************************************
 *
 * rg/pkg/include/libc_config_include.h
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

#ifndef _LIBC_CONFIG_INCLUDE_H_ 
#define _LIBC_CONFIG_INCLUDE_H_

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void


/* Define to the type of elements in the array set by `getgroups'.
   Usually this is either `int' or `gid_t'.  */
#define GETGROUPS_T gid_t
#if defined(GLIBC_VERSION_2_1_3)
	#define RLIMTYPE rlim_t
#endif
/* Define to `short' if <sys/types.h> doesn't define.  */
#define bits16_t short

/* Define to `unsigned short' if <sys/types.h> doesn't define.  */
#define u_bits16_t unsigned short

/* Define to `int' if <sys/types.h> doesn't define.  */
#define bits32_t int

/* Define to `unsigned int' if <sys/types.h> doesn't define.  */
#define u_bits32_t unsigned int

/* Define to `double' if <sys/types.h> doesn't define. */
#define bits64_t double

/* ucd-snmp */
/* define rtentry to ortentry on SYSV machines (alphas) */
#define RTENTRY struct rtentry

#endif
