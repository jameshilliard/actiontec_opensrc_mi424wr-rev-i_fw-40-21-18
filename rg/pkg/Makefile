RGSRC=../
include $(RGSRC)/envir.mak

# XXX Currently libc build from the rg/Makefile. This should be fixed.
PKG_LIBC+=$(if $(CONFIG_ULIBC),$(if $(ULIBC_IN_TOOLCHAIN),,ulibc))
PKG_LIBC+=$(if $(CONFIG_GLIBC),$(if $(GLIBC_IN_TOOLCHAIN),,glibc))
#SUBDIRS+=$(PKG_LIBC)
#LINK_DIRS+=$(PKG_LIBC)
JPKG_EXPORTED_DIR+=$(PKG_LIBC)

# Tools , added By Simba for ipv6 support BEGIN
SUBDIRS+=$(if $(CONFIG_IPV6),ctl_layer)
SUBDIRS+=$(if $(CONFIG_IPV6),haproxy)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_IPV6),haproxy)
SUBDIRS+=$(if $(CONFIG_IPV6),totd)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_IPV6),totd)
SUBDIRS+=$(if $(CONFIG_IPV6),radvd)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_IPV6),radvd)
SUBDIRS+=$(if $(CONFIG_IPV6),iputils)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_IPV6),iputils)
SUBDIRS+=$(if $(CONFIG_IPV6),dhcpv6)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_IPV6),dhcpv6)
SUBDIRS+=$(if $(CONFIG_IPV6),ip6tables)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_IPV6),ip6tables)
SUBDIRS+=$(if $(CONFIG_IPV6),mld)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_IPV6),mld)
SUBDIRS+=$(if $(ACTION_TEC_IPV6_FIREWALL),act_nf)
JPKG_EXPORTED_DIR+=$(if $(ACTION_TEC_IPV6_FIREWALL),act_nf)

# Tools , added By Simba for ipv6 support END


SUBDIRS+=$(if $(ACTION_TEC_DMALLOC),dmalloc)
SUBDIRS+=$(if $(CONFIG_UCLIBCXX),uclibc++)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_UCLIBCXX),uclibc++)
SUBDIRS+=$(if $(CONFIG_RG_OPENSSL_COMMON),openssl)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_OPENSSL_COMMON),openssl)
LINK_DIRS+=$(if $(CONFIG_RG_OPENSSL_COMMON),openssl)
SUBDIRS+=$(if $(CONFIG_RG_SYSLOG_COMMON),syslog)
SUBDIRS+=util
SUBDIRS+=$(if $(CONFIG_RG_ZLIB),zlib)
SUBDIRS+=$(if $(CONFIG_RG_LANG),language)
SUBDIRS+=$(if $(CONFIG_RG_LICENSE),license)
SUBDIRS+=include
SUBDIRS+=$(if $(CONFIG_RG_DOC_ENABLED),doc)
LINK_DIRS+=$(if $(CONFIG_RG_DOC_ENABLED),doc)
SUBDIRS+=$(if $(CONFIG_RG_LZMA),lzma)
SUBDIRS+=build
SUBDIRS+=$(if $(CONFIG_RG_TZ),tz)
SUBDIRS+=$(if $(CONFIG_RG_TDBM),tdbm)
SUBDIRS+=$(if $(CONFIG_RG_MGT),mgt)
ifeq ($(CONFIG_RG_ONE_MODULE),)
SUBDIRS+=$(if $(CONFIG_RG_KERNEL),kernel)
endif
SUBDIRS+=$(if $(CONFIG_RG_LIBMAD),av/libmad)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_LIBMAD),av/libmad)
LINK_DIRS+=$(if $(CONFIG_RG_LIBMAD),av/libmad)
SUBDIRS+=$(if $(CONFIG_IPTABLES),iptables)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_IPTABLES),iptables)
LINK_DIRS+=$(if $(CONFIG_IPTABLES),iptables)
SUBDIRS+=$(if $(CONFIG_RG_IPROUTE2_UTILS),iproute2)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_IPROUTE2_UTILS),iproute2)
LINK_DIRS+=$(if $(CONFIG_RG_IPROUTE2_UTILS),iproute2)
SUBDIRS+=$(if $(CONFIG_BRIDGE_UTILS),bridge-utils)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_BRIDGE_UTILS),bridge-utils)
SUBDIRS+=$(if $(CONFIG_RG_BZIP2)$(CONFIG_BOOTLDR_UBOOT),boot)
SUBDIRS+=$(if $(CONFIG_RG_BUSYBOX),busybox)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_BUSYBOX),busybox)
LINK_DIRS+=$(if $(CONFIG_RG_BUSYBOX),busybox)

SUBDIRS+=$(if $(CONFIG_CRAMFS_FS_COMMON),cramfs)

SUBDIRS+=$(if $(CONFIG_ROMFS_FS),genromfs)
SUBDIRS+=$(if $(CONFIG_RG_TODC)$(CONFIG_RG_SNTPS),tod)
SUBDIRS+=$(if $(CONFIG_RG_DNS),dns)
SUBDIRS+=$(if $(CONFIG_RG_HTTPS),httpd)
SUBDIRS+=$(if $(CONFIG_LOCAL_WBM_LIB)$(CONFIG_RG_WBM)$(CONFIG_RG_SSI),web_mng)
SUBDIRS+=$(if $(CONFIG_RG_LIBIMAGE)$(CONFIG_RG_LIBJPEG),image)
SUBDIRS+=$(if $(CONFIG_RG_KOS),kos)
SUBDIRS+=$(if $(CONFIG_RG_MSS),mss)
SUBDIRS+=$(if $(CONFIG_RG_ELVIS),elvis)
SUBDIRS+=$(if $(CONFIG_RG_CABLEHOME_COMMON),cablehome)
SUBDIRS+=$(if $(CONFIG_RG_KERBEROS),heimdal)
SUBDIRS+=$(if $(CONFIG_RG_UCD_SNMP),ucd-snmp)
SUBDIRS+=$(if $(CONFIG_RG_NETKIT),netkit)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_NETKIT),netkit)
LINK_DIRS+=$(if $(CONFIG_RG_NETKIT),netkit)
SUBDIRS+=$(if $(CONFIG_RG_NETTOOLS_ARP),net-tools)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_RG_NETTOOLS_ARP),net-tools)
LINK_DIRS+=$(if $(CONFIG_RG_NETTOOLS_ARP),net-tools)
SUBDIRS+=$(if $(CONFIG_RG_PERM_STORAGE),perm_storage)
SUBDIRS+=$(if $(CONFIG_FLASH),flash)
SUBDIRS+=$(if $(CONFIG_RG_LIBPCAP),libpcap)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_LIBPCAP),libpcap)
LINK_DIRS+=$(if $(CONFIG_RG_LIBPCAP),libpcap)
SUBDIRS+=$(if $(CONFIG_RG_TERMCAP),termcap)
SUBDIRS+=$(if $(CONFIG_RG_TCPDUMP),tcpdump)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_TCPDUMP),tcpdump)
LINK_DIRS+=$(if $(CONFIG_RG_TCPDUMP),tcpdump)
SUBDIRS+=$(if $(CONFIG_RG_RMT_UPDATE),rmt-update)
SUBDIRS+=$(if $(CONFIG_RG_WGET),wget)
SUBDIRS+=$(if $(CONFIG_RG_BLUETOOTH),bluetooth)
SUBDIRS+=$(if $(CONFIG_RG_KAFFE),kaffe)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_RG_KAFFE),kaffe)
LINK_DIRS+=$(if $(CONFIG_RG_KAFFE),kaffe)
SUBDIRS+=$(if $(CONFIG_RG_VOIP)$(CONFIG_RG_TR_104),voip)
SUBDIRS+=$(if $(CONFIG_RG_VOIP_DEMO),voip/dsp/test)
SUBDIRS+=$(if $(CONFIG_IXJ),voip/ixj)
SUBDIRS+=$(if $(CONFIG_GMP),gmp)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_GMP),gmp)
LINK_DIRS+=$(if $(CONFIG_GMP),gmp)
SUBDIRS+=$(if $(CONFIG_IPSEC),ipsec)
SUBDIRS+=$(if $(CONFIG_FREESWAN_COMMON),freeswan)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_FREESWAN_COMMON),freeswan)
LINK_DIRS+=$(if $(CONFIG_FREESWAN_COMMON),freeswan)
SUBDIRS+=$(if $(CONFIG_RG_X509),x509)
SUBDIRS+=$(if $(CONFIG_PCBOX),lilo)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_PCBOX),lilo)
LINK_DIRS+=$(if $(CONFIG_PCBOX),lilo)
SUBDIRS+=$(if $(CONFIG_ATM),atm)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_ATM),atm)
LINK_DIRS+=$(if $(CONFIG_ATM),atm)
SUBDIRS+=$(if $(CONFIG_RG_PPP_COMMON),ppp)
SUBDIRS+=$(if $(CONFIG_RG_PPPOE_RELAY),pppoe-server)
SUBDIRS+=$(if $(CONFIG_PCMCIA),pcmcia)
SUBDIRS+=$(if $(CONFIG_RG_GDBSERVER),gdb/gdb/gdbserver)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_GDBSERVER),gdb)
LINK_DIRS+=$(if $(CONFIG_RG_GDBSERVER),gdb/gdb/gdbserver)
SUBDIRS+=mtd-utils
JPKG_EXPORTED_DIR+=mtd-utils
LINK_DIRS+=mtd-utils
SUBDIRS+=$(if $(CONFIG_RG_RIP),rip)
SUBDIRS+=$(if $(CONFIG_RG_IGMP_PROXY),igmp)
SUBDIRS+=$(if $(CONFIG_RG_MCAST_RELAY),igmp)
SUBDIRS+=$(if $(CONFIG_RG_XML),expat)
SUBDIRS+=$(if $(CONFIG_RG_XML),xml)
SUBDIRS+=$(if $(CONFIG_RG_CLI),cli)
SUBDIRS+=$(if $(CONFIG_RG_UPNP),upnp)
SUBDIRS+=$(if $(CONFIG_RG_RMT_MNG),rmt_mng)
SUBDIRS+=$(if $(CONFIG_RG_TELNETS),telnet)
SUBDIRS+=$(if $(CONFIG_RG_SSH),openssh)
SUBDIRS+=$(if $(CONFIG_RG_BOOTLDR),bootldr)
SUBDIRS+=$(if $(CONFIG_RG_BRIDGE),bridge)
SUBDIRS+=$(if $(CONFIG_RG_STP),stp)
SUBDIRS+=$(if $(CONFIG_RG_USB_SLAVE),usb_slave)
SUBDIRS+=$(if $(CONFIG_VALGRIND),valgrind)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_VALGRIND),valgrind)
LINK_DIRS+=$(if $(CONFIG_VALGRIND),valgrind)
ifndef ACTION_TEC_USB_STORAGE
SUBDIRS+=$(if $(CONFIG_RG_DISK_MNG),disk_mng)
endif
SUBDIRS+=$(if $(CONFIG_RG_LPD),lpd)
SUBDIRS+=$(if $(CONFIG_RG_FILESERVER),file_server)
SUBDIRS+=$(if $(CONFIG_RG_PRINT_SERVER),print_server)
SUBDIRS+=$(if $(CONFIG_RG_ATTR),attr)
SUBDIRS+=$(if $(CONFIG_RG_ACL),acl)
SUBDIRS+=$(if $(CONFIG_RG_SAMBA),samba)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_RG_SAMBA),samba)
LINK_DIRS+=$(if $(CONFIG_RG_SAMBA),samba)
SUBDIRS+=$(if $(CONFIG_RG_E2FSPROGS),e2fsprogs)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_E2FSPROGS),e2fsprogs)
LINK_DIRS+=$(if $(CONFIG_RG_E2FSPROGS),e2fsprogs)
SUBDIRS+=$(if $(CONFIG_RG_DOSFSTOOLS),dosfstools)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_RG_DOSFSTOOLS),dosfstools)
LINK_DIRS+=$(if $(CONFIG_RG_DOSFSTOOLS),dosfstools)

#modified by Jet :build fuse lib which included by ntfs-3g rather than in forder fuse(it's nonexistent)
#SUBDIRS+=$(if $(CONFIG_FUSE_FS),fuse)
#JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_FUSE_FS),fuse)
#LINK_DIRS+=$(if $(CONFIG_FUSE_FS),fuse)

SUBDIRS+=$(if $(CONFIG_RG_NTFS_3G),ntfs-3g)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_RG_NTFS_3G),ntfs-3g)
LINK_DIRS+=$(if $(CONFIG_RG_NTFS_3G),ntfs-3g)
SUBDIRS+=$(if $(CONFIG_RG_NTFSPROGS),ntfsprogs)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_RG_NTFSPROGS),ntfsprogs)
LINK_DIRS+=$(if $(CONFIG_RG_NTFSPROGS),ntfsprogs)
SUBDIRS+=$(if $(CONFIG_RG_UTIL_LINUX),util-linux)
LINK_DIRS+=$(if $(CONFIG_RG_UTIL_LINUX),util-linux)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_UTIL_LINUX),util-linux)
SUBDIRS+=$(if $(CONFIG_RG_RAID),mdadm)
SUBDIRS+=$(if $(CONFIG_RG_DDNS),ddns)
SUBDIRS+=$(if $(CONFIG_RG_MAIL_SERVER),mail)
SUBDIRS+=$(if $(CONFIG_RG_ENTFY),mail_client)
SUBDIRS+=$(if $(CONFIG_RG_8021X),802.1x)
SUBDIRS+=$(if $(CONFIG_RG_WPA),wpa)
SUBDIRS+=$(if $(CONFIG_RG_RADIUS_COMMON),radius)
SUBDIRS+=$(if $(CONFIG_HOTPLUG),hotplug)
SUBDIRS+=$(if $(CONFIG_RG_FLEX),flex)
SUBDIRS+=$(if $(CONFIG_RG_USAGI),usagi)
SUBDIRS+=$(if $(CONFIG_RG_L2TPC)$(CONFIG_RG_L2TPS),l2tp)
SUBDIRS+=$(if $(CONFIG_RG_VLAN_8021Q),vlan)
SUBDIRS+=$(if $(CONFIG_RG_TFTP_SERVER),tftps)
SUBDIRS+=$(if $(CONFIG_ZC_AUTO_CONFIG_NON_PNP),auto_conf)
SUBDIRS+=$(if $(CONFIG_RG_ROUTE_MULTIWAN)$(CONFIG_RG_OS_VXWORKS)$(CONFIG_RG_QUAGGA),route)
SUBDIRS+=$(if $(CONFIG_RGTV),av rgtv)
SUBDIRS+=$(if $(CONFIG_RG_FTP_SERVER),ftp)
SUBDIRS+=$(if $(CONFIG_RG_WEB_SERVER),web_server)
SUBDIRS+=$(if $(CONFIG_RG_STAR),star)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_STAR),star)
LINK_DIRS+=$(if $(CONFIG_RG_STAR),star)
SUBDIRS+=$(if $(CONFIG_RG_BACKUP),backup)
SUBDIRS+=$(if $(CONFIG_RG_SAMPLES_COMMON),samples)
JPKG_EXPORTED_DIR_SRC+=samples
LINK_DIRS+=samples
SUBDIRS+=$(if $(CONFIG_RG_FIREWALL)$(CONFIG_RG_NAT)$(CONFIG_RG_RNAT),firewall)
SUBDIRS+=$(if $(CONFIG_RG_VENDOR),vendor)
SUBDIRS+=$(if $(CONFIG_RG_RGLOADER_COMMON),rgloader)
SUBDIRS+=$(if $(CONFIG_RG_TEST_TOOLS),test_tools)
SUBDIRS+=$(if $(CONFIG_RG_DHCP_COMMON),dhcp)
SUBDIRS+=$(if $(CONFIG_RG_XML2),libxml)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_XML2),libxml)
SUBDIRS+=$(if $(CONFIG_RG_XMLSEC),xmlsec)
JPKG_EXPORTED_DIR+=$(if $(CONFIG_RG_XMLSEC),xmlsec)
SUBDIRS+=$(if $(ACTION_TEC_TR143),tr143)
SUBDIRS+=$(if $(ACTION_TEC_TR143_VZ_PH2),xget)
ifdef ACTION_TEC_VERIZON
SUBDIRS+=$(if $(CONFIG_RG_DSLHOME),dslhome_aei)
SUBDIRS+=$(if $(ACTION_TEC_WMM),wmm)
else
SUBDIRS+=$(if $(CONFIG_RG_DSLHOME),dslhome)
endif	
SUBDIRS+=$(if $(CONFIG_RG_QOS),qos)
SUBDIRS+=$(if $(CONFIG_RG_TOOLS),tools)
SUBDIRS+=$(if $(CONFIG_RG_WEBCAM),webcam)
SUBDIRS+=$(if $(CONFIG_RG_FASTPATH),fastpath)
SUBDIRS+=$(if $(CONFIG_RG_TUNNELS),tunnels)
SUBDIRS+=$(if $(CONFIG_RG_SSL_VPN),ssl_vpn)
SUBDIRS+=$(if $(CONFIG_RG_WEB_CIFS),web_cifs)
SUBDIRS+=$(if $(CONFIG_RG_JAVA),java)
SUBDIRS+=$(if $(CONFIG_RG_IPERF),iperf)
SUBDIRS+=$(if $(CONFIG_RG_JNET_SERVER)$(CONFIG_RG_JNET_CLIENT),jnet)
ifndef CONFIG_BHR_REV_I
SUBDIRS+=$(if $(CONFIG_RG_WIRELESS_TOOLS),wireless_tools)
endif
SUBDIRS+=$(if $(CONFIG_RG_ANTIVIRUS),antivirus)
SUBDIRS+=$(if $(CONFIG_RG_WSC),wsc)
SUBDIRS+=$(if $(CONFIG_RG_WPS),wps)
JPKG_EXPORTED_DIR_SRC+=$(if $(CONFIG_RG_GNUDIP),gnudip)
ifndef CONFIG_RG_GPL
SUBDIRS+=$(if $(CONFIG_RG_HOSTAPD),hostapd)
endif
SUBDIRS+=$(if $(CONFIG_RG_WPS),wpatalk)
SUBDIRS+=$(if $(CONFIG_RG_REDUCE_SUPPORT),reduce_support)
SUBDIRS+=$(if $(CONFIG_RG_BITTORRENT),bittorrent)
ifdef ACTION_TEC_MEDIA_SERVER
SUBDIRS+=$(if $(ACTION_TEC_FFMPEG),ffmpeg)
SUBDIRS+=$(if $(ACTION_TEC_LIBDLNA),libdlna)
SUBDIRS+=$(if $(ACTION_TEC_LIBUPNP),libupnp)
SUBDIRS+=$(if $(ACTION_TEC_USHARE),ushare)
endif
ifdef ACTION_TEC_FTP_SERVER
SUBDIRS+=$(if $(ACTION_TEC_FTPD),ftpd)
endif
ifdef ACTION_TEC_PRINTER_SERVER
SUBDIRS+=$(if $(ACTION_TEC_PRINTER_SERVER),printer_server)
endif
# file server must be behind printer server
ifdef ACTION_TEC_FILE_SERVER
SUBDIRS+=$(if $(ACTION_TEC_FILE_SERVER),file_server)
SUBDIRS+=$(if $(ACTION_TEC_SAMBA),samba)
JPKG_EXPORTED_DIR_SRC+=$(if $(ACTION_TEC_SAMBA),samba)
LINK_DIRS+=$(if $(ACTION_TEC_SAMBA),samba)
endif

ifdef ACTION_TEC_OSGI
SUBDIRS+=osgiutil
endif

ifdef ACTION_TEC_OSGIFS
SUBDIRS+=osgifs
endif

SUBDIRS+=$(if $(ACTION_TEC_DISK_MNG),disk_mng)
# main _must_ compile after all libraries and tasks.
SUBDIRS+=$(if $(CONFIG_RG_MAIN),main)

# autotest is not part of openrg, but it depends on rg_default.c
# from main therefor it _must_ compile after main.
SUBDIRS+=$(if $(CONFIG_RG_AUTOTEST),autotest)

SUBDIRS+=$(if $(CONFIG_RG_JPKG),jpkg)

# tools are currently relevant for UML only
SUBDIRS+=$(if $(CONFIG_RG_JPKG_UML)$(CONFIG_RG_JPKG_SRC),tools)

ifneq ($(CONFIG_RG_ONE_MODULE),)
SUBDIRS+=$(if $(CONFIG_RG_KERNEL),kernel)
endif

# Build must be the last subdir in order for make ramdisk to work
# XXX We add libc because it is built from rg/Makefile. This shouldbe fixed
RAMDISK_SUBDIRS:=$(PKG_LIBC) $(filter-out build,$(SUBDIRS)) build

include $(RGMK)

