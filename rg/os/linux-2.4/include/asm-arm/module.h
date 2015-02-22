#ifndef _ASM_ARM_MODULE_H
#define _ASM_ARM_MODULE_H
/*
 * This file contains the arm architecture specific module code.
 */

#ifdef CONFIG_ARM_24_FAST_MODULES
#define module_map(x)		module_alloc(x)
#else
#define module_map(x)		vmalloc(x)
#endif
#define module_unmap(x)		vfree(x)
#define module_arch_init(x)	(0)
#define arch_init_modules(x)	do { } while (0)

#endif /* _ASM_ARM_MODULE_H */
