RGSRC=../../
include $(RGSRC)/envir.mak

EXPORT_HEADERS_DIR=freeswan
EXPORT_HEADERS+=rg/netbios_rt/netbios_rt.h

JPKG_EXPORTED_DIR+=doc klips lib libcrypto libdes packaging pluto testing \
  utils zlib

ifeq ($(CONFIG_FREESWAN),y)
  ARCHCONFIG_FIRST_TASKS=kernel_links
  SUBDIRS=lib libdes pluto utils rg klips

else
  ifeq ($(CONFIG_LIBDES),y)
    SUBDIRS=libdes
  endif
endif

ifeq ($(CONFIG_FREESWAN),y)
  KERNELKLIPS=$(CURDIR)/klips/net/ipsec
  KLIPSALG=$(KERNELKLIPS)/alg
  PLUTOALG=$(CURDIR)/pluto/alg
  LINK_DIRS=$(addprefix $(KERNELKLIPS)/,libfreeswan zlib libcrypto libdes)
  LINK_DIRS+=$(addprefix $(KLIPSALG)/,libmd5 libsha1 libaes)
  LINK_DIRS+=$(addprefix $(PLUTOALG)/,libaes)
endif

ifeq ($(CONFIG_FREESWAN),y)
$(LINK_DIRS):
	$(MKDIR) $@

clean_dirs:
	rm -fr $(LINK_DIRS)

zlib_links:
	$(MKDIR) zlib
	$(RG_LN) $(RG_INCLUDE_DIR)/zlib/c/*.c zlib
	$(RG_LN) $(RG_INCLUDE_DIR)/zlib/*.h zlib

kernel_links: $(LINK_DIRS) zlib_links
	$(RG_LN) $(PWD_SRC)/lib/Makefile.kernel $(KERNELKLIPS)/libfreeswan/Makefile
	$(RG_LN) $(PWD_SRC)/lib/*.[ch] $(KERNELKLIPS)/libfreeswan
	$(RG_LN) $(PWD_SRC)/libdes/Makefile.kern $(KERNELKLIPS)/libdes/Makefile
	$(RG_LN) $(PWD_SRC)/libdes/*.[ch] $(KERNELKLIPS)/libdes
	$(RG_LN) $(PWD_SRC)/zlib/Makefile $(KERNELKLIPS)/zlib/Makefile
	$(RG_LN) $(CURDIR)/zlib/*.[chS] $(KERNELKLIPS)/zlib
	$(RG_LN) $(PWD_SRC)/libcrypto/* $(KERNELKLIPS)/libcrypto
	$(RG_LN) $(KERNELKLIPS)/libcrypto/libmd5/* $(KLIPSALG)/libmd5
	$(RG_LN) $(KERNELKLIPS)/libcrypto/libsha1/* $(KLIPSALG)/libsha1
	$(RG_LN) $(KERNELKLIPS)/libcrypto/libaes/Makefile.kern $(KLIPSALG)/libaes/Makefile
	$(RG_LN) $(KERNELKLIPS)/libcrypto/libaes/*.[ch] $(KLIPSALG)/libaes
	$(RG_LN) $(PWD_SRC)/libcrypto/libaes/Makefile $(PLUTOALG)/libaes/Makefile
	$(RG_LN) $(PWD_SRC)/libcrypto/libaes/*.[ch] $(PLUTOALG)/libaes

RAMDISK_LAST_TASKS+=ramdisk_ipsec.d
ramdisk_ipsec.d:
	$(RG_MKDIR) $(RAMDISK_RW_DIR)/etc/ipsec.d
	$(RG_MKDIR) $(RAMDISK_RW_DIR)/etc/ipsec.d/private
	$(RG_MKDIR) $(RAMDISK_RW_DIR)/etc/ipsec.d/cacerts
endif

include $(RGMK)
