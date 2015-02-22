/*
 * arch/arm/mach-feroceon-kw2/include/mach/memory.h
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define PHYS_OFFSET		UL(0x00000000)

/*
 * the dram is contiguous
 */
#define __virt_to_phys__is_a_macro
#define __virt_to_phys(vpage) ((vpage) - PAGE_OFFSET)
#define __phys_to_virt__is_a_macro
#define __phys_to_virt(ppage) ((ppage) + PAGE_OFFSET)

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *              address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *              to an address that the kernel can use.
 */
#define __virt_to_bus__is_a_macro
#define __virt_to_bus(x)	(x - PAGE_OFFSET)
#define __bus_to_virt__is_a_macro
#define __bus_to_virt(x)	(x + PAGE_OFFSET)

#endif
