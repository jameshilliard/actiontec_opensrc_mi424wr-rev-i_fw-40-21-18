/*
	Copyright (c) 2003, Micrel Semiconductors

	Written 2003 by LIQUN RUAN

	This software may be used and distributed according to the terms of 
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice. This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author can be reached as liqun.ruan@micrel.com
	Micrel Semiconductors
	1931 Fortune Drive
	San Jose, CA 95131

	This driver is for Micrel's KS8695P SOHO Router Chipset as PCI bridge driver.

	Support and updates available at
	www.kendin.com or www.micrel.com

*/
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/hardware.h>

#include "ks8695_io.h"

void ks8695p_configure_interrupt(void);

#define	KS8695_CRCFID		0x2000
#define	KS8695_2000		0x2000
#define	KS8695_CRCFCS		0x2004
#define	KS8695_2004		0x2004
#define	KS8695_CRCFRV		0x2008
#define	KS8695_2008		0x2008
#define	KS8695_CRCFLT		0x200c
#define	KS8695_200C		0x200c
#define	KS8695_CRCBMA		0x2010
#define	KS8695_2010		0x2010
#define	KS8695_CRCBA0		0x2014
#define	KS8695_2014		0x2014

#define	KS8695_CRCSID		0x202c
#define	KS8695_202C		0x202c

#define	KS8695_CRCFIT		0x203c
#define	KS8695_203C		0x203c

/* note that PCI configuration bits are defined in pci.h already */

/* bridge configuration related registers */
#define	KS8695_PBCA			0x2100
#define	KS8695_2100			0x2100
#define	KS8695_PBCD			0x2104
#define	KS8695_2104			0x2104

/* bridge mode related registers */
#define	KS8695_PBM			0x2200
#define	KS8695_2200			0x2200
#define	KS8695_PBCS			0x2204
#define	KS8695_2204			0x2204
#define	KS8695_PMBA			0x2208
#define	KS8695_2208			0x2208
#define	KS8695_PMBAC			0x220c
#define	KS8695_220C			0x220c
#define	KS8695_PMBAM			0x2210
#define	KS8695_2210			0x2210
#define	KS8695_PMBAT			0x2214
#define	KS8695_2214			0x2214
#define	KS8695_PIOBA			0x2218
#define	KS8695_2218			0x2218
#define	KS8695_PIOBAC			0x221c
#define	KS8695_221C			0x221c
#define	KS8695_PIOBAM			0x2220
#define	KS8695_2220			0x2220
#define	KS8695_PIOBAT			0x2224
#define	KS8695_2224			0x2224

/* since there is no extra bridge in KS8695P reference board, we only need to support
 * type 0 configuration space access, but not type 1.
 *
 * Also use:
 *	IDSEL 16 for slot 1, 
 *	IDSEL 17 for slot 2,
 *	IDSEL 18 for bridge if it is configured as a guest device.
 */
#define CONFIG_CMD(dev, where)   (0x80000000 | (dev->bus->number << 16) | (dev->devfn << 8) | (where & 0xfffffffc))

static int 
ks8695p_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	u32 reg, shift;

#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s: bus=%d\n", __FUNCTION__, dev->bus->number);
#endif

	/*RLQ, actually there is not need to do shift since the caller will guarantee alignment */
	shift = where & 0x00000003;
	KS8695_WRITE(KS8695_2100, CONFIG_CMD(dev, where));
	reg = KS8695_READ(KS8695_2104);
	*value = (u8)(reg >> (shift * 8));

#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s: value=0x%02x\n", __FUNCTION__, *value);
#endif
	return PCIBIOS_SUCCESSFUL;
}

static int
ks8695p_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	u32 reg, shift;

#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s\n", __FUNCTION__);
#endif

	shift = where & 0x00000002;
	KS8695_WRITE(KS8695_2100, CONFIG_CMD(dev, where));
	reg = KS8695_READ(KS8695_2104);
	*value = (u16)(reg >> (shift * 8));

#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s: value=0x%04x\n", __FUNCTION__, *value);
#endif
	return PCIBIOS_SUCCESSFUL;
}

static int
ks8695p_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s\n", __FUNCTION__);
#endif

	KS8695_WRITE(KS8695_2100, CONFIG_CMD(dev, where));
	*value = KS8695_READ(KS8695_2104);
	
#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s: value=0x%08x\n", __FUNCTION__, *value);
#endif
	return PCIBIOS_SUCCESSFUL;
}

static int
ks8695p_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	u32 reg, shift;

#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s\n", __FUNCTION__);
#endif

	shift = where & 0x00000003;
	KS8695_WRITE(KS8695_2100, CONFIG_CMD(dev, where));
	reg = KS8695_READ(KS8695_2104);
	
	switch (shift) {
	case 3:
		reg &= 0x00ffffff;
		reg |= (u32)value << 24;
		break;

	case 2:
		reg &= 0xff00ffff;
		reg |= (u32)value << 16;
		break;

	case 1:
		reg &= 0xffff00ff;
		reg |= (u32)value << 8;
		break;

	default:
		reg &= 0xffffff00;
		reg |= (u32)value;
		break;
	}

	KS8695_WRITE(KS8695_2100, CONFIG_CMD(dev, where));
	KS8695_WRITE(KS8695_2104, reg);

	return PCIBIOS_SUCCESSFUL;
}

static int
ks8695p_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	u32 reg, shift;

#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s\n", __FUNCTION__);
#endif

	shift = where & 0x00000002;
	KS8695_WRITE(KS8695_2100, CONFIG_CMD(dev, where));
	reg = KS8695_READ(KS8695_2104);
	
	switch (shift) {
	case 2:
		reg &= 0x0000ffff;
		reg |= (u32)value << 16;
		break;

	default:
		reg &= 0xffff0000;
		reg |= (u32)value;
		break;
	}

	KS8695_WRITE(KS8695_2100, CONFIG_CMD(dev, where));
	KS8695_WRITE(KS8695_2104, reg);

	return PCIBIOS_SUCCESSFUL;
}

static int
ks8695p_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s\n", __FUNCTION__);
#endif
	KS8695_WRITE(KS8695_2100, CONFIG_CMD(dev, where));
	KS8695_WRITE(KS8695_2104, value);
	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops ks8695p_ops = {
	ks8695p_read_config_byte,
	ks8695p_read_config_word,
	ks8695p_read_config_dword,
	ks8695p_write_config_byte,
	ks8695p_write_config_word,
	ks8695p_write_config_dword,
};

static struct resource pci_mem = {
name:	"PCI memory space",
start:	KS8695P_PCI_MEM_BASE,
end:	(KS8695P_PCI_MEM_BASE - 1 + KS8695P_PCI_MEM_SIZE),
flags:	IORESOURCE_MEM,
};

static struct resource pci_io = {
name:	"PCI IO space",
start:	KS8695P_PCI_IO_BASE,
end:	KS8695P_PCI_IO_BASE + KS8695P_PCI_IO_SIZE - 1,
flags:	IORESOURCE_IO,
};

void __init ks8695p_setup_resources(struct resource **resource)
{
	if (request_resource(&iomem_resource, &pci_mem))
		printk("%s: request_resource for pci memory space failed\n", __FUNCTION__);

	if (request_resource(&ioport_resource, &pci_io))
		printk("%s: request_resource for pci IO space failed\n", __FUNCTION__);

	resource[0] = &pci_io;
	resource[1] = &pci_mem;
	resource[2] = NULL;
	resource[3] = NULL;
}

void __init ks8695p_configure_interrupt(void)
{
	u32	uReg;

#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s\n", __FUNCTION__);
#endif

	uReg = KS8695_READ(KS8695_GPIO_MODE);
	uReg &= ~0x00000001;
	KS8695_WRITE(KS8695_GPIO_MODE, uReg);

	uReg = KS8695_READ(KS8695_GPIO_CTRL);
	uReg &= ~0xf;
	uReg |= 0x8;
	KS8695_WRITE(KS8695_GPIO_CTRL, uReg);

#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s: OK\n", __FUNCTION__);
#endif
}

void __init ks8695p_init(void *sysdata)
{
#ifdef	DEBUG_THIS
	printk(KERN_INFO "%s\n", __FUNCTION__);
#endif

	/* note that we need a stage 1 initialization in .S file to set 0x202c, 
	 * before the stage 2 initialization here 
	 */
	KS8695_WRITE(KS8695_202C, 0x00010001);	/* stage 1 initialization, subid, subdevice = 0x0001 */

	/* stage 2 initialization */
	KS8695_WRITE(KS8695_2204, 0x40000000);	/* prefetch limits with 16 words, retru enable */

	/* configure memory mapping */
	KS8695_WRITE(KS8695_2208, KS8695P_PCIBG_MEM_BASE);
       //KS8695_WRITE(KS8695_220C, PMBAC_TRANS_ENABLE);		/* enable memory address translation */
	KS8695_WRITE(KS8695_2210, KS8695P_PCI_MEM_MASK);	/* mask bits */
	KS8695_WRITE(KS8695_2214, KS8695P_PCI_MEM_BASE);	/* physical memory address */

	/* configure IO mapping */
	KS8695_WRITE(KS8695_2218, KS8695P_PCIBG_IO_BASE);
        //KS8695_WRITE(KS8695_221C, PMBAC_TRANS_ENABLE);		/* enable IO address translation */
	KS8695_WRITE(KS8695_2220, KS8695P_PCI_IO_MASK);		/* mask bits */
	KS8695_WRITE(KS8695_2224, KS8695P_PCI_IO_BASE);

	ks8695p_configure_interrupt();

	pci_scan_bus(0, &ks8695p_ops, sysdata);
}

