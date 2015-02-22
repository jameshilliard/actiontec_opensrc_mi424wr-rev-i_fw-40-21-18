/****************************************************************************
 *
 * rg/pkg/build/hw_config.c
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "config_opt.h"
#include "create_config.h"

/* ACTION_TEC */
extern char *act_lan_eth_ifname;
extern char *act_wan_eth_ifname;
extern char *act_lan_moca_ifname;
extern char *act_wan_moca_ifname;
extern char *act_wan_eth_pppoe_ifname;
extern char *act_wan_moca_pppoe_ifname;
extern char *act_wifi_ap_ifname;
extern char *act_atheros_vap_primary_ifname;
extern char *act_atheros_vap_secondary_ifname;
extern char *act_atheros_vap_public_ifname;
extern char *act_atheros_vap_help_ifname;
extern char *act_ralink_vap_ifname;
extern char *act_def_bridge_ifname;
/* ACTION_TEC */

void airgo_agn100_add(void)
{
    token_set_y("CONFIG_NET_RADIO");
    token_set_y("CONFIG_NET_WIRELESS");
    token_set_y("CONFIG_RG_VENDOR_WLAN_SEC");
    token_set_y("CONFIG_RG_WPA_WBM");
    token_set_y("CONFIG_RG_8021X_WBM");
    token_set_m("CONFIG_AGN100");
    token_set_y("CONFIG_NETFILTER");
    dev_add("wlan0", DEV_IF_AGN100, DEV_IF_NET_INT);
    dev_can_be_missing("wlan0");
}

#ifdef CONFIG_RG_DO_DEVICES
 #define ralink_rt256x_add(type,wl_name,config) _ralink_rt256x_add(type,wl_name,config) 
static void _ralink_rt256x_add(dev_if_type_t type, char *wl_name,char *config)
#else
 #define ralink_rt256x_add(type,wl_name,config) _ralink_rt256x_add(wl_name,config) 
static void _ralink_rt256x_add(char *wl_name,char *config)
#endif    
{
    token_set_y("CONFIG_NET_RADIO");
    token_set_y("CONFIG_NET_WIRELESS");
    if (token_get("MODULE_RG_WLAN_AND_ADVANCED_WLAN"))
    {
	token_set_y("CONFIG_RG_VENDOR_WLAN_SEC");
	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	token_set_y("CONFIG_RG_WPA_WBM");
	token_set_y("CONFIG_RG_8021X_WBM");
	token_set_y("CONFIG_RG_WLAN_AUTO_CHANNEL_SELECT");
	token_set_y("CONFIG_RG_WSEC_DAEMON");
    }
    token_set_m(config); 
    if (token_get("CONFIG_RG_JPKG"))
	token_set_dev_type(type);
    else
    {
	dev_add(wl_name, type, DEV_IF_NET_INT); 
	dev_can_be_missing(wl_name);
    }
}

static void ralink_rt2560_add(char *wl_name)
{
    ralink_rt256x_add(DEV_IF_RT2560, wl_name, "CONFIG_RALINK_RT2560");
}

static void ralink_rt2561_add(char *wl_name)
{
    ralink_rt256x_add(DEV_IF_RT2561, wl_name, "CONFIG_RALINK_RT2561");
}

void bcm43xx_add(char *wl_name)
{
    if (wl_name)
    {
	dev_add(wl_name, DEV_IF_BCM43XX, DEV_IF_NET_INT);
	dev_can_be_missing(wl_name);
    }
    else
	token_set_dev_type(DEV_IF_BCM43XX);

    token_set_y("CONFIG_NET_RADIO");
    token_set_y("CONFIG_NET_WIRELESS");
    token_set("CONFIG_BCM43XX_MODE", "AP");
    if (token_get("MODULE_RG_WLAN_AND_ADVANCED_WLAN"))
    {
	token_set_y("CONFIG_RG_VENDOR_WLAN_SEC");
	token_set_y("CONFIG_RG_8021X_MD5");
	token_set_y("CONFIG_RG_8021X_TLS");
	token_set_y("CONFIG_RG_8021X_TTLS");
	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	token_set_y("CONFIG_RG_WPA_WBM");
	token_set_y("CONFIG_RG_WPA_BCM");
	token_set_y("CONFIG_RG_WSEC_DAEMON");
    }
}

void isl_softmac_add(void)
{
    token_set_m("CONFIG_ISL_SOFTMAC");
    dev_add("eth0", DEV_IF_ISL_SOFTMAC, DEV_IF_NET_INT);
    token_set_y("CONFIG_RG_WLAN_AUTO_CHANNEL_SELECT");
    dev_can_be_missing("eth0");
}

#ifdef CONFIG_RG_DO_DEVICES
static dev_if_type_t atheros_get_vap_type(void)
{
    if (token_get("CONFIG_RG_ATHEROS_HW_AR5212"))
	return DEV_IF_AR5212_VAP;
    else if (token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	return DEV_IF_AR5416_VAP;
    else
	conf_err("Atheros hardware is not specified");

    /* not reached */
    return DEV_IF_AR5212_VAP;
}
#endif

static void atheros_ar5xxx_add(char *wl_name, char *vap_name, ...)
{
    va_list ap;

    token_set_y("CONFIG_NET_RADIO");
    token_set_y("CONFIG_NET_WIRELESS");
    if (token_get("MODULE_RG_WLAN_AND_ADVANCED_WLAN"))
    {
	token_set_y("CONFIG_RG_VENDOR_WLAN_SEC");
	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	token_set_y("CONFIG_RG_WPA_WBM");
	token_set_y("CONFIG_RG_8021X_WBM");
	token_set_y("CONFIG_RG_WLAN_AUTO_CHANNEL_SELECT");
	token_set_y("CONFIG_RG_WSEC_DAEMON");
    }
    if (token_get("CONFIG_RG_JPKG"))
    {
	token_set_dev_type(DEV_IF_WIFI);
	if (vap_name)
	    token_set_dev_type(atheros_get_vap_type());
    }
    else
    {
	dev_add(wl_name, DEV_IF_WIFI, DEV_IF_NET_INT); 
	dev_can_be_missing(wl_name);

	for (va_start(ap, vap_name); vap_name; vap_name = va_arg(ap, char *))
	    dev_add(vap_name, atheros_get_vap_type(), DEV_IF_NET_INT);
	va_end(ap);
    }
}

static void uml_wlan_add(char *wl_name)
{
    token_set_y("CONFIG_NET_RADIO");
    token_set_y("CONFIG_NET_WIRELESS");
    if (token_get("MODULE_RG_WLAN_AND_ADVANCED_WLAN"))
    {
	token_set_y("CONFIG_RG_VENDOR_WLAN_SEC");
	token_set_y("CONFIG_RG_RADIUS_WBM_IN_CONN");
	token_set_y("CONFIG_RG_WPA_WBM");
	token_set_y("CONFIG_RG_8021X_WBM");
	token_set_y("CONFIG_RG_WLAN_AUTO_CHANNEL_SELECT");
	token_set_y("CONFIG_RG_WSEC_DAEMON");
    }
    if (token_get("CONFIG_RG_JPKG"))
	token_set_dev_type(DEV_IF_UML_WLAN);
    else
    {
	dev_add(wl_name, DEV_IF_UML_WLAN, DEV_IF_NET_INT); 
	dev_can_be_missing(wl_name);
    }
}

void hardware_features(void)
{
    option_t *hw_tok;

    if (!hw)
    {
	token_set("CONFIG_RG_HW", "NO_HW");
	token_set("CONFIG_RG_HW_DESC_STR", "No hardware - local targets only");
	token_set_y("CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY");
	return;
    }

    hw_tok = option_token_get(openrg_hw_options, hw);

    if (!hw_tok->value)
	conf_err("No description available for HW=%s\n", hw);

    token_set("CONFIG_RG_HW", hw);
    token_set("CONFIG_RG_HW_DESC_STR", hw_tok->value);

    if (IS_HW("DANUBE") || IS_HW("TWINPASS"))
    {
	if (IS_HW("DANUBE"))
	{
	    token_set("BOARD", "Danube");
	    token_set_y("CONFIG_RG_HW_DANUBE");
	}
	else
	{
	    token_set("BOARD", "Twinpass");
	    token_set_y("CONFIG_RG_HW_TWINPASS");
	}
	token_set("FIRM", "Infineon");

	set_big_endian(1);
	token_set("LIBC_ARCH", "mips");
	token_set("ARCH", "mips");
	token_set_y("CONFIG_HAS_MMU");
	token_set("CONFIG_BOOTLDR_UBOOT_COMP", "lzma");

	token_set_y("CONFIG_RG_FOOTPRINT_REDUCTION");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");

	token_set("LIBC_IN_TOOLCHAIN", "n");
	token_set_y("CONFIG_RAMFS");
	token_set_y("CONFIG_COPY_CRAMFS_TO_RAM");
	token_set_y("CONFIG_CRAMFS_FS");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS1");
	token_set_y("CONFIG_BOOTLDR_UBOOT");

	token_set_y("CONFIG_EXPERIMENTAL");
	token_set_y("CONFIG_NEW_IRQ");
	token_set_y("CONFIG_NONCOHERENT_IO");
	token_set_y("CONFIG_NEW_TIME_C");
	token_set_y("CONFIG_NONCOHERENT_IO");

	token_set_y("CONFIG_PCI");
	token_set_y("CONFIG_PCI_AUTO");
	token_set_y("CONFIG_DANUBE_PCI");
	token_set_y("CONFIG_DANUBE_PCI_HW_SWAP");

#if 0
	/* TODO: */
	token_set_y("CONFIG_SCSI");
	token_set_y("CONFIG_BLK_DEV_SD");
	token_set("CONFIG_SD_EXTRA_DEVS", "5");
#endif
	token_set_y("CONFIG_CPU_MIPS32");
	token_set_y("CONFIG_CPU_HAS_LLSC");
	token_set_y("CONFIG_CPU_HAS_SYNC");
	token_set_y("CONFIG_PAGE_SIZE_4KB");
	token_set_y("CONFIG_SYSCTL");
	token_set_y("CONFIG_KCORE_ELF");
	token_set_y("CONFIG_BINFMT_ELF");

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_CRYPTO_DANUBE");
	    token_set_y("CONFIG_CRYPTO_DEV_DANUBE");
	    token_set_y("CONFIG_CRYPTO_DEV_DANUBE_DES");
	    token_set_y("CONFIG_CRYPTO_DEV_DANUBE_AES");

	    /* as soon as B45763 is resolved
	    token_set_y("CONFIG_CRYPTO_DEV_DANUBE_SHA1");
	    token_set_y("CONFIG_CRYPTO_DEV_DANUBE_MD5");
	    */

	    token_set_y("CONFIG_CRYPTO_DEV_DANUBE_DMA");
	}

	/* MTD */
	token_set_y("CONFIG_MTD");
	token_set_y("CONFIG_MTD_PARTITIONS");
	token_set_y("CONFIG_MTD_CHAR");
	token_set_y("CONFIG_MTD_BLOCK");
	token_set_y("CONFIG_MTD_CFI");
	token_set_y("CONFIG_MTD_GEN_PROBE");
	token_set_y("CONFIG_MTD_CFI_INTELEXT");
	token_set_y("CONFIG_MTD_CFI_AMDSTD");
	token_set_y("CONFIG_MTD_DANUBE");
	/* TODO: Check flash size */
	token_set("CONFIG_MTD_DANUBE_FLASH_SIZE", "8");

	token_set_y("CONFIG_BLK_DEV_LOOP");

	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	token_set_y("CONFIG_SERIAL_IFX_ASC");
	token_set_y("CONFIG_SERIAL_IFX_ASC_CONSOLE");
	token_set("CONFIG_IFX_ASC_DEFAULT_BAUDRATE", "115200");
	token_set_y("CONFIG_IFX_ASC_CONSOLE_ASC1");

	token_set_y("CONFIG_DANUBE");
	token_set_y("CONFIG_DANUBE_MPS");
	token_set_y("CONFIG_DANUBE_MPS_PROC_DEBUG");

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	/* Networking */
	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_y("CONFIG_ATM_DANUBE");
	    token_set_y("CONFIG_IFX_ATM_OAM");
	    token_set_y("CONFIG_DANUBE_MEI");
	    token_set_y("CONFIG_DANUBE_MEI_MIB");
	    dev_add("atm0", DEV_IF_DANUBE_ATM, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_VDSL_WAN"))
	{
	    token_set_m("CONFIG_DANUBE_PPA");
	    token_set_y("CONFIG_RG_VINAX_VDSL");
	    token_set("CONFIG_RG_VINAX_VDSL_BASE_ADDR", "0x14000000");
	    token_set("CONFIG_RG_VINAX_VDSL_IRQ", "99");
	    dev_add("eth1", DEV_IF_VINAX_VDSL, DEV_IF_NET_EXT);

	    /* VDSL daemon application require threads support. */
	    token_set_y("CONFIG_RG_THREADS");
	    token_set_y("CONFIG_RG_LIBC_CUSTOM_STREAMS");

	    token_set_y("CONFIG_HW_ETH_WAN");
	}

	/* Must be after VDSL because CONFIG_DANUBE_PPA conflicts with
	 * CONFIG_DANUBE_ETHERNET. */
	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    if (!token_get("CONFIG_DANUBE_PPA") ||
		token_get("CONFIG_RG_JPKG"))
	    {
		token_set_y("CONFIG_DANUBE_ETHERNET");
	    }
	    if (token_get("CONFIG_HW_SWITCH_LAN"))
	    {
		if (IS_HW("DANUBE"))
		    dev_add("eth0", DEV_IF_ADM6996_HW_SWITCH, DEV_IF_NET_INT);
		if (IS_HW("TWINPASS"))
		    dev_add("eth0", DEV_IF_PSB6973_HW_SWITCH, DEV_IF_NET_INT);
	    }
	}

	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set_m("CONFIG_RG_ATHEROS");
	    atheros_ar5xxx_add("wifi0", "ath0", NULL);
	    dev_set_dependency("ath0", "wifi0");
	    /* TODO: Check if we need it... */
	    token_set_y("CONFIG_RG_WSEC_DAEMON");
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_VINETIC");
	    token_set("CONFIG_VINETIC_LINES_PER_CHIP", "2");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    token_set_y("CONFIG_RG_DSP_THREAD");
	}

	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_USB");
	    token_set_y("CONFIG_USB_DEVICEFS");
	    token_set_y("CONFIG_GENERIC_ISA_DMA");
	    token_set_y("CONFIG_USB_EHCI_HCD");
	    /* the host controller driver MUST be 'y' and not 'm' because on
	     * some DANUBE boards the insmode halts the board (B45740) */
	    token_set_m("CONFIG_USB_DWC3884_HCD");
	}

	if (token_get("CONFIG_RG_HW_QOS") && IS_HW("TWINPASS"))
	    token_set("CONFIG_RG_QOS_PRIO_BANDS", "4");
	
#if 0
	token_set_y("CONFIG_NET_HW_FLOWCONTROL"); /* Check... */
#endif
    }
    if (IS_HW("ALLWELL_RTL_RTL"))
    {
	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_8139TOO");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("rtl0", DEV_IF_RTL8139, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("rtl1", DEV_IF_RTL8139, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("rtl0", DEV_IF_RTL8139, DEV_IF_NET_INT);

	token_set_y("CONFIG_PCBOX");
    }

    if (IS_HW("WADB100G"))
    {
	token_set_y("CONFIG_BCM963XX_COMMON");
	token_set_y("CONFIG_BCM96348");
	
	if (token_get("CONFIG_RG_OS_LINUX_24"))
	{
	    token_set_m("CONFIG_BCM963XX_BOARD");
	    token_set_m("CONFIG_BCM963XX_MTD");
	}
	if (token_get("CONFIG_RG_OS_LINUX_26"))
	{
	    token_set_y("CONFIG_SERIAL_CORE");
	    token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	    if (token_get("CONFIG_HW_ETH_WAN"))
	    {
		token_set_m("CONFIG_BCM963XX_ETH"); 
		dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_EXT); 
	    }
	    token_set_y("CONFIG_BCM963XX_MTD");
	}
	
	token_set_y("CONFIG_BCM963XX_SERIAL");

        token_set_y("CONFIG_MTD_CFI_AMDSTD");
	
	token_set("CONFIG_BCM963XX_BOARD_ID", "96348GW-10");
	token_set("CONFIG_BCM963XX_CHIP", "6348");
	token_set("CONFIG_BCM963XX_SDRAM_SIZE", "16");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "4");
	/* this value is taken from 
	 * vendor/broadcom/bcm963xx/linux-2.6/bcmdrivers/opensource/include/bcm963xx/board.h*/
	token_set("CONFIG_RG_FLASH_START_ADDR", "0xbfc00000");

	token_set_y("CONFIG_PCI");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_m("CONFIG_BCM963XX_ADSL");
	    token_set_m("CONFIG_BCM963XX_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("bcm_atm0", DEV_IF_BCM963XX_ADSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);

	    if (!token_get("CONFIG_HW_SWITCH_LAN"))
	    {
		token_set_m("CONFIG_BCM963XX_ETH");
		dev_add("bcm1", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);
	    }
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm1", DEV_IF_BCM5325A_HW_SWITCH, DEV_IF_NET_INT);
	}
	
	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	{
	    if (token_get("CONFIG_RG_OS_LINUX_24"))
		token_set_y("CONFIG_BCM963XX_WLAN");
	    if (token_get("CONFIG_RG_OS_LINUX_26"))
                token_set_y("CONFIG_BCM4318");

	    bcm43xx_add("wl0");
	}

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_USB_RNDIS");
	    token_set_m("CONFIG_BCM963XX_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	token_set("BOARD", "WADB100G");
	token_set("FIRM", "Broadcom");
    }

    if (IS_HW("PUNDIT"))
    {
	token_set_y("CONFIG_PCBOX");
	/* the following should be set for screen(rather then serial) output */
	token_set("CONFIG_RG_CONSOLE_DEVICE", "console");
	/* TODO: add Pundit specific modules */
    }

    if (IS_HW("ASUS6020VI"))
    {
	token_set_y("CONFIG_BCM963XX_COMMON");
	token_set_y("CONFIG_BCM96348");
	if (token_get("CONFIG_RG_OS_LINUX_24"))
	{
	    token_set_m("CONFIG_BCM963XX_BOARD");
	    token_set_m("CONFIG_BCM963XX_MTD");
	}
	if (token_get("CONFIG_RG_OS_LINUX_26"))
	{
	    token_set_y("CONFIG_SERIAL_CORE");
	    token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	    if (token_get("CONFIG_HW_ETH_WAN"))
	    {
		token_set_m("CONFIG_BCM963XX_ETH"); 
		dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_EXT); 
	    }
	    token_set_y("CONFIG_BCM963XX_MTD");
	}
	token_set_y("CONFIG_BCM963XX_SERIAL");

        token_set_y("CONFIG_MTD_CFI_AMDSTD");
	
	token_set("CONFIG_BCM963XX_BOARD_ID", "AAM6020VI");
	token_set("CONFIG_BCM963XX_CHIP", "6348");
	token_set("CONFIG_BCM963XX_SDRAM_SIZE", "16");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "4");
	/* this value is taken from 
	 * vendor/broadcom/bcm963xx/linux-2.6/bcmdrivers/opensource/include/bcm963xx/board.h*/
	token_set("CONFIG_RG_FLASH_START_ADDR", "0xbfc00000");

	token_set_y("CONFIG_PCI");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_m("CONFIG_BCM963XX_ADSL");
	    token_set_m("CONFIG_BCM963XX_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("bcm_atm0", DEV_IF_BCM963XX_ADSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm1", DEV_IF_BCM5325E_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm1", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	{
	    if (token_get("CONFIG_RG_OS_LINUX_24"))
	    {
		token_set_y("CONFIG_BCM963XX_WLAN");
		token_set_y("CONFIG_BCM963XX_WLAN_4318");
	    }
	    if (token_get("CONFIG_RG_OS_LINUX_26"))
		token_set_y("CONFIG_BCM4318");

	    bcm43xx_add("wl0");
	}

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	token_set("BOARD", "ASUS6020VI");
	token_set("FIRM", "Asus");
    }

    if (IS_HW("WADB102GB"))
    {
	token_set_y("CONFIG_BCM963XX_COMMON");
	token_set_y("CONFIG_BCM96348");
	token_set_y("CONFIG_BCM963XX_SERIAL");
	token_set_m("CONFIG_BCM963XX_BOARD");
	token_set("CONFIG_BCM963XX_BOARD_ID", "WADB102GB");
	token_set("CONFIG_BCM963XX_CHIP", "6348");
	token_set("CONFIG_BCM963XX_SDRAM_SIZE", "16");

	token_set_y("CONFIG_RG_MTD_DEFAULT_PARTITION");
	token_set("CONFIG_MTD_PHYSMAP_LEN", "0x00400000");
	token_set("CONFIG_MTD_PHYSMAP_START", "0x1FC00000");

	token_set_y("CONFIG_PCI");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_m("CONFIG_BCM963XX_ADSL");
	    token_set_m("CONFIG_BCM963XX_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("bcm_atm0", DEV_IF_BCM963XX_ADSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm1", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm1", DEV_IF_BCM5325E_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	{
	    token_set_y("CONFIG_BCM963XX_WLAN");
	    token_set_y("CONFIG_BCM963XX_WLAN_4318");

	    bcm43xx_add("wl0");
	}

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	token_set("BOARD", "WADB102GB");
	token_set("FIRM", "Belkin");
    }
    
    if (IS_HW("MPC8272ADS"))
    {
	token_set_y("CONFIG_MPC8272ADS");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0" );

	token_set("FIRM", "freescale");
	token_set("BOARD", "MPC8272ADS");

	/* Ethernet drivers */

	if (token_get("CONFIG_HW_ETH_FEC"))
	{
	    /* CPM2 Options */

	    token_set_y("CONFIG_FEC_ENET");
	    token_set_y("CONFIG_USE_MDIO");
            token_set_y("CONFIG_FCC_DM9161");
	    token_set_y("CONFIG_FCC1_ENET");
	    token_set_y("CONFIG_FCC2_ENET");

	    dev_add("eth0", DEV_IF_MPC82XX_ETH, DEV_IF_NET_EXT);
	    dev_add("eth1", DEV_IF_MPC82XX_ETH, DEV_IF_NET_INT);
	}
	else if (token_get("CONFIG_HW_ETH_EEPRO1000"))
	{
	    token_set_y("CONFIG_E1000");
	    
	    dev_add("eth0", DEV_IF_EEPRO1000, DEV_IF_NET_EXT);
	    dev_add("eth1", DEV_IF_EEPRO1000, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_USB_STORAGE"))
	{		
	    token_set_y("CONFIG_USB");
	    token_set_y("CONFIG_USB_M82XX_HCD");
	    token_set_y("CONFIG_USB_DEVICEFS");
	}

	if (token_get("MODULE_RG_BLUETOOTH"))
	    token_set_y("CONFIG_BT_HCIUSB");

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth2", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	    dev_can_be_missing("eth2");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	{
	    ralink_rt2560_add("ra0");
	    token_set("CONFIG_RALINK_RT2560_TIMECSR", "0x40");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	if (token_get("CONFIG_HW_ENCRYPTION"))
	    token_set_y("CONFIG_IPSEC_USE_MPC_CRYPTO");
    }

    if (IS_HW("EP8248"))
    {
	token_set_y("CONFIG_EP8248");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0" );

	token_set("FIRM", "Embedded Planet");
	token_set("BOARD", "EP8248");

	/* Ethernet drivers */

	if (token_get("CONFIG_HW_ETH_FEC"))
	{
	    /* CPM2 Options */

	    token_set_y("CONFIG_FEC_ENET");
	    token_set_y("CONFIG_USE_MDIO");
            token_set_y("CONFIG_FCC_LXT971");
	    token_set_y("CONFIG_FCC1_ENET");
	    token_set_y("CONFIG_FCC2_ENET");

	    dev_add("eth0", DEV_IF_MPC82XX_ETH, DEV_IF_NET_INT);
	    dev_add("eth1", DEV_IF_MPC82XX_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_USB");
	    token_set_y("CONFIG_USB_DEVICEFS");
	    token_set_y("CONFIG_USB_M82XX_HCD");
	}
	
	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}
    }
    
    if (IS_HW("MPC8349ITX"))
    {
	token_set_y("CONFIG_MPC8349_ITX");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");

	token_set("FIRM", "Freescale");
	token_set("BOARD", "mpc8349-itx");

	/* Ethernet drivers */
	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_y("CONFIG_GIANFAR");
	    token_set_y("CONFIG_CICADA_PHY");
	    token_set_y("CONFIG_PHYLIB");
	    
	    dev_add("eth0", DEV_IF_MPC82XX_ETH, DEV_IF_NET_EXT);
	}
	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_y("CONFIG_GIANFAR");
	    token_set_y("CONFIG_CICADA_PHY");
	    token_set_y("CONFIG_PHYLIB");

	    dev_add("eth1", DEV_IF_MPC82XX_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_USB_ETH"))
	{
	    token_set_y("CONFIG_USB_GADGET");
	    token_set_y("CONFIG_USB_ETH");
	    token_set_y("CONFIG_USB_MPC");
	    token_set_y("CONFIG_USB_GADGET_MPC");
	    token_set_y("CONFIG_USB_GADGET_DUALSPEED");
	    token_set_y("CONFIG_USB_ETH_RNDIS");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}
	
	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_USB");
	    token_set_y("CONFIG_USB_EHCI_HCD");
	    token_set_y("CONFIG_USB_EHCI_ROOT_HUB_TT");
	    token_set_y("CONFIG_USB_DEVICEFS");
	    token_set_y("CONFIG_FSL_USB20");
	    token_set_y("CONFIG_MPH_USB_SUPPORT");
	    token_set_y("CONFIG_MPH0_USB_ENABLE");
	    token_set_y("CONFIG_MPH0_ULPI");
	    if (!token_get("CONFIG_USB_MPC"))
	    {
		token_set_y("CONFIG_MPH1_USB_ENABLE");
		token_set_y("CONFIG_MPH1_ULPI");
	    }
	}

	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set_m("CONFIG_RG_ATHEROS");
	    atheros_ar5xxx_add("wifi0", "ath0", NULL);
	    dev_set_dependency("ath0", "wifi0");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	{
	    ralink_rt2560_add("ra0");
	    token_set("CONFIG_RALINK_RT2560_TIMECSR", "0x40");
	}
	
	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_IDE"))
	{
	    token_set_y("CONFIG_SCSI");
	    token_set_y("CONFIG_SCSI_SATA");
	    token_set_y("CONFIG_SCSI_SATA_SIL");
	}

	if (token_get("CONFIG_HW_ENCRYPTION"))
	    token_set_m("CONFIG_MPC8349E_SEC2x");

	/* TODO: Do we need some CONFIG_HW_xxx to enable flash support? */
	/* MTD */
#if 0
	token_set_y("CONFIG_MTD");
	token_set_y("CONFIG_MTD_CFI_AMDSTD");
	token_set_y("CONFIG_MTD_CFI_I1");
	token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_2");
	token_set_y("CONFIG_MTD_CFI_UTIL");
	
	/* TODO: write custom mpc8349-itx MTD driver, as it has 2 8Mb chips.
	 * Currently generic physmap driver is used, only for the second chip */
	token_set_y("CONFIG_MTD_PHYSMAP");
    	token_set("CONFIG_MTD_PHYSMAP_BANKWIDTH", "2");
	token_set("CONFIG_MTD_PHYSMAP_LEN", "0x800000");
	token_set("CONFIG_MTD_PHYSMAP_START", "0xFE800000");
#endif	
	token_set_y("CONFIG_RG_PHY_POLL");
    }

    if (IS_HW("ALLWELL_RTL_RTL_ISL38XX"))
    {
	token_set_m("CONFIG_8139TOO");
	if (token_get("CONFIG_RG_RGLOADER"))
	{
	    dev_add("rtl1", DEV_IF_RTL8139, DEV_IF_NET_INT);
	    dev_add("rtl0", DEV_IF_RTL8139, DEV_IF_NET_INT);
	}
	else
	{
	    dev_add("rtl0", DEV_IF_RTL8139, DEV_IF_NET_EXT);
	    dev_add("rtl1", DEV_IF_RTL8139, DEV_IF_NET_INT);
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	}
	token_set_y("CONFIG_PCBOX");
    }

    if (IS_HW("ALLWELL_RTL_EEP"))
    {
	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_8139TOO");
	    dev_add("rtl0", DEV_IF_RTL8139, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_EEPRO100");
	    dev_add("eep0", DEV_IF_EEPRO100, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_LAN2"))
	{
	    token_set_m("CONFIG_8139TOO");
	    dev_add("rtl0", DEV_IF_RTL8139, DEV_IF_NET_INT);
	}
	
	token_set_y("CONFIG_PCBOX");
    }

    if (IS_HW("ALLWELL_ATMNULL_RTL"))
    {
	token_set_m("CONFIG_8139TOO");
	token_set_y("CONFIG_ATM");
	token_set_y("CONFIG_ATM_NULL");
	dev_add("rtl0", DEV_IF_RTL8139, DEV_IF_NET_INT);
	dev_add("atmnull0", DEV_IF_ATM_NULL, DEV_IF_NET_EXT);
	token_set_y("CONFIG_PCBOX");
    }

    if (IS_HW("PCBOX_EEP_EEP"))
    {
	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_EEPRO100");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("eep0", DEV_IF_EEPRO100, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("eep1", DEV_IF_EEPRO100, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("eep0", DEV_IF_EEPRO100, DEV_IF_NET_INT);

	token_set_y("CONFIG_PCBOX");
    }

    if (IS_HW("CX82100"))
    {
	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_CNXT_EMAC");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("cnx0", DEV_IF_CX821XX_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("cnx1", DEV_IF_CX821XX_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("cnx1", DEV_IF_CX821XX_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_USB_RNDIS"))
	    token_set_y("CONFIG_USB_RNDIS");

	token_set_y("CONFIG_ARMNOMMU");
	token_set_y("CONFIG_CX821XX_COMMON");
	token_set_y("CONFIG_BD_GOLDENGATE");
	token_set_y("CONFIG_CHIP_CX82100");
	token_set_y("CONFIG_PHY_KS8737");

	token_set("FIRM", "Conexant");
	token_set("BOARD", "CX82100");
    }

    if (IS_HW("ADM5120P"))
    {
	token_set_y("CONFIG_ADM5120_COMMON");

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("adm0", DEV_IF_ADM5120_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("adm1", DEV_IF_ADM5120_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_VINETIC");
	    token_set("CONFIG_VINETIC_LINES_PER_CHIP", "2");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	}
    }

    if (IS_HW("BCM91125E") || IS_HW("COLORADO"))
    {
	token_set_y("CONFIG_SIBYTE_SB1250");

	/* Used to be CONFIG_SIBYTE_SWARM */
	if (IS_HW("BCM91125E"))
	    token_set_y("CONFIG_SIBYTE_SENTOSA");
	else if (IS_HW("COLORADO"))
	    token_set_y("CONFIG_SIBYTE_COLORADO");

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("eth1", DEV_IF_BRCM91125E_ETH, DEV_IF_NET_INT);
	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("eth0", DEV_IF_BRCM91125E_ETH, DEV_IF_NET_EXT);
	else if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("eth0", DEV_IF_BRCM91125E_ETH, DEV_IF_NET_INT);

    	/* Flash/MTD */
	token_set_y("CONFIG_MTD");
	token_set_y("CONFIG_MTD_CFI_INTELEXT");
	token_set_y("CONFIG_MTD_CFI_AMDSTD");
	token_set_m("CONFIG_BCM91125E_MTD");
	/* New MTD configs (Linux-2.6) */
	if (token_get("CONFIG_RG_OS_LINUX_26"))
	{
	    token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_1");
	    token_set_y("CONFIG_MTD_CFI_I1");
	    token_set_y("CONFIG_MTD_CFI_UTIL");
	}
    }

    if (IS_HW("CN3XXX"))
    {
	token_set_y("CONFIG_CAVIUM_OCTEON");

	/* Ethernet */
	token_set_y("CONFIG_MII");
	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("eth1", DEV_IF_CN3XXX_ETH, DEV_IF_NET_INT);
	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("eth0", DEV_IF_CN3XXX_ETH, DEV_IF_NET_EXT);
	else if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("eth0", DEV_IF_CN3XXX_ETH, DEV_IF_NET_INT);

	/* USB */
	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_USB");
	    token_set_m("CONFIG_USB_DWC_OTG");
	    token_set_y("CONFIG_USB_DEVICEFS");
	}

	/* MoCA */
	if (token_get("CONFIG_HW_MOCA"))
	{
	    token_set_m("CONFIG_ENTROPIC_EN2210");
	    //token_set_y("CONFIG_ENTROPIC_EN2210_MII");
	    //token_set_m("CONFIG_ENTROPIC_EN2210_MII_REF_CARD");
	    //dev_add("eth2", DEV_IF_MOCA_MII, DEV_IF_NET_INT);
	    dev_add("clink0", DEV_IF_MOCA_PCI, DEV_IF_NET_INT);
	    //dev_can_be_missing("eth2");
	    dev_can_be_missing("clink0");

	    token_set_y("CONFIG_RG_LIMITED_CPP");
	    token_set_y("CONFIG_RG_CXX");
	}
	
	/* Atheros */
	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set_m("CONFIG_RG_ATHEROS");
	    atheros_ar5xxx_add("wifi0", "ath0", NULL);
	    dev_set_dependency("ath0", "wifi0");
	}

    	/* Flash/MTD */
	token_set_y("CONFIG_MTD");
	token_set_y("CONFIG_MTD_CFI_ADV_OPTIONS");
	token_set_y("CONFIG_MTD_CFI_NOSWAP");
	token_set_y("CONFIG_MTD_CFI_GEOMETRY");
	token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_1");
	token_set_y("CONFIG_MTD_CFI_I1");
	token_set_y("CONFIG_MTD_CFI_AMDSTD");
	token_set("CONFIG_MTD_CFI_AMDSTD_RETRY", "0");
	token_set_y("CONFIG_MTD_CFI_UTIL");

	token_set_y("CONFIG_MTD_PHYSMAP");
	token_set("CONFIG_MTD_PHYSMAP_START", "0x1f400000");
	token_set("CONFIG_MTD_PHYSMAP_LEN", "0x800000");
	token_set("CONFIG_MTD_PHYSMAP_BANKWIDTH", "1");

	if (token_get("CONFIG_CAVIUM_FASTPATH") ||
	    token_get("CONFIG_CAVIUM_FASTPATH_AEI"))
	{
	    token_set("CONFIG_RG_FASTPATH_PLAT_PATH", 
		"vendor/cavium/octeon/fastpath");
	}
    }    

    if (IS_HW("MC524WR"))
    {
	int have_switch = 0, have_lan_eth = 0, have_wan_eth = 0;
	int have_lan_moca = 0, have_wan_moca = 0;

	token_set_y("CONFIG_CAVIUM_OCTEON");

        token_set_y("CONFIG_HW_AUTODETECT");
	token_set_y("CONFIG_MII");

        if (token_get("CONFIG_IPV6"))
        {
	     token_set_y("CONFIG_RG_IPROUTE2_UTILS");
            token_set_y("CONFIG_IPV6_PRIVACY");
            token_set_y("CONFIG_IPV6_ROUTER_PREF");
            token_set_y("CONFIG_IPV6_ROUTE_INFO");
            token_set_y("CONFIG_INET6_IPCOMP");
            token_set_y("CONFIG_INET6_TUNNEL");
            token_set_y("CONFIG_IPV6_TUNNEL");
            token_set_y("CONFIG_IPV6_MROUTE");
            token_set_y("CONFIG_IPV6_PIMSM_V2");
            token_set_y("CONFIG_RG_THREADS");
            token_set_y("CONFIG_IPV6_PING6");
	    token_set_y("CONFIG_IPV6_CAP_NS_DAD");

            if ( token_get("CONFIG_ACTION_TEC_IPV6_FIREWALL") )
            {
                 token_set_y("CONFIG_NETFILTER_XTABLES");
                 token_set_y("CONFIG_NF_CONNTRACK");
                 token_set_y("CONFIG_NF_CONNTRACK_FTP");

                 token_set_y("CONFIG_NF_CONNTRACK_IPV6");
                 token_set_y("CONFIG_IP6_NF_QUEUE");
                 token_set_y("CONFIG_IP6_NF_IPTABLES");
                 token_set_y("CONFIG_IP6_NF_MATCH_RT");
                 token_set_y("CONFIG_IP6_NF_MATCH_OPTS");
                 token_set_y("CONFIG_IP6_NF_MATCH_FRAG");
                 token_set_y("CONFIG_IP6_NF_MATCH_HL");
                 token_set_y("CONFIG_IP6_NF_MATCH_MULTIPORT");
                 token_set_y("CONFIG_IP6_NF_MATCH_OWNER");
                 token_set_y("CONFIG_IP6_NF_MATCH_IPV6HEADER");
                 token_set_y("CONFIG_IP6_NF_MATCH_AHESP");
                 token_set_y("CONFIG_IP6_NF_MATCH_EUI64");
                 token_set_y("CONFIG_IP6_NF_MATCH_POLICY");
                 token_set_y("CONFIG_IP6_NF_MATCH_STATE");
                 token_set_y("CONFIG_IP6_NF_FILTER");
                 token_set_y("CONFIG_IP6_NF_TARGET_LOG");
                 token_set_y("CONFIG_IP6_NF_TARGET_REJECT");
                 token_set_y("CONFIG_IP6_NF_MANGLE");
                 token_set_y("CONFIG_IP6_NF_TARGET_HL");
                 token_set_y("CONFIG_IP6_NF_RAW");

                 token_set_y("CONFIG_ACTION_TEC_QOS");
            }
            else if ( token_get("ACTION_TEC_IPV6_FIREWALL") )
            {
                 token_set_y("CONFIG_ACTION_TEC_QOS");
            }
            if ( token_get("CONFIG_ACTION_TEC_QOS") )
            {
                 token_set_y("CONFIG_NET_SCHED");
                 token_set_y("CONFIG_NET_SCH_INGRESS");
                 token_set_y("CONFIG_NET_SCH_HTB");
                 token_set_y("CONFIG_NET_SCH_WRR");
                 token_set_y("CONFIG_NET_SCH_ATM");
                 token_set_y("CONFIG_NET_SCH_PRIO");
                 token_set_y("CONFIG_NET_SCH_RED");
                 token_set_y("CONFIG_NET_SCH_SFQ");
                 token_set_y("CONFIG_NET_SCH_DSMARK");
                 token_set_y("CONFIG_NET_QOS");
                 token_set_y("CONFIG_NET_ESTIMATOR");
                 token_set_y("CONFIG_NET_CLS");
                 token_set_y("CONFIG_NET_CLS_POLICE");
                 token_set_y("CONFIG_NET_CLS_TCINDEX");
                 token_set_y("CONFIG_NET_CLS_FW");
                 token_set_y("CONFIG_NET_CLS_U32");
                 token_set_y("CONFIG_NET_CLS_RSVP");
            }
            if ( token_get("CONFIG_ACTION_TEC_IPSEC") )
            {
                 token_set_y("CONFIG_INET6_AH");
                 token_set_y("CONFIG_INET6_ESP");
                 token_set_y("CONFIG_XFRM_USER");
                 token_set_y("CONFIG_NET_KEY");
                 token_set_y("CONFIG_CRYPTO_DES");
 
                 token_set_y("CONFIG_CRYPTO_SHA1");
                 token_set_y("CONFIG_IPSEC_AUTH_HMAC_SHA1");
                 //token_set_m("CONFIG_IPSEC_ALG_SHA1");
            }

    	 //   if (token_get("CONFIG_IPV6_TR98"))
    	 //   {
    	 	//support tr69
    		    token_set_y("AEI_CONTROL_TR98_IPV6");
    	//    }

    	    /* IOT config options */
	    if( token_get("CONFIG_IOT") )
	    {
		    token_set_y("CONFIG_IOT_SLAAC"); 
		    token_set_y("CONFIG_IOT_DAD");
		    token_set_y("CONFIG_IOT_DSLITE");
		    token_set_y("CONFIG_IOT_ROUTERLIFETIME");
		    token_set_y("CONFIG_IOT_CONFIRM");
		    token_set_y("CONFIG_IOT_RECONFIGURATION");
		    token_set_y("CONFIG_IOT_DIAGNOSTICS");
		    token_set_y("CONFIG_IOT_UI"); 
	    }
	    else
	    {
	    	if (token_get("ACTION_TEC_VERIZON"))
		{
			token_set_y("CONFIG_IPV6_DISABLENA");
		}
	    }
        }
	if (token_get("CONFIG_MC524WR_REV_E"))
	{
	    token_set_m("CONFIG_HW_SWITCH_KENDIN_KS8995M");
	    token_set_y("CONFIG_RG_DEV_IF_KS8995M_HW_SWITCH");

	    token_set_y("CONFIG_RG_DEV_IF_CN3XXX_ETH");

	    token_set_m("CONFIG_ENTROPIC_EN2210");
	    token_set_y("CONFIG_RG_DEV_IF_MOCA_PCI");

	    have_switch = have_wan_eth = have_lan_moca = have_wan_moca = 1;
	}

	if (token_get("CONFIG_MC524WR_REV_F"))
	{
	    token_set_m("CONFIG_HW_SWITCH_MARVELL_MV88E60XX");
	    token_set_y("CONFIG_RG_DEV_IF_MV88E60XX_HW_SWITCH");

	    token_set_m("CONFIG_RG_SWITCH_PORT_DEV");
	    token_set_y("CONFIG_RG_DEV_IF_SWITCH_PORT");

	    token_set_y("CONFIG_RG_DEV_IF_MOCA_MII");
	    token_set_y("CONFIG_ENTROPIC_EN2510_MII");

	    have_switch = have_wan_eth = have_lan_moca = have_wan_moca = 1;
	}

	if (token_get("CONFIG_MC524WR_REV_G"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_y("CONFIG_RG_DEV_IF_BCM5395M_HW_SWITCH");

	    token_set_m("CONFIG_RG_SWITCH_PORT_DEV");
	    token_set_y("CONFIG_RG_DEV_IF_SWITCH_PORT");

	    token_set_y("CONFIG_RG_DEV_IF_MOCA_MII");
	    token_set_y("CONFIG_ENTROPIC_EN2510_MII");

	    have_switch = have_wan_eth = have_lan_moca = have_wan_moca = 1;
	}

	if (token_get("CONFIG_MR1000"))
	{
	    token_set_m("CONFIG_HW_SWITCH_MARVELL_MV88E60XX");
	    token_set_y("CONFIG_RG_DEV_IF_MV88E60XX_HW_SWITCH");

	    token_set_m("CONFIG_RG_SWITCH_PORT_DEV");
	    token_set_y("CONFIG_RG_DEV_IF_SWITCH_PORT");

	    token_set_y("CONFIG_RG_DEV_IF_MOCA_MII");
	    token_set_y("CONFIG_ENTROPIC_EN2512_MII");

	    have_switch = have_wan_eth = have_lan_moca = 1;
	}

	if (have_switch)
	    dev_add(act_lan_eth_ifname, DEV_IF_SWITCH, DEV_IF_NET_INT);
	else if (have_lan_eth)
	    dev_add(act_lan_eth_ifname, DEV_IF_CN3XXX_ETH, DEV_IF_NET_INT);

	if (have_wan_eth)
	{
	    dev_add(act_wan_eth_ifname, DEV_IF_WAN_ETH, DEV_IF_NET_EXT);
	    dev_can_be_missing(act_wan_eth_ifname);
	}

	if (have_lan_moca || have_wan_moca)
	{
	    token_set_y("CONFIG_RG_LIMITED_CPP");
	    token_set_y("CONFIG_RG_CXX");

	    if (have_lan_moca)
	    {
		dev_add(act_lan_moca_ifname, DEV_IF_CLINK, DEV_IF_NET_INT);
		dev_can_be_missing(act_lan_moca_ifname);
	    }

	    if (have_wan_moca)
	    {
		dev_add(act_wan_moca_ifname, DEV_IF_CLINK, DEV_IF_NET_EXT);
		dev_can_be_missing(act_wan_moca_ifname);
	    }
	}

	/* USB */
	token_set_y("CONFIG_USB");
	token_set_m("CONFIG_USB_DWC_OTG");
	token_set_y("CONFIG_USB_DEVICEFS");

	/* Atheros */
	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set_m("CONFIG_RG_ATHEROS"); 

	    //if (token_get("CONFIG_RG_ATHEROS_HW_AR5212"))
		//token_set_y("CONFIG_RG_ATHEROS_DEBUG");

        /* both DIAG and DUAL enabled */
        if ((token_get("ACTION_TEC_DIAGNOSTICS") && token_is_y("ACTION_TEC_DIAGNOSTICS"))
                && (token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID")))
            atheros_ar5xxx_add(act_wifi_ap_ifname, 
                    act_atheros_vap_primary_ifname, act_atheros_vap_help_ifname, act_atheros_vap_secondary_ifname, act_atheros_vap_public_ifname, NULL);
        /* only DIAG enabled */
        else if(token_get("ACTION_TEC_DIAGNOSTICS") && token_is_y("ACTION_TEC_DIAGNOSTICS"))
            atheros_ar5xxx_add(act_wifi_ap_ifname, 
                    act_atheros_vap_primary_ifname, act_atheros_vap_help_ifname, NULL);
        /* only DUAL enabled */
        else if(token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID"))
            atheros_ar5xxx_add(act_wifi_ap_ifname, 
                    act_atheros_vap_primary_ifname, act_atheros_vap_secondary_ifname, act_atheros_vap_public_ifname, NULL);
        /* default */
        else 
            atheros_ar5xxx_add(act_wifi_ap_ifname, 
                    act_atheros_vap_primary_ifname, NULL);

        /* for DIAG */
        if(token_get("ACTION_TEC_DIAGNOSTICS") && token_is_y("ACTION_TEC_DIAGNOSTICS"))
            dev_set_dependency(act_atheros_vap_help_ifname, act_wifi_ap_ifname);

        /* for DUAL */
        if(token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID"))
        {
            dev_set_dependency(act_atheros_vap_secondary_ifname, act_wifi_ap_ifname);
            dev_set_dependency(act_atheros_vap_public_ifname, act_wifi_ap_ifname);
        }

        /* default */
        dev_set_dependency(act_atheros_vap_primary_ifname, act_wifi_ap_ifname);

	    token_set("CONFIG_ATHEROS_AR5008_PCI_SWAP", "0");
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

    	/* Flash/MTD */
	token_set_y("CONFIG_MTD");
	token_set_y("CONFIG_MTD_CFI_ADV_OPTIONS");
	token_set_y("CONFIG_MTD_CFI_NOSWAP");
	token_set_y("CONFIG_MTD_CFI_GEOMETRY");
	token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_1");
	token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_2");
	token_set_y("CONFIG_MTD_CFI_I1");
	token_set_y("CONFIG_MTD_CFI_AMDSTD");
	token_set("CONFIG_MTD_CFI_AMDSTD_RETRY", "0");
	token_set_y("CONFIG_MTD_CFI_UTIL");

	token_set_y("CONFIG_MTD_PHYSMAP"); 
	token_set("CONFIG_MTD_PHYSMAP_START", "0x1ec00000");
	token_set("CONFIG_MTD_PHYSMAP_LEN", "0x1000000");
	token_set("CONFIG_MTD_PHYSMAP_BANKWIDTH", "1");

	if (token_get("CONFIG_CAVIUM_FASTPATH") ||
	    token_get("CONFIG_CAVIUM_FASTPATH_AEI"))
	{
	    token_set("CONFIG_RG_FASTPATH_PLAT_PATH", 
		"vendor/cavium/octeon/fastpath");
	}
    }

    if (IS_HW("INCAIP_LSP"))
    {
	token_set_y("CONFIG_INCAIP_COMMON");

	token_set_y("CONFIG_RG_VLAN_8021Q");
	token_set_m("CONFIG_INCAIP_SWITCH");
	token_set_m("CONFIG_INCAIP_SWITCH_API");
	token_set_m("CONFIG_INCAIP_ETHERNET");
	token_set_m("CONFIG_INCAIP_KEYPAD");
	token_set_m("CONFIG_INCAIP_DSP");
	token_set_m("CONFIG_VINETIC");
	token_set_m("CONFIG_INCAIP_IOM2");
    }

    if (IS_HW("INCAIP"))
    {
	token_set_y("CONFIG_INCAIP_COMMON");

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_m("CONFIG_INCAIP_LEDMATRIX");

	if (token_get("CONFIG_HW_KEYPAD"))
	    token_set_m("CONFIG_INCAIP_KEYPAD");

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_INCAIP_DSP");
	    token_set_y("CONFIG_RG_IPPHONE");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "1");
	}
	
	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("inca0", DEV_IF_INCAIP_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("inca0", DEV_IF_INCAIP_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ENCRYPTION"))
	    token_set_y("CONFIG_INCA_HW_ENCRYPT");
    }

    if (IS_HW("FLEXTRONICS"))
    {
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8"); 
	token_set_y("CONFIG_INCAIP_COMMON");
	token_set_y("CONFIG_INCAIP_FLEXTRONICS");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_m("CONFIG_INCAIP_LEDMATRIX");

	if (token_get("CONFIG_HW_KEYPAD"))
	    token_set_m("CONFIG_INCAIP_KEYPAD");

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_INCAIP_DSP");
	    token_set_y("CONFIG_RG_IPPHONE");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "1");
	}
	
	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("inca0", DEV_IF_INCAIP_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("inca0", DEV_IF_INCAIP_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ENCRYPTION"))
	    token_set_y("CONFIG_INCA_HW_ENCRYPT");
    }

    if (IS_HW("ALLTEK_VLAN"))
    {
	token_set_y("CONFIG_INCAIP_COMMON");
	token_set_y("CONFIG_INCAIP_ALLTEK");

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_VINETIC");
	    token_set("CONFIG_VINETIC_LINES_PER_CHIP", "2");
	    token_set_m("CONFIG_INCAIP_IOM2");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	}
	
	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_y("CONFIG_RG_VLAN_8021Q");
	    dev_add("inca0", DEV_IF_INCAIP_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("inca0.3", DEV_IF_INCAIP_VLAN, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("inca0.2", DEV_IF_INCAIP_VLAN, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ENCRYPTION"))
	    token_set_y("CONFIG_INCA_HW_ENCRYPT");
    }

    if (IS_HW("ALLTEK"))
    {
	token_set_y("CONFIG_INCAIP_COMMON");
	token_set_y("CONFIG_INCAIP_ALLTEK");
	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_VINETIC");
	    token_set("CONFIG_VINETIC_LINES_PER_CHIP", "2");
	    token_set_m("CONFIG_INCAIP_IOM2");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	}
	
	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("inca0", DEV_IF_INCAIP_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ENCRYPTION"))
	    token_set_y("CONFIG_INCA_HW_ENCRYPT");
    }

    if (IS_HW("RTA770W") || IS_HW("GTWX5803"))
    {
	token_set_y("CONFIG_BCM963XX_COMMON");
	token_set_y("CONFIG_BCM96345");
	token_set_y("CONFIG_BCM963XX_SERIAL");
	token_set_m("CONFIG_BCM963XX_BOARD");
	token_set_y("CONFIG_BCM963XX_RGL_FLASH_LAYOUT");
	token_set("CONFIG_BCM963XX_CHIP", "6345");
	token_set("CONFIG_BCM963XX_SDRAM_SIZE", "16");

	if (token_get("CONFIG_RG_MTD"))
	{
	    token_set_y("CONFIG_RG_MTD_DEFAULT_PARTITION");
	    token_set("CONFIG_MTD_PHYSMAP_LEN", "0x00400000");
	    token_set("CONFIG_MTD_PHYSMAP_START", "0x1FC00000");
	}
	else
	{
	    token_set_m("CONFIG_BCM963XX_MTD");
	    token_set_y("CONFIG_MTD_CFI_AMDSTD");
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_m("CONFIG_BCM963XX_ADSL");
	    token_set_m("CONFIG_BCM963XX_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("bcm_atm0", DEV_IF_BCM963XX_ADSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm0", DEV_IF_BCM5325A_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	{
	    token_set_y("CONFIG_BCM963XX_WLAN");
	    bcm43xx_add("wl0");
	}

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_USB_RNDIS");
	    token_set_m("CONFIG_BCM963XX_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	if (IS_HW("RTA770W"))
	{
	    token_set("CONFIG_BCM963XX_BOARD_ID", "RTA770W");
	    token_set("BOARD", "RTA770W");
	    token_set("FIRM", "Belkin");
	}
	else
	{
	    token_set("CONFIG_BCM963XX_BOARD_ID", "GTWX5803");
	    token_set("BOARD", "GTWX5803");
	    token_set("FIRM", "Gemtek");
	}
    }

    if (IS_HW("BCM94702"))
    {
	token_set_y("CONFIG_BCM947_COMMON");
	token_set_y("CONFIG_BCM4702");

	/* In order to make an root cramfs based dist use the following 
	 * instead of SIMPLE_RAMDISK
	 *  token_set_y("CONFIG_CRAMFS");
	 *  token_set_y("CONFIG_SIMPLE_CRAMDISK");
	 *  token_set("CONFIG_CMDLINE", 
	 *      "\"root=/dev/mtdblock2 noinitrd console=ttyS0,115200\"");
	 */

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_ET");
	    token_set_y("CONFIG_ET_47XX");
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("bcm1", DEV_IF_BCM4710_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("bcm0", DEV_IF_BCM4710_ETH, DEV_IF_NET_INT);

	token_set("BOARD", "BCM94702");
	token_set("FIRM", "Broadcom");
    }

    if (IS_HW("BCM94704"))
    {
	token_set_y("CONFIG_BCM947_COMMON");
	token_set_y("CONFIG_BCM4704");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_ET");
	    token_set_y("CONFIG_ET_47XX");
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("bcm1", DEV_IF_BCM4710_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("bcm0", DEV_IF_BCM4710_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	    bcm43xx_add("eth0");

	token_set("BOARD", "BCM94704");
	token_set("FIRM", "Broadcom");
    }

    if (IS_HW("BCM94712"))
    {
	/* This means (among others) copy CRAMFS to RAM, which is much
	 * safer, but PMON/CFE currently has a limit of ~3MB when uncompressing.
	 * If your image is larger than that, either reduce image size or
	 * remove CONFIG_COPY_CRAMFS_TO_RAM for this platform. */
	token_set_y("CONFIG_BCM947_COMMON");
	token_set_y("CONFIG_BCM4710");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_ET");
	    token_set_y("CONFIG_ET_47XX");
	    token_set_y("CONFIG_RG_VLAN_8021Q");
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("bcm0.1", DEV_IF_BCM4710_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("bcm0.0", DEV_IF_BCM4710_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	    bcm43xx_add("eth0");

	token_set("BOARD", "BCM94712");
	token_set("FIRM", "Broadcom");
    }

    if (IS_HW("WRT54G"))
    {
	token_set_y("CONFIG_BCM947_COMMON");
	token_set_y("CONFIG_BCM947_HW_BUG_HACK");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_ET");
	    token_set_y("CONFIG_ET_47XX");
	    token_set_y("CONFIG_RG_VLAN_8021Q");
	    token_set_y("CONFIG_VLAN_8021Q_FAST");
	    dev_add("bcm0", DEV_IF_BCM4710_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("bcm0.1", DEV_IF_VLAN, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("bcm0.2", DEV_IF_VLAN, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	    bcm43xx_add("eth0");

	token_set("BOARD", "Cybertan");
	token_set("FIRM", "Cybertan");
    }
    
    if (IS_HW("CENTROID"))
    {
	set_big_endian(0);

	/* Do not change the order of the devices definition.
	 * Storlink has a bug in their ethernet driver which compells us to `up`
	 * eth0 before eth1
	 */
	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("sl0", DEV_IF_SL2312_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("sl1", DEV_IF_SL2312_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("sl1", DEV_IF_SL2312_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN") || token_get("CONFIG_HW_ETH_WAN"))
	    token_set_m("CONFIG_SL2312_ETH");

	if (token_get("CONFIG_HW_ETH_LAN2") && token_get("CONFIG_HW_ETH_WAN"))
	    conf_err("Can't define both CONFIG_HW_ETH2 and CONFIG_HW_ETH_WAN");

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	    dev_can_be_missing("eth0");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_RG_USB");
	    token_set_y("CONFIG_USB_DEVICEFS");
	    token_set_m("CONFIG_USB_OHCI_SL2312");
	}
	
	token_set_y("CONFIG_ARCH_SL2312"); 
 	token_set_y("CONFIG_SL2312_ASIC"); 
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttySI0");
	token_set("CONFIG_SDRAM_SIZE", "64");

	token_set_m("CONFIG_SL2312_FLASH");
	
	if (!token_get("CONFIG_RG_MTD"))
	    token_set_y("CONFIG_MTD_CFI_AMDSTD");
	
	token_set("FIRM", "Storlink");
	token_set("BOARD", "SL2312");
    }

    if (IS_HW("IXDP425"))
    {
	/* Larger memory is available for Richfield (256MB) or
	 * Matecumbe (128MB) but use 64 for max PCI performance
	 * rates (DMA window size = 64MB) */
	token_set_y("CONFIG_ARCH_IXP425_IXDP425"); 
	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "4");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");

	token_set("FIRM", "Intel");
	token_set("BOARD", "IXDP425");
	token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2") && token_get("CONFIG_HW_ETH_WAN"))
	    conf_err("Can't define both CONFIG_HW_ETH2 and CONFIG_HW_ETH_WAN");

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    /* ADSL Chip Alcatel 20170 on board */
	    token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20150");
	    token_set_y("CONFIG_IXP425_ADSL_USE_MPHY");
	    
	    token_set_m("CONFIG_IXP425_ATM");
	    dev_add("ixp_atm0", DEV_IF_IXP425_DSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_m("CONFIG_RG_USB_SLAVE");
	    token_set_y("CONFIG_IXP425_CSR_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	}
    }

    if (IS_HW("MATECUMBE"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	/* Larger memory is available for Richfield (256MB) or
	 * Matecumbe (128MB) but use 64 for max PCI performance
	 * rates (DMA window size = 64MB) */
	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "4");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	token_set_y("CONFIG_ARCH_IXP425_MATECUMBE");
	token_set_y("CONFIG_IXP425_CSR_USB");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_m("CONFIG_RG_USB_SLAVE");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	token_set("FIRM", "Intel");
	token_set("BOARD", "IXDP425");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");
    }

    if (IS_HW("MI424WR"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "0");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8"); 
	token_set_y("CONFIG_ARCH_IXP425_MI424WR");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_KENDIN_KS8995M");
	    dev_add("ixp1", DEV_IF_KS8995M_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_MOCA"))
	{
	    token_set_m("CONFIG_ENTROPIC_CLINK");
	    token_set_m("CONFIG_ENTROPIC_EN2210");
	    dev_add("clink0", DEV_IF_MOCA_PCI, DEV_IF_NET_EXT);
	    dev_add("clink1", DEV_IF_MOCA_PCI, DEV_IF_NET_INT);
	    dev_can_be_missing("clink1");
	    dev_can_be_missing("clink0");
	}
	
	if (token_get("CONFIG_HW_80211G_ISL_SOFTMAC"))
	    isl_softmac_add();

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	{
	    ralink_rt2560_add("ra0");
	    // token_set_y("CONFIG_WIRELESS_TOOLS");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	{
	    ralink_rt2561_add("ra0");
	    // token_set_y("CONFIG_WIRELESS_TOOLS");
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_RG_ATA");
	    token_set_y("CONFIG_IXP425_DSR");
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_m("CONFIG_IXP425_ATM");
	    dev_add("ixp_atm0", DEV_IF_IXP425_DSL, DEV_IF_NET_EXT);
	}
	
	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set_m("CONFIG_RG_ATHEROS");
	    
        /* both DIAG and DUAL enabled */
        if ((token_get("ACTION_TEC_DIAGNOSTICS") && token_is_y("ACTION_TEC_DIAGNOSTICS"))
                && (token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID")))
            atheros_ar5xxx_add(act_wifi_ap_ifname, 
                    act_atheros_vap_primary_ifname, act_atheros_vap_help_ifname, act_atheros_vap_secondary_ifname, act_atheros_vap_public_ifname, NULL);
        /* only DIAG enabled */
        else if(token_get("ACTION_TEC_DIAGNOSTICS") && token_is_y("ACTION_TEC_DIAGNOSTICS"))
            atheros_ar5xxx_add(act_wifi_ap_ifname, 
                    act_atheros_vap_primary_ifname, act_atheros_vap_help_ifname, NULL);
        /* only DUAL enabled */
        else if(token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID"))
            atheros_ar5xxx_add(act_wifi_ap_ifname, 
                    act_atheros_vap_primary_ifname, act_atheros_vap_secondary_ifname, act_atheros_vap_public_ifname, NULL);
        /* default */
        else 
            atheros_ar5xxx_add(act_wifi_ap_ifname, 
                    act_atheros_vap_primary_ifname, NULL);

        /* for DIAG */
        if(token_get("ACTION_TEC_DIAGNOSTICS") && token_is_y("ACTION_TEC_DIAGNOSTICS"))
            dev_set_dependency(act_atheros_vap_help_ifname, act_wifi_ap_ifname);

        /* for DUAL */
        if(token_get("ACTION_TEC_QUAD_SSID") && token_is_y("ACTION_TEC_QUAD_SSID"))
        {
            dev_set_dependency(act_atheros_vap_secondary_ifname, act_wifi_ap_ifname);
            dev_set_dependency(act_atheros_vap_public_ifname, act_wifi_ap_ifname);
        }

        /* default */
        dev_set_dependency(act_atheros_vap_primary_ifname, act_wifi_ap_ifname);
	}

	token_set("FIRM", "Actiontec");
	token_set("BOARD", "MI424WR");

	/* Flash chip */
	token_set_m("CONFIG_IXP425_FLASH_E28F640J3");
    }

    if (IS_HW("RI408"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "0");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8"); 
	token_set_y("CONFIG_ARCH_IXP425_RI408WR");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);
	    token_set("CONFIG_IXP425_ETH_MAC1_PHYID", "31");
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_MARVELL_MV88E60XX");
	    dev_add("ixp0", DEV_IF_MV88E6083_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_LAN2"))
	{
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	    token_set("CONFIG_IXP425_ETH_MAC1_PHYID", "31");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	{
	    ralink_rt2560_add("ra0");
	    // token_set_y("CONFIG_WIRELESS_TOOLS");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	{
	    ralink_rt2561_add("ra0");
	    // token_set_y("CONFIG_WIRELESS_TOOLS");
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	token_set("FIRM", "Actiontec");
	token_set("BOARD", hw);

	/* Flash chip */
	token_set_m("CONFIG_IXP425_FLASH_E28F640J3");
    }

    if (IS_HW("KINGSCANYON"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "4");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set_y("CONFIG_IXP425_CSR_USB");
	token_set_y("CONFIG_ARCH_IXP425_KINGSCANYON");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_m("CONFIG_RG_USB_SLAVE");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	}

	token_set("FIRM", "Interface_Masters");
	token_set("BOARD", "KINGSCANYON");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");
    }

    if (IS_HW("ROCKAWAYBEACH"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS1");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set_y("CONFIG_ARCH_IXP425_ROCKAWAYBEACH");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	token_set("CONFIG_FILESERVER_KERNEL_CONFIG", "USB");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_IXP425_CSR_USB");
	    token_set_m("CONFIG_RG_USB_SLAVE");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	}

	token_set("FIRM", "Intel");
	token_set("BOARD", "ROCKAWAYBEACH");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");
    }
    
    if (IS_HW("WAV54G"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_ARCH_IXP425_WAV54G"); 
	token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20170");
	token_set_y("CONFIG_IXP425_ADSL_USE_SPHY");

	/* Add VLAN support so Cybertan can add HW DMZ */
	token_set_y("CONFIG_RG_VLAN_8021Q");

	if (token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_m("CONFIG_IXP425_ATM");
	    dev_add("ixp_atm0", DEV_IF_IXP425_DSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	    dev_can_be_missing("eth0");
	}
	
	token_set("FIRM", "Cybertan");
	token_set("BOARD", "WAV54G");

	/* Flash CHIP */
	token_set("CONFIG_IXP425_FLASH_E28F640J3", "m");

	/* Download image to memory before flashing
	 * Only one image section in flash, enough memory */
	token_set_y("CONFIG_RG_RMT_UPGRADE_IMG_IN_MEM");
    }

    if (IS_HW("NAPA"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_ARCH_IXP425_NAPA"); 
	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "4");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "0");

	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_IXP425_CSR_USB");
	    token_set_m("CONFIG_RG_USB_SLAVE");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	token_set("FIRM", "Sohoware");
	token_set("BOARD", "NAPA");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");

	token_set_y("CONFIG_RG_UIEVENTS");
	token_set_m("CONFIG_RG_KLEDS");
    }
	
    if (IS_HW("COYOTE"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_ARCH_IXP425_COYOTE"); 
	if (!token_get("CONFIG_RG_FLASH_LAYOUT_SIZE"))
	    token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	if (!token_get("CONFIG_IXP425_SDRAM_SIZE"))
	    token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS1");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	if (token_get("CONFIG_HW_HSS_WAN"))
	    token_set_y("CONFIG_RG_HSS");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    /* ADSL Chip Alcatel 20170 on board */
	    token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20170");
	    token_set_y("CONFIG_IXP425_ADSL_USE_SPHY");
	    
	    token_set_m("CONFIG_IXP425_ATM");
	    dev_add("ixp_atm0", DEV_IF_IXP425_DSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_LAN2") && token_get("CONFIG_HW_ETH_WAN"))
	    conf_err("Can't define both CONFIG_HW_ETH2 and CONFIG_HW_ETH_WAN");

	if (token_get("CONFIG_HW_ETH_LAN2"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_EEPRO100_LAN"))
	{
	    token_set_m("CONFIG_EEPRO100");
	    dev_add("eep0", DEV_IF_EEPRO100, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_m("CONFIG_RG_USB_SLAVE");
	    token_set_y("CONFIG_IXP425_CSR_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	    dev_can_be_missing("eth0");
	}

	if (token_get("CONFIG_HW_80211G_ISL_SOFTMAC"))
	    isl_softmac_add();

	if (token_get("CONFIG_HW_80211B_PRISM2"))
	{
	    token_set_m("CONFIG_PRISM2_PCI");
	    dev_add("wlan0", DEV_IF_PRISM2, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_IXP425_DSR");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	}
	
	if (token_get("CONFIG_HW_EEPROM"))
	    token_set("CONFIG_PCF8594C2", "m");

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}
	
	if (token_get("CONFIG_IXP425_CSR_FULL"))
	{
	    /* ADSL Chip Alcatel 20170 on board */
	    token_set_y("CONFIG_ADSL_CHIP_ALCATEL_20170");
	    token_set_y("CONFIG_IXP425_ADSL_USE_SPHY");
	}

	token_set("FIRM", "Intel");
	token_set("BOARD", "COYOTE");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");

	if (token_get("CONFIG_HW_IDE"))
	{
	    /* Custom IDE device on expansion BUS */
	    token_set_y("CONFIG_IDE");
	    token_set_y("CONFIG_BLK_DEV_IDE");
	    token_set_y("CONFIG_BLK_DEV_IDEDISK");
	    token_set_y("CONFIG_IDEDISK_MULTI_MODE");
	    token_set_y("CONFIG_RG_IDE_NON_BLOCKING");
	}
    }

    if (IS_HW("MONTEJADE"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_ARCH_IXP425_MONTEJADE"); 
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16"); 
	token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "0");

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_HW_ST_20190");
	    token_set_y("CONFIG_IXP425_ADSL_USE_SPHY");
	    
	    token_set_m("CONFIG_IXP425_ATM");
	    dev_add("ixp_atm0", DEV_IF_IXP425_DSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp0", DEV_IF_RTL8305SB_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_LAN2") && token_get("CONFIG_HW_ETH_WAN"))
	    conf_err("Can't define both CONFIG_HW_ETH2 and CONFIG_HW_ETH_WAN");

	if (token_get("CONFIG_HW_ETH_LAN2"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_m("CONFIG_RG_USB_SLAVE");
	    token_set_y("CONFIG_IXP425_CSR_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	    dev_can_be_missing("eth0");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_80211N_AIRGO_AGN100"))
	    airgo_agn100_add();

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_IXP425_DSR");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "4");
	}

	token_set("FIRM", "Intel");
	token_set("BOARD", "MONTEJADE");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F640J3","m");
    }
 
    if (IS_HW("JIWIS800") || IS_HW("JIWIS832") || IS_HW("JIWIS832FL") ||
	IS_HW("JIWIS842J"))
    {

	if (IS_HW("JIWIS800"))
	{
	    token_set_y("CONFIG_ARCH_IXP425_JIWIS800"); 
	    token_set("BOARD", "JI-WIS 800");
	}
	else if (IS_HW("JIWIS832"))
	{
	    token_set_y("CONFIG_ARCH_IXP425_JIWIS832");
	    token_set("BOARD", "JI-WIS 832");
	}
	else if (IS_HW("JIWIS832FL"))
	{
	    token_set_y("CONFIG_ARCH_IXP425_JIWIS832");
	    token_set("BOARD", "JI-WIS 832FL");
	}
	else if (IS_HW("JIWIS842J"))
	{
	    token_set_y("CONFIG_ARCH_IXP425_JIWIS842"); 
	    token_set("BOARD", "JI-WIS 842JSR0");
	}

	if (IS_HW("JIWIS842J"))
	{
	    /* JIWIS842J came in two batches the first batch had 64 MB RAM.
	     * Current code uses 32 MB RAM to support all the boards */
	    token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	    token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	}
	else
	{
	    token_set("CONFIG_IXP425_SDRAM_SIZE", "128");
	    token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "4");
	}

	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_HW_ST_20190");
	    token_set_y("CONFIG_IXP425_ADSL_USE_SPHY");
	    
	    token_set_m("CONFIG_IXP425_ATM");
	    dev_add("ixp_atm0", DEV_IF_IXP425_DSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp0", DEV_IF_RTL8305SC_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_LAN2") && token_get("CONFIG_HW_ETH_WAN"))
	    conf_err("Can't define both CONFIG_HW_ETH2 and CONFIG_HW_ETH_WAN");

	if (token_get("CONFIG_HW_ETH_LAN2"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	    dev_can_be_missing("eth0");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");
	else if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_80211N_AIRGO_AGN100"))
	    airgo_agn100_add();

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    if (IS_HW("JIWIS800"))
	    {
		token_set_y("CONFIG_IXP425_DSR");
		token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "4");
	    }
	    else if (IS_HW("JIWIS832"))
	    {
		/*
		 * TODO: Add support for JI-WIS 832 in DSR modules
		token_set_y("CONFIG_IXP425_DSR");
		token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "1");
		*/
	    }
	    else if (IS_HW("JIWIS832FL"))
	    {
		/* No VOICE */
	    }
	    else if (IS_HW("JIWIS842J"))
	    {
		token_set_y("CONFIG_IXP425_DSR");
		token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    }
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	token_set("FIRM", "JStream");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3","m");
    }
 
    if (IS_HW("BRUCE"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_ARCH_IXP425_BRUCE");
	if (!token_get("CONFIG_RG_FLASH_LAYOUT_SIZE"))
	    token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "16");
	token_set("CONFIG_IXP425_SDRAM_SIZE", "128");
	token_set("CONFIG_SDRAM_SIZE", "128");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");

	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "0");

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_LAN2") && token_get("CONFIG_HW_ETH_WAN"))
	{
	    fprintf(stderr, "Can't define CONFIG_HW_ETH2 and CONFIG_HW_ETH_WAN "
		"together\n");
	    exit(1);
	}

	if (token_get("CONFIG_HW_ETH_LAN2"))
	{
	    token_set_m("CONFIG_IXP425_ETH");
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	    dev_can_be_missing("eth0");
	}

	if (token_get("CONFIG_HW_EEPROM"))
	    token_set("CONFIG_PCF8594C2", "m");

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}
	
	token_set("FIRM", "Jabil");
	token_set("BOARD", "Bruce");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");

	if (token_get("CONFIG_HW_IDE"))
	{
	    /* SiI680ACL144 and SiI3512CT128 on PCI BUS */
	    token_set_y("CONFIG_IDE");
	    token_set_y("CONFIG_BLK_DEV_IDE");
	    token_set_y("CONFIG_BLK_DEV_IDEDISK");
	    token_set_y("CONFIG_BLK_DEV_IDECD");
	    token_set_y("CONFIG_BLK_DEV_IDEPCI");
	    token_set_y("CONFIG_BLK_DEV_IDEDMA");
	    token_set_y("CONFIG_BLK_DEV_IDEDMA_PCI");
	    token_set_y("CONFIG_BLK_DEV_OFFBOARD");
	    token_set_y("CONFIG_BLK_DEV_SIIMAGE");
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	/* I2C Bus */
	if (token_get("CONFIG_HW_I2C"))
	{
	    token_set_y("CONFIG_I2C");
	    token_set_y("CONFIG_I2C_ALGOBIT");
	    token_set_y("CONFIG_I2C_IXP425");
	}

	/* HW clock */
	if (token_get("CONFIG_HW_CLOCK"))
	    token_set_m("CONFIG_I2C_DS1374");

	/* Env. monitor clock */
	if (token_get("CONFIG_HW_ENV_MONITOR"))
	    token_set_m("CONFIG_I2C_LM81");

	/* CPLD chip module and API */
	token_set_m("CONFIG_ARCH_IXP425_BRUCE_CPLD");
	
    }

    if (IS_HW("AD6834"))
    {
	int is_linux26 = !strcmp(os, "LINUX_26");

	token_set_y("CONFIG_MACH_ADI_FUSIV");

	token_set_y("CONFIG_CPU_LX4189");
	token_set_y("CONFIG_ADI_6843");
	token_set_y("CONFIG_PCI");
	token_set_y("CONFIG_NEW_PCI");
	token_set_y("CONFIG_PCI_AUTO");

	token_set("CONFIG_SDRAM_SIZE", "32");
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8");

	token_set_y("CONFIG_ADI_6843_RG1");
	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    if (!is_linux26)
		token_set_y("CONFIG_ETH_NETPRO_SIERRA");
	    else
		token_set_y("CONFIG_ADI_FUSIV_ETHERNET");
	    
	    dev_add("eth0", DEV_IF_AD6834_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_y("CONFIG_ETH_NETPRO_SIERRA");
	    dev_add("eth1", DEV_IF_AD6834_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    if (!is_linux26) /* linux-2.6 BSP doesn't use zipped ADSL fw */
		token_set_y("CONFIG_ZLIB_INFLATE");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("dsl0", DEV_IF_AD68XX_ADSL, DEV_IF_NET_EXT);
	}
	
	token_set("BOARD", "AD6834");
	token_set("FIRM", "Analog Devices");

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_USB_RNDIS");
	    token_set_m("CONFIG_ADI_6843_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	{
	    ralink_rt2560_add("ra0");
	    token_set("CONFIG_RALINK_RT2560_TIMECSR", "0x40");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_DSPDRIVER_218X");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	}

	if (!is_linux26)
	    token_set_m("CONFIG_ADI_6843_MTD");
    }
    if (IS_HW("ALASKA"))
    {
	token_set("BOARD", "ALASKA");
	token_set("FIRM", "Ikanos Communications");
	
	/* General setup */
	token_set_y("CONFIG_OBSOLETE_INTERMODULE");

	/* Machine selection */
	token_set_y("CONFIG_MACH_ADI_FUSIV");
	token_set_y("CONFIG_FUSIV_VX160");

	/* RAM */
	token_set("CONFIG_SDRAM_SIZE", "32");

	/* Flash */
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8");

	/* Ethernet */
	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    /* Fusiv drivers */
	    token_set("CONFIG_FUSIV_LIBRARY", "m");
	    token_set("CONFIG_FUSIV_BMDRIVER", "m");
	    token_set("CONFIG_FUSIV_KERNEL_ETHERNET", "m");
	    token_set("CONFIG_FUSIV_TIMERS", "m");
	    
	    dev_add("eth0", DEV_IF_AD6834_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_VDSL_WAN"))
	{
	    dev_add("eth1", DEV_IF_IKANOS_VDSL, DEV_IF_NET_EXT);
	    token_set_y("CONFIG_RG_IKANOS_VDSL");
	    token_set_y("CONFIG_RG_THREADS");
	    token_set_y("CONFIG_HW_ETH_WAN");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	{
	    token_set("CONFIG_FUSIV_PCI_OUTB_ENDIAN_CONVERSION_ENABLE", "0");
	    ralink_rt2561_add("ra0");
	}

	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set("CONFIG_FUSIV_PCI_OUTB_ENDIAN_CONVERSION_ENABLE", "1");
	    token_set_m("CONFIG_RG_ATHEROS");
	    atheros_ar5xxx_add("wifi0", "ath0", NULL);
	    dev_set_dependency("ath0", "wifi0");
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_FUSIV_KERNEL_DSP");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	}
	
	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_USB_RNDIS");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}
    }

    if (IS_HW("BCM96358") || IS_HW("DWV_BCM96358"))
    {
	token_set_y("CONFIG_BCM963XX_COMMON");
	token_set_y("CONFIG_BCM96358");
	token_set_y("CONFIG_SERIAL_CORE");
	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	token_set_y("CONFIG_BCM963XX_SERIAL");
	token_set_y("CONFIG_BCM96358_BOARD");
	token_set_y("CONFIG_BCM963XX_MTD");
	if (IS_HW("BCM96358"))
	    token_set("CONFIG_BCM963XX_BOARD_ID", "96358GW");
	else
	    token_set("CONFIG_BCM963XX_BOARD_ID", "DWV-S0");
	token_set("CONFIG_BCM963XX_SDRAM_SIZE", "32");
	if (IS_HW("DWV_BCM96358"))
	{
	    token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    token_set("CONFIG_RG_FLASH_START_ADDR", "0xbe000000");
	}
	else
	{
	    /* this value is taken from 
	     * vendor/broadcom/bcm963xx/linux-2.6/bcmdrivers/opensource/include/bcm963xx/board.h*/
	    token_set("CONFIG_RG_FLASH_START_ADDR", "0xbfc00000");
	    token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "4");
	}
	token_set("CONFIG_BCM963XX_CHIP", "6358");

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_m("CONFIG_BCM963XX_ADSL");
	    token_set_m("CONFIG_BCM963XX_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("bcm_atm0", DEV_IF_BCM963XX_ADSL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_WAN") && token_get("CONFIG_HW_ETH_LAN"))
	    conf_err("Ethernet device 'bcm0' cannot be WAN and LAN device");

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    dev_add("bcm0", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);

	    if (!token_get("CONFIG_HW_SWITCH_LAN"))
	    {
		token_set_m("CONFIG_BCM963XX_ETH");
		dev_add("bcm1", DEV_IF_BCM963XX_ETH, DEV_IF_NET_INT);
	    }
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm1", DEV_IF_BCM5325E_HW_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	{
	    token_set_y("CONFIG_BCM4318");
	    bcm43xx_add("wl0");
	}

	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_USB_RNDIS");
	    token_set_m("CONFIG_BCM963XX_USB");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_HW_FXO");
	    token_set_m("CONFIG_BCM963XX_DSP");
	    token_set_y("CONFIG_BCM963XX_FXO");
	    token_set_m("CONFIG_BCM_ENDPOINT");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    token_set("CONFIG_HW_NUMBER_OF_FXO_PORTS", "1");
	}

	token_set_y("CONFIG_PCI");
	token_set_y("CONFIG_NEW_PCI");
	token_set_y("CONFIG_PCI_AUTO");
	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	token_set("BOARD", "BCM96358");
	token_set("FIRM", "Broadcom");
    }
    
    if (IS_HW("HG21"))
    {
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_ARCH_IXP425_HG21"); 
	token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set("CONFIG_IXDP425_KGDB_UART", "0");
	
	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}
	
	token_set("FIRM", "Welltech");
	token_set("BOARD", "HG21");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F640J3","m");
	token_set("CONFIG_IXP425_FLASH_USER_PART", "0x00100000");

	/* Download image to memory before flashing
	 * Only one image section in flash, enough memory */
	token_set_y("CONFIG_RG_RMT_UPGRADE_IMG_IN_MEM");
    }
    
    if (IS_HW("BAMBOO"))
    {
	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}
	
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_ARCH_IXP425_BAMBOO"); 
	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "4");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS1");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");
	token_set_m("CONFIG_AT93CXX");
	token_set_m("CONFIG_ADM6996");
	token_set_y("CONFIG_RG_VLAN_8021Q");

	/* CSR HSS support */
	token_set_y("CONFIG_IXP425_CSR_HSS");
	token_set("CONFIG_IXP425_CODELETS", "m");
	token_set("CONFIG_IXP425_CODELET_HSS", "m");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");

	token_set("FIRM", "Planex");
	token_set("BOARD", "BAMBOO");
    }
	
    if (IS_HW("USR8200"))
    {
	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_CLOCK"))
	    token_set("CONFIG_JEEVES_RTC7301", "m");

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F128J3", "m");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS1");
	token_set("FIRM", "USR");
	token_set("BOARD", "USR8200");
	token_set_y("CONFIG_ARCH_IXP425_JEEVES");
	token_set("CONFIG_IXP425_SDRAM_SIZE", "64");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
    }
    
    if (IS_HW("GTWX5715"))
    {
	if (token_get("CONFIG_HW_ETH_WAN") || token_get("CONFIG_HW_ETH_LAN"))
	    token_set_m("CONFIG_IXP425_ETH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("ixp0", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("ixp1", DEV_IF_IXP425_ETH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth0", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_ENCRYPTION"))
	{
	    token_set_y("CONFIG_IPSEC_USE_IXP4XX_CRYPTO");
	    token_set_y("CONFIG_IPSEC_ENC_AES");
	}
	
	token_set_y("CONFIG_GEMTEK_COMMON");
	token_set_y("CONFIG_GEMTEK_WX5715");
	token_set_y("CONFIG_IXP425_COMMON_RG");
	token_set_y("CONFIG_ARCH_IXP425_GTWX5715"); 
	token_set("CONFIG_IXP425_SDRAM_SIZE", "32");
	token_set("CONFIG_IXP425_NUMBER_OF_MEM_CHIPS", "2");
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS1");
	token_set("CONFIG_IXDP425_KGDB_UART", "1");

	/* Flash chip */
	token_set("CONFIG_IXP425_FLASH_E28F640J3", "m");

	token_set("FIRM", "Gemtek");
	token_set("BOARD", "GTWX5715");
    }

    if (IS_HW("TI_404"))
    {
	if (token_get("CONFIG_HW_CABLE_WAN"))
	{
	    dev_add("cbl0", DEV_IF_TI404_CBL, DEV_IF_NET_EXT);
	    dev_set_dependency("cbl0", "cable0");
	    dev_add("cable0", DEV_IF_DOCSIS, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("lan0", DEV_IF_TI404_LAN, DEV_IF_NET_INT);

	set_big_endian(1);
	token_set_y("CONFIG_TI_404_MIPS");
	token_set_y("CONFIG_TI_404_COMMON");
	token_set("CONFIG_ARCH_MACHINE", "ti404");
	token_set("RAM_HIGH_ADRS", "0x94F00000");
	token_set("RAM_LOW_ADRS", "0x94001000");
    }

    if (IS_HW("AR531X_G") || IS_HW("WRT108G") || IS_HW("AR531X_WRT55AG") ||
	IS_HW("AR531X_AG"))
    {
	char *size;
	int is_ag_board = IS_HW("AR531X_WRT55AG") || IS_HW("AR531X_AG");

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    dev_add(is_ag_board ? "ae1" : "ae3", DEV_IF_AR531X_ETH,
		DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    dev_add(is_ag_board ? "ae0" : "ae2", DEV_IF_AR531X_ETH,
		DEV_IF_NET_INT);
	}

	if (IS_HW("AR531X_WRT55AG"))
	    token_set_y("CONFIG_PHY_KS8995M");
	else
	    token_set_y("CONFIG_PHY_MARVEL");

	if (token_get("CONFIG_HW_80211G_AR531X"))
	{
	    token_set_y("CONFIG_RG_VENDOR_WLAN_SEC");
	    token_set_y("CONFIG_RG_WPA_WBM");
	    token_set_y("CONFIG_RG_8021X_WBM");
	    token_set_y("CONFIG_RG_WLAN_AUTO_CHANNEL_SELECT");
	    dev_add("vp256", DEV_IF_AR531X_WLAN_G, DEV_IF_NET_INT);
	}
	if (token_get("CONFIG_HW_80211A_AR531X"))
	    dev_add("vp0", DEV_IF_AR531X_WLAN_A, DEV_IF_NET_INT);

	token_set("ARCH", "mips");
	set_big_endian(1);
	token_set_y("CONFIG_ATHEROS_AR531X_MIPS");
	token_set("RAM_HIGH_ADRS", "0x80680000");
	token_set("RAM_LOW_ADRS", "0x80010000");
	token_set_y("CONFIG_VX_TFFS");
	token_set_y("CONFIG_RG_VX_DEFERRED_TX");

	if (IS_HW("WRT108G"))
	{
	    token_set_y("CONFIG_ARCH_WRT108G");
	    token_set_y("CONFIG_RG_WLAN_AUTO_CHANNEL_SELECT");
	}
	else
	    token_set_y("CONFIG_ARCH_AR531X");

	if ((size = token_get_str("CONFIG_SDRAM_SIZE")) && atoi(size) <= 8)
	    token_set_y("CONFIG_SMALL_SDRAM");
    }

    if (IS_HW("CX8620XR") || IS_HW("CX8620XD"))
    {
	if (IS_HW("CX8620XR"))
	{
	    token_set("CONFIG_CX8620X_SDRAM_SIZE", "8");
	    token_set_y("CONFIG_RG_BOOTSTRAP");
	    
	    /* Flash chip */
	    token_set_m("CONFIG_CX8620X_FLASH_TE28F160C3");
	    token_set("BOARD", "CX8620XR");
	}
	else
	{
	    token_set("CONFIG_CX8620X_SDRAM_SIZE", "64");
	
	    if (token_get("CONFIG_LSP_DIST"))
		token_set_y("CONFIG_RG_BOOTSTRAP");

	    /* Flash chip */
	    if (!token_get("CONFIG_CX8620X_FLASH_TE28F320C3"))
		token_set_m("CONFIG_CX8620X_FLASH_TE28F640C3");
	    token_set("BOARD", "CX8620XD");
	}
	    
	token_set_m("CONFIG_CX8620X_SWITCH");

	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("cnx1", DEV_IF_CX8620X_SWITCH, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("cnx0", DEV_IF_CX8620X_SWITCH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("cnx1", DEV_IF_CX8620X_SWITCH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_USB_HOST_EHCI"))
	    token_set_y("CONFIG_CX8620X_EHCI");
	
	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}

	if (token_get("CONFIG_HW_80211G_ISL_SOFTMAC"))
	    isl_softmac_add();
	
	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	token_set("FIRM", "Conexant");
    }
    if (IS_HW("CX9451X"))
    {
	set_big_endian(0);
	token_set("ARCH", "arm");
	token_set("CONFIG_ARCH_MACHINE", "solos");
	token_set_y("CONFIG_HAS_MMU");
	token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	token_set("CONFIG_CMDLINE", "console=ttyS0,38400");
	token_set_y("CONFIG_DEBUG_USER");
	token_set_y("CONFIG_KALLSYMS");
	token_set_y("CONFIG_SERIAL_CORE");
	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	token_set_y("CONFIG_SERIAL_SOLOS_CONSOLE");
	token_set_y("CONFIG_UNIX98_PTYS");
	token_set_y("CONFIG_RG_ARCHINIT");

#if 0
	/* Conexant does not use this ethernet driver. If we try to set it, it
	 * block the kernel
	 */
	token_set_y("CONFIG_MII");
	token_set_y("CONFIG_ARM_SOLOS_ETHER");
#endif
	token_set_y("CONFIG_RG_INITFS_RAMFS");
	token_set_y("CONFIG_RG_MODFS_CRAMFS");
	token_set("CONFIG_ZBOOT_ROM_TEXT", "0");
	token_set_y("CONFIG_CONEXANT_COMMON");
	token_set_y("CONFIG_CX9451X_COMMON");
	token_set_y("CONFIG_CPU_32");
	/* XXX Remove CONFIG_LOCK_KERNEL after resolving B36439 */
	token_set_y("CONFIG_LOCK_KERNEL");
	token_set_y("CONFIG_FPE_NWFPE");
	token_set_y("CONFIG_ALIGNMENT_TRAP");
	token_set_y("CONFIG_PREVENT_FIRMWARE_BUILD");
	token_set_y("CONFIG_ARCH_SOLOS_GALLUS");
	token_set_y("CONFIG_ARCH_SOLOS_376PIN");
	token_set_y("CONFIG_MACH_SOLOS_GALLUSBU");
	token_set_y("CONFIG_CPU_ARM1026");
	token_set_y("CONFIG_CPU_32v5");
	token_set_y("CONFIG_CPU_ABRT_EV5T");
	token_set_y("CONFIG_CPU_CACHE_VIVT");
	token_set_y("CONFIG_CPU_COPY_V4WB");
	token_set_y("CONFIG_CPU_TLB_V4WBI");
	token_set_y("CONFIG_FW_LOADER");
	token_set_y("CONFIG_STANDALONE");
	token_set_y("CONFIG_RWSEM_GENERIC_SPINLOCK");
	token_set_y("CONFIG_UID16");
	token_set_y("CONFIG_UNIX98_PTYS");
	token_set_y("CONFIG_SERIAL_CORE");
	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");
	token_set_y("CONFIG_SERIAL_SOLOS");
	token_set_y("CONFIG_SERIAL_SOLOS_CONSOLE");
	token_set("CONFIG_SERIAL_SOLOS_CONSOLE_BAUD", "38400");
	token_set("LIBC_ARCH", "arm");
	token_set("CONFIG_SPLIT_PTLOCK_CPUS", "4");
	token_set_y("CONFIG_FLATMEM");
	token_set_y("CONFIG_FLAT_NODE_MEM_MAP");

	/* MTD */
	token_set_y("CONFIG_MTD");
	token_set_y("CONFIG_MTD_CMDLINE_PARTS");
	token_set_y("CONFIG_MTD_CHAR");
	token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_1");
	token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_2");
	token_set_y("CONFIG_MTD_MAP_BANK_WIDTH_4");
	token_set_y("CONFIG_MTD_CFI_I1");
	token_set_y("CONFIG_MTD_CFI_I2");
	token_set_y("CONFIG_MTD_CFI_INTELEXT");
	token_set_y("CONFIG_MTD_CFI_UTIL");
	token_set_y("CONFIG_MTD_PHYSMAP");
	token_set("CONFIG_MTD_PHYSMAP_START", "0x38000000");
	token_set("CONFIG_MTD_PHYSMAP_LEN", "0x1000000"); /* 16M */
	token_set("CONFIG_MTD_PHYSMAP_BANKWIDTH", "1");
	token_set_y("CONFIG_OBSOLETE_INTERMODULE");
	
	if (!token_get("CONFIG_LSP_DIST"))
	{
	    dev_add("lan0", DEV_IF_SOLOS_LAN, DEV_IF_NET_INT);
	    if (token_get("CONFIG_HW_ETH_WAN"))
		dev_add("dmz0", DEV_IF_SOLOS_DMZ, DEV_IF_NET_EXT);
	}
	if (token_get("CONFIG_HW_USB_STORAGE"))
	{
	    token_set_y("CONFIG_USB");
	    token_set_y("CONFIG_USB_DEVICEFS");
	    token_set_y("CONFIG_USB_EHCI_HCD");
	    token_set_y("CONFIG_USB_EHCI_ROOT_HUB_TT");
	    token_set_y("CONFIG_USB_SOLOS_HCD");
	    token_set_y("CONFIG_SOLOS_USB_HOST0");
	    token_set_y("CONFIG_SOLOS_USB_HOST1");
	}

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_m("CONFIG_CX9451X_DSP");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	}

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
	    dev_add("bunatm", DEV_IF_SOLOS_DSL, DEV_IF_NET_EXT);
	}
    }

    if (IS_HW("COMCERTO"))
    {
	if (token_get("CONFIG_COMCERTO_COMMON"))
	{
	    /* Network interfaces */
	    if (token_get("CONFIG_HW_ETH_LAN"))
		dev_add("eth0", DEV_IF_COMCERTO_ETH, DEV_IF_NET_INT);
	    if (token_get("CONFIG_HW_ETH_WAN"))
		dev_add("eth2", DEV_IF_COMCERTO_ETH, DEV_IF_NET_EXT);

	    /* VED */
	    token_set_y("CONFIG_COMCERTO_VED");
	    token_set_y("CONFIG_MII");
	    dev_add("eth1", DEV_IF_COMCERTO_ETH_VED, DEV_IF_NET_INT);

	    token_set_y("CONFIG_RG_ARCHINIT");
	}

	/* Ralink WiFi card */
	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");
	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	token_set_y("CONFIG_NETFILTER");

	token_set_y("CONFIG_COMCERTO_MATISSE");

	if (token_get("CONFIG_COMCERTO_MALINDI"))
	{
	    token_set_y("CONFIG_EVM_MALINDI");
	    token_set("CONFIG_LOCALVERSION","malindi");
	    token_set("BOARD", "Malindi");
	    if (token_get("CONFIG_HW_DSP"))
	    {
		token_set_y("CONFIG_COMCERTO_CAL");
		token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "4");
	    }
	}
	else if (token_get("CONFIG_COMCERTO_NAIROBI"))
	{
	    token_set_y("CONFIG_EVM_SUPERMOMBASA");
	    token_set("CONFIG_LOCALVERSION","nairobi");
	    token_set("BOARD", "Nairobi");
	    if (token_get("CONFIG_HW_DSP"))
	    {
		token_set_y("CONFIG_COMCERTO_CAL");
		token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "2");
	    }
	}

	token_set("FIRM", "Mindspeed");
    }

    if (IS_HW("TI_TNETWA100"))
    {
	set_big_endian(1);
	token_set_y("CONFIG_TI_404_MIPS");
	token_set_y("CONFIG_TI_404_COMMON");
	token_set("RAM_HIGH_ADRS", "0x94F00000");
	token_set("RAM_LOW_ADRS", "0x94001000");
    }

    if (IS_HW("CENTAUR"))
    {
	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "4"); 
	token_set("CONFIG_SDRAM_SIZE", "32");	
	token_set("CONFIG_RG_CONSOLE_DEVICE", "ttyS0");
	token_set_y("CONFIG_RG_DYN_FLASH_LAYOUT");

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_y("CONFIG_KS8695");
	    dev_add("eth0", DEV_IF_KS8695_ETH, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_y("CONFIG_KS8695");
	    dev_add("eth1", DEV_IF_KS8695_ETH, DEV_IF_NET_INT);
	}
	
	if (token_get("CONFIG_HW_80211G_ISL38XX"))
	{
	    token_set_m("CONFIG_ISL38XX");
	    dev_add("eth2", DEV_IF_ISL38XX, DEV_IF_NET_INT);
	    dev_can_be_missing("eth2");
	}

	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_ZSP400");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "4");
	}

	token_set("FIRM", "Micrel");
	token_set("BOARD", "CENTAUR");

	/* Flash chip XXX Should be module */
	token_set_y("CONFIG_KS8695_FLASH_AM29LV033C");
    }

    if (IS_HW("I386_BOCHS"))	
    {
	set_big_endian(0);
	token_set_y("CONFIG_I386_BOCHS");
	token_set("CONFIG_ARCH_MACHINE", "i386");
	if (token_get("CONFIG_RG_OS_VXWORKS"))
	{
	    dev_add("ene0", DEV_IF_NE2K_VX, DEV_IF_NET_INT);
	    dev_add("ene1", DEV_IF_NE2K_VX, DEV_IF_NET_INT);
	    dev_add("ene2", DEV_IF_NE2K_VX, DEV_IF_NET_EXT);
	    token_set_y("CONFIG_VX_KNET_SYMLINK");
	    token_set("RAM_HIGH_ADRS", "0x00008000");
	    token_set("RAM_LOW_ADRS", "0x00108000");
	}
	else
	{
	    token_set_m("CONFIG_NE2000");
	    dev_add("ne0", DEV_IF_NE2000, DEV_IF_NET_INT);
	    dev_add("ne1", DEV_IF_NE2000, DEV_IF_NET_EXT);
	    token_set_y("CONFIG_PCBOX");
	}
    }

    if (IS_HW("UML"))
    {
	token_set("ARCH", "um");

	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "64"); 
	if (token_get("CONFIG_HW_ETH_WAN"))
	    dev_add("eth0", DEV_IF_UML, DEV_IF_NET_EXT);
	if (token_get("CONFIG_HW_ETH_WAN2"))
	    dev_add("eth3", DEV_IF_UML, DEV_IF_NET_EXT);

	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM_NULL");
	    dev_add("atmnull0", DEV_IF_ATM_NULL, DEV_IF_NET_EXT);
	}

	if (token_get("CONFIG_HW_HSS_WAN"))
	    token_set_y("CONFIG_RG_HSS");

	if (token_get("CONFIG_HW_ETH_LAN"))
	    dev_add("eth1", DEV_IF_UML_HW_SWITCH, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_ETH_LAN2"))
	    dev_add("eth2", DEV_IF_UML, DEV_IF_NET_INT);

	if (token_get("CONFIG_HW_80211G_UML_WLAN"))
	    uml_wlan_add("uml_wlan0");

	token_set_y("CONFIG_RG_UML");

	token_set_y("CONFIG_DEBUGSYM"); /* UML is always for debug ;-) */
	
	token_set("CONFIG_RG_CONSOLE_DEVICE", "console");

	token_set_y("CONFIG_RAMFS");
	token_set("CONFIG_KERNEL_STACK_ORDER", "2");
	token_set_y("CONFIG_MODE_TT");
	token_set_y("CONFIG_MODE_SKAS");
	token_set("CONFIG_NEST_LEVEL", "0");
	token_set("CONFIG_CON_ZERO_CHAN", "fd:0,fd:1");
	token_set("CONFIG_CON_CHAN", "xterm");
	token_set("CONFIG_SSL_CHAN", "pty");
	token_set("CONFIG_KERNEL_HALF_GIGS", "1");
	token_set_y("CONFIG_PT_PROXY");
	token_set_y("CONFIG_STDIO_CONSOLE");
	token_set_y("CONFIG_SSL");
	token_set_y("CONFIG_FD_CHAN");
	token_set_y("CONFIG_NULL_CHAN");
	token_set_y("CONFIG_PORT_CHAN");
	token_set_y("CONFIG_PTY_CHAN");
	token_set_y("CONFIG_TTY_CHAN");
	token_set_y("CONFIG_XTERM_CHAN");
	token_set_y("CONFIG_BLK_DEV_NBD");
	if (token_get("CONFIG_RG_OS_LINUX_26"))
	{
	    token_set_y("CONFIG_RG_INITFS_RAMFS");
	    token_set_y("CONFIG_RG_MAINFS_CRAMFS");
	    token_set_y("CONFIG_RG_MODFS_CRAMFS");
	    token_set_y("CONFIG_UML_X86");

	    token_set_y("CONFIG_FLATMEM");
	}
	else
	{
	    token_set_y("CONFIG_BLK_DEV_UBD");
	    token_set_y("CONFIG_SIMPLE_RAMDISK");
	    token_set("CONFIG_BLK_DEV_RAM_SIZE", "8192");
	    token_set_y("CONFIG_RG_INITFS_RAMDISK");
	}
	token_set_y("CONFIG_UML_NET");
	token_set_y("CONFIG_UML_NET_ETHERTAP");
	token_set_y("CONFIG_UML_NET_TUNTAP");
	token_set_y("CONFIG_UML_NET_SLIP");
	token_set_y("CONFIG_UML_NET_SLIRP");
	token_set_y("CONFIG_UML_NET_DAEMON");
	token_set_y("CONFIG_UML_NET_MCAST");
	token_set_y("CONFIG_DUMMY");
	token_set_y("CONFIG_TUN");
	token_set_y("CONFIG_KALLSYMS");

	token_set("CONFIG_UML_RAM_SIZE",
	    token_get("CONFIG_VALGRIND") ? "192M" : "64M");

	token_set_y("CONFIG_USERMODE");
	token_set_y("CONFIG_UID16");
	token_set_y("CONFIG_EXPERIMENTAL");
	token_set_y("CONFIG_BSD_PROCESS_ACCT");
	token_set_y("CONFIG_HOSTFS");
	token_set_y("CONFIG_HPPFS");
	token_set_y("CONFIG_MCONSOLE");
	token_set_y("CONFIG_MAGIC_SYSRQ");
	if (!token_get("CONFIG_RG_OS_LINUX_26"))
	    token_set_y("CONFIG_PROC_MM");

	token_set_y("CONFIG_PACKET_MMAP");
	token_set_y("CONFIG_QUOTA");
	token_set_y("CONFIG_AUTOFS_FS");
	token_set_y("CONFIG_AUTOFS4_FS");
	token_set_y("CONFIG_REISERFS_FS");

	token_set_y("CONFIG_MTD_BLKMTD");
	token_set_y("CONFIG_ZLIB_INFLATE");
	token_set_y("CONFIG_ZLIB_DEFLATE");

	token_set_y("CONFIG_PT_PROXY");
	token_set_y("CONFIG_RG_THREADS");
	token_set("CONFIG_RG_KERNEL_COMP_METHOD", "gzip");
	token_set("CONFIG_RG_CRAMFS_COMP_METHOD", "lzma");
	if (token_get("CONFIG_HW_DSP"))
	{
	    token_set_y("CONFIG_HW_FXO");
	    token_set_y("CONFIG_RG_VOIP_HW_EMULATION");
	    token_set("CONFIG_HW_NUMBER_OF_FXS_PORTS", "8");
	    token_set("CONFIG_HW_NUMBER_OF_FXO_PORTS", "1");
	}
    }
    
    if (IS_HW("JPKG"))
    {
	if (token_get("CONFIG_HW_80211G_RALINK_RT2560"))
	    ralink_rt2560_add("ra0");

	if (token_get("CONFIG_HW_80211G_RALINK_RT2561"))
	    ralink_rt2561_add("ra0");

	if (token_get("CONFIG_HW_80211G_BCM43XX"))
	    bcm43xx_add(NULL);

	if (token_get("CONFIG_RG_ATHEROS_HW_AR5212") ||
	    token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set_m("CONFIG_RG_ATHEROS");
	    atheros_ar5xxx_add("wifi0", "ath0", NULL);
	}

	if (token_get("CONFIG_HW_80211N_AIRGO_AGN100"))
	    airgo_agn100_add();
    }
    
    if (IS_HW("FEROCEON"))
    {
	token_set("ARCH", "arm");
	token_set("LIBC_ARCH", "arm");

	token_set_y("CONFIG_MV_INCLUDE_PEX");
	token_set_y("CONFIG_MV_INCLUDE_USB");
	token_set_y("CONFIG_MV_INCLUDE_NAND");
	token_set_y("CONFIG_MV_INCLUDE_TDM");
	token_set_y("CONFIG_MV_INCLUDE_GIG_ETH");
	//token_set_y("CONFIG_MV_INCLUDE_SWITCH");
	token_set_y("CONFIG_MV_INCLUDE_SPI");
	token_set_y("CONFIG_MV_GPP_MAX_PINS");
	token_set("CONFIG_MV_DCACHE_SIZE", "0x4000");
	token_set("CONFIG_MV_ICACHE_SIZE", "0x4000");
	token_set_y("CONFIG_L2_CACHE_ENABLE");
	token_set_y("CONFIG_MV_SP_I_FTCH_DB_INV");

#ifndef CONFIG_RG_GPL
	token_set_m("CONFIG_HW_SWITCH_MARVELL_FEROCEON");
#endif
	/* Ethernet */
	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    // token_set_y("CONFIG_MII");
	    token_set_y("CONFIG_RG_DEV_IF_FEROCEON_HW_SWITCH");
	    dev_add("eth0", DEV_IF_SWITCH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_ETH_WAN"))
	{
	    token_set_y("CONFIG_RG_DEV_IF_FEROCEON_ETH");
	    dev_add("eth1", DEV_IF_WAN_ETH, DEV_IF_NET_EXT);
	}

	/* Atheros */
	if (token_get("CONFIG_RG_ATHEROS_HW_AR5416"))
	{
	    token_set_m("CONFIG_RG_ATHEROS");
	    if(token_get("ACTION_TEC_DIAGNOSTICS") && token_is_y("ACTION_TEC_DIAGNOSTICS"))
	    {
		atheros_ar5xxx_add("wifi0", "ath0", "ath1", NULL);
		dev_set_dependency("ath0", "wifi0");
		dev_set_dependency("ath1", "wifi0");
	    }
	    else
	    {
		atheros_ar5xxx_add("wifi0", "ath0", NULL);
		dev_set_dependency("ath0", "wifi0");
	    }
	    token_set("CONFIG_ATHEROS_AR5008_PCI_SWAP", "0");
	}

        if (token_get("CONFIG_HW_MOCA"))
	{
	    token_set_y("CONFIG_RG_DEV_IF_MOCA_MII");
	    token_set_y("CONFIG_ENTROPIC_EN2510_MII");
	    token_set_y("CONFIG_RG_MOCA_NO_UNDERLYING");

	    dev_add("clink1", DEV_IF_CLINK, DEV_IF_NET_EXT);
	    dev_add("clink0", DEV_IF_CLINK, DEV_IF_NET_INT);
	    dev_can_be_missing("clink0");
	    dev_can_be_missing("clink1");
	}
	
	token_set_y("CONFIG_ALIGNMENT_TRAP");

	token_set_y("CONFIG_BOOTLDR_UBOOT");
	token_set("CONFIG_BOOTLDR_UBOOT_COMP", "none");

	/* physical BAR from mach-feroceon-kw2/nand.c */
	token_set("CONFIG_MTD_PHYSMAP_START", "0xe0850000"); 
	token_set("CONFIG_MTD_PHYSMAP_LEN", "0x04000000"); /* 64MB flash size */
	token_set_y("CONFIG_MTD_CMDLINE_PARTS");

	if (token_get("CONFIG_HW_LEDS"))
	{
	    token_set_y("CONFIG_RG_UIEVENTS");
	    token_set_m("CONFIG_RG_KLEDS");
	}


	/* IPv6 */
	if (token_get("CONFIG_IPV6"))
	{
		token_set_y("CONFIG_RG_IPROUTE2_UTILS");
		token_set_y("CONFIG_IPV6_PRIVACY");
		token_set_y("CONFIG_IPV6_ROUTER_PREF");
		token_set_y("CONFIG_IPV6_ROUTE_INFO");
		token_set_y("CONFIG_INET6_IPCOMP");
		token_set_y("CONFIG_INET6_TUNNEL");
		token_set_y("CONFIG_IPV6_TUNNEL");
		token_set_y("CONFIG_IPV6_MROUTE");
		token_set_y("CONFIG_IPV6_PIMSM_V2");
		token_set_y("CONFIG_RG_THREADS");
		token_set_y("CONFIG_IPV6_PING6");
		token_set_y("CONFIG_IPV6_CAP_NS_DAD");

            if ( token_get("CONFIG_ACTION_TEC_IPV6_FIREWALL") )
            {
                 token_set_y("CONFIG_NETFILTER_XTABLES");
                 token_set_y("CONFIG_NF_CONNTRACK");
                 token_set_y("CONFIG_NF_CONNTRACK_FTP");

                 token_set_y("CONFIG_NF_CONNTRACK_IPV6");
                 token_set_y("CONFIG_IP6_NF_QUEUE");
                 token_set_y("CONFIG_IP6_NF_IPTABLES");
                 token_set_y("CONFIG_IP6_NF_MATCH_RT");
                 token_set_y("CONFIG_IP6_NF_MATCH_OPTS");
                 token_set_y("CONFIG_IP6_NF_MATCH_FRAG");
                 token_set_y("CONFIG_IP6_NF_MATCH_HL");
                 token_set_y("CONFIG_IP6_NF_MATCH_MULTIPORT");
                 token_set_y("CONFIG_IP6_NF_MATCH_OWNER");
                 token_set_y("CONFIG_IP6_NF_MATCH_IPV6HEADER");
                 token_set_y("CONFIG_IP6_NF_MATCH_AHESP");
                 token_set_y("CONFIG_IP6_NF_MATCH_EUI64");
                 token_set_y("CONFIG_IP6_NF_MATCH_POLICY");
                 token_set_y("CONFIG_IP6_NF_MATCH_STATE");
                 token_set_y("CONFIG_IP6_NF_FILTER");
                 token_set_y("CONFIG_IP6_NF_TARGET_LOG");
                 token_set_y("CONFIG_IP6_NF_TARGET_REJECT");
                 token_set_y("CONFIG_IP6_NF_MANGLE");
                 token_set_y("CONFIG_IP6_NF_TARGET_HL");
                 token_set_y("CONFIG_IP6_NF_RAW");
 
                 token_set_y("CONFIG_ACTION_TEC_QOS");
            }
            else if ( token_get("ACTION_TEC_IPV6_FIREWALL") )
            {
                token_set_y("CONFIG_ACTION_TEC_QOS");
            }
            
            if ( token_get("CONFIG_ACTION_TEC_QOS") )
            {
                 token_set_y("CONFIG_NET_SCHED");
                 token_set_y("CONFIG_NET_SCH_INGRESS");
                 token_set_y("CONFIG_NET_SCH_HTB");
                 token_set_y("CONFIG_NET_SCH_WRR");
                 token_set_y("CONFIG_NET_SCH_ATM");
                 token_set_y("CONFIG_NET_SCH_PRIO");
                 token_set_y("CONFIG_NET_SCH_RED");
                 token_set_y("CONFIG_NET_SCH_SFQ");
                 token_set_y("CONFIG_NET_SCH_DSMARK");
                 token_set_y("CONFIG_NET_QOS");
                 token_set_y("CONFIG_NET_ESTIMATOR");
                 token_set_y("CONFIG_NET_CLS");
                 token_set_y("CONFIG_NET_CLS_POLICE");
                 token_set_y("CONFIG_NET_CLS_TCINDEX");
                 token_set_y("CONFIG_NET_CLS_FW");
                 token_set_y("CONFIG_NET_CLS_U32");
                 token_set_y("CONFIG_NET_CLS_RSVP");
            }
            if ( token_get("CONFIG_ACTION_TEC_IPSEC") )
            {
                 token_set_y("CONFIG_INET6_AH");
                 token_set_y("CONFIG_INET6_ESP");
                 token_set_y("CONFIG_XFRM_USER");
                 token_set_y("CONFIG_NET_KEY");
                 token_set_y("CONFIG_CRYPTO_DES");
 
                 token_set_y("CONFIG_CRYPTO_SHA1");
                 token_set_y("CONFIG_IPSEC_AUTH_HMAC_SHA1");
                 //token_set_m("CONFIG_IPSEC_ALG_SHA1");
            }

	   // if (token_get("CONFIG_IPV6_TR98"))
	  //  {
	  		//support tr69
			token_set_y("AEI_CONTROL_TR98_IPV6");
	//    }

	    /* IOT config options */
	    if( token_get("CONFIG_IOT") )
	    {
		    token_set_y("CONFIG_IOT_SLAAC"); 
		    token_set_y("CONFIG_IOT_DAD");
		    token_set_y("CONFIG_IOT_DSLITE");
		    token_set_y("CONFIG_IOT_ROUTERLIFETIME");
		    token_set_y("CONFIG_IOT_CONFIRM");
		    token_set_y("CONFIG_IOT_RECONFIGURATION");
		    token_set_y("CONFIG_IOT_DIAGNOSTICS");
		    token_set_y("CONFIG_IOT_UI"); 
	    }
	    else
	    {
	    	if (token_get("ACTION_TEC_VERIZON"))
		{
			token_set_y("CONFIG_IPV6_DISABLENA");
		}
	    }

       }  
     }
    
    if (token_get("MODULE_RG_IPV6"))
	dev_add("sit0", DEV_IF_IPV6_OVER_IPV4_TUN, DEV_IF_NET_INT);

    if (IS_HW("OLYMPIA_P40X"))
    {
	/* Linux/Build generic */
	set_big_endian(0);
	token_set("ARCH", "mips");
	token_set("LIBC_ARCH", "mips");

	token_set_y("CONFIG_CENTILLIUM_P400");
	token_set_y("CONFIG_P400_REF");
	token_set_m("CONFIG_MTD_CTLM_SF");

	token_set_y("CONFIG_SERIAL_CORE");
	token_set_y("CONFIG_SERIAL_CORE_CONSOLE");

	token_set_y("CONFIG_SERIAL_EXTERNAL_8250");
	token_set_y("CONFIG_SERIAL_8250");
	token_set_y("CONFIG_SERIAL_8250_CONSOLE");
	token_set("CONFIG_SERIAL_8250_NR_UARTS", "4");

	token_set("CONFIG_BLK_DEV_RAM_SIZE", "8192");

	token_set("CONFIG_RG_FLASH_LAYOUT_SIZE", "8");
	token_set("CONFIG_RG_FLASH_START_ADDR", "0xbe000000");

	/* next step, interfaces. Not there yet */
	if (token_get("CONFIG_HW_DSL_WAN"))
	{
	    token_set_y("CONFIG_ATM");
	    token_set_y("CONFIG_RG_ATM_QOS");
# if 0 /* ATM device */
	    dev_add("atm0", DEV_IF_BCM963XX_ADSL, DEV_IF_NET_EXT);
#endif
	}

	if (token_get("CONFIG_HW_ETH_LAN"))
	{
	    token_set_y("CONFIG_MII");
	    dev_add("eth0", DEV_IF_KS8995MA_ETH, DEV_IF_NET_INT);
	}

	if (token_get("CONFIG_HW_SWITCH_LAN"))
	{
	    token_set_y("CONFIG_MII");
#if 0 /* ETH Switch*/
	    token_set_m("CONFIG_HW_SWITCH_BCM53XX");
	    token_set_m("CONFIG_BCM963XX_ETH");
	    dev_add("bcm1", DEV_IF_BCM5325E_HW_SWITCH, DEV_IF_NET_INT);
#endif
	}

	/* WLAN */
	token_set_y("CONFIG_PCI");

	/* USB */ 
	if (token_get("CONFIG_HW_USB_RNDIS"))
	{
	    token_set_y("CONFIG_USB_RNDIS");
	    token_set_y("CONFIG_USB_CENTILLIUM");
	    dev_add("usb0", DEV_IF_USB_RNDIS, DEV_IF_NET_INT);
	}

	/* VOICE  */
	if (token_get("CONFIG_HW_LEDS"))
	    token_set_y("CONFIG_RG_UIEVENTS");

	token_set("BOARD", "OLYMPIA_P40X");
	token_set("FIRM", "Centillium");
    }
}

