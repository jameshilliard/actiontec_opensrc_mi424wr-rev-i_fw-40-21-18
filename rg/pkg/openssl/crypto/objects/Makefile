RGSRC=../../../../
include $(RGSRC)/envir.mak
include $(PWD_SRC)/../crypto_env.mak

EXPORT_HEADERS=objects.h
CD_EXPORTED_FILES=obj_dat.pl objects.pl objects.txt obj_mac.num.orig

O_OBJS=o_names.o obj_dat.o obj_lib.o obj_err.o

include $(RGMK)

archconfig:: obj_dat.h

obj_dat.h: obj_dat.pl obj_mac.h
	$(PERL) $^ $@

# objects.pl both reads and writes obj_mac.num
obj_mac.h: objects.pl objects.txt obj_mac.num
	$(PERL) $^ $@
	$(RG_LN) $(CURDIR)/$@ $(RG_INCLUDE_DIR)/$(EXPORT_HEADERS_DIR)/$(notdir $@)

obj_mac.num: obj_mac.num.orig
	@cp $< $@
	chmod u+w $@

