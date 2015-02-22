# File: drivers/at91/mtd/Makefile
#
# Makefile for the Atmel AT91RM9200 MTD devices.
#   Includes: NAND flash (SmartMedia) & DataFlash
#

O_TARGET := at91mtd.o

export-objs :=

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

obj-$(CONFIG_MTD_AT91_DATAFLASH) += at91_dataflash.o
obj-$(CONFIG_MTD_AT91_SMARTMEDIA) += at91_nand.o

include $(TOPDIR)/Rules.make
