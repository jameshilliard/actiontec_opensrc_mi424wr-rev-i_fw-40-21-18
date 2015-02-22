RGSRC=../../
include $(RGSRC)/envir.mak

A_TARGET=libz.a
EXPORT_LIBS+=libz.a liblocal_z.a
O_OBJS_libz.a = adler32.o compress.o crc32.o gzio.o uncompr.o deflate.o \
  trees.o zutil.o inflate.o infblock.o inftrees.o infcodes.o infutil.o \
  inffast.o

EXPORT_LIB_SRC = adler32.c zutil.h zutil.c infblock.h inflate.c inftrees.h \
  infcodes.h infutil.h infblock.c inffixed.h inftrees.c inffast.h infcodes.c \
  infutil.c inffast.c

ALL_C+=$(filter %.c,$(EXPORT_LIB_SRC)) uncompr.c deflate.c trees.c

ifndef CONFIG_RG_JPKG_SRC
  ALL_C+=zlib_export.c
  EXPORT_HEADERS+=zlib_export.h
endif

CREATE_LOCAL=$(A_TARGET)
DONT_EXPORT=deflate.o local_deflate.o

EXPORT_HEADERS_DIR=zlib
EXPORT_HEADERS+=zlib.h zconf.h trees.h deflate.h \
  $(foreach f,$(ALL_C),$f__c/$f) $(filter %.h,$(EXPORT_LIB_SRC))

#use awk to remove all local includes: #include "*.?"
AWK_RM_INCLUDES='/\#include ["].*[.][hc]["]/ {print "/*",$$0,"*/"} \
  !/\#include ["].*[.][hc]["]/ {print}'
zlib_export.h: zconf.h zlib.h
	@cat $^ > zlib_export_.h
	@awk $(AWK_RM_INCLUDES) zlib_export_.h > $@
	@rm -f zlib_export_.h

zlib_export.c: $(EXPORT_LIB_SRC)
	@cat $^ > zlib_export_.c
	@echo "#define _ZLIB_EXPORT_C" > $@
	@echo "#include \"zlib.h\"" >> $@  # won't remove by awk
	@awk $(AWK_RM_INCLUDES) zlib_export_.c >> $@
	@rm -f zlib_export_.c

ARCHCONFIG_FIRST_TASKS+=zlib_export.c zlib_export.h

include $(RGMK)
