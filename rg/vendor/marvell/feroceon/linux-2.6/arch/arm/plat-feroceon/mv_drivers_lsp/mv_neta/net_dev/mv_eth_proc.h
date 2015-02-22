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
#ifndef __mv_eth_proc
#define __mv_eth_proc

#define FILE_NAME		"mv_eth_tool"
#define PORT_STAT_FILE_NAME  	"mv_eth_port_stat_tool"
#define FILE_PATH		"/proc/net/"
#define STS_FILE		"mvethtool.sts"


#define ETH_CMD_STRING      "%2x %x"
#define ETH_PRINTF_LIST     command, value
#define ETH_SCANF_LIST      &command, &value
#define ETH_LIST_LEN        2

#define PORT_CMD_STRING     "%2x %2x %2x %x"
#define PORT_PRINTF_LIST    command, port, txp, value
#define PORT_SCANF_LIST     &command, &port, &txp, &value
#define PORT_LIST_LEN       4

#define RXQ_CMD_STRING      "%2x %2x %2x %x"
#define RXQ_PRINTF_LIST     command, port, q, value
#define RXQ_SCANF_LIST      &command, &port, &q, &value
#define RXQ_LIST_LEN        4

#define TXQ_CMD_STRING      "%2x %2x %2x %2x %x"
#define TXQ_PRINTF_LIST     command, port, txp, q, value
#define TXQ_SCANF_LIST      &command, &port, &txp, &q, &value
#define TXQ_LIST_LEN        5

#define STATUS_CMD_STRING   "%2x %2x %2x %2x %2x %x"
#define STATUS_PRINTF_LIST  command, status, port, txp, q, value
#define STATUS_SCANF_LIST   &command, &status, &port, &txp, &q, &value
#define STATUS_LIST_LEN     6

#define NETDEV_CMD_STRING  "%2x %s %d"
#define NETDEV_PRINTF_LIST command, name, value
#define NETDEV_SCANF_LIST  &command, name, &value
#define NETDEV_LIST_LEN    3

typedef enum {
    COM_RXQ_TYPE = 0,
    COM_RXQ_MC,
    COM_RXQ_TIME_COAL,
    COM_RXQ_PKTS_COAL,
    COM_TXQ_COAL,
    COM_TXDONE_Q,
    COM_SKB_RECYCLE,
    COM_EJP_MODE,
    COM_RXQ_TOS_MAP,
    COM_TX_NOQUEUE,
    COM_STS,
    COM_NFP_ENABLE,
    COM_NETDEV_STS,
    COM_TXP_BW,
    COM_TXQ_WRR,
    COM_TXQ_BW,
    COM_NETDEV_PORT_ADD,
    COM_NETDEV_PORT_DEL,
    COM_TXQ_TOS,

} command_t;

typedef enum {
	PT_BPDU = 0,
	PT_ARP,
	PT_TCP,
	PT_UDP,
	PT_NONE
} packet_t;

typedef enum {
	STS_PORT = 0,
	STS_PORT_RXQ,
	STS_PORT_TXQ,
	STS_TXP_WRR,
	STS_PORT_REGS,
	STS_PORT_MIB,
	STS_PORT_STATS,
    STS_PORT_MAC,
    STS_PORT_TOS_MAP,
    STS_SWITCH_STATS,
    STS_NETA_REGS,
    STS_PNC_REGS,
    STS_BM_REGS,
	STS_HWF_REGS,
	STS_TXP_REGS,
    STS_GMAC_REGS,
    STS_RXQ_REGS,
    STS_TXQ_REGS,
    STS_PMT_REGS,

} status_t;

typedef enum {
	DB_ROUTING = 0,
	DB_NAT,
} db_type_t;

#endif

