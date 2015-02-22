RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_loginutils.a+=$(if $(CONFIG_ADDGROUP),addgroup.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_ADDUSER),adduser.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_GETTY),getty.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_LOGIN),login.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_PASSWD),passwd.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_SU),su.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_SULOGIN),sulogin.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_VLOCK),vlock.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_DELUSER),deluser.o)
O_OBJS_loginutils.a+=$(if $(CONFIG_DELGROUP),delgroup.o)

ifneq ($(CONFIG_LOGIN)$(CONFIG_PASSWD)$(CONFIG_SU)$(CONFIG_SULOGIN)$(CONFIG_VLOCK),)
  LDLIBS_loginutils.a:=lcrypt $(LDLIBS_loginutils.a)
endif

ifneq ($(strip $(O_OBJS_loginutils.a)),)
  A_TARGET+=loginutils.a
endif

include $(RGMK)
