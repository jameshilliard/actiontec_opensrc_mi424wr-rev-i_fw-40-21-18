/*
 *  linux/arch/arm/mach-ks8695/pci.c
 *
 *  PCI bios-type initialisation for PCI machines
 *
 *  Bits taken from various places.
 */
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/mach/pci.h>
#include <asm/hardware.h>

#define DEBUG_THIS

extern void ks8695p_init(void *);
extern void ks8695p_setup_resources(struct resource **);

static int __init ks8695p_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
#ifdef	DEBUG_THIS
	printk("%s: slot=%d, pin=%d\n", __FUNCTION__, slot, pin);
#endif
	
	return 2;

#if	0
	if (dev->devfn) return 20;	/* we use GPIO0 for interrupt, but may use it simply as indicator? */
	else
		return 0;
#endif
}

static u8 __init ks8695p_swizzle(struct pci_dev *dev, u8 *pinp)
{
	int	pin = *pinp;

#ifdef	DEBUG_THIS
	printk("%s: pin=%d\n", __FUNCTION__, pin);
#endif
	//if (0 == pin) {
		*pinp = 1;
	//}
	
	return PCI_SLOT(dev->devfn);
}

struct hw_pci ks8695p_pci __initdata = {
	setup_resources:ks8695p_setup_resources,
	init:		ks8695p_init,
	mem_offset:	0,
	swizzle:	ks8695p_swizzle,
	map_irq:	ks8695p_map_irq,
};
