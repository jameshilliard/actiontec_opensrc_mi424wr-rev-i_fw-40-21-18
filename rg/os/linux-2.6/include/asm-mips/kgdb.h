#ifdef __KERNEL__
#ifndef _ASM_KGDB_H_
#define _ASM_KGDB_H_

#define BUFMAX			2048
#define NUMREGBYTES		(90*sizeof(long))
#define NUMCRITREGBYTES		(12*sizeof(long))
#define BREAK_INSTR_SIZE	4
#define BREAKPOINT()		__asm__ __volatile__(		\
					".globl breakinst\n\t"	\
					".set\tnoreorder\n\t"	\
					"nop\n"			\
					"breakinst:\tbreak\n\t"	\
					"nop\n\t"		\
					".set\treorder")
#define CACHE_FLUSH_IS_SAFE	0

extern int kgdb_early_setup;

#endif				/* _ASM_KGDB_H_ */
#endif				/* __KERNEL__ */
