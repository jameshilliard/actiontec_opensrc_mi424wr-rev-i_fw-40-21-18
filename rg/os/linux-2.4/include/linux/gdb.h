#ifndef _GDB_H_
#define _GDB_H_

/*
 * Copyright (C) 2001 Amit S. Kale
 */

/* gdb locks */
#define KGDB_MAX_NO_CPUS 8

extern int gdb_enter;	/* 1 = enter debugger on boot */
extern int gdb_ttyS;
extern int gdb_baud;
extern int gdb_initialized;

extern int gdb_hook(void);
extern void breakpoint(void);

typedef int     gdb_debug_hook(int trapno,
                               int signo,
                               int err_code,
                               struct pt_regs *regs);
extern gdb_debug_hook  *linux_debug_hook;

#ifndef CONFIG_MIPS
extern volatile unsigned kgdb_lock;
#endif

extern volatile int kgdb_memerr_expected;

struct console;
void gdb_console_write(struct console *co, const char *s,
				unsigned count);
void gdb_console_init(void);

extern volatile int procindebug[KGDB_MAX_NO_CPUS];

void gdb_wait(struct pt_regs *regs);

#define KGDB_ASSERT(message, condition)	do {			\
	if (!(condition)) {					\
		printk("kgdb assertion failed: %s\n", message); \
		asm ("int $0x3");				\
	}							\
} while (0)

#ifdef CONFIG_KERNEL_ASSERTS
#define KERNEL_ASSERT(message, condition) KGDB_ASSERT(message, condition)
#else
#define KERNEL_ASSERT(message, condition)
#endif

#define KA_VALID_ERRNO(errno) ((errno) > 0 && (errno) <= EMEDIUMTYPE)

#define KA_VALID_PTR_ERR(ptr) KA_VALID_ERRNO(-PTR_ERR(ptr))

#define KA_VALID_KPTR(ptr)  (!(ptr) ||	\
	       ((void *)(ptr) >= (void *)PAGE_OFFSET &&  \
	       (void *)(ptr) < ERR_PTR(-EMEDIUMTYPE)))

#define KA_VALID_PTRORERR(errptr) (KA_VALID_KPTR(errptr) || KA_VALID_PTR_ERR(errptr))

#ifndef CONFIG_SMP
#define KA_HELD_GKL()	1
#else
#define KA_HELD_GKL()	(current->lock_depth >= 0)
#endif

#endif /* _GDB_H_ */
