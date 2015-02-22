#
# Makefile for the Marvell Watchdog Driver
#
ifeq ($(CONFIG_ARCH_FEROCEON),y)
	include $(srctree)/$(MACHINE)/config/mvRules.mk
endif

obj-$(CONFIG_MV_WATCHDOG) += feroceon_wdt.o
