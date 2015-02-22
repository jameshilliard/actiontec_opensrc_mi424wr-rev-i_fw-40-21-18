/****************************************************************************
 *
 * rg/pkg/include/igmp_proxy_consts.h
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
#ifndef _IGMP_PROXY_CONSTS_H_
#define _IGMP_PROXY_CONSTS_H_

#define IGMP_PROXY_BASE			220
#define IGMP_PROXY_INIT			(IGMP_PROXY_BASE + 0)
#define IGMP_PROXY_UNINIT	        (IGMP_PROXY_BASE + 1)
#define IGMP_PROXY_WAN_IF_ADD		(IGMP_PROXY_BASE + 2)
#define IGMP_PROXY_LAN_IF_ADD		(IGMP_PROXY_BASE + 3)
#define IGMP_PROXY_LAN_PORT_ADD		(IGMP_PROXY_BASE + 4)
#define IGMP_PROXY_STATUS_GET		(IGMP_PROXY_BASE + 5)


#define DEFAULT_MAX_MEMBERSHIPS 256
#define DEFAULT_MAX_GASS_QUERIES 8
#define DEFAULT_MAX_REC_SOURCES 64
#define IGMP_UNSOL_INT 1

#define IGMP_PROXY_SET_CLIENT_UNSOL_INTERVAL    (IGMP_PROXY_BASE + 6)
#define IGMP_PROXY_GET_CLIENT_UNSOL_INTERVAL    (IGMP_PROXY_BASE + 7)

#define IGMP_PROXY_SET_QUERIER_COUNTERS         (IGMP_PROXY_BASE + 8)
#define IGMP_PROXY_GET_QUERIER_COUNTERS         (IGMP_PROXY_BASE + 9)

#define IGMP_PROXY_GET_CLIENT_VERSION	        (IGMP_PROXY_BASE + 10)

#define IGMP_PROXY_GET_CLIENT_GROUP_TABLE_SIZE	        (IGMP_PROXY_BASE + 12)
#define IGMP_PROXY_GET_CLIENT_GROUP_TABLE_NEXT_INDEX	(IGMP_PROXY_BASE + 13)
#define IGMP_PROXY_GET_CLIENT_GROUP_TABLE_ENTRY	        (IGMP_PROXY_BASE + 14)

#define IGMP_PROXY_GET_ROUTER_INTF_TABLE_SIZE	    (IGMP_PROXY_BASE + 15)
#define IGMP_PROXY_GET_ROUTER_INTF_TABLE_NEXT_INDEX	(IGMP_PROXY_BASE + 16)
#define IGMP_PROXY_GET_ROUTER_INTF_TABLE_ENTRY	(IGMP_PROXY_BASE + 17)

#define IGMP_PROXY_GET_CLIENT_GROUPSTATS_TABLE_SIZE	        (IGMP_PROXY_BASE + 18)
#define IGMP_PROXY_GET_CLIENT_GROUPSTATS_TABLE_NEXT_INDEX	(IGMP_PROXY_BASE + 19)
#define IGMP_PROXY_GET_CLIENT_GROUPSTATS_TABLE_ENTRY	    (IGMP_PROXY_BASE + 20)

#define IGMP_PROXY_GET_INCLUDE_SRC_TABLE_SIZE	    (IGMP_PROXY_BASE + 21)
#define IGMP_PROXY_GET_INCLUDE_SRC_TABLE_NEXT_INDEX	(IGMP_PROXY_BASE + 22)
#define IGMP_PROXY_GET_INCLUDE_SRC_TABLE_ENTRY	    (IGMP_PROXY_BASE + 23)

#define IGMP_PROXY_GET_EXCLUDE_SRC_TABLE_SIZE	    (IGMP_PROXY_BASE + 24)
#define IGMP_PROXY_GET_EXCLUDE_SRC_TABLE_NEXT_INDEX	(IGMP_PROXY_BASE + 25)
#define IGMP_PROXY_GET_EXCLUDE_SRC_TABLE_ENTRY	    (IGMP_PROXY_BASE + 26)

#define IGMP_PROXY_GET_ROUTER_HOST_TABLE_SIZE	    (IGMP_PROXY_BASE + 27)
#define IGMP_PROXY_GET_ROUTER_HOST_TABLE_NEXT_INDEX	(IGMP_PROXY_BASE + 28)
#define IGMP_PROXY_GET_ROUTER_HOST_TABLE_ENTRY	    (IGMP_PROXY_BASE + 29)

#define IGMP_PROXY_GET_CLIENT_GROUP_TABLE_MAXSIZE   (IGMP_PROXY_BASE + 30)
#define IGMP_PROXY_GET_ROUTER_HOST_TABLE_MAXSIZE    (IGMP_PROXY_BASE + 31)

#ifdef ACTION_TEC_IGMP_MCF
#define IGMP_PROXY_SET_MCF                          (IGMP_PROXY_BASE + 32)
#endif

#define IGMP_PROXY_GET_GUI_GROUP_ENTRY              (IGMP_PROXY_BASE + 33)
#define IGMP_PROXY_GET_GUI_GROUPSTAT_ENTRY          (IGMP_PROXY_BASE + 34)

#define IGMP_PROXY_GET_STATUS                       (IGMP_PROXY_BASE + 35)

#define BRDG_IGMP_SNOOPING_RECONF                   (IGMP_PROXY_BASE + 36)
#define BRDG_IGMP_SNOOPING_MCF                      (IGMP_PROXY_BASE + 37)
#define BRDG_GET_MC_GROUP_INFO                      (IGMP_PROXY_BASE + 38)
#define IGMP_PROXY_GET_FAST_LEAVE_STATUS            (IGMP_PROXY_BASE + 39)
#define IGMP_PROXY_SET_FAST_LEAVE_STATUS            (IGMP_PROXY_BASE + 40)

#ifdef ACTION_TEC_IGMP_MCF
#define IGMP_PROXY_UPDATE_HOST_MCF_ENTRY               (IGMP_PROXY_BASE + 41)
//#define IGMP_PROXY_DEL_HOST_MCF_ENTRY               (IGMP_PROXY_BASE + 42)
#define IGMP_PROXY_UPDATE_INTF_MCF_ENTRY               (IGMP_PROXY_BASE + 42)
//#define IGMP_PROXY_DEL_INTF_MCF_ENTRY               (IGMP_PROXY_BASE + 44)
#define IGMP_PROXY_UPDATE_SERV_MCF_ENTRY               (IGMP_PROXY_BASE + 43)
//#define IGMP_PROXY_DEL_SERV_MCF_ENTRY               (IGMP_PROXY_BASE + 46)
#define IGMP_PROXY_DEL_ALL_INTF_MCF_ENTRY              (IGMP_PROXY_BASE + 44)
#define IGMP_PROXY_UPDATE_ACL                          (IGMP_PROXY_BASE + 45)
#endif

#define IGMP_PROXY_GET_CLIENT_PERSISTENT_INTERVAL   (IGMP_PROXY_BASE + 46)
#define IGMP_PROXY_SET_CLIENT_PERSISTENT_INTERVAL   (IGMP_PROXY_BASE + 47)
#define IGMP_PROXY_END                              (IGMP_PROXY_BASE + 48)

#endif

