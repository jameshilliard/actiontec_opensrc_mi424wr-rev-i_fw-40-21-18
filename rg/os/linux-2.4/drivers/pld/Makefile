#
# Makefile for the kernel pld device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#
#  $Id: Makefile,v 1.1.1.1 2007/05/07 23:29:28 jungo Exp $
#

O_TARGET := pld.o

export-objs	:= pld_hotswap.o
obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

obj-$(CONFIG_PLD)              += pld_epxa.o
obj-$(CONFIG_PLD_HOTSWAP)      += pld_hotswap.o

include $(TOPDIR)/Rules.make

fastdep:

