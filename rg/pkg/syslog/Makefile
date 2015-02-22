RGSRC=../../
include $(RGSRC)/envir.mak

# We may enter to this package when glibc is required and|or
# syslog is required.


# There is a bug (B28967) in glibc/ulibc, which is fixed in pkg/ulibc.
# Workaround this bug in other libcs (glibc, or ulibc from toolchain) by using
# code from this glibc directory.
ifdef CONFIG_RG_SYSLOG_GLIBC
  SUBDIRS+=glibc
endif

ifndef CONFIG_RG_GPL

EXPORT_HEADERS_DIR=syslog
EXPORT_HEADERS+=syslog.h rg_log.h klog.h

ifeq ($(CONFIG_RG_SYSLOG),y)
  O_TARGET=sys_log.o
  O_OBJS_$(O_TARGET)=syslog.o

  ifdef CONFIG_RG_LOG_DEV
    O_OBJS_$(O_TARGET)+=rg_log.o
    MOD_TARGET=log_chardev.o
    RAMDISK_MODULES_FILES+=$(MOD_TARGET)
    OX_OBJS_$(MOD_TARGET)=log_dev.o
  endif

  ifeq ($(CONFIG_RG_LOG_DEV),y)
    MOD_2_STAT+=$(MOD_TARGET)
  endif

  ifdef CONFIG_RG_KERN_LOG
    O_OBJS_$(O_TARGET)+=klog.o
  endif

  SUBDIRS+=main cli
  L_OBJS_$(O_TARGET)=main/syslog_main.o cli/cli.o

  ifneq ($(CONFIG_RG_WBM),)
    SUBDIRS+=wbm
    JPKG_EXPORTED_DIR+=wbm
    L_OBJS_$(O_TARGET)+=wbm/wbm.o
  endif

  ifneq ($(CONFIG_RG_SSI),)
    SUBDIRS+=ssi
    L_OBJS_$(O_TARGET)+=ssi/ssi.o
  endif
endif

endif

include $(RGMK)
