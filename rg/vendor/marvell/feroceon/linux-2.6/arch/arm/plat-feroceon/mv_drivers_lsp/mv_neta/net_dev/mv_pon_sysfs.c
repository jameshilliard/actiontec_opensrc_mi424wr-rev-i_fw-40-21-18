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
#include <linux/netdevice.h>

#include "gbe/mvNeta.h"
#include "pnc/mvPnc.h"

#include "mv_netdev.h"

static ssize_t mv_pon_help(char *buf)
{
	int off = 0;

	off += sprintf(buf+off, "cat help                   - show this help\n");

#ifdef MV_PON_MIB_SUPPORT
	off += sprintf(buf+off, "echo mib gp  > mib_gpid    - MIB set <mib> for incoming packets with GemPID <gp>\n");
	off += sprintf(buf+off, "echo mib     > mib_def     - MIB set <mib> for incoming packets not matched any GemPID\n");
#endif /* MV_PON_MIB_SUPPORT */

	return off;
}


static ssize_t mv_pon_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int        off = 0;
	const char *name = attr->attr.name;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!strcmp(name, "help")) {
		off = mv_pon_help(buf);
	} else
		off = mv_pon_help(buf);

	return off;
}

static ssize_t mv_pon_1_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	unsigned int    v;
	unsigned long   flags;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* Read input */
	v = 0;

	sscanf(buf, "%x", &v);

	raw_local_irq_save(flags);

#ifdef MV_PON_MIB_SUPPORT
	if (!strcmp(name, "mib_def")) {
		mvNetaPonRxMibDefault(v);
	} else
#endif /* MV_PON_MIB_SUPPORT */
		printk("%s: illegal operation <%s>\n", __func__, attr->attr.name);

	raw_local_irq_restore(flags);

	return len;
}

static ssize_t mv_pon_2_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t len)
{
	const char	*name = attr->attr.name;
	unsigned int    p, v;
	unsigned long   flags;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* Read input */
	v = 0;
	sscanf(buf, "%d %x", &p, &v);

	raw_local_irq_save(flags);

#ifdef MV_PON_MIB_SUPPORT
	if (!strcmp(name, "mib_gpid")) {
		mvNetaPonRxMibGemPid(p, v);
	} else
#endif /* MV_PON_MIB_SUPPORT */
		printk("%s: illegal operation <%s>\n", __func__, attr->attr.name);

	raw_local_irq_restore(flags);

	return len;
}

static DEVICE_ATTR(mib_gpid,   S_IWUSR, mv_pon_show, mv_pon_2_store);
static DEVICE_ATTR(mib_def,    S_IWUSR, mv_pon_show, mv_pon_1_store);
static DEVICE_ATTR(help,       S_IRUSR, mv_pon_show, NULL);

static struct attribute *mv_pon_attrs[] = {
	&dev_attr_mib_def.attr,
	&dev_attr_mib_gpid.attr,
	&dev_attr_help.attr,
	NULL
};

static struct attribute_group mv_pon_group = {
	.name = "pon",
	.attrs = mv_pon_attrs,
};

int __devinit mv_pon_sysfs_init(void)
{
		int err;
		struct device *pd;

		pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
		if (!pd) {
			platform_device_register_simple("neta", -1, NULL, 0);
			pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
		}

		if (!pd) {
			printk(KERN_ERR"%s: cannot find neta device\n", __func__);
			pd = &platform_bus;
		}

		err = sysfs_create_group(&pd->kobj, &mv_pon_group);
		if (err) {
			printk(KERN_INFO "sysfs group failed %d\n", err);
			goto out;
		}
out:
		return err;
}

module_init(mv_pon_sysfs_init);

MODULE_AUTHOR("Dmitri Epshtein");
MODULE_DESCRIPTION("sysfs for Marvell PON");
MODULE_LICENSE("GPL");

