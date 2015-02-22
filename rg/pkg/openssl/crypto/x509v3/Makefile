RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=x509v3.h
INTERNAL_HEADERS=ext_dat.h pcy_int.h

O_OBJS=	v3_bcons.o v3_bitst.o v3_conf.o v3_extku.o v3_ia5.o v3_lib.o \
	v3_prn.o v3_utl.o v3err.o v3_genn.o v3_alt.o v3_skey.o v3_akey.o \
	v3_pku.o \
	v3_int.o v3_enum.o v3_sxnet.o v3_cpols.o v3_crld.o v3_purp.o v3_info.o \
	v3_ocsp.o v3_akeya.o v3_pmaps.o v3_pcons.o v3_ncons.o v3_pcia.o \
	v3_pci.o \
	pcy_cache.o pcy_node.o pcy_data.o pcy_map.o pcy_tree.o pcy_lib.o

include $(RGMK)

