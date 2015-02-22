RGSRC=../../../../..
include $(RGSRC)/envir.mak

TARGET_$(CONFIG_USB_SPCA5XX)+=spca5xx_mod.o
MOD_2_STAT_PREFIX_spca5xx_mod.o=2
OX_OBJS_spca5xx_mod.o+=spca5xx.o spcadecoder.o

# optional: -DSPCA50X_ENABLE_DEBUG
CFLAGS_spca5xx.o+=-DVID_HARDWARE_SPCA5XX=0xFF -DSPCA5XX_VERSION=\"00.57.00\"

MOD_TARGET=$(TARGET_m) $(TARGET_y)
MOD_2_STAT=$(TARGET_y)

MOD_CFLAGS+=-I../include

include $(RGMK)
