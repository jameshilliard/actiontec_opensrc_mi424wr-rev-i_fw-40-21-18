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

#include "mvCommon.h"
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <linux/mii.h>

#include "mvOs.h"
#include "mvDebug.h"
#include "dbg-trace.h"
#include "mvSysHwConfig.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "ctrlEnv/mvCtrlEnvLib.h"
#include "eth-phy/mvEthPhy.h"
#include "mvSysEthPhyApi.h"
#include "mvSysNetaApi.h"


#include "gbe/mvNeta.h"
#include "bm/mvBm.h"
#include "pnc/mvPnc.h"
#include "nfp/mvNfp.h"

#include "mv_switch.h"
#include "mv_netdev.h"

#include "mvOs.h"
#include "mvSysHwConfig.h"

#define MV_ETH_TOOL_AN_TIMEOUT	5000

extern spinlock_t          mv_eth_mii_lock;

int isSwitch(struct eth_port *priv)
{
	return (priv->flags & (MV_ETH_F_SWITCH | MV_ETH_F_EXT_SWITCH));
}


/******************************************************************************
* mv_eth_tool_restore_settings
* Description:
*	restore saved speed/dublex/an settings
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	None
* RETURN:
*	0 for success
*
*******************************************************************************/
int mv_eth_tool_restore_settings(struct net_device *netdev)
{
	struct eth_port 	*priv = MV_ETH_PRIV(netdev);
	int 				mv_phy_speed, mv_phy_duplex;

	MV_U32			mv_phy_addr = mvBoardPhyAddrGet(priv->port);
	MV_ETH_PORT_SPEED	mv_mac_speed;
	MV_ETH_PORT_DUPLEX	mv_mac_duplex;
	int			err = -EINVAL;

#ifdef CONFIG_MV_ETH_SWITCH
	 if (isSwitch(priv))
		 return -EPERM;
#endif /* CONFIG_MV_ETH_SWITCH */

	switch (priv->speed_cfg) {
	case SPEED_10:
		mv_phy_speed  = 0;
		mv_mac_speed = MV_ETH_SPEED_10;
		break;
	case SPEED_100:
		mv_phy_speed  = 1;
		mv_mac_speed = MV_ETH_SPEED_100;
		break;
	case SPEED_1000:
		mv_phy_speed  = 2;
		mv_mac_speed = MV_ETH_SPEED_1000;
		break;
	default:
		return -EINVAL;
	}

	switch (priv->duplex_cfg) {
	case DUPLEX_HALF:
		mv_phy_duplex = 0;
		mv_mac_duplex = MV_ETH_DUPLEX_HALF;
		break;
	case DUPLEX_FULL:
		mv_phy_duplex = 1;
		mv_mac_duplex = MV_ETH_DUPLEX_FULL;
		break;
	default:
		return -EINVAL;
	}

	if (priv->autoneg_cfg == AUTONEG_ENABLE) {
		err = mvNetaSpeedDuplexSet(priv->port, MV_ETH_SPEED_1000, MV_ETH_DUPLEX_FULL);

		/* Restart AN on PHY enables it */
		if (!err) {

			err = mvEthPhyRestartAN(mv_phy_addr, MV_ETH_TOOL_AN_TIMEOUT);
			if (err == MV_TIMEOUT) {
				MV_ETH_PORT_STATUS ps;

				mvNetaLinkStatus(priv->port, &ps);

				if (!ps.linkup)
					err = 0;
			}
		}
	} else if (priv->autoneg_cfg == AUTONEG_DISABLE) {
		err = mvEthPhyDisableAN(mv_phy_addr, mv_phy_speed, mv_phy_duplex);
		if (!err)
			err = mvNetaSpeedDuplexSet(priv->port, mv_mac_speed, mv_mac_duplex);
	} else {
		err = -EINVAL;
	}
	return err;
}

/******************************************************************************
* mv_eth_tool_get_settings
* Description:
*	ethtool get standard port settings
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	cmd		command (settings)
* RETURN:
*	0 for success
*
*******************************************************************************/
int mv_eth_tool_get_settings(struct net_device *netdev, struct ethtool_cmd *cmd)
{
	struct eth_port 	*priv = MV_ETH_PRIV(netdev);

	int		 retval;
#ifdef CONFIG_MV_ETH_SWITCH
	 if (isSwitch(priv))
		 return -EPERM;
#endif /* CONFIG_MV_ETH_SWITCH */

#ifdef CONFIG_MII
	retval = mii_ethtool_gset(&priv->mii, cmd);
	if (retval)
		return retval;
#endif /* CONFIG_MII */

	if (priv) {
		MV_ETH_PORT_STATUS  status;

		mvNetaLinkStatus(priv->port, &status);

		switch (status.speed) {
		case MV_ETH_SPEED_1000:
			cmd->speed = SPEED_1000;
			break;
		case MV_ETH_SPEED_100:
			cmd->speed = SPEED_100;
			break;
		case MV_ETH_SPEED_10:
			cmd->speed = SPEED_10;
			break;
		default:
			return -EINVAL;
		}

		if (status.duplex == MV_ETH_DUPLEX_FULL)
			cmd->duplex = 1;
		else
			cmd->duplex = 0;

		cmd->port = PORT_MII;
		cmd->phy_address = mvBoardPhyAddrGet(priv->port);
	}
	return 0;
}


/******************************************************************************
* mv_eth_tool_set_settings
* Description:
*	ethtool set standard port settings
* INPUT:
*	netdev		Network device structure pointer
*	cmd		command (settings)
* OUTPUT
*	None
* RETURN:
*	0 for success
*
*******************************************************************************/
int mv_eth_tool_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct eth_port *priv = MV_ETH_PRIV(dev);
	int _speed, _duplex, _autoneg, err;

#ifdef CONFIG_MV_ETH_SWITCH
	 if (isSwitch(priv))
		 return -EPERM;
#endif /* CONFIG_MV_ETH_SWITCH */


	_duplex  = priv->duplex_cfg;
	_speed   = priv->speed_cfg;
	_autoneg = priv->autoneg_cfg;

	priv->duplex_cfg = cmd->duplex;
	priv->speed_cfg = cmd->speed;
	priv->autoneg_cfg = cmd->autoneg;
	err = mv_eth_tool_restore_settings(dev);

	if (err) {
		priv->duplex_cfg = _duplex;
		priv->speed_cfg = _speed;
		priv->autoneg_cfg = _autoneg;
	}
	return err;
}




/******************************************************************************
* mv_eth_tool_get_regs_len
* Description:
*	ethtool get registers array length
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	None
* RETURN:
*	registers array length
*
*******************************************************************************/
int mv_eth_tool_get_regs_len(struct net_device *netdev)
{
#define MV_ETH_TOOL_REGS_LEN 32

	return MV_ETH_TOOL_REGS_LEN * sizeof(uint32_t);
}


/******************************************************************************
* mv_eth_tool_get_drvinfo
* Description:
*	ethtool get driver information
* INPUT:
*	netdev		Network device structure pointer
*	info		driver information
* OUTPUT
*	info		driver information
* RETURN:
*	None
*
*******************************************************************************/
void mv_eth_tool_get_drvinfo(struct net_device *netdev,
			     struct ethtool_drvinfo *info)
{
	strcpy(info->driver, "mv_eth");
	/*strcpy(info->version, LSP_VERSION);*/
	strcpy(info->fw_version, "N/A");
	strcpy(info->bus_info, "Mbus");
/*   TBD
	info->n_stats = MV_ETH_TOOL_STATS_LEN;
*/
	info->testinfo_len = 0;
	info->regdump_len = mv_eth_tool_get_regs_len(netdev);
	info->eedump_len = 0;
}


/******************************************************************************
* mv_eth_tool_get_regs
* Description:
*	ethtool get registers array
* INPUT:
*	netdev		Network device structure pointer
*	regs		registers information
* OUTPUT
*	p		registers array
* RETURN:
*	None
*
*******************************************************************************/
void mv_eth_tool_get_regs(struct net_device *netdev,
			  struct ethtool_regs *regs, void *p)
{
	struct eth_port	*priv = MV_ETH_PRIV(netdev);
	uint32_t 	*regs_buff = p;

	memset(p, 0, MV_ETH_TOOL_REGS_LEN * sizeof(uint32_t));

	regs->version = mvCtrlModelRevGet();

	/* ETH port registers */
	regs_buff[0]  = MV_REG_READ(ETH_PORT_STATUS_REG(priv->port));
	regs_buff[1]  = MV_REG_READ(ETH_PORT_SERIAL_CTRL_REG(priv->port));
	regs_buff[2]  = MV_REG_READ(ETH_PORT_CONFIG_REG(priv->port));
	regs_buff[3]  = MV_REG_READ(ETH_PORT_CONFIG_EXTEND_REG(priv->port));
	regs_buff[4]  = MV_REG_READ(ETH_SDMA_CONFIG_REG(priv->port));
/*	regs_buff[5]  = MV_REG_READ(ETH_TX_FIFO_URGENT_THRESH_REG(priv->port)); */
	regs_buff[6]  = MV_REG_READ(ETH_RX_QUEUE_COMMAND_REG(priv->port));
	/* regs_buff[7]  = MV_REG_READ(ETH_TX_QUEUE_COMMAND_REG(priv->port)); */
	regs_buff[8]  = MV_REG_READ(ETH_INTR_CAUSE_REG(priv->port));
	regs_buff[9]  = MV_REG_READ(ETH_INTR_CAUSE_EXT_REG(priv->port));
	regs_buff[10] = MV_REG_READ(ETH_INTR_MASK_REG(priv->port));
	regs_buff[11] = MV_REG_READ(ETH_INTR_MASK_EXT_REG(priv->port));
	/* ETH Unit registers */
	regs_buff[16] = MV_REG_READ(ETH_PHY_ADDR_REG(priv->port));
	regs_buff[17] = MV_REG_READ(ETH_UNIT_INTR_CAUSE_REG(priv->port));
	regs_buff[18] = MV_REG_READ(ETH_UNIT_INTR_MASK_REG(priv->port));
	regs_buff[19] = MV_REG_READ(ETH_UNIT_ERROR_ADDR_REG(priv->port));
	regs_buff[20] = MV_REG_READ(ETH_UNIT_INT_ADDR_ERROR_REG(priv->port));

}




/******************************************************************************
* mv_eth_tool_nway_reset
* Description:
*	ethtool restart auto negotiation
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	None
* RETURN:
*	0 on success
*
*******************************************************************************/
int mv_eth_tool_nway_reset(struct net_device *netdev)
{
	struct eth_port 	*priv = MV_ETH_PRIV(netdev);
	MV_U32		mv_phy_addr = mvBoardPhyAddrGet(priv->port);
#ifdef CONFIG_MV_ETH_SWITCH
	 if (isSwitch(priv))
		 return -EPERM;
#endif /* CONFIG_MV_ETH_SWITCH */


	if (mvEthPhyRestartAN(mv_phy_addr, MV_ETH_TOOL_AN_TIMEOUT) != MV_OK)
		return -EINVAL;

	return 0;
}

/******************************************************************************
* mv_eth_tool_get_link
* Description:
*	ethtool get link status
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	None
* RETURN:
*	0 if link is down, 1 if link is up
*
*******************************************************************************/
u32 mv_eth_tool_get_link(struct net_device *netdev)
{
	struct eth_port     *pp = MV_ETH_PRIV(netdev);
#ifdef CONFIG_MV_ETH_SWITCH
	 if (isSwitch(pp))
		 return -EPERM;
#endif /* CONFIG_MV_ETH_SWITCH */


	if (pp) {
		if (mvNetaLinkIsUp(pp->port))
			return 1;
	}
	return 0;
}
/******************************************************************************
* mv_eth_tool_get_coalesce
* Description:
*	ethtool get RX/TX coalesce parameters
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	cmd		Coalesce parameters
* RETURN:
*	0 on success
*
*******************************************************************************/
int mv_eth_tool_get_coalesce(struct net_device *netdev,
			     struct ethtool_coalesce *cmd)
{
/*	printk("in %s \n",__FUNCTION__);*/
	return 0;
}

/******************************************************************************
* mv_eth_tool_set_coalesce
* Description:
*	ethtool set RX/TX coalesce parameters
* INPUT:
*	netdev		Network device structure pointer
*	cmd		Coalesce parameters
* OUTPUT
*	None
* RETURN:
*	0 on success
*
*******************************************************************************/
int mv_eth_tool_set_coalesce(struct net_device *netdev,
			     struct ethtool_coalesce *cmd)
{
/*	printk("in %s \n",__FUNCTION__);*/
/*
	struct eth_port 	*priv = MV_ETH_PRIV(netdev);

	if (mvEthCoalGet(priv->hal_priv, &cmd->rx_coalesce_usecs,
	    &cmd->tx_coalesce_usecs) != MV_OK)
		return -EINVAL;
*/
	return 0;
}


/******************************************************************************
* mv_eth_tool_get_ringparam
* Description:
*	ethtool get ring parameters
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	ring		Ring paranmeters
* RETURN:
*	None
*
*******************************************************************************/
void mv_eth_tool_get_ringparam(struct net_device *netdev,
				struct ethtool_ringparam *ring)
{
/*	printk("in %s \n",__FUNCTION__); */
}

/******************************************************************************
* mv_eth_tool_get_pauseparam
* Description:
*	ethtool get pause parameters
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	pause		Pause paranmeters
* RETURN:
*	None
*
*******************************************************************************/
void mv_eth_tool_get_pauseparam(struct net_device *netdev,
				struct ethtool_pauseparam *pause)
{
	struct eth_port 	*priv = MV_ETH_PRIV(netdev);
	int 			port = priv->port;
	MV_ETH_PORT_STATUS	portStatus;
	MV_ETH_PORT_FC 		flowCtrl;

#ifdef CONFIG_MV_ETH_SWITCH
	if (isSwitch(priv)) {
		printk("mv_eth_tool_get_pauseparam() is not supported on a switch port\n");
		return;
	}
#endif /* CONFIG_MV_ETH_SWITCH */

	mvNetaFlowCtrlGet(port, &flowCtrl);
	if ((flowCtrl == MV_ETH_FC_AN_NO) || (flowCtrl == MV_ETH_FC_AN_SYM) || (flowCtrl == MV_ETH_FC_AN_ASYM))
		pause->autoneg = AUTONEG_ENABLE;
	else
		pause->autoneg = AUTONEG_DISABLE;

	mvNetaLinkStatus(port, &portStatus);
	if (portStatus.rxFc == MV_ETH_FC_DISABLE)
		pause->rx_pause = 0;
	else
		pause->rx_pause = 1;

	if (portStatus.txFc == MV_ETH_FC_DISABLE)
		pause->tx_pause = 0;
	else
		pause->tx_pause = 1;
}




/******************************************************************************
* mv_eth_tool_set_pauseparam
* Description:
*	ethtool configure pause parameters
* INPUT:
*	netdev		Network device structure pointer
*	pause		Pause paranmeters
* OUTPUT
*	None
* RETURN:
*	0 on success
*
*******************************************************************************/
int mv_eth_tool_set_pauseparam(struct net_device *netdev,
				struct ethtool_pauseparam *pause)
{
	struct eth_port *priv = MV_ETH_PRIV(netdev);
	int				port = priv->port;
	MV_U32			phy_addr;
	MV_STATUS		status = MV_FAIL;

#ifdef CONFIG_MV_ETH_SWITCH
	 if (isSwitch(priv))
		 return -EPERM;
#endif /* CONFIG_MV_ETH_SWITCH */


	if (pause->rx_pause && pause->tx_pause) { /* Enable FC */
		if (pause->autoneg) { /* autoneg enable */
			status = mvNetaFlowCtrlSet(port, MV_ETH_FC_AN_SYM);
		} else { /* autoneg disable */
			status = mvNetaFlowCtrlSet(port, MV_ETH_FC_ENABLE);
		}
	} else if (!pause->rx_pause && !pause->tx_pause) { /* Disable FC */
		if (pause->autoneg) { /* autoneg enable */
			status = mvNetaFlowCtrlSet(port, MV_ETH_FC_AN_NO);
		} else { /* autoneg disable */
			status = mvNetaFlowCtrlSet(port, MV_ETH_FC_DISABLE);
		}
	}
	/* Only symmetric change for RX and TX flow control is allowed */
	if (status == MV_OK) {
		phy_addr = mvBoardPhyAddrGet(priv->port);
		status = mvEthPhyRestartAN(phy_addr, MV_ETH_TOOL_AN_TIMEOUT);
	}
	if (status != MV_OK)
		return -EINVAL;

	return 0;
}


/******************************************************************************
* mv_eth_tool_get_rx_csum
* Description:
*	ethtool get RX checksum offloading status
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	None
* RETURN:
*	RX checksum
*
*******************************************************************************/
u32 mv_eth_tool_get_rx_csum(struct net_device *netdev)
{
#ifdef CONFIG_MV_ETH_RX_CSUM_OFFLOAD
	struct eth_port *priv = MV_ETH_PRIV(netdev);

	return (priv->rx_csum_offload != 0);
#else
	return 0;
#endif
}

/******************************************************************************
* mv_eth_tool_set_rx_csum
* Description:
*	ethtool enable/disable RX checksum offloading
* INPUT:
*	netdev		Network device structure pointer
*	data		Command data
* OUTPUT
*	None
* RETURN:
*	0 on success
*
*******************************************************************************/
int mv_eth_tool_set_rx_csum(struct net_device *netdev, uint32_t data)
{
#ifdef CONFIG_MV_ETH_RX_CSUM_OFFLOAD
	struct eth_port *priv = MV_ETH_PRIV(netdev);

	priv->rx_csum_offload = data;
	return 0;
#else
	return -EOPNOTSUPP;
#endif
}

/******************************************************************************
* mv_eth_tool_set_tx_csum
* Description:
*	ethtool enable/disable TX checksum offloading
* INPUT:
*	netdev		Network device structure pointer
*	data		Command data
* OUTPUT
*	None
* RETURN:
*	0 on success
*
*******************************************************************************/
int mv_eth_tool_set_tx_csum(struct net_device *netdev, uint32_t data)
{
#ifdef CONFIG_MV_ETH_TX_CSUM_OFFLOAD
	if (data)
		netdev->features |= NETIF_F_IP_CSUM;
	else
		netdev->features &= ~NETIF_F_IP_CSUM;

	return 0;
#else
	return -EOPNOTSUPP;
#endif /* TX_CSUM_OFFLOAD */
}


/******************************************************************************
* mv_eth_tool_set_tso
* Description:
*	ethtool enable/disable TCP segmentation offloading
* INPUT:
*	netdev		Network device structure pointer
*	data		Command data
* OUTPUT
*	None
* RETURN:
*	0 on success
*
*******************************************************************************/
int mv_eth_tool_set_tso(struct net_device *netdev, uint32_t data)
{
#ifdef CONFIG_MV_ETH_SWITCH
	struct eth_port 	*priv = MV_ETH_PRIV(netdev);

	if (isSwitch(priv)) {
		printk("mv_eth_tool_set_tso() is not supported on a switch port\n");
		return  -EOPNOTSUPP;
	}
#endif /* CONFIG_MV_ETH_SWITCH */

#if defined(CONFIG_MV_ETH_TSO)
	if (data)
		netdev->features |= NETIF_F_TSO;
	else
		netdev->features &= ~NETIF_F_TSO;

	return 0;
#else
	return -EOPNOTSUPP;
#endif
}
/******************************************************************************
* mv_eth_tool_set_ufo
* Description:
*	ethtool enable/disable UDP segmentation offloading
* INPUT:
*	netdev		Network device structure pointer
*	data		Command data
* OUTPUT
*	None
* RETURN:
*	0 on success
*
*******************************************************************************/
int mv_eth_tool_set_ufo(struct net_device *netdev, uint32_t data)
{
/*	printk("in %s \n",__FUNCTION__);*/
	return -EOPNOTSUPP;
}
/******************************************************************************
* mv_eth_tool_get_strings
* Description:
*	ethtool get strings (used for statistics and self-test descriptions)
* INPUT:
*	netdev		Network device structure pointer
*	stringset	strings parameters
* OUTPUT
*	data		output data
* RETURN:
*	None
*
*******************************************************************************/
void mv_eth_tool_get_strings(struct net_device *netdev,
			     uint32_t stringset, uint8_t *data)
{
/*	printk("in %s \n",__FUNCTION__);*/

}

/*******************************************************************************/
int mv_eth_tool_phys_id(struct net_device *netdev, u32 data)
{
/*	printk("in %s \n",__FUNCTION__);*/
	return 0;
}

/******************************************************************************
* mv_eth_tool_get_stats_count
* Description:
*	ethtool get statistics count (number of stat. array entries)
* INPUT:
*	netdev		Network device structure pointer
* OUTPUT
*	None
* RETURN:
*	statistics count
*
*******************************************************************************/
int mv_eth_tool_get_stats_count(struct net_device *netdev)
{
/*	printk("in %s \n",__FUNCTION__);*/
	return 0;
}

/******************************************************************************
* mv_eth_tool_get_ethtool_stats
* Description:
*	ethtool get statistics
* INPUT:
*	netdev		Network device structure pointer
*	stats		stats parameters
* OUTPUT
*	data		output data
* RETURN:
*	None
*
*******************************************************************************/
void mv_eth_tool_get_ethtool_stats(struct net_device *netdev,
				   struct ethtool_stats *stats, uint64_t *data)
{

}

const struct ethtool_ops mv_eth_tool_ops = {
	.get_settings				= mv_eth_tool_get_settings,
	.set_settings				= mv_eth_tool_set_settings,
	.get_drvinfo				= mv_eth_tool_get_drvinfo,
	.get_regs_len				= mv_eth_tool_get_regs_len,
	.get_regs					= mv_eth_tool_get_regs,
	.nway_reset					= mv_eth_tool_nway_reset,
	.get_link					= mv_eth_tool_get_link,
	.get_coalesce				= mv_eth_tool_get_coalesce,
	.set_coalesce				= mv_eth_tool_set_coalesce,
	.get_ringparam  			= mv_eth_tool_get_ringparam,
	.get_pauseparam				= mv_eth_tool_get_pauseparam,
	.set_pauseparam				= mv_eth_tool_set_pauseparam,
	.get_rx_csum				= mv_eth_tool_get_rx_csum,
	.set_rx_csum				= mv_eth_tool_set_rx_csum,
	.get_tx_csum				= ethtool_op_get_tx_csum,
	.set_tx_csum				= mv_eth_tool_set_tx_csum,
	.get_sg						= ethtool_op_get_sg,
	.set_sg						= ethtool_op_set_sg,
	.get_tso					= ethtool_op_get_tso,
	.set_tso					= mv_eth_tool_set_tso,
	.get_ufo					= ethtool_op_get_ufo,
	.set_ufo					= mv_eth_tool_set_ufo,
	.get_strings				= mv_eth_tool_get_strings,
	.phys_id					= mv_eth_tool_phys_id,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
	.get_stats_count			= mv_eth_tool_get_stats_count,
#endif
	.get_ethtool_stats			= mv_eth_tool_get_ethtool_stats,
};

