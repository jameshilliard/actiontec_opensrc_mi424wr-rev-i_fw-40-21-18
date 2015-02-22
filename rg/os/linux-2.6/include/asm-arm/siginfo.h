#ifndef _ASMARM_SIGINFO_H
#define _ASMARM_SIGINFO_H

#include <asm-generic/siginfo.h>
#ifdef CONFIG_ARCH_FEROCEON
#define	FPE_FLTISN	(__SI_FAULT|9)	/* Input Subnormal (VFPv2) */
#endif

#endif
