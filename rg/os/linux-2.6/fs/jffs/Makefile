#
# Makefile for the linux Journalling Flash FileSystem (JFFS) routines.
#
# $Id: Makefile,v 1.1.1.1 2007/05/07 23:30:04 jungo Exp $
#

obj-$(CONFIG_JFFS_FS) += jffs.o

jffs-y 				:= jffs_fm.o intrep.o inode-v23.o
jffs-$(CONFIG_JFFS_PROC_FS)	+= jffs_proc.o
jffs-objs			:= $(jffs-y)
