#
# linux/arch/arm/boot/compressed/Makefile
#
# create a compressed vmlinuz image from the original vmlinux
#
# Note! SYSTEM, ZTEXTADDR, ZBSSADDR and ZRELADDR are now exported
# from arch/arm/boot/Makefile
#

INIT		 = init.o
HEAD		 = head.o
OBJS		 = misc.o
CFLAGS		 = $(CPPFLAGS) -O2 -DSTDC_HEADERS $(CFLAGS_BOOT) -fpic -Uarm
FONTC		 = $(TOPDIR)/drivers/video/font_acorn_8x8.c
ZLDFLAGS	 = -p -X -T vmlinux.lds --no-warn-mismatch
ILDFLAGS	 = -p -X -T init.lds --no-warn-mismatch --defsym kernel_addr=$(ZTEXTADDR)

#
# Architecture dependencies
#
ifeq ($(CONFIG_ARCH_ACORN),y)
OBJS		+= ll_char_wr.o font.o
CFLAGS		+= -DPARAMS_PHYS=$(PARAMS_PHYS)
endif

ifeq ($(CONFIG_ARCH_RISCSTATION),y)
OBJS		+= ll_char_wr.o font.o
CFLAGS		+= -DPARAMS_PHYS=$(PARAMS_PHYS)
endif

ifeq ($(CONFIG_ARCH_NETWINDER),y)
OBJS		+= head-netwinder.o
endif

ifeq ($(CONFIG_ARCH_SHARK),y)
OBJS		+= head-shark.o ofw-shark.o
endif

ifeq ($(CONFIG_ARCH_INTEGRATOR),y)
OBJS		+= head-integrator.o
endif

ifeq ($(CONFIG_ARCH_AT91RM9200),y)
OBJS		+= head-at91rm9200.o
endif

ifeq ($(CONFIG_ARCH_MX1ADS),y)
OBJS		+= head-mx1ads.o
endif

ifeq ($(CONFIG_ARCH_CAMELOT),y)
OBJS		+= head-epxa10db.o
endif

ifeq ($(CONFIG_ARCH_FTVPCI),y)
OBJS		+= head-ftvpci.o
endif

ifeq ($(CONFIG_ARCH_L7200),y)
OBJS		+= head-l7200.o
endif

ifeq ($(CONFIG_ARCH_CLPS7500),y)
HEAD		= head-clps7500.o
endif

ifeq ($(CONFIG_ARCH_P720T),y)
# Borrow this code from SA1100
OBJS		+= head-sa1100.o
endif

ifeq ($(CONFIG_ARCH_SA1100),y)
OBJS		+= head-sa1100.o
endif

ifeq ($(CONFIG_CPU_XSCALE),y)
OBJS		+= head-xscale.o
endif

ifeq ($(CONFIG_ARCH_SL2312),y)
OBJS		+= head-sl2312.o
endif

ifeq ($(CONFIG_ARCH_CX8620X),y)
AFLAGS		+= -D$(BOARD)
endif

#
# Just borrow from SA1* for now
#
ifeq ($(CONFIG_ARCH_IXP1200),y)
OBJS		+= head-sa1100.o
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
OBJS		+= big-endian.o
endif

ifeq ($(CONFIG_RG_BZIP2_COMPRESSED_KERNEL),y)
COMPRESS	= bzip2 -c9
INFLATE_FILES	= $(TOPDIR)/lib/bzlib.c
endif
ifeq ($(CONFIG_RG_GZIP_COMPRESSED_KERNEL),y)
COMPRESS	= gzip -c9
INFLATE_FILES	= $(TOPDIR)/lib/inflate.c
endif
ifeq ($(CONFIG_RG_LZMA_COMPRESSED_KERNEL),y)
COMPRESS	= $(BUILDDIR)/pkg/lzma/SRC/lzma e -so
INFLATE_FILES	= $(TOPDIR)/lib/lzma_decode/decode.c $(TOPDIR)/lib/lzma_decode/LzmaDecode.c
endif

SEDFLAGS	= s/TEXT_START/$(ZTEXTADDR)/;s/LOAD_ADDR/$(ZRELADDR)/;s/BSS_START/$(ZBSSADDR)/

LIBGCC		:= $(shell $(CC) $(CFLAGS) --print-libgcc-file-name)

all:		initImage

initImage:	$(BOOTSTRAP) $(INIT) init.lds kernel.o
		$(LD) $(ILDFLAGS) $(BOOTSTRAP) $(INIT) kernel.o -o $@ 

kernel.o:	vmlinux.o
		$(OBJCOPY) $(OBJCOPYFLAGS) vmlinux.o vmlinux
		$(LD) -r -s -o $@ -b binary vmlinux

vmlinux.o:	$(HEAD) $(OBJS) piggy.o vmlinux.lds
		$(LD) $(ZLDFLAGS) $(HEAD) $(OBJS) $(LIBGCC) -o vmlinux.o

$(HEAD): 	$(HEAD:.o=.S) \
			$(wildcard $(TOPDIR)/include/config/zboot/rom.h) \
			$(wildcard $(TOPDIR)/include/config/cpu/32.h) \
			$(wildcard $(TOPDIR)/include/config/cpu/26.h)
		$(CC) $(AFLAGS) -traditional -c $(HEAD:.o=.S)

piggy.o:	$(SYSTEM)
		$(OBJCOPY) $(OBJCOPYFLAGS) $(SYSTEM) piggy
		$(COMPRESS) piggy > piggy.gz
		$(LD) -r -s -o $@ -b binary piggy.gz
		rm -f piggy piggy.gz

font.o:		$(FONTC)
		$(CC) $(CFLAGS) -Dstatic= -c -o $@ $(FONTC)

vmlinux.lds:	vmlinux.lds.in Makefile $(TOPDIR)/arch/$(ARCH)/boot/Makefile $(TOPDIR)/.config
		@sed "$(SEDFLAGS)" < vmlinux.lds.in > $@

clean:;		rm -f vmlinux vmlinux.o kernel.o core piggy* vmlinux.lds $(OBJS) $(HEAD) $(INIT) initImage

.PHONY:	clean

misc.o: misc.c $(TOPDIR)/include/asm/arch/uncompress.h $(INFLATE_FILES)

%.o: %.S
	$(CC) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) -c -o $@ $<
