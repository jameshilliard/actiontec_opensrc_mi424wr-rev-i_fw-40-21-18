RGSRC=../../
include $(RGSRC)/envir.mak

include $(PWD_SRC)/openssl_env.mak

SUBDIRS=crypto $(if $(CONFIG_RG_OPENSSL),ssl)

EXPORT_HEADERS+=e_os.h e_os2.h
CD_EXPORTED_FILES=LICENSE openssl_env.mak

# go throu all directories just to export header files.
ARCHCONFIG_SUBDIRS=crypto ssl

include $(RGMK)

