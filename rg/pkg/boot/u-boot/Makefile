RGSRC=../../../
include $(RGSRC)/envir.mak

INTERNAL_HEADERS+=image.h
MKIMAGE=mkimage

LOCAL_TARGET=$(MKIMAGE)
O_OBJS_$(LOCAL_TARGET)=mkimage.o crc32.o

CD_EXPORTED_FILES+=u-boot.README u-boot.patch

VMLINUX=$(BUILDDIR)/os/linux/vmlinux
VMLINUX_BIN=vmlinux.bin
SYSTEM_MAP=$(BUILDDIR)/os/linux/System.map
GET_SYMBOL=$(shell grep -w $1 $(SYSTEM_MAP) | awk '{ printf "0x%s", $$1 }')

ifdef CONFIG_DANUBE
  LZMA=$(RGSRC)/vendor/infineon/danube/tools/lzma e -so
else
  LZMA=$(error LZMA is not defined for this arch)
endif
GZIP=gzip -f -9
BZIP2=bzip2

ifeq ($(ARCH),ppc)
  LOADADDR=0x00000000
  KERNEL_ENTRY=0x00000000
endif
ifeq ($(ARCH),mips)
ifdef CONFIG_LOADADDR_SYM_TEXT
  LOADADDR=$(call GET_SYMBOL, _text)
else
  LOADADDR=$(call GET_SYMBOL, _ftext)
endif
  KERNEL_ENTRY=$(call GET_SYMBOL, kernel_entry)
endif
ifeq ($(ARCH),arm)
  ifneq ($(CONFIG_FEROCEON),)
    include $(RGSRC)/vendor/marvell/feroceon/linux-2.6/arch/arm/mach-feroceon-kw2/Makefile.boot
    #LOADADDR=$(zreladdr-y)
    #KERNEL_ENTRY=$(zreladdr-y)
    LOADADDR=0x01000000
    KERNEL_ENTRY=0x01000000
  else
    LOADADDR=$(call GET_SYMBOL, _stext)
    KERNEL_ENTRY=$(call GET_SYMBOL, stext)
  endif
endif

ifndef CONFIG_BOOTLDR_UBOOT_COMP
  CONFIG_BOOTLDR_UBOOT_COMP=gzip
endif

ifeq ($(CONFIG_BOOTLDR_UBOOT_COMP),none)
  ZIMAGE=$(VMLINUX_BIN)
endif
ifeq ($(CONFIG_BOOTLDR_UBOOT_COMP),gzip)
  ZIMAGE=$(VMLINUX_BIN).gz
endif
ifeq ($(CONFIG_BOOTLDR_UBOOT_COMP),bzip2)
  ZIMAGE=$(VMLINUX_BIN).bz2
endif
ifeq ($(CONFIG_BOOTLDR_UBOOT_COMP),lzma)
  ZIMAGE=$(VMLINUX_BIN).lzma
endif

ifneq ($(CONFIG_ARCH_FEROCEON),)
  ZIMAGE=$(BUILDDIR)/os/linux-2.6/arch/arm/boot/zImage
endif

LOCAL_TARGET=$(MKIMAGE)
O_OBJS_$(MKIMAGE)=mkimage.o crc32.o

include $(RGMK)

ifdef IS_BUILDDIR
%.lzma: %
	$(LZMA) $^ > $@

%.gz: %
	$(GZIP) $^

%.bz2: %
	$(BZIP2) $^

$(VMLINUX_BIN): $(VMLINUX)
	$(OBJCOPY) -O binary $^ $@

$(OPENRG_IMG): $(MKIMAGE) $(ZIMAGE)
	$(MKIMAGE) -n 'OpenRG' -A $(ARCH) -O linux -T kernel -C \
	$(CONFIG_BOOTLDR_UBOOT_COMP) -a $(LOADADDR) -e $(KERNEL_ENTRY) \
	-d $(ZIMAGE) $@
endif
