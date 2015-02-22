# Makefile for KLIPS kernel code
# Copyright (C) 1998, 1999  Richard Guy Briggs.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

RGSRC=../../../../..
include $(RGSRC)/envir.mak

SUBDIRS = zlib libfreeswan

MOD_TARGET = ipsec.o
O_OBJS_$(MOD_TARGET) = ipsec_init.o sysctl_net_ipsec.o \
	pfkey_v2.o pfkey_v2_parser.o ipsec_sa.o ipsec_life.o ipsec_proc.o \
	ipsec_rcv_common.o ipsec_tunnel_common.o

OX_OBJS_$(MOD_TARGET) = radij.o ipsec_log.o ipsec_radij.o
CD_EXPORTED_FILES = radij.h ipsec_encap.h ipsec_netlink.h ipsec_kversion.h \
  ipsec_log.h ipsec_radij.h ipsec_tunnel.h ipsec_rcv_common.h \
  ipsec_tunnel_common.h

EXPORT_HEADERS_DIR=freeswan/klips/net/ipsec
EXPORT_HEADERS+=ipsec_encap.h ipsec_tunnel.h radij.h ipsec_radij.h

MOD_CFLAGS += -Ilibfreeswan

ifeq ($(CONFIG_IPSEC_NAT_TRAVERSAL),y)
  MOD_CFLAGS+=-DNAT_TRAVERSAL
endif

ifeq ($(CONFIG_IPSEC_USE_IXP4XX_CRYPTO),y)
  O_OBJS_$(MOD_TARGET) += ipsec_glue.o ipsec_glue_mbuf.o ipsec_glue_desc.o \
	ipsec_rcv_hwaccel.o
  OX_OBJS_$(MOD_TARGET) += ipsec_hwaccel.o ipsec_tunnel_hwaccel.o
  IXP425_SW := $(RGSRC)/vendor/intel/ixp425/csr
  MOD_CFLAGS += -I. -I$(IXP425_SW)/include -I$(IXP425_SW)/linux
  # Added for the possibility to remove HW Acceleration
  CD_EXPORTED_FILES += ipsec_hwaccel.h
else
  O_OBJS_$(MOD_TARGET) += ipsec_rcv.o
  OX_OBJS_$(MOD_TARGET) += ipsec_tunnel.o
endif

ifeq ($(CONFIG_IPSEC_DEBUG),y)
  MOD_CFLAGS += -g
  O_OBJS_$(MOD_TARGET) += ipsec_reject_debug.o
endif

MOD_CFLAGS += -Wall 
# MOD_CFLAGS += -Wconversion 
# MOD_CFLAGS += -Wmissing-prototypes 
MOD_CFLAGS += -Wpointer-arith 
# MOD_CFLAGS += -Wcast-qual 
# MOD_CFLAGS += -Wmissing-declarations 
MOD_CFLAGS += -Wstrict-prototypes
# MOD_CFLAGS += -pedantic
# MOD_CFLAGS += -O3
# MOD_CFLAGS += -W
# MOD_CFLAGS += -Wwrite-strings 
MOD_CFLAGS += -Wbad-function-cast 

ifeq ($(CONFIG_IPSEC_AUTH_HMAC_MD5),y)
  O_OBJS_$(MOD_TARGET) += ipsec_md5c.o
endif

ifeq ($(CONFIG_IPSEC_AUTH_HMAC_SHA1),y)
  O_OBJS_$(MOD_TARGET) += ipsec_sha1.o
endif

ifeq ($(CONFIG_IPSEC_IPCOMP),y)
  O_OBJS_$(MOD_TARGET) += ipcomp.o 
  L_OBJS += zlib/zlib.o
endif

ifneq ($(CONFIG_IPSEC_ENC_1DES)$(CONFIG_IPSEC_ENC_3DES),)
  SUBDIRS += libdes
  MOD_CFLAGS += -Ilibdes
  L_OBJS += libdes/libdes.o
endif

L_OBJS += libfreeswan/libfreeswan.o

ifeq ($(CONFIG_IPSEC_DRIVER),y)
  MOD_2_STAT+=$(MOD_TARGET)
else
  RAMDISK_MODULES_FILES+=$(MOD_TARGET)
endif

ifeq ($(CONFIG_RG_IPSEC_ESP_ALG),y)
  SUBDIRS += alg
  OX_OBJS_$(MOD_TARGET) += ipsec_alg.o
  L_OBJS += alg/ipsec_alg_static.o
endif

include $(RGMK)
