#
# Makefile for the Marvell CESA driver
#

TOPDIR=$(shell pwd)
ifeq ($(CONFIG_ARCH_FEROCEON),y)
	include $(TOPDIR)/$(MACHINE)/config/mvRules.mk
endif

ifeq ($(CONFIG_MV_CESA_OCF),y)
EXTRA_CFLAGS    += -I$(TOPDIR)/crypto/ocf
endif

obj-$(CONFIG_MV_CESA_TOOL) += cesa_dev.o
obj-$(CONFIG_MV_CESA_OCF)  += cesa_ocf_drv.o

