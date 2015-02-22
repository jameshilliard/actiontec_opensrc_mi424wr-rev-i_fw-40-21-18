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
#include "mvCommon.h"
#include "ctrlEnv/mvCtrlEnvLib.h"

#include "gbe/mvNeta.h"

#include "pnc/mvPnc.h"
#include "pnc/mvTcam.h"

static struct tcam_entry te;

static ssize_t tcam_help(char *buf)
{
    int off = 0;
    
    off += mvOsSPrintf(buf+off, "cat <file>\n");
    off += mvOsSPrintf(buf+off, " sw_dump         - dump sw entry\n");
    off += mvOsSPrintf(buf+off, " hw_dump         - dump valid entries\n");
    off += mvOsSPrintf(buf+off, " hw_regs         - dump registers\n");
    off += mvOsSPrintf(buf+off, " hw_hits         - decode hit sequences\n");
    off += mvOsSPrintf(buf+off, " age_dump        - dump non-zero aging counters\n");
    off += mvOsSPrintf(buf+off, " age_dump_all    - dump all aging counters\n");
    off += mvOsSPrintf(buf+off, " age_scan        - dump aging Scanner log\n");
    off += mvOsSPrintf(buf+off, " age_reset       - reset all aging counters\n");
    
    off += mvOsSPrintf(buf+off, "echo a > <file>\n");
    off += mvOsSPrintf(buf+off, " hw_write        - write sw entry into tcam entry <a>\n");
    off += mvOsSPrintf(buf+off, " hw_read         - read tcam entry <a> into sw entry\n");
    off += mvOsSPrintf(buf+off, " hw_inv          - disable tcam entry <a>\n");
    off += mvOsSPrintf(buf+off, " hw_inv_all      - disable all tcam entries\n");
    off += mvOsSPrintf(buf+off, " hw_hits         - start recording for port <a>\n");
    off += mvOsSPrintf(buf+off, " age_clear       - clear aging counter for tcam entry <a>\n");
    off += mvOsSPrintf(buf+off, " age_cntr        - show aging counter for tcam entry <a>\n");
    off += mvOsSPrintf(buf+off, " s_rinfo         - set result info bit <a>\n");
    
    off += mvOsSPrintf(buf+off, "echo a b > <file>\n");
    off += mvOsSPrintf(buf+off, " t_offset_byte   - on offset <a> match value <b>\n");
    off += mvOsSPrintf(buf+off, " t_offset_mask   - on offset <a> use mask <b>\n");
    off += mvOsSPrintf(buf+off, " t_port          - match port value <a> with mask <b>\n");
    off += mvOsSPrintf(buf+off, " t_ainfo         - match ainfo value <a> with mask <b>\n");
    off += mvOsSPrintf(buf+off, " s_shift_update  - fill sram shift index <a> with value <b>\n");
    off += mvOsSPrintf(buf+off, " s_ainfo         - set ainfo value <a> with mask <b>\n");
    off += mvOsSPrintf(buf+off, " s_flowid        - fill sram flowid nibbles <b> from value <a>\n");
    off += mvOsSPrintf(buf+off, " s_flowid_nibble - fill sram flowid nibble <b> with value <a>\n");
    off += mvOsSPrintf(buf+off, " s_rinfo_extra   - set 2 bits value <a> to extra result info offset <b>\n");
    off += mvOsSPrintf(buf+off, " age_gr_set      - set group <b> of aging counter for tcam entry <a>\n");
    
    return off;
}

static ssize_t tcam_show(struct device *dev, 
				  struct device_attribute *attr, char *buf)
{
	const char* name = attr->attr.name;
	unsigned int v, m;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!strcmp(name, "t_port")) {	
		tcam_sw_get_port(&te, &v, &m);
		return mvOsSPrintf(buf, "value:0x%x mask:0x%x\n", v, m);
	}
	else if (!strcmp(name, "t_lookup")) {
		tcam_sw_get_lookup(&te, &v, &m);
		return mvOsSPrintf(buf, "value:0x%x mask:0x%x\n", v, m);
	}
	else if (!strcmp(name, "sw_dump")) 
		return tcam_sw_dump(&te, buf);
	else if (!strcmp(name, "hw_dump")) 
		return tcam_hw_dump(0);
	else if (!strcmp(name, "hw_dump_all")) 
		return tcam_hw_dump(1);
	else if (!strcmp(name, "hw_regs")) 
		mvNetaPncRegs();
	else if (!strcmp(name, "hw_hits")) 
		return tcam_hw_hits(buf);
#ifdef CONFIG_MV_ETH_PNC_AGING
	else if (!strcmp(name, "age_dump")) 
		mvPncAgingDump(0);
	else if (!strcmp(name, "age_dump_all")) 
		mvPncAgingDump(1);
	else if (!strcmp(name, "age_scan")) 
		mvPncAgingScannerDump();
	else if (!strcmp(name, "age_reset")) 
		mvPncAgingReset();
#endif /* CONFIG_MV_ETH_PNC_AGING */
	else if (!strcmp(name, "help")) 
		return tcam_help(buf);

	return 0;
}
static ssize_t tcam_store(struct device *dev, 
				   struct device_attribute *attr, const char *buf, size_t len)
{
	const char* name = attr->attr.name;
	unsigned int err=0, a=0, b=0;
	unsigned long flags;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	sscanf(buf,"%x %x",&a, &b);

	local_irq_save(flags);

	if (!strcmp(name, "hw_write")) 
		tcam_hw_write(&te, a);
	else if (!strcmp(name, "hw_read")) 
		tcam_hw_read(&te, a);
	else if (!strcmp(name, "hw_debug")) 
		tcam_hw_debug(a);
	else if (!strcmp(name, "hw_inv")) 
		tcam_hw_inv(a);
	else if (!strcmp(name, "hw_inv_all")) 
		tcam_hw_inv_all();
	else if (!strcmp(name, "hw_hits")) 
		tcam_hw_record(a);
#ifdef CONFIG_MV_ETH_PNC_AGING
	else if (!strcmp(name, "age_clear")) 
		mvPncAgingCntrClear(a);
	else if (!strcmp(name, "age_cntr")) {
		b = mvPncAgingCntrRead(a);
		printk("tid=%d: age_cntr = 0x%08x\n", a, b);
	}
#endif /* CONFIG_MV_ETH_PNC_AGING */
	else if (!strcmp(name, "sw_clear")) 
		tcam_sw_clear(&te);
	else if (!strcmp(name, "sw_text")) 
	{
		/* Remove last byte (new line) from the buffer */
		int len = strlen(buf);
		char* temp = mvOsMalloc(len + 1);

		strncpy(temp, buf, len-1);
		temp[len-1] = 0;
		tcam_sw_text(&te, temp);
		mvOsFree(temp);
	}
	else if (!strcmp(name, "t_port"))
		tcam_sw_set_port(&te, a, b);	
	else if (!strcmp(name, "t_lookup")) 
		tcam_sw_set_lookup(&te, a);
	else if (!strcmp(name, "t_ainfo_0")) 
		tcam_sw_set_ainfo(&te, 0<<a, 1<<a);
	else if (!strcmp(name, "t_ainfo_1")) 
		tcam_sw_set_ainfo(&te, 1<<a, 1<<a);
	else if (!strcmp(name, "t_ainfo")) 
		tcam_sw_set_ainfo(&te, a, b);
	else if (!strcmp(name, "t_offset_byte"))
		tcam_sw_set_byte(&te, a, b);
	else if (!strcmp(name, "t_offset_mask"))
		tcam_sw_set_mask(&te, a, b);
	else if (!strcmp(name, "s_lookup")) 
		sram_sw_set_next_lookup(&te, a);
	else if (!strcmp(name, "s_ainfo")) 
		sram_sw_set_ainfo(&te, a, b);
	else if (!strcmp(name, "s_lookup_done"))
		sram_sw_set_lookup_done(&te, a);
	else if (!strcmp(name, "s_next_lookup_shift"))
		sram_sw_set_next_lookup_shift(&te, a);	
	else if (!strcmp(name, "s_rxq")) 
		sram_sw_set_rxq(&te, a, b);
	else if (!strcmp(name, "s_shift_update"))
		sram_sw_set_shift_update(&te,a,b);
	else if (!strcmp(name, "s_rinfo")) 
		sram_sw_set_rinfo(&te, 1 << a);
	else if (!strcmp(name, "s_rinfo_extra")) 
		sram_sw_set_rinfo_extra(&te, a << (b & ~1));
	else if (!strcmp(name, "s_flowid")) 
		sram_sw_set_flowid(&te, a, b);
	else if (!strcmp(name, "s_flowid_nibble")) 
		sram_sw_set_flowid_nibble(&te, a, b);
#ifdef CONFIG_MV_ETH_PNC_AGING
	else if (!strcmp(name, "age_gr_set")) 
		mvPncAgingCntrGroupSet(a, b);
#endif /* CONFIG_MV_ETH_PNC_AGING */
	else {
		err = 1;
		printk("%s: illegal operation <%s>\n", __FUNCTION__, attr->attr.name);
	}
	local_irq_restore(flags);

	if (err) 
		printk("%s: <%s>, error %d\n", __FUNCTION__, attr->attr.name, err);
	
	return err ? -EINVAL : len;
}

static DEVICE_ATTR(hw_write,    S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(hw_read,     S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(hw_debug,    S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(hw_inv,      S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(hw_inv_all,  S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(hw_dump,     S_IRUSR, tcam_show, tcam_store);
static DEVICE_ATTR(hw_dump_all, S_IRUSR, tcam_show, tcam_store);
static DEVICE_ATTR(age_dump,    S_IRUSR, tcam_show, tcam_store);
static DEVICE_ATTR(age_dump_all,S_IRUSR, tcam_show, tcam_store);
static DEVICE_ATTR(age_scan,    S_IRUSR, tcam_show, tcam_store);
static DEVICE_ATTR(age_reset,   S_IRUSR, tcam_show, tcam_store);
static DEVICE_ATTR(hw_regs,     S_IRUSR, tcam_show, tcam_store);
static DEVICE_ATTR(hw_hits,     S_IRUSR | S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(sw_dump,     S_IRUSR, tcam_show, tcam_store);
static DEVICE_ATTR(sw_clear,    S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(sw_text,     S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(age_clear,   S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(age_cntr,    S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(age_gr_set,  S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(t_port,      S_IRUSR | S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(t_lookup,    S_IRUSR | S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(t_ainfo_0,   S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(t_ainfo_1,   S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(t_ainfo,     S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(t_offset_byte, 	S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(t_offset_mask, 	S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_lookup, 		S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_ainfo, 		S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_lookup_done, 	S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_next_lookup_shift, S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_rxq, 			S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_shift_update, 	S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_rinfo, 		S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_rinfo_extra, 	S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_flowid, 		S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(s_flowid_nibble, S_IWUSR, tcam_show, tcam_store);
static DEVICE_ATTR(help, 			S_IRUSR, tcam_show, tcam_store);

static struct attribute *pnc_attrs[] = {
    &dev_attr_hw_write.attr,
    &dev_attr_hw_read.attr,
    &dev_attr_hw_debug.attr,
    &dev_attr_hw_inv.attr,
    &dev_attr_hw_inv_all.attr,
    &dev_attr_hw_dump.attr,
    &dev_attr_hw_dump_all.attr,
    &dev_attr_age_dump.attr,
    &dev_attr_age_dump_all.attr,
    &dev_attr_age_scan.attr,
    &dev_attr_age_reset.attr,
    &dev_attr_hw_regs.attr,
    &dev_attr_hw_hits.attr,
    &dev_attr_sw_dump.attr,
    &dev_attr_sw_clear.attr,
    &dev_attr_sw_text.attr,
    &dev_attr_age_clear.attr,
    &dev_attr_age_cntr.attr,
    &dev_attr_age_gr_set.attr,
    &dev_attr_t_port.attr,
    &dev_attr_t_lookup.attr,
    &dev_attr_t_ainfo_0.attr,
    &dev_attr_t_ainfo.attr,
    &dev_attr_t_ainfo_1.attr,
    &dev_attr_t_offset_byte.attr,
    &dev_attr_t_offset_mask.attr,
    &dev_attr_s_lookup.attr,
    &dev_attr_s_ainfo.attr,
    &dev_attr_s_lookup_done.attr,
    &dev_attr_s_next_lookup_shift.attr,
    &dev_attr_s_rxq.attr,
    &dev_attr_s_shift_update.attr,
    &dev_attr_s_rinfo.attr,
    &dev_attr_s_rinfo_extra.attr,
    &dev_attr_s_flowid.attr,
    &dev_attr_s_flowid_nibble.attr,
    &dev_attr_help.attr,
    NULL
};

static struct attribute_group pnc_group = {
	.name = "pnc",
	.attrs = pnc_attrs,
};

int __devinit pnc_sysfs_init(void)
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

    err = sysfs_create_group(&pd->kobj, &pnc_group);
    if (err) {
        printk(KERN_INFO "sysfs group failed %d\n", err);
        goto out;
    }
out:
    return err;
}

module_init(pnc_sysfs_init); 

MODULE_AUTHOR("Kostya Belezko");
MODULE_DESCRIPTION("PNC for Marvell NetA MV65xxx");
MODULE_LICENSE("GPL");

