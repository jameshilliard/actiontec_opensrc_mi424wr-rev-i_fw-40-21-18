RGSRC= ../../
include $(RGSRC)/envir.mak

ifndef CONFIG_RG_GPL

ifdef CONFIG_IPV6
CFLAGS +=-DHAVE_IPV6 -DHAVE_PROC_NET_IF_INET6
endif
ifneq ($(CONFIG_RG_OS_VXWORKS),)
  EXPORT_HEADERS_DIR=route
  EXPORT_HEADERS+=rg_route.h

  O_TARGET=rg_route_mod.o
  O_OBJS=rg_route.o rg_route_cache.o

  CFLAGS+=-D__KERNEL__ -I$(RGSRC)/os/vx/net
endif

ifneq ($(CONFIG_RG_ROUTE_MULTIWAN),)
  A_TARGET=libroute.a
  
  SUBDIRS=main
  L_OBJS_$(A_TARGET)+=main/route_main.o
  
  ifdef CONFIG_RG_WBM
    SUBDIRS+=wbm
    JPKG_EXPORTED_DIR+=wbm
    L_OBJS_$(A_TARGET)+=wbm/wbm.o
  endif
endif

endif

ifneq ($(CONFIG_RG_QUAGGA),)
  SUBDIRS+=quagga
endif

include $(RGMK)
