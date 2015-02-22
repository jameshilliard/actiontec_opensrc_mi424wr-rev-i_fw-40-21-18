RGSRC=../../..
include $(RGSRC)/envir.mak

include no_enterprise.config

include ../common/Makefile.common

# OBJS: object files for hostapd program
OBJS = hostapd.o
OBJS += accounting.o
OBJS += ap_list.o
OBJS += beacon.o
OBJS += config.o
OBJS += ctrl_iface.o
OBJS += driver_conf.o
OBJS += eapol_sm.o
OBJS += hw_features.o
OBJS += ieee802_11h.o
OBJS += ieee802_1x.o
OBJS += ieee802_11.o
OBJS += ieee802_11_auth.o
OBJS += mlme.o
OBJS += pmksa_cache.o
OBJS += preauth.o
OBJS += radius.o
OBJS += radius_client.o
# oddly, these functions are not used! # OBJS += reconfig.o
OBJS += sta_info.o
OBJS += topology.o
OBJS += vlan_init.o
OBJS += wme.o
OBJS += wpa.o

# define HOSTAPD_DUMP_STATE to include SIGUSR1 handler for dumping state to
# a file (undefine it, if you want to save in binary size)
#could be# CFLAGS += -DHOSTAPD_DUMP_STATE

ifdef CONFIG_DRIVER_HOSTAP
CFLAGS += -DCONFIG_DRIVER_HOSTAP
OBJS += driver.o
endif

ifdef CONFIG_DRIVER_WIRED
CFLAGS += -DCONFIG_DRIVER_WIRED
OBJS += driver_wired.o
endif

ifdef CONFIG_DRIVER_MADWIFI
CFLAGS += -DCONFIG_DRIVER_MADWIFI
OBJS += driver_madwifi.o
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_DRIVER_PRISM54
CFLAGS += -DCONFIG_DRIVER_PRISM54
OBJS += driver_prism54.o
endif

ifdef CONFIG_DRIVER_DEVICESCAPE
CFLAGS += -DCONFIG_DRIVER_DEVICESCAPE
OBJS += driver_devicescape.o
endif

ifdef CONFIG_DRIVER_BSD
CFLAGS += -DCONFIG_DRIVER_BSD
OBJS += driver_bsd.o
CONFIG_L2_PACKET=y
CONFIG_DNET_PCAP=y
CONFIG_L2_FREEBSD=y
endif

######################
ifdef CONFIG_IAPP
OBJS += iapp.o
endif

ifdef CONFIG_DRIVER_TEST
CFLAGS += -DCONFIG_DRIVER_TEST
OBJS += driver_test.o
endif

ifdef CONFIG_EAP_MD5
OBJS += eap_md5.o
endif

ifdef CONFIG_EAP_TLS
OBJS += eap_tls.o
endif

ifdef CONFIG_EAP_PEAP
OBJS += eap_peap.o
endif

ifdef CONFIG_EAP_TTLS
OBJS += eap_ttls.o
endif

ifdef CONFIG_EAP_MSCHAPV2
OBJS += eap_mschapv2.o
endif

ifdef CONFIG_EAP_GTC
OBJS += eap_gtc.o
endif

ifdef CONFIG_EAP_SIM
OBJS += eap_sim.o
endif

ifdef CONFIG_EAP_AKA
OBJS += eap_aka.o
endif

ifdef CONFIG_EAP_SIM_COMMON
# Example EAP-SIM/AKA interface for GSM/UMTS authentication. This can be
# replaced with another file implementating the interface specified in
# eap_sim_db.h.
OBJS += eap_sim_db.o
endif

ifdef CONFIG_EAP_PAX
OBJS += eap_pax.o
endif

ifdef CONFIG_EAP_PSK
OBJS += eap_psk.o
endif

ifdef CONFIG_EAP_SAKE
OBJS += eap_sake.o 
endif

ifdef CONFIG_EAP_GPSK
OBJS += eap_gpsk.o
endif

ifdef CONFIG_EAP_WPS
OBJS += eap_wps.o
OBJS += wps_config.o
OBJS += config_rewrite.o
ifeq ($(CONFIG_WPS_OPT_UPNP),y)
OBJS += wps_opt_upnp.o
ifeq ($(CONFIG_TINYUPNP),y)
OBJS += tiny_upnp_wps_device.o
else
OBJS += upnp_wps_device.o
endif #CONFIG_TINYUPNP
endif #CONFIG_WPS_OPT_UPNP
endif #CONFIG_EAP_WPS

ifdef CONFIG_EAP_TLV
OBJS += eap_tlv.o
endif

ifdef CONFIG_EAP
OBJS += eap.o 
OBJS += eap_methods.o 
OBJS += eap_identity.o
endif

ifndef CONFIG_TLS
endif

ifdef TLS_FUNCS
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
OBJS += eap_tls_common.o
endif

ifdef CONFIG_PKCS12
endif

ifdef CONFIG_FULL_DYNAMIC_VLAN
# define CONFIG_FULL_DYNAMIC_VLAN to have hostapd manipulate bridges
# and vlan interfaces for the vlan feature.
CFLAGS += -DCONFIG_FULL_DYNAMIC_VLAN
endif


TARGET=hostapd
RAMDISK_BIN_FILES+=$(TARGET)

O_OBJS_hostapd=$(OBJS)
L_OBJS_hostapd=$(BUILDDIR)/pkg/lib/$(COMMON_LIBS)


RAMDISK_LAST_TASKS+=ramdisk_hostapd
ifdef ACTION_TEC_QUAD_SSID
HOSTAPD_CONF_FILES=topology_ap.conf 80211g.ap_radio topology_ap2.conf topology_ap3.conf
else
HOSTAPD_CONF_FILES=topology_ap.conf 80211g.ap_radio wpa-ap.conf
endif

ramdisk_hostapd:
	$(MKDIR) $(RAMDISK_RW_DIR)/etc/hostapd
	cp -vf $(HOSTAPD_CONF_FILES) $(RAMDISK_RW_DIR)/etc/hostapd/

include $(RGMK)

