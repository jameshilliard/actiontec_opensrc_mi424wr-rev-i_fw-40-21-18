#
# Makefile for the linux cramfs routines.
#

RGSRC=../../../../

CFLAGS_inode.o+=-DCRAMFS_TIMESTAMP=$(shell date +%s)

ifdef CONFIG_RG_CRAMFS_IN_FLASH
  ifdef CRAMFS_FLASH_ADDR
    CFLAGS_inode.o+=-DCRAMFS_FLASH_ADDR=$(CRAMFS_FLASH_ADDR)
  endif
  ifdef CRAMFS_FLASH_SIZE
    CFLAGS_inode.o+=-DCRAMFS_FLASH_SIZE=$(CRAMFS_FLASH_SIZE)
  endif
endif

obj-$(CONFIG_CRAMFS) += cramfs.o

cramfs-objs := inode.o uncompress.o

# Always recompile to update cramfs timestamp with every compilation
ifdef IS_BUILDDIR
inode.o: dummy
endif

