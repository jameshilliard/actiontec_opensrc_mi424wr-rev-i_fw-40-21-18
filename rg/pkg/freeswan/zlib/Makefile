# (kernel) Makefile for IPCOMP zlib deflate code
# Copyright (C) 1998, 1999, 2000, 2001  Richard Guy Briggs.
# Copyright (C) 2000  Svenning Soerensen
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

RGSRC=../../../../../..
include $(RGSRC)/envir.mak

MOD_O_TARGET = zlib.o
O_OBJS = adler32.o deflate.o infblock.o infcodes.o inffast.o inflate.o \
	 inftrees.o infutil.o trees.o zutil.o

ifeq ($(CONFIG_IPSEC_DEBUG),y)
  MOD_CFLAGS += -g
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
MOD_CFLAGS += -DIPCOMP_PREFIX -DLOCAL_INFLATE_COPYRIGHT

ifdef CONFIG_M586
  O_OBJS += match586.o
  MOD_CFLAGS += -DASMV
endif

ifdef CONFIG_M586TSC
  O_OBJS += match586.o
  MOD_CFLAGS += -DASMV
endif

ifdef CONFIG_M686
  O_OBJS += match686.o
  MOD_CFLAGS += -DASMV
endif

include $(RGMK)
