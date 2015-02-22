/*
 *  linux/drivers/at91/usb/at91_usb_ohci-at91.c
 *
 *  (c) Rick Bronson
 *
 *  The outline of this code was taken from Brad Parkers <brad@heeltoe.com>
 *  original OHCI driver modifications, and reworked into a cleaner form
 *  by Russell King <rmk@arm.linux.org.uk>.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/arch/AT91RM9200_UHP.h>

/*
  NOTE:
  The following is so that we don't have to include usb-ohci.h or pci.h as the
  usb-ohci.c driver needs these routines even when the architecture
  has no PCI bus...
*/

extern int __devinit hc_add_ohci(struct pci_dev *dev, int irq, void *membase,
	    unsigned long flags, void *ohci, const char *name,
	    const char *slot_name);
extern void hc_remove_ohci(void *ohci);

static void *at91_ohci;
AT91PS_UHP ohci_regs;

static int __init at91_ohci_init(void)
{
	int ret;

	ohci_regs = ioremap(AT91_UHP_BASE, SZ_4K);
	if (!ohci_regs) {
		printk(KERN_ERR "at91_usb-ohci: ioremap failed\n");
		return -EIO;
	}

	/* Now, enable the USB clock */
	AT91_SYS->PMC_SCER = AT91C_PMC_UHP;	/* enable system clock */
	AT91_SYS->PMC_PCER = 1 << AT91C_ID_UHP;	/* enable peripheral clock */

	/* Take Hc out of reset */
	ohci_regs->UHP_HcControl = 2 << 6;

	/* Initialise the generic OHCI driver. */
	ret = hc_add_ohci((struct pci_dev *) 1, AT91C_ID_UHP,
			  (void *)ohci_regs, 0, &at91_ohci,
			  "usb-ohci", "at91");
	if (ret)
		iounmap(ohci_regs);

	return ret;
}

static void __exit at91_ohci_exit(void)
{
	hc_remove_ohci(at91_ohci);

	/* Force UHP_Hc to reset */
	ohci_regs->UHP_HcControl = 0;

	 /* Stop the USB clock. */
	AT91_SYS->PMC_SCDR = AT91C_PMC_UHP;	/* disable system clock */
	AT91_SYS->PMC_PCDR = 1 << AT91C_ID_UHP;	/* disable peripheral clock */

	iounmap(ohci_regs);
}

module_init(at91_ohci_init);
module_exit(at91_ohci_exit);
