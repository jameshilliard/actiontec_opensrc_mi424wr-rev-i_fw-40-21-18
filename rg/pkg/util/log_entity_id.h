/****************************************************************************
 *
 * rg/pkg/util/log_entity_id.h
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

#ifndef _LOG_ENTITY_ID_H_
#define _LOG_ENTITY_ID_H_

/* Enumeration of separate log entities */
typedef enum {
    LOG_ENTITY_ALL = 0, /* All entities */
    LOG_ENTITY_JUTIL = 10,
    LOG_ENTITY_HTTP = 20,
    LOG_ENTITY_8021X = 30,
    LOG_ENTITY_ANTIVIRUS = 40,
    LOG_ENTITY_ATM = 50,
    LOG_ENTITY_AUTO_CONF = 60,
    LOG_ENTITY_SYSLOG = 70,
    LOG_ENTITY_XML = 80,
    LOG_ENTITY_X509 = 90,
    LOG_ENTITY_WGET = 100,
    LOG_ENTITY_WEB_SERVER = 110,
    LOG_ENTITY_TOD = 120,
    LOG_ENTITY_TFTPS = 130,
    LOG_ENTITY_QOS = 140,
    LOG_ENTITY_RIP = 150,
    LOG_ENTITY_SSH = 160,
    LOG_ENTITY_PERMST = 170,
    LOG_ENTITY_IPSEC = 180,
    LOG_ENTITY_IGMP = 190,
    LOG_ENTITY_FIREWALL = 200,
    LOG_ENTITY_WEB_FILTERING = 210,
    LOG_ENTITY_DSLHOME = 220,
    LOG_ENTITY_DHCP = 230,
    LOG_ENTITY_DDNS = 240,
    LOG_ENTITY_DNS = 250,
    LOG_ENTITY_BLUETOOTH = 260,
    LOG_ENTITY_DISK_MNG = 265,
    LOG_ENTITY_BACKUP = 270,
    LOG_ENTITY_PRINT_SERVER = 280,
    LOG_ENTITY_PPPOE_RELAY = 290,
    LOG_ENTITY_JNET = 300,
    LOG_ENTITY_ROUTE = 310,
    LOG_ENTITY_RMT_UPD = 320,
    LOG_ENTITY_FTP = 330,
    LOG_ENTITY_FILE_SERVER = 340,
    LOG_ENTITY_CABLEHOME = 350,
    LOG_ENTITY_KERBEROS = 360,
    LOG_ENTITY_BRIDGE = 370,
    LOG_ENTITY_RADIUS = 380,
    LOG_ENTITY_RMT_MNG = 390,
    LOG_ENTITY_WATCHDOG = 400,
    LOG_ENTITY_WIRELESS = 410,
    LOG_ENTITY_HW_SWITCH = 420,
    LOG_ENTITY_IPV6 = 430,
    LOG_ENTITY_WBM = 440,
    LOG_ENTITY_IMAGE = 450,
    LOG_ENTITY_WEB_CIFS = 460,
    LOG_ENTITY_CLI = 470,
    LOG_ENTITY_LPD = 480,
    LOG_ENTITY_MAIL_SERVER = 490,
    LOG_ENTITY_SNMP = 500,
    LOG_ENTITY_UPNP = 510,
    LOG_ENTITY_VOIP = 520,
    LOG_ENTITY_MAIN = 530,
    LOG_ENTITY_GENERIC_PROXY = 540,
    LOG_ENTITY_HSS = 550,
    LOG_ENTITY_MULTICAST_RELAY = 560,
    LOG_ENTITY_OPTION_MANAGER = 570,
    LOG_ENTITY_SSI = 580,
    LOG_ENTITY_VOATM = 590,
    LOG_ENTITY_VALGRIND = 600,
    LOG_ENTITY_PPP = 610,
    LOG_ENTITY_L2TPC = 620,
    LOG_ENTITY_L2TPS = 630,
    LOG_ENTITY_L2TP = 640,
    LOG_ENTITY_PPTPC = 650,
    LOG_ENTITY_PPTPS = 660,
    LOG_ENTITY_PPTP = 670,
    LOG_ENTITY_PPPOE_SERVER = 680,
    LOG_ENTITY_TUNNELS = 690,
    LOG_ENTITY_MAIL_CLIENT = 700,
    LOG_ENTITY_WEBCAM = 710,
    LOG_ENTITY_CLI_LOGIN = 720,
    LOG_ENTITY_WBM_LOGIN = 730,
    LOG_ENTITY_SYS_UPDOWN = 740,
    LOG_ENTITY_KERNEL = 750,
    LOG_ENTITY_NAT = 760,
    LOG_ENTITY_BT = 770,
    LOG_ENTITY_PASSWORD_CH = 780,
    LOG_ENTITY_USERNAME_CH = 781,
    LOG_ENTITY_PERMISSION_CH = 782,
    LOG_ENTITY_MEDIA_SERVER = 790,
    LOG_ENTITY_DIAG = 799,
    LOG_ENTITY_OTHER = 800, /* Other entities */
} log_entity_id_t;

#endif

