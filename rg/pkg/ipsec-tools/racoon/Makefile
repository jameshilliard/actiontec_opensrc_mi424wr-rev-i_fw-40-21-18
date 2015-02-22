RGSRC= ../../../
include $(RGSRC)/envir.mak

LDFLAGS+=-L../libipsec -lcrypto -lresolv -lipsec#../libipsec/libipsec.a 
CFLAGS+=-I../include-glibc/ -I../ -I../../ -I../libipsec -I. -DHAVE_CONFIG_H -DSYSCONFDIR=\"/etc\" -DADMINPORTDIR=\"/var/racoon\" -I./missing

TARGET=racoon
RAMDISK_BIN_FILES+=$(TARGET)
INTERNAL_HEADERS+= \
admin.h           debug.h     gnuc.h         isakmp_frag.h   isakmp_xauth.h  plog.h        safefile.h  var.h \
admin_var.h       debugrm.h   grabmyaddr.h   isakmp.h        localconf.h     policy.h      sainfo.h    vendorid.h \
algorithm.h       dhgroup.h   gssapi.h       isakmp_ident.h  logger.h        privsep.h     schedule.h  vmbuf.h \
backupsa.h        dnssec.h    handler.h      isakmp_inf.h    misc.h          proposal.h    session.h \
cfparse.h         dump.h      ipsec_doi.h    isakmp_newg.h   nattraversal.h  prsa_par.h    sockmisc.h \
cfparse_proto.h   evt.h       isakmp_agg.h   isakmp_quick.h  netdb_dnssec.h  racoonctl.h   str2val.h \
cftoken_proto.h   gcmalloc.h  isakmp_base.h  isakmp_unity.h  oakley.h        remoteconf.h  strnames.h \
crypto_openssl.h  genlist.h   isakmp_cfg.h   isakmp_var.h    pfkey.h         rsalist.h     throttle.h \
missing/crypto/rijndael/rijndael-api-fst.h \
missing/crypto/rijndael/rijndael_local.h \
missing/crypto/rijndael/rijndael-alg-fst.h \
missing/crypto/rijndael/rijndael.h \
missing/crypto/sha2/sha2.h \
missing/crypto/rijndael/boxes-fst.dat


O_OBJS=admin.o evt.o handler.o isakmp_ident.o isakmp_quick.o misc.o policy.o sainfo.o algorithm.o getcertsbyname.o  ipsec_doi.o    isakmp_inf.o    localconf.o     oakley.o  privsep.o     schedule.o  vendorid.o crypto_openssl.o  grabmyaddr.o      isakmp_agg.o   isakmp_newg.o   logger.o        pfkey.o   proposal.o    session.o   vmbuf.o dnssec.o          gssapi.o          isakmp_base.o  isakmp.o        main.o          plog.o    remoteconf.o  str2val.o strnames.o sockmisc.o genlist.o safefile.o rsalist.o backupsa.o cfparse.o cftoken.o prsa_par.o prsa_tok.o\
./missing/crypto/rijndael/rijndael-alg-fst.o \
./missing/crypto/rijndael/rijndael-api-fst.o \


#./missing/crypto/sha2/sha2.o

include $(RGMK)
