RGSRC=../../../
include $(RGSRC)/envir.mak

A_TARGET+=libbb.a

CFLAGS+=-DL_$(@:%.o=%)

LIBBB_MSRC0=messages.c
LIBBB_MOBJ0=full_version.o memory_exhausted.o invalid_date.o io_error.o \
  read_error.o write_error.o name_longer_than_foo.o unknown.o \
  can_not_create_raw_socket.o perm_denied_are_you_root.o shadow_file.o \
  passwd_file.o group_file.o gshadow_file.o nologin_file.o securetty_file.o \
  motd_file.o msg_standard_input.o msg_standard_output.o shell_file.o

LIBBB_MSRC1=xfuncs.c
LIBBB_MOBJ1=xmalloc.o xrealloc.o xcalloc.o xstrdup.o xstrndup.o xfopen.o \
  xopen.o xread.o xread_all.o xread_char.o xferror.o xferror_stdout.o \
  xfflush_stdout.o strlen.o

LIBBB_MSRC2=printf.c
LIBBB_MOBJ2=bb_vfprintf.o bb_vprintf.o bb_fprintf.o bb_printf.o

LIBBB_MSRC3=xgetularg.c
LIBBB_MOBJ3=xgetularg_bnd_sfx.o xgetlarg_bnd_sfx.o getlarg10_sfx.o \
  xgetularg_bnd.o xgetularg10_bnd.o xgetularg10.o

LIBBB_MSRC4=safe_strtol.c
LIBBB_MOBJ4=safe_strtoi.o $(if $(CONFIG_PRINTF),safe_strtod.o) safe_strtol.o safe_strtoul.o

O_OBJS_libbb.a+=bb_asprintf.o ask_confirmation.o change_identity.o chomp.o \
  compare_string_array.o concat_path_file.o copy_file.o copyfd.o \
  correct_password.o create_icmp_socket.o create_icmp6_socket.o \
  device_open.o dump.o error_msg.o error_msg_and_die.o find_mount_point.o \
  find_pid_by_name.o find_root_device.o fgets_str.o full_read.o \
  full_write.o get_last_path_component.o get_line_from_file.o get_ug_id.o \
  get_terminal_width_height.o hash_fd.o herror_msg.o herror_msg_and_die.o \
  human_readable.o inet_common.o inode_hash.o interface.o isdirectory.o \
  kernel_version.o last_char_is.o llist_add_to.o login.o loop.o \
  make_directory.o mode_string.o module_syscalls.o mtab.o mtab_file.o \
  my_getgrgid.o my_getgrnam.o my_getpwnam.o my_getug.o my_getpwuid.o \
  obscure.o parse_mode.o parse_number.o perror_msg.o perror_msg_and_die.o \
  print_file.o get_console.o process_escape_sequence.o procps.o pwd2spwd.o \
  pw_encrypt.o qmodule.o read_package_field.o recursive_action.o \
  remove_file.o restricted_shell.o run_parts.o run_shell.o safe_read.o \
  safe_write.o safe_strncpy.o setup_environment.o simplify_path.o syscalls.o \
  trim.o u_signal_names.o vdprintf.o verror_msg.o vherror_msg.o vperror_msg.o \
  wfopen.o xconnect.o xgetcwd.o xgethostbyname.o xgethostbyname2.o \
  xreadlink.o xregcomp.o xgetlarg.o get_terminal_width_height.o \
  fclose_nonstdin.o fflush_stdout_and_exit.o getopt_ulflags.o \
  default_error_retval.o wfopen_input.o speed_table.o perror_nomsg_and_die.o \
  perror_nomsg.o skip_whitespace.o bb_askpass.o warn_ignoring_args.o \
  concat_subpath_file.o vfork_daemon_rexec.o \
  $(LIBBB_MOBJ0) $(LIBBB_MOBJ1) $(LIBBB_MOBJ2) $(LIBBB_MOBJ3) $(LIBBB_MOBJ4)
  
OTHER_OBJS=$(LIBBB_MOBJ0) $(LIBBB_MOBJ1) $(LIBBB_MOBJ2) $(LIBBB_MOBJ3) $(LIBBB_MOBJ4)

include $(RGMK)

$(LIBBB_MOBJ0): $(LIBBB_MSRC0)
$(LIBBB_MOBJ1): $(LIBBB_MSRC1)
$(LIBBB_MOBJ2): $(LIBBB_MSRC2)
$(LIBBB_MOBJ3): $(LIBBB_MSRC3)
$(LIBBB_MOBJ4): $(LIBBB_MSRC4)
