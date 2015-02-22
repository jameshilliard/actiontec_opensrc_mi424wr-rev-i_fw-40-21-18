#
# Makefile for the IPsec basic part (PF_KEY_V2)
#
# $USAGI: Makefile,v 1.4 2002/07/23 13:14:02 yoshfuji Exp $

O_TARGET := key.o

obj-y := af_key.o pfkey_v2_build.o pfkey_v2_ext_bits.o \
	pfkey_v2_msg.o pfkey_v2_msg_add.o pfkey_v2_msg_delete.o pfkey_v2_msg_get.o \
	pfkey_v2_msg_getspi.o pfkey_v2_msg_update.o pfkey_v2_msg_flow.o \
	sa_index.o sadb.o sadb_utils.o sockaddrtoa.o spd.o spd_utils.o 


obj-$(CONFIG_SYSCTL) += sysctl_net_ipsec.o

EXTRA_CFLAGS+=-I./
include $(TOPDIR)/Rules.make
