#
# Makefile for the linux ext3-filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := ext3.o

obj-y    := balloc.o bitmap.o dir.o file.o fsync.o ialloc.o inode.o \
		ioctl.o namei.o super.o symlink.o
obj-m    := $(O_TARGET)

export-objs += xattr.o
obj-$(CONFIG_EXT3_FS_XATTR) += xattr.o
obj-$(CONFIG_EXT3_FS_XATTR_USER) += xattr_user.o
obj-$(CONFIG_EXT3_FS_XATTR_TRUSTED) += xattr_trusted.o
obj-$(CONFIG_EXT3_FS_POSIX_ACL) += acl.o

include $(TOPDIR)/Rules.make
