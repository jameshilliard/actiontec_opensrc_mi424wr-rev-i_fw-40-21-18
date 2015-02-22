#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995, 1998, 2001, 2002 by Ralf Baechle
# Copyright (C) 2004  Maciej W. Rozycki
#

TOPDIR=../../..
RGSRC=$(TOPDIR)/../..
USE_STANDARD_AS_RULE := true
include $(RGSRC)/envir.mak

#
# Some DECstations need all possible sections of an ECOFF executable
#
ifdef CONFIG_MACH_DECSTATION
  E2EFLAGS = -a
else
  E2EFLAGS =
endif

#
# Drop some uninteresting sections in the kernel.
# This is only relevant for ELF kernels but doesn't hurt a.out
#
drop-sections	= .reginfo .mdebug .comment .note .pdr .options .MIPS.options
strip-flags	= $(addprefix --remove-section=,$(drop-sections))

VMLINUX = vmlinux

ifeq ($(CONFIG_RG_BZIP2_COMPRESSED_KERNEL),y)
  COMPRESS=bzip2 -c9
endif
ifeq ($(CONFIG_RG_GZIP_COMPRESSED_KERNEL),y)
  COMPRESS=gzip -c9
endif
ifeq ($(CONFIG_RG_LZMA_COMPRESSED_KERNEL),y)
  COMPRESS=$(BUILDDIR)/pkg/lzma/SRC/lzma e -so
endif
    
all: vmlinux.ecoff vmlinux.srec addinitrd

vmlinux.ecoff: $(obj)/elf2ecoff $(VMLINUX)
	$(obj)/elf2ecoff $(VMLINUX) vmlinux.ecoff $(E2EFLAGS)

$(obj)/elf2ecoff: $(obj)/elf2ecoff.c
	$(HOSTCC) -o $@ $^

vmlinux.bin: $(VMLINUX)
	$(OBJCOPY) -O binary $(strip-flags) $(VMLINUX) $(obj)/vmlinux.bin

vmlinux.srec: $(VMLINUX)
	$(OBJCOPY) -S -O srec $(strip-flags) $(VMLINUX) $(obj)/vmlinux.srec

$(obj)/addinitrd: $(obj)/addinitrd.c
	$(HOSTCC) -o $@ $^

archhelp:
	@echo	'* vmlinux.ecoff	- ECOFF boot image'
	@echo	'* vmlinux.srec		- SREC boot image'

vmlinuz.bin.gz: vmlinux.bin
	$(COMPRESS) $(obj)/$^ > $(obj)/vmlinuz.bin.gz

clean-files += addinitrd \
	       elf2ecoff \
	       vmlinux.bin \
	       vmlinux.ecoff \
	       vmlinux.srec
