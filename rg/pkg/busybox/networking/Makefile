RGSRC=../../../
include $(RGSRC)/envir.mak

A_TARGET+=networking.a

O_OBJS_networking.a+=$(if $(CONFIG_ARPING),arping.o)
O_OBJS_networking.a+=$(if $(CONFIG_FTPGET),ftpgetput.o)
O_OBJS_networking.a+=$(if $(CONFIG_FTPPUT),ftpgetput.o)
O_OBJS_networking.a+=$(if $(CONFIG_HOSTNAME),hostname.o)
O_OBJS_networking.a+=$(if $(CONFIG_HTTPD),httpd.o)
O_OBJS_networking.a+=$(if $(CONFIG_IFCONFIG),ifconfig.o)
O_OBJS_networking.a+=$(if $(CONFIG_IFUPDOWN),ifupdown.o)
O_OBJS_networking.a+=$(if $(CONFIG_INETD),inetd.o)
O_OBJS_networking.a+=$(if $(CONFIG_IP),ip.o)
O_OBJS_networking.a+=$(if $(CONFIG_IPCALC),ipcalc.o)
O_OBJS_networking.a+=$(if $(CONFIG_IPADDR),ipaddr.o)
O_OBJS_networking.a+=$(if $(CONFIG_IPLINK),iplink.o)
O_OBJS_networking.a+=$(if $(CONFIG_IPROUTE),iproute.o)
O_OBJS_networking.a+=$(if $(CONFIG_IPTUNNEL),iptunnel.o)
O_OBJS_networking.a+=$(if $(CONFIG_NAMEIF),nameif.o)
O_OBJS_networking.a+=$(if $(CONFIG_NC),nc.o)
O_OBJS_networking.a+=$(if $(CONFIG_NETSTAT),netstat.o)
O_OBJS_networking.a+=$(if $(CONFIG_NSLOOKUP),nslookup.o)
O_OBJS_networking.a+=$(if $(CONFIG_PING),ping.o)
O_OBJS_networking.a+=$(if $(CONFIG_PING6),ping6.o)
O_OBJS_networking.a+=$(if $(CONFIG_ROUTE),route.o)
O_OBJS_networking.a+=$(if $(CONFIG_TELNET),telnet.o)
O_OBJS_networking.a+=$(if $(CONFIG_TELNETD),telnetd.o)
O_OBJS_networking.a+=$(if $(CONFIG_TFTP),tftp.o)
O_OBJS_networking.a+=$(if $(CONFIG_TRACEROUTE),traceroute.o)
O_OBJS_networking.a+=$(if $(CONFIG_VCONFIG),vconfig.o)
O_OBJS_networking.a+=$(if $(CONFIG_WGET),wget.o)

ifdef CONFIG_FEATURE_HTTPD_AUTH_MD5
  LDLIBS_networking.a:=lcrypt $(LDLIBS_networking.a)
endif

include $(RGMK)
