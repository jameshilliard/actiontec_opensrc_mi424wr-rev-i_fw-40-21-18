/*
 * arch/arm/mach-ixp425/arch.c 
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

extern void ixp425_map_io(void);

extern void ixp425_init_irq(void);
 
#ifdef CONFIG_ARCH_IXP425_MATECUMBE
MACHINE_START(MATECUMBE, "Intel IXP425 Matecumbe")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_YOSEMITE
MACHINE_START(YOSEMITE, "Intel IXP425 Yosemite")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_IXDP425
MACHINE_START(IXDP425, "Intel IXP425 IXDP425")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_NAPA
MACHINE_START(NAPA, "Intel IXP425 NAPA")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_COYOTE
MACHINE_START(COYOTE, "Intel IXP425 Coyote")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_HG21
MACHINE_START(HG21, "Intel IXP425 HG21")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_BAMBOO
MACHINE_START(BAMBOO, "Intel IXP425 Bamboo")
	MAINTAINER("PCI") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_JEEVES
MACHINE_START(JEEVES, "Intel IXP425 Jeeves")
	MAINTAINER("USR") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_GTWX5800
MACHINE_START(GTWX5800, "Gemtek IXP425 WX5800")
	MAINTAINER("Gemtek") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_GTWX5711
MACHINE_START(GTWX5711, "Gemtek IXP425 WX5711")
	MAINTAINER("Gemtek") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_GTWX5715
MACHINE_START(GTWX5715, "Gemtek IXP425 WX5715")
	MAINTAINER("Gemtek") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_WAV54G
extern void wav54g_fixup(struct machine_desc *, struct param_struct *, char **, 
	struct meminfo *);

MACHINE_START(WAV54G, "Cybertan IXP425 WAV54G")
	MAINTAINER("Cybertan") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
	FIXUP(wav54g_fixup)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_KINGSCANYON
MACHINE_START(KINGSCANYON, "Interface Masters IXP425 - KingsCanyon")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_ROCKAWAYBEACH
MACHINE_START(ROCKAWAYBEACH, "Intel IXP425 - RockAwayBeach")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXP425_MONTEJADE
MACHINE_START(MONTEJADE, "Intel IXP425 Monte Jade")
	MAINTAINER("Intel - IABU") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif
#ifdef CONFIG_ARCH_IXP425_BRUCE
MACHINE_START(BRUCE, "Jabil All-In-One (Bruce)")
	MAINTAINER("Jabil") 
	/*       Memory Base, Phy IO,    Virtual IO */
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
MACHINE_END
#endif
