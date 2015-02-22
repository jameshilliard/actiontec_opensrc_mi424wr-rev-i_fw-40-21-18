RGSRC=../..
include $(RGSRC)/envir.mak

ifdef CONFIG_RG_RGLOADER
  SUBDIRS=applets modutils init procps util-linux libbb
else
  SUBDIRS=applets archival archival/libunarchive coreutils console-tools \
    debianutils editors findutils init miscutils modutils networking \
    networking/libiproute networking/udhcp procps loginutils shell \
    sysklogd util-linux libpwdgrp coreutils/libcoreutils libbb
endif

busybox.links: applets/busybox.mkll include/config.h include/applets.h
ifdef IS_BUILDDIR
	$^ >$@
endif

ARCHCONFIG_FIRST_TASKS+=del_busybox_libs.mk
del_busybox_libs.mk:
	rm -f busybox_libs.mk

ifndef DOING_MAKE_CONFIG
include busybox_libs.mk
endif

OTHER_TARGETS+=busybox.links

RAMDISK_LAST_TASKS+=ramdisk_links
ramdisk_links: ./applets/install.sh
	$(call RAMDISK_MKDIR_FUNC,$(RAMDISK_RW_DIR)/bin)
	$(call RAMDISK_MKDIR_FUNC,$(RAMDISK_RO_DIR)/bin)
	$< $(PREFIX)

TARGET+=busybox
RAMDISK_BIN_FILES+=busybox

LDFLAGS+=-Wl,--start-group $(BUSYBOX_LIBS) -Wl,--end-group

include $(RGMK)

ifdef IS_BUILDDIR
busybox: $(BUSYBOX_LIBS)
endif
