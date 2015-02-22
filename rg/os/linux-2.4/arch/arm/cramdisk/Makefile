#
# Makefile for a ramdisk image
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

O_FORMAT = $(shell $(OBJDUMP) -i | head -2 | grep elf32)

CRAMFS_TARGETS=cramdisk.o
ifdef CONFIG_RG_INITFS_CRAMFS
  CRAMFS_TARGETS+=cramdisk_init.o
endif

all: $(CRAMFS_TARGETS)

cramdisk.o: cramfs.img ld.script
	echo "O_FORMAT:  " $(O_FORMAT)
	$(LD) -T ld.script -b binary --oformat $(O_FORMAT) -o $@ cramfs.img

cramdisk_init.o: cramfs_init.img init.ld.script
	echo "O_FORMAT:  " $(O_FORMAT)
	$(LD) -T init.ld.script -b binary --oformat $(O_FORMAT) -o $@ cramfs_init.img

include $(TOPDIR)/Rules.make

