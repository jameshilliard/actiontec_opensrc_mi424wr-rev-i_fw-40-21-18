#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= ixp2000.o

# Object file lists.

obj-y			:= arch.o mm.o ixp2000-irq.o ixp2000-pci.o pci.o slowport.o ixp2000-gpio.o time.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= ixp2000-gpio.o 

obj-$(CONFIG_ARCH_IXDP2400)	+= ixdp2400-irq.o ixdp2400-pci.o 

obj-$(CONFIG_ARCH_IXDP2800)	+= ixdp2800-irq.o ixdp2800-pci.o pci-auto.o

leds-$(CONFIG_ARCH_IXDP2800)	+= ixdp2800-leds.o
obj-$(CONFIG_LEDS)		+= $(leds-y)

obj-$(CONFIG_KGDB)      	+= ixp2000-kgdb.o

include $(TOPDIR)/Rules.make
