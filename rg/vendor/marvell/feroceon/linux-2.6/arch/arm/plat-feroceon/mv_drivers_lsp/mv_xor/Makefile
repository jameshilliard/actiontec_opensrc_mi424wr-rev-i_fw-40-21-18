#
# Makefile for the Marvell XOR/DMA Driver
#
ifeq ($(CONFIG_ARCH_FEROCEON),y)
	include $(srctree)/$(MACHINE)/config/mvRules.mk
endif

obj-$(CONFIG_MV_USE_XOR_ENGINE) += mv_netdma.o
