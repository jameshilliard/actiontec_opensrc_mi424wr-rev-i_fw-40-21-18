#
# linux/drivers/chips/Makefile
#
# $Id: Makefile,v 1.1.1.1.44.1 2010/12/23 00:13:11 athill Exp $

#                       *** BIG UGLY NOTE ***
#
# The removal of get_module_symbol() and replacement with
# inter_module_register() et al has introduced a link order dependency
# here where previously there was none.  We now have to ensure that
# the CFI command set drivers are linked before gen_probe.o

ifeq ($(CONFIG_ARCH_FEROCEON),y)
obj-$(CONFIG_MV_FLASH_CTRL)	+= ../../../arch/arm/plat-feroceon/mv_drivers_lsp/mv_mtd/
endif
obj-$(CONFIG_MTD)		+= chipreg.o
obj-$(CONFIG_MTD_AMDSTD)	+= amd_flash.o
obj-$(CONFIG_MTD_CFI)		+= cfi_probe.o
obj-$(CONFIG_MTD_CFI_UTIL)	+= cfi_util.o
obj-$(CONFIG_MTD_CFI_STAA)	+= cfi_cmdset_0020.o
obj-$(CONFIG_MTD_CFI_AMDSTD)	+= cfi_cmdset_0002.o
obj-$(CONFIG_MTD_CFI_INTELEXT)	+= cfi_cmdset_0001.o
obj-$(CONFIG_MTD_GEN_PROBE)	+= gen_probe.o
obj-$(CONFIG_MTD_JEDEC)		+= jedec.o
obj-$(CONFIG_MTD_JEDECPROBE)	+= jedec_probe.o
obj-$(CONFIG_MTD_RAM)		+= map_ram.o
obj-$(CONFIG_MTD_ROM)		+= map_rom.o
obj-$(CONFIG_MTD_SHARP)		+= sharp.o
obj-$(CONFIG_MTD_ABSENT)	+= map_absent.o
