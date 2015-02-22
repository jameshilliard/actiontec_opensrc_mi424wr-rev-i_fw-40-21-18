#ifndef _CONFIG_H_GDBSERVER_
#define _CONFIG_H_GDBSERVER_

#include <libc_config_int.h>
#include <rg_config.h>

/* The following are not mentioned in pkg/include. The values are taken from
 * the values that are created when runnig ./configure with the i386 toolchain
 * (pkg/ulibc/extra/gcc-uClibc/i386-uclibc-gcc).
 */

/* Define if the target supports PTRACE_PEEKUSR for register access.  */
#define HAVE_LINUX_USRREGS 1

/* Define if the target supports PTRACE_GETREGS for register access.  */
/* This probably needs to be defined only for I386, but not sure so we only
 * undef for PPC */
#if !defined(CONFIG_PPC) && !defined(CONFIG_ARM) && !defined(CONFIG_MIPS)
#define HAVE_LINUX_REGSETS 1
#endif

/* Define if the target supports PTRACE_GETFPXREGS for extended
   register access.  */
#define HAVE_PTRACE_GETFPXREGS 1

/* Define if the prfpregset_t type is broken. */
/* #undef PRFPREGSET_T_BROKEN */

/* Define if you have the <linux/elf.h> header file.  */
#define HAVE_LINUX_ELF_H 1

/* Define if you have the <proc_service.h> header file.  */
/* #undef HAVE_PROC_SERVICE_H */

/* Define if you have the <sys/procfs.h> header file.  */
#define HAVE_SYS_PROCFS_H 1

/* Define if you have the <sys/reg.h> header file.  */
#if !defined(CONFIG_ARM) && !defined(CONFIG_MIPS)
#define HAVE_SYS_REG_H 1
#endif

/* Define if you have the <thread_db.h> header file.  */
#define HAVE_THREAD_DB_H 1

/* Define if strerror is not declared in system header files. */
/* #undef NEED_DECLARATION_STRERROR */

/* Define if <sys/procfs.h> has lwpid_t. */
#define HAVE_LWPID_T 1

/* Define if <sys/procfs.h> has psaddr_t. */
#define HAVE_PSADDR_T 1

/* Define if <sys/procfs.h> has prgregset_t. */
#define HAVE_PRGREGSET_T 1

/* Define if <sys/procfs.h> has prfpregset_t. */
#define HAVE_PRFPREGSET_T 1

/* Define if <sys/procfs.h> has elf_fpregset_t. */
#define HAVE_ELF_FPREGSET_T 1

#endif
