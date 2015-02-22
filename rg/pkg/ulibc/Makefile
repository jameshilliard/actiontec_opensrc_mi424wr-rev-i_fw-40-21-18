# Makefile for uClibc
#
# Copyright (C) 2000-2005 Erik Andersen <andersen@uclibc.org>
#
# Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
#
RGSRC=../../
include $(RGSRC)/envir.mak

top_srcdir=./
top_builddir=./
#include $(top_builddir)Rules.mak
#all: libs
include Makefile.in

archconfig:

ramdisk: lib_ramdisk

lib_ramdisk:
	$(MAKE) -C lib ramdisk

finished:
	$(MKDIR) $(RG_LIB)
	$(RG_LN) $(BUILDDIR)/pkg/ulibc/lib/* $(RG_LIB)/
	$(SECHO)
	$(SECHO) Finally finished compiling ...
	$(SECHO)


include $(top_srcdir)Makerules
