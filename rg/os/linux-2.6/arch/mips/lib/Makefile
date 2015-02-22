#
# Makefile for MIPS-specific library files..
#

lib-y	+= csum_partial_copy.o promlib.o strlen_user.o strncpy_user.o \
	   strnlen_user.o uncached.o

MEMCPY=memcpy.o
ifdef CONFIG_MIPS_PATENTFREE
  MEMCPY = memcpy_NP.o
endif
ifdef CONFIG_CPU_CAVIUM_OCTEON
  MEMCPY = memcpy_octeon.o
  lib-y += iomap_cavium.o
else
  lib-y += iomap.o
endif

lib-y	+= $(MEMCPY)

EXTRA_AFLAGS := $(CFLAGS)
