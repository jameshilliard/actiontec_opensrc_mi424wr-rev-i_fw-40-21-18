RGSRC=../../../../..
include $(RGSRC)/envir.mak

ARCHCONFIG_FIRST_TASKS=libsupc

libsupc::
	$(AR) x $(shell $(CC) -print-file-name=libsupc++.a)
	$(RM) new_op*.o del_op*.o pure.o new_handler.o

include $(RGMK)
