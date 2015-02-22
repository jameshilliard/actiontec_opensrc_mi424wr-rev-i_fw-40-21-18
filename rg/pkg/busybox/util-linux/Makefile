RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_util-linux.a+=$(if $(CONFIG_DMESG),dmesg.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_FBSET),fbset.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_FDFLUSH),fdflush.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_FDFORMAT),fdformat.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_FDISK),fdisk.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_FREERAMDISK),freeramdisk.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_FSCK_MINIX),fsck_minix.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_GETOPT),getopt.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_HEXDUMP),hexdump.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_HWCLOCK),hwclock.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_LOSETUP),losetup.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_MKFS_MINIX),mkfs_minix.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_MKSWAP),mkswap.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_MORE),more.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_MOUNT),mount.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_NFSMOUNT),nfsmount.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_PIVOT_ROOT),pivot_root.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_RDATE),rdate.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_SWAPONOFF),swaponoff.o)
O_OBJS_util-linux.a+=$(if $(CONFIG_UMOUNT),umount.o)

ifneq ($(strip $(O_OBJS_util-linux.a)),)
  A_TARGET+=util-linux.a
endif

ifneq ($(strip $(CONFIG_LFS)),y)
ifeq ($(strip $(FDISK_SUPPORT_LARGE_DISKS)),y)
  CFLAGS_fdisk.o+=-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE \
    -D_FILE_OFFSET_BITS=64
endif
endif

include $(RGMK)
