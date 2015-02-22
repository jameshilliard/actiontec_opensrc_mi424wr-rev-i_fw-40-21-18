#
# Makefile for the kernel serial device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#
#  $Id: Makefile,v 1.1.1.1 2007/05/07 23:29:50 jungo Exp $
#

O_TARGET := serial.o

export-objs	:= core.o 8250.o
obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

serial-8250-y :=
serial-8250-$(CONFIG_PCI) += 8250_pci.o
serial-8250-$(CONFIG_ISAPNP) += 8250_pnp.o
obj-$(CONFIG_SERIAL_CORE) += core.o
obj-$(CONFIG_SERIAL_21285) += 21285.o
obj-$(CONFIG_SERIAL_8250) += 8250.o $(serial-8250-y)
obj-$(CONFIG_SERIAL_ANAKIN) += anakin.o
obj-$(CONFIG_SERIAL_AMBA) += amba.o
obj-$(CONFIG_SERIAL_CLPS711X) += clps711x.o
obj-$(CONFIG_SERIAL_SA1100) += sa1100.o
obj-$(CONFIG_SERIAL_UART00) += uart00.o
obj-$(CONFIG_SERIAL_KS8695) += serial_ks8695.o
obj-$(CONFIG_SERIAL_IXP1200) += ixp1200.o
obj-$(CONFIG_SERIAL_OMAHA) += omaha.o
obj-$(CONFIG_SERIAL_AT91US3) += at91us3.o

include $(TOPDIR)/Rules.make

fastdep:

