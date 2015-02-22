#
# linux/arch/arm/boot/bootp/Makefile
#

RGSRC=../../../../../..
include $(RGSRC)/envir.mak

BOOTP_DEPS	:= init.o kernel.o initrd.o bootp.lds $(BOOTSTRAP)
BOOTP_OBJS	:= init.o $(notdir $(BOOTSTRAP))
IMAGE_OFFSET	:= 0

ifdef CONFIG_CRAMFS_FS
# Cramfs padding value
# 512: CRAMFS will make 512 skip (constant), skip from the beginning of image
# 76: sizeof(cramfs superblock) 
IMAGE_OFFSET	:= 588 # (512 + 76)
BOOTP_DEPS	+= boot.512
endif

ifdef CONFIG_IXP425_POST
BOOTP_DEPS	+= post
BOOTP_OBJS	+= $(POST)
endif

ZSYSTEM		=$(TOPDIR)/arch/arm/boot/zImage
ZLDFLAGS	=-p -X -T bootp.lds \
		 --defsym initrd_addr=$(INITRD_PHYS) \
		 --defsym params=$(PARAMS_PHYS) \
		 --defsym kernel_addr=$(ZTEXTADDR) \

all:		bootp

# Note that bootp.lds picks up kernel.o and initrd.o
bootp::		$(BOOTP_DEPS)
		$(LD) $(ZLDFLAGS) -o $@ $(BOOTP_OBJS)
		
ifdef CONFIG_CRAMFS_FS
boot.512:	boot.o
		$(OBJCOPY) -O binary -R .note -R .comment -S $< $@
endif

ifdef CONFIG_IXP425_POST
post:
		$(MAKE) -C ../post $@
endif

%.o:%.S
		$(CC) $(AFLAGS) -c $<

kernel.o:	$(ZSYSTEM)
		$(LD) -r -s -o $@ -b binary $(ZSYSTEM)

initrd.o:	$(INITRD)
		$(LD) -r -s -o $@ -b binary $(INITRD)

bootp.lds:	bootp.lds.in
		@sed 's/IMAGE_OFFSET/$(IMAGE_OFFSET)/' $< >$@
	
.PHONY:		$(INITRD) $(ZSYSTEM)

clean:
		$(RM) bootp bootp.lds *.o 
ifdef CONFIG_CRAMFS_FS
		$(RM) boot.512
endif
ifdef CONFIG_IXP425_POST
		$(MAKE) -C ../post $@
endif

