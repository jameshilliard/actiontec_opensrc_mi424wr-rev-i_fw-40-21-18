RGSRC=../../

include $(RGSRC)/envir.mak

JPKG_TARGET_BIN_jspawn=y
A_TARGET_PIC=y

# We need to compile the language compiler as stand alone, because we generate
# the language header files in "make config" stage
ifndef CONFIG_RG_GPL
 ifdef CONFIG_RG_LANG
  LOCAL_TARGET+=$(if ($(CONFIG_RG_RGLOADER)$(CONFIG_OPENRG)),lang_compiler)
  O_OBJS_lang_compiler+=lang_compiler.o
  CFLAGS_local_lang_compiler.o+=-DLANG_COMPILER_STANDALONE
  CFLAGS_REMOVE_local_lang_compiler.o+=-Wstrict-prototypes
  JPKG_TARGET_BIN_lang_compiler=y
 endif
endif

ifdef CONFIG_RG_IR_TEST
  TARGET=tty_write
  O_OBJS_tty_write=tty_write.o
endif

CD_EXPORTED_FILES+=dep_l.mak test_lang.csv
CD_EXPORTED_FILES+=kos_26_mod.c
EXPORT_AS_SRC+=enums.c mkconfig.c rg_version.c rg_version_jungo.c openrg_gpl.c \
  rtnetlink_route.c rtnetlink.c rtnetlink_base.c \
  log_entity.c log_entity_ext_proc.c
INTERNAL_HEADERS+=estream_int.h code2str_ex_test.h getdate.h range_set.h

# Used by create_config
EXPORT_AS_SRC+=str.c session.c
# Used by test utility
INTERNAL_HEADERS+=test.c jtest_util.c jtest_util.h 

LOCAL_TARGET+=mkconfig
MGT_LIB=libmgt_client.a
O_OBJS_mkconfig+=mkconfig.o
JPKG_TARGET_BIN_mkconfig=y

EXPORT_LIBS+=$(LIB_RG) $(LIB_JUTIL) $(LOCAL_LIB_RG) $(LOCAL_LIB_JUTIL) \
  $(LIB_GPL) $(MGT_LIB) $(CONFIG_LIB) $(CONFIG_LOCAL_LIB) \
  $(LIB_SESSION) $(LOCAL_LIB_SESSION)

EXPORT_HEADERS_DIR=util/
EXPORT_HEADERS+=estream_ssl.h rg_ssl.h clock.h base64.h \
  etask.h state_machine.h conv.h rg_system.h rg_time.h tftp_utils.h \
  openrg_gpl.h seq_detector.h file.h date.h str.h strconv.h \
  rg_memory.h rg_random.h dot11.h \
  eresolv.h names.h rg_error.h rg_error_filter.h rg_print.h rg_error_crypto.h \
  memarea.h rg_error_rate.h util.h snprintf.h strace.h alloc.h \
  lines.h attrib.h attrib_io.h console_colors.h csv.h \
  dbi.h dbi_int.h file_util.h fnmatch_ext.h get_next_id.h hash.h \
  html.h jftw.h keyword.h mail.h md5.h os_ip.h \
  proc.h sed.h sort.h log_entity.h log_entity_id.h log_entity_ext_proc.h \
  rg_error_conv.h user_kernel_util.h \
  sql.h sql_html_edit.h sql_print.h sql_util.h str_escape.h str_regex.h \
  str_sql.h str_util.h sys.h term.h testutil.h user.h \
  csv_util.h cvs.h jtest.h parse.h uml.h rg_serial.h net_wait.h \
  mgt_client.h linked_list.h rg_auth.h http_auth.h \
  matrix.h matrix_io.h lang.h rtnetlink_base.h rtnetlink.h rtnetlink_route.h \
  rg_chardev.h queue.h str_iconv.h erecv.h iptables.h range_set.h \
  dbi_helper.h dbi_init.h sql_mysql.h session.h usb_info.h
ifdef ACTION_TEC
EXPORT_HEADERS+=rg_aes.h
ifdef CONFIG_IPV6
EXPORT_HEADERS+=ipv6utils.h
endif
endif

CD_EXPORTED_FILES+=session_cb.h
CD_EXPORTED_FILES+=$(if $(CONFIG_RG_SESSION_LIBDB),session_libdb_utils.h)

LOCAL_TARGET+=jspawn
O_OBJS_jspawn=jspawn.o
LDLIBS_jspawn:=-llocal_jutil $(LDLIBS_jspawn)

ifeq ($(CONFIG_RG_JPKG_SRC)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY)-$(CONFIG_OPENRG),-y)
  LOCAL_TARGET+=local_test
  RUN_UNITTEST+=test
  O_OBJS_local_test+=test.o jtest_util.o test_lang.o test_lang_static.o range_set.o 
  LDLIBS_local_test:=-llocal_openrg -llocal_jutil -L../openssl/crypto -llocal_crypto $(LDLIBS_local_test)
  LDLIBS_test:=$(OPENRG_LDLIBS) $(LDLIBS_test)
endif

ifdef CONFIG_RG_SESSION_LIBDB
  LOCAL_TARGET+=local_session_test
  O_OBJS_local_session_test+=session_test.o jtest_util.o
  LDLIBS_local_session_test:=-llocal_session -llocal_jutil -ldb $(LDLIBS_local_test)
endif

RUN_UNITTEST_DATA+=test.csv test.sql test.tab test_expand.csv \
  test_lang.csv attrib_test.txt

CD_EXPORTED_FILES+=$(RUN_UNITTEST_DATA)
		   
EXPORT_HEADERS+=rg_error.c__c/rg_error.c rg_error_rate.c__c/rg_error_rate.c \
  conv.c__c/conv.c ip_proto_name.c__c/ip_proto_name.c \
  inet_conv.c__c/inet_conv.c parse.c__c/parse.c str_util.c__c/str_util.c \
  rg_print.c__c/rg_print.c alloc.c__c/alloc.c lines.c__c/lines.c \
  rg_chardev.c__c/rg_chardev.c user_kernel_util.c__c/user_kernel_util.c

SUBDIRS+=eresolv

ifdef CONFIG_DYN_LINK
  LIB_RG=libopenrg.so
  LIB_JUTIL=libjutil.so
  CONFIG_LIB=librg_config.so
  SO_TARGET=$(if $(CONFIG_RG_RGLOADER)$(CONFIG_OPENRG), \
    $(LIB_RG)) $(LIB_JUTIL) $(CONFIG_LIB)
  RAMDISK_LIB_FILES+=$(SO_TARGET)
else
  LIB_RG=libopenrg.a
  LIB_JUTIL=libjutil.a
  CONFIG_LIB=librg_config.a
  A_TARGET=$(if $(CONFIG_RG_RGLOADER)$(CONFIG_OPENRG), \
    $(LIB_RG)) $(LIB_JUTIL) $(CONFIG_LIB)
endif

JPKG_TARGET_liblocal_rg_config.a=y
CONFIG_LOCAL_LIB=liblocal_rg_config.a
LOCAL_A_TARGET+=$(CONFIG_LOCAL_LIB)
O_OBJS_$(CONFIG_LIB)=rg_c_config.o
O_OBJS_$(CONFIG_LOCAL_LIB)+=rg_c_config.o
AUTOGEN_SRC=rg_c_config.c

rg_config_lib: $(if $(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),,$(CONFIG_LIB)) $(CONFIG_LOCAL_LIB)
mgt_client_lib: $(if $(CONFIG_RG_RGLOADER)$(CONFIG_OPENRG),$(MGT_LIB))

A_TARGET+=$(if $(CONFIG_RG_RGLOADER)$(CONFIG_OPENRG),$(MGT_LIB))
O_OBJS_$(MGT_LIB)=mgt_client.o

ifneq ($(CONFIG_RG_RGLOADER)$(CONFIG_OPENRG),)
ifneq ($(CONFIG_RG_OS_LINUX)$(CONFIG_RG_JPKG_SRC)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),)
# Note: Any change here, should be done also in Makefile.gpl
  ifdef CONFIG_DYN_LINK
    LIB_GPL=libopenrg_gpl.so
    SO_TARGET+=$(LIB_GPL)
    ifeq ($(CONFIG_RG_SYSLOG_GLIBC),y)
      SO_LDFLAGS_$(LIB_GPL)+=-lulibc_syslog -L$(RGSRC)/pkg/lib
    endif
  else
    LIB_GPL=libopenrg_gpl.a
    A_TARGET+=$(LIB_GPL)
  endif
  EXPORT_LIBS+=$(LIB_GPL)
  O_OBJS_$(LIB_GPL)=openrg_gpl.o ipc.o bio_socket.o rg_memory.o alloc.o \
      log_entity_ext_proc.o str.o
endif # CONFIG_RG_OS_LINUX CONFIG_RG_JPKG_SRC
endif # CONFIG_OPENRG CONFIG_RG_RGLOADER

LOCAL_LIB_RG=liblocal_openrg.a
LOCAL_LIB_JUTIL=liblocal_jutil.a
LDLIBS_$(LOCAL_LIB_RG):=-llocal_rg_config $(LDLIBS_$(LOCAL_LIB_RG))
JPKG_TARGET_BIN_liblocal_openrg.a=y
JPKG_TARGET_liblocal_jutil.a=y

LOCAL_A_TARGET+=$(if $(CONFIG_WINDOWS_HOST),,\
  $(if $(CONFIG_RG_RGLOADER)$(CONFIG_OPENRG),$(LOCAL_LIB_RG)) $(LOCAL_LIB_JUTIL))
  
OBJS_COMMON_RG+=rg_iplist.o ipc.o rg_set_utils.o \
  rg_error_rate.o names.o rg_random.o \
  syscalls.o bio_socket.o stop.o etask.o dot11.o \
  memarea.o ebuf.o ip_proto_name.o rg_chardev.o 

ifeq ($(CONFIG_GLIBC)$(GLIBC_HAVE_FORK),yy)
  JPKG_BIN_LIBCS_daemon_init.o+=GLIBC
  OBJS_COMMON_RG+=daemon_init.o
endif
ifeq ($(CONFIG_ULIBC)$(ULIBC_HAVE_FORK),yy)
  JPKG_BIN_LIBCS_daemon_init.o+=ULIBC
  OBJS_COMMON_RG+=daemon_init.o
endif

OBJS_COMMON_RG+=$(if $(CONFIG_HW_CLOCK),clock.o) \
  $(if $(CONFIG_RG_UML)$(CONFIG_RG_JPKG_SRC)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),uml.o) \
  $(if $(CONFIG_RG_SERIAL),rg_serial.o) $(if $(CONFIG_RG_AUTH),rg_auth.o) \
  $(if $(CONFIG_RG_PPPOS_CLI),seq_detector.o)
ifdef ACTION_TEC
OBJS_COMMON_RG+=rg_aes.o
endif

ifeq ($(CONFIG_RG_JPKG_SRC)-$(CONFIG_RG_SESSION_LIBDB),-y)
  LOCAL_LDLIBS+=-ldb
endif

OBJS_COMMON_JUTIL+=event.o ../openssl/crypto/libcrypto.a rg_auth.o set.o set_utils.o alloc.o net_utils.o str.o \
  lines.o conv.o rg_memory.o base64.o rg_time.o attrib.o \
  proc.o str_escape.o inet_conv.o str_regex.o sys.o file_util.o str_util.o \
  fnmatch_ext.o jftw.o lang.o strconv.o estream.o rg_print.o erecv.o \
  estream_utils.o html.o net_wait.o snprintf.o \
  rtnetlink_base.o syscalls_base.o \
  log_entity_ext_proc.o rg_error_conv.o user_kernel_util.o \
  range_set.o estream_throttle.o enums.o tftp_utils.o date.o usb_info.o \
  $(if $(CONFIG_OPENRG)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),sed.o) \
  $(if $(CONFIG_RG_CRYPTO),rg_error_crypto.o) \
  $(if $(CONFIG_RG_OPENSSL),rg_ssl.o estream_ssl.o) \
  $(if $(CONFIG_RG_OPENSSL_MD5),obscure.o http_auth.o) \
  $(O_LANG_FILES)
ifdef ACTION_TEC
OBJS_COMMON_JUTIL+=rg_aes.o
ifdef CONFIG_IPV6
OBJS_COMMON_JUTIL+=ipv6utils.o
endif
endif

LIB_SESSION=libsession.a
LOCAL_LIB_SESSION=liblocal_session.a
A_TARGET+=$(LIB_SESSION)
LOCAL_A_TARGET+=$(LOCAL_LIB_SESSION)

OBJS_LIBSESSION+=session.o
ifdef CONFIG_RG_SESSION_LIBDB
  OBJS_LIBSESSION+=session_libdb.o session_libdb_utils.o
endif
ifdef CONFIG_RG_SESSION_MEMORY
  OBJS_LIBSESSION+=session_memory.o
endif

O_OBJS_$(LIB_SESSION)+=$(OBJS_LIBSESSION)
O_OBJS_$(LOCAL_LIB_SESSION)+=$(OBJS_LIBSESSION)

ifndef CONFIG_LSP_DIST
  OBJS_COMMON_JUTIL+=rg_version.o rg_version_jungo.o jtest.o \
    keyword.o attrib_io.o sort.o testutil.o parse.o rg_error.o rg_error_filter.o
endif

LINUX_OBJS+=rtnetlink.o rtnetlink_route.o rg_system.o process_funcs.o

O_OBJS_$(LIB_RG)+=$(OBJS_COMMON_RG) \
  $(if $(CONFIG_RG_TARGET_LINUX),$(LINUX_OBJS)) \
  $(if $(CONFIG_RG_TARGET_VXWORKS)$(CONFIG_RG_JPKG_SRC)$(CONFIG_RG_BUILD_LOCAL_TARGETS_ONLY),\
  os_libc_extra.o vx_net_fixup.o)
  
O_OBJS_$(LIB_JUTIL)+=$(OBJS_COMMON_JUTIL) \
  $(if $(CONFIG_RG_TARGET_LINUX),file.o) $(if $(CONFIG_LSP_DIST),,log_entity.o)
O_OBJS_$(LOCAL_LIB_RG)+=$(OBJS_COMMON_RG) $(LINUX_OBJS)

O_OBJS_$(LOCAL_LIB_JUTIL)+=$(OBJS_COMMON_JUTIL) csv.o csv_util.o \
  file.o dbi_result.o get_next_id.o mail.o md5.o str_sql.o \
  sql.o sql_file.o sql_html_edit.o sql_print.o sql_util.o \
  term.o user.o matrix.o matrix_io.o getdate.o $(if $(HAVE_CVS),cvs.o) \
  $(if $(HAVE_MYSQL),$(if $(CONFIG_RG_JPKG_SRC),,sql_mysql.o)) str_iconv.o \
  str_util.o iptables.o sed.o user_kernel_util.o \
  dbi_helper.o dbi.o dbi_init.o $(if $(HAVE_ODBC),sql_odbc.o)

CD_EXPORTED_FILES+=$(if $(CONFIG_RG_JPKG_SRC),sql_mysql.c)

ifeq ($(CONFIG_OPENRG),)
  O_OBJS_$(LOCAL_LIB_JUTIL)+=rg_version.o rg_version_jungo.o attrib_io.o date.o jtest.o
endif  

CFLAGS+=$(if $(CONFIG_TI_404_COMMON),\
  -I$(RGSRC)/os/vx/ti_404/baselinevxw/pform/inc/)
CFLAGS+=-I$(RGSRC)/pkg/openssl/crypto/des

# There are c files which are included from inside lang_compiler (e.g. file.c),
# but are not exported to the BUILDDIR at that stage:
CFLAGS_local_lang_compiler.o+=-I$(RGSRC)/pkg/include
CFLAGS_local_lang_compiler.o+=-I$(RGSRC)/pkg/util

CFLAGS_REMOVE_local_getdate.o+=-Wstrict-prototypes
CFLAGS_snprintf.o+=$(if $(CONFIG_CC_HAVE_VA_COPY),-DHAVE_VA_COPY)

ifdef CONFIG_RG_DATE
  TARGET+=rg_date
  O_OBJS_rg_date=rg_date.o
  LDLIBS_rg_date:=$(OPENRG_LDLIBS) $(LDLIBS_rg_date)
  EXPORT_AS_SRC+=rg_date.c
endif

ifdef ACTION_TEC
TARGET+=rg_usb_led
O_OBJS_rg_usb_led=rg_usb_led.o
LDLIBS_rg_usb_led:=$(OPENRG_LDLIBS) $(LDLIBS_rg_usb_led)
EXPORT_AS_SRC+=rg_usb_led.c


LOCAL_TARGET+=updcfg
O_OBJS_updcfg=rg_cfg_update.o
#CFLAGS_updcfg.o+=-I$(RGSRC)/pkg/include
LDLIBS_updcfg:=-llocal_openrg -llocal_jutil -L../openssl/crypto -llocal_crypto $(LDLIBS_updcfg)

endif

ifdef CONFIG_RG_EXEC_CLI_CMD
  TARGET+=exec_cli_cmd
  O_OBJS_exec_cli_cmd=exec_cli_cmd.o
  LDLIBS_exec_cli_cmd=-lmgt_client $(OPENRG_LDLIBS)
  RAMDISK_BIN_FILES+=exec_cli_cmd
endif

ARCHCONFIG_FIRST_TASKS+=mkconfig

ifndef CONFIG_RG_GPL
  ARCHCONFIG_LAST_TASKS+=$(if $(call VPATH_WILDCARD,lang_compiler.c),getdate.c) 
  ifdef CONFIG_RG_LANG
   ARCHCONFIG_LAST_TASKS+=lang_compiler
  endif

NOT_RG_LANG_FILES+=test_lang.csv

local_test.o test.o: test_lang.h test_lang_static.h

test_lang_static.csv: test_lang.csv
	# the following should have been 'V\?T', but this ('?')
	# does not seem to work on SunOS's /bin/sed, so it was replaced
	# by 'V*T', which is close enough for this purpose.
	sed 's/^\(V*T[^,]*\),/\1_static,/g' < $< > $@

# We need explicit make rules for the language test files, since we need to
# compile them with the --static option on/off, no matter if
# CONFIG_RG_LANG_TEST is set. The default make rule in rg.mak for *_lang.csv
# will be overrided by the following specific rules

test_lang.h: test_lang.csv
	$(LANG_COMPILER) csv2h test $< $@

test_lang.c: test_lang.csv
	$(LANG_COMPILER) csv2c test $< $@

test_lang_static.h: test_lang_static.csv
	$(LANG_COMPILER) --static csv2h test_static $< $@

test_lang_static.c: test_lang_static.csv
	$(LANG_COMPILER) --static csv2c test_static $< $@
else
LOCAL_TARGET:= $(filter %config,$(LOCAL_TARGET))    
TARGET:= $(filter %config %gpl,$(TARGET))    
SO_TARGET:= $(filter %config.so %gpl.so,$(SO_TARGET))    
A_TARGET:= $(filter %config.a %gpl.a $(MGT_LIB) ,$(A_TARGET))    
LOCAL_A_TARGET:= $(filter %config.a %gpl.a,$(LOCAL_A_TARGET))
EXPORT_LIBS:= $(filter %config.a %gpl.a %config.so %gpl.so $(MGT_LIB) ,$(EXPORT_LIBS))  
SUBDIRS=
endif
include $(RGMK)

