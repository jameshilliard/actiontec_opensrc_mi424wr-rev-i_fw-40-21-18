/****************************************************************************
 *
 * rg/pkg/build/dist_config.c
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

#include <string.h>
#include "config_opt.h"
#include "create_config.h"
#include <sys/stat.h>
#include <unistd.h>

/* ACTION_TEC */
/* 
 * These are the fefault values for the interface names. The
 * distribution specific interface names must be set in
 * distribution_features()
 */
char *act_lan_eth_ifname;
char *act_wan_eth_ifname;
char *act_lan_moca_ifname;
char *act_wan_moca_ifname;
char *act_wan_eth_pppoe_ifname;
char *act_wan_moca_pppoe_ifname;
char *act_wifi_ap_ifname;
char *act_atheros_vap_primary_ifname;
char *act_atheros_vap_secondary_ifname;
char *act_atheros_vap_public_ifname;
char *act_atheros_vap_help_ifname;
char *act_ralink_vap_ifname;
char *act_def_bridge_ifname;
/* ACTION_TEC */

static int stat_lic_file(char *path)
{
    struct stat s;
    int ret = stat(path, &s);

    printf("Searching for license file in %s: %sfound\n", path,
	ret ? "not " : "");
    return ret;
}

static void set_jnet_server_configs(void)
{
    token_set_y("CONFIG_RG_HTTPS");
    token_set_y("CONFIG_RG_SSL");
    token_set_y("CONFIG_RG_OPENSSL_MD5");
    token_set_y("CONFIG_RG_XML");
    token_set_y("CONFIG_RG_DSLHOME");
    token_set_y("CONFIG_RG_WGET");
    token_set_y("CONFIG_LOCAL_WBM_LIB");
    token_set_y("CONFIG_RG_SESSION_LIBDB");
    token_set_y("HAVE_MYSQL");
    token_set_y("CONFIG_RG_JNET_SERVER");
    token_set("CONFIG_RG_JPKG_DIST", "JPKG_LOCAL_I386");
    token_set("TARGET_MACHINE", "local_i386");
    token_set_y("CONFIG_RG_LANG");
    token_set_y("CONFIG_RG_GNUDIP");
    token_set_y("CONFIG_GLIBC");
    token_set_y("CONFIG_RG_LIBIMAGE_DIM");
}


static void set_hosttools_configs(void)
{
    /***************************************************************************
    * this function created due to bug B32553 . until the bug will fix         *
    * we need to seperate the configs used by the HOSTTOOLS dist to 2 sets     *
    * 1 - contain CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY which can turn on by the  *
    * dist         but not by the jpkg                                         * 
    * 2 - contain configs which should also turn on by the JPKG                *
    ***************************************************************************/
    token_set_y("CONFIG_RG_ZLIB");
    token_set_y("CONFIG_RG_TOOLS");
}

char *set_dist_license(void)
{
#define DEFAULT_LIC_DIR "pkg/license/licenses/"
#define INSTALL_LIC_DIR "pkg/jpkg/install/"
#define DEFAULT_LIC_FILE "license.lic"
    char *lic = NULL;

    if (IS_DIST("RTA770W"))
	lic = DEFAULT_LIC_DIR "belkin.lic";
    else if (IS_DIST("MI424WR") || IS_DIST("RI408") || IS_DIST("VI414WG") ||
	IS_DIST("VI414WG_ETH") || IS_DIST("KI414WG") ||
	IS_DIST("KI414WG_ETH") || IS_DIST("BA214WG") ||
	IS_DIST("RGLOADER_MI424WR") || IS_DIST("RGLOADER_RI408"))
    {
	lic = INSTALL_LIC_DIR "jpkg_actiontec.lic";
    }
    else if (IS_DIST("MC524WR"))
	lic = INSTALL_LIC_DIR "jpkg_actiontec_oct.lic";
    else if (IS_DIST("FEROCEON"))
	lic = INSTALL_LIC_DIR "jpkg_actiontec_mv.lic";
    else if (IS_DIST("UML_BHR"))
	lic = DEFAULT_LIC_DIR "actiontec.lic";
    else if (!stat_lic_file(DEFAULT_LIC_DIR DEFAULT_LIC_FILE))
	lic = DEFAULT_LIC_DIR DEFAULT_LIC_FILE;
    else if (!stat_lic_file(DEFAULT_LIC_FILE))
	lic = DEFAULT_LIC_FILE;

    if (lic)
	token_set("LIC", lic);
    return lic;
}

static void small_flash_default_dist(void)
{
    enable_module("MODULE_RG_FOUNDATION");
    enable_module("MODULE_RG_UPNP");
    enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
    enable_module("MODULE_RG_VLAN");
    enable_module("MODULE_RG_PPP");
    enable_module("MODULE_RG_PPTP");
    enable_module("MODULE_RG_L2TP");
    enable_module("MODULE_RG_QOS");
    enable_module("MODULE_RG_ROUTE_MULTIWAN");
    enable_module("MODULE_RG_MAIL_FILTER");
    enable_module("MODULE_RG_URL_FILTERING");
    enable_module("MODULE_RG_DSLHOME");
    enable_module("MODULE_RG_TR_064");
    enable_module("MODULE_RG_DSL");
    enable_module("MODULE_RG_SSL_VPN");
    enable_module("MODULE_RG_REDUCE_SUPPORT");

    token_set_y("CONFIG_ULIBC_SHARED");
    token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");
    token_set_y("CONFIG_RG_SSL_VPN_SMALL_FLASH");
}

static void set_jpkg_dist_configs(char *jpkg_dist)
{
    int is_src = !strcmp(jpkg_dist, "JPKG_SRC");

    if (is_src || !strcmp(jpkg_dist, "JPKG_UML"))
    {
	jpkg_dist_add("UML");
	jpkg_dist_add("UML_GLIBC");
	jpkg_dist_add("UML_26");
	jpkg_dist_add("RGLOADER_UML");
	jpkg_dist_add("UML_VALGRIND");

	set_hosttools_configs();
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_ARMV5B"))
    {
	jpkg_dist_add("MONTEJADE");
	jpkg_dist_add("MONTEJADE_ATM");
	jpkg_dist_add("COYOTE");
	jpkg_dist_add("JIWIS8XX");
	jpkg_dist_add("JIWIS842J");
	jpkg_dist_add("MI424WR");
	jpkg_dist_add("RI408");
	jpkg_dist_add("RGLOADER_MONTEJADE");
	jpkg_dist_add("RGLOADER_COYOTE");

	enable_module("CONFIG_HW_80211N_AIRGO_AGN100");
	enable_module("MODULE_RG_VOIP_OSIP");
        enable_module("MODULE_RG_VOIP_RV_H323");
        enable_module("MODULE_RG_VOIP_RV_MGCP");
        enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	enable_module("MODULE_RG_ATA");
        enable_module("CONFIG_HW_80211G_RALINK_RT2561");

	token_set_y("CONFIG_UCLIBCXX");
	enable_module("CONFIG_RG_ATHEROS_HW_AR5212");
	token_set_m("CONFIG_RG_SWITCH_PORT_DEV");
	enable_module("MODULE_RG_WPS");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB_INFINEON"))
    {
	jpkg_dist_add("DANUBE");
	jpkg_dist_add("TWINPASS");
	
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_RADIUS_SERVER");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_LX4189_ADI"))
    {
	jpkg_dist_add("AD6834");
        enable_module("CONFIG_HW_80211G_RALINK_RT2561");

	enable_module("MODULE_RG_VOIP_OSIP");
        enable_module("MODULE_RG_VOIP_RV_H323");
        enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_RADIUS_SERVER");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_LX4189"))
    {
	jpkg_dist_add("ALASKA");
	
	/* List of modules that are currently not in the default image due to
	 * footprint issues */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_PBX");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
        enable_module("CONFIG_HW_80211G_RALINK_RT2561");

	enable_module("MODULE_RG_VOIP_RV_H323");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_RADIUS_SERVER");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_ARM_920T_LE"))
    {
	jpkg_dist_add("CENTROID");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
        enable_module("CONFIG_HW_80211G_RALINK_RT2561");
	/* XXX MODULE_RG_ADVANCED_MANAGEMENT Needs DYN_LINK 
	 * and Dyn link causes a crash on CENTROID.
	 * B30410
	 */
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_ARMV5L"))
    {
	jpkg_dist_add("SOLOS");
	
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_ATA");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_VLAN");
	/* Can't be included in JPKG_ARMV5L  because B37659
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	*/
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");	
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	/* Can't be included in JPKG_ARMV5L because B3774
	enable_module("MODULE_RG_VOIP_OSIP");
	*/
	enable_module("MODULE_RG_VOIP_RV_H323");
	enable_module("MODULE_RG_PBX");
        enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	/* Needed for voip compilation */
	token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");

        enable_module("CONFIG_HW_80211G_RALINK_RT2561");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_ARMV4L"))
    {
	jpkg_dist_add("MALINDI");
	jpkg_dist_add("MALINDI2");
	
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
	/* Can't be included in JPKG_ARMV4L because B37659
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	*/
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
        enable_module("CONFIG_HW_80211G_RALINK_RT2560");
        enable_module("CONFIG_HW_80211G_RALINK_RT2561");
	/* Can't be included in JPKG_ARMV4L because B3774
	enable_module("MODULE_RG_VOIP_OSIP");
	*/
        enable_module("MODULE_RG_VOIP_RV_H323");
        enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_ATA");
	token_set_y("CONFIG_RG_DSLHOME_VOUCHERS"); /* removed from MALINDI2 */
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_KS8695"))
    {
	jpkg_dist_add("CENTAUR_VGW");
	jpkg_dist_add("CENTAUR");
	jpkg_dist_add("RGLOADER_CENTAUR");

	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	/* Needed for voip compilation */
	token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
    }    
    if (is_src || !strcmp(jpkg_dist, "JPKG_PPC"))
    {
	/* XXX Restore these dists in B36583 
	jpkg_dist_add("EP8248_26");
	token_set_y("CONFIG_HW_BUTTONS");
	*/
	jpkg_dist_add("MPC8272ADS");
	jpkg_dist_add("MPC8349ITX");

	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_FTP_SERVER");
        enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("MODULE_RG_PBX");
	enable_module("MODULE_RG_JVM");
	/* Can't be included in JPKG_PPC because B37659
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	*/
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ADVANCED_ROUTING");

	/* Wireless */
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("CONFIG_RG_ATHEROS_HW_AR5212");
	enable_module("CONFIG_RG_ATHEROS_HW_AR5416");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_SB1250"))
    {
	jpkg_dist_add("BCM91125E");
	jpkg_dist_add("BCM_SB1125");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_OCTEON"))
    {
        jpkg_dist_add("CN3XXX");
        jpkg_dist_add("MC524WR");

        enable_module("CONFIG_RG_ATHEROS_HW_AR5212");
        enable_module("CONFIG_RG_ATHEROS_HW_AR5416");
        token_set_m("CONFIG_RG_SWITCH_PORT_DEV");
        token_set_m("CONFIG_HW_SWITCH_BCM53XX");
        dev_add("eth0", DEV_IF_BCM5395M_HW_SWITCH, DEV_IF_NET_INT);
        token_set_m("CONFIG_HW_SWITCH_KENDIN_KS8995M");
        enable_module("MODULE_RG_WPS");
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_MIPSEB"))
    {
	jpkg_dist_add("DWV_96358");
	jpkg_dist_add("BCM96358");
	jpkg_dist_add("ASUS6020VI_26");
	jpkg_dist_add("WADB100G_26");

	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_ATA");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "4194304");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_IPSEC");
	/* Can't be included in JPKG_MIPSEB because B37659
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	*/
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");	
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	/* Can't be included in JPKG_MIPSEB because B3774
	enable_module("MODULE_RG_VOIP_OSIP");
	*/
        enable_module("MODULE_RG_VOIP_RV_H323");
        enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_RADIUS_SERVER");
	enable_module("CONFIG_HW_USB_RNDIS");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_TR_064");
	token_set_y("CONFIG_RG_CRAMFS_IN_FLASH");
	enable_module("MODULE_RG_VLAN");

    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_BCM9634X"))
    {
	jpkg_dist_add("ASUS6020VI");
	jpkg_dist_add("WADB100G");
	jpkg_dist_add("WADB102GB");

	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");

    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_ARMV5L_FEROCEON"))
    {
	jpkg_dist_add("FEROCEON");
	token_set_y("CONFIG_HOTPLUG");

#if 0
	enable_module("CONFIG_RG_ATHEROS_HW_AR5212");
	enable_module("CONFIG_RG_ATHEROS_HW_AR5416");
	token_set_m("CONFIG_RG_SWITCH_PORT_DEV");
	token_set_m("CONFIG_ENTROPIC_EN2210");
	token_set_y("CONFIG_ENTROPIC_EN2210_MII");
	enable_module("MODULE_RG_WPS");
#endif
    }
    if (is_src || !strcmp(jpkg_dist, "JPKG_LOCAL_I386"))
    {
	/* Can't use JNET_SERVER in JPKG_SRC because it turns on
	 * CONFIG_RG_USE_LOCAL_TOOLCHAIN, which we don't want.
	 * Remove this when B32553 is fixed.
	 */
	if (is_src)
	    set_jnet_server_configs();
	else
	    jpkg_dist_add("JNET_SERVER");
    }
    if (is_src)
    {
	token_set_y("CONFIG_RG_TCPDUMP");
	token_set_y("CONFIG_RG_LIBPCAP");
	token_set_y("CONFIG_RG_IPROUTE2_UTILS");
        token_set_y("CONFIG_RG_JAVA");
        token_set_y("CONFIG_RG_JTA");
        token_set_y("CONFIG_RG_PROPER_JAVA_RDP");
        token_set_y("CONFIG_RG_JVFTP");
        token_set_y("CONFIG_RG_JCIFS");
        token_set_y("CONFIG_RG_SMB_EXPLORER");
        token_set_y("CONFIG_RG_TIGHT_VNC");
	token_set_y("GLIBC_IN_TOOLCHAIN");	
	token_set_y("CONFIG_RG_JNET_SERVER_TUTORIAL");	
    }
    else
    {
        token_set_y("CONFIG_RG_JPKG_BIN");
    }

    /* Common additional features: */
    token_set_y("CONFIG_RG_JPKG");
    if (strcmp(jpkg_dist, "JPKG_LOCAL_I386"))
    {
	/* These shouldn't be turbed on in binary local jpkg */
	token_set_y("CONFIG_RG_DOC_ENABLED");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	token_set_y("CONFIG_RG_TOOLS");
	token_set_y("CONFIG_RG_CONF_INFLATE");
        token_set_y("CONFIG_OPENRG");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
        enable_module("MODULE_RG_DSLHOME");
    }
}

/* ACTION_TEC */
static void actiontec_set_names(void)
{
    token_set("CONFIG_RG_HOST_NAME", "Wireless Broadband Router"); 
    token_set("RMT_UPG_SITE", "upgrade.actiontec.com");
    token_set("CONFIG_VENDOR_URL", "http://www.actiontec.com");
    token_set("CONFIG_PING_SITE","www.actiontec.com");
    token_set("FIRM", "Actiontec");

    /* Ethernet interfaces */
    token_set("ACTION_TEC_LAN_ETH_IFNAME", act_lan_eth_ifname);
    token_set("ACTION_TEC_WAN_ETH_IFNAME", act_wan_eth_ifname);

    /* Coax(MoCA) interfaces */
    token_set("ACTION_TEC_LAN_MOCA_IFNAME", act_lan_moca_ifname);
    token_set("ACTION_TEC_WAN_MOCA_IFNAME", act_wan_moca_ifname);

    /* PPPoE interfaces */
    token_set("ACTION_TEC_WAN_ETH_PPPoE_IFNAME", act_wan_eth_pppoe_ifname);
    token_set("ACTION_TEC_WAN_MOCA_PPPoE_IFNAME", act_wan_moca_pppoe_ifname);

    /* Wireless interfaces */
    token_set("ACTION_TEC_WIFI_IFNAME", act_wifi_ap_ifname);
    token_set("ACTION_TEC_ATHEROS_PRIMARY_IFNAME", act_atheros_vap_primary_ifname);
    token_set("ACTION_TEC_ATHEROS_SECONDARY_IFNAME", act_atheros_vap_secondary_ifname);
    token_set("ACTION_TEC_ATHEROS_PUBLIC_IFNAME", act_atheros_vap_public_ifname);
    token_set("ACTION_TEC_ATHEROS_HELP_IFNAME", act_atheros_vap_help_ifname);
    token_set("ACTION_TEC_RALINK_IFNAME", act_ralink_vap_ifname);

    /* Bridge interface */
    token_set("ACTION_TEC_DEFAULT_BR_NAME", act_def_bridge_ifname);
}

#define ACTION_TEC_NCS_ENABLED (token_get("ACTION_TEC_NCS") && token_is_y("ACTION_TEC_NCS"))

void verizon_specific_features(void)
{
    token_set_y("ACTION_TEC"); 

    if (IS_DIST("MC524WR"))
	token_set_y("ACTION_TEC_DUAL_IMG"); 

    if (!token_get("ACTION_TEC_SMALL_IMG"))
    {
	token_set_y("ACTION_TEC_MGCPALG_ONOFF");
	token_set_y("ACTION_TEC_SIPALG_ONOFF");
    }

    /* the general GUI module for Actiontec */
    token_set_y("CONFIG_GUI_AEI");
    token_set_y("CONFIG_GUI_AEI_VZ");
    token_set_y("CONFIG_GUI_RG2");

    /* Support the self heal function */ 
    token_set_y("ACTION_TEC_VERIZON_SHEAL");

    token_set_y("CONFIG_DNS_AEI_VZ");

    /* Default Language is English */
    //token_set("CONFIG_RG_LANGUAGES", "DEF"); 

    if (!token_get("ACTION_TEC_SMALL_IMG"))
	token_set_y("ACTION_TEC_PERSISTENT_LOG");

    /* related SetTopBox (STB) support */
    token_set_y("CONFIG_FW_AEI_VZ_STB"); 
    token_set("ACTION_TEC_STB_VENDOR_STR", "IP-STB");

    if (!token_get("ACTION_TEC_SMALL_IMG"))
    {
	token_set_y("CONFIG_TR64_AEI"); 

	token_set_y("ACTION_TEC_IGMP_MCF");

	token_set_y("ACTION_TEC_DIAGNOSTICS");
    }

    token_set_y("ACTION_TEC_80211N");
    token_set_y("ACTION_TEC_WMM");

    /* Add new port forwarding page */
    token_set_y("ACTION_TEC_NCS_NEW_PORTFW");

    /* TR-143 support */
    token_set_y("ACTION_TEC_TR143");
    token_set_y("ACTION_TEC_TR143_VZ_EXT");
    token_set_y("ACTION_TEC_TR143_VZ_PH2");
}

void actiontec_retail_specific_features(void)
{
    /* come from Verizon product */
    token_set_y("ACTION_TEC_VERIZON");

    /* custom GUI for Wireless settings */
    /*token_set_y("ACTION_TEC_80211N_GUI"); we use Verizon's GUI in phrase 1 */

    //if (!token_get("ACTION_TEC_SMALL_IMG")) Spec: no USB support
	//token_set_y("ACTION_TEC_NAS_FEATURES");/* this will be used in phrase 2, both file and printer server support */
    //token_set_y("ACTION_TEC_FILE_SERVER_ONLY_FEATURES");/* in phrase 1, only file server support, no printer server */
}
/* ACTION_TEC */

void distribution_features()
{
    if (!dist)
	conf_err("ERROR: DIST is not defined\n");

    /* MIPS */
    if (IS_DIST("ADM5120_LSP"))
    {
	hw = "ADM5120P";
	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_FRAME_POINTER");
    }
    else if (IS_DIST("DANUBE_LSP") || IS_DIST("TWINPASS_LSP"))
    {
	hw = "DANUBE";
	os = "LINUX_24";
	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_HW_ETH_LAN");
        token_set_y("CONFIG_RG_VOIP_DEMO");
	enable_module("CONFIG_HW_DSP");	
    }
    else if (IS_DIST("DANUBE") || IS_DIST("TWINPASS"))
    {
	if (IS_DIST("DANUBE"))
	    hw = "DANUBE";
	else
	    hw = "TWINPASS";

	os = "LINUX_24";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB_INFINEON");
	enable_module("MODULE_RG_FOUNDATION");

	/* XXX: Removing temprarily due to flash size limitation 
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	*/
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	if (IS_DIST("DANUBE"))
	    enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_UPNP_AV");

        enable_module("CONFIG_HW_ENCRYPTION");
	/* XXX: Removing temprarily due to flash size limitation 
	enable_module("MODULE_RG_IPSEC");
	*/
	token_set_y("CONFIG_IPSEC_USE_DANUBE_CRYPTO");

	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_PRINTSERVER");

	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_UPNP");

	enable_module("MODULE_RG_ATA");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");

	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_TR_064");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_SSL_VPN");
	/* XXX: Removing temprarily due to flash size limitation 
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");
	*/
	token_set_m("CONFIG_RG_DANUBE_SWITCH_IGMP");
	enable_module("CONFIG_HW_USB_STORAGE");
	token_set_y("CONFIG_HW_ETH_LAN");
	dev_add_bridge("br0", DEV_IF_NET_INT, "eth0", NULL);
	token_set_y("CONFIG_HW_SWITCH_LAN");
	enable_module("CONFIG_HW_DSP");
	enable_module("CONFIG_RG_ATHEROS_HW_AR5212");
	dev_add_to_bridge_if_opt("br0", "ath0", "CONFIG_RG_ATHEROS_HW_AR5212");
	if (IS_DIST("DANUBE"))
	{
	    token_set_y("CONFIG_HW_DSL_WAN");
	    token_set_m("CONFIG_HW_LEDS");
	    token_set_m("CONFIG_HW_BUTTONS");
	}
	else
	{
	    /* TWINPASS */
	    token_set_y("CONFIG_HW_VDSL_WAN");
	    token_set_m("CONFIG_RG_FASTPATH");
	    token_set("CONFIG_RG_FASTPATH_PLAT_PATH",
		"vendor/infineon/danube/modules");
	    token_set_y("CONFIG_RG_HW_QOS");
	}
    }
    else if (IS_DIST("BCM91125E") || IS_DIST("BCM_SB1125"))
    {
    	if (IS_DIST("BCM91125E"))
	    hw = "BCM91125E";
	else if (IS_DIST("BCM_SB1125"))
	    hw = "COLORADO";
	os = "LINUX_26";
	
	token_set_y("CONFIG_RG_SMB");
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_SB1250");

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");

	token_set_y("CONFIG_DYN_LINK");

	/*  SMB Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	/*  SMB Priority 2  */
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");

	/*  SMB Priority 3  */
	enable_module("MODULE_RG_RADIUS_SERVER");

	/*  SMB Priority 4  */
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");

	/* Can't add JVM to BCM1125E see B38742    	
	enable_module("MODULE_RG_JVM");*/

	/*  SMB Priority 5  */
	/* Can't add H323 to BCM1125 see B38603    	
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");*/
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	
	/* Only Ethernet HW for now */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	if (IS_DIST("BCM_SB1125"))
	{
	    /* Disk (USB) dependent modules */
	    enable_module("MODULE_RG_BLUETOOTH");
	    enable_module("MODULE_RG_FILESERVER");
	    enable_module("MODULE_RG_UPNP_AV");
	    enable_module("MODULE_RG_MAIL_SERVER");
	    enable_module("MODULE_RG_WEB_SERVER");
	    enable_module("MODULE_RG_FTP_SERVER");
	    enable_module("MODULE_RG_PRINTSERVER");
	    token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	    enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	    enable_module("MODULE_RG_PBX");
	    enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	}

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
    }

    else if (IS_DIST("CN3XXX_LSP"))
    {
	hw = "CN3XXX";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");
	
	token_set("LIBC_IN_TOOLCHAIN", "y");
	token_set_y("CONFIG_GLIBC");

	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
    }

    else if (IS_DIST("CN3XXX"))
    {
	hw = "CN3XXX";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_OCTEON");

	token_set_y("CONFIG_RG_SMB");

	/*  SMB Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_WEB_SERVER");

	/*  SMB Priority 2  */
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	
	/*  SMB Priority 3  */
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_RADIUS_SERVER");
	
	/*  SMB Priority 4  */
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_FTP_SERVER");
	/* pkg/kaffe doesn't support 64bit */
	//enable_module("MODULE_RG_JVM");

	/*  SMB Priority 5  */
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	
	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	token_set_y("CONFIG_DYN_LINK");
	
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_ULIBC");

	/* HW Configuration Section */

	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_USB_STORAGE");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
    }

    else if (IS_DIST("RGLOADER_ADM5120"))
    {
	hw = "ADM5120P";
	os = "LINUX_24";
	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_FRAME_POINTER");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("ADM5120_ATA"))
    {
	hw = "ADM5120P";
	os = "LINUX_24";
	
	/* OpenRG Feature set */
	token_set_y("CONFIG_RG_FOUNDATION_CORE");
	token_set_y("CONFIG_RG_CHECK_BAD_REBOOTS");
	/* From MODULE_RG_UPNP take only this */
	token_set_y("CONFIG_AUTO_LEARN_DNS");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_VOIP_RV_SIP");
    	token_set_y("CONFIG_RG_EVENT_LOGGING");
	token_set_y("CONFIG_RG_STATIC_ROUTE"); /* Static Routing */
	token_set_y("CONFIG_RG_UCD_SNMP"); /* SNMP v1/2 only */
	enable_module("MODULE_RG_ATA");

	/* OpenRG HW support */
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_HW_ETH_WAN");

	token_set_y("CONFIG_DEF_WAN_ALIAS_IP");
    }
    else if (IS_DIST("ADM5120_VGW") || IS_DIST("ADM5120_VGW_OSIP"))
    {
	hw = "ADM5120P";
	os = "LINUX_24";
	
	/* OpenRG Feature set */
	enable_module("MODULE_RG_FOUNDATION");
	/* XXX Workaround for B31610, remove when bug is resovled */
#if 0
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
#endif
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_UPNP");
        enable_module("MODULE_RG_VLAN");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_DSP");

	token_set_y("CONFIG_DYN_LINK");

	/* VoIP */
	enable_module("MODULE_RG_ATA");
	if (IS_DIST("ADM5120_VGW_OSIP"))
	    enable_module("MODULE_RG_VOIP_OSIP");
	else
	    enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
    }
    else if (IS_DIST("INCAIP_LSP"))
    {
	hw = "INCAIP_LSP";
	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_FRAME_POINTER");
	token_set_y("CONFIG_VINETIC_TAPIDEMO");
    }
    else if (IS_DIST("RGLOADER_INCAIP"))
    {
	hw = "INCAIP";
	token_set_y("CONFIG_RG_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("INCAIP_IPPHONE"))
    {
	hw = "INCAIP";

	/* OpenRG Feature set */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_H323");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
    	token_set_y("CONFIG_RG_EVENT_LOGGING"); /* Event Logging */

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_HW_KEYPAD");
	token_set_y("CONFIG_HW_LEDS");

	token_set_y("CONFIG_DYN_LINK");
	token_set_y("CONFIG_DEF_WAN_ALIAS_IP");
    }
    else if (IS_DIST("RGLOADER_FLEXTRONICS"))
    {
	hw = "FLEXTRONICS";
	token_set_y("CONFIG_RG_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("FLEXTRONICS_IPPHONE"))
    {
	hw = "FLEXTRONICS";

	/* OpenRG Feature set */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_H323");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
    	token_set_y("CONFIG_RG_EVENT_LOGGING"); /* Event Logging */

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_HW_KEYPAD");
	token_set_y("CONFIG_HW_LEDS");

	token_set_y("CONFIG_DYN_LINK");
	token_set_y("CONFIG_DEF_WAN_ALIAS_IP");
    }
    else if (IS_DIST("INCAIP_ATA") || IS_DIST("INCAIP_ATA_OSIP"))
    {
	hw = "ALLTEK";

	/* OpenRG Feature set */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
    	token_set_y("CONFIG_RG_EVENT_LOGGING");

	/* OpenRG HW support */
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_HW_ETH_WAN");

	token_set_y("CONFIG_DYN_LINK");
	token_set_y("CONFIG_DEF_WAN_ALIAS_IP");

	/* VoIP */
	enable_module("MODULE_RG_ATA");
	if (IS_DIST("INCAIP_ATA_OSIP"))
	    enable_module("MODULE_RG_VOIP_OSIP");
	else
	{
	    enable_module("MODULE_RG_VOIP_RV_SIP");
	    enable_module("MODULE_RG_VOIP_RV_MGCP");
	    enable_module("MODULE_RG_VOIP_RV_H323");
	}
    }
    else if (IS_DIST("RGLOADER_ALLTEK") ||
	IS_DIST("RGLOADER_ALLTEK_FULLSOURCE"))
    {
	hw = "ALLTEK";

	token_set_y("CONFIG_RG_RGLOADER");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_LAN");

    }
    else if (IS_DIST("INCAIP_VGW") || IS_DIST("INCAIP_VGW_OSIP"))
    {
	hw = "ALLTEK_VLAN";

    	token_set_y("CONFIG_RG_SMB");

	/* OpenRG Feature set */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_PPP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_L2TP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
        enable_module("MODULE_RG_IPV6");
        enable_module("MODULE_RG_URL_FILTERING");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_DSP");
        enable_module("CONFIG_HW_ENCRYPTION");

	token_set_y("CONFIG_DYN_LINK");

	/* VoIP */
	enable_module("MODULE_RG_ATA");
	if (IS_DIST("INCAIP_VGW_OSIP"))
	    enable_module("MODULE_RG_VOIP_OSIP");
	else
	{
	    enable_module("MODULE_RG_VOIP_RV_SIP");
	    enable_module("MODULE_RG_VOIP_RV_H323");
	    enable_module("MODULE_RG_VOIP_RV_MGCP");
	}
    }
    else if (IS_DIST("INCAIP_FULLSOURCE"))
    {
	hw = "ALLTEK_VLAN";

    	token_set_y("CONFIG_RG_SMB");

	/* OpenRG Feature set */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_PPP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_L2TP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
        enable_module("MODULE_RG_IPV6");
        enable_module("MODULE_RG_URL_FILTERING");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_ENCRYPTION");

	token_set_y("CONFIG_DYN_LINK");
    }
    else if (IS_DIST("BCM94702"))
    {
	hw = "BCM94702";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("BCM94704"))
    {
	hw = "BCM94704";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_SNMP");
    	token_set_y("CONFIG_RG_EVENT_LOGGING"); /* Event Logging */
	token_set_y("CONFIG_RG_ENTFY");	/* Email notification */

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0", "eth0", NULL);
    }
    else if (IS_DIST("USI_BCM94712"))
    {
	hw = "BCM94712";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0.0", "eth0", NULL);

	token_set_y("CONFIG_DYN_LINK");
    }
    else if (IS_DIST("SRI_USI_BCM94712"))
    {
	hw = "BCM94712";

	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_URL_FILTERING");
	token_set("CONFIG_RG_SURFCONTROL_PARTNER_ID", "6003");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_SNMP");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	
	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0.0", "eth0", NULL);

	/* The Broadcom nas application is dynamically linked */
	token_set_y("CONFIG_DYN_LINK");

	/* Wireless GUI options */
	/* Do NOT show Radius icon in advanced */
	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
    }
    else if (IS_DIST("RTA770W"))
    {
	hw = "RTA770W";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	/* broadcom nas (wpa application) needs ulibc.so so we need to compile
	 * openrg dynamically */
	token_set_y("CONFIG_DYN_LINK");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	token_set_y("CONFIG_RG_IGD_XBOX");
	enable_module("MODULE_RG_DSL");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("CONFIG_HW_USB_RNDIS");
	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0", "usb0", "wl0", NULL);

	token_set_y("CONFIG_GUI_BELKIN");
	token_set("RG_PROD_STR", "Prodigy infinitum");
	token_set_y("CONFIG_RG_DSL_CH");
	token_set_y("CONFIG_RG_PPP_ON_DEMAND_AS_DEFAULT");
	/* Belkin's requirement - 3 hours of idle time */
	token_set("CONFIG_RG_PPP_ON_DEMAND_DEFAULT_MAX_IDLE_TIME", "10800");
	/* from include/enums.h PPP_COMP_ALLOW is 1 */
	token_set("CONFIG_RG_PPP_DEFAULT_BSD_COMPRESSION", "1");
	/* from include/enums.h PPP_COMP_ALLOW is 1 */
	token_set("CONFIG_RG_PPP_DEFAULT_DEFLATE_COMPRESSION", "1");
	/* Download image to memory before flashing
	 * Only one image section in flash, enough memory */
	token_set_y("CONFIG_RG_RMT_UPGRADE_IMG_IN_MEM");
	/* For autotest and development purposes, Spanish is the default
	 * language allowing an override */
	if (!token_get_str("CONFIG_RG_DIST_LANG"))
	    token_set("CONFIG_RG_DIST_LANG", "spanish_belkin");
	token_set_y("CONFIG_RG_CFG_SERVER");
	token_set_y("CONFIG_RG_OSS_RMT");
	token_set_y("CONFIG_RG_RMT_MNG");
	token_set_y("CONFIG_RG_NON_ROUTABLE_LAN_DEVICE_IP");
    }
    else if (IS_DIST("RTA770W_EVAL"))
    {
	hw = "RTA770W";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_DSL");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("CONFIG_HW_USB_RNDIS");
	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0", "usb0", "wl0", NULL);

	/* Download image to memory before flashing
	 * Only one image section in flash, enough memory */
	token_set_y("CONFIG_RG_RMT_UPGRADE_IMG_IN_MEM");
	/* broadcom nas (wpa application) needs ulibc.so so we need to compile
	 * openrg dynamically */
	token_set_y("CONFIG_DYN_LINK");
    }
    else if (IS_DIST("RGLOADER_RTA770W"))
    {
	hw = "RTA770W";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_BCM963XX_BOOTSTRAP");
	token_set_m("CONFIG_RG_KRGLDR");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("GTWX5803"))
    {
	hw = "GTWX5803";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_BCM9634X");

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_UPNP");
    	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
    	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	
	/*  RG Priority 2  */
    	enable_module("MODULE_RG_PPTP");
    	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	
	/*  RG Priority 3  */
    	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	/*Not enough space on flash 
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_JVM");
	*/

	/*  RG Priority 7  */
	/*Not enough space on flash 
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	*/

	token_set_y("CONFIG_ULIBC_SHARED");
	token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");
	token_set_y("CONFIG_RG_SSL_VPN_SMALL_FLASH");

	token_set_m("CONFIG_RG_MTD");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("CONFIG_HW_USB_RNDIS");
	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0", NULL);
	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");
    }
    else if (IS_DIST("RGLOADER_GTWX5803"))
    {
	hw = "GTWX5803";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_RG_TELNETS");
	token_set_y("CONFIG_BCM963XX_BOOTSTRAP");
	token_set_m("CONFIG_RG_KRGLDR");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set_m("CONFIG_RG_MTD");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("WRT54G"))
    {
	hw = "WRT54G";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_SNMP");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	token_set_m("CONFIG_HW_BUTTONS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0.2", "eth0", NULL);

	token_set_y("CONFIG_ARCH_BCM947_CYBERTAN");
	token_set_y("CONFIG_RG_BCM947_NVRAM_CONVERT");
	token_set_y("CONFIG_DYN_LINK");
	token_set_y("CONFIG_GUI_LINKSYS");
    }
    else if (IS_DIST("SOLOS_LSP") || IS_DIST("SOLOS"))
    {
	hw = "CX9451X";
	os = "LINUX_26";
	token_set_y("CONFIG_ARCH_SOLOS");
	
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	
	if (IS_DIST("SOLOS_LSP"))
	{
	    token_set_y("CONFIG_RG_KGDB");
	    token_set_y("CONFIG_LSP_DIST");
	}
	else
	{

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_ATA");
	
	/*  RG Priority 2  */
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "4194304");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");

	/*  RG Priority 3  */
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	enable_module("MODULE_RG_IPSEC");
           token_set_y("CONFIG_IPSEC_USE_SOLOS_CRYPTO");
	/* Can't add H323 to SOLOS see B38603    	
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");*/	
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_MAIL_SERVER");	
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");

	/*  RG Priority 7  */
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SNMP");
	/* Can't add IPV6 to SOLOS see B38590 
	enable_module("MODULE_RG_IPV6");  */

	    /* HW configuration */
	    enable_module("CONFIG_HW_DSP");
	    enable_module("CONFIG_HW_USB_STORAGE");
	    token_set_y("CONFIG_HW_DSL_WAN");
	    token_set_y("CONFIG_HW_ETH_WAN");
	    token_set_y("CONFIG_HW_ETH_LAN");

	    dev_add_bridge("br0", DEV_IF_NET_INT, "lan0", NULL);
	}

	token_set_y("CONFIG_DYN_LINK");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5L");
	/* XXX remove after resolving B36422 */
	token_set("CONFIG_RG_EXTERNAL_TOOLS_PATH",
	    "/usr/local/virata/tools_v10.1c/redhat-9-x86");
    }
    else if (IS_DIST("CX82100_SCHMID"))
    {
	hw = "CX82100";
	os = "LINUX_22";
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_USB_RNDIS");

	token_set_y("CONFIG_RG_TODC");
    }
    else if (IS_DIST("X86_FRG_TMT")) /* x86 */
    {
	hw = "PCBOX_EEP_EEP";
	
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_VLAN");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_GLIBC");
    }
    else if (IS_DIST("RGLOADER_X86_TMT"))
    {
	hw = "PCBOX_EEP_EEP";

	token_set_y("CONFIG_RG_RGLOADER");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("ALLWELL_RTL_EEP"))
    {
	hw = "ALLWELL_RTL_EEP";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_URL_FILTERING");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

    }
    else if (IS_DIST("ALLWELL_RTL_RTL_WELLTECH"))
    {
	hw = "ALLWELL_RTL_RTL";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_GUI_WELLTECH");
    }
    /* XScale IXP425 based boards */
    else if (IS_DIST("COYOTE_BHR"))
    {
	hw = "COYOTE";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	token_set_y("CONFIG_RG_8021Q_IF");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");

	token_set_m("CONFIG_RG_PPPOE_RELAY"); /* PPPoE Relay */
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_80211G_ISL_SOFTMAC");

	token_set("CONFIG_RG_SSID_NAME", "openrg");
	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "eth0", NULL);
	
	/* Dist specific configuration */
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_RG_PROD_IMG");

	/* Tasklet me harder */
	token_set_y("CONFIG_TASKLET_ME_HARDER");

	/* Make readonly GUI */
	token_set_y("CONFIG_RG_WBM_READONLY_USERS_GROUPS");
    }

    else if (IS_DIST("RGLOADER_USR8200"))
    {
	hw = "USR8200";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("USR8200_EVAL"))
    {
	hw = "USR8200";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_L2TP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_UHCI");
	enable_module("CONFIG_HW_FIREWIRE");
	enable_module("CONFIG_HW_FIREWIRE_STORAGE");
	enable_module("CONFIG_HW_USB_STORAGE");
        enable_module("CONFIG_HW_ENCRYPTION");

	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS1");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set_y("CONFIG_RG_DATE");
    }
    else if (IS_DIST("USR8200") || IS_DIST("USR8200_ALADDIN"))
    {
	hw = "USR8200";

	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_PRINTSERVER");
	token_set_y("CONFIG_RG_SURFCONTROL");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_UHCI");
	enable_module("CONFIG_HW_FIREWIRE");
	enable_module("CONFIG_HW_FIREWIRE_STORAGE");
	enable_module("CONFIG_HW_USB_STORAGE");
        enable_module("CONFIG_HW_ENCRYPTION");
        token_set_y("CONFIG_HW_LEDS");
        token_set_y("CONFIG_HW_CLOCK");

	token_set_y("CONFIG_RG_FW_ICSA");
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set_y("CONFIG_RG_DATE");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	token_set_y("CONFIG_STOP_ON_INIT_FAIL");
	token_set("CONFIG_RG_SURFCONTROL_PARTNER_ID", "6002");
	token_set_y("CONFIG_RG_DHCPR");

	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	
	/* Include automatic daylight saving time calculation */
	token_set_y("CONFIG_RG_TZ_FULL");
	token_set("CONFIG_RG_TZ_YEARS", "5");

	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_MAIL_FILTER");
    }
    else if (IS_DIST("USR8200_TUTORIAL"))
    {
	hw = "USR8200";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_L2TP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_UHCI");
	enable_module("CONFIG_HW_FIREWIRE");
	enable_module("CONFIG_HW_FIREWIRE_STORAGE");
	enable_module("CONFIG_HW_USB_STORAGE");
        enable_module("CONFIG_HW_ENCRYPTION");
        token_set_y("CONFIG_HW_LEDS");
        token_set_y("CONFIG_HW_CLOCK");

	token_set_y("CONFIG_RG_DATE");
	token_set_y("CONFIG_RG_TUTORIAL");
	token_set_y("CONFIG_STOP_ON_INIT_FAIL");
	token_set_y("CONFIG_IXP425_COMMON_RG");

	/* Include automatic daylight saving time calculation */
	token_set_y("CONFIG_RG_TZ_FULL");
	token_set("CONFIG_RG_TZ_YEARS", "5");
    }
    else if (IS_DIST("BAMBOO") || IS_DIST("BAMBOO_ALADDIN"))
    {
	hw = "BAMBOO"; 

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	token_set_y("CONFIG_RG_PROXY_ARP");
	token_set_y("CONFIG_RG_ENTFY");	/* Email notification */
	token_set_y("CONFIG_RG_UCD_SNMP"); /* SNMP v1/v2 */
	token_set_y("CONFIG_RG_8021X_TLS");
	token_set_y("CONFIG_RG_8021X_RADIUS");
	enable_module("MODULE_RG_ATA");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_USB_RNDIS");
        enable_module("CONFIG_HW_80211B_PRISM2");
        enable_module("CONFIG_HW_DSP");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_UHCI");
	enable_module("CONFIG_HW_USB_STORAGE");
        enable_module("CONFIG_HW_ENCRYPTION");
        token_set_y("CONFIG_HW_CAMERA_USB_OV511");
        token_set_m("CONFIG_HW_PCMCIA_CARDBUS");
	token_set_m("CONFIG_HW_BUTTONS");
	token_set_m("CONFIG_HW_LEDS");

	token_set_y("CONFIG_GLIBC");
	token_set_y("CONFIG_IXP425_COMMON_RG");

	/* PPTP, PPPoE */
        token_set("CONFIG_RG_PPTP_ECHO_INTERVAL", "20");
        token_set("CONFIG_RG_PPTP_ECHO_FAILURE", "3");
        token_set("CONFIG_PPPOE_MAX_RETRANSMIT_TIMEOUT", "64");

	/* DSR support */
	token_set_y("CONFIG_IXP425_DSR");
	token_set_y("CONFIG_RG_OLD_XSCALE_TOOLCHAIN");

	if (IS_DIST("BAMBOO_ALADDIN"))
	{
	    enable_module("MODULE_RG_MAIL_FILTER");
	    enable_module("MODULE_RG_PRINTSERVER");
	    token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	}
    }
    else if (IS_DIST("RGLOADER_CENTROID"))
    {
	hw = "CENTROID";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_SL2312_COMMON");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
    }
    else if (IS_DIST("CENTROID"))
    {
	hw = "CENTROID";
	
	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	enable_module("MODULE_RG_IPSEC");
	
	/* XXX this is a workaround until B22301 is resolved */
	token_set_y("CONFIG_RG_THREADS");

	token_set_m("CONFIG_RG_MTD");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
 	enable_module("CONFIG_HW_80211G_RALINK_RT2560");
	enable_module("CONFIG_HW_USB_STORAGE");
	
	token_set_y("CONFIG_SL2312_COMMON_RG");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8"); 

	dev_add_bridge("br0", DEV_IF_NET_INT, "sl0", NULL);
	dev_add_to_bridge_if_opt("br0", "ra0", 
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");
    }
    else if (IS_DIST("MATECUMBE"))
    {
	hw = dist;

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_L2TP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_URL_FILTERING");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_USB_RNDIS");
        enable_module("CONFIG_HW_ENCRYPTION");
	token_set_y("CONFIG_RG_INITFS_RAMDISK");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "usb0", NULL);
    }
    else if (IS_DIST("IXDP425"))
    {
	hw = "IXDP425";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");
	token_set_y("CONFIG_RG_SMB");
	token_set_y("CONFIG_ARM_24_FAST_MODULES");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_L2TP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
    	enable_module("CONFIG_HW_USB_STORAGE");
        enable_module("CONFIG_HW_USB_RNDIS");
        enable_module("CONFIG_HW_ENCRYPTION");

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20150");
	token_set_y("CONFIG_IXP425_ADSL_USE_MPHY");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", NULL);
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");
    }
    else if (IS_DIST("IXDP425_WIRELESS"))
    {
	hw = "IXDP425";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");
	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_L2TP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_USB_RNDIS");
        enable_module("CONFIG_HW_80211G_ISL38XX");
        enable_module("CONFIG_HW_ENCRYPTION");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "usb0", "eth0", NULL);

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20150");
	token_set_y("CONFIG_IXP425_ADSL_USE_MPHY");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");
    }
     else if (IS_DIST("IXDP425_NETKLASS"))
    {
	hw = "IXDP425";

	enable_module("MODULE_RG_FOUNDATION");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	token_set_y("CONFIG_RG_ENTFY");	/* Email notification */
    	token_set_y("CONFIG_RG_EVENT_LOGGING"); /* Event Logging */
	token_set_y("CONFIG_RG_8021X");
	token_set_y("CONFIG_RG_8021X_MD5");
	token_set_y("CONFIG_RG_PROXY_ARP");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_ENCRYPTION");

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_IXP425_CSR_USB");
	token_set_y("CONFIG_SIMPLE_RAMDISK");
	token_set("CONFIG_RAMDISK_SIZE", "8192");
	token_set_y("CONFIG_GLIBC");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	
	/* VPN HW Acceleration */
	token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");

    }
    else if (IS_DIST("WAV54G"))
    {
	hw = "WAV54G";

	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_L2TP");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_ISL38XX");
        enable_module("CONFIG_HW_ENCRYPTION");
	token_set_m("CONFIG_HW_BUTTONS");

	token_set_y("CONFIG_GLIBC");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "eth0", NULL);
    }
    else if (IS_DIST("CX8620XR_LSP") || IS_DIST("CX8620XD_LSP"))
    {
	if (IS_DIST("CX8620XR_LSP"))
	    hw = "CX8620XR";
	else
	    hw = "CX8620XD";

	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_IPTABLES");
	token_set_m("CONFIG_BRIDGE");
	token_set_y("CONFIG_BRIDGE_UTILS");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	token_set_m("CONFIG_ISL_SOFTMAC");
	token_set_y("CONFIG_RG_NETKIT");
	
	token_set_y("CONFIG_CX8620X_COMMON");

	enable_module("CONFIG_HW_USB_HOST_EHCI");
    	enable_module("CONFIG_HW_USB_STORAGE");
    }
    else if (IS_DIST("CX8620XR"))
    {
	hw = "CX8620XR";
	
	token_set_y("CONFIG_CX8620X_COMMON");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
  	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_RALINK_RT2560");
	token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");
	
	dev_add_bridge("br0", DEV_IF_NET_INT, "cnx0", NULL);
	dev_add_to_bridge_if_opt("br0", "ra0", 
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5L");
    }
    else if (IS_DIST("CX8620XD_FILESERVER"))
    {
	hw = "CX8620XD";

	token_set_y("CONFIG_CX8620X_COMMON");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");

	/* HW Configuration Section */
	enable_module("CONFIG_HW_USB_HOST_EHCI");
    	enable_module("CONFIG_HW_USB_STORAGE");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
 	enable_module("CONFIG_HW_80211G_RALINK_RT2560");
	
	dev_add_bridge("br0", DEV_IF_NET_INT, "cnx0", NULL);
	dev_add_to_bridge_if_opt("br0", "ra0", 
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");
    }
    else if (IS_DIST("CX8620XD_SOHO"))
    {
	hw = "CX8620XD";

	token_set_y("CONFIG_CX8620X_COMMON");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");

    	token_set_y("CONFIG_RG_SMB");
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_L2TP");
        enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
        enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	token_set("CONFIG_RG_SURFCONTROL_PARTNER_ID", "6002");

	/* HW Configuration Section */
	enable_module("CONFIG_HW_USB_HOST_EHCI");
    	enable_module("CONFIG_HW_USB_STORAGE");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
 	enable_module("CONFIG_HW_80211G_RALINK_RT2560");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5L");
	token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");

	dev_add_bridge("br0", DEV_IF_NET_INT, "cnx0", NULL);
	dev_add_to_bridge_if_opt("br0", "ra0",
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");
    }
    else if (IS_DIST("MALINDI") || IS_DIST("MALINDI2"))
    {
	hw = "COMCERTO";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV4L");
	
	/* Comcerto chipset */
	token_set_y("CONFIG_COMCERTO_COMMON");

	/* Board */
	if (IS_DIST("MALINDI"))
	    token_set_y("CONFIG_COMCERTO_MALINDI");
	else if (IS_DIST("MALINDI2"))
	    token_set_y("CONFIG_COMCERTO_NAIROBI");

	token_set_y("CONFIG_RG_SMB");

	/*  SMB Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("MODULE_RG_QOS");

	/* CONFIG_RG_DSLHOME_VOUCHERS is disabled in feature_config */
	enable_module("MODULE_RG_DSLHOME"); 
	
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
        enable_module("MODULE_RG_PBX");

	/*Not enough space on flash
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_L2TP");
	*/
	
	/*  SMB Priority 2  */
	/*Not enough space on flash
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	*/
	
	/*  SMB Priority 3  */
	enable_module("MODULE_RG_RADIUS_SERVER");
	/*Not enough space on flash
	enable_module("MODULE_RG_BLUETOOTH");
	*/
	
	/*  SMB Priority 4  */
	/*Not enough space on flash
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
	*/
	
	/*  SMB Priority 5  */
	/*Not enough space on flash
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	*/

	/* Cryptographic hardware accelerator */
	token_set_y("CONFIG_CADENCE_IPSEC2");

	/* HW Configuration Section */
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	enable_module("CONFIG_HW_ENCRYPTION");
	enable_module("CONFIG_HW_DSP");

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	/* Ralink Wi-Fi card */
	enable_module("CONFIG_HW_80211G_RALINK_RT2561");

	dev_add_bridge("br0", DEV_IF_NET_INT, "eth0", NULL);

	dev_add_to_bridge_if_opt("br0", "ra0",
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ?
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");
    }

    else if (IS_DIST("RGLOADER_CX8620XD"))
    {
	hw = "CX8620XD";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_CX8620X_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5L");
    }
    else if (IS_DIST("IXDP425_CYBERTAN"))
    {
	hw = "IXDP425";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");
	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_SNMP");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_USB_RNDIS");
        enable_module("CONFIG_HW_ENCRYPTION");

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "usb0", NULL);
    }
    else if (IS_DIST("IXDP425_ATM") || IS_DIST("IXDP425_ATM_WIRELESS"))
    {
	hw = "IXDP425";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_DSL");
	if (IS_DIST("IXDP425_ATM_WIRELESS"))
	    enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_ENCRYPTION");
	if (IS_DIST("IXDP425_ATM_WIRELESS"))
	{
	    enable_module("CONFIG_HW_80211G_ISL38XX");
	    dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "eth0", NULL);
	}

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20150");
	token_set_y("CONFIG_IXP425_ADSL_USE_MPHY");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");
    }
    else if (IS_DIST("NAPA"))
    {
	hw = dist;

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_USB_RNDIS");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_FIREWIRE");
	enable_module("CONFIG_HW_FIREWIRE_STORAGE");
	enable_module("CONFIG_HW_USB_STORAGE");
        enable_module("CONFIG_HW_ENCRYPTION");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp1", "usb0", NULL);
    }
    /* Gemtek boards */
    else if (IS_DIST("GTWX5715"))
    {
	hw = "GTWX5715";

	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_ENCRYPTION");
	token_set_m("CONFIG_HW_BUTTONS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", NULL);
	
	enable_module("CONFIG_HW_80211G_ISL38XX");
	dev_add_to_bridge_if_opt("br0", "eth0", "CONFIG_HW_80211G_ISL38XX");

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	    token_set_y("CONFIG_RG_WPA");
	
	dev_add_to_bridge_if_opt("br0", "ra0", 
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");

	token_set_y("CONFIG_GLIBC");

	/* Download image to memory before flashing
	 * Only one image section in flash, enough memory */
	token_set_y("CONFIG_RG_RMT_UPGRADE_IMG_IN_MEM");
    }
    else if (IS_DIST("HG21"))
    {
	hw = "HG21";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_ISL38XX");
        enable_module("CONFIG_HW_ENCRYPTION");

	token_set_y("CONFIG_GLIBC");
	token_set_y("CONFIG_RG_INSMOD_SILENT");
	token_set("CONFIG_JFFS2_FS", "m");
	token_set_y("CONFIG_IXP425_JFFS2_WORKAROUND");
	token_set_y("CONFIG_IXP425_CSR_HSS");
	token_set("CONFIG_IXP425_CODELETS", "m");
	token_set("CONFIG_IXP425_CODELET_HSS", "m");
	token_set_y("CONFIG_GUI_WELLTECH");
	token_set_y("CONFIG_GUI_RG");
	token_set_y("CONFIG_RG_SYSLOG_REMOTE");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "eth0", NULL);
    }
    else if (IS_DIST("IXDP425_FRG"))
    {
	hw = "IXDP425";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_USB_RNDIS");
        enable_module("CONFIG_HW_ENCRYPTION");

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20150");
	token_set_y("CONFIG_IXP425_ADSL_USE_MPHY");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");	

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "usb0", NULL);
    }
    else if (IS_DIST("IXDP425_TMT"))
    {
	hw = "IXDP425_TMT";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_ENCRYPTION");
	token_set_m("CONFIG_HW_BUTTONS");

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	token_set_y("CONFIG_GLIBC");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");
    }
    else if (IS_DIST("RGLOADER_WADB100G"))
    {
    	hw = "WADB100G";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_BCM9634X");
	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_BCM963XX_BOOTSTRAP");
	token_set_m("CONFIG_RG_KRGLDR");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("WADB100G"))
    {
    	hw = "WADB100G";
	os = "LINUX_24";
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_BCM9634X");

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_UPNP");
    	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
    	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	
	/*  RG Priority 2  */
    	enable_module("MODULE_RG_PPTP");
    	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	
	/*  RG Priority 3  */
    	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	/*Not enough space on flash 
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	*/

	/*  RG Priority 7  */
	/*Not enough space on flash 
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	*/
	
	token_set_y("CONFIG_ULIBC_SHARED");
	token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");
	token_set_y("CONFIG_RG_SSL_VPN_SMALL_FLASH");

	token_set_m("CONFIG_RG_MTD");

	/* OpenRG HW support */
	
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("CONFIG_HW_USB_RNDIS");
	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0", "bcm1", NULL);
	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");
    }
    else if (IS_DIST("WADB100G_26"))
    {
    	hw = "WADB100G";
	os = "LINUX_26";
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
    	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
    	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_SSL_VPN");
	
	/*  RG Priority 2  */
    	enable_module("MODULE_RG_PPTP");
    	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	
	/*  RG Priority 3  */
    	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	/*Not enough space on flash 
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_JVM");
	*/

	/*  RG Priority 7  */
	/*Not enough space on flash 
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	*/
	

	token_set_y("CONFIG_ULIBC_SHARED");
	token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");
	token_set_y("CONFIG_RG_SSL_VPN_SMALL_FLASH");

	token_set_y("CONFIG_RG_MTD");

	/* OpenRG HW support */
	
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("CONFIG_HW_USB_RNDIS");

	/* B40399: HW led/button support is required for ASUS kernel 2.6
	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");
	*/

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm0", "bcm1", NULL);
	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_CRAMFS_IN_FLASH");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
    }
    else if (IS_DIST("ASUS6020VI"))
    {
    	hw = "ASUS6020VI";
	os = "LINUX_24";
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_BCM9634X");

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_UPNP");
    	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
    	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_SSL_VPN");
	token_set_y("CONFIG_RG_SSL_VPN_SMALL_FLASH");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	
	/*  RG Priority 2  */
 	enable_module("MODULE_RG_PPTP");
  	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	
	/*  RG Priority 3  */
  	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	/* Not enough RAM on board for IPSEC
	enable_module("MODULE_RG_IPSEC");
	*/
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	/*Not enough space on flash 
	enable_module("MODULE_RG_JVM");
	*/

	/*  RG Priority 7  */
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	/*Not enough space on flash
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	*/
	
	/* OpenRG HW support */
	
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");

	token_set_y("CONFIG_ULIBC_SHARED");
	token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", NULL);
	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");
    }
    else if (IS_DIST("ASUS6020VI_26"))
    {
    	hw = "ASUS6020VI";
	os = "LINUX_26";
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_UPNP");
    	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
    	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
    	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_SSL_VPN");
	token_set_y("CONFIG_RG_SSL_VPN_SMALL_FLASH");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	
	/*  RG Priority 2  */
	/* Not enough space on flash 
 	enable_module("MODULE_RG_PPTP");
  	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_MAIL_FILTER");
	*/
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	
	/*  RG Priority 3  */
	/* Not enough space on flash 
  	enable_module("MODULE_RG_VLAN");
	*/

	/*  RG Priority 4  */
	/* Not enough RAM on board for IPSEC
	enable_module("MODULE_RG_IPSEC");
	*/
	/* Not enough space on flash 
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_JVM");
	*/

	/*  RG Priority 7  */
	/* Not enough space on flash
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	*/
	
	/* OpenRG HW support */
	
	token_set_y("CONFIG_HW_DSL_WAN");
	/*
	token_set_y("CONFIG_HW_SWITCH_LAN");
	*/
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");

	/* B40399: HW led/button support is required for ASUS kernel 2.6
	token_set_m("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_LEDS");
	*/

	token_set_y("CONFIG_ULIBC_SHARED");
	token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_CRAMFS_IN_FLASH");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", NULL);
	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");
    }
    else if (IS_DIST("RGLOADER_WADB102GB"))
    {
    	hw = "WADB102GB";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_BCM9634X");
	token_set_y("CONFIG_RG_RGLOADER");
	token_set_m("CONFIG_RG_KRGLDR");
	token_set_y("CONFIG_RG_TELNETS");
	
	token_set_y("CONFIG_RG_MTD");

	token_set("CONFIG_SDRAM_SIZE", "16");
	token_set_y("CONFIG_BCM963XX_BOOTSTRAP");

	/* OpenRG HW support */
	
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("WADB102GB"))
    {
    	hw = "WADB102GB";
	os = "LINUX_24";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_BCM9634X");
	
	small_flash_default_dist();

	/* OpenRG HW support */

	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	enable_module("CONFIG_HW_80211G_BCM43XX");
	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", NULL);
	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");
    }
    else if (IS_DIST("MPC8272ADS_LSP") || IS_DIST("MPC8272ADS_LSP_26"))
    {
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_PPC");

	/* Hardware */
	
	hw = "MPC8272ADS";
	token_set_y("CONFIG_HW_ETH_FEC");

	if (IS_DIST("MPC8272ADS_LSP_26"))
	    os = "LINUX_26";
	else
	    os = "LINUX_24";
	
	/* Software */

	token_set_y("CONFIG_LSP_DIST");
    }
    else if (IS_DIST("MPC8272ADS"))
    {
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_PPC");

	hw = "MPC8272ADS";
	os = "LINUX_26";

	token_set_y("CONFIG_RG_SMB");

	/*  SMB Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
        enable_module("MODULE_RG_PBX"); 

	/*  SMB Priority 2  */
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");

	/*  SMB Priority 3  */
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_RADIUS_SERVER");

	/*  SMB Priority 4  */
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");

	/*Not enough space on flash
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_JVM");	
	*/

	/*  SMB Priority 5  */
	/*Not enough space on flash
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
        */
	
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	
	/* Include automatic daylight saving time calculation */
	
	token_set_y("CONFIG_RG_DATE");
	token_set_y("CONFIG_RG_TZ_FULL");
	token_set("CONFIG_RG_TZ_YEARS", "5");

	/* HW Configuration Section */
	
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_HW_ETH_FEC");

	enable_module("CONFIG_HW_USB_STORAGE");

	/* Ralink RT2560 */

	enable_module("CONFIG_HW_80211G_RALINK_RT2560");

	enable_module("CONFIG_HW_ENCRYPTION");
	
	dev_add_bridge("br0", DEV_IF_NET_INT, "eth1", NULL);
	dev_add_to_bridge_if_opt("br0", "ra0", 
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
    }
    else if (IS_DIST("EP8248_LSP_26"))
    {
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_PPC");

	/* Hardware */
	
	hw = "EP8248";
	os = "LINUX_26";
	
	token_set_y("CONFIG_HW_ETH_FEC");

	/* Software */

	token_set_y("CONFIG_LSP_DIST");
    }
    else if (IS_DIST("EP8248_26"))
    {
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_PPC");

	hw = "EP8248";
	os = "LINUX_26";
	
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_URL_FILTERING");
#if 0
	if (!IS_DIST("MPC8272ADS_26")) /* Temporary */
	{
	    enable_module("MODULE_RG_MAIL_SERVER");
	    enable_module("MODULE_RG_WEB_SERVER");
	    enable_module("MODULE_RG_FTP_SERVER");

	    enable_module("MODULE_RG_PRINTSERVER");
	    token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	}

	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
#endif	
	token_set_y("CONFIG_RG_NETTOOLS_ARP");

	/* Include automatic daylight saving time calculation */
	
	token_set_y("CONFIG_RG_DATE");
	token_set_y("CONFIG_RG_TZ_FULL");
	token_set("CONFIG_RG_TZ_YEARS", "5");

	/* HW Configuration Section */
	
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_HW_ETH_FEC");

//	enable_module("CONFIG_HW_USB_STORAGE");

	dev_add_bridge("br0", DEV_IF_NET_INT, "eth0", NULL);
    }
    else if (IS_DIST("MPC8349ITX_LSP"))
    {
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_PPC");

	/* Hardware */
	
	os = "LINUX_26";
	hw = "MPC8349ITX";

	/* Software */
	token_set_y("CONFIG_GIANFAR");
	token_set_y("CONFIG_CICADA_PHY");
	token_set_y("CONFIG_PHYLIB");
	token_set_y("CONFIG_LSP_DIST");

	token_set_y("CONFIG_USB_GADGET");
	token_set_y("CONFIG_USB_ETH");
	token_set_y("CONFIG_USB_MPC");
	token_set_y("CONFIG_USB_GADGET_MPC");
	token_set_y("CONFIG_USB_GADGET_DUALSPEED");
	token_set_y("CONFIG_USB_ETH_RNDIS");

	token_set_y("CONFIG_SCSI");
	token_set_y("CONFIG_SCSI_SATA");
	token_set_y("CONFIG_SCSI_SATA_SIL");

	token_set_y("CONFIG_USB");
	token_set_y("CONFIG_USB_EHCI_HCD");
	token_set_y("CONFIG_USB_EHCI_ROOT_HUB_TT");
	token_set_y("CONFIG_USB_DEVICEFS");
	token_set_y("CONFIG_FSL_USB20");
	token_set_y("CONFIG_MPH_USB_SUPPORT");
	token_set_y("CONFIG_MPH0_USB_ENABLE");
	token_set_y("CONFIG_MPH0_ULPI");
    }
    else if (IS_DIST("MPC8349ITX"))
    {
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_PPC");

	os = "LINUX_26";
	hw = "MPC8349ITX";

	token_set_y("CONFIG_RG_SMB");

	/*  SMB Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
        enable_module("MODULE_RG_PBX"); 

	/*  SMB Priority 2  */
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_MAIL_FILTER");
	/*Not enough space on flash
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	*/

	/*  SMB Priority 3  */
	enable_module("MODULE_RG_RADIUS_SERVER");
	enable_module("MODULE_RG_UPNP_AV");
	/*Not enough space on flash
	enable_module("MODULE_RG_BLUETOOTH");
	*/
	
	
	/*  SMB Priority 4  */
	/*Not enough space on flash
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_SNMP");	
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_JVM");
	*/
	
	/*  SMB Priority 5  */
	/*Not enough space on flash
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	*/

	token_set_y("CONFIG_RG_NETTOOLS_ARP");

	/* Include automatic daylight saving time calculation */
	token_set_y("CONFIG_RG_DATE");
	token_set_y("CONFIG_RG_TZ_FULL");
	token_set("CONFIG_RG_TZ_YEARS", "5");

	/* HW Configuration Section */
	/* Remove RNDIS until  B38111 is resolved  
	token_set_y("CONFIG_HW_USB_ETH");
	*/
	token_set_y("CONFIG_HW_IDE");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_USB_STORAGE");
	if (token_get("MODULE_RG_IPSEC"))
	    enable_module("CONFIG_HW_ENCRYPTION");

	dev_add_bridge("br0", DEV_IF_NET_INT, "eth1", NULL);
	dev_add_to_bridge_if_opt("br0", "ath0", "CONFIG_RG_ATHEROS_HW_AR5212");
	dev_add_to_bridge_if_opt("br0", "ath0", "CONFIG_RG_ATHEROS_HW_AR5416");
	
	token_set_y("CONFIG_RG_SKB_CACHE");
	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
    }
    else if (IS_DIST("CENTAUR_LSP"))
    {
	hw = "CENTAUR";
 	os = "LINUX_24";
 
 	token_set_y("CONFIG_KS8695_COMMON");
 
 	/* Software */
 	token_set_y("CONFIG_LSP_DIST");
 	token_set_y("CONFIG_SIMPLE_RAMDISK");
 	token_set_y("CONFIG_LSP_FLASH_LAYOUT");
 	token_set("CONFIG_RAMDISK_SIZE", "1024");
 	token_set_y("CONFIG_HW_ETH_WAN");
 	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("CENTAUR") || IS_DIST("CENTAUR_VGW"))
    {
 	hw = "CENTAUR";
 	os = "LINUX_24";
 
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_KS8695");
	enable_module("MODULE_RG_FOUNDATION");
 	enable_module("MODULE_RG_ADVANCED_ROUTING");
 	enable_module("MODULE_RG_VLAN");
 	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
   	enable_module("MODULE_RG_PPP");
 	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
 	enable_module("MODULE_RG_URL_FILTERING");
 	enable_module("MODULE_RG_UPNP");
 	enable_module("MODULE_RG_SNMP");
 	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
 
 	/* HW Configuration Section */
 	token_set_y("CONFIG_HW_ETH_WAN");
 	token_set_y("CONFIG_HW_ETH_LAN");
 	enable_module("CONFIG_HW_80211G_RALINK_RT2560");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
 
 	token_set("CONFIG_RG_SSID_NAME", "Centaur");
 	dev_add_bridge("br0", DEV_IF_NET_INT, "eth1", NULL);
	dev_add_to_bridge_if_opt("br0", "ra0", 
            "CONFIG_HW_80211G_RALINK_RT2561");

	if (IS_DIST("CENTAUR_VGW"))
	{
	    enable_module("MODULE_RG_VOIP_OSIP");
	    enable_module("MODULE_RG_ATA");
	    enable_module("CONFIG_HW_DSP");
	}
 
 	/* Software */
 	token_set_y("CONFIG_KS8695_COMMON");
    }
    else if (IS_DIST("RGLOADER_CENTAUR"))
    {
 	hw = "CENTAUR";
 	os = "LINUX_24";
 
 	token_set_y("CONFIG_RG_RGLOADER");
 
 	token_set_y("CONFIG_KS8695_COMMON");
 	token_set_y("CONFIG_SIMPLE_RAMDISK");
 	token_set("CONFIG_RAMDISK_SIZE", "4096");
 	token_set_y("CONFIG_PROC_FS");
 	token_set_y("CONFIG_EXT2_FS");
 	token_set_y("CONFIG_HW_ETH_WAN");
 	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("IXDP425_LSP"))
    {
	/* Hardware */
	hw = "IXDP425";

        token_set_y("CONFIG_IXP425_COMMON_LSP");
        token_set("CONFIG_IXDP425_KGDB_UART", "1");

	/* ADSL Chip Alcatel 20150 on board */
	token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20150");
	token_set_y("CONFIG_IXP425_ADSL_USE_MPHY");

	/* EEPROM */
	token_set("CONFIG_PCF8594C2", "m");

	/* IXP425 Eth driver module */
	token_set("CONFIG_IXP425_ETH", "m");

        /* Flash chip */
	token_set_y("CONFIG_IXP425_FLASH_E28F128J3");

	/* Software */
	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_GLIBC");
	token_set_y("CONFIG_SIMPLE_RAMDISK");
	token_set_y("CONFIG_LSP_FLASH_LAYOUT");
    }
    else if (IS_DIST("COYOTE"))
    {
	hw = "COYOTE";
	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_ATA");

	/*  RG Priority 2  */	
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");	
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	
	/*  RG Priority 3  */
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	/*Not enough space on flash
	enable_module("MODULE_RG_IPSEC");
        enable_module("CONFIG_HW_ENCRYPTION");
	*/
	
	/*  RG Priority 7  */
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	

	token_set_y("CONFIG_ARM_24_FAST_MODULES");

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");

	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");
        enable_module("CONFIG_HW_USB_RNDIS");
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_HW_IDE");
	enable_module("CONFIG_HW_80211G_RALINK_RT2560");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", NULL);
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");
	dev_add_to_bridge_if_opt("br0", "ra0", 
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");

	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");

    }
    else if (IS_DIST("MONTEJADE") || IS_DIST("MONTEJADE_ATM"))
    {
	hw = "MONTEJADE";
	os = "LINUX_24";

	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_DSLHOME");
	
	/*  RG Priority 2  */
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "4194304");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	
	/*  RG Priority 3  */
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section as well */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	/* Not enough RAM for ASRERISK_H323 on 32mb 
        enable_module("MODULE_RG_VOIP_ASTERISK_H323"); */

	/*  RG Priority 7  */
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SNMP");
	/* IPv6 can not work with airgo - B21867 */
	if (!token_get("CONFIG_HW_80211N_AIRGO_AGN100"))
	    enable_module("MODULE_RG_IPV6");
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");
	/* Notice: this config interfere with kernel debugger. Disable it while
	 * debugging. */
	token_set_y("CONFIG_ARM_24_FAST_MODULES");

	/* HW Configuration Section */

	enable_module("CONFIG_HW_USB_RNDIS");
	enable_module("MODULE_RG_ATA");
	
	token_set_m("CONFIG_HW_BUTTONS");
	if (IS_DIST("MONTEJADE_ATM"))
	{
	    enable_module("MODULE_RG_DSL");
	    token_set_y("CONFIG_HW_DSL_WAN");
	}
	else
	{
	    token_set_y("CONFIG_HW_ETH_WAN");
	}

        token_set_y("CONFIG_HW_SWITCH_LAN");

	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_ENCRYPTION");

	/* DSR support */
	enable_module("CONFIG_HW_DSP");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", NULL);

	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");

	dev_add_to_bridge_if_opt("br0", "wlan0",
	    "CONFIG_HW_80211N_AIRGO_AGN100");

	enable_module("CONFIG_HW_80211G_RALINK_RT2560");
	dev_add_to_bridge_if_opt("br0", "ra0",
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");
    }
    else if (IS_DIST("JIWIS8XX") || IS_DIST("JIWIS842J"))
    {
	if (IS_DIST("JIWIS8XX"))
	    hw = "JIWIS800";
	if (IS_DIST("JIWIS842J"))
	    hw = "JIWIS842J";

	token_set_y("CONFIG_RG_SMB");

	/*  SMB Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	/* When removing IPSec module you MUST remove the HW ENCRYPTION config
	 * from the HW section aswell */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
        enable_module("MODULE_RG_PBX"); 
	
	/*  SMB Priority 2  */
	enable_module("MODULE_RG_PRINTSERVER");
	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "4194304");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");

	/*  SMB Priority 3  */
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_RADIUS_SERVER");

	/*  SMB Priority 4  */
	enable_module("MODULE_RG_SNMP");
	/* IPv6 can not work with airgo - B21867 */
	if (!token_get("CONFIG_HW_80211N_AIRGO_AGN100"))
	    enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_FTP_SERVER");
	enable_module("MODULE_RG_JVM");
	
	/*  SMB Priority 5  */
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");

	/* Not enough RAM for ASRERISK_H323 on 32mb 
        enable_module("MODULE_RG_VOIP_ASTERISK_H323"); */

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");

	/* Notice: this config interfere with kernel debugger. Disable it while
	 * debugging.
	 */
	token_set_y("CONFIG_ARM_24_FAST_MODULES");
	
	/* HW Configuration Section */
	token_set_m("CONFIG_HW_LEDS");
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_m("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_ETH_WAN");
        token_set_y("CONFIG_HW_SWITCH_LAN");	
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");
	enable_module("CONFIG_HW_ENCRYPTION");

	/* DSR support */
	enable_module("CONFIG_HW_DSP");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", NULL);

	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");

	dev_add_to_bridge_if_opt("br0", "wlan0",
	    "CONFIG_HW_80211N_AIRGO_AGN100");

	if (IS_DIST("JIWIS842J"))
	    enable_module("CONFIG_HW_80211G_RALINK_RT2561");
	else
	    enable_module("CONFIG_HW_80211G_RALINK_RT2560");
	dev_add_to_bridge_if_opt("br0", "ra0",
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");

    }
    else if (IS_DIST("PCBOX_EEP_EEP_EICON") || IS_DIST("PCBOX_RTL_RTL_EICON"))
    {
	if (IS_DIST("PCBOX_EEP_EEP_EICON"))
	    hw = "PCBOX_EEP_EEP";
	else
	    hw = "ALLWELL_RTL_RTL";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_IPSEC");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_GLIBC");
    }
    else if (IS_DIST("COYOTE_LSP"))
    {
	hw = "COYOTE";

	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_GLIBC");
	token_set_y("CONFIG_SIMPLE_RAMDISK");
	token_set_y("CONFIG_LSP_FLASH_LAYOUT");
	token_set_y("CONFIG_IXP425_COMMON_LSP");
    }
    /* JStream */
    else if (IS_DIST("RGLOADER_JIWIS8XX") || IS_DIST("RGLOADER_JIWIS842J"))
    {
	/* By default DIST=JIWIS8XX will build JIWIS800 hardware (keMontajade)
	 * (see Montejade section)
	 * Other supported HW may be added later,
	 * currently HW=JIWIS800 and HW=JIWIS832 are supported
	 */
	if (IS_DIST("RGLOADER_JIWIS842J"))
	{
	    token_set_m("CONFIG_HW_BUTTONS");
	    hw = "JIWIS842J";
	}
	else
	    hw = "JIWIS800";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");
    }
    else if (IS_DIST("RGLOADER_BRUCE"))
    {
	hw = "BRUCE";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("BRUCE"))
    {
	hw = "BRUCE";

	/* Modules */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_FTP_SERVER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_80211G_ISL38XX");
	enable_module("CONFIG_HW_ENCRYPTION");
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");
	token_set_y("CONFIG_HW_IDE");
	token_set_m("CONFIG_HW_BUTTONS");
	token_set_m("CONFIG_HW_LEDS");
        token_set_y("CONFIG_HW_CLOCK");
        token_set_y("CONFIG_HW_I2C");
        token_set_y("CONFIG_HW_ENV_MONITOR");

	token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "16777216");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp1", "eth0", NULL);
    }
    else if (IS_DIST("AD6834"))
    {
	hw = "AD6834";

	/* Modules */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");

	token_set_y("CONFIG_RG_PERM_STORAGE_VENDOR_HEADER");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_DYN_LINK");

	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
        enable_module("CONFIG_HW_DSP");
	enable_module("MODULE_RG_ATA");

	/* OpenRG HW support */
 	enable_module("CONFIG_HW_80211G_RALINK_RT2560");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_DSL_WAN");
        enable_module("CONFIG_HW_USB_RNDIS");
        enable_module("CONFIG_HW_ENCRYPTION");
	dev_add_bridge("br0", DEV_IF_NET_INT, "eth0", NULL);
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");
	dev_add_to_bridge_if_opt("br0", "ra0",
	    token_get("CONFIG_HW_80211G_RALINK_RT2561") ? 
	    "CONFIG_HW_80211G_RALINK_RT2561" :
	    "CONFIG_HW_80211G_RALINK_RT2560");

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_LX4189_ADI");
    }
    else if (IS_DIST("AD6834_26") || IS_DIST("AD6834_26_FULL"))
    {
	hw = "AD6834";
	os = "LINUX_26";

	/* Modules */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	
	token_set_y("CONFIG_RG_PERM_STORAGE_VENDOR_HEADER");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	
	token_set_y("CONFIG_DYN_LINK");

	enable_module("MODULE_RG_BLUETOOTH");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_DSL_WAN");
        enable_module("CONFIG_HW_ENCRYPTION");
	dev_add_bridge("br0", DEV_IF_NET_INT, "eth0", NULL);

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_LX4189_ADI");

	/* Ramdisk */
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
    }
    else if (IS_DIST("AD6834_26_LSP"))
    {
	hw = "AD6834";
	os = "LINUX_26";
	token_set_y("CONFIG_DYN_LINK");

	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_RG_PERM_STORAGE");

	/* Devices */
	token_set_y("CONFIG_HW_ETH_LAN");

	/* Ramdisk */
	//token_set_y("CONFIG_SIMPLE_RAMDISK");
	token_set_y("CONFIG_LSP_FLASH_LAYOUT");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	//token_set_y("CONFIG_RG_MODFS_CRAMFS");
	token_set_y("CONFIG_SIMPLE_RAMDISK");

    }
    else if (IS_DIST("ALASKA"))
    {
	hw = "ALASKA";
	os = "LINUX_26";
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_LX4189");

	/*  RG Priority 1  */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_ATA");

	/*  RG Priority 2  */
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	/*doesn't work*/
//	enable_module("CONFIG_HW_USB_RNDIS");

	/*  RG Priority 3  */
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_VLAN");

	/*  RG Priority 4  */
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_FTP_SERVER");
	/*Not enough space on flash 
	enable_module("MODULE_RG_MAIL_SERVER");
	*/
	/*doesn't work see B45361 */
//	enable_module("MODULE_RG_JVM");

	/* General */
	token_set_y("CONFIG_DYN_LINK");
	token_set_y("CONFIG_KALLSYMS");

	token_set_y("CONFIG_RG_NETTOOLS_ARP");

	/* Devices */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_VDSL_WAN");

	/* this enslaves all LAN devices, if no other device is enslaved
	 * explicitly */
	dev_add_bridge("br0", DEV_IF_NET_INT, NULL);

	/* Ramdisk */
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");

	token_set_y("CONFIG_HW_LEDS");

	/* Fast Path */
       token_set_m("CONFIG_RG_FASTPATH");
       token_set("CONFIG_RG_FASTPATH_PLAT_PATH",
           "vendor/ikanos/fusiv/modules");

       /* HW QoS */
       token_set_y("CONFIG_RG_HW_QOS");
       token_set_m("CONFIG_RG_HW_QOS_PLAT_IKANOS");

       /* build options */
       token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
       token_set("LIBC_IN_TOOLCHAIN", "n");
       token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");

       /* Additional LAN Interface: Wireless LAN, IEEE 802.11a/b/g (Atheros)
	* */
       enable_module("CONFIG_RG_ATHEROS_HW_AR5212");

       /* USB */

       enable_module("CONFIG_HW_USB_HOST_EHCI");
       enable_module("CONFIG_HW_USB_HOST_OHCI");
       enable_module("CONFIG_HW_USB_STORAGE");

       /* DSP */
       enable_module("CONFIG_HW_DSP");

       /* VOIP */
       enable_module("MODULE_RG_ATA");
    }
    else if (IS_DIST("ALASKA_LSP"))
    {
	hw = "ALASKA";
	os = "LINUX_26";
	
	token_set_y("CONFIG_DYN_LINK");
	token_set_y("CONFIG_LSP_DIST");
	token_set_y("CONFIG_KALLSYMS");	

	/* Devices */
	token_set_y("CONFIG_HW_ETH_LAN");
#if 0
	/* Not tested yet... */
	token_set_y("CONFIG_HW_VDSL_WAN");
#endif

	/* Ramdisk */
	token_set_y("CONFIG_LSP_FLASH_LAYOUT");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_SIMPLE_RAMDISK");

	/* VoIP */
	enable_module("CONFIG_HW_DSP");
	token_set_y("CONFIG_RG_VOIP_DEMO");
    }
    else if (IS_DIST("JNET_SERVER"))
    {
	set_jnet_server_configs();
	token_set_y("CONFIG_RG_USE_LOCAL_TOOLCHAIN");
	token_set_y("CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY");
    }
    else if (IS_DIST("HOSTTOOLS"))
    {
	set_hosttools_configs();
	/* we want to use objects from JPKG_UML (local_*.o.i386-jungo-linux-gnu)
	 * despite the fact that we use the host local compiler */
	token_set_y("CONFIG_RG_USE_LOCAL_TOOLCHAIN");
	token_set("I386_TARGET_MACHINE", "i386-jungo-linux-gnu");
	token_set("TARGET_MACHINE", token_get_str("I386_TARGET_MACHINE"));

	token_set_y("CONFIG_GLIBC");
	token_set_y("CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
    }
    else if (IS_DIST("BCM96358_LSP"))
    {
	hw = "BCM96358";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");

	/* Devices */
	/* XXX: Add all devices when drivers are ready.
	 */
	token_set_y("CONFIG_HW_ETH_LAN");
	
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
    }
    /* Two bcm96358 boards exist, bcm96358GW and bcm96358M, for now their
     * features are the same, but in the future if different features will be 
     * implemented, must configure two dists */
    else if (IS_DIST("BCM96358"))
    {
	hw = "BCM96358";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");
	
	/*  RG Priority 1  */
    	enable_module("MODULE_RG_FOUNDATION");
    	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
        enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	if (token_get("CONFIG_RG_NETWORK_BOOT_IMAGE"))
	{
	    enable_module("CONFIG_HW_DSP");
	    enable_module("MODULE_RG_ATA");
	    enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	    enable_module("MODULE_RG_WEB_SERVER");
	    enable_module("MODULE_RG_FILESERVER");
	    enable_module("MODULE_RG_UPNP_AV");
	}

	/*  RG Priority 2  */
	if (token_get("CONFIG_RG_NETWORK_BOOT_IMAGE"))
	{
	    enable_module("MODULE_RG_L2TP");
	    enable_module("MODULE_RG_ZERO_CONFIGURATION_NETWORKING");
	    enable_module("MODULE_RG_PPTP");
	    enable_module("MODULE_RG_MAIL_FILTER");
	    enable_module("MODULE_RG_PRINTSERVER");
	    token_set("CONFIG_RG_PRINT_SERVER_SPOOL", "4194304");
	}

	/*  RG Priority 3  */
	/*Not enough space on flash
	 * XXX: wrap with CONFIG_RG_NETWORK_BOOT_IMAGE
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_BLUETOOTH");
        */

	/*  RG Priority 4  */
	/*Not enough space on flash
	 * XXX: wrap with CONFIG_RG_NETWORK_BOOT_IMAGE
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_VOIP_ASTERISK_H323");	
	enable_module("MODULE_RG_VOIP_ASTERISK_MGCP_CALL_AGENT");
	enable_module("MODULE_RG_MAIL_SERVER");	
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_FTP_SERVER");
        */	
	
	/*  RG Priority 7  */
	/*Not enough space on flash
	 * XXX: wrap with CONFIG_RG_NETWORK_BOOT_IMAGE
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
        */
    
    	token_set_y("CONFIG_ULIBC_SHARED");
    	token_set("CONFIG_RG_ADV_SERVICES_LEVEL", "1");
    	token_set_y("CONFIG_RG_SSL_VPN_SMALL_FLASH");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");

	if (token_get("CONFIG_RG_NETWORK_BOOT_IMAGE"))
	{
	    /* USB Host */
	    enable_module("CONFIG_HW_USB_HOST_EHCI");
	    enable_module("CONFIG_HW_USB_HOST_OHCI");
	    enable_module("CONFIG_HW_USB_STORAGE");
	}

	enable_module("CONFIG_HW_80211G_BCM43XX");
      	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", NULL);
      	dev_add_to_bridge_if_opt("br0", "wl0", "CONFIG_HW_80211G_BCM43XX");

	token_set("LIBC_IN_TOOLCHAIN", "y");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");

	/* USB device */
	/* RNDIS is disabled due to lack of flash.
	 * XXX: wrap with CONFIG_RG_NETWORK_BOOT_IMAGE
	enable_module("CONFIG_HW_USB_RNDIS");
	*/
	dev_add_to_bridge_if_opt("br0", "usb0", "CONFIG_HW_USB_RNDIS");
    }
    else if (IS_DIST("DWV_96358"))
    {
	hw = "DWV_BCM96358";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_MIPSEB");
	token_set_y("CONFIG_HW_DWV_96358");

	enable_module("MODULE_RG_FOUNDATION");
	/* Additional LAN Interface - See below */
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
        enable_module("MODULE_RG_DSL");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_URL_FILTERING");
	token_set("CONFIG_RG_SURFCONTROL_PARTNER_ID", "6003");
	/* The following are making the image pass the 6.5 MB and then we have 
	 * LZMA error. For now they are deleted. */
	/*
	 * enable_module("MODULE_RG_IPSEC");
	 * enable_module("MODULE_RG_PPTP");
	 * enable_module("MODULE_RG_L2TP");
	 */
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN"); /* WLAN */
	enable_module("MODULE_RG_FILESERVER");
        enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_TR_064");

	/* Additional LAN Interface */
	enable_module("CONFIG_HW_USB_RNDIS"); /* USB Slave */

	/* VOIP */
	enable_module("MODULE_RG_ATA");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("CONFIG_HW_DSP");
	/* XXX: B39186: RV MGCP is crashing on init */
	enable_module("MODULE_RG_VOIP_RV_MGCP");

	/* WLAN */
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW switch */
	token_set_y("CONFIG_HW_SWITCH_LAN");
	/* DSL */
	token_set_y("CONFIG_HW_DSL_WAN");
	/* USB Host */
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("CONFIG_HW_USB_STORAGE");	
	
	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", "wl0", "usb0", NULL);

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
        
	token_set_y("CONFIG_HW_BUTTONS");

	token_set_y("CONFIG_HW_LEDS");
#if 0
	/* B37386: Footprint Reduction cause crashes in our dist... */
	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
#endif
    }
    else if (IS_DIST("DWV_96348"))
    {
    	hw = "DWV_BCM96348";
	os = "LINUX_26";
	
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_BCM9634X");
	token_set_y("CONFIG_HW_DWV_96348");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_URL_FILTERING");
	token_set("CONFIG_RG_SURFCONTROL_PARTNER_ID", "6003");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_TR_064");

	/* Additional LAN Interface */
	enable_module("CONFIG_HW_USB_RNDIS"); /* USB Slave */

	/* WLAN */
	enable_module("CONFIG_HW_80211G_BCM43XX");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW switch */
#if 0
	/* B38935: HW switch does not work */
	token_set_y("CONFIG_HW_SWITCH_LAN");
#else
	token_set_y("CONFIG_HW_ETH_LAN");
#endif

	/* ETH WAN. TODO: Set as DMZ? */
	token_set_y("CONFIG_HW_ETH_WAN");

	/* DSL */
	enable_module("MODULE_RG_DSL");
	token_set_y("CONFIG_HW_DSL_WAN");

	/* USB Host */
#if 0
	enable_module("CONFIG_HW_USB_HOST_EHCI");
	enable_module("CONFIG_HW_USB_HOST_OHCI");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("CONFIG_HW_USB_STORAGE");   
#endif

	token_set_m("CONFIG_HW_BUTTONS");
        token_set_y("CONFIG_HW_LEDS");

	dev_add_bridge("br0", DEV_IF_NET_INT, "bcm1", "wl0", "usb0", NULL);

	/* build */
	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
    }
    /* REDBOOT distributions for IXP425 based boards */
    else if (IS_DIST("REDBOOT_RICHFIELD"))
    {
	hw = "RICHFIELD";
	os = "ECOS";
	token_set("DIST_TYPE", "BOOTLDR");
    }
    else if (IS_DIST("REDBOOT_MATECUMBE"))
    {
	hw = "MATECUMBE";
	os = "ECOS";
	token_set("DIST_TYPE", "BOOTLDR");
    }
    else if (IS_DIST("REDBOOT_COYOTE"))
    {
	hw = "COYOTE";
	os = "ECOS";
	token_set("DIST_TYPE", "BOOTLDR");
    }
    else if (IS_DIST("REDBOOT_NAPA"))
    {
	hw = "NAPA";
	os = "ECOS";
	token_set("DIST_TYPE", "BOOTLDR");
    }
    else if (IS_DIST("RGLOADER_IXDP425") ||
	IS_DIST("RGLOADER_IXDP425_FULLSOURCE") ||
	IS_DIST("RGLOADER_IXDP425_16MB") || IS_DIST("RGLOADER_IXDP425_LSP"))
    {
	hw = "IXDP425";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set("CONFIG_IXDP425_KGDB_UART", "1");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");	

	if (IS_DIST("RGLOADER_IXDP425_16MB"))
	    token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16");

	if (IS_DIST("RGLOADER_IXDP425_LSP"))
	    token_set_y("CONFIG_LSP_FLASH_LAYOUT");
    }
    else if (IS_DIST("RGLOADER_MATECUMBE"))
    {
	hw = "MATECUMBE";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("RGLOADER_NAPA"))
    {
	hw = "NAPA";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");
	token_set_y("CONFIG_IXP425_POST");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("RGLOADER_COYOTE"))
    {
	hw = "COYOTE";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set("CONFIG_SDRAM_SIZE", "64");
    }
    else if (IS_DIST("RGLOADER_COYOTE_64MB_RAM"))
    {
	hw = "COYOTE";
	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");
    }
    else if (IS_DIST("RGLOADER_COYOTE_64MB_RAM_32MB_FLASH"))
    {
	hw = "COYOTE";
	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");
	token_set_y("CONFIG_MTD_CONCAT");

	/* HW Configuration Section */
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "32"); 
	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set("CONFIG_IXP425_NUMBER_OF_FLASH_CHIPS", "2");
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("RGLOADER_COYOTE_LSP"))
    {
	hw = "COYOTE";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");
	token_set_y("CONFIG_LSP_FLASH_LAYOUT");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
    }
    else if (IS_DIST("RGLOADER_MONTEJADE"))
    {
	hw = "MONTEJADE";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");
    }
    else if (IS_DIST("RGLOADER_HG21"))
    {
	hw = "HG21";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
    }
    else if (IS_DIST("RGLOADER_BAMBOO"))
    {	
	hw = "BAMBOO";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
    }
    else if (IS_DIST("RGLOADER_GTWX5715"))
    {
	hw = "GTWX5715";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");

	/* Disable VLAN switch on production boards */
	token_set_y("CONFIG_WX5715_VLAN_SWITCH_DISABLE");
    }
    else if (IS_DIST("RGLOADER_ALLWELL_RTL_RTL"))
    {
	hw = "ALLWELL_RTL_RTL";

	token_set_y("CONFIG_RG_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
    }
    else if (IS_DIST("RGLOADER_ALLWELL_RTL_EEP"))
    {
	hw = "ALLWELL_RTL_EEP";

	token_set_y("CONFIG_RG_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
    }
    else if (IS_DIST("RGLOADER_ALLWELL_EEP_EEP"))
    {
	hw = "PCBOX_EEP_EEP";

	token_set_y("CONFIG_RG_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
    }
    else if (IS_DIST("RGLOADER_WAV54G"))
    {
	hw = "WAV54G";

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_m("CONFIG_HW_BUTTONS");
    }
    else if (IS_DIST("WRT55AG") || IS_DIST("ATHEROS_AR531X_AG_VX") ||
	IS_DIST("WRT55AG_INT"))
    {
	os = "VXWORKS";

	if (IS_DIST("WRT55AG"))
	    hw = "AR531X_WRT55AG";
	else if (IS_DIST("ATHEROS_AR531X_AG_VX"))
	    hw = "AR531X_AG";
	else
	    hw = "AR531X_G";
		
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
        enable_module("MODULE_RG_URL_FILTERING");

	/* OpenRG HW support */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_80211G_AR531X");
	if (IS_DIST("WRT55AG") || IS_DIST("ATHEROS_AR531X_AG_VX"))
	    enable_module("CONFIG_HW_80211A_AR531X");
	token_set_m("CONFIG_HW_BUTTONS");

	token_set_y("CONFIG_RG_SSI_PAGES");
	token_set_y("CONFIG_RG_LAN_BRIDGE_CONST");
	token_set_y("CONFIG_RG_RMT_MNG");
	token_set_y("CONFIG_RG_STATIC_ROUTE"); /* Static Routing */
	token_set_y("CONFIG_RG_ENTFY"); /* Email notification */
	token_set("CONFIG_SDRAM_SIZE", "16");

	token_set_y("CONFIG_RG_WLAN_STA_STATISTICS_WBM");

	/* WLAN */
	token_set_y("CONFIG_RG_WPA_WBM");
	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	token_set_y("CONFIG_RG_8021X_WBM");

	if (!IS_DIST("WRT55AG_INT"))
	    dev_add_bridge("br0", DEV_IF_NET_INT, "ae0", "vp0", "vp256", NULL);
	else
	    dev_add_bridge("br0", DEV_IF_NET_INT, "ae2", "vp256", NULL);
    }
    else if (IS_DIST("AR2313_LIZZY") || IS_DIST("BOCHS_LIZZY"))
    {
	os = "VXWORKS";

	/* This distribution is FOUNDATION_CORE plus many features */
	token_set_y("CONFIG_RG_FOUNDATION_CORE");

    	/* DHCP Server */
	token_set_y("CONFIG_RG_DHCPS");

	/* Telnet Server */
	token_set_y("CONFIG_RG_TELNETS");

	/* Bridging */
	token_set_y("CONFIG_RG_BRIDGE");
	
	/* NAT/NAPT */	
	token_set_y("CONFIG_RG_NAT");

	/* ALG support */
	token_set_y("CONFIG_RG_ALG");
	token_set_y("CONFIG_RG_ALG_SIP");
	token_set_y("CONFIG_RG_ALG_H323");
	token_set_y("CONFIG_RG_ALG_AIM");
	token_set_y("CONFIG_RG_ALG_MSNMS");
	token_set_y("CONFIG_RG_ALG_PPTP");
	token_set_y("CONFIG_RG_ALG_IPSEC");
	token_set_y("CONFIG_RG_ALG_L2TP");
	token_set_y("CONFIG_RG_ALG_ICMP");
	token_set_y("CONFIG_RG_ALG_PORT_TRIGGER");
	token_set_y("CONFIG_RG_ALG_FTP");
	token_set_y("CONFIG_RG_PROXY_RTSP");

	/* Firewall */
	token_set_y("CONFIG_RG_MSS");
	
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	/* Vlan */
	enable_module("MODULE_RG_VLAN");

	/* Additional features */
    	token_set_y("CONFIG_RG_DHCPR");
	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");

	if (IS_DIST("AR2313_LIZZY"))
	{
	    /* WLAN */
	    enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	    token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	    token_set_y("CONFIG_RG_WLAN_REPEATING");
	    token_set_y("CONFIG_RG_WDS_CONN_NOTIFIER");
	    token_set("CONFIG_RG_SSID_NAME", "wireless");
	}

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	if (IS_DIST("AR2313_LIZZY"))
	{
	    enable_module("CONFIG_HW_80211G_AR531X");
	    token_set_y("CONFIG_HW_BUTTONS");
	}

	token_set_y("CONFIG_RG_SSI_PAGES");
	token_set_y("CONFIG_RG_LAN_BRIDGE_CONST");
	token_set_y("CONFIG_RG_RMT_MNG");
	token_set_y("CONFIG_RG_STATIC_ROUTE"); /* Static Routing */

	/* Email notification */
	token_set_y("CONFIG_RG_ENTFY");

	/* Event Logging */
    	token_set_y("CONFIG_RG_EVENT_LOGGING");

	token_set("CONFIG_SDRAM_SIZE", "8");

	/* Statistics control */
	token_set_y("CONFIG_RG_WLAN_STA_STATISTICS_WBM");

	if (IS_DIST("AR2313_LIZZY"))
	{
	    hw = "AR531X_G";

	    dev_add_bridge("br0", DEV_IF_NET_INT, "ae2", "vp256", NULL);
	}
	else /* BOCHS_LIZZY */
	{
	    hw = "I386_BOCHS";

	    dev_add_bridge("br0", DEV_IF_NET_INT, "ene0", "ene1", NULL);
	}
    }
    else if (IS_DIST("AR2313_ZIPPY") || IS_DIST("BOCHS_ZIPPY"))
    {
	os = "VXWORKS";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");

	/* Additional features */
    	token_set_y("CONFIG_RG_DHCPR");
	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	if (IS_DIST("AR2313_ZIPPY"))
	{
	    enable_module("CONFIG_HW_80211G_AR531X");
	    token_set_y("CONFIG_HW_BUTTONS");
	}

	token_set_y("CONFIG_RG_SSI_PAGES");
	token_set_y("CONFIG_RG_LAN_BRIDGE_CONST");

	if (IS_DIST("AR2313_ZIPPY"))
	{
	    enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	    token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	    token_set_y("CONFIG_AR531X_DEBUG");
	}

	token_set("CONFIG_SDRAM_SIZE", "16");

	/* Statistics control */
	token_set_y("CONFIG_RG_WLAN_STA_STATISTICS_WBM");

	if (IS_DIST("AR2313_ZIPPY"))
	{
	    hw = "AR531X_G";
	    dev_add_bridge("br0", DEV_IF_NET_INT, "ae2", "vp256", NULL);
	} 
	else /* BOCHS_ZIPPY */
	{
	    hw = "I386_BOCHS";
	    dev_add_bridge("br0", DEV_IF_NET_INT, "ene0", "ene1", NULL);
	}
    }
    else if (IS_DIST("ATHEROS_AR531X_VX"))
    {
	os = "VXWORKS";
	hw = "AR531X_G";

	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	token_set_y("CONFIG_RG_TFTP_UPGRADE");
	token_set_y("CONFIG_RG_TFTP_SERVER_PASSWORD");
	token_set_y("CONFIG_RG_ENTFY");	/* Email notification */
    	token_set_y("CONFIG_RG_EVENT_LOGGING"); /* Event Logging */
	token_set_y("CONFIG_RG_URL_KEYWORD_FILTER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_80211G_AR531X");
	token_set_y("CONFIG_HW_BUTTONS");

	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	token_set_y("CONFIG_RG_SSI_PAGES");
	token_set_y("CONFIG_AR531X_DEBUG");
	token_set_y("CONFIG_RG_LAN_BRIDGE_CONST");
	token_set("CONFIG_SDRAM_SIZE", "16");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ae2", "vp256", NULL);
    }
    else if (IS_DIST("WRT108G_VX"))
    {
	os = "VXWORKS";
	hw = "WRT108G";

	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	token_set_y("CONFIG_RG_TFTP_UPGRADE");
	token_set_y("CONFIG_RG_TFTP_SERVER_PASSWORD");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_80211G_AR531X");
	token_set_y("CONFIG_HW_BUTTONS");

	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	token_set_y("CONFIG_GUI_LINKSYS");
	token_set_y("CONFIG_RG_LAN_BRIDGE_CONST");

	token_set_y("CONFIG_RG_RGLOADER_CLI_CMD");
	dev_add_bridge("br0", DEV_IF_NET_INT, "ae2", "ar1", NULL);

	token_set("CONFIG_SDRAM_SIZE", "16");

	/* Features not ready */
#if 0
	token_set_y("CONFIG_RG_IGMP_PROXY");
	
	token_set_y("CONFIG_RG_RMT_UPGRADE_IMG_IN_MEM");
#endif
    }
    else if (IS_DIST("TI_404_VX_EVAL") || IS_DIST("T_TI_404_VX") ||
    	IS_DIST("ARRIS_TI_404_VX"))
    {
	os = "VXWORKS";
	hw = "TI_404";

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_SNMP");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_CABLE_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

    }
    else if (IS_DIST("T_TI_404_VX_CH") || IS_DIST("ARRIS_TI_404_VX_CH"))
    {
	os = "VXWORKS";
	hw = "TI_404";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_CABLEHOME");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
    	token_set_y("CONFIG_RG_EVENT_LOGGING"); /* Event Logging */

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_CABLE_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	if (IS_DIST("SMC_TI_404_VX_CH"))
	{
	    enable_module("MODULE_RG_UPNP");
	    enable_module("MODULE_RG_ADVANCED_ROUTING");
	}

	dev_add_bridge("br0", DEV_IF_NET_INT, "cbl0", "lan0", NULL);
    }
    else if (IS_DIST("TI_TNETC440_VX_CH") || IS_DIST("HITRON_TNETC440_VX_CH") ||
	IS_DIST("HITRON_TNETC440_VX"))
    {
	os = "VXWORKS";
	hw = "TI_404";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
    	token_set_y("CONFIG_RG_EVENT_LOGGING");	/* Event Logging */
	if (!IS_DIST("HITRON_TNETC440_VX"))
	    enable_module("MODULE_RG_CABLEHOME");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_CABLE_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	dev_add_bridge("br0", DEV_IF_NET_INT, "cbl0", "lan0", NULL);

	if (IS_DIST("HITRON_TNETC440_VX_CH") || IS_DIST("HITRON_TNETC440_VX"))
	    token_set_y("CONFIG_HITRON_BSP");
    }
    else if (IS_DIST("TI_404_VX_CH_EVAL"))
    {
	os = "VXWORKS";
	hw = "TI_404";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_CABLEHOME");
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_CABLE_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	dev_add_bridge("br0", DEV_IF_NET_INT, "cbl0", "lan0", NULL);
    }
    else if (IS_DIST("TI_WPA_VX"))
    {
	os = "VXWORKS";
	hw = "TI_TNETWA100";
	token_set_y("CONFIG_RG_WPA");
	token_set_y("CONFIG_RG_8021X_RADIUS");
	token_set_y("CONFIG_RG_OPENSSL");
    }
    else if (IS_DIST("I386_BOCHS_VX"))
    {
	os = "VXWORKS";
	hw = "I386_BOCHS";

	token_set_y("CONFIG_RG_SMB");

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	dev_add_bridge("br0", DEV_IF_NET_INT, "ene0", "ene1", NULL);
    }
    else if (IS_DIST("ALLWELL_RTL_RTL_VALGRIND"))
    {
	hw = "ALLWELL_RTL_RTL";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_ADVANCED_ROUTING");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");

	token_set_y("CONFIG_VALGRIND");
    }
    else if (IS_DIST("ALLWELL_RTL_RTL_ISL38XX"))
    {
	hw = "ALLWELL_RTL_RTL_ISL38XX";

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
        enable_module("CONFIG_HW_80211G_ISL38XX");

	dev_add_bridge("br0", DEV_IF_NET_INT, "rtl1", "eth0", NULL);
    }
    else if (IS_DIST("RGLOADER_UML"))
    {
	hw = "UML";
	os = "LINUX_24";

	token_set_y("CONFIG_RG_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_y("CONFIG_RG_TELNETS");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
    }
    else if (IS_DIST("JPKG_SRC"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"src\"");
	token_set_y("CONFIG_RG_JPKG_SRC");

	set_jpkg_dist_configs("JPKG_SRC");
    }
    else if (IS_DIST("JPKG_UML"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"uml\"");
        token_set_y("CONFIG_RG_JPKG_UML");

	set_jpkg_dist_configs("JPKG_UML");
    }
    else if (IS_DIST("JPKG_ARMV5B"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"armv5b\"");
	token_set_y("CONFIG_RG_JPKG_ARMV5B");
	    
	token_set_y("CONFIG_GLIBC");
	token_set_y("GLIBC_IN_TOOLCHAIN");

	set_jpkg_dist_configs("JPKG_ARMV5B");
    }
    else if (IS_DIST("JPKG_ARMV5L"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"armv5l\"");
        token_set_y("CONFIG_RG_JPKG_ARMV5L");

	set_jpkg_dist_configs("JPKG_ARMV5L");
    }
    else if (IS_DIST("JPKG_ARMV4L"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"armv4l\"");
        token_set_y("CONFIG_RG_JPKG_ARMV4L");

	set_jpkg_dist_configs("JPKG_ARMV4L");
    }
    else if (IS_DIST("JPKG_KS8695"))
    {
	hw = "JPKG";
	token_set("JPKG_ARCH", "\"ks8695\"");
	token_set_y("CONFIG_RG_JPKG_KS8695");

	set_jpkg_dist_configs("JPKG_KS8695");
    }
    else if (IS_DIST("JPKG_PPC"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"ppc\"");
        token_set_y("CONFIG_RG_JPKG_PPC");

	set_jpkg_dist_configs("JPKG_PPC");
    }
    else if (IS_DIST("JPKG_MIPSEB"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"mipseb\"");
        token_set_y("CONFIG_RG_JPKG_MIPSEB");

	set_jpkg_dist_configs("JPKG_MIPSEB");
    }
    else if (IS_DIST("JPKG_BCM9634X"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"bcm9634x\"");
        token_set_y("CONFIG_RG_JPKG_BCM9634X");

	set_jpkg_dist_configs("JPKG_BCM9634X");
    }
    else if (IS_DIST("JPKG_SB1250"))
    {
	hw = "JPKG";
	token_set("JPKG_ARCH", "\"sb1250\"");

	token_set_y("CONFIG_RG_JPKG_SB1250");

	set_jpkg_dist_configs("JPKG_SB1250");
    }
    else if (IS_DIST("JPKG_OCTEON"))
    {
	hw = "JPKG";
	token_set("JPKG_ARCH", "\"octeon\"");

	token_set_y("CONFIG_RG_JPKG_OCTEON");

	set_jpkg_dist_configs("JPKG_OCTEON");
    }    
    else if (IS_DIST("JPKG_ARM_920T_LE"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"arm-920t-le\"");
        token_set_y("CONFIG_RG_JPKG_ARM_920T_LE");

	set_jpkg_dist_configs("JPKG_ARM_920T_LE");
    }
    else if (IS_DIST("JPKG_MIPSEB_INFINEON"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"mipseb-infineon\"");
        token_set_y("CONFIG_RG_JPKG_MIPSEB_INFINEON");

	set_jpkg_dist_configs("JPKG_MIPSEB_INFINEON");
    }
    else if (IS_DIST("JPKG_LX4189_ADI"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"lx4189-adi\"");
	token_set_y("CONFIG_RG_JPKG_LX4189");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_LX4189"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"lx4189\"");
	token_set_y("CONFIG_RG_JPKG_LX4189");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_ARMV5L_FEROCEON"))
    {
	hw = "JPKG";
        token_set("JPKG_ARCH", "\"armv5l-feroceon\"");
	token_set_y("CONFIG_RG_JPKG_ARMV5L_FEROCEON");

	set_jpkg_dist_configs(dist);
    }
    else if (IS_DIST("JPKG_LOCAL_I386"))
    {
	hw = NULL;
        token_set("JPKG_ARCH", "\"local-i386\"");
	token_set_y("CONFIG_RG_JPKG_LOCAL_I386");

	set_jpkg_dist_configs("JPKG_LOCAL_I386");
    }
    else if (IS_DIST("UML_LSP"))
    {
	hw = "UML";
	os = "LINUX_24";

	token_set_y("CONFIG_RG_UML");

	token_set_y("CONFIG_LSP_DIST");
    }
    else if (IS_DIST("UML_LSP_26"))
    {
	hw = "UML";
	os = "LINUX_26";

	token_set_y("CONFIG_RG_UML");
	
	token_set_y("CONFIG_GLIBC");

	token_set_y("CONFIG_LSP_DIST");
    }
    else if (IS_DIST("UML") || IS_DIST("UML_GLIBC") || IS_DIST("UML_DUAL_WAN")
	|| IS_DIST("UML_ATM") || IS_DIST("UML_26") || IS_DIST("UML_VALGRIND"))
    {
	hw = "UML";

	if (IS_DIST("UML_26") || IS_DIST("UML_VALGRIND"))
	    os = "LINUX_26";

	if (IS_DIST("UML_GLIBC"))
	{
	    token_set_y("CONFIG_GLIBC");
	    token_set_y("GLIBC_IN_TOOLCHAIN");
	}

	if (IS_DIST("UML_VALGRIND"))
	    token_set_y("CONFIG_VALGRIND");

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_ROUTE_MULTIWAN");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_TR_064");
	enable_module("MODULE_RG_JVM");
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_REDUCE_SUPPORT");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_ANTIVIRUS_LAN_PROXY");
	enable_module("MODULE_RG_ANTIVIRUS_NAC");
	enable_module("MODULE_RG_RADIUS_SERVER");
	enable_module("MODULE_RG_VOIP_ASTERISK_SIP");
	enable_module("MODULE_RG_PBX");
	enable_module("MODULE_RG_BLUETOOTH");
	enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
	enable_module("MODULE_RG_WEB_SERVER");
	enable_module("MODULE_RG_PRINTSERVER");
	enable_module("MODULE_RG_MAIL_SERVER");
	enable_module("MODULE_RG_FTP_SERVER");
        enable_module("CONFIG_HW_80211G_UML_WLAN");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_UML_LOOP_STORAGE"); /* UML Disk Emulation */
	token_set_y("CONFIG_HW_ETH_WAN");
	enable_module("CONFIG_HW_DSP");
	if (IS_DIST("UML_DUAL_WAN"))
	    token_set_y("CONFIG_HW_ETH_WAN2");
	if (IS_DIST("UML_ATM"))
	{
	    enable_module("MODULE_RG_DSL");
	    token_set_y("CONFIG_HW_DSL_WAN");
	}
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set_y("CONFIG_RG_MT_PROFILING_FULL_INFO");

	dev_add_bridge("br0", DEV_IF_NET_INT, "eth1", "eth2", NULL);
	dev_add_to_bridge("br0", "uml_wlan0");
    }
    else if (IS_DIST("UML_BHR"))
    {
	hw = "UML";
    
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	token_set_y("CONFIG_RG_8021Q_IF");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");

	token_set_m("CONFIG_RG_PPPOE_RELAY"); /* PPPoE Relay */
	token_set_y("CONFIG_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");

#if 0
	token_set_y("CONFIG_GUI_ACTIONTEC");
#endif
	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");

	dev_add_bridge("br0", DEV_IF_NET_INT, "eth1", "eth2", NULL);

	/* Dist specific configuration */
	token_set_y("CONFIG_RG_PROD_IMG");

	token_set("RG_PROD_STR", "Actiontec Home Router");
	token_set("ACTION_TEC_MODEL_NAME", "MI424WR"); 
	actiontec_set_names();

	/* Tasklet me harder */
	token_set_y("CONFIG_TASKLET_ME_HARDER");

	/* Make readonly GUI */
	token_set_y("CONFIG_RG_WBM_READONLY_USERS_GROUPS");
    }
    else if (IS_DIST("UML_IPPHONE"))
    {
	hw = "UML";

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_VOIP_RV_H323");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_DSP");

	/* VoIP */
	token_set_y("CONFIG_RG_IPPHONE");
    }
    else if (IS_DIST("UML_IPPHONE_VALGRIND"))
    {
	hw = "UML";

	token_set_y("CONFIG_RG_SMB");

	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_VOIP_RV_H323");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_DSP");

	token_set_y("CONFIG_VALGRIND");

	/* VoIP */
	token_set_y("CONFIG_RG_IPPHONE");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
    }
    else if (IS_DIST("UML_ATA_OSIP"))
    {
	hw = "UML";

	token_set_y("CONFIG_RG_SMB");
	token_set("CONFIG_RG_JPKG_DIST", "JPKG_UML");
        
	/* ALL OpenRG Available Modules - ALLMODS */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	/* For Customer distributions only:
	 * When removing IPV6 you must replace in feature_config.c the line 
	 * if(token_get("MODULE_RG_IPV6")) with if(1) */
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_VOIP_OSIP");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	enable_module("CONFIG_HW_DSP");

	/* VoIP */
	enable_module("MODULE_RG_ATA");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set_y("CONFIG_RG_MT_PROFILING_FULL_INFO");
    }
    else if (IS_DIST("RGTV"))
    {
	hw = "PUNDIT";

	/* Basic Linux support */
	token_set_y("CONFIG_LSP_DIST");
	/* RGTV components are incompatible with ulibc */
	token_set_y("CONFIG_GLIBC");
	token_set_y("CONFIG_RGTV");
	/* Kernel modules */
	token_set_m("CONFIG_SOUND_ICH");
	token_set_y("CONFIG_VIDEO_DEV");
	token_set_m("CONFIG_SMB_FS");
    	token_set_y("CONFIG_NLS");
    	token_set_y("CONFIG_NLS_DEFAULT");
	token_set_y("CONFIG_BLK_DEV_SIS5513");
	token_set_m("CONFIG_B44");
    }
    else if (IS_DIST("MC524WR"))
    {
	hw = "MC524WR";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_OCTEON");

	enable_module("MODULE_RG_FOUNDATION");
	if (!token_get("ACTION_TEC_SMALL_IMG"))
	{
	    enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	    enable_module("MODULE_RG_VLAN");
	    enable_module("MODULE_RG_URL_FILTERING");
	    enable_module("MODULE_RG_QOS");
	}
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_DSLHOME");

	token_set_y("CONFIG_BHR_FEATURES");

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_ULIBC");
	token_set_y("CONFIG_DYN_LINK");

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_AUTODETECT");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_MOCA");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_LEDS");
	token_set_m("CONFIG_HW_BUTTONS");

	/* ACTION_TEC */
	token_set_y("ACTION_TEC_MC524WR");

	/* ACTION_TEC */
	if (!token_get("ACTION_TEC_SMALL_IMG"))
	{
	    //enable_module("MODULE_RG_WPS");
	    if (token_is_y("CONFIG_MC524WR_REV_E"))
		enable_module("CONFIG_RG_ATHEROS_HW_AR5212"); 
	    if (token_is_y("CONFIG_MC524WR_REV_F") || token_is_y("CONFIG_MC524WR_REV_G"))
		enable_module("CONFIG_RG_ATHEROS_HW_AR5416");

	    token_set_y("CONFIG_RG_WIRELESS_TOOLS");
	}

	token_set_y("CONFIG_RG_AUTO_WAN_DETECTION");
	token_set_y("CONFIG_RG_ACTIVE_WAN");

	/* 
	 * Set distribution specific interface names here. These
	 * interface names must not be hard coded anywhere else.
	 * The defines for interface names in rg_config.h must be used
	 * everyehere else.
	 */
	act_lan_eth_ifname="eth0";
	act_wan_eth_ifname="eth1";
	act_lan_moca_ifname="clink0";
	act_wan_moca_ifname="clink1";

	act_wan_eth_pppoe_ifname="ppp0";
	act_wan_moca_pppoe_ifname="ppp1";
	act_wifi_ap_ifname="wifi0";
	act_atheros_vap_primary_ifname="ath0";
	act_atheros_vap_secondary_ifname="ath2";
	act_atheros_vap_public_ifname="ath3";
	act_atheros_vap_help_ifname="ath1";
	act_ralink_vap_ifname="ra0";
	act_def_bridge_ifname="br0";

	dev_add_bridge(act_def_bridge_ifname, DEV_IF_NET_INT, act_lan_eth_ifname, NULL);
	if (!token_get("ACTION_TEC_SMALL_IMG"))
	{
	    if (token_is_y("CONFIG_MC524WR_REV_E"))
	    {
		dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_atheros_vap_primary_ifname, "CONFIG_RG_ATHEROS_HW_AR5212");
		if (token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID"))
		    dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_atheros_vap_secondary_ifname, "CONFIG_RG_ATHEROS_HW_AR5212");
	    }
	    if (token_is_y("CONFIG_MC524WR_REV_F") || token_is_y("CONFIG_MC524WR_REV_G"))
	    {
		dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_atheros_vap_primary_ifname, "CONFIG_RG_ATHEROS_HW_AR5416");
		if (token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID"))
		    dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_atheros_vap_secondary_ifname, "CONFIG_RG_ATHEROS_HW_AR5416");
	    }
	}
	dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_lan_moca_ifname, "CONFIG_HW_MOCA"); 

	token_set_y("CONFIG_RG_VENDOR_FACTORY_SETTINGS");
	token_set_y("CONFIG_VENDOR_ACTIONTEC");
	token_set("RG_PROD_STR", "Wireless Broadband Router");
	token_set_y("CONFIG_RG_WBM_READONLY_USERS_GROUPS");

	/* ACTION_TEC - Start */

	token_set_y("ACTION_TEC_STB_PROTECTION");
	token_set("ACTION_TEC_MODEL_NAME", "MI424WR-GEN3G"); 

	actiontec_set_names();

	if (ACTION_TEC_NCS_ENABLED)
	    actiontec_retail_specific_features();

	if (token_get("ACTION_TEC_VERIZON") && token_is_y("ACTION_TEC_VERIZON"))
	{
	    verizon_specific_features();
	}
	else
	{
	    token_set_y("CONFIG_GUI_RG");
	    token_set_y("CONFIG_GUI_RG2");
	    token_set_y("CONFIG_GUI_AEI_TELUS");
	    token_set_y("CONFIG_GUI_AEI_QWEST");
	}

	if (token_get("ACTION_TEC_NAS_FEATURES") || 
		token_get("ACTION_TEC_MEDIA_SERVER") || 
		token_get("ACTION_TEC_FTP_SERVER") ||
		token_get("ACTION_TEC_FILE_SERVER") ||
		token_get("ACTION_TEC_DMS") ||
		token_get("ACTION_TEC_PRINTER_SERVER"))
	{
	    token_set_y("ACTION_TEC_USB_STORAGE");
	}
	else
	{
	    if (!token_get("ACTION_TEC_SMALL_IMG"))
	    {
		/* RMA USB testing */
		token_set_y("ACTION_TEC_RMA_USB");
		token_set_y("CONFIG_HOTPLUG");
	    }
	}

	if (token_get("ACTION_TEC_USB_STORAGE") ||
		token_get("ACTION_TEC_RMA_USB"))
	{
	    token_set_y("ACTION_TEC_HW_USB_HOST_EHCI");
	    token_set_y("ACTION_TEC_HW_USB_HOST_UHCI");
	    token_set_y("ACTION_TEC_HW_USB_HOST_OHCI");
	    token_set_m("ACTION_TEC_HW_USB_STORAGE");
	    token_set_y("CONFIG_SCSI");
	    token_set_y("CONFIG_BLK_DEV_SD");
	    //token_set("CONFIG_SD_EXTRA_DEVS", "32");
	    token_set_y("CONFIG_USB_STORAGE");
	    //token_set_y("ACTION_TEC_DISK_MNG");
	} 

	/* RMA WPS testing */
	token_set_y("ACTION_TEC_RMA_WPS");
	/* ACTION_TEC - End */
    } // end IS_DIST(MC524WR)
    else if (IS_DIST("MI424WR"))
    {
	/* ACTION_TEC */
	hw = dist;

	token_set_y("ACTION_TEC_MI424WR");

	/* 
	 * Set distribution specific interface names here. These
	 * interface names must not be hard coded anywhere else.
	 * The defines for interface names in rg_config.h must be used
	 * everyehere else.
	 */
	act_lan_eth_ifname="ixp1";
	act_wan_eth_ifname="ixp0";
	act_lan_moca_ifname="clink1";
	act_wan_moca_ifname="clink0";
	act_wan_eth_pppoe_ifname="ppp0";
	act_wan_moca_pppoe_ifname="ppp1";
	act_wifi_ap_ifname="wifi0";
	act_atheros_vap_primary_ifname="ath0";
	act_atheros_vap_secondary_ifname="ath2";
	act_atheros_vap_public_ifname="ath3";
	act_atheros_vap_help_ifname="ath1";
	act_ralink_vap_ifname="ra0";
	act_def_bridge_ifname="br0";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5B");
	/* VPN SSL Demo */
#if 0
	/* XXX modify set_dist_license() to compile with the RnD license,
	 * since these modules are not purchased and will not compile with
	 * the Actiontec's license.
	 */
	/* XXX Consider removing CONFIG_RG_BGP and CONFIG_RG_OSPF from advanced
	 * routing when enabling the lines below, due to footprint limitatios.
	 */
	enable_module("MODULE_RG_SSL_VPN");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
#endif

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	if (token_get("MODULE_RG_DSL"))
	    token_set_y("CONFIG_HW_DSL_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_MOCA");
	token_set_y("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_LEDS");

	enable_module("CONFIG_RG_ATHEROS_HW_AR5212");

	token_set("CONFIG_RG_SSID_NAME", "openrg");

	dev_add_bridge(act_def_bridge_ifname, DEV_IF_NET_INT, act_lan_eth_ifname, NULL);
#if 0
	dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_ralink_vap_ifname, "CONFIG_HW_80211G_RALINK_RT2561");
	dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_ralink_vap_ifname, "CONFIG_HW_80211G_RALINK_RT2560");
#endif
	dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_atheros_vap_primary_ifname, "CONFIG_RG_ATHEROS_HW_AR5212");
	dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_atheros_vap_primary_ifname, "CONFIG_RG_ATHEROS_HW_AR5416");
	if (token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID"))
	    dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_atheros_vap_secondary_ifname, "CONFIG_RG_ATHEROS_HW_AR5212");
	dev_add_to_bridge_if_opt(act_def_bridge_ifname, act_lan_moca_ifname, "CONFIG_HW_MOCA");

	/* Dist specific configuration */

	/* Customer specific defines */
	if (ACTION_TEC_NCS_ENABLED)
	{
	    actiontec_retail_specific_features();
	}

	/* Verizon specific features */
	if (token_get("ACTION_TEC_VERIZON") && token_is_y("ACTION_TEC_VERIZON"))
	{
	    verizon_specific_features();
	}
	else
	{
	    token_set_y("CONFIG_GUI_RG");
	    token_set_y("CONFIG_GUI_RG2");
	    token_set_y("CONFIG_GUI_AEI_TELUS");
	    token_set_y("CONFIG_GUI_AEI_QWEST");
	}

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set_y("CONFIG_RG_VENDOR_FACTORY_SETTINGS");

	token_set_y("CONFIG_VENDOR_ACTIONTEC");
	token_set_y("CONFIG_BHR_FEATURES");
	token_set_y("CONFIG_RG_AUTO_WAN_DETECTION");
	token_set_y("CONFIG_RG_ACTIVE_WAN");

	token_set_y("CONFIG_RG_DSLHOME_VOUCHERS");
	token_set_y("CONFIG_ARM_24_FAST_MODULES");
	token_set_y("CONFIG_TASKLET_ME_HARDER");

	/* Make readonly GUI */
	token_set_y("CONFIG_RG_WBM_READONLY_USERS_GROUPS");

	/* GPL distribution */
	if (token_get("CONFIG_RG_GPL"))
	{
	    token_set_y("CONFIG_RG_GDBSERVER");
	    token_set_y("CONFIG_RG_TERMCAP");
	    token_set_y("CONFIG_ATM");
	    token_set_y("CONFIG_RG_IPROUTE2_UTILS");
	    token_set_y("CONFIG_RG_PPPOE_RELAY");
	    token_set_y("CONFIG_RG_STAR");
	    token_set_y("CONFIG_RG_LIBMAD");
	    token_set_y("CONFIG_RG_KAFFE");
	    token_set_y("CONFIG_ARCH_IXP425_JIWIS800");
	    token_set_y("CONFIG_RALINK_RT2561");
	    token_set_y("CONFIG_RG_HOSTAPD");
	    token_set_y("CONFIG_RG_QUAGGA");
	    token_set_y("CONFIG_RG_BGP");
	    token_set_y("CONFIG_RG_OSPF");
	}

	token_set("RG_PROD_STR", "Wireless Broadband Router");
        token_set("ACTION_TEC_MODEL_NAME", "MI424WR");

	actiontec_set_names();
    }
    else if (IS_DIST("RGLOADER_MI424WR"))
    {
	hw = dist + strlen("RGLOADER_");

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_LEDS");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");

	token_set("RG_PROD_STR", "Wireless Broadband Router");
	token_set("ACTION_TEC_MODEL_NAME", "MI424WR"); 

	actiontec_set_names();
	token_set_y("CONFIG_VENDOR_ACTIONTEC");

	token_set_y("CONFIG_HW_BUTTONS");
    }
    else if (IS_DIST("RI408"))
    {
	hw = dist;

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_BUTTONS");
	token_set_y("CONFIG_HW_LEDS");
	if (token_get("CONFIG_RALINK_RT2561"))
	    enable_module("CONFIG_HW_80211G_RALINK_RT2561");
	else
	    enable_module("CONFIG_HW_80211G_RALINK_RT2560");

	token_set("CONFIG_RG_SSID_NAME", "openrg");
	dev_add_bridge("br0", DEV_IF_NET_INT, "ixp0", "ra0", NULL);
	
	/* Dist specific configuration */
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");
	token_set("RG_PROD_STR", "Broadband Router");
	token_set("ACTION_TEC_MODEL_NAME", "MI424WR"); 

	actiontec_set_names();
	token_set_y("CONFIG_VENDOR_ACTIONTEC");
	
	token_set_y("CONFIG_RG_DSLHOME_VOUCHERS");
	token_set_y("CONFIG_ARM_24_FAST_MODULES");
	token_set_y("CONFIG_TASKLET_ME_HARDER");
    }
    else if (IS_DIST("RGLOADER_RI408"))
    {
	hw = dist + strlen("RGLOADER_");

	token_set_y("CONFIG_RG_RGLOADER");
	token_set_y("CONFIG_IXP425_COMMON_RGLOADER");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_ETH_LAN2");
	token_set_y("CONFIG_HW_ETH_LAN");
	token_set_y("CONFIG_HW_LEDS");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");

	token_set("RG_PROD_STR", "Broadband Router");
	token_set("ACTION_TEC_MODEL_NAME", "MI424WR"); 

	actiontec_set_names();
	token_set_y("CONFIG_VENDOR_ACTIONTEC");

	token_set_y("CONFIG_HW_BUTTONS");
    }
    else if (IS_DIST("FULL"))
    {
	token_set_y("CONFIG_RG_SMB");

	/* All OpenRG available Modules */
	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_IPSEC");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_SNMP");
	enable_module("MODULE_RG_IPV6");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_VOIP_RV_SIP");
	enable_module("MODULE_RG_VOIP_RV_MGCP");
	enable_module("MODULE_RG_VOIP_RV_H323");
        enable_module("MODULE_RG_DSL");
        enable_module("MODULE_RG_FILESERVER");
	enable_module("MODULE_RG_UPNP_AV");
        enable_module("MODULE_RG_PRINTSERVER");
        enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
        enable_module("MODULE_RG_CABLEHOME");
        enable_module("MODULE_RG_VODSL");
    }
    else if (IS_DIST("OLYMPIA_P402_LSP"))
    {
	hw = "OLYMPIA_P40X";
	os = "LINUX_26";

	token_set_y("CONFIG_LSP_DIST");

	/* Devices */
	/* XXX: Add all devices when drivers are ready.
	 */
	token_set_y("CONFIG_HW_ETH_LAN");
	
	token_set_y("CONFIG_ULIBC");
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_SIMPLE_RAMDISK");
 	
	token_set_y("CONFIG_LSP_FLASH_LAYOUT");

	token_set_y("CONFIG_PROC_FS");
 	token_set_y("CONFIG_EXT2_FS");
    }
    else if (IS_DIST("OLYMPIA_P402"))
    {
	hw = "OLYMPIA_P40X";
	os = "LINUX_26";

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_VLAN");
#if 0
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_PPTP");
	enable_module("MODULE_RG_L2TP");
	enable_module("MODULE_RG_DSL");
#endif	
	enable_module("MODULE_RG_QOS");
	enable_module("MODULE_RG_MAIL_FILTER");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_TR_064");

	/* Devices */
	/* XXX: Add all devices when drivers are ready.
	 */
	token_set_y("CONFIG_HW_ETH_LAN");
	
	token_set_y("CONFIG_ULIBC");
	token_set("LIBC_IN_TOOLCHAIN", "n");

	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
        token_set_y("CONFIG_RG_MODFS_CRAMFS");

	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");

	token_set_y("CONFIG_PROC_FS");
 	token_set_y("CONFIG_EXT2_FS");
	
	token_set_m("CONFIG_HW_BUTTONS");
 
	/* XXX Dependency of api and spi modules on crypto should be resolved
	 */
        token_set_m("CONFIG_P400_CRYPTO");

    }
    else if (IS_DIST("FEROCEON"))
    {
	/* Marvell SoC */
	hw = "FEROCEON";
	os = "LINUX_26";

	token_set("CONFIG_RG_JPKG_DIST", "JPKG_ARMV5L_FEROCEON");
	token_set_y("CONFIG_FEROCEON");
	token_set_y("CONFIG_FEROCEON_COMMON");
	token_set_y("CONFIG_ARCH_FEROCEON");
	token_set_y("CONFIG_ARCH_FEROCEON_KW2");

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_ULIBC");
	token_set_y("CONFIG_DYN_LINK");

#if 0
	if (IS_DIST("FEROCEON_LSP"))
	    token_set_y("CONFIG_LSP_DIST");
#endif

	enable_module("MODULE_RG_FOUNDATION");
	enable_module("MODULE_RG_WLAN_AND_ADVANCED_WLAN");
	enable_module("MODULE_RG_ADVANCED_ROUTING");
	enable_module("MODULE_RG_ADVANCED_MANAGEMENT");
	enable_module("MODULE_RG_VLAN");
	enable_module("MODULE_RG_FIREWALL_AND_SECURITY");
	enable_module("MODULE_RG_URL_FILTERING");
	enable_module("MODULE_RG_UPNP");
	enable_module("MODULE_RG_PPP");
	enable_module("MODULE_RG_DSLHOME");
	enable_module("MODULE_RG_QOS");

	enable_module("MODULE_RG_PSE");
	token_set_y("CONFIG_BHR_FEATURES");

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_ULIBC");
	token_set_y("CONFIG_DYN_LINK");

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set_y("CONFIG_RG_NETTOOLS_ARP");
	
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
 	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set_y("CONFIG_RG_PERM_STORAGE_JFFS2");
	token_set("CONFIG_RG_FFS_DEV", "/dev/mtdblock0");
	token_set("CONFIG_RG_FFS_MNT_DIR", "/mnt/jffs2");

	/* HW Configuration Section */
	token_set_y("CONFIG_HW_AUTODETECT");
	token_set_y("CONFIG_HW_ETH_WAN");
	token_set_y("CONFIG_HW_MOCA");
	token_set_y("CONFIG_HW_SWITCH_LAN");
	token_set_y("CONFIG_HW_LEDS");
	token_set_m("CONFIG_HW_BUTTONS");

	token_set_y("CONFIG_RG_AUTO_WAN_DETECTION");
	token_set_y("CONFIG_RG_WAN_SWAP");

	token_set_y("CONFIG_RG_VENDOR_FACTORY_SETTINGS");
	token_set_y("CONFIG_VENDOR_ACTIONTEC");
	token_set("RG_PROD_STR", "Wireless Broadband Router");
	token_set_y("CONFIG_RG_WBM_READONLY_USERS_GROUPS");

	token_set_y("CONFIG_HOTPLUG");

	enable_module("MODULE_RG_WPS");
	enable_module("CONFIG_RG_ATHEROS_HW_AR5416");
	token_set_y("CONFIG_RG_WIRELESS_TOOLS");

	/* 
	 * Set distribution specific interface names here. These
	 * interface names must not be hard coded anywhere else.
	 * The defines for interface names in rg_config.h must be used
	 * everyehere else.
	 */
	act_lan_eth_ifname="eth0";
	act_wan_eth_ifname="eth1";
	act_lan_moca_ifname="clink0";
	act_wan_moca_ifname="clink1";

	act_wan_eth_pppoe_ifname="ppp0";
	act_wan_moca_pppoe_ifname="ppp1";
	act_wifi_ap_ifname="wifi0";
	act_atheros_vap_primary_ifname="ath0";
	act_atheros_vap_help_ifname="ath1";
	act_atheros_vap_secondary_ifname="ath2";
	act_atheros_vap_public_ifname="ath3";
	act_ralink_vap_ifname="ra0";
	act_def_bridge_ifname="br0";

	dev_add_bridge("br0", DEV_IF_NET_INT, NULL);
	dev_add_to_bridge("br0", "eth0");
	dev_add_to_bridge_if_opt("br0", "ath0", "CONFIG_RG_ATHEROS_HW_AR5416");

	/* ACTION_TEC - START */
	token_set_y("ACTION_TEC_STB_PROTECTION");
	token_set("ACTION_TEC_MODEL_NAME", "MI424WR-GEN3I"); 
	token_set_y("ACTION_TEC_BACKUP_CONF");

	actiontec_set_names();

	if (token_get("ACTION_TEC_VERIZON") && token_is_y("ACTION_TEC_VERIZON"))
	    verizon_specific_features();

	token_set_y("CONFIG_RG_TEMP_PASSWORD");

	/* RMA USB testing */
	token_set_y("ACTION_TEC_RMA_USB");

	if (token_get("ACTION_TEC_RMA_USB"))
	{
	    token_set_y("ACTION_TEC_HW_USB_HOST_EHCI");
	    token_set_y("ACTION_TEC_HW_USB_HOST_UHCI");
	    token_set_y("ACTION_TEC_HW_USB_HOST_OHCI");
	    token_set_m("ACTION_TEC_HW_USB_STORAGE");
	    token_set_y("CONFIG_SCSI");
	    token_set_y("CONFIG_BLK_DEV_SD");
	    //token_set("CONFIG_SD_EXTRA_DEVS", "32");
	    token_set_y("CONFIG_USB_STORAGE");
	    //token_set_y("ACTION_TEC_DISK_MNG");
	} 

	/* RMA WPS testing */
	token_set_y("ACTION_TEC_RMA_WPS");
	/* ACTION_TEC - END */
    }
    else
	conf_err("invalid DIST=%s\n", dist);

    if (hw && strcmp(hw, "JPKG") && !(*os))
	os = "LINUX_24";

    token_set("CONFIG_RG_DIST", dist);
}
