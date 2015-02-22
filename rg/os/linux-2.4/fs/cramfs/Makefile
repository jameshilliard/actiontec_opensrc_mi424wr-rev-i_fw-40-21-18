#
# Makefile for the linux cramfs routines.
#

O_TARGET := cramfs.o

CFLAGS_inode.o=-DCRAMFS_TIMESTAMP=$(shell date +%s)

obj-y  := inode.o uncompress.o

obj-m := $(O_TARGET)

CFLAGS_uncompress.o := -I $(TOPDIR)/fs/inflate_fs

include $(TOPDIR)/Rules.make

ifdef IS_BUILDDIR
inode.o: dummy
endif

