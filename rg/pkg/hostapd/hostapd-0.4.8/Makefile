RGSRC=../../..
include $(RGSRC)/envir.mak

DIR_WPA_SUPPLICANT=.
DIR_HOSTAP=.

# define HOSTAPD_DUMP_STATE to include SIGUSR1 handler for dumping state to
# a file (undefine it, if you want to save in binary size)
#CFLAGS += -DHOSTAPD_DUMP_STATE

# Include directories for CVS version
CFLAGS += -I. -I$(DIR_HOSTAP) -I../utils -I$(DIR_WPA_SUPPLICANT)

# Uncomment following line and set the path to your kernel tree include
# directory if your C library does not include all header files.
# CFLAGS += -DUSE_KERNEL_HEADERS -I/usr/src/linux/include

OBJS =	hostapd.o eloop.o ieee802_1x.o eapol_sm.o radius.o md5.o rc4.o \
	common.o ieee802_11.o config.o ieee802_11_auth.o accounting.o \
	sta_info.o radius_client.o sha1.o wpa.o aes_wrap.o ctrl_iface.o \
	driver_conf.o

ifdef CONFIG_IAPP
CFLAGS += -DCONFIG_IAPP
OBJS += iapp.o
endif

ifdef CONFIG_HOSTAPD_RSN_PREAUTH
CFLAGS += -DCONFIG_RSN_PREAUTH
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_DRIVER_HOSTAP
CFLAGS += -DCONFIG_DRIVER_HOSTAP
OBJS += driver.o
endif

ifdef CONFIG_DRIVER_WIRED
CFLAGS += -DCONFIG_DRIVER_WIRED
OBJS += driver_wired.o
endif

ifdef CONFIG_HOSTAPD_DRIVER_MADWIFI
CFLAGS += -D"__packed=__attribute__((__packed__))"
CFLAGS += -DCONFIG_DRIVER_MADWIFI -DCONFIG_NO_AUTH -D_LINUX_IF_H
CFLAGS += -DCONFIG_RG_ATHEROS_HW_AR5416
CFLAGS_driver_madwifi.o += -I$(BUILDDIR)/pkg/include/vendor/atheros/pb42_kiwi_rc
OBJS += driver_madwifi.o
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_DRIVER_PRISM54
CFLAGS += -DCONFIG_DRIVER_PRISM54
OBJS += driver_prism54.o
endif

ifdef CONFIG_DRIVER_BSD
CFLAGS += -DCONFIG_DRIVER_BSD
OBJS += driver_bsd.o
CONFIG_L2_PACKET=y
CONFIG_DNET_PCAP=y
CONFIG_L2_FREEBSD=y
endif

ifdef CONFIG_DRIVER_TEST
CFLAGS += -DCONFIG_DRIVER_TEST
OBJS += driver_test.o
endif

ifdef CONFIG_L2_PACKET
ifdef CONFIG_DNET_PCAP
CFLAGS += -DUSE_DNET_PCAP
ifdef CONFIG_L2_FREEBSD
LIBS += -lpcap
OBJS += $(DIR_WPA_SUPPLICANT)/l2_packet_freebsd.o
else
LIBS += -ldnet -lpcap
OBJS += $(DIR_WPA_SUPPLICANT)/l2_packet_pcap.o
endif
else
OBJS += $(DIR_WPA_SUPPLICANT)/l2_packet_linux.o
endif
endif


ifdef CONFIG_EAP_MD5
CFLAGS += -DEAP_MD5
OBJS += eap_md5.o
endif

ifdef CONFIG_EAP_TLS
CFLAGS += -DEAP_TLS
OBJS += eap_tls.o
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_PEAP
CFLAGS += -DEAP_PEAP
OBJS += eap_peap.o
TLS_FUNCS=y
CONFIG_EAP_TLV=y
CONFIG_EAP_MSCHAPV2=y
endif

ifdef CONFIG_EAP_TTLS
CFLAGS += -DEAP_TTLS
OBJS += eap_ttls.o
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_MSCHAPV2
CFLAGS += -DEAP_MSCHAPv2
OBJS += eap_mschapv2.o
MS_FUNCS=y
endif

ifdef CONFIG_EAP_GTC
CFLAGS += -DEAP_GTC
OBJS += eap_gtc.o
endif

ifdef CONFIG_EAP_SIM
CFLAGS += -DEAP_SIM
OBJS += eap_sim.o $(DIR_WPA_SUPPLICANT)/eap_sim_common.o
# Example EAP-SIM interface for GSM authentication. This can be replaced with
# another file implementating the interface specified in eap_sim_db.h.
OBJS += eap_sim_db.o
endif

ifdef CONFIG_EAP_PAX
CFLAGS += -DEAP_PAX
OBJS += eap_pax.o $(DIR_WPA_SUPPLICANT)/eap_pax_common.o
endif

ifdef CONFIG_EAP_PSK
CFLAGS += -DEAP_PSK
OBJS += eap_psk.o $(DIR_WPA_SUPPLICANT)/eap_psk_common.o
endif

ifdef CONFIG_EAP_TLV
CFLAGS += -DEAP_TLV
OBJS += eap_tlv.o
endif

ifneq ($(CONFIG_RG_WPS)$(CONFIG_EAP),)
CFLAGS += -DEAP_SERVER
OBJS += eap.o eap_identity.o
else
OBJS += eap_stubs.o
endif

ifdef TLS_FUNCS
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
CFLAGS += -DEAP_TLS_FUNCS
OBJS += eap_tls_common.o $(DIR_WPA_SUPPLICANT)/tls_openssl.o
LIBS += -lssl -lcrypto
LIBS_p += -lcrypto
else
OBJS += $(DIR_WPA_SUPPLICANT)/tls_none.o
endif

ifdef CONFIG_PKCS12
CFLAGS += -DPKCS12_FUNCS
endif

ifdef MS_FUNCS
ifndef TLS_FUNCS
LIBS += -lcrypto
endif
OBJS += $(DIR_WPA_SUPPLICANT)/ms_funcs.o $(DIR_WPA_SUPPLICANT)/crypto.o
endif

ifdef CONFIG_RADIUS_SERVER
CFLAGS += -DRADIUS_SERVER
OBJS += radius_server.o
endif

ifdef CONFIG_IPV6
CFLAGS += -DCONFIG_IPV6
endif

ifeq ($(CONFIG_RG_WPS),y)
  CFLAGS += -DEAP_WSC -DWSC_IE -DSIMPLE_CONFIG -DWSC_AKM_NO_1X
  OBJS += eap_wsc.o UdpLib.o wsc_ie.o
else
  OBJS += wsc_stubs.o
endif

ifdef CONFIG_RG_JPKG
  OBJS += eap_stubs.o wsc_stubs.o
endif

TARGET=hostapd_048
RAMDISK_BIN_FILES+=$(TARGET)
O_OBJS=$(OBJS)
INTERNAL_HEADERS+=accounting.h aes_wrap.h ap.h config.h \
  config_types.h ctrl_iface.h defs.h driver.h eap.h eap_defs.h \
  eap_i.h eap_pax_common.h eap_psk_common.h eap_sim_common.h eap_sim_db.h \
  eap_tls_common.h eap_ttls.h eapol_sm.h eloop.h hostap_common.h hostapd.h \
  iapp.h ieee802_11.h ieee802_11_auth.h ieee802_1x.h l2_packet.h \
  ms_funcs.h prism54.h priv_netlink.h radius.h radius_client.h radius_server.h \
  sha1.h sta_info.h tls.h version.h wireless_copy.h wpa.h wpa_ctrl.h

CD_EXPORTED_FILES+=aes.c md5.c rc4.c common.h md5.h crypto.h rc4.h
include $(RGMK)

