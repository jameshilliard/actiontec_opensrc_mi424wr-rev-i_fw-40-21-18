
# Change the following to switch configurations:
#  (uncomment exactly one)
#include full.config
include no_enterprise.config
#include small.config
#include tiny.config

include ../common/Makefile.common

ALL=hostapd
# ? hostapd_cli hcmd

all: $(ALL)
	echo hostapd compiled ok

install: all
	cp -a $(ALL) $(INSTALL_ROOT)/sbin/.

# OBJS: object files for nostapd program
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

hostapd: $(OBJS) $(COMMON_LIBS)
	$(CC) -o hostapd $(CFLAGS) $(OBJS) $(LIBS)

driver_conf.c: Makefile $(CONFIG_FILE)
	rm -f driver_conf.c
	echo '/* THIS FILE AUTOMATICALLY GENERATED, DO NOT EDIT! */' \
							> driver_conf.c
	echo '#include "includes.h"'			>> driver_conf.c
	echo '#include "hostapd.h"'			>> driver_conf.c
	echo '#include "driver.h"'			>> driver_conf.c
ifdef CONFIG_DRIVER_HOSTAP
	echo "void hostap_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_WIRED
	echo "void wired_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_MADWIFI
	echo "void madwifi_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_PRISM54
	echo "void prism54_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_DEVICESCAPE
	echo "void devicescape_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_BSD
	echo "void bsd_driver_register(void);"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_TEST
	echo "void test_driver_register(void);"		>> driver_conf.c
endif
	echo 'void register_drivers(void) {'		>> driver_conf.c
ifdef CONFIG_DRIVER_HOSTAP
	echo "hostap_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_WIRED
	echo "wired_driver_register();"			>> driver_conf.c
endif
ifdef CONFIG_DRIVER_MADWIFI
	echo "madwifi_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_PRISM54
	echo "prism54_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_DEVICESCAPE
	echo "devicescape_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_BSD
	echo "bsd_driver_register();"			>> driver_conf.c
endif
ifdef CONFIG_DRIVER_TEST
	echo "test_driver_register();"			>> driver_conf.c
endif
	echo '}'					>> driver_conf.c

#hostapd_cli: hostapd_cli.o $(DIR_WPA_SUPPLICANT)/wpa_ctrl.o
#	$(CC) -o hostapd_cli hostapd_cli.o $(DIR_WPA_SUPPLICANT)/wpa_ctrl.o
#
#hcmd: hcmd.o $(DIR_WPA_SUPPLICANT)/wpa_ctrl.o
#	$(CC) -o hcmd hcmd.o $(DIR_WPA_SUPPLICANT)/wpa_ctrl.o
#
#NOBJS = nt_password_hash.o $(DIR_WPA_SUPPLICANT)/ms_funcs.o sha1.o rc4.o md5.o
#NOBJS += $(DIR_WPA_SUPPLICANT)/crypto.o os_$(CONFIG_OS).o
#
#nt_password_hash: $(NOBJS)
#	$(CC) -o nt_password_hash $(NOBJS) $(LIBS_n)
#
#hlr_auc_gw: $(HOBJS)
#	$(CC) -o hlr_auc_gw $(HOBJS) $(LIBS_h)
#
clean:
	rm -f core junk* *~ *.o hostapd hostapd_cli hcmd nt_password_hash hlr_auc_gw
	rm -f *.d driver_conf.c

%.eps: %.fig
	fig2dev -L eps $*.fig $*.eps

%.png: %.fig
	fig2dev -L png -m 3 $*.fig | pngtopnm | pnmscale 0.4 | pnmtopng \
		> $*.png

docs-pics: doc/hostapd.png doc/hostapd.eps

docs: docs-pics
	doxygen doc/doxygen.full
	$(MAKE) -C doc/latex
	cp doc/latex/refman.pdf hostapd-devel.pdf

docs-fast: docs-pics
	doxygen doc/doxygen.fast

clean-docs:
	rm -rf doc/latex doc/html
	rm -f doc/hosta.d{eps,png} hostapd-devel.pdf

#TEST_SRC_MILENAGE = milenage.c aes_wrap.c common.c os_$(CONFIG_OS).c
#test-milenage: $(TEST_SRC_MILENAGE)
#	$(CC) -o test-milenage -Wall -Werror $(TEST_SRC_MILENAGE) \
#		-DTEST_MAIN_MILENAGE -I. -I../wpa_supplicant -DINTERNAL_AES
#	./test-milenage
#	rm test-milenage

-include $(OBJS:%.o=%.d)
