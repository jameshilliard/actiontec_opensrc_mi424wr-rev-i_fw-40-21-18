/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.


********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File in accordance with the terms and conditions of the General
Public License Version 2, June 1991 (the "GPL License"), a copy of which is
available along with the File in the license.txt file or by writing to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
on the worldwide web at http://www.gnu.org/licenses/gpl.txt.

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
DISCLAIMED.  The GPL License provides additional details about this warranty
disclaimer.
*******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include "mvOs.h"
#include "gbe/mvNeta.h"
#include "nfp/mvNfp.h"

#include "mv_nfp_mgr.h"
#include "net_dev/mv_netdev.h" 


static ssize_t nfp_help(char *buf)
{
	int off = 0;

	off += mvOsSPrintf(buf+off, "cat                    help    - print this help\n");
	off += mvOsSPrintf(buf+off, "cat                    stats   - print NFP_MGR statistics\n");
	off += mvOsSPrintf(buf+off, "cat                    dump    - print NFP databases\n");
	off += mvOsSPrintf(buf+off, "echo <0 | 1>         > nfp     - disable / enable NFP support\n");
	off += mvOsSPrintf(buf+off, "echo port            > pstats  - print NFP port statistics\n");
#ifdef CONFIG_MV_ETH_NFP_HWF
	off += mvOsSPrintf(buf+off, "echo rxp p txp txq   > hwf     - use <txp/txq> for NFP HWF flows from <rxp> to <p>\n"); 
#endif /* CONFIG_MV_ETH_NFP_HWF */
#ifdef NFP_SWF
	off += mvOsSPrintf(buf+off, "echo flow txp txq mh > swf_add - add entry to SWF engine\n");
	off += mvOsSPrintf(buf+off, "echo flow            > swf_del - delete entry to SWF engine\n");
#endif /* NFP_SWF */

	return off;
}

static ssize_t nfp_show(struct device *dev, 
				  struct device_attribute *attr, char *buf)
{
	const char* name = attr->attr.name;
	int off = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!strcmp(name, "dump")) {	
		nfp_mgr_dump();
	}
	else if (!strcmp(name, "stats")) {
		nfp_mgr_stats();
	}
	else
		off = nfp_help(buf);

	return off;
}

static ssize_t nfp_store(struct device *dev, 
			 struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned int err, a, b, c, d;
	const char* name = attr->attr.name;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	a = b = c = d = err = 0;
	sscanf(buf,"%x %x %x %x",&a, &b, &c, &d);

	if(!strcmp(name, "pstats")) {
		mvNfpStats(a);
	}
	else if(!strcmp(name, "nfp")) {
		mv_eth_ctrl_nfp(a);
		nfp_mgr_enable(a); 
	}
#ifdef NFP_SWF
	else
	{
		if (!strcmp(name, "swf_add")) {
			err = nfp_swf_rule_add(a, b, c, d);
		}
		else if (!strcmp(name, "swf_del")) {
			err = nfp_swf_rule_del(a);
		}
	}
#endif	/* NFP_SWF */
        else
                printk("%s: illegal operation <%s>\n", __FUNCTION__, attr->attr.name);

	return err ? -EINVAL : len;
}

#ifdef CONFIG_MV_ETH_NFP_HWF
static ssize_t nfp_hwf_store(struct device *dev, 
				   struct device_attribute *attr, const char *buf, size_t len)
{
	const char*		name = attr->attr.name;
	int			err=0, rxp=0, p=0, txp=0, txq=0;
	int			hwf_txp, hwf_txq;
	unsigned long		flags;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	sscanf(buf,"%d %d %d %d", &rxp, &p, &txp, &txq);

	local_irq_save(flags);

	if(!strcmp(name, "hwf") )
	{
		if(txq == -1)
		{
			/* Disable NFP HWF from rxp to p. Free ownership of txp/txq */
			nfp_hwf_txq_get(rxp, p, &hwf_txp, &hwf_txq);

			if(hwf_txq != -1)
			{
				mv_eth_ctrl_txq_hwf_own(p, hwf_txp, hwf_txq, -1);
				nfp_hwf_txq_set(rxp, p, -1, -1);
			}
			mvNetaHwfTxqEnable(rxp, p, hwf_txp, hwf_txq, 0);
		}
		else
		{
			/* Enable NFP HWF from rxp to p. Set txp/txq ownership to HWF from rxp */
			err = mv_eth_ctrl_txq_hwf_own(p, txp, txq, rxp);
			if(err)
			{
				printk("%s failed: p=%d, txp=%d, txq=%d\n", 
					__FUNCTION__, p, txp, txq);
			}
			else
			{
				nfp_hwf_txq_set(rxp, p, txp, txq);
				mvNetaHwfTxqEnable(rxp, p, txp, txq, 1);
			}
		}
	} 
	else
		printk("%s: illegal operation <%s>\n", __FUNCTION__, attr->attr.name);

	local_irq_restore(flags);

	return err ? -EINVAL : len;
}

static DEVICE_ATTR(hwf, S_IWUSR, nfp_show, nfp_hwf_store);
#endif /* CONFIG_MV_ETH_NFP_HWF */

#ifdef NFP_SWF
static DEVICE_ATTR(swf_add, S_IWUSR, nfp_show, nfp_store);
static DEVICE_ATTR(swf_del, S_IWUSR, nfp_show, nfp_store);
#endif /* NFP_SWF */

static DEVICE_ATTR(dump,   S_IRUSR, nfp_show, nfp_store);
static DEVICE_ATTR(stats,  S_IRUSR, nfp_show, nfp_store);
static DEVICE_ATTR(pstats, S_IWUSR, nfp_show, nfp_store);
static DEVICE_ATTR(nfp,    S_IWUSR, nfp_show, nfp_store);
static DEVICE_ATTR(help,   S_IRUSR, nfp_show, nfp_store);

static struct attribute *nfp_attrs[] = {
#ifdef CONFIG_MV_ETH_NFP_HWF
	&dev_attr_hwf.attr,
#endif /* CONFIG_MV_ETH_NFP_HWF */
#ifdef NFP_SWF
	&dev_attr_swf_add.attr,
	&dev_attr_swf_del.attr,
#endif /* NFP_SWF */
	&dev_attr_dump.attr,
	&dev_attr_stats.attr,
	&dev_attr_pstats.attr,
    &dev_attr_nfp.attr,
	&dev_attr_help.attr,
	NULL
};

static struct attribute_group nfp_group = {
	.name = "nfp",
	.attrs = nfp_attrs,
};

int __devinit nfp_sysfs_init(void)
{
		int err;
		struct device *pd;

		pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
		if (!pd) {
			platform_device_register_simple("neta", -1, NULL, 0);
			pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
		}

		if (!pd) {
			printk(KERN_ERR"%s: cannot find neta device\n", __FUNCTION__);
			pd = &platform_bus;
		}

		err = sysfs_create_group(&pd->kobj, &nfp_group);
		if (err) {
			printk(KERN_INFO "sysfs group failed %d\n", err);
			goto out;
		}
out:
		return err;
}

module_init(nfp_sysfs_init); 

MODULE_AUTHOR("Kostya Belezko");
MODULE_DESCRIPTION("NFP for Marvell NetA MV65xxx");
MODULE_LICENSE("GPL");

