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

#include "mvCommon.h"		/* Should be included before mvSysHwConfig */
#include <linux/etherdevice.h>
#include "mvOs.h"
#include "mvSysHwConfig.h"
#include "eth-phy/mvEthPhy.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "ctrlEnv/mvCtrlEthCompLib.h"

#include "msApi.h"
#include "h/platform/gtMiiSmiIf.h"
#include "mv_switch.h"
#include "gbe/mvNeta.h"

#ifdef CONFIG_MV_ETH_SWITCH_LINK
extern void mv_eth_switch_update_link(unsigned int p, unsigned int link_up);
extern void mv_eth_switch_interrupt_unmask(int qsgmii_module, int gephy_on_port);
extern void mv_eth_switch_interrupt_clear(int qsgmii_module, int gephy_on_port);
#endif

#define MV_SWITCH_DEF_INDEX     0
#define MV_ETH_PORT_0           0
#define MV_ETH_PORT_1           1

/* uncomment for debug prints */
/* #define SWITCH_DEBUG */

#define SWITCH_DBG_OFF      0x0000
#define SWITCH_DBG_LOAD     0x0001
#define SWITCH_DBG_MCAST    0x0002
#define SWITCH_DBG_VLAN     0x0004
#define SWITCH_DBG_ALL      0xffff

#ifdef SWITCH_DEBUG
static u32 switch_dbg = 0;
#define SWITCH_DBG(FLG, X) if ((switch_dbg & (FLG)) == (FLG)) printk X
#else
#define SWITCH_DBG(FLG, X)
#endif /* SWITCH_DEBUG */

static GT_QD_DEV qddev, *qd_dev = NULL;
static GT_SYS_CONFIG qd_cfg;

static int qd_cpu_port = -1;
static int qsgmii_module = 0;
static int gephy_on_port = -1;
static int rgmiia_on_port = -1;

#ifdef CONFIG_MV_ETH_SWITCH_LINK
static int switch_irq = -1;
int switch_link_poll = 0;
static struct timer_list switch_link_timer;
#endif /* CONFIG_MV_ETH_SWITCH_LINK */

static spinlock_t switch_lock;

static GT_BOOL mv_switch_mii_read(GT_QD_DEV *dev, unsigned int phy, unsigned int reg, unsigned int *data)
{
	unsigned long flags;
	unsigned short tmp;
	MV_STATUS status;

	spin_lock_irqsave(&switch_lock, flags);
	status = mvEthPhyRegRead(phy, reg, &tmp);
	spin_unlock_irqrestore(&switch_lock, flags);
	*data = tmp;

	if (status == MV_OK)
		return GT_TRUE;

	return GT_FALSE;
}

static GT_BOOL mv_switch_mii_write(GT_QD_DEV *dev, unsigned int phy, unsigned int reg, unsigned int data)
{
	unsigned long flags;
	unsigned short tmp;
	MV_STATUS status;

	spin_lock_irqsave(&switch_lock, flags);
	tmp = (unsigned short)data;
	status = mvEthPhyRegWrite(phy, reg, tmp);
	spin_unlock_irqrestore(&switch_lock, flags);

	if (status == MV_OK)
		return GT_TRUE;

	return GT_FALSE;
}

int mv_switch_mac_addr_set(unsigned char *mac_addr, unsigned char db, unsigned int ports_mask, unsigned char op)
{
	GT_ATU_ENTRY mac_entry;

	memset(&mac_entry, 0, sizeof(GT_ATU_ENTRY));

	mac_entry.trunkMember = GT_FALSE;
	mac_entry.prio = 0;
	mac_entry.exPrio.useMacFPri = GT_FALSE;
	mac_entry.exPrio.macFPri = 0;
	mac_entry.exPrio.macQPri = 0;
	mac_entry.DBNum = db;
	mac_entry.portVec = ports_mask;
	memcpy(mac_entry.macAddr.arEther, mac_addr, 6);

	if (is_multicast_ether_addr(mac_addr))
		mac_entry.entryState.mcEntryState = GT_MC_STATIC;
	else
		mac_entry.entryState.ucEntryState = GT_UC_NO_PRI_STATIC;

	if ((op == 0) || (mac_entry.portVec == 0)) {
		if (gfdbDelAtuEntry(qd_dev, &mac_entry) != GT_OK) {
			printk("gfdbDelAtuEntry failed\n");
			return -1;
		}
	} else {
		if (gfdbAddMacEntry(qd_dev, &mac_entry) != GT_OK) {
			printk("gfdbAddMacEntry failed\n");
			return -1;
		}
	}

	return 0;
}

int mv_switch_port_based_vlan_set(unsigned int ports_mask, int set_cpu_port)
{
	unsigned int p, pl;
	unsigned char cnt;
	GT_LPORT port_list[MAX_SWITCH_PORTS];

	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(ports_mask, p) && (set_cpu_port || (p != qd_cpu_port))) {
			SWITCH_DBG(SWITCH_DBG_LOAD | SWITCH_DBG_MCAST | SWITCH_DBG_VLAN,
				   ("port based vlan, port %d: ", p));
			for (pl = 0, cnt = 0; pl < qd_dev->numOfPorts; pl++) {
				if (MV_BIT_CHECK(ports_mask, pl) && (pl != p)) {
					SWITCH_DBG(SWITCH_DBG_LOAD | SWITCH_DBG_MCAST | SWITCH_DBG_VLAN, ("%d ", pl));
					port_list[cnt] = pl;
					cnt++;
				}
			}
			if (gvlnSetPortVlanPorts(qd_dev, p, port_list, cnt) != GT_OK) {
				printk("gvlnSetPortVlanPorts failed\n");
				return -1;
			}
			SWITCH_DBG(SWITCH_DBG_LOAD | SWITCH_DBG_MCAST | SWITCH_DBG_VLAN, ("\n"));
		}
	}
	return 0;
}

int mv_switch_vlan_in_vtu_set(unsigned short vlan_id, unsigned short db_num, unsigned int ports_mask)
{
	GT_VTU_ENTRY vtu_entry;
	unsigned int p;

	vtu_entry.vid = vlan_id;
	vtu_entry.DBNum = db_num;
	vtu_entry.vidPriOverride = GT_FALSE;
	vtu_entry.vidPriority = 0;
	vtu_entry.vidExInfo.useVIDFPri = GT_FALSE;
	vtu_entry.vidExInfo.vidFPri = 0;
	vtu_entry.vidExInfo.useVIDQPri = GT_FALSE;
	vtu_entry.vidExInfo.vidQPri = 0;
	vtu_entry.vidExInfo.vidNRateLimit = GT_FALSE;
	SWITCH_DBG(SWITCH_DBG_LOAD | SWITCH_DBG_MCAST | SWITCH_DBG_VLAN, ("vtu entry: vid=0x%x, port ", vtu_entry.vid));

	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(ports_mask, p)) {
			SWITCH_DBG(SWITCH_DBG_LOAD | SWITCH_DBG_MCAST | SWITCH_DBG_VLAN, ("%d ", p));
			vtu_entry.vtuData.memberTagP[p] = MEMBER_EGRESS_UNMODIFIED;
		} else {
			vtu_entry.vtuData.memberTagP[p] = NOT_A_MEMBER;
		}
		vtu_entry.vtuData.portStateP[p] = 0;
	}

	if (gvtuAddEntry(qd_dev, &vtu_entry) != GT_OK) {
		printk("gvtuAddEntry failed\n");
		return -1;
	}

	SWITCH_DBG(SWITCH_DBG_LOAD | SWITCH_DBG_MCAST | SWITCH_DBG_VLAN, ("\n"));
	return 0;
}

int mv_switch_atu_db_flush(int db_num)
{
	if (gfdbFlushInDB(qd_dev, GT_FLUSH_ALL, db_num) != GT_OK) {
		printk("gfdbFlushInDB failed\n");
		return -1;
	}
	return 0;
}

int mv_switch_promisc_set(u16 vlan_grp_id, u16 port_map, u16 cpu_port, u8 promisc_on)
{
	int i;

	if (promisc_on) {

		mv_switch_port_based_vlan_set((port_map | (1 << cpu_port)), 0);

		for (i = 0; i < qd_dev->numOfPorts; i++) {
			if (MV_BIT_CHECK(port_map, i) && (i != cpu_port)) {
				if (mv_switch_vlan_in_vtu_set(MV_SWITCH_PORT_VLAN_ID(vlan_grp_id, i),
							      MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id),
							      (port_map | (1 << cpu_port))) != 0) {
					printk("mv_switch_vlan_in_vtu_set failed\n");
					return -1;
				}
			}
		}

	} else {

		mv_switch_port_based_vlan_set((port_map & ~(1 << cpu_port)), 0);

		for (i = 0; i < qd_dev->numOfPorts; i++) {
			if (MV_BIT_CHECK(port_map, i) && (i != cpu_port)) {
				if (mv_switch_vlan_in_vtu_set(MV_SWITCH_PORT_VLAN_ID(vlan_grp_id, i),
							      MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id),
							      (port_map & ~(1 << cpu_port))) != 0) {
					printk("mv_switch_vlan_in_vtu_set failed\n");
					return -1;
				}
			}
		}

	}

	return 0;
}

int mv_eth_switch_vlan_set(u16 vlan_grp_id, u16 port_map, u16 cpu_port)
{
	int p;

	/* set port's default private vlan id and database number (DB per group): */
	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(port_map, p) && (p != cpu_port)) {
			if (gvlnSetPortVid(qd_dev, p, MV_SWITCH_PORT_VLAN_ID(vlan_grp_id, p)) != GT_OK) {
				printk("gvlnSetPortVid failed");
				return -1;
			}
			if (gvlnSetPortVlanDBNum(qd_dev, p, MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id)) != GT_OK) {
				printk("gvlnSetPortVlanDBNum failed\n");
				return -1;
			}
		}
	}

	/* set port's port-based vlan (CPU port is not part of VLAN) */
	if (mv_switch_port_based_vlan_set((port_map & ~(1 << cpu_port)), 0) != 0)
		printk("mv_switch_port_based_vlan_set failed\n");

	/* set vtu with group vlan id (used in tx) */
	if (mv_switch_vlan_in_vtu_set(vlan_grp_id, MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id), port_map | (1 << cpu_port)) != 0)
		printk("mv_switch_vlan_in_vtu_set failed\n");

	/* set vtu with each port private vlan id (used in rx) */
	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(port_map, p) && (p != cpu_port)) {
			if (mv_switch_vlan_in_vtu_set(MV_SWITCH_PORT_VLAN_ID(vlan_grp_id, p),
						      MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id),
						      port_map & ~(1 << cpu_port)) != 0) {
				printk("mv_switch_vlan_in_vtu_set failed\n");
			}
		}
	}

	return 0;
}

#ifdef CONFIG_MV_ETH_SWITCH_LINK
void mv_switch_link_update_event(MV_U32 port_mask, int force_link_check)
{
	int p;
	unsigned short phy_cause = 0;

	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(port_mask, p)) {
			if ((!qsgmii_module) || (p == gephy_on_port)) { /* liron TODO: || (p == rgmiia_on_port)  */
				/* this is needed to clear the PHY interrupt */
				gprtGetPhyIntStatus(qd_dev, p, &phy_cause);
			} else {
				phy_cause |= GT_LINK_STATUS_CHANGED;
			}

			if (force_link_check)
				phy_cause |= GT_LINK_STATUS_CHANGED;

			if (phy_cause & GT_LINK_STATUS_CHANGED) {
				char *link = NULL, *duplex = NULL, *speed = NULL;
				GT_BOOL flag;
				GT_PORT_SPEED_MODE speed_mode;

				if (gprtGetLinkState(qd_dev, p, &flag) != GT_OK) {
					printk("gprtGetLinkState failed (port %d)\n", p);
					link = "ERR";
				} else
					link = (flag) ? "up" : "down";

				if (flag) {
					if (gprtGetDuplex(qd_dev, p, &flag) != GT_OK) {
						printk("gprtGetDuplex failed (port %d)\n", p);
						duplex = "ERR";
					} else
						duplex = (flag) ? "Full" : "Half";

					if (gprtGetSpeedMode(qd_dev, p, &speed_mode) != GT_OK) {
						printk("gprtGetSpeedMode failed (port %d)\n", p);
						speed = "ERR";
					} else {
						if (speed_mode == PORT_SPEED_1000_MBPS)
							speed = "1000Mbps";
						else if (speed_mode == PORT_SPEED_100_MBPS)
							speed = "100Mbps";
						else
							speed = "10Mbps";
					}
					mv_eth_switch_update_link(p, 1);
					printk("Port %d: Link-%s, %s-duplex, Speed-%s.\n",
					       mvBoardSwitchPortMap(MV_SWITCH_DEF_INDEX, p), link, duplex, speed);
				} else {
					mv_eth_switch_update_link(p, 0);
					printk("Port %d: Link-down\n", mvBoardSwitchPortMap(MV_SWITCH_DEF_INDEX, p));
				}
			}
		}
	}
}

void mv_switch_link_timer_function(unsigned long data)
{
	/* GT_DEV_INT_STATUS devIntStatus; */
	MV_U32 port_mask = (data & 0xFF);

	mv_switch_link_update_event(port_mask, 0);

	if (switch_link_poll) {
		switch_link_timer.expires = jiffies + (HZ);	/* 1 second */
		add_timer(&switch_link_timer);
	}
}

static irqreturn_t mv_switch_isr(int irq, void *dev_id)
{
	GT_DEV_INT_STATUS devIntStatus;
	MV_U32 port_mask = 0, reg = 0;

	if (qsgmii_module) {
		reg = MV_REG_READ(MV_ETHCOMP_INT_MAIN_CAUSE_REG);

		if (reg & MV_ETHCOMP_PCS0_LINK_INT_MASK)
			port_mask |= 0x1;
		if (reg & MV_ETHCOMP_PCS1_LINK_INT_MASK)
			port_mask |= 0x2;
		if (reg & MV_ETHCOMP_PCS2_LINK_INT_MASK)
			port_mask |= 0x4;
		if (reg & MV_ETHCOMP_PCS3_LINK_INT_MASK)
			port_mask |= 0x8;
	} else {
		if (geventGetDevIntStatus(qd_dev, &devIntStatus) != GT_OK)
			printk("geventGetDevIntStatus failed\n");

		if (devIntStatus.devIntCause & GT_DEV_INT_PHY)
			port_mask = devIntStatus.phyInt & 0xFF;
	}

	if (gephy_on_port >= 0)
		port_mask |= (1 << gephy_on_port);

	mv_switch_link_update_event(port_mask, 0);

	mv_eth_switch_interrupt_clear(qsgmii_module, gephy_on_port);

	return IRQ_HANDLED;
}

/* Get switch port link status - link state, speed & duplex */
void mv_eth_switch_port_link_status_get(unsigned int port, 
				int* link, 
				MV_ETH_PORT_DUPLEX* duplex, 
				MV_ETH_PORT_SPEED* speed)
{
    GT_BOOL flag;
    GT_PORT_SPEED_MODE speed_mode;
    int switch_port = mvBoardSwitchPortGet(0, port);

    if (switch_port == -1)
    {
	printk("gprtGetLinkState failed - invalid or irrelevant port (%d) number\n", port);
        return;
    }

    (*link) = 0;

    if(gprtGetLinkState(qd_dev, switch_port, &flag) != GT_OK) 
        printk("gprtGetLinkState failed (port %d)\n", port);
    else
	(*link) = (flag) ? 1 : 0;

    if (*link) 
    {
	if(gprtGetDuplex(qd_dev, switch_port, &flag) != GT_OK) 
	    printk("gprtGetDuplex failed (port %d)\n", port);
        else 
	    (*duplex) = (flag) ? MV_ETH_DUPLEX_FULL : MV_ETH_DUPLEX_HALF;

	if(gprtGetSpeedMode(qd_dev, switch_port, &speed_mode) != GT_OK) 
	    printk("gprtGetSpeedMode failed (port %d)\n", port);
	else 
	{
	    if (speed_mode == PORT_SPEED_1000_MBPS)
		(*speed) = MV_ETH_SPEED_1000;
	    else if (speed_mode == PORT_SPEED_100_MBPS)
		(*speed) = MV_ETH_SPEED_100;
	    else
		(*speed) = MV_ETH_SPEED_10;
	}
    }
} 
#endif /* CONFIG_MV_ETH_SWITCH_LINK */

int mv_switch_jumbo_mode_set(int max_size)
{
	int i;
	GT_JUMBO_MODE jumbo_mode;

	/* Set jumbo frames mode */
	if (max_size <= 1522)
		jumbo_mode = GT_JUMBO_MODE_1522;
	else if (max_size <= 2048)
		jumbo_mode = GT_JUMBO_MODE_2048;
	else
		jumbo_mode = GT_JUMBO_MODE_10240;

	for (i = 0; i < qd_dev->numOfPorts; i++) {
		if (gsysSetJumboMode(qd_dev, i, jumbo_mode) != GT_OK) {
			printk("gsysSetJumboMode %d failed\n", jumbo_mode);
			return -1;
		}
	}
	return 0;
}

int mv_switch_load(unsigned int switch_ports_mask)
{
	int p;

	printk("  o Loading Switch QuarterDeck driver\n");

	if (qd_dev) {
		printk("    o %s: Already initialized\n", __func__);
		return 0;
	}

	memset((char *)&qd_cfg, 0, sizeof(GT_SYS_CONFIG));
	spin_lock_init(&switch_lock);

	/* init config structure for qd package */
	qd_cfg.BSPFunctions.readMii = mv_switch_mii_read;
	qd_cfg.BSPFunctions.writeMii = mv_switch_mii_write;
	qd_cfg.BSPFunctions.semCreate = NULL;
	qd_cfg.BSPFunctions.semDelete = NULL;
	qd_cfg.BSPFunctions.semTake = NULL;
	qd_cfg.BSPFunctions.semGive = NULL;
	qd_cfg.initPorts = GT_TRUE;
	qd_cfg.cpuPortNum = mvBoardSwitchCpuPortGet(MV_SWITCH_DEF_INDEX);
	if (mvBoardSmiScanModeGet(MV_SWITCH_DEF_INDEX) == 1) {
		qd_cfg.mode.baseAddr = 0;
		qd_cfg.mode.scanMode = SMI_MANUAL_MODE;
	} else if (mvBoardSmiScanModeGet(MV_SWITCH_DEF_INDEX) == 2) {
		qd_cfg.mode.scanMode = SMI_MULTI_ADDR_MODE;
		if (mvBoardSwitchConnectedPortGet(MV_ETH_PORT_0) != -1) {
			qd_cfg.mode.baseAddr = mvBoardPhyAddrGet(MV_ETH_PORT_0);
		} else if (mvBoardSwitchConnectedPortGet(MV_ETH_PORT_1) != -1) {
			qd_cfg.mode.baseAddr = mvBoardPhyAddrGet(MV_ETH_PORT_1);
		} else {
			printk("qdLoadDriver failed: Wrong SCAN mode\n");
			return -1;
		}
	}

	/* load switch sw package */
	if (qdLoadDriver(&qd_cfg, &qddev) != GT_OK) {
		printk("qdLoadDriver failed\n");
		return -1;
	}
	qd_dev = &qddev;
	qd_cpu_port = qd_cfg.cpuPortNum;

	SWITCH_DBG(SWITCH_DBG_LOAD, ("Device ID     : 0x%x\n", qd_dev->deviceId));
	SWITCH_DBG(SWITCH_DBG_LOAD, ("Base Reg Addr : 0x%x\n", qd_dev->baseRegAddr));
	SWITCH_DBG(SWITCH_DBG_LOAD, ("No. of Ports  : %d\n", qd_dev->numOfPorts));
	SWITCH_DBG(SWITCH_DBG_LOAD, ("CPU Ports     : %ld\n", qd_dev->cpuPortNum));

	qsgmii_module = mvBoardIsQsgmiiModuleConnected();
	if (qsgmii_module)
		printk("    o QSGMII Module Detected\n");

	gephy_on_port = mvBoardGePhySwitchPortGet();
	if (gephy_on_port >= 0)
		printk("    o Internal GE PHY Connected to Switch Port %d Detected\n", gephy_on_port);

	rgmiia_on_port = mvBoardRgmiiASwitchPortGet();
	if (rgmiia_on_port >= 0)
		printk("    o RGMII-A Connected to Switch Port %d Detected\n", rgmiia_on_port);

	/* disable all disconnected ports */
	for (p = 0; p < qd_dev->numOfPorts; p++) {
		/* Do nothing for ports that are not part of the given switch_port_mask */
		if (!MV_BIT_CHECK(switch_ports_mask, p))
			continue;

		if (mvBoardSwitchPortMap(MV_SWITCH_DEF_INDEX, p) != -1) {
			/* Switch port mapped to connector on the board */

			if ((gpcsSetFCValue(qd_dev, p, GT_FALSE) != GT_OK) ||
			    (gpcsSetForcedFC(qd_dev, p, GT_FALSE) != GT_OK)) {
				printk("Force Flow Control - Failed\n");
				return -1;
			}
#if 0
			/* TODO - decide if we want to enable auto-negotiation of Flow Control for external ports */
			if (qsgmii_module) {
				/* TODO - configure ports via QSGMII registers */
			} else {
				GT_STATUS status;

				status = gprtSetPause(qd_dev, p, GT_PHY_PAUSE);
				if (status != GT_OK) {
					printk("Failed set pause for switch port #%d: status = %d\n", p, status);
				}
			}
#endif
			continue;
		}
		if ((mvBoardSwitchConnectedPortGet(MV_ETH_PORT_0) == p) ||
		    (mvBoardSwitchConnectedPortGet(MV_ETH_PORT_1) == p)) {
			/* Switch port connected to GMAC - force link UP - 1000 Full with FC */
			printk("    o Setting Switch CPU port (port #%d) for 1000 Full with FC\n", p);
			if (gpcsSetForceSpeed(qd_dev, p, PORT_FORCE_SPEED_1000_MBPS) != GT_OK) {
				printk("Force speed 1000mbps - Failed\n");
				return -1;
			}

			if ((gpcsSetDpxValue(qd_dev, p, GT_TRUE) != GT_OK) ||
			    (gpcsSetForcedDpx(qd_dev, p, GT_TRUE) != GT_OK)) {
				printk("Force duplex FULL - Failed\n");
				return -1;
			}

			if ((gpcsSetFCValue(qd_dev, p, GT_TRUE) != GT_OK) ||
			    (gpcsSetForcedFC(qd_dev, p, GT_TRUE) != GT_OK)) {
				printk("Force Flow Control - Failed\n");
				return -1;
			}

			if ((gpcsSetLinkValue(qd_dev, p, GT_TRUE) != GT_OK) ||
			    (gpcsSetForcedLink(qd_dev, p, GT_TRUE) != GT_OK)) {
				printk("Force Link UP - Failed\n");
				return -1;
			}
			continue;
		}
		printk("    o Disable disconnected switch port (port #%d) and force link down\n", p);

		if (gstpSetPortState(qd_dev, p, GT_PORT_DISABLE) != GT_OK) {
			printk("gstpSetPortState failed\n");
			return -1;
		}
		if ((gpcsSetLinkValue(qd_dev, p, GT_FALSE) != GT_OK) ||
		    (gpcsSetForcedLink(qd_dev, p, GT_TRUE) != GT_OK)) {
			printk("Force Link DOWN - Failed\n");
			return -1;
		}
	}

	return 0;
}

int mv_switch_unload(unsigned int switch_ports_mask)
{
	int i;

	printk("  o Unloading Switch QuarterDeck driver\n");

	if (qd_dev == NULL) {
		printk("    o %s: Already un-initialized\n", __func__);
		return 0;
	}

	/* Flush all addresses from the MAC address table */
	/* this also happens in mv_switch_init() but we call it here to clean-up nicely */
	/* Note: per DB address flush (gfdbFlushInDB) happens when doing ifconfig down on a Switch interface */
	if (gfdbFlush(qd_dev, GT_FLUSH_ALL) != GT_OK)
		printk("gfdbFlush failed\n");

	/* Reset VLAN tunnel mode */
	for (i = 0; i < qd_dev->numOfPorts; i++) {
		if (MV_BIT_CHECK(switch_ports_mask, i) && (i != qd_cpu_port))
			if (gprtSetVlanTunnel(qd_dev, i, GT_FALSE) != GT_OK)
				printk("gprtSetVlanTunnel failed (port %d)\n", i);
	}

	/* restore port's default private vlan id and database number to their default values after reset: */
	for (i = 0; i < qd_dev->numOfPorts; i++) {
		if (gvlnSetPortVid(qd_dev, i, 0x0001) != GT_OK) { /* that's the default according to the spec */
			printk("gvlnSetPortVid failed");
			return -1;
		}
		if (gvlnSetPortVlanDBNum(qd_dev, i, 0) != GT_OK) {
			printk("gvlnSetPortVlanDBNum failed\n");
			return -1;
		}
	}

	/* Port based VLAN */
	if (mv_switch_port_based_vlan_set(switch_ports_mask, 1))
		printk("mv_switch_port_based_vlan_set failed\n");

	/* Remove all entries from the VTU table */
	if (gvtuFlush(qd_dev) != GT_OK)
		printk("gvtuFlush failed\n");

	/* unload switch sw package */
	if (qdUnloadDriver(qd_dev) != GT_OK) {
		printk("qdUnloadDriver failed\n");
		return -1;
	}
	qd_dev = NULL;
	qd_cpu_port = -1;
	qsgmii_module = 0;
	gephy_on_port = -1;
	rgmiia_on_port = -1;

#ifdef CONFIG_MV_ETH_SWITCH_LINK
	switch_irq = -1;
	switch_link_poll = 0;
	del_timer(&switch_link_timer);
#endif /* CONFIG_MV_ETH_SWITCH_LINK */

	return 0;
}

int mv_switch_init(int mtu, unsigned int switch_ports_mask)
{
	unsigned int i, p;
	unsigned char cnt;
	GT_LPORT port_list[MAX_SWITCH_PORTS];

	/* printk("init switch layer... "); */
	if (qd_dev == NULL) {
		printk("mv_switch_init: qd_dev not initialized, call mv_switch_load() first\n");
		return -1;
	}

	/* general Switch initialization - relevant for all Switch devices */

	/* disable all ports */
	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p))
			if (gstpSetPortState(qd_dev, p, GT_PORT_DISABLE) != GT_OK) {
				printk("gstpSetPortState failed\n");
				return -1;
			}
	}

	/* flush All counters for all ports */
	if (gstatsFlushAll(qd_dev) != GT_OK) {
		printk("gstatsFlushAll failed\n");
	}

	/* set all ports not to unmodify the vlan tag on egress */
	for (i = 0; i < qd_dev->numOfPorts; i++) {
		if (MV_BIT_CHECK(switch_ports_mask, p))
			if (gprtSetEgressMode(qd_dev, i, GT_UNMODIFY_EGRESS) != GT_OK) {
				printk("gprtSetEgressMode GT_UNMODIFY_EGRESS failed\n");
				return -1;
			}
	}

	/* initializes the PVT Table (cross-chip port based VLAN) to all one's (initial state) */
	if (gpvtInitialize(qd_dev) != GT_OK) {
		printk("gpvtInitialize failed\n");
		return -1;
	}

	/* set all ports to work in Normal mode */
	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p))
			if (gprtSetFrameMode(qd_dev, p, GT_FRAME_MODE_NORMAL) != GT_OK) {
				printk("gprtSetFrameMode GT_FRAME_MODE_NORMAL failed\n");
				return -1;
			}
	}

	/* set priorities rules */
	for (i = 0; i < qd_dev->numOfPorts; i++) {
		if (MV_BIT_CHECK(switch_ports_mask, p)) {
			/* default port priority to queue zero */
			if (gcosSetPortDefaultTc(qd_dev, i, 0) != GT_OK)
				printk("gcosSetPortDefaultTc failed (port %d)\n", i);

			/* enable IP TOS Prio */
			if (gqosIpPrioMapEn(qd_dev, i, GT_TRUE) != GT_OK)
				printk("gqosIpPrioMapEn failed (port %d)\n", i);

			/* set IP QoS */
			if (gqosSetPrioMapRule(qd_dev, i, GT_FALSE) != GT_OK)
				printk("gqosSetPrioMapRule failed (port %d)\n", i);

			/* disable Vlan QoS Prio */
			if (gqosUserPrioMapEn(qd_dev, i, GT_FALSE) != GT_OK)
				printk("gqosUserPrioMapEn failed (port %d)\n", i);
		}
	}

	/* specific Switch initialization according to Switch ID */
	switch (qd_dev->deviceId) {
	case GT_88E6161:
	case GT_88E6165:
	case GT_88E6171:
	case GT_88E6351:
		/* set Header Mode in all ports to False */
		for (p = 0; p < qd_dev->numOfPorts; p++) {
			if (MV_BIT_CHECK(switch_ports_mask, p))
				if (gprtSetHeaderMode(qd_dev, p, GT_FALSE) != GT_OK) {
					printk("gprtSetHeaderMode GT_FALSE failed\n");
					return -1;
				}
		}

		if (gprtSetHeaderMode(qd_dev, qd_cpu_port, GT_TRUE) != GT_OK) {
			printk("gprtSetHeaderMode GT_TRUE failed\n");
			return -1;
		}

		mv_switch_jumbo_mode_set(mtu);
		break;

	default:
		printk("Unsupported Switch. Switch ID is 0x%X.\n", qd_dev->deviceId);
		return -1;
	}

	/* The switch CPU port is not part of the VLAN, but rather connected by tunneling to each */
	/* of the VLAN's ports. Our MAC addr will be added during start operation to the VLAN DB  */
	/* at switch level to forward packets with this DA to CPU port.                           */
	SWITCH_DBG(SWITCH_DBG_LOAD, ("Enabling Tunneling on ports: "));
	for (i = 0; i < qd_dev->numOfPorts; i++) {
		if (MV_BIT_CHECK(switch_ports_mask, i) && (i != qd_cpu_port)) {
			if (gprtSetVlanTunnel(qd_dev, i, GT_TRUE) != GT_OK) {
				printk("gprtSetVlanTunnel failed (port %d)\n", i);
				return -1;
			} else {
				SWITCH_DBG(SWITCH_DBG_LOAD, ("%d ", i));
			}
		}
	}
	SWITCH_DBG(SWITCH_DBG_LOAD, ("\n"));

	/* set cpu-port with port-based vlan to all other ports */
	SWITCH_DBG(SWITCH_DBG_LOAD, ("cpu port-based vlan:"));
	for (p = 0, cnt = 0; p < qd_dev->numOfPorts; p++) {
		if (p != qd_cpu_port) {
			SWITCH_DBG(SWITCH_DBG_LOAD, ("%d ", p));
			port_list[cnt] = p;
			cnt++;
		}
	}
	SWITCH_DBG(SWITCH_DBG_LOAD, ("\n"));
	if (gvlnSetPortVlanPorts(qd_dev, qd_cpu_port, port_list, cnt) != GT_OK) {
		printk("gvlnSetPortVlanPorts failed\n");
		return -1;
	}

	if (gfdbFlush(qd_dev, GT_FLUSH_ALL) != GT_OK) {
		printk("gfdbFlush failed\n");
	}

	mv_switch_link_detection_init();

	/* Configure Ethernet related LEDs, currently according to Switch ID */
	switch (qd_dev->deviceId) {
	case GT_88E6161:
	case GT_88E6165:
	case GT_88E6171:
	case GT_88E6351:
		break;		/* do nothing */

	default:
		for (p = 0; p < qd_dev->numOfPorts; p++) {
			if ((p != qd_cpu_port) && ((p))) {
				if (gprtSetPhyReg(qd_dev, p, 22, 0x1FFA)) {
					/* Configure Register 22 LED0 to 0xA for Link/Act */
					printk("gprtSetPhyReg failed (port=%d)\n", p);
				}
			}
		}
		break;
	}

	/* enable all relevant ports (ports connected to the MAC or external ports) */
	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(switch_ports_mask, p)) {
			if ((mvBoardSwitchPortMap(MV_SWITCH_DEF_INDEX, p) != -1) ||
			    (mvBoardSwitchConnectedPortGet(MV_ETH_PORT_0) == p) ||
			    (mvBoardSwitchConnectedPortGet(MV_ETH_PORT_1) == p)) {
				if (gstpSetPortState(qd_dev, p, GT_PORT_FORWARDING) != GT_OK) {
					printk("gstpSetPortState failed\n");
					return -1;
				}
			}
		}
	}

#ifdef SWITCH_DEBUG
	/* for debug: */
	mv_switch_status_print();
#endif
	/* printk("done\n"); */
	return 0;
}

void mv_switch_link_detection_init(void)
{
#ifdef CONFIG_MV_ETH_SWITCH_LINK

	unsigned int p;
	static int link_init_done = 0;
	unsigned int connected_phys_mask = 0;

	/* initialize link detection only once */
	if (link_init_done)
		return;

	switch_irq = mvBoardSwitchIrqGet();

	if (!qsgmii_module) {

		/* QSGMII module is not connected, Switch is working in 3xFE mode */
		connected_phys_mask = 0x0E;	/* Switch PHYs 1, 2, 3 */

		/* Enable Phy Link Status Changed interrupt at Phy level for the all enabled ports */
		for (p = 0; p < qd_dev->numOfPorts; p++) {
			if (MV_BIT_CHECK(connected_phys_mask, p) && (p != qd_cpu_port)) {
				if (gprtPhyIntEnable(qd_dev, p, (GT_LINK_STATUS_CHANGED)) != GT_OK) {
					printk("gprtPhyIntEnable failed port %d\n", p);
				}
			}
		}

		if (switch_irq != -1) {
			/* Interrupt supported */

			if ((qd_dev->deviceId == GT_88E6161) || (qd_dev->deviceId == GT_88E6165)
			    || (qd_dev->deviceId == GT_88E6351)) {
				GT_DEV_EVENT gt_event = { GT_DEV_INT_PHY, 0, connected_phys_mask };

				if (eventSetDevInt(qd_dev, &gt_event) != GT_OK)
					printk("eventSetDevInt failed\n");

				if (eventSetActive(qd_dev, GT_DEVICE_INT) != GT_OK)
					printk("eventSetActive failed\n");
			} else {
				if (eventSetActive(qd_dev, GT_PHY_INTERRUPT) != GT_OK)
					printk("eventSetActive failed\n");
			}
		}
	}

	if (gephy_on_port >= 0) {
		if (gprtPhyIntEnable(qd_dev, gephy_on_port, (GT_LINK_STATUS_CHANGED)) != GT_OK) {
			printk("gprtPhyIntEnable failed port %d\n", gephy_on_port);
		}
	}

	if (qsgmii_module)
		connected_phys_mask = 0x0F;	/* Switch ports 0, 1, 2, 3 connected to QSGMII */

	if (gephy_on_port >= 0)
		connected_phys_mask |= (1 << gephy_on_port);

	if (rgmiia_on_port >= 0)
		connected_phys_mask |= (1 << rgmiia_on_port);

	/* we want to use a timer for polling link status if no interrupt is available for all or some of the PHYs */
	if ((switch_irq == -1)) { /* liron TODO: || (rgmiia_on_port >= 0) */
		/* Use timer for polling */
		switch_link_poll = 1;
		init_timer(&switch_link_timer);
		switch_link_timer.function = mv_switch_link_timer_function;

		if (switch_irq == -1)
			switch_link_timer.data = connected_phys_mask;
		else		/* timer only for RGMII-A connected port */
			switch_link_timer.data = (1 << rgmiia_on_port);

		switch_link_timer.expires = jiffies + (HZ);	/* 1 second */
		add_timer(&switch_link_timer);
	}

	if (switch_irq != -1) {
		/* Interrupt supported */

		mv_eth_switch_interrupt_unmask(qsgmii_module, gephy_on_port);

		if (request_irq(switch_irq, mv_switch_isr, (SA_INTERRUPT | SA_SAMPLE_RANDOM), "switch", NULL))
			printk(KERN_ERR "failed to assign irq%d\n", switch_irq);
	}

	link_init_done = 1;

#endif /* CONFIG_MV_ETH_SWITCH_LINK */
}

int mv_switch_tos_get(unsigned char tos)
{
	unsigned char queue;
	int rc;

	rc = gcosGetDscp2Tc(qd_dev, tos >> 2, &queue);
	if (rc)
		return -1;

	return (int)queue;
}

int mv_switch_tos_set(unsigned char tos, int rxq)
{
	return gcosSetDscp2Tc(qd_dev, tos >> 2, (unsigned char)rxq);
}

int mv_switch_get_free_buffers_num(void)
{
	MV_U16 regVal;

	if (gsysGetFreeQSize(qd_dev, &regVal) != GT_OK) {
		printk("gsysGetFreeQSize - FAILED\n");
		return -1;
	}

	return regVal;
}

#define QD_FMT "%10lu %10lu %10lu %10lu %10lu %10lu %10lu\n"
#define QD_CNT(c, f) (GT_U32)c[0].f, (GT_U32)c[1].f, (GT_U32)c[2].f, (GT_U32)c[3].f, (GT_U32)c[4].f, (GT_U32)c[5].f, (GT_U32)c[6].f
#define QD_MAX 7
void mv_switch_stats_print(void)
{
	GT_STATS_COUNTER_SET3 counters[QD_MAX];
	GT_PORT_STAT2 port_stats[QD_MAX];
	int p;

	if (qd_dev == NULL) {
		printk("Switch is not initialized\n");
		return;
	}
	memset(counters, 0, sizeof(GT_STATS_COUNTER_SET3) * QD_MAX);

	printk("Total free buffers:      %u\n\n", mv_switch_get_free_buffers_num());

	for (p = 0; p < QD_MAX; p++) {
		if (gstatsGetPortAllCounters3(qd_dev, p, &counters[p]) != GT_OK)
			printk("gstatsGetPortAllCounters3 for port #%d - FAILED\n", p);

		if (gprtGetPortCtr2(qd_dev, p, &port_stats[p]) != GT_OK)
			printk("gprtGetPortCtr2 for port #%d - FAILED\n", p);
	}

	printk("PortNum         " QD_FMT, (GT_U32) 0, (GT_U32) 1, (GT_U32) 2, (GT_U32) 3, (GT_U32) 4, (GT_U32) 5,
	       (GT_U32) 6);
	printk("-----------------------------------------------------------------------------------------------\n");
	printk("InGoodOctetsLo  " QD_FMT, QD_CNT(counters, InGoodOctetsLo));
	printk("InGoodOctetsHi  " QD_FMT, QD_CNT(counters, InGoodOctetsHi));
	printk("InBadOctets     " QD_FMT, QD_CNT(counters, InBadOctets));
	printk("InUnicasts      " QD_FMT, QD_CNT(counters, InUnicasts));
	printk("InBroadcasts    " QD_FMT, QD_CNT(counters, InBroadcasts));
	printk("InMulticasts    " QD_FMT, QD_CNT(counters, InMulticasts));
	printk("inDiscardLo     " QD_FMT, QD_CNT(port_stats, inDiscardLo));
	printk("inDiscardHi     " QD_FMT, QD_CNT(port_stats, inDiscardHi));
	printk("InFiltered      " QD_FMT, QD_CNT(port_stats, inFiltered));

	printk("OutOctetsLo     " QD_FMT, QD_CNT(counters, OutOctetsLo));
	printk("OutOctetsHi     " QD_FMT, QD_CNT(counters, OutOctetsHi));
	printk("OutUnicasts     " QD_FMT, QD_CNT(counters, OutUnicasts));
	printk("OutMulticasts   " QD_FMT, QD_CNT(counters, OutMulticasts));
	printk("OutBroadcasts   " QD_FMT, QD_CNT(counters, OutBroadcasts));
	printk("OutFiltered     " QD_FMT, QD_CNT(port_stats, outFiltered));

	printk("OutPause        " QD_FMT, QD_CNT(counters, OutPause));
	printk("InPause         " QD_FMT, QD_CNT(counters, InPause));

	printk("Octets64        " QD_FMT, QD_CNT(counters, Octets64));
	printk("Octets127       " QD_FMT, QD_CNT(counters, Octets127));
	printk("Octets255       " QD_FMT, QD_CNT(counters, Octets255));
	printk("Octets511       " QD_FMT, QD_CNT(counters, Octets511));
	printk("Octets1023      " QD_FMT, QD_CNT(counters, Octets1023));
	printk("OctetsMax       " QD_FMT, QD_CNT(counters, OctetsMax));

	printk("Excessive       " QD_FMT, QD_CNT(counters, Excessive));
	printk("Single          " QD_FMT, QD_CNT(counters, Single));
	printk("Multiple        " QD_FMT, QD_CNT(counters, InPause));
	printk("Undersize       " QD_FMT, QD_CNT(counters, Undersize));
	printk("Fragments       " QD_FMT, QD_CNT(counters, Fragments));
	printk("Oversize        " QD_FMT, QD_CNT(counters, Oversize));
	printk("Jabber          " QD_FMT, QD_CNT(counters, Jabber));
	printk("InMACRcvErr     " QD_FMT, QD_CNT(counters, InMACRcvErr));
	printk("InFCSErr        " QD_FMT, QD_CNT(counters, InFCSErr));
	printk("Collisions      " QD_FMT, QD_CNT(counters, Collisions));
	printk("Late            " QD_FMT, QD_CNT(counters, Late));
	printk("OutFCSErr       " QD_FMT, QD_CNT(counters, OutFCSErr));
	printk("Deferred        " QD_FMT, QD_CNT(counters, Deferred));

	gstatsFlushAll(qd_dev);
}

static char *mv_str_port_state(GT_PORT_STP_STATE state)
{
	switch (state) {
	case GT_PORT_DISABLE:
		return "Disable";
	case GT_PORT_BLOCKING:
		return "Blocking";
	case GT_PORT_LEARNING:
		return "Learning";
	case GT_PORT_FORWARDING:
		return "Forwarding";
	default:
		return "Invalid";
	}
}

static char *mv_str_speed_state(int port)
{
	GT_PORT_SPEED_MODE speed;
	char *speed_str;

	if (gprtGetSpeedMode(qd_dev, port, &speed) != GT_OK) {
		printk("gprtGetSpeedMode failed (port %d)\n", port);
		speed_str = "ERR";
	} else {
		if (speed == PORT_SPEED_1000_MBPS)
			speed_str = "1 Gbps";
		else if (speed == PORT_SPEED_100_MBPS)
			speed_str = "100 Mbps";
		else
			speed_str = "10 Mbps";
	}
	return speed_str;
}

static char *mv_str_duplex_state(int port)
{
	GT_BOOL duplex;

	if (gprtGetDuplex(qd_dev, port, &duplex) != GT_OK) {
		printk("gprtGetDuplex failed (port %d)\n", port);
		return "ERR";
	} else
		return (duplex) ? "Full" : "Half";
}

static char *mv_str_link_state(int port)
{
	GT_BOOL link;

	if (gprtGetLinkState(qd_dev, port, &link) != GT_OK) {
		printk("gprtGetLinkState failed (port %d)\n", port);
		return "ERR";
	} else
		return (link) ? "Up" : "Down";
}

static char *mv_str_pause_state(int port)
{
	GT_BOOL force, pause;

	if (gpcsGetForcedFC(qd_dev, port, &force) != GT_OK) {
		printk("gpcsGetForcedFC failed (port %d)\n", port);
		return "ERR";
	}
	if (force) {
		if (gpcsGetFCValue(qd_dev, port, &pause) != GT_OK) {
			printk("gpcsGetFCValue failed (port %d)\n", port);
			return "ERR";
		}
	} else {
		if (gprtGetPauseEn(qd_dev, port, &pause) != GT_OK) {
			printk("gprtGetPauseEn failed (port %d)\n", port);
			return "ERR";
		}
	}
	return (pause) ? "Enable" : "Disable";
}

static char *mv_str_egress_mode(GT_EGRESS_MODE mode)
{
	switch (mode) {
	case GT_UNMODIFY_EGRESS:
		return "Unmodify";
	case GT_UNTAGGED_EGRESS:
		return "Untagged";
	case GT_TAGGED_EGRESS:
		return "Tagged";
	case GT_ADD_TAG:
		return "Add Tag";
	default:
		return "Invalid";
	}
}

static char *mv_str_frame_mode(GT_FRAME_MODE mode)
{
	switch (mode) {
	case GT_FRAME_MODE_NORMAL:
		return "Normal";
	case GT_FRAME_MODE_DSA:
		return "DSA";
	case GT_FRAME_MODE_PROVIDER:
		return "Provider";
	case GT_FRAME_MODE_ETHER_TYPE_DSA:
		return "EtherType DSA";
	default:
		return "Invalid";
	}
}

static char *mv_str_header_mode(GT_BOOL mode)
{
	switch (mode) {
	case GT_FALSE:
		return "False";
	case GT_TRUE:
		return "True";
	default:
		return "Invalid";
	}
}

void mv_switch_status_print(void)
{
	int p;
	GT_PORT_STP_STATE port_state = -1;
	GT_EGRESS_MODE egress_mode = -1;
	GT_FRAME_MODE frame_mode = -1;
	GT_BOOL header_mode = -1;

	if (qd_dev == NULL) {
		printk("Switch is not initialized\n");
		return;
	}
	printk("Printing Switch Status:\n");

	printk("Port   State     Link   Duplex   Speed    Pause     Egress     Frame    Header\n");
	for (p = 0; p < qd_dev->numOfPorts; p++) {

		if (gstpGetPortState(qd_dev, p, &port_state) != GT_OK) {
			printk("gstpGetPortState failed\n");
		}

		if (gprtGetEgressMode(qd_dev, p, &egress_mode) != GT_OK) {
			printk("gprtGetEgressMode failed\n");
		}

		if (gprtGetFrameMode(qd_dev, p, &frame_mode) != GT_OK) {
			printk("gprtGetFrameMode failed\n");
		}

		if (gprtGetHeaderMode(qd_dev, p, &header_mode) != GT_OK) {
			printk("gprtGetHeaderMode failed\n");
		}

		printk("%2d, %10s,  %4s,  %4s,  %8s,  %7s,  %s,  %s,  %s\n",
		       p, mv_str_port_state(port_state), mv_str_link_state(p),
		       mv_str_duplex_state(p), mv_str_speed_state(p), mv_str_pause_state(p),
		       mv_str_egress_mode(egress_mode), mv_str_frame_mode(frame_mode), mv_str_header_mode(header_mode));
	}
}

int mv_switch_reg_read(int port, int reg, int type, MV_U16 *value)
{
	GT_STATUS status;

	if (qd_dev == NULL) {
		printk("Switch is not initialized\n");
		return 1;
	}

	switch (type) {
	case MV_SWITCH_PHY_ACCESS:
		if (qsgmii_module)
			printk("warning: cannot read Switch PHY register when QSGMII module is connected\n");
		status = gprtGetPhyReg(qd_dev, port, reg, value);
		break;

	case MV_SWITCH_PORT_ACCESS:
		status = gprtGetSwitchReg(qd_dev, port, reg, value);
		break;

	case MV_SWITCH_GLOBAL_ACCESS:
		status = gprtGetGlobalReg(qd_dev, reg, value);
		break;

	case MV_SWITCH_GLOBAL2_ACCESS:
		status = gprtGetGlobal2Reg(qd_dev, reg, value);
		break;

	case MV_SWITCH_SMI_ACCESS:
		/* port means phyAddr */
		status = miiSmiIfReadRegister(qd_dev, port, reg, value);
		break;

	default:
		printk("%s Failed: Unexpected access type %d\n", __func__, type);
		return 1;
	}
	if (status != GT_OK) {
		printk("%s Failed: status = %d\n", __func__, status);
		return 2;
	}
	return 0;
}

int mv_switch_reg_write(int port, int reg, int type, MV_U16 value)
{
	GT_STATUS status;

	if (qd_dev == NULL) {
		printk("Switch is not initialized\n");
		return 1;
	}

	switch (type) {
	case MV_SWITCH_PHY_ACCESS:
		if (qsgmii_module)
			printk("warning: cannot write Switch PHY register when QSGMII module is connected\n");
		status = gprtSetPhyReg(qd_dev, port, reg, value);
		break;

	case MV_SWITCH_PORT_ACCESS:
		status = gprtSetSwitchReg(qd_dev, port, reg, value);
		break;

	case MV_SWITCH_GLOBAL_ACCESS:
		status = gprtSetGlobalReg(qd_dev, reg, value);
		break;

	case MV_SWITCH_GLOBAL2_ACCESS:
		status = gprtSetGlobal2Reg(qd_dev, reg, value);
		break;

	case MV_SWITCH_SMI_ACCESS:
		/* port means phyAddr */
		status = miiSmiIfWriteRegister(qd_dev, port, reg, value);
		break;

	default:
		printk("%s Failed: Unexpected access type %d\n", __func__, type);
		return 1;
	}
	if (status != GT_OK) {
		printk("%s Failed: status = %d\n", __func__, status);
		return 2;
	}
	return 0;
}

int mv_switch_all_multicasts_del(int db_num)
{
	GT_STATUS status = GT_OK;
	GT_ATU_ENTRY atu_entry;
	GT_U8 mc_mac[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
	GT_U8 bc_mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	memcpy(atu_entry.macAddr.arEther, &mc_mac, 6);
	atu_entry.DBNum = db_num;

	while ((status = gfdbGetAtuEntryNext(qd_dev, &atu_entry)) == GT_OK) {

		/* we don't want to delete the broadcast entry which is the last one */
		if (memcmp(atu_entry.macAddr.arEther, &bc_mac, 6) == 0)
			break;

		SWITCH_DBG(SWITCH_DBG_MCAST, ("Deleting ATU Entry: db = %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X\n",
					      atu_entry.DBNum, atu_entry.macAddr.arEther[0],
					      atu_entry.macAddr.arEther[1], atu_entry.macAddr.arEther[2],
					      atu_entry.macAddr.arEther[3], atu_entry.macAddr.arEther[4],
					      atu_entry.macAddr.arEther[5]));

		if (gfdbDelAtuEntry(qd_dev, &atu_entry) != GT_OK) {
			printk("gfdbDelAtuEntry failed\n");
			return -1;
		}
		memcpy(atu_entry.macAddr.arEther, &mc_mac, 6);
		atu_entry.DBNum = db_num;
	}

	return 0;
}

int mv_switch_port_add(int switch_port, u16 vlan_grp_id, u16 port_map)
{
	int p;

	/* Set default VLAN_ID for port */
	if (gvlnSetPortVid(qd_dev, switch_port, MV_SWITCH_PORT_VLAN_ID(vlan_grp_id, switch_port)) != GT_OK) {
		printk("gvlnSetPortVid failed");
		return -1;
	}
	/* Map port to VLAN DB */
	if (gvlnSetPortVlanDBNum(qd_dev, switch_port, MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id)) != GT_OK) {
		printk("gvlnSetPortVlanDBNum failed\n");
		return -1;
	}

	/* Add port to the VLAN (CPU port is not part of VLAN) */
	if (mv_switch_port_based_vlan_set((port_map & ~(1 << qd_cpu_port)), 0) != 0)
		printk("mv_switch_port_based_vlan_set failed\n");

	/* Add port to vtu (used in tx) */
	if (mv_switch_vlan_in_vtu_set(vlan_grp_id, MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id),
				      (port_map | (1 << qd_cpu_port)))) {
		printk("mv_switch_vlan_in_vtu_set failed\n");
	}

	/* set vtu with each port private vlan id (used in rx) */
	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(port_map, p) && (p != qd_cpu_port)) {
			if (mv_switch_vlan_in_vtu_set(MV_SWITCH_PORT_VLAN_ID(vlan_grp_id, p),
						      MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id),
						      port_map & ~(1 << qd_cpu_port)) != 0) {
				printk("mv_switch_vlan_in_vtu_set failed\n");
			}
		}
	}

	/* Enable port */
	if (gstpSetPortState(qd_dev, switch_port, GT_PORT_FORWARDING) != GT_OK) {
		printk("gstpSetPortState failed\n");
	}
#ifdef CONFIG_MV_ETH_SWITCH_LINK
	if (!qsgmii_module) {
		/* Enable Phy Link Status Changed interrupt at Phy level for the port */
		if (gprtPhyIntEnable(qd_dev, switch_port, (GT_LINK_STATUS_CHANGED)) != GT_OK) {
			printk("gprtPhyIntEnable failed port %d\n", switch_port);
		}
	}
#endif /* CONFIG_MV_ETH_SWITCH_LINK */

	return 0;
}

int mv_switch_port_del(int switch_port, u16 vlan_grp_id, u16 port_map)
{
	int p;

#ifdef CONFIG_MV_ETH_SWITCH_LINK
	if (!qsgmii_module) {
		/* Disable link change interrupts on unmapped port */
		if (gprtPhyIntEnable(qd_dev, switch_port, 0) != GT_OK)
			printk("gprtPhyIntEnable failed on port #%d\n", switch_port);
	}
#endif /* CONFIG_MV_ETH_SWITCH_LINK */

	/* Disable unmapped port */
	if (gstpSetPortState(qd_dev, switch_port, GT_PORT_DISABLE) != GT_OK)
		printk("gstpSetPortState failed on port #%d\n", switch_port);

	/* Remove port from the VLAN (CPU port is not part of VLAN) */
	if (mv_switch_port_based_vlan_set((port_map & ~(1 << qd_cpu_port)), 0) != 0)
		printk("mv_gtw_set_port_based_vlan failed\n");

	/* Remove port from vtu (used in tx) */
	if (mv_switch_vlan_in_vtu_set(vlan_grp_id, MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id),
				      (port_map | (1 << qd_cpu_port))) != 0) {
		printk("mv_gtw_set_vlan_in_vtu failed\n");
	}

	/* Remove port from vtu of each port private vlan id (used in rx) */
	for (p = 0; p < qd_dev->numOfPorts; p++) {
		if (MV_BIT_CHECK(port_map, p) && (p != qd_cpu_port)) {
			if (mv_switch_vlan_in_vtu_set(MV_SWITCH_PORT_VLAN_ID(vlan_grp_id, p),
						      MV_SWITCH_VLAN_TO_GROUP(vlan_grp_id),
						      (port_map & ~(1 << qd_cpu_port))) != 0)
				printk("mv_gtw_set_vlan_in_vtu failed\n");
		}
	}

	return 0;
}
