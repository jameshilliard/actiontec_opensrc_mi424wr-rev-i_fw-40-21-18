#
# linux/drivers/nand/Makefile
#
# $Id: Makefile,v 1.1.1.1.2.1.42.1 2010/12/23 00:13:11 athill Exp $

obj-$(CONFIG_MTD_NAND)			+= nand.o nand_ecc.o
obj-$(CONFIG_MTD_NAND_IDS)		+= nand_ids.o

obj-$(CONFIG_MTD_NAND_SPIA)		+= spia.o
obj-$(CONFIG_MTD_NAND_TOTO)		+= toto.o
obj-$(CONFIG_MTD_NAND_AUTCPU12)		+= autcpu12.o
obj-$(CONFIG_MTD_NAND_EDB7312)		+= edb7312.o
obj-$(CONFIG_MTD_NAND_COMCERTO)		+= comcerto.o
obj-$(CONFIG_MTD_NAND_AU1550)		+= au1550nd.o
obj-$(CONFIG_MTD_NAND_PPCHAMELEONEVB)	+= ppchameleonevb.o
obj-$(CONFIG_MTD_NAND_S3C2410)		+= s3c2410.o
obj-$(CONFIG_MTD_NAND_DISKONCHIP)	+= diskonchip.o
obj-$(CONFIG_MTD_NAND_H1900)		+= h1910.o
obj-$(CONFIG_MTD_NAND_RTC_FROM4)	+= rtc_from4.o
obj-$(CONFIG_MTD_NAND_SHARPSL)		+= sharpsl.o
obj-$(CONFIG_MTD_NAND_NANDSIM)		+= nandsim.o

nand-objs = nand_base.o nand_bbt.o
ifeq ($(CONFIG_MV_NAND),y)
ifeq ($(CONFIG_ARCH_FEROCEON_KW2),y)
TOPDIR=$(shell pwd)
include $(TOPDIR)/arch/arm/mach-feroceon-kw2/config/mvRules.mk
nand-objs += ../../../arch/arm/mach-feroceon-kw2/nand.o
endif
 
endif
