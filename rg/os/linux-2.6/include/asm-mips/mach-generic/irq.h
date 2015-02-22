/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 by Ralf Baechle
 */
#ifndef __ASM_MACH_GENERIC_IRQ_H
#define __ASM_MACH_GENERIC_IRQ_H

#ifdef CONFIG_EXTEND_IRQ_VECTOR
#define NR_IRQS	256
#else
#define NR_IRQS	128
#endif

#endif /* __ASM_MACH_GENERIC_IRQ_H */
