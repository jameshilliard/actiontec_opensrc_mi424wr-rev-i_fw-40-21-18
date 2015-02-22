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

/*******************************************************************************
* mv_nfp_mgr.h - Header File for Marvell NFP Manager
*
* DESCRIPTION:
*       This header file contains macros, typedefs and function declarations
* 	specific to the Marvell Network Fast Processing Manager.
*
* DEPENDENCIES:
*       None.
*
*******************************************************************************/

#ifndef __mv_nfp_mgr_h__
#define __mv_nfp_mgr_h__

#include <linux/netdevice.h>

#include "net_dev/mv_netdev.h"

typedef struct
{
	MV_U16  sid;
	MV_U32  iif;
	MV_U32  chan;
	void*   dev;
	MV_U8	da[MV_MAC_ADDR_SIZE];
} MV_RULE_PPP;


/* NFP interface type, used for registration */
typedef enum {
	MV_NFP_IF_INV,   /* Invalid interface */
	MV_NFP_IF_INT,   /* use to register a Marvell GbE interface */
	MV_NFP_IF_BRG,   /* use to register a virtual interface such as bridge */
	MV_NFP_IF_EXT,   /* use to register an external interface such as WLAN */
	MV_NFP_IF_PPP,    /* PPPoE */
} MV_NFP_IF_TYPE;

int 	nfp_hwf_txq_get(int rxp, int port, int *txp, int *txq);
int		nfp_hwf_txq_set(int rxp, int port, int txp, int txq);

#ifdef NFP_SWF
int nfp_swf_rule_add(u32 flowid, u32 txp, u32 txq, u32 mh_sel);
int nfp_swf_rule_del(u32 flowid);
#endif

void nfp_mgr_enable(int);
void nfp_mgr_stats(void);
void nfp_mgr_dump(void);
int  nfp_mgr_if_register(int if_index, MV_NFP_IF_TYPE if_type, struct net_device* dev, struct eth_port *pp);
int  nfp_mgr_if_unregister(int if_index);
void nfp_eth_dev_db_clear(void);

#endif /* __mv_nfp_mgr_h__ */

