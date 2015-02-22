/*
 * arch/arm/mach-ixp2000/ixmb2800-pci.c
 *
 * PCI routines for IXDP2800 board
 *
 * Author: Jeff Daly <jeffrey.daly@intel.com>
 *
 * Copyright 2002-2003 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 * TODO: This is almost identical to IXDP2400..may consider merging the two.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/hardware.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci_auto.h>
#include <asm/arch/pci-bridge.h>

// #define DEBUG 
#ifdef DEBUG 
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif /* DEBUG */

extern void ixp2000_pci_init(void *);
extern struct pci_ops ixp2000_ops;
static struct pci_controller *hose;

/*
 * This board does not do normal PCI IRQ routing, or any
 * sort of swizzling, so we just need to check where on the
 * bus the device is and figure out what CPLD pin it's 
 * being routed to.
 */
static int __init
ixdp2800_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{

	if (npu_is_master()) {

		/*
		 * Root bus devices.  Slave NPU is only one with interrupt.
		 * Everything else, we just return -1 which is invalid.
		 */
		if(!dev->bus->self) {
			if(dev->devfn == 0x0028 )
				return IRQ_IXDP2800_INGRESS_NPU;

			return -1;
		}

		/*
		 * We have a bridge behind the PMC slot
		 * NOTE: Only INTA from the PMC slot is routed.
		 */
		if(PCI_SLOT(dev->bus->self->devfn) == 0x7 &&
			  PCI_SLOT(dev->bus->parent->self->devfn) == 0x4 &&
				  !dev->bus->parent->self->bus->parent)
				  return IRQ_IXDP2800_PMC_PCI;

		/*
		 * Device behind the first bridge
		 */
		if(PCI_SLOT(dev->bus->self->devfn) == 0x4) {
			switch(PCI_SLOT(dev->devfn)) {
				case 0x2:	// Master PMC
					return IRQ_IXDP2800_PMC_PCI;	
			
				case 0x3:	// Ethernet
					return IRQ_IXDP2800_EGRESS_NIC;

				case 0x5:	// Switch fabric
					return IRQ_IXDP2800_FABRIC;
			}
		}

		return 0;

	} else return IRQ_IXP2000_PCIB; /* Slave NIC interrupt */
}

extern void ixp2000_pci_init(void *);

void __init ixdp2800_pci_init(void *sysdata)
{
	ixp2000_pci_init(sysdata);

	*IXP2000_PCI_CMDSTAT = 0;
	*IXP2000_PCI_ADDR_EXT = 0x0000e000;

	*IXP2000_PCI_DRAM_BASE_ADDR_MASK = (0x20000000 - 1) & ~0xfffff;
	*IXP2000_PCI_SRAM_BASE_ADDR_MASK = (0x40000 - 1) & ~0x3ffff;

	DBG("allocating hose\n");
	hose = pcibios_alloc_controller();
	if (!hose)
		panic("Could not allocate PCI hose");

	hose->first_busno = 0;
	hose->last_busno = 0;
	hose->io_space.start = 0x00000000;
	hose->io_space.end = 0x0000ffff;
	hose->mem_space.start = 0xe0000000;
	hose->mem_space.end = 0xffffffff;

	/*
	 * Scan bus and exclude devices depending on whether we are the 
	 * master or slave NPU. Sigh...
	 *
	 * NOTE TO HW DESIGNERS: DO NOT PUT MULTIPLE PCI HOST CPUS
	 * ON THE SAME BUS SEGMENT. THAT'S WHAT NON-TRANSPARENT 
	 * BRIDGES ARE FOR.
	 */
	if (npu_is_master()) {
		DBG("setup BARS\n");
		*IXP2000_PCI_SDRAM_BAR = PHYS_OFFSET;

		*IXP2000_PCI_CMDSTAT = (PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY);
		/* autoconfig the bus */ 
		DBG("AUTOCONFIG\n");
		hose->last_busno = pciauto_bus_scan(hose, 0);

		/* scan the bus */
		DBG("SCANNING THE BUS\n");
		pci_scan_bus(0,&ixp2000_ops, sysdata);

		pci_exclude_device(0x01, 0x20); /* Remove slave's NIC*/
		pci_exclude_device(0x0, 0x38); 	/* Remove self XXX FIXME*/
	} else {
		/* Wait for the master NPU to enable us */
		while(!(*IXP2000_PCI_CMDSTAT & PCI_COMMAND_MASTER));
		pci_scan_bus(0,&ixp2000_ops, sysdata);
		int i;

		pci_exclude_device(0x01, 0x38);	/* Remove PMC site */
		pci_exclude_device(0x01, 0x18);	/* Remove master NIC*/
		pci_exclude_device(0x0, 0x20);	/* Remove remove 21154*/
		pci_exclude_device(0x0, 0x30);	/* Remove 21555*/
		pci_exclude_device(0x0, 0x28);	/* Remove Self XXX FIXME */
		/* 
		 * need to keep for IXA SDK support
		 */
#if 0
		pci_exclude_device(0x0, 0x38);	/* Remove Master IXP*/
#endif

		/*
		 * In case there's a bridge on the PMC, we remove everything
		 * on bus 2.
		 */
		for(i = 0; i < 0xff; i++)
			pci_exclude_device(0x2, i);
	}
}

struct hw_pci ixdp2800_pci __initdata = {
	init:		ixdp2800_pci_init,
	swizzle:	no_swizzle,
	map_irq:	ixdp2800_map_irq,
};


