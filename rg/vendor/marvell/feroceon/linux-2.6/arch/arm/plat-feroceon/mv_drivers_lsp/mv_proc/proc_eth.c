/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/sysdev.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include "ctrlEnv/mvCtrlEnvLib.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "mvOs.h"
#include "ctrlEnv/mvCtrlEthCompLib.h"
#include "eth-phy/mvEthPhy.h"
#include "neta/gbe/mvNeta.h"
#include "mv_neta/net_dev/mv_netdev.h"

#ifdef WAN_SWAP_FEATURE

static struct proc_dir_entry *eth_config_proc;

#if 0
#define SWITCH_PHY_ADDR	9
#define RGMII_PHY_ADDR	8
#define GEPHY_PHY_ADDR	7
#else
#define SWITCH_PHY_ADDR	8
#define RGMII_PHY_ADDR	0x13
#define GEPHY_PHY_ADDR	9
#endif

static int do_eth_cmp_switch(int wan_mode)
{
	MV_U32 oldCfg;
	MV_U32 newCfg;
	static int prev_wan_mode = MV_WAN_MODE_MOCA; /* new mode: 0 - MoCA, 1 - GbE */
	static MV_U32 firstInit = 1;
	MV_STATUS status = 0;

	if (prev_wan_mode == wan_mode) {
		/* do nothing */
		printk("Requested WAN mode is identical to the current mode - doing nothing...\n");
		return 0;
	}

	if (mv_eth_check_all_ports_down())
		return -1;

	oldCfg = mvBoardEthComplexConfigGet();
	if (oldCfg & ESC_OPT_MAC1_2_SW_P5) {
		/* sanity check */
		if (prev_wan_mode != MV_WAN_MODE_MOCA)
			printk("Error: prev_wan_mode is GbE but configuration is MoCA\n");

		/* GbE mode */
		newCfg = oldCfg & ~(ESC_OPT_MAC1_2_SW_P5 | ESC_OPT_RGMIIB_MAC0);
		newCfg |= ESC_OPT_MAC0_2_SW_P4;
		newCfg |= ESC_OPT_GEPHY_MAC1; 
		mvEthGmacRgmiiSet(0, 0);
		mvEthGmacRgmiiSet(1, 0);
		mvBoardMacSpeedSet(0, BOARD_MAC_SPEED_1000M);
		mvBoardMacSpeedSet(1, BOARD_MAC_SPEED_AUTO);
		mvBoardPhyAddrSet(0, SWITCH_PHY_ADDR);
		mvBoardPhyAddrSet(1, GEPHY_PHY_ADDR);
		mvCtrlEthComplexMppUpdate(ESC_OPT_RGMIIB_MAC0, MV_FALSE);
/*#if 0*/
		mvOsDelay(1);
		status = mvEthPhyRestartAN(mvBoardPhyAddrGet(1), 0);
		if (status != MV_OK)
			printk("Phy restart auto-negotiation failed\n");
/*#endif*/
	} else {
		/* sanity check */
		if (prev_wan_mode != MV_WAN_MODE_GBE)
			printk("Error: prev_wan_mode is MoCA but configuration is GbE\n");

		/* MoCA mode */
		newCfg = oldCfg & ~(ESC_OPT_MAC0_2_SW_P4 | ESC_OPT_GEPHY_MAC1);
		newCfg |= ESC_OPT_MAC1_2_SW_P5;
		newCfg |= ESC_OPT_RGMIIB_MAC0; 
		mvEthGmacRgmiiSet(0, 0);
		mvEthGmacRgmiiSet(1, 0);
		mvBoardMacSpeedSet(0, BOARD_MAC_SPEED_100M);
		mvBoardMacSpeedSet(1, BOARD_MAC_SPEED_1000M);		
		mvBoardPhyAddrSet(0, RGMII_PHY_ADDR);
		mvBoardPhyAddrSet(1, SWITCH_PHY_ADDR);
		mvCtrlEthComplexMppUpdate(ESC_OPT_RGMIIB_MAC0, MV_TRUE);
#if 0
		mvOsDelay(1);
		status = mvEthPhyRestartAN(mvBoardPhyAddrGet(0), 5000);
		if (status != MV_OK)
			printk("Phy restart auto-negotiation failed\n");
#endif
	}

	mvBoardEthComplexConfigSet(newCfg);
	mvBoardSwitchInfoUpdate();
	mvEthernetComplexChangeMode(oldCfg, newCfg);

	if ((firstInit) && (newCfg & ESC_OPT_GEPHY_MAC1)) {
		firstInit = 0;
		mvOsDelay(1);
		mvEthInternalGEPhyBasicInit(mvBoardPhyAddrGet(1));
	}

	mvBoardMppModuleTypePrint();

	status = mv_eth_ctrl_wan_mode(wan_mode);
	if (status == MV_OK)
		prev_wan_mode = wan_mode;
	
	return status;
}

int do_gbe_phy_link_status(void)
{
	MV_STATUS status = 0;
#if 0
	MV_U16 phy_data = 0;
	unsigned long flags;
	int link, resolved, duplex, speed;

	raw_local_irq_save(flags);
	status = mvEthPhyRestartAN(GEPHY_PHY_ADDR, 5000);
	status |= mvEthPhyRegRead(GEPHY_PHY_ADDR, 17, &phy_data);	/* Read PHY Specific Status Register - Copper */
	raw_local_irq_restore(flags);

	if (status == MV_OK) {
		link = (phy_data & BIT10);
		if (link) {
			resolved = (phy_data & BIT11);
			printk("GbE PHY: link up");
			if (resolved) {
				duplex = (phy_data & BIT13);
				speed = ((phy_data & (BIT14 | BIT15)) >> 14);
				printk(", %s duplex, speed ", duplex ? "full" : "half");
				if (speed == 0x2) 
					printk("1 Gbps\n");
				else if (speed == 0x1) 
					printk("100 Mbps\n");
				else if (speed == 0x0) 
					printk("10 Mbps\n");
				else
					printk("unknown\n");
			}
			else {
				printk(", duplex and speed unresolved\n");
			}
		}
		else {
			printk("GbE PHY: link down\n");
		}
	}
#endif
	return status;
}

int eth_config_proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
	/* Switch ethernet complex configuration. */
	if (!strncmp(buffer, "moca", 4))
		do_eth_cmp_switch(0);
	else if (!strncmp(buffer, "eth", 3))
		do_eth_cmp_switch(1);
	else if (!strncmp(buffer, "phy", 3))
		do_gbe_phy_link_status();
	else {
		printk("Usage: \n");
		printk("'echo moca > /proc/eth_config' for MoCA WAN\n");
		printk("'echo gbe > /proc/eth_config' for GbE WAN\n");
		/* printk("'echo phy > /proc/eth_config' to check GbE PHY link status\n"); */
	}

	return count;
}


int eth_config_proc_read(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *zero, void *ptr)
{
	MV_U32 ethComp;

	if (offset > 0)
		return 0;

	ethComp = mvBoardEthComplexConfigGet();

	if (ethComp & ESC_OPT_MAC0_2_SW_P4)
		return sprintf(buffer, "GbE WAN Mode: Switch on MAC0, GE-PHY on MAC1.\n");
	else if (ethComp & ESC_OPT_MAC1_2_SW_P5)
		return sprintf(buffer, "MoCA WAN Mode: Switch on MAC1, RGMII-B on MAC0.\n");
	return 0;
}


int __init eth_config_proc_init(void)
{

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	eth_config_proc = create_proc_entry("eth_config", 0666, &proc_root);
#else
	eth_config_proc = create_proc_entry("eth_config", 0666, NULL);
#endif
	eth_config_proc->read_proc = eth_config_proc_read;
	eth_config_proc->write_proc = eth_config_proc_write;
	eth_config_proc->nlink = 1;

	return 0;
}

module_init(eth_config_proc_init);

#endif /* WAN_SWAP_FEATURE */
