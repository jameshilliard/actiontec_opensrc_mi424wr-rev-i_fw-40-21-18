#Define if the `getpgrp' function takes no argument.
GETPGRP_VOID=1

#Define if system calls automatically restart after interruption by a signal.
HAVE_RESTARTABLE_SYSCALLS=1

#Define if you have <sys/wait.h> that is POSIX.1 compatible.
HAVE_SYS_WAIT_H=1

#Define if you have <sys/time.h> 
HAVE_SYS_TIME_H=1

#Define if you can safely include both <sys/time.h> and <time.h>.
TIME_WITH_SYS_TIME=1

HAVE_SYS_TIMES_H=1

#Define if you have the vprintf function.
HAVE_VPRINTF=1

HAVE_WAIT3=1

#The number of bytes in a short.
SIZEOF_SHORT=2

#The number of bytes in a int.
SIZEOF_INT=4

#The number of bytes in a long.
SIZEOF_LONG=4

#The number of bytes in a pointer to char.
SIZEOF_CHAR_P=4

#The number of bytes in a double (hopefully 8).
SIZEOF_DOUBLE=8

SIZEOF_INO_T=8
SIZEOF_OFF_T=8

##If using the C implementation of alloca, define if you know the
#   direction of stack growth for your system; otherwise it will be
#  automatically deduced at run-time.
#	STACK_DIRECTION > 0 => grows toward higher addresses
#	STACK_DIRECTION < 0 => grows toward lower addresses
#	STACK_DIRECTION = 0 => direction of growth unknown
#*/
#STACK_DIRECTION

#Define if you have the ANSI C header files.
STDC_HEADERS=1

#Define if `sys_siglist' is declared by <signal.h> or <unistd.h>.
SYS_SIGLIST_DECLARED=1

#Define if `_sys_siglist' is declared by <signal.h> or <unistd.h>.
UNDER_SYS_SIGLIST_DECLARED=1

HAVE_GETHOSTBYNAME=1

HAVE_INET_ATON=1

HAVE_GETADDRINFO=1

HAVE_GETNAMEINFO=1

#Define if you have the getrlimit function.
HAVE_GETRLIMIT=1

HAVE_GETRUSAGE=1

HAVE_GETTIMEOFDAY=1

GWINSZ_IN_SYS_IOCTL=1

FIONREAD_IN_SYS_IOCTL=1

HAVE_HASH_BANG_EXEC=1

HAVE_POSIX_SIGNALS=1

HAVE_DEV_FD=1

DEV_FD_PREFIX="/dev/fd/"

HAVE_DEV_STDIN=1

HAVE_QUAD_T=1

HAVE_STRSIGNAL=1

HAVE_SYS_ERRLIST=1

HAVE_TIMEVAL=1

#Define if you have the memmove function.
HAVE_MEMMOVE=1

HAVE_MKFIFO=1

PRINTF_DECLARED=1

HAVE_SYS_SIGLIST=1

HAVE_TIMES=1

HAVE_UNDER_SYS_SIGLIST=1

VOID_SIGHANDLER=1

TERMIOS_LDISC=1

TERMIO_LDISC=1

ULIMIT_MAXFDS=1

STRUCT_DIRENT_HAS_D_INO=1

STRUCT_DIRENT_HAS_D_FILENO=1

STRUCT_WINSIZE_IN_SYS_IOCTL=1

CAN_REDEFINE_GETENV=1

HAVE_POSIX_SIGSETJMP=1

DEFAULT_MAIL_DIRECTORY="/var/mail"

#Define if you have the bcopy function.
HAVE_BCOPY=1

#Define if you have the bzero function.
HAVE_BZERO=1

#Define if you have the confstr function.
HAVE_CONFSTR=1

#Define if you have the dup2 function.
HAVE_DUP2=1

#Define if you have the getcwd function.
HAVE_GETCWD=1

#Define if you have the getdtablesize function.
HAVE_GETDTABLESIZE=1

#Define if you have the getgroups function.
HAVE_GETGROUPS=1

#Define if you have the setgroups function.
HAVE_SETGROUPS=1

#Define if you have the gethostname function.
HAVE_GETHOSTNAME=1

#Define if you have the getpagesize function.
HAVE_GETPAGESIZE=1

#Define if you have the getpeername function.
HAVE_GETPEERNAME=1

#Define if you have the lstat function.
HAVE_LSTAT=1

#Define if you have the putenv function.
HAVE_PUTENV=1

#Define if you have the rename function.
HAVE_RENAME=1

#Define if you have the select function.
HAVE_SELECT=1

#Define if you have the setlinebuf function.
HAVE_SETLINEBUF=1

#Define if you have the setvbuf function.
HAVE_SETVBUF=1

#Define if you have the siginterrupt function.
HAVE_SIGINTERRUPT=1

#Define if you have the strnlen function.
HAVE_STRNLEN=1

#Define if you have the strlcpy function.
#glibc doesnt have strlcpy, support moved to ulibc
#HAVE_STRLCPY=1

#Define if you have the strcasecmp function.
HAVE_STRCASECMP=1

#Define if you have the strchr function.
HAVE_STRCHR=1

#Define if you have the strerror function.
HAVE_STRERROR=1

#Define if you have the strpbrk function.
HAVE_STRPBRK=1

#Define if you have the strtod function.
HAVE_STRTOD=1

#Define if you have the strtol function.
HAVE_STRTOL=1

#Define if you have the strtoul function.
HAVE_STRTOUL=1

#Define if you have the strdup function.
HAVE_STRDUP=1

#Define if you have the strndup function. 
HAVE_STRNDUP=1

#Define if you have the strftime function.
HAVE_STRFTIME=1

#Define if you have the tcgetattr function.
HAVE_TCGETATTR=1

#Define if you have the sysconf function.
HAVE_SYSCONF=1

#Define if you have the uname function.
HAVE_UNAME=1

#Define if you have the ulimit function.
HAVE_ULIMIT=1

HAVE_TTYNAME=1

#Define if you have the waitpid function.
HAVE_WAITPID=1

HAVE_TCGETPGRP=1

HAVE_STRCOLL=1

#Define if you have the vsyslog function.
HAVE_VSYSLOG=1

#Define if you have the snprintf function.
HAVE_SNPRINTF=1

#Define if you have the vsnprintf function.
HAVE_VSNPRINTF=1

#Define if you have the srand function.
HAVE_SRAND=1

#Define if you have the srandom function.
HAVE_SRANDOM=1

#Define if you have the symlink function.
HAVE_SYMLINK=1

#Define if you have the syscall function.
HAVE_SYSCALL=1

#Define if you have the utime function.
HAVE_UTIME=1

#Define if you have the utimes function.
HAVE_UTIMES=1

#Define if you have the endnetgrent function.
HAVE_ENDNETGRENT=1

#Define if you have the execl function.
HAVE_EXECL=1

#Define if you have the fchmod function.
HAVE_FCHMOD=1

#Define if you have the fchown function.
HAVE_FCHOWN=1

#Define if you have the fstat function.
HAVE_FSTAT=1

#Define if you have the getdents function.
HAVE_GETDENTS=1

#Define if you have the getgrent function.
HAVE_GETGRENT=1

#Define if you have the getgrnam function.
HAVE_GETGRNAM=1

#Define if you have the connect function.
HAVE_CONNECT=1

#Define if you have the crypt function.
HAVE_CRYPT=1

#Define if you have the glob function.
HAVE_GLOB=1

#Define if you have the grantpt function.
HAVE_GRANTPT=1

#Define if you have the link function.
HAVE_LINK=1

#Define if you have the llseek function.
HAVE_LLSEEK=1

#Define if you have the mknod function.
HAVE_MKNOD=1

#Define if you have the mktime function.
HAVE_MKTIME=1

#Define if you have the pathconf function.
HAVE_PATHCONF=1

#Define if you have the pipe function.
HAVE_PIPE=1

#Define if you have the pread function.
#HAVE_PREAD=1

#Define if you have the pututline function.
HAVE_PUTUTLINE=1

#Define if you have the pwrite function.
#HAVE_PWRITE=1

#Define if you have the readlink function.
HAVE_READLINK=1

#Define if you have the realpath function.
HAVE_REALPATH=1

#Define if you have the setpgid function.
HAVE_SETPGID=1

# Define if your struct stat has st_rdev.
HAVE_ST_RDEV=1

#Undocumented (from samba)
HAVE_VOLATILE=1
HAVE_C99_VSNPRINTF=1
HAVE_ERRNO_DECL=1
HAVE_LONGLONG=1
HAVE_UTIMBUF=1
HAVE_SIG_ATOMIC_T_TYPE=1
HAVE_SOCKLEN_T_TYPE=1
HAVE_SOCKLEN_T=1
HAVE_FCNTL_LOCK=1
HAVE_FTRUNCATE_EXTEND=1
HAVE_GETTIMEOFDAY_TZ=1
HAVE_IFACE_IFCONF=1
#removed until B13951 is cleared
#HAVE_STAT_ST_BLKSIZE=1
#HAVE_STAT_ST_BLOCKS=1
#STAT_ST_BLOCKSIZE=512
STAT_STATFS2_BSIZE=1
HAVE_DIRENT_D_OFF=1
HAVE_UT_UT_NAME=1
HAVE_UT_UT_USER=1
HAVE_UT_UT_ID=1
HAVE_UT_UT_HOST=1
HAVE_UT_UT_TIME=1
HAVE_UT_UT_TV=1
HAVE_UT_UT_TYPE=1
HAVE_UT_UT_PID=1
HAVE_UT_UT_EXIT=1
HAVE_UT_UT_ADDR=1
HAVE_SECURE_MKSTEMP=1
HAVE_ASPRINTF_DECL=1
HAVE_SNPRINTF_DECL=1
HAVE_VSNPRINTF_DECL=1
HAVE_UNIXSOCKET=1
HAVE_DEVICE_MAJOR_FN=1
HAVE_DEVICE_MINOR_FN=1

#Define if you have the __FILE__ macro
HAVE_FILE_MACRO=1

#Define if you have the __FUNCTION__ macro
HAVE_FUNCTION_MACRO=1

#Define if you have the <dirent.h> header file.
HAVE_DIRENT_H=1

#Define if you have the <limits.h> header file.
HAVE_LIMITS_H=1

#Define if you have the <stdlib.h> header file.
HAVE_STDLIB_H=1

#Define if you have the <stdarg.h> header file.
HAVE_STDARG_H=1

#Define if you have the <string.h> header file.
HAVE_STRING_H=1

#Define if you have the <memory.h> header file.
HAVE_MEMORY_H=1

#Define if you have the <sys/file.h> header file.
HAVE_SYS_FILE_H=1

#Define if you have the <sys/param.h> header file.
HAVE_SYS_PARAM_H=1

#Define if you have the <sys/resource.h> header file.
HAVE_SYS_RESOURCE_H=1

#Define if you have the <sys/select.h> header file.
HAVE_SYS_SELECT_H=1

#Define if you have the <sys/socket.h> header file.
HAVE_SYS_SOCKET_H=1

#Define if you have the <termcap.h> header file.
HAVE_TERMCAP_H=1

#Define if you have the <termio.h> header file.
HAVE_TERMIO_H=1

#Define if you have the <termios.h> header file.
HAVE_TERMIOS_H=1

#Define if you have the <unistd.h> header file.
HAVE_UNISTD_H=1

#Define if you have the <varargs.h> header file.
HAVE_VARARGS_H=1

#Define if you have the <stddef.h> header file.
HAVE_STDDEF_H=1

#Define if you have the <netdh.h> header file.
HAVE_NETDB_H=1

#Define if you have the <netinet/in.h> header file.
HAVE_NETINET_IN_H=1

#Define if you have the <arpa/ftp.h> header file.
HAVE_ARPA_FTP_H=1

#Define if you have the <arpa/inet.h> header file.
HAVE_ARPA_INET_H=1

#Define if you have the <arpa/nameser.h> header file.
HAVE_ARPA_NAMESER_H=1

#Define if you have the <arpa/telnet.h> header file.
HAVE_ARPA_TELNET_H=1
#Define if you have the <utmp.h> header file.
HAVE_UTMP_H=1

#Define if you have the <syscall.h> header file.
HAVE_SYSCALL_H=1

#Define if you have the <sys/unistd.h> header file.
HAVE_SYS_UNISTD_H=1

#Define if you have the <sys/syslog.h> header file.
HAVE_SYS_SYSLOG_H=1

#Define if you have the <sys/syscall.h> header file.
HAVE_SYS_SYSCALL_H=1

#Define if you have the <sys/shm.h> header file.
HAVE_SYS_SHM_H=1

#Define if you have the <sys/mman.h> header file.
HAVE_SYS_MMAN_H=1

#Define if you have the <sys/ipc.h> header file.
HAVE_SYS_IPC_H=1

#Define if you have the <sys/fcntl.h> header file.
HAVE_SYS_FCNTL_H=1

#Define if you have the <sys/dir.h> header file.
HAVE_SYS_DIR_H=1

#Define if you have the <stropts.h> header file.
# ulibc has broken stropts.h support (Moved to glibc)
#HAVE_STROPTS_H=1

#Define if you have the <rpc/rpc.h> header file.
HAVE_RPC_RPC_H=1

#Define if you have the <ctype.h> header file.
HAVE_CTYPE_H=1

#Define if you have the <dlfcn.h> header file.
HAVE_DLFCN_H=1

#Define if you have the <glob.h> header file.
HAVE_GLOB_H=1

#Define if you have the <linux/xqm.h> header file.
HAVE_LINUX_XQM_H=1


#added by sh-utils

GETLOADAVG_PRIVILEGED=1
HAVE___ARGZ_COUNT=1
HAVE_ARGZ_H=1
HAVE___ARGZ_NEXT=1
HAVE___ARGZ_STRINGIFY=1
HAVE_BTOWC=1

#Define if you have the chmod function.
HAVE_CHMOD=1

#Define if you have the chown function.
HAVE_CHOWN=1

#Define if you have the chroot function.
HAVE_CHROOT=1

HAVE_C_LINE=1
HAVE_DECL_FREE=1
HAVE_DECL_LSEEK=1
HAVE_DECL_MALLOC=1
HAVE_DECL_MEMCHR=1
HAVE_DECL_REALLOC=1
HAVE_DECL_STPCPY=1
HAVE_DECL_STRSTR=1
HAVE_DONE_WORKING_MALLOC_CHECK=1
HAVE_DONE_WORKING_REALLOC_CHECK=1
HAVE_ENDGRENT=1
HAVE_ENDPWENT=1
HAVE_FCNTL_H=1
HAVE_FLOAT_H=1
HAVE_FLOOR=1
HAVE_FNMATCH=1
HAVE_FTIME=1
HAVE_GETHOSTBYADDR=1
HAVE_GETHOSTID=1
HAVE_GETUSERSHELL=1
HAVE_INET_NTOA=1

#Define if you have the initgroups function.
HAVE_INITGROUPS=1

HAVE_ISASCII=1
HAVE_LCHOWN=1
HAVE_LC_MESSAGES=1
HAVE_LOCALTIME_R=1
HAVE_MALLOC_H=1
HAVE_ALLOCA_H=1
HAVE_MEMCHR=1
HAVE_MEMCPY=1
HAVE_MEMPCPY=1

#Define if you have the memset function.
HAVE_MEMSET=1

# Define if you have a working `mmap' system call.
HAVE_MMAP=1
HAVE_MODF=1
HAVE_MUNMAP=1
HAVE_NL_TYPES_H=1
HAVE_PATHS_H=1
HAVE_RINT=1
HAVE_SETHOSTNAME=1
HAVE_STIME=1
HAVE_STPCPY=1
HAVE_STRCSPN=1

#Define if you have the strerror function.
HAVE_STRERROR_R=1

#Define if you have the <strings.h> header file.
HAVE_STRINGS_H=1

HAVE_STRNCASECMP=1
HAVE_STRRCHR=1
HAVE_STRSTR=1
HAVE_STRTOUMAX=1
HAVE_STRUCT_UTIMBUF=1
HAVE_SYSINFO=1

#Define if you have the syslog function.
HAVE_SYSLOG=1

HAVE_SYS_TIMEB_H=1
HAVE_TM_ZONE=1
HAVE_UNSIGNED_LONG_LONG=1
HAVE_UT_HOST=1

#Define if you have the <utime.h> header file.
HAVE_UTIME_H=1

HAVE_UTMPNAME=1

#fileutils

HAVE_CLEARERR_UNLOCKED=1
HAVE_ERRNO_H=1
HAVE_EUIDACCESS=1
HAVE_FCHDIR=1
HAVE_FEOF_UNLOCKED=1
HAVE_FERROR_UNLOCKED=1
HAVE_FFLUSH_UNLOCKED=1
HAVE_FPUTC_UNLOCKED=1
HAVE_FREAD_UNLOCKED=1

#Define if you have the ftruncate function.
HAVE_FTRUNCATE=1

HAVE_FWRITE_UNLOCKED=1
HAVE_GETCHAR_UNLOCKED=1
HAVE_GETC_UNLOCKED=1
HAVE_GETMNTENT=1
HAVE_HASMNTOPT=1
HAVE_LONG_FILE_NAMES=1
HAVE_MEMCMP=1
HAVE_MKDIR=1
HAVE_MNTENT_H=1
HAVE_OBSTACK=1
HAVE_PUTCHAR_UNLOCKED=1
HAVE_PUTC_UNLOCKED=1
HAVE_RMDIR=1
HAVE_RPMATCH=1
HAVE_ST_BLOCKS=1
HAVE_STRVERSCMP=1

#Define if you have the <sys/ioctl.h> header file.
HAVE_SYS_IOCTL_H=1

#Define if you have the <sys/mount.h> header file.
HAVE_SYS_MOUNT_H=1

#Define if you have the <sys/statfs.h> header file.
HAVE_SYS_STATFS_H=1

#Define if you have the <sys/vfs.h> header file
HAVE_SYS_VFS_H=1

HAVE_UTIME_NULL=1
HAVE_VALUES_H=1
HAVE_WCHAR_H=1
HAVE_WCTYPE_H=1
HAVE_WORKING_READDIR=1

#ucd-snmp
HAVE_ASM_PAGE_H=1
HAVE_CPP_UNDERBAR_FUNCTION_DEFINED=1
HAVE_ERR_H=1
HAVE_VFORK=1
HAVE_GETOPT_H=1
HAVE_IF_FREENAMEINDEX=1
HAVE_IF_NAMEINDEX=1
HAVE_INDEX=1
HAVE_LINUX_HDREG_H=1

#Define if you have the <net/if.h> header file.
HAVE_NET_IF_H=1

#Define if you have the <net/if_arp.h> header file.
HAVE_NET_IF_ARP_H=1

HAVE_NETINET_IF_ETHER_H=1

#Define if you have the <netinet/in_systm.h> header file.
HAVE_NETINET_IN_SYSTM_H=1

HAVE_NETINET_IP6_H=1

#Define if you have the <netinet/ip.h> header file.
HAVE_NETINET_IP_H=1

#Define if you have the <netinet/tcp.h> header file.
HAVE_NETINET_TCP_H=1

HAVE_NETINET_UDP_H=1
HAVE_NET_ROUTE_H=1
HAVE_NLIST=1
HAVE_PTHREAD_H=1

#Define if you have the rand function.
HAVE_RAND=1

#Define if you have the random function.
HAVE_RANDOM=1

HAVE_REGCOMP=1
HAVE_REGEX_H=1
HAVE_SETMNTENT=1
HAVE_SGTTY_H=1

#Define if you have the sigblock function.
HAVE_SIGBLOCK=1

#Define if you have the sigprocmask function.
HAVE_SIGPROCMASK=1

HAVE_SIGHOLD=1
HAVE_SIGNAL=1

#Define if you have the sigset function.
HAVE_SIGSET=1

HAVE_SOCKET=1
HAVE_STATFS=1
HAVE_STRCASESTR=1

#Define if you have the <sys/cdefs.h> header file.
HAVE_SYS_CDEFS_H=1

HAVE_SYS_SOCKETVAR_H=1
HAVE_SYS_STAT_H=1
HAVE_SYS_SWAP_H=1
HAVE_SYS_USER_H=1
HAVE_TZSET=1
HAVE_HERROR=1

#Define if you have the <linux/if_packet.h> file (defines struct sockaddr_ll).
HAVE_LINUX_IF_PACKET_H=1
HAVE_STRUCT_SOCKADDR_LL=1

#Define if you have the <linux/if_ether.h> header file.
HAVE_LINUX_IF_ETHER_H=1

# mount
HAVE_NCURSES=no
HAVE_TERMCAP=no
NEED_LIBCRYPT=yes
CAN_DO_STATIC=yes
HAVE_XGETTEXT=no
HAVE_GOOD_RPC=yes

# installed as it is not PAM aware.
HAVE_PAM=no

# If HAVE_SHADOW is set to "yes", then login, chfn, chsh, newgrp, passwd,
# and vipw will not be built or installed from the login-utils
# subdirectory.  
HAVE_SHADOW=yes

# If HAVE_PASSWD is set to "yes", then passwd will not be built or
# installed from the login-utils subdirectory (but login, chfn, chsh,
# newgrp, and vipw *will* be installed).
HAVE_PASSWD=no

# If you use chfn and chsh from this package, REQUIRE_PASSWORD will require
# non-root users to enter the account password before updating /etc/passwd.
REQUIRE_PASSWORD=yes

# If you use chsh from this package, ONLY_LISTED_SHELLS will require that
# the selected shell be listed in /etc/shells -- otherwise only a warning is
# printed.  This prevents someone from setting their shell to /bin/false.
ONLY_LISTED_SHELLS=yes
# If HAVE_SYSVINIT is set to "yes", then simpleinit and shutdown will not
# be built or installed from the login-utils subdirectory.  (The shutdown
# and halt that come with the SysVinit package should be used with the init
# found in that package.)
HAVE_SYSVINIT=yes

# If HAVE_SYSVINIT_UTILS is set to "yes", then last, mesg, and wall will
# not be built or installed from the login-utils subdirectory.  (The
# shutdown and init from the SysVinit package do not depend on the last,
# mesg, and wall from that package.)
HAVE_SYSVINIT_UTILS=yes

# If HAVE_GETTY is set to "yes", then agetty will not be built or
# installed from the login-utils subdirectory.  Note that agetty can
# co-exist with other gettys, so this option should never be used.
HAVE_GETTY=no

# If USE_TTY_GROUP is set to "yes", then wall and write will be installed
# setgid to the "tty" group, and mesg will only set the group write bit.
# Note that this is only useful if login/xterm/etc. change the group of the
# user's tty to "tty" [The login in util-linux does this correctly, and
# xterm will do it correctly if X is compiled with USE_TTY_GROUP set
# properly.]
USE_TTY_GROUP=yes

# If HAVE_RESET is set to "yes", then reset won't be installed.  The version
# of reset that comes with the ncurses package is less aggressive.
HAVE_RESET=yes

# If HAVE_SLN is set to "yes", then sln won't be installed
# (but the man page sln.8 will be installed anyway).
# sln also comes with libc and glibc.
HAVE_SLN=no

# If HAVE_TSORT is set to "yes", then tsort won't be installed.
# GNU textutils 2.0 includes tsort.
HAVE_TSORT=no

# If HAVE_FDUTILS is set to "yes", then setfdprm won't be installed.
HAVE_FDUTILS=no

# util-linux 
NEED_tqueue_h=1
HAVE_tm_gmtoff=1

# libpopt
PROTOTYPES=1

# ucd_snmp

#Define if you have the usleep function.
HAVE_USLEEP=1

STRUCT_RTENTRY_HAS_RT_DST=1
STRUCT_IFNET_HAS_IF_SPEED=1
STRUCT_IFNET_HAS_IF_TYPE=1
STRUCT_IFNET_HAS_IF_OBYTES=1
STRUCT_IFNET_HAS_IF_IBYTES=1
STRUCT_IFADDR_HAS_IFA_NEXT=1

# thttpd
HAVE_SYS_POLL_H=1

#Define if you have the setsid function.
HAVE_SETSID=1

HAVE_POLL=1

#net-tools
HAVE_AFUNIX=1
HAVE_AFINET=1
HAVE_HWETHER=1
HAVE_HWPPP=1
HAVE_FW_MASQUERADE=1

#gdbm
HAVE_ST_BLKSIZE=1
HAVE_FLOCK=1

#Define if you have the fsync function.
HAVE_FSYNC=1

#Define if you have the atexit function.
HAVE_ATEXIT=1

HAVE_CRYPT_H=1
HAVE_ON_EXIT=1
HAVE_SIGLONGJMP=1

#heimdal

#Define if you have the asprintf function.
HAVE_ASPRINTF=1

#Define if you have the vasprintf function. 
HAVE_VASPRINTF=1

HAVE_GETEGID=1
HAVE_GETEUID=1
HAVE_GETGID=1
HAVE_INET_NTOP=1
HAVE_INET_PTON=1
HAVE_PATHS_H=1
HAVE_SETITIMER=1
HAVE_SETREGID=1
HAVE_SETREUID=1
HAVE_SETSOCKOPT=1

#Define if you have the sigaction function.
HAVE_SIGACTION=1

HAVE_SIGNAL_H=1
HAVE_STRSEP=1
HAVE_STRTOK_R=1
HAVE_STRUCT_SOCKADDR=1
HAVE_STRUCT_TM_TM_GMTOFF=1
HAVE_STRUCT_TM_TM_ZONE=1
HAVE_SYS_BITYPES_H=1
HAVE_SYS_TYPES_H=1
HAVE_SYS_UIO_H=1
HAVE_TIME_H=1
HAVE_TIMEZONE=1
HAVE_INTTYPES_H=1
HAVE_C_BIGENDIAN=1
