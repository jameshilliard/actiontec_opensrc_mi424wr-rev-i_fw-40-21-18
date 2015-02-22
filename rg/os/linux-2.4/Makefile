RGSRC=../..
include $(RGSRC)/envir.mak
VERSION = 2
PATCHLEVEL = 4
SUBLEVEL = 21
EXTRAVERSION = openrg-rmk1

KERNELRELEASE=$(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

# SUBARCH tells the usermode build what the underlying arch is.  That is set
# first, and if a usermode build is happening, the "ARCH=um" on the command
# line overrides the setting of ARCH below.  If a native build is happening,
# then ARCH is assigned, getting whatever value it gets normally, and 
# SUBARCH is subsequently ignored.

SUBARCH:=$(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/)

KERNELPATH=kernel-$(shell echo $(KERNELRELEASE) | sed -e "s/-//g")

CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	  else if [ -x /bin/bash ]; then echo /bin/bash; \
	  else echo sh; fi ; fi)
TOPDIR	:= $(shell /bin/pwd)

HPATH   	= $(TOPDIR)/include
FINDHPATH	= $(HPATH)/asm $(HPATH)/linux $(HPATH)/scsi $(HPATH)/net $(HPATH)/math-emu

HOSTCFLAGS	= -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer

#
# Include the make variables (CC, etc...)
#

CPP		= $(CC) -E
MAKEFILES	= $(TOPDIR)/.config
GENKSYMS	= /sbin/genksyms
DEPMOD		= /sbin/depmod
MODFLAGS	= -DMODULE
CFLAGS_KERNEL	=
PERL		= perl

export	VERSION PATCHLEVEL SUBLEVEL EXTRAVERSION KERNELRELEASE ARCH \
	CONFIG_SHELL TOPDIR HPATH HOSTCC HOSTCFLAGS CROSS_COMPILE AS LD CC \
	CPP AR NM STRIP OBJCOPY OBJDUMP MAKE MAKEFILES GENKSYMS MODFLAGS PERL

all:	do-it-all

#
# Make "config" the default target if there is no configuration file or
# "depend" the target if there is no top-level dependency information.
#

ifeq (.config,$(wildcard .config))
include .config
do-it-all:	vmlinux
else
CONFIGURATION = config
do-it-all:	config
endif

#
# INSTALL_PATH specifies where to place the updated kernel and system map
# images.  Uncomment if you want to place them anywhere other than root.
#

#export	INSTALL_PATH=/boot

#
# INSTALL_MOD_PATH specifies a prefix to MODLIB for module directory
# relocations required by build roots.  This is not defined in the
# makefile but the arguement can be passed to make if needed.
#

MODLIB	:= $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)
export MODLIB

#
# standard CFLAGS
#

CPPFLAGS := -D__KERNEL__ -I$(HPATH) -D__TARGET__

CFLAGS := $(CPPFLAGS) -Wall -Wstrict-prototypes -Wno-trigraphs -O2 \
	  -fno-strict-aliasing -fno-common
ifeq ($(CONFIG_RG_KGDB),y)
  # Add debug symbols only to kernel proper and not modules
  ifneq ($(MAKECMDGOALS),modules)
    CFLAGS += -g
  endif
else
  CFLAGS += -fomit-frame-pointer
endif

AFLAGS := -D__ASSEMBLY__ $(CPPFLAGS)

ifeq ($(ARCH),mips)
# egcs-1.0.2 compiler for MIPS has a problem for which this is a work-around
CFLAGS += $(shell if $(CC) -mno-split-addresses -S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo "-mno-split-addresses"; fi)
endif

#
# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, CURRENT, /dev/xxxx or empty, in which case
# the default of FLOPPY is used by 'build'.
# This is i386 specific.
#

export ROOT_DEV = CURRENT

#
# If you want to preset the SVGA mode, uncomment the next line and
# set SVGA_MODE to whatever number you want.
# Set it to -DSVGA_MODE=NORMAL_VGA if you just want the EGA/VGA mode.
# The number is the same as you would ordinarily press at bootup.
# This is i386 specific.
#

export SVGA_MODE = -DSVGA_MODE=NORMAL_VGA

#
# If you want the RAM disk device, define this to be the size in blocks.
# This is i386 specific.
#

export RAMDISK = -DRAMDISK=512

CORE_FILES	=kernel/kernel.o mm/mm.o fs/fs.o ipc/ipc.o
NETWORKS	=net/network.o

LIBS		=$(TOPDIR)/lib/lib.a
SUBDIRS		=kernel drivers mm fs net ipc lib

ifeq ($(CONFIG_CRYPTO),y)
  SUBDIRS += crypto
  CORE_FILES += crypto/crypto.o
endif

ifeq ($(CONFIG_ZLIB_INFLATE),y)
  CORE_FILES += $(TOPDIR)/lib/zlib_inflate/zlib_inflate.o
endif

ifeq ($(CONFIG_ZLIB_DEFLATE),y)
  CORE_FILES += $(TOPDIR)/lib/zlib_deflate/zlib_deflate.o
endif

DRIVERS-n :=
DRIVERS-y :=
DRIVERS-m :=
DRIVERS-  :=

DRIVERS-$(CONFIG_ACPI) += drivers/acpi/acpi.o
DRIVERS-$(CONFIG_PARPORT) += drivers/parport/driver.o
DRIVERS-$(CONFIG_I2C) += drivers/i2c/i2c.o
DRIVERS-$(CONFIG_L3) += drivers/l3/l3.o
DRIVERS-y += drivers/serial/serial.o \
	drivers/char/char.o \
	drivers/block/block.o \
	drivers/misc/misc.o \
	drivers/net/net.o
DRIVERS-$(CONFIG_AGP) += drivers/char/agp/agp.o
DRIVERS-$(CONFIG_DRM_NEW) += drivers/char/drm/drm.o
DRIVERS-$(CONFIG_DRM_OLD) += drivers/char/drm-4.0/drm.o
DRIVERS-$(CONFIG_NUBUS) += drivers/nubus/nubus.a
DRIVERS-$(CONFIG_NET_FC) += drivers/net/fc/fc.o
DRIVERS-$(CONFIG_DEV_APPLETALK) += drivers/net/appletalk/appletalk.o
DRIVERS-$(CONFIG_TR) += drivers/net/tokenring/tr.o
DRIVERS-$(CONFIG_WAN) += drivers/net/wan/wan.o
DRIVERS-$(CONFIG_ARCNET) += drivers/net/arcnet/arcnetdrv.o
DRIVERS-$(CONFIG_ATM) += drivers/atm/atm.o
DRIVERS-$(CONFIG_IDE) += drivers/ide/idedriver.o
DRIVERS-$(CONFIG_FC4) += drivers/fc4/fc4.a
DRIVERS-$(CONFIG_SCSI) += drivers/scsi/scsidrv.o
DRIVERS-$(CONFIG_FUSION_BOOT) += drivers/message/fusion/fusion.o
DRIVERS-$(CONFIG_IEEE1394) += drivers/ieee1394/ieee1394drv.o

ifneq ($(CONFIG_CD_NO_IDESCSI)$(CONFIG_BLK_DEV_IDECD)$(CONFIG_BLK_DEV_SR)$(CONFIG_PARIDE_PCD),)
DRIVERS-y += drivers/cdrom/driver.o
endif

DRIVERS-$(CONFIG_SSI) += drivers/ssi/ssi.o
DRIVERS-$(CONFIG_SOUND) += drivers/sound/sounddrivers.o
DRIVERS-$(CONFIG_PCI) += drivers/pci/driver.o
DRIVERS-$(CONFIG_MTD) += drivers/mtd/mtdlink.o
DRIVERS-$(CONFIG_PCMCIA) += drivers/pcmcia/pcmcia.o
DRIVERS-$(CONFIG_NET_PCMCIA) += drivers/net/pcmcia/pcmcia_net.o
DRIVERS-$(CONFIG_NET_WIRELESS) += drivers/net/wireless/wireless_net.o
DRIVERS-$(CONFIG_PCMCIA_CHRDEV) += drivers/char/pcmcia/pcmcia_char.o
DRIVERS-$(CONFIG_DIO) += drivers/dio/dio.a
DRIVERS-$(CONFIG_SBUS) += drivers/sbus/sbus_all.o
DRIVERS-$(CONFIG_ZORRO) += drivers/zorro/driver.o
DRIVERS-$(CONFIG_FC4) += drivers/fc4/fc4.a
DRIVERS-$(CONFIG_PPC32) += drivers/macintosh/macintosh.o
DRIVERS-$(CONFIG_MAC) += drivers/macintosh/macintosh.o
DRIVERS-$(CONFIG_ISAPNP) += drivers/pnp/pnp.o
DRIVERS-$(CONFIG_SGI_IP22) += drivers/sgi/sgi.a
DRIVERS-$(CONFIG_VT) += drivers/video/video.o
DRIVERS-$(CONFIG_PARIDE) += drivers/block/paride/paride.a
DRIVERS-$(CONFIG_HAMRADIO) += drivers/net/hamradio/hamradio.o
DRIVERS-$(CONFIG_TC) += drivers/tc/tc.a
DRIVERS-$(CONFIG_USB) += drivers/usb/usbdrv.o
DRIVERS-y +=drivers/media/media.o
DRIVERS-$(CONFIG_INPUT) += drivers/input/inputdrv.o
DRIVERS-$(CONFIG_HIL) += drivers/hil/hil.o
DRIVERS-$(CONFIG_I2O) += drivers/message/i2o/i2o.o
DRIVERS-$(CONFIG_IRDA) += drivers/net/irda/irda.o
DRIVERS-$(CONFIG_PHONE) += drivers/telephony/telephony.o
DRIVERS-$(CONFIG_MD) += drivers/md/mddev.o
DRIVERS-$(CONFIG_GSC) += drivers/gsc/gscbus.o
DRIVERS-$(CONFIG_BLUEZ) += drivers/bluetooth/bluetooth.o
DRIVERS-$(CONFIG_HOTPLUG_PCI) += drivers/hotplug/vmlinux-obj.o
DRIVERS-$(CONFIG_ISDN_BOOL) += drivers/isdn/vmlinux-obj.o
DRIVERS-$(CONFIG_PLD) += drivers/pld/pld.o
DRIVERS-$(CONFIG_ARCH_AT91RM9200) += drivers/at91/at91drv.o

DRIVERS := $(DRIVERS-y)


# files removed with 'make clean'
CLEAN_FILES = \
	kernel/ksyms.lst include/linux/compile.h \
	vmlinux System.map \
	.tmp* \
	size.sec \
	drivers/char/consolemap_deftbl.c drivers/video/promcon_tbl.c \
	drivers/char/conmakehash \
	drivers/char/drm/*-mod.c \
	drivers/pci/devlist.h drivers/pci/classlist.h drivers/pci/gen-devlist \
	drivers/zorro/devlist.h drivers/zorro/gen-devlist \
	drivers/sound/bin2hex drivers/sound/hex2hex \
	drivers/atm/fore200e_mkfirm drivers/atm/{pca,sba}*{.bin,.bin1,.bin2} \
	drivers/scsi/aic7xxx/aicasm/aicasm \
	drivers/scsi/aic7xxx/aicasm/aicasm_gram.c \
	drivers/scsi/aic7xxx/aicasm/aicasm_gram.h \
	drivers/scsi/aic7xxx/aicasm/aicasm_macro_gram.c \
	drivers/scsi/aic7xxx/aicasm/aicasm_macro_gram.h \
	drivers/scsi/aic7xxx/aicasm/aicasm_macro_scan.c \
	drivers/scsi/aic7xxx/aicasm/aicasm_scan.c \
	drivers/scsi/aic7xxx/aicasm/aicdb.h \
	drivers/scsi/aic7xxx/aicasm/y.tab.h \
	drivers/scsi/53c700_d.h \
	drivers/ieee1394/oui.c \
	net/khttpd/make_times_h \
	net/khttpd/times.h \
	init/main.o init/version.o init/do_mounts.o \
	submenu* \
	export_symbols.* \
	defined_symbols.* \
	needed_symbols.* \
	vmlinux.link.map

ifdef CONFIG_RG_UML
CLEAN_FILES+=openrg.uml
endif

# directories removed with 'make clean'
CLEAN_DIRS = \
	modules

CLEAN_TARGETS=archclean
CLEAN=$(CLEAN_FILES)

# files removed with 'make mrproper'
MRPROPER_FILES = \
	include/linux/version.h \
	drivers/net/hamradio/soundmodem/sm_tbl_{afsk1200,afsk2666,fsk9600}.h \
	drivers/net/hamradio/soundmodem/sm_tbl_{hapn4800,psk4800}.h \
	drivers/net/hamradio/soundmodem/sm_tbl_{afsk2400_7,afsk2400_8}.h \
	drivers/net/hamradio/soundmodem/gentbl \
	drivers/sound/*_boot.h drivers/sound/.*.boot \
	drivers/sound/msndinit.c \
	drivers/sound/msndperm.c \
	drivers/sound/pndsperm.c \
	drivers/sound/pndspini.c \
	drivers/atm/fore200e_*_fw.c drivers/atm/.fore200e_*.fw \
	.version .config* config.in config.old \
	scripts/tkparse scripts/kconfig.tk scripts/kconfig.tmp \
	scripts/lxdialog/*.o scripts/lxdialog/lxdialog \
	.menuconfig.log \
	include/asm \
	.hdepend scripts/mkdep scripts/split-include scripts/docproc \
	kernel.spec

# directories removed with 'make mrproper'
MRPROPER_DIRS = \
	include/config \
	$(TOPDIR)/include/linux/modules


include arch/$(ARCH)/Makefile

# Extra cflags for kbuild 2.4.  The default is to forbid includes by kernel code
# from user space headers.  Some UML code requires user space headers, in the
# UML Makefiles add 'kbuild_2_4_nostdinc :=' before include Rules.make.  No
# other kernel code should include user space headers, if you need
# 'kbuild_2_4_nostdinc :=' or -I/usr/include for kernel code and you are not UML
# then your code is broken!  KAO.

kbuild_2_4_nostdinc	:= -nostdinc -iwithprefix include
export kbuild_2_4_nostdinc

export	CPPFLAGS CFLAGS CFLAGS_KERNEL AFLAGS AFLAGS_KERNEL

export	NETWORKS DRIVERS LIBS HEAD LDFLAGS LINKFLAGS MAKEBOOT ASFLAGS

boot: vmlinux
	@$(MAKE) CFLAGS="$(CFLAGS) $(CFLAGS_KERNEL)" -C arch/$(ARCH)/boot

ifdef CONFIG_RG_KERNEL_NEEDED_SYMBOLS
EXPORT_NEEDED_SYMBOLS=export_symbols.o
DEFINED_SYMBOLS=defined_symbols.lst
KERNEL_OBJS=$(HEAD) init/main.o init/version.o init/do_mounts.o $(CORE_FILES) $(DRIVERS) $(NETWORKS) $(LIBS) $(wildcard modules_static/*.o)

export_symbols.c: $(NEEDED_SYMBOLS) $(KERNEL_OBJS)
	echo $(KERNEL_OBJS) | xargs -n 1 $(NM) --defined-only | cut -b 12-120 | sort -u > $(DEFINED_SYMBOLS)
	cp -f export.c $@
	chmod u+w $@
	# export only kernel symbols, without symbols from modules
	grep -w --file $(NEEDED_SYMBOLS) $(DEFINED_SYMBOLS) | xargs -n 1 -ixxx echo -e extern int xxx\; '\n'EXPORT_SYMBOL\(xxx\)\; >> $@
endif

vmlinux: include/linux/version.h $(CONFIGURATION) init/main.o init/version.o init/do_mounts.o linuxsubdirs $(EXPORT_NEEDED_SYMBOLS)
	$(LD) $(LINKFLAGS) --no-warn-mismatch $(HEAD) init/main.o init/version.o init/do_mounts.o \
		--start-group \
		$(CORE_FILES) \
		$(DRIVERS) \
		$(NETWORKS) \
		$(LIBS) \
		$(sort $(wildcard modules_static/*.o)) \
		$(EXPORT_NEEDED_SYMBOLS) \
		--end-group -Map vmlinux.link.map\
		-o vmlinux
	$(NM) vmlinux | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aUw] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | sort > System.map
	$(RG_LN) $(PWD_BUILD)/$@ $(DEBUG_PATH)/$@

.config: $(CONFIG_FILE)
	$(RG_LN) $< $@

symlinks:
	rm -f include/asm
	( cd include ; ln -sf asm-$(ARCH) asm)
	@if [ ! -d include/linux/modules ]; then \
		mkdir include/linux/modules; \
	fi

archconfig:: symlinks .config include/linux/version.h include/config/MARKER

ifeq (arm,$(ARCH))
archconfig:: makeheaders
endif

oldconfig: symlinks
	$(CONFIG_SHELL) scripts/Configure -d arch/$(ARCH)/config.in

xconfig: symlinks
	$(MAKE) -C scripts kconfig.tk
	wish -f scripts/kconfig.tk

menuconfig: include/linux/version.h symlinks
	$(MAKE) -C scripts/lxdialog all
	$(CONFIG_SHELL) scripts/Menuconfig arch/$(ARCH)/config.in

config: symlinks
	$(CONFIG_SHELL) scripts/Configure arch/$(ARCH)/config.in

include/config/MARKER: scripts/split-include $(AUTOCONF_H)
	scripts/split-include $(AUTOCONF_H) include/config
	@ touch include/config/MARKER

linuxsubdirs: $(patsubst %, _dir_%, $(SUBDIRS))

$(patsubst %, _dir_%, $(SUBDIRS)) : dummy include/linux/version.h include/config/MARKER
	$(MAKE) CFLAGS="$(CFLAGS) $(CFLAGS_KERNEL)" -C $(patsubst _dir_%, %, $@)

$(TOPDIR)/include/linux/version.h: include/linux/version.h
$(TOPDIR)/include/linux/compile.h: include/linux/compile.h

newversion:
	. scripts/mkversion > .tmpversion
	@mv -f .tmpversion .version

uts_len		:= 64
uts_truncate	:= sed -e 's/\(.\{1,$(uts_len)\}\).*/\1/'

include/linux/compile.h: $(CONFIGURATION) include/linux/version.h newversion
	@echo -n \#`cat .version` > .ver1
	@if [ -n "$(CONFIG_SMP)" ] ; then echo -n " SMP" >> .ver1; fi
	@if [ -f .name ]; then  echo -n \-`cat .name` >> .ver1; fi
	@LANG=C echo ' '`date` >> .ver1
	@echo \#define UTS_VERSION \"`cat .ver1 | $(uts_truncate)`\" > .ver
	@LANG=C echo \#define LINUX_COMPILE_TIME \"`date +%T`\" >> .ver
	@echo \#define LINUX_COMPILE_BY \"`whoami`\" >> .ver
	@echo \#define LINUX_COMPILE_HOST \"`hostname | $(uts_truncate)`\" >> .ver
	@([ -x /bin/dnsdomainname ] && /bin/dnsdomainname > .ver1) || \
	 ([ -x /bin/domainname ] && /bin/domainname > .ver1) || \
	 echo > .ver1
	@echo \#define LINUX_COMPILE_DOMAIN \"`cat .ver1 | $(uts_truncate)`\" >> .ver
	@echo \#define LINUX_COMPILER \"`$(CC) $(CFLAGS) -v 2>&1 | tail -n 1`\" >> .ver
	@mv -f .ver $@
	@rm -f .ver1

include/linux/version.h: ./Makefile
	@expr length "$(KERNELRELEASE)" \<= $(uts_len) > /dev/null || \
	  (echo KERNELRELEASE \"$(KERNELRELEASE)\" exceeds $(uts_len) characters >&2; false)
	@echo \#define UTS_RELEASE \"$(KERNELRELEASE)\" > .ver
	@echo \#define LINUX_VERSION_CODE `expr $(VERSION) \\* 65536 + $(PATCHLEVEL) \\* 256 + $(SUBLEVEL)` >> .ver
	@echo '#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))' >>.ver
	@mv -f .ver $@

comma	:= ,

init/version.o: init/version.c include/linux/compile.h include/config/MARKER
	$(CC) $(CFLAGS) $(CFLAGS_KERNEL) -DUTS_MACHINE='"$(ARCH)"' -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) -c -o init/version.o init/version.c

init/main.o: init/main.c include/config/MARKER
	$(CC) $(CFLAGS) $(CFLAGS_KERNEL) -I$(RGSRC) $(PROFILING) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) -c -o $@ $<

init/do_mounts.o: init/do_mounts.c include/config/MARKER
	$(CC) $(CFLAGS) $(CFLAGS_KERNEL) $(PROFILING) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) -c -o $@ $<

fs lib mm ipc kernel drivers net: dummy
	$(MAKE) CFLAGS="$(CFLAGS) $(CFLAGS_KERNEL)" $(subst $@, _dir_$@, $@)

TAGS: dummy
	{ find include/asm-${ARCH} -name '*.h' -print ; \
	find include -type d \( -name "asm-*" -o -name config \) -prune -o -name '*.h' -print ; \
	find $(SUBDIRS) init arch/${ARCH} -name '*.[chS]' ; } | grep -v SCCS | grep -v '\.svn' | etags -

# Exuberant ctags works better with -I
tags: dummy
	CTAGSF=`ctags --version | grep -i exuberant >/dev/null && echo "-I __initdata,__exitdata,EXPORT_SYMBOL,EXPORT_SYMBOL_NOVERS"`; \
	ctags $$CTAGSF `find include/asm-$(ARCH) -name '*.h'` && \
	find include -type d \( -name "asm-*" -o -name config \) -prune -o -name '*.h' -print | xargs ctags $$CTAGSF -a && \
	find $(SUBDIRS) init -name '*.[ch]' | xargs ctags $$CTAGSF -a

ifdef CONFIG_MODULES
ifdef CONFIG_MODVERSIONS
MODFLAGS += -DMODVERSIONS -include $(HPATH)/linux/modversions.h
endif

.PHONY: modules
modules: $(patsubst %, _mod_%, $(SUBDIRS))

.PHONY: $(patsubst %, _mod_%, $(SUBDIRS))
$(patsubst %, _mod_%, $(SUBDIRS)) : include/linux/version.h include/config/MARKER
	$(MAKE) -C $(patsubst _mod_%, %, $@) CFLAGS="$(CFLAGS) $(MODFLAGS)" MAKING_MODULES=1 modules

.PHONY: modules_install
modules_install: _modinst_ $(patsubst %, _modinst_%, $(SUBDIRS)) _modinst_post

.PHONY: _modinst_
_modinst_:
	@rm -rf $(MODLIB)/kernel
	@rm -f $(MODLIB)/build
	@mkdir -p $(MODLIB)/kernel
	@ln -s $(TOPDIR) $(MODLIB)/build

# If System.map exists, run depmod.  This deliberately does not have a
# dependency on System.map since that would run the dependency tree on
# vmlinux.  This depmod is only for convenience to give the initial
# boot a modules.dep even before / is mounted read-write.  However the
# boot script depmod is the master version.
ifeq "$(strip $(INSTALL_MOD_PATH))" ""
depmod_opts	:=
else
depmod_opts	:= -b $(INSTALL_MOD_PATH) -r
endif
.PHONY: _modinst_post
_modinst_post: _modinst_post_pcmcia
	if [ -r System.map ]; then $(DEPMOD) -ae -F System.map $(depmod_opts) $(KERNELRELEASE); fi

# Backwards compatibilty symlinks for people still using old versions
# of pcmcia-cs with hard coded pathnames on insmod.  Remove
# _modinst_post_pcmcia for kernel 2.4.1.
.PHONY: _modinst_post_pcmcia
_modinst_post_pcmcia:
	cd $(MODLIB); \
	mkdir -p pcmcia; \
	find kernel -path '*/pcmcia/*' -name '*.o' | xargs -i -r ln -sf ../{} pcmcia

.PHONY: $(patsubst %, _modinst_%, $(SUBDIRS))
$(patsubst %, _modinst_%, $(SUBDIRS)) :
	$(MAKE) -C $(patsubst _modinst_%, %, $@) modules_install

# modules disabled....

else
modules modules_install: dummy
	@echo
	@echo "The present kernel configuration has modules disabled."
	@echo "Type 'make config' and enable loadable module support."
	@echo "Then build a kernel with module support enabled."
	@echo
	@exit 1
endif

mrproper: clean archmrproper
	find . -follow \( -size 0 -o -name .depend \) -type f -print | \
		xargs rm -f
	rm -f $(MRPROPER_FILES)
	rm -rf $(MRPROPER_DIRS)
	$(MAKE) -C Documentation/DocBook mrproper

backup: mrproper
	cd .. && tar cf - linux/ | gzip -9 > backup.gz
	sync

sgmldocs: 
	chmod 755 $(TOPDIR)/scripts/docgen
	chmod 755 $(TOPDIR)/scripts/gen-all-syms
	chmod 755 $(TOPDIR)/scripts/kernel-doc
	$(MAKE) -C $(TOPDIR)/Documentation/DocBook books

psdocs: sgmldocs
	$(MAKE) -C Documentation/DocBook ps

pdfdocs: sgmldocs
	$(MAKE) -C Documentation/DocBook pdf

htmldocs: sgmldocs
	$(MAKE) -C Documentation/DocBook html

mandocs:
	chmod 755 $(TOPDIR)/scripts/kernel-doc
	chmod 755 $(TOPDIR)/scripts/split-man
	$(MAKE) -C Documentation/DocBook man

sums:
	find . -type f -print | sort | xargs sum > .SUMS

ifdef CONFIG_MODVERSIONS
MODVERFILE := $(TOPDIR)/include/linux/modversions.h
else
MODVERFILE :=
endif
export	MODVERFILE

checkconfig:
	find * -name '*.[hcS]' -type f -print | sort | xargs $(PERL) -w scripts/checkconfig.pl

checkhelp:
	find * -name [cC]onfig.in -print | sort | xargs $(PERL) -w scripts/checkhelp.pl

checkincludes:
	find * -name '*.[hcS]' -type f -print | sort | xargs $(PERL) -w scripts/checkincludes.pl

ifdef CONFIGURATION
..$(CONFIGURATION):
	@echo
	@echo "You have a bad or nonexistent" .$(CONFIGURATION) ": running 'make" $(CONFIGURATION)"'"
	@echo
	$(MAKE) $(CONFIGURATION)
	@echo
	@echo "Successful. Try re-making (ignore the error that follows)"
	@echo
	exit 1

#dummy: ..$(CONFIGURATION)
dummy:

else

dummy:

endif

ARCHCONFIG_SUBDIRS=_DUMMY_SUBDIR_
include Rules.make

#
# This generates dependencies for the .h files.
#

scripts/mkdep: scripts/mkdep.c
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/mkdep scripts/mkdep.c

scripts/split-include: scripts/split-include.c
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/split-include scripts/split-include.c

#
# RPM target
#
#	If you do a make spec before packing the tarball you can rpm -ta it
#
spec:
	. scripts/mkspec >kernel.spec

#
#	Build a tar ball, generate an rpm from it and pack the result
#	There arw two bits of magic here
#	1) The use of /. to avoid tar packing just the symlink
#	2) Removing the .dep files as they have source paths in them that
#	   will become invalid
#
rpm:	clean spec
	find . \( -size 0 -o -name .depend -o -name .hdepend \) -type f -print | xargs rm -f
	set -e; \
	cd $(TOPDIR)/.. ; \
	ln -sf $(TOPDIR) $(KERNELPATH) ; \
	tar -cvz --exclude CVS -f $(KERNELPATH).tar.gz $(KERNELPATH)/. ; \
	rm $(KERNELPATH) ; \
	cd $(TOPDIR) ; \
	. scripts/mkversion > .version ; \
	rpm -ta $(TOPDIR)/../$(KERNELPATH).tar.gz ; \
	rm $(TOPDIR)/../$(KERNELPATH).tar.gz
