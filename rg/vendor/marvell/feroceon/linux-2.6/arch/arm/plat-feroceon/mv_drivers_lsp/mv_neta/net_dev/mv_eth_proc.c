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
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/reboot.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/seq_file.h>

#include <asm/system.h>
#include <asm/dma.h>
#include <asm/io.h>

#include <linux/netdevice.h>
#include "ctrlEnv/mvCtrlEnvLib.h"

#include "gbe/mvNeta.h"
#include "pmt/mvPmt.h"

#ifdef CONFIG_MV_ETH_BM
#include "bm/mvBm.h"
#endif

#ifdef CONFIG_MV_ETH_NFP
#include "nfp_mgr/mv_nfp_mgr.h"
#endif

#include "mv_switch.h"

#include "mv_eth_proc.h"
#include "mv_netdev.h"


//#define MV_DEBUG
#ifdef MV_DEBUG
#define DP printk
#else
#define DP(fmt,args...)
#endif


/* global variables from 'regdump' */
static struct proc_dir_entry *mv_eth_tool;

static unsigned int port = 0, txp=0, q = 0, status = 0;
static unsigned int command = 0;
static unsigned int value = 0;
static char name[20] = {'\0', };


#ifndef CONFIG_MV_ETH_PNC
void run_com_rxq_type(void)
{
    void* port_hndl = mvNetaPortHndlGet(port);

    if(port_hndl == NULL)
        return;

    switch(value) {
	case PT_BPDU:
		mvNetaBpduRxq(port, q);
		break;
	case PT_ARP:
		mvNetaArpRxq(port, q);
		break;
	case PT_TCP:
		mvNetaTcpRxq(port, q);
		break;
	case PT_UDP:
		mvNetaUdpRxq(port, q);
		break;
	default:
		printk("eth proc unknown packet type: value=%d\n", value);
    }
}
#endif /* CONFIG_MV_ETH_PNC */


void run_com_stats(const char *buffer)
{
    int scan_count;

    scan_count = sscanf(buffer, STATUS_CMD_STRING, STATUS_SCANF_LIST);
    if( scan_count != STATUS_LIST_LEN)
    {
	    printk("STATUS_CMD bad format %x != %x\n", scan_count, STATUS_LIST_LEN );
	    return;
    }
	printk("\n\n#########################################################################################\n\n");

	switch(status) {
		case STS_PORT:
            mv_eth_status_print();
            mv_eth_port_status_print(port);
			mvNetaPortStatus(port);
			break;

        case STS_PORT_MAC:
            mv_eth_mac_show(port);
        	break;

		case STS_PORT_TXQ:
			mvNetaTxqShow(port, txp, q, value);
			break;

		case STS_PORT_RXQ:
			mvNetaRxqShow(port, q, value);
			break;

		case STS_PORT_TOS_MAP:
			mv_eth_tos_map_show(port);
			break;

		case STS_TXP_WRR:
			mvEthTxpWrrRegs(port, txp);
			break;

		case STS_PORT_REGS:
			mvEthRegs(port);
			mvEthPortRegs(port);
			break;

        case STS_NETA_REGS:
			mvNetaPortRegs(port);
        	break;

#ifdef MV_ETH_GMAC_NEW
        case STS_GMAC_REGS:
            mvNetaGmacRegs(port);
            break;
#endif /* MV_ETH_GMAC_NEW */

		case STS_TXP_REGS:
			mvNetaTxpRegs(port, txp);
			break;

   		case STS_RXQ_REGS:
			mvNetaRxqRegs(port, q);
			break;

   		case STS_TXQ_REGS:
			mvNetaTxqRegs(port, txp, q);
			break;

#ifdef CONFIG_MV_ETH_PMT
		case STS_PMT_REGS:
			mvNetaPmtRegs(port, txp);
			break;
#endif /* CONFIG_MV_ETH_PMT */

#ifdef CONFIG_MV_ETH_PNC
        case STS_PNC_REGS:
        	mvNetaPncRegs();
        	break;
#endif /* CONFIG_MV_ETH_PNC */

#ifdef CONFIG_MV_ETH_HWF
        case STS_HWF_REGS:
            mvNetaHwfRxpRegs(port);
			break;
#endif /* CONFIG_MV_ETH_HWF */

#ifdef CONFIG_MV_ETH_BM
        case STS_BM_REGS:
        	mvBmRegs();
        	break;
#endif /* CONFIG_MV_ETH_BM */

		case STS_PORT_MIB:
	    	mvEthPortCounters(port, txp);
	    	mvEthPortRmonCounters(port, txp);
			break;

		case STS_PORT_STATS:
			printk("  PORT %d: GET ETH PORT STATISTIC\n\n", port);
			mv_eth_port_stats_print(port);
            break;

       	case STS_SWITCH_STATS:
#ifdef CONFIG_MV_ETH_SWITCH
            mv_eth_switch_status_print(port);
            printk("\n");
#endif /* CONFIG_MV_ETH_SWITCH */

#ifdef CONFIG_MV_INCLUDE_SWITCH
   	    mv_switch_stats_print();
#endif /* CONFIG_MV_INCLUDE_SWITCH */
       	    break;

	default:
			printk(" Unknown status command \n");
	}
}

int run_eth_com(const char *buffer) {

    int scan_count;

    scan_count = sscanf(buffer, ETH_CMD_STRING, ETH_SCANF_LIST);
    if( scan_count != ETH_LIST_LEN) {
	    printk("eth command bad format %x != %x\n", scan_count, ETH_LIST_LEN );
	    return 1;
    }
    switch(command) {

        case COM_TXDONE_Q:
            mv_eth_ctrl_txdone(value);
            break;

#ifdef CONFIG_NET_SKB_RECYCLE
        case COM_SKB_RECYCLE:
            mv_eth_ctrl_recycle(value);
            break;
#endif /* CONFIG_NET_SKB_RECYCLE */

#ifdef CONFIG_MV_ETH_NFP
        case COM_NFP_ENABLE:
            mv_eth_ctrl_nfp(value);
			nfp_mgr_enable(value);
            break;
#endif /* CONFIG_MV_ETH_NFP */

	    default:
            printk(" Unknown ETH command \n");
    }
    return 0;
}

/* NetDev commands */
int run_netdev_cmd(const char *buffer) {
    int 			    scan_count;
	struct net_device* 	dev;

        scan_count = sscanf(buffer, NETDEV_CMD_STRING, NETDEV_SCANF_LIST);

        if( scan_count != NETDEV_LIST_LEN) {
                printk("netdev command bad format %x != %x\n", scan_count, NETDEV_LIST_LEN );
                return 1;
        }

        switch(command) {

		case COM_TX_NOQUEUE:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
			dev = dev_get_by_name(name);
#else
		    dev = dev_get_by_name(&init_net, name);
#endif
                    if(dev != NULL)
                    {
			mv_eth_set_noqueue(dev, value);
			dev_put(dev);
		    }
		    break;

			case COM_NETDEV_STS:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
					dev = dev_get_by_name(name);
#else
                    dev = dev_get_by_name(&init_net, name);
#endif
                    if(dev != NULL)
                    {
			            mv_eth_netdev_print(dev);
                        dev_put(dev);
                    }
                    break;

#ifdef CONFIG_MV_ETH_SWITCH

			case COM_NETDEV_PORT_ADD:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						dev = dev_get_by_name(name);
#else
                        dev = dev_get_by_name(&init_net, name);
#endif
                        if(dev != NULL)
                        {
                            mv_eth_switch_port_add(dev, value);
                            dev_put(dev);
                        }
                        break;

		case COM_NETDEV_PORT_DEL:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
					dev = dev_get_by_name(name);
#else
					dev = dev_get_by_name(&init_net, name);
#endif
					if(dev != NULL)
                        {
                            mv_eth_switch_port_del(dev, value);
                            dev_put(dev);
                        }
                        break;

#endif /* CONFIG_MV_ETH_SWITCH */

                default:
                        printk(" Unknown netdev command \n");
        }
        return 0;
}

/* Giga Port commands */
int run_port_com(const char *buffer) {

	int scan_count;
    void*   port_hndl;

	scan_count = sscanf(buffer, PORT_CMD_STRING, PORT_SCANF_LIST);

	if( scan_count != PORT_LIST_LEN) {
		printk("eth port command bad format %x != %x\n", scan_count, PORT_LIST_LEN );
		return 1;
	}
    port_hndl = mvNetaPortHndlGet(port);
    if(port_hndl == NULL)
        return 1;

    	switch(command) {
	    case COM_TXP_BW:
	        mvNetaTxpRateSet(port, txp, value);
	        break;

/*
            case COM_EJP_MODE:
                mvEthEjpModeSet(port, value);
                break;
*/
  	    default:
	        printk(" Unknown port command \n");
    	}
   	return 0;
}

/* Giga RX Queue commands */
int run_port_rxq_cmd(const char *buffer) {

	int scan_count;

	scan_count = sscanf(buffer, RXQ_CMD_STRING, RXQ_SCANF_LIST);

	if( scan_count != RXQ_LIST_LEN) {
		printk("eth RXQ command bad format %x != %x\n", scan_count, RXQ_LIST_LEN );
		return 1;
	}

	switch(command) {
		case COM_RXQ_TOS_MAP:
			mv_eth_rxq_tos_map_set(port, q, value);
			break;

		case COM_RXQ_TIME_COAL:
			mvNetaRxqTimeCoalSet(port, q, value);
			break;

		case COM_RXQ_PKTS_COAL:
			mvNetaRxqPktsCoalSet(port, q, value);
			break;

#ifndef CONFIG_MV_ETH_PNC
		case COM_RXQ_TYPE:
			run_com_rxq_type();
			break;
#endif /* CONFIG_MV_ETH_PNC */

		default:
			printk(" Unknown RXQ command \n");
	}
	return 0;
}

/* Giga TX Queue commands */
int run_port_txq_cmd(const char *buffer) {

    int scan_count;

    scan_count = sscanf(buffer, TXQ_CMD_STRING, TXQ_SCANF_LIST);

    if( scan_count != TXQ_LIST_LEN) {
            printk("eth TXQ command bad format %x != %x\n", scan_count, TXQ_LIST_LEN );
            return 1;
    }

    switch(command)
    {
    	case COM_TXQ_COAL:
        		mvNetaTxDonePktsCoalSet(port, txp, q, value);
    		break;

        case COM_TXQ_TOS:
            mv_eth_txq_tos_map_set(port, q, value);
        	break;

        case COM_TXQ_BW:
		    mvNetaTxqRateSet(port, txp, q, value);
		    break;

	    case COM_TXQ_WRR:
		    if(value == 0)
			    mvNetaTxqFixPrioSet(port, txp, q);
		    else
			    mvNetaTxqWrrPrioSet(port, txp, q, value);
		    break;

        default:
            printk(" Unknown TXQ command \n");
    }
    return 0;
}

int mv_eth_tool_write (struct file *file, const char *buffer,
                      unsigned long count, void *data) {

	sscanf(buffer, "%x", &command);

	switch (command) {
        case COM_EJP_MODE:
		case COM_TXP_BW:
			run_port_com(buffer);
			break;

		case COM_TXDONE_Q:
       	case COM_SKB_RECYCLE:
	    case COM_NFP_ENABLE:
			run_eth_com(buffer);
			break;

		case COM_RXQ_TOS_MAP:
		case COM_RXQ_PKTS_COAL:
		case COM_RXQ_TIME_COAL:
		case COM_RXQ_TYPE:
            run_port_rxq_cmd(buffer);
			break;

		case COM_TXQ_COAL:
		case COM_TXQ_TOS:
		case COM_TXQ_WRR:
		case COM_TXQ_BW:
            run_port_txq_cmd(buffer);
			break;

		case COM_TX_NOQUEUE:
        case COM_NETDEV_STS:
		case COM_NETDEV_PORT_ADD:
		case COM_NETDEV_PORT_DEL:
            run_netdev_cmd(buffer);
            break;

   		case COM_STS:
			run_com_stats(buffer);
			break;

		default:
			printk("eth proc unknown command.\n");
			break;
	}
	return count;
}

static int proc_calc_metrics(char *page, char **start, off_t off,
                                 int count, int *eof, int len)
{
        if (len <= off+count)
		*eof = 1;

        *start = page + off;
        len -= off;

        if (len > count)
		len = count;

        if (len < 0)
		len = 0;

        return len;
}



int mv_eth_tool_read (char *page, char **start, off_t off,
                            int count, int *eof, void *data) {
	unsigned int len = 0;

   	return proc_calc_metrics(page, start, off, count, eof, len);
}



int __init start_mv_eth_tool(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
     mv_eth_tool = proc_net_create(FILE_NAME , 0666 , NULL);
#else
     mv_eth_tool = create_proc_entry(FILE_NAME , 0666 , init_net.proc_net);
#endif
  mv_eth_tool->read_proc = mv_eth_tool_read;
  mv_eth_tool->write_proc = mv_eth_tool_write;
  mv_eth_tool->nlink = 1;
  return 0;
}

module_init(start_mv_eth_tool);
