/****************************************************************************
 *
 * rg/pkg/include/kos_chardev_id.h
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

#ifndef _KOS_CHARDEV_ID_H_
#define _KOS_CHARDEV_ID_H_

typedef enum {
    KOS_CDT_DHCP = 1,
    KOS_CDT_BRIDGE = 2,
    KOS_CDT_IPF = 3,
    KOS_CDT_IPF_NAT = 4,
    KOS_CDT_IPF_STATE = 5,
    KOS_CDT_IPV4 = 7,
    KOS_CDT_BROUTE = 8,
    KOS_CDT_USFS = 9,
    KOS_CDT_CHWAN = 10,
    KOS_CDT_LOG = 11,
    KOS_CDT_KNET_IF_EXT = 12,
    KOS_CDT_RAUL = 13,
    KOS_CDT_ARPRESD = 14,
    KOS_CDT_KLEDS = 15,
    KOS_CDT_HW_BUTTONS = 16,
    KOS_CDT_AUTH1X = 17,
    KOS_CDT_KCALL = 18,
    KOS_CDT_PKTCBL = 19,
    KOS_CDT_NETBIOS_RT = 20,
    KOS_CDT_JFW = 21,
    KOS_CDT_WATCHDOG = 22,
    KOS_CDT_PPPOE_RELAY = 23,
    KOS_CDT_KNET_COUNTERS = 24,
    KOS_CDT_PPP = 25,
    KOS_CDT_PPPOE_BACKEND = 26,
    KOS_CDT_PPPCHARDEV_BACKEND = 27,
    KOS_CDT_MCAST_RELAY = 28,
    KOS_CDT_LIC = 29,
    KOS_CDT_ROUTE = 30,
    KOS_CDT_RTP = 31,
    KOS_CDT_VOIP_SLIC = 32,
    KOS_CDT_RGLDR = 33,
    KOS_CDT_AR531X = 34,
    KOS_CDT_WDS_CONN_NOTIFIER = 35,
    KOS_CDT_PPTP_BACKEND = 36,
    KOS_CDT_PPPOH_BACKEND = 37,
    KOS_CDT_HSS = 38,
    KOS_CDT_MSS = 39,
    KOS_CDT_VOIP_HWEMU = 40,
    KOS_CDT_VOIP_DSP = 41,
    KOS_CDT_COMP_REG = 42,
    KOS_CDT_M88E6021_SWITCH = 43,
    KOS_CDT_JTIMER = 44,
    KOS_CDT_QOS_INGRESS = 45,
    KOS_CDT_BCM53XX_SWITCH = 46,
    KOS_CDT_FW_STATS = 47,
    KOS_CDT_STP = 48,
    KOS_CDT_FASTPATH = 49,
    KOS_CDT_HW_QOS = 50,
    KOS_CDT_SKB_CACHE = 51,
    KOS_CDT_KS8995M_SWITCH = 52,
    KOS_CDT_SWITCH_PORT_DEV = 53,
    KOS_CDT_MV88E60XX_SWITCH = 54,
#ifdef ACTION_TEC
    KOS_CDT_IGMP = 55,
#endif /* ACTION_TEC */
} kos_chardev_type_t;

#endif

