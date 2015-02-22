/*
 * drivers/char/ixp2000.c
 *
 * Author:      Naeem Afzal <naeem.m.afzal@intel.com>
 * Copyright:   (C) 2002 Intel Corp.
 * Copyright (C), 2003 MontaVista Software, Inc.
 *
 * Maintained by: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Slave IXP2000 NPU driver, Configure PCI interface to download images
 * over PCI BUS 
 *
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/fs.h>   
#include <linux/types.h> 
#include <linux/proc_fs.h>
#include <linux/fcntl.h>  
#include <linux/devfs_fs_kernel.h>  

#include <asm/uaccess.h>
#include <asm/hardware.h>

#include "ixp2000.h"

// #define DEBUG

#ifdef DEBUG
#define  DBG(x...) printk(x)
#else
#define  DBG(x...)
#endif


/*
 * If the master CPU is an IXP2000, we need to do our "special"
 * ioremap which fiddles with the PCI address extension.
 * This is a requirement for the IXASDK.  sigh...
 */
#ifdef CONFIG_ARCH_IXP2000


#define	PHY_PCI_MEM	0xe0000000
#undef	ioremap
#undef	iounmap

unsigned long pci_window_address = 0;

/*
 * ixp_ioremap...just returns the real PCI bus address
 */
static inline u32 ixp_ioremap(unsigned long address, unsigned long size)
{                                                              
	/*
	 * Subtract Linux PCI offset
	 */
	address -= 0xe0000000;

	return address;
}

#define ioremap ixp_ioremap
#define iounmap 

#undef 	readb
#undef	readw
#undef	readl
#undef	writeb
#undef	writew
#undef	writel

static inline u8 readb(unsigned long addr)
{
	unsigned long flags;
	unsigned long addr_extension;
	u8 value;

	local_irq_save(flags);
	addr_extension = *IXP2000_PCI_ADDR_EXT;

	*IXP2000_PCI_ADDR_EXT = (addr & 0xe0000000) >> 16;

	value = *(volatile u8*)((u32)pci_window_address + (addr & ~0xe0000000));

	*IXP2000_PCI_ADDR_EXT = addr_extension;

	local_irq_restore(flags);

	return value;
}

static inline u16 readw(unsigned long addr)
{
	unsigned long flags;
	unsigned long addr_extension;
	u16 value;

	local_irq_save(flags);
	addr_extension = *IXP2000_PCI_ADDR_EXT;

	*IXP2000_PCI_ADDR_EXT = (addr & 0xe0000000) >> 16;

	value = *(volatile u16*)((u32)pci_window_address + (addr & ~0xe0000000));

	*IXP2000_PCI_ADDR_EXT = addr_extension;

	local_irq_restore(flags);

	return value;
}

static inline u32 readl(unsigned long addr)
{
	unsigned long flags;
	unsigned long addr_extension;
	u32 value;

	local_irq_save(flags);
	addr_extension = *IXP2000_PCI_ADDR_EXT;

	*IXP2000_PCI_ADDR_EXT = (addr & 0xe0000000) >> 16;

	value = *(volatile u32*)((u32)pci_window_address + (addr & ~0xe0000000));

	*IXP2000_PCI_ADDR_EXT = addr_extension;

	local_irq_restore(flags);

	return value;
}


static inline void writeb(u8 value, unsigned long addr)
{
	unsigned long flags;
	unsigned long addr_extension;
	volatile u8* tmp = (u8*)((u32)pci_window_address + (addr & ~0xe0000000));

	local_irq_save(flags);
	addr_extension = *IXP2000_PCI_ADDR_EXT;

	*IXP2000_PCI_ADDR_EXT = (addr & 0xe0000000) >> 16;

	*tmp = value;

	*IXP2000_PCI_ADDR_EXT = addr_extension;

	local_irq_restore(flags);
}

static inline void writew(u16 value, unsigned long addr)
{
	unsigned long flags;
	unsigned long addr_extension;
	volatile u16* tmp = (u16*)((u32)pci_window_address + (addr & ~0xe0000000));

	local_irq_save(flags);
	addr_extension = *IXP2000_PCI_ADDR_EXT;

	*IXP2000_PCI_ADDR_EXT = (addr & 0xe0000000) >> 16;

	*tmp = value;

	*IXP2000_PCI_ADDR_EXT = addr_extension;

	local_irq_restore(flags);
}

static inline void writel(u32 value, unsigned long addr)
{
	unsigned long flags;
	unsigned long addr_extension;
	volatile u32* tmp = (u32*)((u32)pci_window_address + (addr & ~0xe0000000));

	local_irq_save(flags);
	addr_extension = *IXP2000_PCI_ADDR_EXT;

	*IXP2000_PCI_ADDR_EXT = (addr & 0xe0000000) >> 16;

	*tmp = value;

	*IXP2000_PCI_ADDR_EXT = addr_extension;

	local_irq_restore(flags);
}
#endif

static int ixp2000_pci_init (struct pci_dev *, const struct pci_device_id *);
static ssize_t ixp2000_read (struct file *,char *, size_t,loff_t *);
static ssize_t ixp2000_write (struct file *,const char *, size_t,loff_t *);
int ixp2000_open (struct inode *, struct file *);
static int ixp2000_release(struct inode *, struct file *);
static loff_t ixp2000_llseek (struct file *, loff_t, int);
static int ixp2000_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

static int ixp2000_major = 251;
static int num_slave_devices = 0;

struct ixp_info *ixp_devices;

static struct file_operations ixp2000_fops = {
	//module: THIS_MODULE,
	open:	ixp2000_open,
	write: 	ixp2000_write,
	ioctl:	ixp2000_ioctl,
	llseek:	ixp2000_llseek,
	release: ixp2000_release,
};

int ixp2000_open (struct inode *inode, struct file *filp)
{
	struct ixp_info *tmp = ixp_devices;

    	int num = MINOR(inode->i_rdev); 

    	/*  check the device number */
    	if (num >= num_slave_devices) return -ENODEV;
	while (num) {
		if (tmp->next)
			tmp = tmp->next;
		else
			return -ENODEV;
		num--;
	}
    	filp->private_data = tmp;

    	MOD_INC_USE_COUNT;
    	return 0;
}

static int ixp2000_release(struct inode *inode, struct file *filp)
{
        MOD_DEC_USE_COUNT;
        return 0;
}
ssize_t ixp2000_read (struct file *filp, char *buf, size_t count,                loff_t *f_pos) 
{
        //struct ixp_info *ixp = filp->private_data;
	return count;

} 
loff_t ixp2000_llseek (struct file *filp, loff_t off, int whence)
{
    	long newpos;
    	struct ixp_info *ixp = filp->private_data;

    	switch(whence) {
      		case 0: /* SEEK_SET */
        		newpos = off;
        		break;

      		case 1: /* SEEK_CUR */
        		newpos = filp->f_pos + off;
        		break;

      		case 2: /* SEEK_END */
        		newpos = ixp->sdram_size + off;
        		break;

      		default: /* can't happen */
        		return -EINVAL;
    	}
    	if (newpos<0 || (newpos >= ixp->sdram_size)) return -EINVAL;

    	filp->f_pos = newpos;
	DBG("seek position :0x%x\n",newpos);

    	return newpos;
}

void query_ixp(struct ixp_info *ixp)
{
	/*
	 * TODO: FIX THIS TO READ FROM DEVICE
	 */

	unsigned int ixp_names[] = {2800, 2850, 2400};

	unsigned long product_id =  le32_to_cpu(readl((ixp->csr_ioaddr + 0x04a00)));

	ixp->ixp_type = (product_id >> 8) & 0xff;
        ixp->ixp_rev = (product_id >> 4) & 0xf;
        ixp->flags = 0x0;

	printk("   IXP%d NPU, revision %d\n", ixp_names[ixp->ixp_type], ixp->ixp_rev);
}

int ixp2000_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int val;
        struct ixp_info *ixp = filp->private_data;

  	switch(cmd) {
      		case IXP_IOC_QUERY_INFO:
                  	if (copy_to_user((void *)arg, ixp, sizeof(struct ixp_info))) {
                                return -EFAULT;
                        }
                        break;
      		case IXP_IOC_RELEASE_RST: /* Set: arg points to the value */
			DBG("Relesing Slave NPU from RESET... ");

       			val = le32_to_cpu(readl(ixp->csr_ioaddr + IXP_RESET0_FRM_PCI));
        		val &= ~XSCALE_RESET_BIT;
        		writel(cpu_to_le32(val),ixp->csr_ioaddr + IXP_RESET0_FRM_PCI);
			udelay(1);

			ixp->flags |= IXP_FLAG_NPU_UP;
        		break;
		default:  
        		return -EINVAL;
 	}
        return 0;
}

static void ixp2000_slave_remove (struct pci_dev *pdev)
{
	pci_disable_device(pdev);
}

static struct pci_device_id ixp2000_slave_pci_tbl[] __devinitdata = {
        { PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_IXP2000,
                PCI_ANY_ID, PCI_ANY_ID, },
        { 0,}
};

static struct pci_driver ixp2000_slave_driver = {
        name:           "IXP2000 Slave Driver",
        id_table:       ixp2000_slave_pci_tbl,
        probe:          ixp2000_pci_init,
        remove:         ixp2000_slave_remove,
};

static int __devinit ixp2000_pci_init (struct pci_dev *pdev,
                const struct pci_device_id *ent)
{
	int val;
	struct ixp_info *newnode;
	unsigned long flags;
	unsigned long strap_options;

	num_slave_devices++;

	if (pci_enable_device(pdev))
		goto err_out_disable;

	if(num_slave_devices == 1)
		printk("IXP2000 found at bus:%d slot:%d\n" ,
				pdev->bus->number, PCI_SLOT(pdev->devfn));
	else
		printk("IXP2000 NPU (#%02d) found at bus:%d slot:%d\n" ,
				num_slave_devices - 1, pdev->bus->number, 
				PCI_SLOT(pdev->devfn));

	if (!(newnode = kmalloc(sizeof(struct ixp_info), GFP_KERNEL))) {
		printk(KERN_ERR "ixp2400: can't alloc ixp_info.\n");
		return -ENOMEM ;
	}

	memset(newnode, 0, sizeof(struct ixp_info));

	if (num_slave_devices == 1 ) 
		ixp_devices = newnode;
	else {
		struct ixp_info *tmp = ixp_devices;
		while ( tmp->next) {
			tmp = tmp->next;
		}
		tmp->next = newnode;
	}

	newnode->csr_bar = pci_resource_start(pdev, 0);
	newnode->sram_bar = pci_resource_start(pdev, 1);
	newnode->sdram_bar = pci_resource_start(pdev, 2);

	newnode->sram_size = pci_resource_len(pdev, 1);
	newnode->sdram_size = pci_resource_len(pdev, 2);

	strcpy(newnode->name,"IXP2400");

	newnode->csr_ioaddr = (unsigned long)ioremap(newnode->csr_bar, 0x100000);

#ifndef CONFIG_ARCH_IXP2000
	if (!newnode->csr_ioaddr) {
		printk (KERN_ERR "Cannot ioremap CSR MMIO region %lx @ %lx\n", (unsigned long)0x100000, newnode->csr_bar);
		
		goto err_out;
	}
#endif
	
	newnode->sram_ioaddr = (unsigned long)ioremap(newnode->sram_bar, newnode->sram_size);
#ifndef CONFIG_ARCH_IXP2000
	if (!newnode->sram_ioaddr) {
		printk (KERN_ERR "Cannot remap SRAM MMIO region %lx @ %lx\n",
						 newnode->sram_size, newnode->sram_bar);
		goto err_out_iounmap_csr;
	}
#endif
	
	newnode->sdram_ioaddr = (unsigned long)ioremap(newnode->sdram_bar, newnode->sdram_size);
#ifndef CONFIG_ARCH_IXP2000
	if (!newnode->sdram_ioaddr) {
		printk (KERN_ERR "Cannot remap SDRAM MMIO region %lx @ %lx\n",
						newnode->sdram_size, newnode->sdram_bar);
		goto err_out_iounmap_sram;
	}
#endif

	/* populate more ixp_info structure */
	query_ixp(newnode);

	printk("   %#010x bytes SRAM, %#010x bytes SDRAM\n",
			(u32)newnode->sram_size, (u32)newnode->sdram_size);

 	DBG("IXP2400 ioremaped CSR:%lx len %lx SRAM:%lx len %x SDRAM:%lx len %lx\n"
		,newnode->csr_ioaddr, (int)0x100000
		,newnode->sram_ioaddr, newnode->sram_size
		,newnode->sdram_ioaddr, newnode->sdram_size);

	strap_options = le32_to_cpu(readl(newnode->csr_ioaddr + STRAP_OPTIONS_FRM_PCI));

	DBG("STRAP_OPTIONS = %#010x\n", strap_options);

	DBG("Check for Strap options for Flash on Slave NPU\n");
	if (strap_options & CFG_BOOT_ROM){
		newnode->flags &= ~IXP_FLAG_NO_FLASH;
		newnode->flags |= IXP_FLAG_NPU_UP;
	} else { /* No flash */
		newnode->flags |= IXP_FLAG_NO_FLASH; 
		
		printk("   Device has no bootrom - reseting chip\n"); 
		
		DBG("Reset NPU\n");
		val = le32_to_cpu(readl(newnode->csr_ioaddr + IXP_RESET0_FRM_PCI));
		val |= XSCALE_RESET_BIT;
		writel(cpu_to_le32(val),newnode->csr_ioaddr + IXP_RESET0_FRM_PCI);
		udelay(100); 
		
		/* disable Flash alias on SDRAM */
		DBG("disable flash alias\n");
		val = le32_to_cpu(readl(newnode->csr_ioaddr + MISC_CONTROL_FRM_PCI));
		writel(cpu_to_le32(val|FLASH_ALIAS_DISABLE),newnode->csr_ioaddr+MISC_CONTROL_FRM_PCI);
		udelay(10);
	}

	return 0;

err_out_iounmap_sram: 
	iounmap ((void *)newnode->sram_ioaddr);
err_out_iounmap_csr: 
	iounmap ((void *)newnode->csr_ioaddr);

err_out:
	kfree(newnode);
err_out_disable:
	pci_disable_device(pdev);
	return -ENODEV;
}

ssize_t ixp2000_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	loff_t off = *f_pos;
        unsigned char *tmpwritebuf;
	unsigned char *from;
	size_t i = count;
        struct ixp_info *ixp = filp->private_data;

	off = *f_pos;

	DBG(" ixp2000_write: offset to write to: 0x%x\n",(int)off);

 	if (*f_pos > ixp->sdram_size || (*f_pos + count) > ixp->sdram_size) 
        	return -ENOSPC;

	tmpwritebuf = kmalloc(count,GFP_KERNEL);

	if (!tmpwritebuf) {
		printk("No Memory available\n");
		return -ENOMEM;
	}

        if (copy_from_user (tmpwritebuf, buf, count)) {
		kfree(tmpwritebuf);
                return -EFAULT;
        }
    	*f_pos += count;

	from = tmpwritebuf;
	while (i) {
		unsigned long to = ixp->sdram_ioaddr+off;
		i--;
		writeb(*from, to);
		from++;
		off++;
	}

	kfree(tmpwritebuf);

	return count;

}

static int __init ixp2000_slave_module_init(void)
{
    	int result = devfs_register_chrdev(ixp2000_major, "ixpctl", &ixp2000_fops);

    	if (result < 0) {
        	printk("ixpctl: unable to get major %d\n", ixp2000_major);
        	return result;
    	}

#ifdef CONFIG_ARCH_IXP2000
	pci_window_address = __ioremap(PHY_PCI_MEM, 0x20000000, 0);
	printk("pci_window_address = %#010x\n", pci_window_address);
	if(!pci_window_address) {
		printk("ixpctl: unable to __ioremap PCI memory region\n");
    		devfs_unregister_chrdev(ixp2000_major, "ixpctl");
		return -EIO;
	}
#endif

        return pci_module_init(&ixp2000_slave_driver);
}

static void __exit ixp2000_slave_module_cleanup(void)
{
    	devfs_unregister_chrdev(ixp2000_major, "ixpctl");
	
	printk("Unregistered device\n");

#ifdef CONFIG_ARCH_IXP2000
	__iounmap(pci_window_address);
	printk("iounmapped window\n");
#endif

    	pci_unregister_driver(&ixp2000_slave_driver);

	printk("unregistered driver\n");
}

module_init(ixp2000_slave_module_init);
module_exit(ixp2000_slave_module_cleanup);

MODULE_LICENSE("GPL");
EXPORT_SYMBOL(pci_window_address);


