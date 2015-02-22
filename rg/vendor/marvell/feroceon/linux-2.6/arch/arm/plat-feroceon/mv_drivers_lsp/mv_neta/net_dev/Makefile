#
# Makefile for the Marvell Gigabit Ethernet driver
#
TOPDIR=$(shell pwd)

ifeq ($(CONFIG_ARCH_FEROCEON_KW2),y)
	include $(TOPDIR)/arch/arm/mach-feroceon-kw2/config/mvRules.mk
endif

ifeq ($(CONFIG_ARCH_ARMADA_XP),y)
	include $(TOPDIR)/arch/arm/mach-armadaxp/config/mvRules.mk
endif

obj-$(CONFIG_MV_ETHERNET) += mv_netdev.o mv_ethernet.o mv_eth_sysfs.o
obj-$(CONFIG_MV_ETH_PROC) += mv_eth_proc.o
obj-$(CONFIG_MV_PON)      += mv_pon_sysfs.o
obj-$(CONFIG_MV_ETH_SWITCH) +=  mv_eth_switch.o mv_eth_port_stat_proc.o
obj-$(CONFIG_MV_ETH_TOOL) += mv_eth_tool.o

