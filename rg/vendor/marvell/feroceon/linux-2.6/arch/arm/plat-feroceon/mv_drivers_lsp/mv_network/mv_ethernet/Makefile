#
# Makefile for the Marvell Gigabit Ethernet driver
#
ifeq ($(CONFIG_ARCH_FEROCEON),y)
	include $(srctree)/$(MACHINE)/config/mvRules.mk
endif

obj-$(CONFIG_MV_ETHERNET) += mv_netdev.o mv_ethernet.o
obj-$(CONFIG_MV_ETH_PROC) += mv_eth_proc.o
obj-$(CONFIG_MV_GATEWAY) +=  mv_gateway.o
obj-$(CONFIG_MV_GTW_IGMP) += mv_gtw_igmp.o
obj-$(CONFIG_MV_ETH_TOOL) += mv_eth_tool.o

