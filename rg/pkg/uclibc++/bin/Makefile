RGSRC=../../..
include $(RGSRC)/envir.mak

ifeq ($(CONFIG_MIPS),y)
  # taken from Config.mips
  ARCH_CFLAGS="-mno-split-addresses"
endif

WRAPPER	= g++-uc

ARCHCONFIG_LAST_TASKS+=$(WRAPPER)

UCLIBCXX_RUNTIME_INCLUDEDIR=$(RGSRC)/pkg/uclibc++/include
UCLIBCXX_RUNTIME_LIBDIR=$(BUILDDIR)/pkg/uclibc++/src
LNAME=uClibc++

$(WRAPPER):
	echo "#!/bin/bash" > $(WRAPPER)
	echo "" >> $(WRAPPER)
	echo 'WRAPPER_INCLUDEDIR="$${WRAPPER_INCLUDEDIR:=-I$(UCLIBCXX_RUNTIME_INCLUDEDIR)}"' >> $(WRAPPER)
	# this is done by rg_gcc for C programs
	echo 'WRAPPER_INCLUDEDIR="$${WRAPPER_INCLUDEDIR} -I$(BUILDDIR)/pkg/uclibc++/include -I$(BUILDDIR)/pkg/include"' >> $(WRAPPER)
	echo 'WRAPPER_LIBDIR="$${WRAPPER_LIBDIR:=-L$(UCLIBCXX_RUNTIME_LIBDIR)}"' >> $(WRAPPER)
	echo "" >> $(WRAPPER)
	echo 'WRAPPER_OPTIONS=""' >> $(WRAPPER)
	echo "WRAPPER_LIBS=\"$(LIBS)\"" >> $(WRAPPER)
	echo "" >> $(WRAPPER)
	echo 'WRAPPER_INCLIB="Y"' >> $(WRAPPER)
	echo 'while [ -n "$$1" ]' >> $(WRAPPER)
	echo 'do' >> $(WRAPPER)
	echo '	if [ "$$1" != "-Wno-pointer-sign" ] ; then' >> $(WRAPPER)
	echo '		WRAPPER_OPTIONS="$$WRAPPER_OPTIONS $$1"' >> $(WRAPPER)
	echo '	fi' >> $(WRAPPER)
	echo '	if [ "$$1" == "-c" ]' >> $(WRAPPER)
	echo '	then' >> $(WRAPPER)
	echo '		WRAPPER_INCLIB="N"' >> $(WRAPPER)
	echo '	fi' >> $(WRAPPER)
ifneq ($(strip $(LIBS)),$(strip $(STATIC_LIBS)))
	echo '	if [ "$$1" == "-static" ]' >> $(WRAPPER)
	echo '	then' >> $(WRAPPER)
	echo "		WRAPPER_LIBS=\"$(STATIC_LIBS)\"" >> $(WRAPPER)
	echo '	fi' >> $(WRAPPER)
endif
	echo '	shift' >> $(WRAPPER)
	echo 'done' >> $(WRAPPER)
	echo 'if [ "$$WRAPPER_INCLIB" == "Y" ]' >> $(WRAPPER)
	echo 'then' >> $(WRAPPER)
	echo '	WRAPPER_OPTIONS="$$WRAPPER_OPTIONS -nodefaultlibs $$WRAPPER_LIBDIR -l$(LNAME) $$WRAPPER_LIBS"' >> $(WRAPPER)
	echo 'fi' >> $(WRAPPER)
	echo "" >> $(WRAPPER)
ifeq ($(DODEBUG),y)
	echo 'echo $(TARGET_CXX) $(ARCH_CFLAGS) $(GEN_CFLAGS) $(GEN_CXXFLAGS) $(EH_CXXFLAGS) $$WRAPPER_INCLUDEDIR $$WRAPPER_OPTIONS' >> $(WRAPPER)
endif
	echo 'exec $(TARGET_CXX) $(filter-out -Wno-pointer-sign,$(ARCH_CFLAGS)) $(GEN_CFLAGS) $(GEN_CXXFLAGS) $(EH_CXXFLAGS) $$WRAPPER_INCLUDEDIR $$WRAPPER_OPTIONS' >> $(WRAPPER)
	chmod 755 $(WRAPPER)

include $(RGMK)
