/*
 * hardware.h 
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/*
 * Hardware definitions for IXP425 based systems
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H__
#define __ASM_ARCH_HARDWARE_H__

#include <asm/sizes.h>

/* common definitions for all boards */
#include "ixp425.h"
#include "gpio.h"

#define MEM_SIZE			(CONFIG_SDRAM_SIZE*1024*1024)

#define PCIO_BASE              		0
#define PCIBIOS_MIN_IO			0x0
#define	PCIBIOS_MIN_MEM			IXP425_PCI_BASE_PHYS

#define pcibios_assign_all_busses()	1

#endif  /* _ASM_ARCH_HARDWARE_H */
