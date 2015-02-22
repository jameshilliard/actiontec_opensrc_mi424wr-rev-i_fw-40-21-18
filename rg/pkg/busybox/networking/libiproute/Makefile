RGSRC=../../../
include $(RGSRC)/envir.mak

O_OBJS_libiproute.a+=$(if $(CONFIG_IP),ip_parse_common_args.o ipaddress.o \
  iplink.o iproute.o iptunnel.o libnetlink.o ll_addr.o ll_map.o ll_proto.o \
  ll_types.o rt_names.o rtm_map.o utils.o)

O_OBJS_libiproute.a+=$(if $(CONFIG_IPADDR),ip_parse_common_args.o \
  ipaddress.o libnetlink.o ll_addr.o ll_map.o ll_types.o rt_names.o utils.o)

O_OBJS_libiproute.a+=$(if $(CONFIG_IPROUTE),ip_parse_common_args.o iproute.o \
  libnetlink.o ll_map.o rt_names.o rtm_map.o utils.o)

O_OBJS_libiproute.a+=$(if $(CONFIG_IPTUNNEL),ip_parse_common_args.o \
  iptunnel.o rt_names.o utils.o)

ifneq ($(strip $(O_OBJS_libiproute.a)),)
  A_TARGET+=libiproute.a
endif

include $(RGMK)
