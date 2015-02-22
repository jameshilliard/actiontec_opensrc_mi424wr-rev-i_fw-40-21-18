
/*
 *	Copyright (c) 2001 Red Hat, Inc. All rights reserved.
 *
 *	This software may be freely redistributed under the terms
 * 	of the GNU public license.
 * 
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 	Author: Arjan van de Ven <arjanv@redhat.com>
 *
 */


/*
 * Generic PCI driver for PCI bridges for powermanagement purposes
 *
 */

#include <linux/config.h> 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>

static struct pci_device_id bridge_pci_table[] __devinitdata = {
        {/* handle all PCI bridges */
	class:          ((PCI_CLASS_BRIDGE_PCI << 8) | 0x00),
	class_mask:     ~0,
	vendor:         PCI_ANY_ID,
	device:         PCI_ANY_ID,
	subvendor:      PCI_ANY_ID,
	subdevice:      PCI_ANY_ID,
	},
        {0,},
};

static int bridge_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static int pci_bridge_save_state_bus(struct pci_bus *bus, int force);
int pci_generic_resume_compare(struct pci_dev *pdev);

int pci_bridge_force_restore = 0;




static int __init bridge_setup(char *str)
{
	if (!strcmp(str,"force"))
		pci_bridge_force_restore = 1;
	else if (!strcmp(str,"noforce"))
		pci_bridge_force_restore = 0;
	return 0;
}

__setup("resume=",bridge_setup);


static int pci_bridge_save_state_bus(struct pci_bus *bus, int force)
{
	struct list_head *list;
	int error = 0;

	list_for_each(list, &bus->children) {
		error = pci_bridge_save_state_bus(pci_bus_b(list),force);
		if (error) return error;
	}
	list_for_each(list, &bus->devices) {
		pci_generic_suspend_save(pci_dev_b(list),0);
	}
	return 0;
}


static int pci_bridge_restore_state_bus(struct pci_bus *bus, int force)
{
	struct list_head *list;
	int error = 0;
	static int printed_warning=0;

	list_for_each(list, &bus->children) {
		error = pci_bridge_restore_state_bus(pci_bus_b(list),force);
		if (error) return error;
	}
	list_for_each(list, &bus->devices) {
		if (force)
			pci_generic_resume_restore(pci_dev_b(list));
		else {
			error = pci_generic_resume_compare(pci_dev_b(list));
			if (error && !printed_warning++) { 
				printk(KERN_WARNING "resume warning: bios doesn't restore PCI state properly\n");
				printk(KERN_WARNING "resume warning: if resume failed, try booting with resume=force\n");
			}
			if (error)
				return error;
		}
	}
	return 0;
}

static int bridge_suspend(struct pci_dev *dev, u32 force)
{
	pci_generic_suspend_save(dev,force);
	if (dev->subordinate)
		pci_bridge_save_state_bus(dev->subordinate,force);
	return 0;
}

static int bridge_resume(struct pci_dev *dev)
{

	pci_generic_resume_restore(dev);
	if (dev->subordinate)
		pci_bridge_restore_state_bus(dev->subordinate,pci_bridge_force_restore);
	return 0;
}


MODULE_DEVICE_TABLE(pci, bridge_pci_table);
static struct pci_driver bridge_ops = {
        name:           "PCI Bridge",   
        id_table:       bridge_pci_table,
        probe:          bridge_probe,    
        suspend: 	bridge_suspend,
        resume: 	bridge_resume
};

static int __devinit bridge_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	return 0;
}

static int __init bridge_init(void) 
{
        pci_register_driver(&bridge_ops);
        return 0;
}

static void __exit bridge_exit(void)
{
        pci_unregister_driver(&bridge_ops);
} 


module_init(bridge_init)
module_exit(bridge_exit)

