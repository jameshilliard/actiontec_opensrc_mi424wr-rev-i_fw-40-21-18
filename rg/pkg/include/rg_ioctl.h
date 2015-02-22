/****************************************************************************
 *
 * rg/pkg/include/rg_ioctl.h
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

#ifndef _RG_IOCTL_H_
#define _RG_IOCTL_H_

#ifndef __NO_INCLUDES__
#include <rg_os.h>
#ifdef __KERNEL__
#if defined(__OS_LINUX__)
#include <linux/ioctl.h>
#elif defined(__OS_VXWORKS__)
#include <sys/ioctl.h>
#else
#error "Unknown kernel"
#endif
#else
#include <sys/ioctl.h>
#endif
#endif

#define RG_IOCTL(type, nr) ((type) << 8 | (nr))

/* This is the start of a free window of IOCTL prefixes for ALL RG ioctls.
 * for Linux OS generic ioctls (not RG related) see [1] for prefixes.
 */
#define RG_IOCTL_PREFIX_BASE 0xD0

#define RG_IOCTL_PREFIX_DHCP (RG_IOCTL_PREFIX_BASE + 0)
#define RG_IOCTL_PREFIX_LOG (RG_IOCTL_PREFIX_BASE + 1)
#define RG_IOCTL_PREFIX_FW (RG_IOCTL_PREFIX_BASE + 2)
#define RG_IOCTL_PREFIX_BRIDGE (RG_IOCTL_PREFIX_BASE + 3)
#define RG_IOCTL_PREFIX_CHWAN (RG_IOCTL_PREFIX_BASE + 4)
#define RG_IOCTL_PREFIX_USFS (RG_IOCTL_PREFIX_BASE + 5)
#define RG_IOCTL_PREFIX_RAUL (RG_IOCTL_PREFIX_BASE + 6)
#define RG_IOCTL_PREFIX_AUTH1X (RG_IOCTL_PREFIX_BASE + 7)
#define RG_IOCTL_PREFIX_KOS (RG_IOCTL_PREFIX_BASE + 8)
#define RG_IOCTL_PREFIX_PKTCBL (RG_IOCTL_PREFIX_BASE + 9)
#define RG_IOCTL_PREFIX_NB_ROUTE (RG_IOCTL_PREFIX_BASE + 10)
#define RG_IOCTL_PREFIX_JFW (RG_IOCTL_PREFIX_BASE + 11)
#define RG_IOCTL_PREFIX_PPPOE_RELAY (RG_IOCTL_PREFIX_BASE + 12)
#define RG_IOCTL_PREFIX_PPP (RG_IOCTL_PREFIX_BASE + 13)
#define RG_IOCTL_PREFIX_PPPOE (RG_IOCTL_PREFIX_BASE + 14)
#define RG_IOCTL_PREFIX_MCAST_RELAY (RG_IOCTL_PREFIX_BASE + 15)
#define RG_IOCTL_PREFIX_LIC (RG_IOCTL_PREFIX_BASE + 16)
#define RG_IOCTL_PREFIX_RTP (RG_IOCTL_PREFIX_BASE + 17)
#define RG_IOCTL_PREFIX_VOIP (RG_IOCTL_PREFIX_BASE + 18)
#define RG_IOCTL_PREFIX_ARPRESD (RG_IOCTL_PREFIX_BASE + 19)
#define RG_IOCTL_PREFIX_ROUTE (RG_IOCTL_PREFIX_BASE + 20)
#define RG_IOCTL_PREFIX_RGLDR (RG_IOCTL_PREFIX_BASE + 21)
#define RG_IOCTL_PREFIX_AR531X (RG_IOCTL_PREFIX_BASE + 22)
#define RG_IOCTL_PREFIX_PPTP (RG_IOCTL_PREFIX_BASE + 23)
#define RG_IOCTL_PREFIX_PPPOH (RG_IOCTL_PREFIX_BASE + 24)
#define RG_IOCTL_PREFIX_HSS (RG_IOCTL_PREFIX_BASE + 25)
#define RG_IOCTL_PREFIX_MSS (RG_IOCTL_PREFIX_BASE + 26)
#define RG_IOCTL_PREFIX_VOIP_HWEMU (RG_IOCTL_PREFIX_BASE + 27)
#define RG_IOCTL_PREFIX_COMP_REG (RG_IOCTL_PREFIX_BASE + 28)
#define RG_IOCTL_PREFIX_M88E6021_SWITCH (RG_IOCTL_PREFIX_BASE + 29)
#define RG_IOCTL_PREFIX_WATCHDOG_TUTORIAL (RG_IOCTL_PREFIX_BASE + 30)
#define RG_IOCTL_PREFIX_JTIMER (RG_IOCTL_PREFIX_BASE + 31)
#define RG_IOCTL_PREFIX_QOS_INGRESS (RG_IOCTL_PREFIX_BASE + 32)
#define RG_IOCTL_PREFIX_BCM53XX_SWITCH (RG_IOCTL_PREFIX_BASE + 33)
#define RG_IOCTL_PREFIX_FASTPATH (RG_IOCTL_PREFIX_BASE + 34)
#define RG_IOCTL_PREFIX_HW_QOS (RG_IOCTL_PREFIX_BASE + 35)
#define RG_IOCTL_PREFIX_SKB_CACHE (RG_IOCTL_PREFIX_BASE + 36)
#define RG_IOCTL_PREFIX_KS8995M_SWITCH (RG_IOCTL_PREFIX_BASE + 37)
#define RG_IOCTL_PREFIX_SWITCH_PORT_DEV (RG_IOCTL_PREFIX_BASE + 38)
#define RG_IOCTL_PREFIX_MV88E60XX_SWITCH (RG_IOCTL_PREFIX_BASE + 39)

#ifdef ACTION_TEC
#define RG_IOCTL_PREFIX_IGMP (RG_IOCTL_PREFIX_BASE + 40)
#define RG_IOCTL_PREFIX_LOG_AGGR (RG_IOCTL_PREFIX_BASE + 41)
#endif /* ACTION_TEC */
/* add here additional prefixes (modules) for ioctls */

#endif
