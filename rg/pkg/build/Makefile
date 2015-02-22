RGSRC=../../
include $(RGSRC)/envir.mak
ifdef IS_BUILDDIR

CLEAN=rootfs.img $(STRIP_IFDEF) $(STRIP_IFDEF_CONFIG) \
  efss_data.o $(CRAMFS_FILES_FILE) $(CRAMFS_FOOTPRINT_FILE) \
  $(COMPRESSED_DISK) $(MODFS_DISK) $(COMPRESSED_INIT_DISK)
ifdef CONFIG_RG_UML
CLEAN+=$(RGSRC)/os/linux/fs.img
endif

export REMOVE_IFDEFS=y

# OTHER_TASKS and not OTHER_TARGETS because we don't want it to
# be deleted in every clean
ifndef CONFIG_WINDOWS_HOST
OTHER_TASKS=exec_as_root

LOCAL_TARGET=checksum
O_OBJS_checksum=checksum.o
endif

CD_EXPORTED_FILES+=exec_as_root.c ./make_ramdisk.sh export_src.c
INTERNAL_HEADERS=license_text.h

EXPORT_HEADERS_DIR=build
EXPORT_HEADERS+=features.c__c/features.c
ifdef CONFIG_RG_CONFIG_STRINGS
  EXPORT_HEADERS+=config_strings.h
endif
AUTOGEN_SRC+=features.c
CD_EXPORTED_FILES+=create_config.c \
  create_config.h device_config.c dist_config.c feature_config.c \
  hw_config.c config_opt.c major_features.c config_opt.h \
  Makefile.create_config config_host.c config_target_os.c detect_host.sh
  
ifeq ($(CONFIG_RG_EFSS),y)
  LOCAL_TARGET+=mkefs
  LDLIBS_mkefs:=-llocal_z $(LDLIBS_mkefs)
  O_OBJS_mkefs=mkefs.o
  O_TARGET+=efss_main.o
  O_OBJS_efss_main.o=efss.o
  CLEAN+=image.h info.h libramdisk.a
  CD_EXPORTED_FILES+=efss_data.c efss_data.h efss.h
endif

LOCAL_TARGET+=write_int
O_OBJS_write_int=write_int.o

# Mips want a special structure prepended to their ramdisk...
ifdef CONFIG_RG_OS_LINUX_22
  ifdef CONFIG_MIPS
    LOCAL_TARGET+=mips_initrd
    O_OBJS_mips_initrd=mips_initrd.o
    VPATH+=$(RGSRC)/os/kernel/arch/mips/boot/
    CFLAGS_local_mips_initrd.o+=
      $(if $(CONFIG_CPU_LITTLE_ENDIAN),-DCONFIG_CPU_LITTLE_ENDIAN,)
  endif
endif

ifeq ($(CONFIG_OPENRG)-$(CONFIG_RG_GPL),y-)
  LOCAL_TARGET+=rg_version_info
  LDLIBS_rg_version_info:=-llocal_openrg -llocal_jutil $(LDLIBS_rg_version_info)
  O_OBJS_rg_version_info=rg_version_info.o
endif

FIRST_CLEAN=disk_image_clean

ifdef CONFIG_RG_JPKG
  SUBDIRS=conf_files
endif
ifndef CONFIG_RG_RGLOADER
  SUBDIRS=conf_files
endif
CLEAN_SUBDIRS=$(SUBDIRS) $(if $(IS_DISTRIBUTION),,make_cd)

CD_EXPORTED_FILES+=rg_gcc.c
rg_gcc: rg_gcc.c
	$(CC_FOR_BUILD) $(LOCAL_CFLAGS) \
	  -DRG_GCC="\"$(REAL_GCC)\"" \
	  -DGCC_PREFIX="\"$(LIB_GCC_DIR_PREFIX)\"" \
	  -DRGSRC="\"$(RGSRC)\"" \
	  -DBUILDDIR="\"$(BUILDDIR)\"" \
	  -o $@ $^

gcc: rg_gcc
	$(RG_LN) $(CURDIR)/rg_gcc gcc

include $(PWD_SRC)/Makefile.create_config

RAMDISK_LAST_TASKS+=make_ramdisk
make_ramdisk: ./make_ramdisk.sh
ifdef CONFIG_RG_OS_LINUX
	$^
  ifdef CONFIG_RG_OS_LINUX_22
    ifdef CONFIG_MIPS
# TODO: The Mips structure-at-the-beginning-of-ramdisk quirk. Remove this
# and change linux/arch/mips/kernel/setup.c
	mv $(COMPRESSED_DISK) $(COMPRESSED_DISK).tmp
	./mips_initrd $(COMPRESSED_DISK).tmp $(COMPRESSED_DISK)
	rm -f $(COMPRESSED_DISK).tmp
    endif
  endif
endif
ifdef CONFIG_RG_EFSS
	./mkefs
	$(CC) $(CFLAGS) -c efss_data.c
	$(AR) cruv libramdisk.a efss_main.o efss_data.o
endif

disk_image_clean: $(_EXEC_AS_ROOT)
	$(_EXEC_AS_ROOT_COMMAND) rm -fr disk_image
	$(_EXEC_AS_ROOT_COMMAND) rm -fr bootldr_disk_image

.PHONY: do_footprint
CLEAN+=footprint_report.txt pkg_sum_file.txt  
ifdef CONFIG_RG_LZMA_COMPRESSED_KERNEL
  ARCHIVER:=--archiver "$(BUILDDIR)/pkg/lzma/SRC/lzma -so e"
endif

ifndef CONFIG_RG_GPL
  LOCAL_TARGET+=footprint_report
endif
LDFLAGS_footprint_report+=-llocal_jutil
CFLAGS_footprint_report+=-g
O_OBJS_footprint_report+=footprint_report.o

do_footprint: footprint_report
	pwd
	./footprint_report --output footprint_report.txt \
	  --output-list footprint_list_file.txt \
	  --strip $(STRIP) --objcopy $(OBJCOPY) \
	  --linux_img $(PROD_KERNEL_IMAGE) \
	  $(ARCHIVER)
endif

include $(RGMK)
