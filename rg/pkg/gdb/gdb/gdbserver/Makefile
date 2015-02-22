# Generated automatically from Makefile.in by configure.
# Copyright 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
# 1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

# This file is part of GDB.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

RGSRC=../../../../
include $(RGSRC)/envir.mak

INTERNAL_HEADERS=../../include/gdb/signals.h ../regformats/regdef.h \
  ../../gdb/gdb_proc_service.h ../../gdb/gregset.h server.h config.h \
  regcache.h target.h mem-break.h terminal.h linux-low.h

# Perhaps should come from parent Makefile
VERSION=gdbserver-4.12.3

ifneq ($(CONFIG_RG_UML)$(CONFIG_M386),)
  GENERIC_OBJS+=linux-i386-low.o i387-fp.o
  REGFILE=reg-i386-linux
endif
ifdef CONFIG_PPC
  GENERIC_OBJS+=linux-ppc-low.o
  REGFILE=reg-ppc
endif
ifdef CONFIG_ARM
  GENERIC_OBJS+=linux-arm-low.o
  REGFILE=reg-arm
endif
ifdef CONFIG_MIPS
#  GENERIC_OBJS+=linux-m68k-low.o linux-mips-low.o
  GENERIC_OBJS+=linux-mips-low.o
  ifdef CONFIG_64BIT
    REGFILE=reg-mips64
  else
    REGFILE=reg-mips
  endif
endif

TARGET=gdbserver
RAMDISK_BIN_FILES+=$(TARGET)
# To add gdbreplay, uncomment the following
# TARGET+=gdbreplay
# O_OBJS_gdbreplay=gdbreplay.o

O_OBJS_gdbserver=inferiors.o regcache.o remote-utils.o server.o signals.o \
  target.o utils.o mem-break.o linux-low.o proc-service.o \
  $(REGFILE).o $(GENERIC_OBJS)

ifdef CONFIG_RG_THREADS
  CFLAGS_linux-low.o+=-DUSE_THREAD_DB
  O_OBJS_gdbserver+=thread-db.o
  LDLIBS_gdbserver:=-lthread_db $(LDLIBS_gdbserver)
endif

#An explicit include is required when a file with the name "regdef.h" exists in
#glibc includes (for example, in mips64-octeon-linux-gnu toolchain), but it
#isn't the regdef.h we actually want.
CFLAGS+=-I. -I../regformats -I../../include -include "../regformats/regdef.h"
CFLAGS+=-DGDBSERVER

CLEAN += signals.c $(REGFILE).c

regdat_sh=../regformats/regdat.sh
reg_dat=../regformats/$(REGFILE).dat

include $(RGMK)

signals.c: ../signals/signals.c
	$(RG_LN) $< $@

version.c:
	echo 'char *version = "$(VERSION)";' >version.c


$(REGFILE).c: $(regdat_sh) $(reg_dat)
	sh $^ $@

