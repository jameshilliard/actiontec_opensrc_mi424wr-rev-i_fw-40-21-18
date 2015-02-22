KAFFEDIR=$(RGSRC)/pkg/kaffe

WARN2ERR=n

KAFFE_ENGINE=intrp
KAFFE_THREADS=unix-pthreads
KAFFE_GC=kaffe-gc

INCDIRS= \
	-I$(KAFFEDIR)/config \
	-I$(KAFFEDIR)/config/$(LIBC_ARCH)/linux \
	-I$(KAFFEDIR)/include \
	-I$(KAFFEDIR)/kaffe \
	-I$(KAFFEDIR)/kaffe/kaffevm/jni \
	-I$(KAFFEDIR)/kaffe/kaffevm \
	-I$(KAFFEDIR)/kaffe/kaffevm/$(KAFFE_ENGINE) \
	-I$(KAFFEDIR)/kaffe/kaffevm/systems/$(KAFFE_THREADS) \
	-I$(KAFFEDIR)/replace \
	-I$(KAFFEDIR)/binreloc \
	-I$(KAFFEDIR)/libraries/clib/classpath \
	-I$(KAFFEDIR)/libraries/clib/fdlibm \
	-I$(KAFFEDIR)/libraries/clib/target/Linux \
	-I$(KAFFEDIR)/libraries/clib/target/generic \
	-I$(KAFFEDIR)/kaffe/jvmpi \
	-I$(KAFFEDIR)/kaffe/xprof \
	-I$(KAFFEDIR)/kaffe/kaffevm/verifier \
	-I$(KAFFEDIR)/config/$(LIBC_ARCH) \
	-I$(KAFFEDIR)/libltdl

CFLAGS+= \
	-DBR_PTHREADS=0 \
	-DENABLE_RELOC \
	-DHAVE_CONFIG_H \
	-DINTERPRETER \
	-DKAFFEMD_BUGGY_STACK_OVERFLOW=1 \
	-DKAFFE_VMDEBUG=1 \
	-D$(LIBC_ARCH) \
	$(INCDIRS)


