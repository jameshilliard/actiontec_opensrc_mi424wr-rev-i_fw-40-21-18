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
#include "mv_netdev.h"

static ssize_t mv_eth_help(char *buf)
{
	int off = 0;

	off += sprintf(buf+off, "cat                ports           - show all ports info\n");
	off += sprintf(buf+off, "echo {0|1}         > skb           - enable / disable SKB recycle\n");
	off += sprintf(buf+off, "echo p             > port          - show a port info\n");
	off += sprintf(buf+off, "echo p             > stats         - show a port statistics\n");
	off += sprintf(buf+off, "echo p mib         > cntrs         - show a port counters\n");
	off += sprintf(buf+off, "echo p             > tos           - show RX and TX TOS map for port <p>\n");
	off += sprintf(buf+off, "echo p             > mac           - show MAC info for port <p>\n");
	off += sprintf(buf+off, "echo p             > p_regs        - show port registers for <p>\n");
	off += sprintf(buf+off, "echo p rxq         > rxq_regs      - show RXQ registers for <p/rxq>\n");
	off += sprintf(buf+off, "echo p txp         > wrr_regs      - show WRR registers for <p/txp>\n");
	off += sprintf(buf+off, "echo p txp txq     > txq_regs      - show TXQ registers for <p/txp/txq>\n");
	off += sprintf(buf+off, "echo p rxq v       > rxq           - show RXQ descriptors ring for <p/rxq>. v=0-brief, v=1-full\n");
	off += sprintf(buf+off, "echo p txp txq v   > txq           - show TXQ descriptors ring for <p/txp/txq>. v=0-brief, v=1-full\n");
	off += sprintf(buf+off, "echo p {0|1}       > mh_en         - enable Marvell Header\n");
	off += sprintf(buf+off, "echo p {0|1}       > tx_nopad      - disable zero padding\n");
	off += sprintf(buf+off, "echo p hex         > mh_2B         - set 2 bytes of Marvell Header\n");
	off += sprintf(buf+off, "echo p hex         > tx_cmd        - set 4 bytes of TX descriptor offset 0xc\n");
	off += sprintf(buf+off, "echo p hex         > debug         - bit0:rx, bit1:tx, bit2:isr, bit3:poll, bit4:dump\n");
	off += sprintf(buf+off, "echo p l s         > buf_num       - set number of long <l> and short <s> buffers allocated for port <p>\n");
	off += sprintf(buf+off, "echo p rxq tos     > rxq_tos       - set <rxq> for incoming IP packets with <tos>\n");
	off += sprintf(buf+off, "echo p rxq cpus    > rxq_cpus      - set <cpus> enable to process packets incoming to <rxq>\n");
	off += sprintf(buf+off, "echo p rxq v       > rxq_size      - set number of descriptors <v> for <port/rxq>.\n");
	off += sprintf(buf+off, "echo p rxq v       > rxq_pkts_coal - set RXQ interrupt coalesing. <v> - number of received packets\n");
	off += sprintf(buf+off, "echo p rxq v       > rxq_time_coal - set RXQ interrupt coalesing. <v> - time in microseconds\n");
	off += sprintf(buf+off, "echo p             > rx_reset      - reset RX part of the port <p>\n");
	off += sprintf(buf+off, "echo p txp         > txp_reset     - reset TX part of the port <p/txp>\n");
	off += sprintf(buf+off, "echo p txq tos     > txq_tos       - set <txq> for outgoing IP packets with <tos>\n");
	off += sprintf(buf+off, "echo p txp txq cpu > txq_def       - set default <txp/txq> for packets sent to port <p> by <cpu>\n");
	off += sprintf(buf+off, "echo p txp {0|1}   > ejp           - enable/disable EJP mode for <port/txp>\n");
	off += sprintf(buf+off, "echo p txp v       > txp_rate      - set outgoing rate <v> in [kbps] for <port/txp>\n");
	off += sprintf(buf+off, "echo p txp v       > txp_burst     - set maximum burst <v> in [Bytes] for <port/txp>\n");
	off += sprintf(buf+off, "echo p txp txq v   > txq_rate      - set outgoing rate <v> in [kbps] for <port/txp/txq>\n");
	off += sprintf(buf+off, "echo p txp txq v   > txq_burst     - set maximum burst <v> in [Bytes] for <port/txp/txq>\n");
	off += sprintf(buf+off, "echo p txp txq v   > txq_wrr       - set outgoing WRR weight for <port/txp/txq>. <v=0> - fixed\n");
	off += sprintf(buf+off, "echo p txp txq v   > txq_size      - set number of descriptors <v> for <port/txp/txq>.\n");
	off += sprintf(buf+off, "echo p txp txq v   > txq_coal      - set TXP/TXQ interrupt coalesing. <v> - number of sent packets\n");

	return off;
}

static ssize_t mv_eth_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    const char      *name = attr->attr.name;
    unsigned int    p;
    int             off = 0;

    if (!capable(CAP_NET_ADMIN))
    	return -EPERM;

    if (!strcmp(name, "ports")) {
        mv_eth_status_print();

    	for(p=0; p<=CONFIG_MV_ETH_PORTS_NUM; p++) {
    		mv_eth_port_status_print(p);
    	}
    }
    else
    	off = mv_eth_help(buf);

    return off;
}

static ssize_t mv_eth_port_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	int             err;
	unsigned int    p, v;
	unsigned long   flags;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* Read port and value */
	err = p = v = 0;
	sscanf(buf, "%d %x", &p, &v);

	local_irq_save(flags);

	if (!strcmp(name, "debug")) {
		err = mv_eth_ctrl_flag(p, MV_ETH_F_DBG_RX,   v & 0x1);
		err = mv_eth_ctrl_flag(p, MV_ETH_F_DBG_TX,   v & 0x2);
		err = mv_eth_ctrl_flag(p, MV_ETH_F_DBG_ISR,  v & 0x4);
		err = mv_eth_ctrl_flag(p, MV_ETH_F_DBG_POLL, v & 0x8);
		err = mv_eth_ctrl_flag(p, MV_ETH_F_DBG_DUMP, v & 0x10);
	}
	else if (!strcmp(name, "tx_cmd")) {
		err = mv_eth_ctrl_tx_cmd(p, v);
	}
	else if (!strcmp(name, "mh_2B")) {
		err = mv_eth_ctrl_tx_mh(p, (u16)v);
	}
	else if (!strcmp(name, "mh_en")) {
		err = mv_eth_ctrl_flag(p, MV_ETH_F_MH, v);
	}
	else if (!strcmp(name, "tx_nopad")) {
		err = mv_eth_ctrl_flag(p, MV_ETH_F_NO_PAD, v);
	}
	else if (!strcmp(name, "port")) {
		mv_eth_status_print();
		mv_eth_port_status_print(p);
	}
	else if (!strcmp(name, "stats")) {
		mv_eth_port_stats_print(p);
	}
	else if (!strcmp(name, "tos")) {
		mv_eth_tos_map_show(p);
	}
	else if (!strcmp(name, "mac")) {
		mv_eth_mac_show(p);
	}
	else if (!strcmp(name, "p_regs")) {
		mvEthRegs(p);
		mvNetaPortRegs(p);
	}
	else {
		err = 1;
		printk("%s: illegal operation <%s>\n", __func__, attr->attr.name);
	}

	local_irq_restore(flags);

	if (err)
		printk("%s: error %d\n", __func__, err);

	return err ? -EINVAL : len;
}

static ssize_t mv_eth_3_hex_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	int             err;
	unsigned int    p, i, v;
	unsigned long   flags;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	err = p = i = v = 0;
	sscanf(buf, "%d %d %x", &p, &i, &v);

	local_irq_save(flags);

	if (!strcmp(name, "txq_tos")) {
		err = mv_eth_txq_tos_map_set(p, i, v);
	}
	else if (!strcmp(name, "rxq_tos")) {
		err = mv_eth_rxq_tos_map_set(p, i, v);
	}
	else if (!strcmp(name, "rxq_cpus")) {
		err = mvNetaRxqCpuMaskSet(p, i, v);
	}
	else {
		err = 1;
		printk("%s: illegal operation <%s>\n", __func__, attr->attr.name);
	}

	local_irq_restore(flags);

	return err ? -EINVAL : len;
}

static ssize_t mv_eth_3_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	int             err;
	unsigned int    p, i, v;
	unsigned long   flags;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	err = p = i = v = 0;
	sscanf(buf, "%d %d %d", &p, &i, &v);

	local_irq_save(flags);

	if (!strcmp(name, "txp_rate")) {
		err = mvNetaTxpRateSet(p, i, v);
	}
	else if (!strcmp(name, "txp_burst")) {
		err = mvNetaTxpBurstSet(p, i, v);
	}
	else if(!strcmp(name, "ejp")) {
		err = mvNetaTxpEjpSet(p, i, v);
	}
	else if (!strcmp(name, "rxq_size")) {
		err = mv_eth_ctrl_rxq_size_set(p, i, v);
	}
	else if (!strcmp(name, "rxq_pkts_coal")) {
		err = mvNetaRxqPktsCoalSet(p, i, v);
	}
	else if (!strcmp(name, "rxq_time_coal")) {
		err = mvNetaRxqTimeCoalSet(p, i, v);
	}
	else if (!strcmp(name, "rxq")) {
		mvNetaRxqShow(p, i, v);
	}
	else if (!strcmp(name, "rxq_regs")) {
		mvNetaRxqRegs(p, i);
	}
	else if (!strcmp(name, "buf_num")) {
		err = mv_eth_ctrl_port_buf_num_set(p, i, v);
	}
	else if (!strcmp(name, "rx_reset")) {
		err = mv_eth_rx_reset(p);
	}
	else if (!strcmp(name, "txp_reset")) {
		err = mv_eth_txp_reset(p, i);
	}
	else {
		err = 1;
		printk("%s: illegal operation <%s>\n", __func__, attr->attr.name);
	}

	local_irq_restore(flags);

	if (err)
		printk("%s: error %d\n", __func__, err);

	return err ? -EINVAL : len;
}

static ssize_t mv_eth_4_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	int             err;
	unsigned int    p, txp, txq, v;
	unsigned long   flags;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	err = p = txp = txq = v = 0;
	sscanf(buf, "%d %d %d %d", &p, &txp, &txq, &v);

	local_irq_save(flags);

	if (!strcmp(name, "txq_def")) {
		err = mv_eth_ctrl_txq_cpu_def(p, txp, txq, v);
	}
	else if (!strcmp(name, "cntrs")) {
		mvEthPortCounters(p, txp);
		mvEthPortRmonCounters(p, txp);
	}
	else if (!strcmp(name, "wrr_regs")) {
		mvEthTxpWrrRegs(p, txp);
	}
	else if (!strcmp(name, "txq_rate")) {
		err = mvNetaTxqRateSet(p, txp, txq, v);
	}
	else if (!strcmp(name, "txq_burst")) {
		err = mvNetaTxqBurstSet(p, txp, txq, v);
	}
	else if (!strcmp(name, "txq_wrr")) {
		if(v == 0)
			err = mvNetaTxqFixPrioSet(p, txp, txq);
		else
			err = mvNetaTxqWrrPrioSet(p, txp, txq, v);
	}
	else if (!strcmp(name, "txq_size")) {
		err = mv_eth_ctrl_txq_size_set(p, txp, txq, v);
	}
	else if (!strcmp(name, "txq_coal")) {
		mvNetaTxDonePktsCoalSet(p, txp, txq, v);
	}
	else if (!strcmp(name, "txq")) {
		mvNetaTxqShow(p, txp, txq, v);
	}
	else if (!strcmp(name, "txq_regs")) {
		mvNetaTxqRegs(p, txp, txq);
	}
	else {
		err = 1;
		printk("%s: illegal operation <%s>\n", __func__, attr->attr.name);
	}
	local_irq_restore(flags);

	if (err)
		printk("%s: error %d\n", __func__, err);

	return err ? -EINVAL : len;
}

static DEVICE_ATTR(rxq,	        S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(txq,         S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(rxq_size,    S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(rxq_pkts_coal, S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(rxq_time_coal, S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(rx_reset,    S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(txq_size,    S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(txq_coal,    S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(txq_def,     S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(txq_wrr,     S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(txq_rate,    S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(txq_burst,   S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(txp_rate,    S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(txp_burst,   S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(txp_reset,   S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(ejp,         S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(buf_num,     S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(rxq_tos,     S_IWUSR, mv_eth_show, mv_eth_3_hex_store);
static DEVICE_ATTR(rxq_cpus,    S_IWUSR, mv_eth_show, mv_eth_3_hex_store);
static DEVICE_ATTR(txq_tos,     S_IWUSR, mv_eth_show, mv_eth_3_hex_store);
static DEVICE_ATTR(mh_en,       S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(mh_2B,       S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(tx_cmd,      S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(tx_nopad,    S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(debug,       S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(wrr_regs,    S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(cntrs,       S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(port,        S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(p_regs,      S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(rxq_regs,    S_IWUSR, mv_eth_show, mv_eth_3_store);
static DEVICE_ATTR(txq_regs,    S_IWUSR, mv_eth_show, mv_eth_4_store);
static DEVICE_ATTR(mac,         S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(tos,         S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(stats,       S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(skb,	        S_IWUSR, mv_eth_show, mv_eth_port_store);
static DEVICE_ATTR(ports,       S_IRUSR, mv_eth_show, NULL);
static DEVICE_ATTR(help,        S_IRUSR, mv_eth_show, NULL);

static struct attribute *mv_eth_attrs[] = {

    &dev_attr_rxq.attr,
    &dev_attr_txq.attr,
    &dev_attr_rxq_time_coal.attr,
    &dev_attr_rx_reset.attr,
    &dev_attr_rxq_size.attr,
    &dev_attr_rxq_pkts_coal.attr,
    &dev_attr_txq_size.attr,
    &dev_attr_txq_coal.attr,
    &dev_attr_txq_def.attr,
    &dev_attr_txq_wrr.attr,
    &dev_attr_txq_rate.attr,
    &dev_attr_txq_burst.attr,
    &dev_attr_txp_rate.attr,
    &dev_attr_txp_burst.attr,
    &dev_attr_txp_reset.attr,
    &dev_attr_ejp.attr,
    &dev_attr_buf_num.attr,
    &dev_attr_rxq_cpus.attr,
    &dev_attr_rxq_tos.attr,
    &dev_attr_txq_tos.attr,
    &dev_attr_mh_en.attr,
    &dev_attr_mh_2B.attr,
    &dev_attr_tx_cmd.attr,
    &dev_attr_tx_nopad.attr,
    &dev_attr_debug.attr,
    &dev_attr_wrr_regs.attr,
    &dev_attr_p_regs.attr,
    &dev_attr_rxq_regs.attr,
    &dev_attr_txq_regs.attr,
    &dev_attr_port.attr,
    &dev_attr_stats.attr,
    &dev_attr_cntrs.attr,
    &dev_attr_ports.attr,
    &dev_attr_tos.attr,
    &dev_attr_mac.attr,
    &dev_attr_skb.attr,
    &dev_attr_help.attr,
	NULL
};

static struct attribute_group mv_eth_group = {
	.name = "gbe",
	.attrs = mv_eth_attrs,
};

int __devinit mv_eth_sysfs_init(void)
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

	err = sysfs_create_group(&pd->kobj, &mv_eth_group);
	if (err) {
		printk(KERN_INFO "sysfs group failed %d\n", err);
		goto out;
	}
out:
	return err;
}

module_init(mv_eth_sysfs_init);

MODULE_AUTHOR("Kostya Belezko");
MODULE_DESCRIPTION("sysfs for marvell GbE");
MODULE_LICENSE("GPL");

