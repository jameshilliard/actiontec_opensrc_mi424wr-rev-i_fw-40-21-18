#
# Makefile for a ramdisk image
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

O_FORMAT = $(shell $(OBJDUMP) -i | head -2 | grep elf32)

RAMDISK_TARGETS=
ifdef CONFIG_RG_INITFS_RAMDISK
  RAMDISK_TARGETS+=ramdisk.o
endif
ifdef CONFIG_RG_MODFS
  RAMDISK_TARGETS+=modrd.o
endif

all: $(RAMDISK_TARGETS)

ramdisk.o: ramdisk.gz ld.script
	echo "O_FORMAT:  " $(O_FORMAT)
	$(LD) -T ld.script -b binary --oformat $(O_FORMAT) -o $@ ramdisk.gz

modrd.o: mod.img modld.script
	$(LD) -T modld.script -b binary --oformat $(O_FORMAT) -o $@ mod.img

include $(TOPDIR)/Rules.make

