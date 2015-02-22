/*
 * linux/include/asm-arm/arch-iop80310/hardware.h
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <linux/config.h>

#define PCIO_BASE	0x6e000000

#ifndef __ASSEMBLY__
extern unsigned int processor_id;
#endif

#define pcibios_assign_all_busses() 1

#ifdef CONFIG_ARCH_IOP310
/*
 * these are the values for the secondary PCI bus on the 80312 chip.  I will
 * have to do some fixup in the bus/dev fixup code
 */ 
#define PCIBIOS_MIN_IO		0x90010000
#define PCIBIOS_MIN_MEM		0x88000000

// Generic chipset bits
#include "iop310.h"

// Board specific
#if defined(CONFIG_ARCH_IQ80310)
#include "iq80310.h"
#endif

#ifndef __ASSEMBLY__
#define iop_is_310() ((processor_id & 0xffffe3f0) == 0x69052000)
#endif

#else	// !IOP310

#define iop_is_310() (0)

#endif

#ifdef CONFIG_ARCH_IOP321

#define PCIBIOS_MIN_IO		0x90000000
#define PCIBIOS_MIN_MEM		0x80000000

#include "iop321.h"

#if defined(CONFIG_ARCH_IQ80321) || defined(CONFIG_ARCH_IQ31244)
#include "iq80321.h"
#endif

#ifndef __ASSEMBLY__
#define iop_is_321() ((processor_id & 0xfffff7e0) == 0x69052420)
#endif

#else	// !IOP321

#define iop_is_321() (0)

#endif

#endif  /* _ASM_Arch_hardwARE_H */
