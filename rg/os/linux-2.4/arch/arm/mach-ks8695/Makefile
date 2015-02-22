#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= ks8695.o

# Object file lists.

obj-y			:= arch.o irq.o mm.o ks8695_utils.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= leds.o ks8695_utils.o

obj-$(CONFIG_LEDS)	+= leds.o
obj-$(CONFIG_PCI)       += ks8695p_pci.o pci.o
obj-$(CONFIG_KGDB) 	+= kgdb.o

include $(TOPDIR)/Rules.make
