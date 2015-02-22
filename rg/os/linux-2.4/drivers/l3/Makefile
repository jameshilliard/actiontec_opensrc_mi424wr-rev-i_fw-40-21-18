#
# Makefile for the L3 bus driver.
#

O_TARGET := l3.o

export-objs	:= l3-core.o l3-algo-bit.o
l3-y		:=
l3-n		:=
l3-drv-y	:=
l3-drv-n	:=

# Link order:
#  (core, adapters, algorithms, drivers) then clients

l3-$(CONFIG_L3_ALGOBIT)		+= l3-algo-bit.o
l3-$(CONFIG_BIT_SA1100_GPIO)	+= l3-bit-sa1100.o
l3-$(CONFIG_L3_SA1111)		+= l3-sa1111.o

obj-$(CONFIG_L3)		+= l3-core.o $(l3-y) $(l3-drv-y)

include $(TOPDIR)/Rules.make

