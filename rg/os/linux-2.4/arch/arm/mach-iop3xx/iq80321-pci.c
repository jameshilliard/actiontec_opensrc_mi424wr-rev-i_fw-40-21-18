/*
 * arch/arm/mach-iop3xx/iq80321-pci.c
 *
 * PCI support for the Intel IQ80321 reference board
 *
 * Author: Rory Bolt <rorybolt@pacbell.net>
 * Copyright (C) 2002 Rory Bolt
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/hardware.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci_auto.h>
#include <asm/arch/pci-bridge.h>
#include <asm/arch/pci_iop321.h>

#undef DEBUG
#ifdef DEBUG
#define  DBG(x...) printk(x)
#else
#define  DBG(x...)
#endif

static struct pci_controller *hose = NULL;

void iq80321_init(void*);
int iq80321_map_irq(struct pci_dev *, unsigned char, unsigned char);


struct hw_pci iq80321_pci __initdata = {
	init:		iq80321_init,
	swizzle:	common_swizzle,
	map_irq:	iq80321_map_irq,
};


void __init
iq80321_init(void *data)
{
	struct pci_bus *bus;
	struct pci_sys_data *sysdata = (struct pci_sys_data *)data;
	struct resource *mem =
		kmalloc(sizeof(struct resource), GFP_KERNEL);
	unsigned long pci_mem_start = 
		*IOP321_IABAR1 & PCI_BASE_ADDRESS_MEM_MASK;

	if(!mem)
		printk(KERN_ERR "Could not allocate PCI resource\n");

	/* IOP321 default setup */
	iop321_init();

	hose = pcibios_alloc_controller();

	if(!hose)
		panic("Could not allocate PCI hose");

	hose->first_busno = (*IOP321_PCIXSR >> 8) & 0xff;

	if (hose->first_busno == 0xff)
		hose->first_busno = 0;

	hose->last_busno = 0xff;
	hose->cfg_addr = (unsigned int *)IOP321_OCCAR;
	hose->cfg_data = (unsigned char *)IOP321_OCCDR;
	hose->io_space.start = IOP321_PCI_LOWER_IO;
	hose->io_space.end = IOP321_PCI_UPPER_IO;

	/*
	 * Since the IQ80321 is a slave card on a PC, we use 
	 * BAR1 to reserve a portion of PCI memory space for
	 * use with the private devices on the secondary bus
	 * (GigE and PCI-X slot). We read BAR1 and configure
	 * our outbound translation windows to target that
	 * address range and assign all devices in that 
	 * address range. Note that the same cannot be done
	 * with I/O space, so hopefully the host will stick to
	 * the lower 64K for PCI I/O and leave us alone.
	 */
	hose->mem_space.start = pci_mem_start;
	hose->mem_space.end = pci_mem_start + IOP321_PCI_WINDOW_SIZE;
	sysdata->mem_offset = IOP321_PCI_LOWER_MEM - pci_mem_start;

	/* Autoconfig the hose */
	hose->last_busno = pciauto_bus_scan(hose, hose->first_busno);


	if(mem)
	{
		mem->flags = IORESOURCE_MEM;
		mem->name = "IOP321 ATU";
		allocate_resource(&iomem_resource, mem,
					IOP321_PCI_WINDOW_SIZE,
					IOP321_PCI_LOWER_MEM,
					0xffffffff,
					IOP321_PCI_LOWER_MEM 
						& 0xff000000,
					NULL, NULL);
	}

	/* Scan the hose */
	bus = pci_scan_bus(hose->first_busno, &iop321_ops, sysdata);
}

#define INTA	IRQ_IQ80321_INTA
#define INTB	IRQ_IQ80321_INTB
#define INTC	IRQ_IQ80321_INTC
#define INTD	IRQ_IQ80321_INTD

#define INTE	IRQ_IQ80321_I82544

static inline int __init
iq80321_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	char ret;

	static char pci_irq_table[3][4] = 
		{
			/*
			 * PCI IDSEL/INTPIN->INTLINE
			 * A       B       C       D
			 */
			{INTE, INTE, INTE, INTE}, /* Gig-E */
			{INTD, INTC, INTD, INTA}, /* Unused */
			{INTC, INTD, INTA, INTB}, /* PCI-X Slot */
		};
	ret = pci_irq_table[idsel%4][pin-1];
	return ret;
}

