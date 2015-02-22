#
# Makefile for the Marvell Gigabit Ethernet driver
#
ifeq ($(CONFIG_ARCH_FEROCEON_KW2),y)
	TOPDIR=$(shell pwd)
	include $(TOPDIR)/arch/arm/mach-feroceon-kw2/config/mvRules.mk
endif

obj-$(CONFIG_MV_ETH_NFP)     += mv_nfp_mgr.o nfp_sysfs.o
obj-$(CONFIG_MV_ETH_NFP_SEC) += mv_nfp_sec.o
