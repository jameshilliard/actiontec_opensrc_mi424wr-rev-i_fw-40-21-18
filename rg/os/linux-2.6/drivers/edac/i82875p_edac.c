/*
 * Intel D82875P Memory Controller kernel module
 * (C) 2003 Linux Networx (http://lnxi.com)
 * This file may be distributed under the terms of the
 * GNU General Public License.
 *
 * Written by Thayne Harbaugh
 * Contributors:
 *	Wang Zhenyu at intel.com
 *
 * $Id: i82875p_edac.c,v 1.1.1.1 2007/05/07 23:30:42 jungo Exp $
 *
 * Note: E7210 appears same as D82875P - zhenyu.z.wang at intel.com
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/pci.h>
#include <linux/pci_ids.h>

#include <linux/slab.h>

#include "edac_mc.h"


#ifndef PCI_DEVICE_ID_INTEL_82875_0
#define PCI_DEVICE_ID_INTEL_82875_0	0x2578
#endif				/* PCI_DEVICE_ID_INTEL_82875_0 */

#ifndef PCI_DEVICE_ID_INTEL_82875_6
#define PCI_DEVICE_ID_INTEL_82875_6	0x257e
#endif				/* PCI_DEVICE_ID_INTEL_82875_6 */


/* four csrows in dual channel, eight in single channel */
#define I82875P_NR_CSROWS(nr_chans) (8/(nr_chans))


/* Intel 82875p register addresses - device 0 function 0 - DRAM Controller */
#define I82875P_EAP		0x58	/* Error Address Pointer (32b)
					 *
					 * 31:12 block address
					 * 11:0  reserved
					 */

#define I82875P_DERRSYN		0x5c	/* DRAM Error Syndrome (8b)
					 *
					 *  7:0  DRAM ECC Syndrome
					 */

#define I82875P_DES		0x5d	/* DRAM Error Status (8b)
					 *
					 *  7:1  reserved
					 *  0    Error channel 0/1
					 */

#define I82875P_ERRSTS		0xc8	/* Error Status Register (16b)
					 *
					 * 15:10 reserved
					 *  9    non-DRAM lock error (ndlock)
					 *  8    Sftwr Generated SMI
					 *  7    ECC UE
					 *  6    reserved
					 *  5    MCH detects unimplemented cycle
					 *  4    AGP access outside GA
					 *  3    Invalid AGP access
					 *  2    Invalid GA translation table
					 *  1    Unsupported AGP command
					 *  0    ECC CE
					 */

#define I82875P_ERRCMD		0xca	/* Error Command (16b)
					 *
					 * 15:10 reserved
					 *  9    SERR on non-DRAM lock
					 *  8    SERR on ECC UE
					 *  7    SERR on ECC CE
					 *  6    target abort on high exception
					 *  5    detect unimplemented cyc
					 *  4    AGP access outside of GA
					 *  3    SERR on invalid AGP access
					 *  2    invalid translation table
					 *  1    SERR on unsupported AGP command
					 *  0    reserved
					 */


/* Intel 82875p register addresses - device 6 function 0 - DRAM Controller */
#define I82875P_PCICMD6		0x04	/* PCI Command Register (16b)
					 *
					 * 15:10 reserved
					 *  9    fast back-to-back - ro 0
					 *  8    SERR enable - ro 0
					 *  7    addr/data stepping - ro 0
					 *  6    parity err enable - ro 0
					 *  5    VGA palette snoop - ro 0
					 *  4    mem wr & invalidate - ro 0
					 *  3    special cycle - ro 0
					 *  2    bus master - ro 0
					 *  1    mem access dev6 - 0(dis),1(en)
					 *  0    IO access dev3 - 0(dis),1(en)
					 */

#define I82875P_BAR6		0x10	/* Mem Delays Base ADDR Reg (32b)
					 *
					 * 31:12 mem base addr [31:12]
					 * 11:4  address mask - ro 0
					 *  3    prefetchable - ro 0(non),1(pre)
					 *  2:1  mem type - ro 0
					 *  0    mem space - ro 0
					 */

/* Intel 82875p MMIO register space - device 0 function 0 - MMR space */

#define I82875P_DRB_SHIFT 26	/* 64MiB grain */
#define I82875P_DRB		0x00	/* DRAM Row Boundary (8b x 8)
					 *
					 *  7    reserved
					 *  6:0  64MiB row boundary addr
					 */

#define I82875P_DRA		0x10	/* DRAM Row Attribute (4b x 8)
					 *
					 *  7    reserved
					 *  6:4  row attr row 1
					 *  3    reserved
					 *  2:0  row attr row 0
					 *
					 * 000 =  4KiB
					 * 001 =  8KiB
					 * 010 = 16KiB
					 * 011 = 32KiB
					 */

#define I82875P_DRC		0x68	/* DRAM Controller Mode (32b)
					 *
					 * 31:30 reserved
					 * 29    init complete
					 * 28:23 reserved
					 * 22:21 nr chan 00=1,01=2
					 * 20    reserved
					 * 19:18 Data Integ Mode 00=none,01=ecc
					 * 17:11 reserved
					 * 10:8  refresh mode
					 *  7    reserved
					 *  6:4  mode select
					 *  3:2  reserved
					 *  1:0  DRAM type 01=DDR
					 */


enum i82875p_chips {
	I82875P = 0,
};


struct i82875p_pvt {
	struct pci_dev *ovrfl_pdev;
	void __iomem *ovrfl_window;
};


struct i82875p_dev_info {
	const char *ctl_name;
};


struct i82875p_error_info {
	u16 errsts;
	u32 eap;
	u8 des;
	u8 derrsyn;
	u16 errsts2;
};


static const struct i82875p_dev_info i82875p_devs[] = {
	[I82875P] = {
		     .ctl_name = "i82875p"},
};

static struct pci_dev *mci_pdev = NULL;	/* init dev: in case that AGP code
					   has already registered driver */
static int i82875p_registered = 1;

static void i82875p_get_error_info (struct mem_ctl_info *mci,
		struct i82875p_error_info *info)
{
	/*
	 * This is a mess because there is no atomic way to read all the
	 * registers at once and the registers can transition from CE being
	 * overwritten by UE.
	 */
	pci_read_config_word(mci->pdev, I82875P_ERRSTS, &info->errsts);
	pci_read_config_dword(mci->pdev, I82875P_EAP, &info->eap);
	pci_read_config_byte(mci->pdev, I82875P_DES, &info->des);
	pci_read_config_byte(mci->pdev, I82875P_DERRSYN, &info->derrsyn);
	pci_read_config_word(mci->pdev, I82875P_ERRSTS, &info->errsts2);

	pci_write_bits16(mci->pdev, I82875P_ERRSTS, 0x0081, 0x0081);

	/*
	 * If the error is the same then we can for both reads then
	 * the first set of reads is valid.  If there is a change then
	 * there is a CE no info and the second set of reads is valid
	 * and should be UE info.
	 */
	if (!(info->errsts2 & 0x0081))
		return;
	if ((info->errsts ^ info->errsts2) & 0x0081) {
		pci_read_config_dword(mci->pdev, I82875P_EAP, &info->eap);
		pci_read_config_byte(mci->pdev, I82875P_DES, &info->des);
		pci_read_config_byte(mci->pdev, I82875P_DERRSYN,
		    &info->derrsyn);
	}
}

static int i82875p_process_error_info (struct mem_ctl_info *mci,
		struct i82875p_error_info *info, int handle_errors)
{
	int row, multi_chan;

	multi_chan = mci->csrows[0].nr_channels - 1;

	if (!(info->errsts2 & 0x0081))
		return 0;

	if (!handle_errors)
		return 1;

	if ((info->errsts ^ info->errsts2) & 0x0081) {
		edac_mc_handle_ce_no_info(mci, "UE overwrote CE");
		info->errsts = info->errsts2;
	}

	info->eap >>= PAGE_SHIFT;
	row = edac_mc_find_csrow_by_page(mci, info->eap);

	if (info->errsts & 0x0080)
		edac_mc_handle_ue(mci, info->eap, 0, row, "i82875p UE");
	else
		edac_mc_handle_ce(mci, info->eap, 0, info->derrsyn, row,
				       multi_chan ? (info->des & 0x1) : 0,
				       "i82875p CE");

	return 1;
}


static void i82875p_check(struct mem_ctl_info *mci)
{
	struct i82875p_error_info info;

	debugf1("MC%d: " __FILE__ ": %s()\n", mci->mc_idx, __func__);
	i82875p_get_error_info(mci, &info);
	i82875p_process_error_info(mci, &info, 1);
}


#ifdef CONFIG_PROC_FS
extern int pci_proc_attach_device(struct pci_dev *);
#endif

static int i82875p_probe1(struct pci_dev *pdev, int dev_idx)
{
	int rc = -ENODEV;
	int index;
	struct mem_ctl_info *mci = NULL;
	struct i82875p_pvt *pvt = NULL;
	unsigned long last_cumul_size;
	struct pci_dev *ovrfl_pdev;
	void __iomem *ovrfl_window = NULL;

	u32 drc;
	u32 drc_chan;		/* Number of channels 0=1chan,1=2chan */
	u32 nr_chans;
	u32 drc_ddim;		/* DRAM Data Integrity Mode 0=none,2=edac */

	debugf0("MC: " __FILE__ ": %s()\n", __func__);

	ovrfl_pdev = pci_find_device(PCI_VEND_DEV(INTEL, 82875_6), NULL);

	if (!ovrfl_pdev) {
		/*
		 * Intel tells BIOS developers to hide device 6 which
		 * configures the overflow device access containing
		 * the DRBs - this is where we expose device 6.
		 * http://www.x86-secret.com/articles/tweak/pat/patsecrets-2.htm
		 */
		pci_write_bits8(pdev, 0xf4, 0x2, 0x2);
		ovrfl_pdev =
		    pci_scan_single_device(pdev->bus, PCI_DEVFN(6, 0));
		if (!ovrfl_pdev)
			goto fail;
	}
#ifdef CONFIG_PROC_FS
	if (!ovrfl_pdev->procent && pci_proc_attach_device(ovrfl_pdev)) {
		printk(KERN_ERR "MC: " __FILE__
		       ": %s(): Failed to attach overflow device\n",
		       __func__);
		goto fail;
	}
#endif				/* CONFIG_PROC_FS */
	if (pci_enable_device(ovrfl_pdev)) {
		printk(KERN_ERR "MC: " __FILE__
		       ": %s(): Failed to enable overflow device\n",
		       __func__);
		goto fail;
	}

	if (pci_request_regions(ovrfl_pdev, pci_name(ovrfl_pdev))) {
#ifdef CORRECT_BIOS
		goto fail;
#endif
	}
	/* cache is irrelevant for PCI bus reads/writes */
	ovrfl_window = ioremap_nocache(pci_resource_start(ovrfl_pdev, 0),
				       pci_resource_len(ovrfl_pdev, 0));

	if (!ovrfl_window) {
		printk(KERN_ERR "MC: " __FILE__
		       ": %s(): Failed to ioremap bar6\n", __func__);
		goto fail;
	}

	/* need to find out the number of channels */
	drc = readl(ovrfl_window + I82875P_DRC);
	drc_chan = ((drc >> 21) & 0x1);
	nr_chans = drc_chan + 1;
	drc_ddim = (drc >> 18) & 0x1;

	mci = edac_mc_alloc(sizeof(*pvt), I82875P_NR_CSROWS(nr_chans),
				 nr_chans);

	if (!mci) {
		rc = -ENOMEM;
		goto fail;
	}

	debugf3("MC: " __FILE__ ": %s(): init mci\n", __func__);

	mci->pdev = pdev;
	mci->mtype_cap = MEM_FLAG_DDR;

	mci->edac_ctl_cap = EDAC_FLAG_NONE | EDAC_FLAG_SECDED;
	mci->edac_cap = EDAC_FLAG_UNKNOWN;
	/* adjust FLAGS */

	mci->mod_name = BS_MOD_STR;
	mci->mod_ver = "$Revision: 1.1.1.1 $";
	mci->ctl_name = i82875p_devs[dev_idx].ctl_name;
	mci->edac_check = i82875p_check;
	mci->ctl_page_to_phys = NULL;

	debugf3("MC: " __FILE__ ": %s(): init pvt\n", __func__);

	pvt = (struct i82875p_pvt *) mci->pvt_info;
	pvt->ovrfl_pdev = ovrfl_pdev;
	pvt->ovrfl_window = ovrfl_window;

	/*
	 * The dram row boundary (DRB) reg values are boundary address
	 * for each DRAM row with a granularity of 32 or 64MB (single/dual
	 * channel operation).  DRB regs are cumulative; therefore DRB7 will
	 * contain the total memory contained in all eight rows.
	 */
	for (last_cumul_size = index = 0; index < mci->nr_csrows; index++) {
		u8 value;
		u32 cumul_size;
		struct csrow_info *csrow = &mci->csrows[index];

		value = readb(ovrfl_window + I82875P_DRB + index);
		cumul_size = value << (I82875P_DRB_SHIFT - PAGE_SHIFT);
		debugf3("MC: " __FILE__ ": %s(): (%d) cumul_size 0x%x\n",
			__func__, index, cumul_size);
		if (cumul_size == last_cumul_size)
			continue;	/* not populated */

		csrow->first_page = last_cumul_size;
		csrow->last_page = cumul_size - 1;
		csrow->nr_pages = cumul_size - last_cumul_size;
		last_cumul_size = cumul_size;
		csrow->grain = 1 << 12;	/* I82875P_EAP has 4KiB reolution */
		csrow->mtype = MEM_DDR;
		csrow->dtype = DEV_UNKNOWN;
		csrow->edac_mode = drc_ddim ? EDAC_SECDED : EDAC_NONE;
	}

	/* clear counters */
	pci_write_bits16(mci->pdev, I82875P_ERRSTS, 0x0081, 0x0081);

	if (edac_mc_add_mc(mci)) {
		debugf3("MC: " __FILE__
			": %s(): failed edac_mc_add_mc()\n", __func__);
		goto fail;
	}

	/* get this far and it's successful */
	debugf3("MC: " __FILE__ ": %s(): success\n", __func__);
	return 0;

      fail:
	if (mci)
		edac_mc_free(mci);

	if (ovrfl_window)
		iounmap(ovrfl_window);

	if (ovrfl_pdev) {
		pci_release_regions(ovrfl_pdev);
		pci_disable_device(ovrfl_pdev);
	}

	/* NOTE: the ovrfl proc entry and pci_dev are intentionally left */
	return rc;
}


/* returns count (>= 0), or negative on error */
static int __devinit i82875p_init_one(struct pci_dev *pdev,
				      const struct pci_device_id *ent)
{
	int rc;

	debugf0("MC: " __FILE__ ": %s()\n", __func__);

	printk(KERN_INFO "i82875p init one\n");
	if(pci_enable_device(pdev) < 0)
		return -EIO;
	rc = i82875p_probe1(pdev, ent->driver_data);
	if (mci_pdev == NULL)
		mci_pdev = pci_dev_get(pdev);
	return rc;
}


static void __devexit i82875p_remove_one(struct pci_dev *pdev)
{
	struct mem_ctl_info *mci;
	struct i82875p_pvt *pvt = NULL;

	debugf0(__FILE__ ": %s()\n", __func__);

	if ((mci = edac_mc_find_mci_by_pdev(pdev)) == NULL)
		return;

	pvt = (struct i82875p_pvt *) mci->pvt_info;
	if (pvt->ovrfl_window)
		iounmap(pvt->ovrfl_window);

	if (pvt->ovrfl_pdev) {
#ifdef CORRECT_BIOS
		pci_release_regions(pvt->ovrfl_pdev);
#endif				/*CORRECT_BIOS */
		pci_disable_device(pvt->ovrfl_pdev);
		pci_dev_put(pvt->ovrfl_pdev);
	}

	if (edac_mc_del_mc(mci))
		return;

	edac_mc_free(mci);
}


static const struct pci_device_id i82875p_pci_tbl[] __devinitdata = {
	{PCI_VEND_DEV(INTEL, 82875_0), PCI_ANY_ID, PCI_ANY_ID, 0, 0,
	 I82875P},
	{0,}			/* 0 terminated list. */
};

MODULE_DEVICE_TABLE(pci, i82875p_pci_tbl);


static struct pci_driver i82875p_driver = {
	.name = BS_MOD_STR,
	.probe = i82875p_init_one,
	.remove = __devexit_p(i82875p_remove_one),
	.id_table = i82875p_pci_tbl,
};


static int __init i82875p_init(void)
{
	int pci_rc;

	debugf3("MC: " __FILE__ ": %s()\n", __func__);
	pci_rc = pci_register_driver(&i82875p_driver);
	if (pci_rc < 0)
		return pci_rc;
	if (mci_pdev == NULL) {
		i82875p_registered = 0;
		mci_pdev =
		    pci_get_device(PCI_VENDOR_ID_INTEL,
				   PCI_DEVICE_ID_INTEL_82875_0, NULL);
		if (!mci_pdev) {
			debugf0("875p pci_get_device fail\n");
			return -ENODEV;
		}
		pci_rc = i82875p_init_one(mci_pdev, i82875p_pci_tbl);
		if (pci_rc < 0) {
			debugf0("875p init fail\n");
			pci_dev_put(mci_pdev);
			return -ENODEV;
		}
	}
	return 0;
}


static void __exit i82875p_exit(void)
{
	debugf3("MC: " __FILE__ ": %s()\n", __func__);

	pci_unregister_driver(&i82875p_driver);
	if (!i82875p_registered) {
		i82875p_remove_one(mci_pdev);
		pci_dev_put(mci_pdev);
	}
}


module_init(i82875p_init);
module_exit(i82875p_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Networx (http://lnxi.com) Thayne Harbaugh");
MODULE_DESCRIPTION("MC support for Intel 82875 memory hub controllers");
