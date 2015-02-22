RGSRC=../../../
include $(RGSRC)/envir.mak

LIBPWDGRP_MSRC0=pwd_grp.c
LIBPWDGRP_MOBJ0=fgetpwent_r.o fgetgrent_r.o fgetpwent.o fgetgrent.o \
  getpwnam_r.o getgrnam_r.o getpwuid_r.o getgrgid_r.o getpwuid.o getgrgid.o \
  getpwnam.o getgrnam.o getpw.o getpwent_r.o getgrent_r.o getpwent.o \
  getgrent.o initgroups.o putpwent.o putgrent.o

LIBPWDGRP_MSRC1=pwd_grp.c
LIBPWDGRP_MOBJ1=__parsepwent.o __parsegrent.o __pgsreader.o fgetspent_r.o \
  fgetspent.o sgetspent_r.o getspnam_r.o getspnam.o getspent_r.o getspent.o \
  sgetspent.o putspent.o __parsespent.o # getspuid_r.o getspuid.o

CFLAGS+=-DL_$(@:%.o=%)

O_OBJS_libpwdgrp.a+=$(if $(CONFIG_USE_BB_PWD_GRP),$(LIBPWDGRP_MOBJ0)) \
  $(if $(CONFIG_USE_BB_PWD_GRP),$(LIBPWDGRP_MOBJ1))

ifneq ($(strip $(O_OBJS_libpwdgrp.a)),)
  A_TARGET+=libpwdgrp.a
endif

include $(RGMK)
