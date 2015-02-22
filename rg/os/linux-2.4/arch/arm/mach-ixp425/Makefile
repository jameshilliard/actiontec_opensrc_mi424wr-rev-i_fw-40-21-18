#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= ixp425.o

# Object file lists.

obj-y			:= arch.o  mm.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= gpio.o

ifdef CONFIG_PCI
  export-objs += pcibuf.o pci-io.o
endif

ifdef CONFIG_ARCH_IXP425_BAMBOO
  export-objs += bamboo-cs6.o
endif

obj-$(CONFIG_ARCH_IXP425)	+= ixp425-irq.o gpio.o 

obj-$(CONFIG_PCI)		+= ixp425-pci.o pci.o pci-auto.o \
				   pci-io.o pcibuf.o
ifdef CONFIG_PCI
obj-$(CONFIG_ARCH_IXP425_IXDP425)	+= ixdp425-pci.o

obj-$(CONFIG_ARCH_IXP425_HG21)		+= hg21-pci.o

obj-$(CONFIG_ARCH_IXP425_JEEVES)	+= jeeves-pci.o

obj-$(CONFIG_ARCH_IXP425_MATECUMBE)	+= matecumbe-pci.o

obj-$(CONFIG_ARCH_IXP425_NAPA)		+= napa-pci.o

obj-$(CONFIG_ARCH_IXP425_YOSEMITE)	+= yosemite-pci.o

obj-$(CONFIG_ARCH_IXP425_KINGSCANYON)	+= kingscanyon-pci.o

obj-$(CONFIG_ARCH_IXP425_ROCKAWAYBEACH)	+= rockawaybeach-pci.o

obj-$(CONFIG_ARCH_IXP425_COYOTE)	+= coyote-pci.o

obj-$(CONFIG_ARCH_IXP425_BAMBOO)	+= bamboo-pci.o

obj-$(CONFIG_ARCH_IXP425_GTWX5800)	+= wx5800-pci.o

obj-$(CONFIG_ARCH_IXP425_GTWX5711)	+= wx5711-pci.o

obj-$(CONFIG_ARCH_IXP425_GTWX5715)	+= wx5715-pci.o

obj-$(CONFIG_ARCH_IXP425_WAV54G)	+= wagp100g-pci.o

obj-$(CONFIG_ARCH_IXP425_MONTEJADE)	+= montejade-pci.o

obj-$(CONFIG_ARCH_IXP425_BRUCE)		+= bruce-pci.o
endif

obj-$(CONFIG_IXP425_OS_TIMER1)		+= ixp425-time.o 

obj-$(CONFIG_KGDB) 	+= kgdb.o

obj-$(CONFIG_ARCH_IXP425_BAMBOO)	+= bamboo-cs6.o

obj-$(CONFIG_ARCH_IXP425_WAV54G)	+= wav54g-fixup.o

include $(TOPDIR)/Rules.make
