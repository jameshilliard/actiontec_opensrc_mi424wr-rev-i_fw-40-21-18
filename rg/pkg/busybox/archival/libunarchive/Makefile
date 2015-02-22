RGSRC=../../../../
include $(RGSRC)/envir.mak

A_TARGET+=libunarchive.a

O_OBJS_libunarchive.a+=data_skip.o data_extract_all.o \
  data_extract_to_stdout.o data_extract_to_buffer.o filter_accept_all.o \
  filter_accept_list.o filter_accept_reject_list.o header_skip.o \
  header_list.o header_verbose_list.o archive_xread_all.o \
  archive_xread_all_eof.o seek_by_char.o seek_by_jump.o data_align.o \
  find_list_entry.o open_transformer.o init_handle.o

GUNZIP_FILES=check_header_gzip.o decompress_unzip.o
DPKG_FILES=get_header_ar.o unpack_ar_archive.o get_header_tar.o \
  filter_accept_list_reassign.o

O_OBJS_libunarchive.a+=$(if $(CONFIG_AR) get_header_ar.o,unpack_ar_archive.o)
O_OBJS_libunarchive.a+=$(if $(CONFIG_BUNZIP2),decompress_bunzip2.o)
O_OBJS_libunarchive.a+=$(if $(CONFIG_CPIO),get_header_cpio.o)
O_OBJS_libunarchive.a+=$(if $(CONFIG_FEATURE_DEB_TAR_GZ),get_header_tar_gz.o)
O_OBJS_libunarchive.a+=\
  $(if $(CONFIG_FEATURE_DEB_TAR_BZ2),decompress_bunzip2.o get_header_tar_bz2.o)
O_OBJS_libunarchive.a+=\
  $(if $(CONFIG_FEATURE_GUNZIP_UNCOMPRESS),decompress_uncompress.o)
O_OBJS_libunarchive.a+=$(if $(CONFIG_RPM2CPIO),get_header_cpio.o)
O_OBJS_libunarchive.a+=$(if $(CONFIG_RPM),get_header_cpio.o)
O_OBJS_libunarchive.a+=$(if $(CONFIG_TAR),get_header_tar.o)
O_OBJS_libunarchive.a+=\
  $(if $(CONFIG_FEATURE_TAR_BZIP2),decompress_bunzip2.o get_header_tar_bz2.o)
O_OBJS_libunarchive.a+=$(if $(CONFIG_FEATURE_TAR_GZIP),get_header_tar_gz.o)
O_OBJS_libunarchive.a+=\
  $(if $(CONFIG_FEATURE_TAR_COMPRESS),decompress_uncompress.o)
O_OBJS_libunarchive.a+=$(if $(CONFIG_UNCOMPRESS),decompress_uncompress.o)

O_OBJS_libunarchive.a+=$(if $(CONFIG_FEATURE_DEB_TAR_GZ)$(CONFIG_GUNZIP)$(CONFIG_RPM2CPIO)$(CONFIG_RPM)$(CONFIG_FEATURE_TAR_GZIP)$(CONFIG_UNZIP),$(GUNZIP_FILES))

O_OBJS_libunarchive.a+=$(if $(CONFIG_DPKG)$(CONFIG_DPKG_DEB),$(DPKG_FILES))

include $(RGMK)
