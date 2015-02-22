#
# Makefile for the Marvell Gigabit Ethernet driver
#
ifeq ($(CONFIG_ARCH_FEROCEON_KW2),y)
	TOPDIR=$(shell pwd)
	include $(TOPDIR)/arch/arm/mach-feroceon-kw2/config/mvRules.mk
endif

obj-$(CONFIG_MV_ETH_HWF) += hwf_sysfs.o
