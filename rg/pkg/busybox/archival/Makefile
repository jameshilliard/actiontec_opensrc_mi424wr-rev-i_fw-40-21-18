RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_archival.a+=$(if $(CONFIG_AR),ar.o)
O_OBJS_archival.a+=$(if $(CONFIG_BUNZIP2),bunzip2.o)
O_OBJS_archival.a+=$(if $(CONFIG_CPIO),cpio.o)
O_OBJS_archival.a+=$(if $(CONFIG_DPKG),dpkg.o)
O_OBJS_archival.a+=$(if $(CONFIG_DPKG_DEB),dpkg_deb.o)
O_OBJS_archival.a+=$(if $(CONFIG_GUNZIP),gunzip.o)
O_OBJS_archival.a+=$(if $(CONFIG_GZIP),gzip.o)
O_OBJS_archival.a+=$(if $(CONFIG_RPM2CPIO),rpm2cpio.o)
O_OBJS_archival.a+=$(if $(CONFIG_RPM),rpm.o)
O_OBJS_archival.a+=$(if $(CONFIG_TAR),tar.o)
O_OBJS_archival.a+=$(if $(CONFIG_UNCOMPRESS),uncompress.o)
O_OBJS_archival.a+=$(if $(CONFIG_UNZIP),unzip.o)

ifneq ($(strip $(O_OBJS_archival.a)),)
  A_TARGET+=archival.a
endif

include $(RGMK)
