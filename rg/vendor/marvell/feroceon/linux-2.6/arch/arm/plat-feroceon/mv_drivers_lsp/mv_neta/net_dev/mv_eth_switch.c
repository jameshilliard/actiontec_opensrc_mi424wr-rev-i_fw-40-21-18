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

#include "mvCommon.h"  /* Should be included before mvSysHwConfig */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/pci.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/version.h>
#include <net/ip.h>
#include <net/xfrm.h>

#include "mvOs.h"
#include "dbg-trace.h"
#include "mvSysHwConfig.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "ctrlEnv/mvCtrlEthCompLib.h"

#include "mv_switch.h"
#include "mv_netdev.h"

/* Example: "mv_net_config=4,(00:99:88:88:99:77,0)(00:55:44:55:66:77,1:2:3:4)(00:11:22:33:44:55,),mtu=1500" */
static char			*net_config_str = NULL;
struct mv_eth_switch_config     switch_net_config;
static int                      mv_eth_switch_started = 0;
unsigned int                    switch_enabled_ports = 0;

/* Required to get the configuration string from the Kernel Command Line */
int mv_eth_cmdline_config(char *s);
__setup("mv_net_config=", mv_eth_cmdline_config);

int mv_eth_cmdline_config(char *s)
{
	net_config_str = s;
	return 1;
}

static struct net_device* mv_eth_main_net_dev_get(void)
{
    int                 i;
    struct eth_port     *priv;
    struct net_device   *dev;

    for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) {
        dev = mv_net_devs[i];
        priv = MV_ETH_PRIV(dev);

        if (netif_running(dev) && (priv->flags & MV_ETH_F_SWITCH))
            return dev;
    }
    return NULL;
}

/* Local function prototypes */
static int mv_eth_check_open_bracket(char **p_net_config)
{
	if (**p_net_config == '(') {
		(*p_net_config)++;
		return 0;
	}
	printk("Syntax error: could not find opening bracket\n");
	return -EINVAL;
}

static int mv_eth_check_closing_bracket(char **p_net_config)
{
	if (**p_net_config == ')') {
		(*p_net_config)++;
		return 0;
	}
	printk("Syntax error: could not find closing bracket\n");
	return -EINVAL;
}

static int mv_eth_check_comma(char **p_net_config)
{
	if (**p_net_config == ',') {
		(*p_net_config)++;
		return 0;
	}
	printk("Syntax error: could not find comma\n");
	return -EINVAL;
}

static int mv_eth_netconfig_mac_addr_get(char **p_net_config, int idx)
{
	int     num;
	char *config_str = *p_net_config;
	MV_U32  mac[MV_MAC_ADDR_SIZE];

	/* the MAC address should look like: 00:99:88:88:99:77 */
	/* that is, 6 two-digit numbers, separated by :        */
	num = sscanf(config_str, "%2x:%2x:%2x:%2x:%2x:%2x", 
		&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
	if (num == MV_MAC_ADDR_SIZE)
	{                
		while (--num >= 0)
			switch_net_config.mac_addr[idx][num] = (mac[num] & 0xFF);

		(*p_net_config) = config_str + 17;
		return 0;
	}
	printk("Syntax error while parsing MAC address from command line\n");
	return -EINVAL;
}

static int mv_eth_netconfig_ports_get(char **p_net_config, int idx)
{
	char ch;
	char *config_str = *p_net_config;
	int  port, mask = 0, status = -EINVAL;

	/* the port list should look like this: */
	/* example 0: )         - no ports */
	/* example 1: 0)        - single port 0 */
	/* example 2: 1:2:3:4)  - multiple ports */

	while (1)
	{
		ch = *config_str++;
    
		if (ch == ')') /* Finished */
		{
			status = 0;
			break;
		}
		port = mvCharToDigit(ch);
		if (port < 0)
			break;

		/* TBD - Check port validity */
		mask |= (1 << port);

		if (*config_str == ':') 
			config_str++;
	}
	*p_net_config = config_str;

	if (status == 0) {
		switch_net_config.board_port_map[idx] = mask;
		return 0;
	}
	printk("Syntax error while parsing port mask from command line\n");
	return -EINVAL;
}

/* the mtu value is constructed as follows: */
/* mtu=value                                */
static int  mv_eth_netconfig_mtu_get(char **p_net_config)
{
	unsigned int mtu;

	if (strncmp(*p_net_config, "mtu=", 4) == 0)
	{
		*p_net_config += 4;
		mtu = strtol(*p_net_config, p_net_config, 10);
		if (mtu > 0)
		{
			switch_net_config.mtu = mtu;
			printk("      o MTU set to %d.\n", mtu);
			return 0;
		}
		printk("Syntax error while parsing mtu value from command line\n");
		return -EINVAL;
	}

	switch_net_config.mtu = 1500;
	printk("      o Using default MTU %d\n", switch_net_config.mtu); 
	return 0;
}

static int mv_eth_netconfig_max_get(char **p_net_config)
{
	char num = **p_net_config;
	int netdev_num;

	netdev_num = mvCharToDigit(num);
	if (netdev_num >= 0) {
		switch_net_config.netdev_max = netdev_num;
		(*p_net_config) += 1;
		return 0;
	}
	printk("Syntax error while parsing number of netdevs from command line\n");
	return -EINVAL;
}

int mv_eth_switch_config_get(int use_existing_config)
{
	char *p_net_config;
	int i = 0;

	if (!use_existing_config) {	
		memset(&switch_net_config, 0, sizeof(switch_net_config));

		if (net_config_str != NULL) {
			printk("      o Using UBoot netconfig string\n");
		}
		else {
			printk("      o Using default netconfig string from Kconfig\n");
			net_config_str = CONFIG_MV_ETH_SWITCH_NETCONFIG;
		}
		printk("        net_config_str: %s\n", net_config_str);

		p_net_config = net_config_str;
		if (mv_eth_netconfig_max_get(&p_net_config))
			return -EINVAL;

		if (switch_net_config.netdev_max == 0)
			return 1;

		if (switch_net_config.netdev_max > CONFIG_MV_ETH_SWITCH_NETDEV_NUM) {
			printk("Too large number of netdevs (%d) in command line: cut to %d\n",
				switch_net_config.netdev_max, CONFIG_MV_ETH_SWITCH_NETDEV_NUM);
			switch_net_config.netdev_max = CONFIG_MV_ETH_SWITCH_NETDEV_NUM;
		}
   
		if (mv_eth_check_comma(&p_net_config))
			return -EINVAL;

		for (i = 0; (i < CONFIG_MV_ETH_SWITCH_NETDEV_NUM) && (*p_net_config != '\0'); i++) {
			if (mv_eth_check_open_bracket(&p_net_config))
				return -EINVAL;

			if (mv_eth_netconfig_mac_addr_get(&p_net_config, i))
				return -EINVAL;

			if (mv_eth_check_comma(&p_net_config))
				return -EINVAL;

			if (mv_eth_netconfig_ports_get(&p_net_config, i))
				return -EINVAL;

			switch_net_config.netdev_cfg++;

			/* If we have a comma after the closing bracket, then interface */
			/* definition is done.                                          */
			if (*p_net_config == ',') {
				p_net_config++;
				break;
			}
		}

		/* there is a chance the previous loop did not end because a comma was found but because	*/
		/* the maximum number of interfaces was reached, so check for the comma now.		*/
		if (i == CONFIG_MV_ETH_SWITCH_NETDEV_NUM)
			if (mv_eth_check_comma(&p_net_config))
				return -EINVAL;

		if (*p_net_config != '\0')
			if (mv_eth_netconfig_mtu_get(&p_net_config))
				return -EINVAL;
    
		/* at this point, we have parsed up to CONFIG_MV_ETH_SWITCH_NETDEV_NUM, and the mtu value */
		/* if the net_config string is not finished yet, then its format is invalid */
		if (*p_net_config != '\0')
		{
			printk("Switch netconfig string is too long: %s\n", p_net_config);
			return -EINVAL;
		}
	}
	else {
		/* leave most of the configuration as-is, but update MAC addresses */
		/* MTU is saved in mv_eth_switch_change_mtu */

		/* Note: at this point, since this is a re-init, mv_eth_switch_netdev_first */
		/* and mv_eth_switch_netdev_last, as well as mv_net_devs[], are valid.      */
		for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) {		
			if (mv_net_devs[i] != NULL)
				memcpy(switch_net_config.mac_addr[i - mv_eth_switch_netdev_first], mv_net_devs[i]->dev_addr, MV_MAC_ADDR_SIZE);
		}

		if (switch_net_config.netdev_max == 0)
			return 1;
	}

	return 0;
}

int    mv_eth_switch_set_mac_addr(struct net_device *dev, void *mac)
{
	struct eth_netdev *dev_priv = MV_DEV_PRIV(dev);
	struct sockaddr *addr = (struct sockaddr *)mac;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	/* remove old mac addr from VLAN DB */
	mv_switch_mac_addr_set(dev->dev_addr, MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id), (1 << dev_priv->cpu_port), 0);

	memcpy(dev->dev_addr, addr->sa_data, MV_MAC_ADDR_SIZE);

	/* add new mac addr to VLAN DB */
	mv_switch_mac_addr_set(dev->dev_addr, MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id), (1 << dev_priv->cpu_port), 1);

	printk("mv_eth_switch: %s change mac address to %02x:%02x:%02x:%02x:%02x:%02x\n", 
		dev->name, *(dev->dev_addr), *(dev->dev_addr+1), *(dev->dev_addr+2), 
		*(dev->dev_addr+3), *(dev->dev_addr+4), *(dev->dev_addr+5));

	return 0;
}

void    mv_eth_switch_set_multicast_list(struct net_device *dev)
{
	struct eth_netdev *dev_priv = MV_DEV_PRIV(dev);
	int i = 0;
	struct dev_mc_list *curr_addr = dev->mc_list;

	if (dev->flags & IFF_PROMISC) {
		/* promiscuous mode - connect the CPU port to the VLAN (port based + 802.1q) */
		/* printk("mv_eth_switch: setting promiscuous mode\n"); */
		if (mv_switch_promisc_set(dev_priv->vlan_grp_id, dev_priv->port_map, dev_priv->cpu_port, 1))
			printk("mv_switch_promisc_set to 1 failed\n");
	}
	else {
		/* not in promiscuous mode - disconnect the CPU port to the VLAN (port based + 802.1q) */
		if (mv_switch_promisc_set(dev_priv->vlan_grp_id, dev_priv->port_map, dev_priv->cpu_port, 0))
			printk("mv_switch_promisc_set to 0 failed\n");

		if (dev->flags & IFF_ALLMULTI) {
			/* allmulticast - not supported. There is no way to tell the Switch to accept only	*/
			/* the multicast addresses but not Unicast addresses, so the alternatives are:	*/
			/* 1) Don't support multicast and do nothing					*/
			/* 2) Support multicast with same implementation as promiscuous			*/
			/* 3) Don't rely on Switch for MAC filtering, but use PnC			*/
			/* Currently option 1 is selected						*/
			printk("mv_eth_switch: setting all-multicast mode is not supported\n");
		}

		/* Add or delete specific multicast addresses:						*/
		/* Linux provides a list of the current multicast addresses for the device.		*/
		/* First, we delete all the multicast addresses in the ATU.				*/
		/* Then we add the specific multicast addresses Linux provides.				*/
		if (mv_switch_all_multicasts_del(MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id)))
			printk("mv_eth_switch: mv_switch_all_multicasts_del failed\n");

		if (dev->mc_count) {
			/* accept specific multicasts */
			for (i = 0; i < dev->mc_count; i++, curr_addr = curr_addr->next) {
				if (!curr_addr)
					break;

				/*
				printk("Setting MC = %02X:%02X:%02X:%02X:%02X:%02X\n", 
				curr_addr->dmi_addr[0], curr_addr->dmi_addr[1], curr_addr->dmi_addr[2], 
				curr_addr->dmi_addr[3], curr_addr->dmi_addr[4], curr_addr->dmi_addr[5]);
				*/
				mv_switch_mac_addr_set(curr_addr->dmi_addr,
					MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id), 
					(dev_priv->port_map | (1 << dev_priv->cpu_port)), 1);
			}
		}
	}
}

int     mv_eth_switch_change_mtu(struct net_device *dev, int mtu)
{
	int i;
	struct eth_port *priv = MV_ETH_PRIV(dev);

	if (netif_running(dev)) {
		printk("mv_eth_switch does not support changing MTU for active interfaces.\n"); 
		return -EPERM;
	}

	/* check mtu - can't change mtu if there is a gateway interface that */
	/* is currently up and has a different mtu */
	for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) {		
		if (	(mv_net_devs[i] != NULL)	&& 
			(mv_net_devs[i]->mtu != mtu)	&& 
			(netif_running(mv_net_devs[i])) ) 
		{		
			printk(KERN_ERR "All gateway devices must have same MTU\n");
			return -EPERM;
		}
	}

	if (dev->mtu != mtu) {
		int old_mtu = dev->mtu;

		/* stop tx/rx activity, mask all interrupts, relese skb in rings,*/
		mv_eth_stop_internals(priv);

		if (mv_eth_change_mtu_internals(dev, mtu) == -1) 
			return -EPERM;

		/* fill rx buffers, start rx/tx activity, set coalescing */
		if(mv_eth_start_internals(priv, dev->mtu) != 0 ) {
			printk( KERN_ERR "%s: start internals failed\n", dev->name );
			return -EPERM;
		}

		printk( KERN_NOTICE "%s: change mtu %d (pkt-size %d) to %d (pkt-size %d)\n",
			dev->name, old_mtu, RX_PKT_SIZE(old_mtu), 
			dev->mtu, RX_PKT_SIZE(dev->mtu) );
	}

	if (switch_net_config.mtu != mtu) {				
		mv_switch_jumbo_mode_set(RX_PKT_SIZE(mtu));
		switch_net_config.mtu = mtu;
	}
	
	return 0;
} 

int    mv_eth_switch_start(struct net_device *dev)
{
	struct eth_port	*priv = MV_ETH_PRIV(dev);
	struct eth_netdev *dev_priv = MV_DEV_PRIV(dev);
	unsigned char broadcast[MV_MAC_ADDR_SIZE] = {0xff,0xff,0xff,0xff,0xff,0xff};
	int i;

	/* check mtu */
	for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) {		
		if ((mv_net_devs[i] != NULL) && (mv_net_devs[i]->mtu != dev->mtu) ) {		
			printk(KERN_ERR "All gateway devices must have same MTU\n");
			return -EPERM;
		}
	}

	/* in default link is down */
	netif_carrier_off(dev);

	/* Stop the TX queue - it will be enabled upon PHY status change after link-up interrupt/timer */
	netif_stop_queue(dev);

	/* start upper layer accordingly with ports_map */
#ifdef CONFIG_MV_ETH_SWITCH_LINK
	dev_priv->link_map = 0;
	mv_switch_link_update_event(dev_priv->port_map, 1);
#else
	dev_priv->link_map = dev_priv->port_map;
#endif /* CONFIG_MV_ETH_SWITCH_LINK */ 

	if (mv_eth_switch_started == 0)
	{
		/* enable polling on the port, must be used after netif_poll_disable */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		netif_poll_enable(dev);
#else
		napi_enable(&priv->napi);
#endif
		/* fill rx buffers, start rx/tx activity, set coalescing */
		if (mv_eth_start_internals(priv, dev->mtu) != 0) {
			printk( KERN_ERR "%s: start internals failed\n", dev->name );
			goto error;
		}

		priv->dev = dev; /* store this device in priv for NAPI use */

		/* connect to port interrupt line */
		if (request_irq(dev->irq, mv_eth_isr, SA_INTERRUPT|SA_SAMPLE_RANDOM, "mv_eth", priv)) {
			printk(KERN_ERR "cannot request irq %d for %s port %d\n", 
				dev->irq, dev->name, priv->port);
			dev->irq = 0;
			goto error;
		}

		/* unmask interrupts */
		mv_eth_interrupts_unmask(priv);
	}

	mv_eth_switch_started++;

	/* Add our MAC addr to the VLAN DB at switch level to forward packets with this DA   */
	/* to CPU port by using the tunneling feature. The device is always in promisc mode. */
	mv_switch_mac_addr_set(dev->dev_addr, MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id), (1 << dev_priv->cpu_port), 1);

	/* We also need to allow L2 broadcasts comming up for this interface */ 
	mv_switch_mac_addr_set(broadcast, MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id), 
			(dev_priv->port_map | (1 << dev_priv->cpu_port)), 1);

	printk("%s: started (on switch)\n", dev->name);
	return 0;

error:
	printk( KERN_ERR "%s: start failed\n", dev->name);
	return -1;
}

int     mv_eth_switch_stop(struct net_device *dev)
{
	unsigned long flags;
	struct eth_port *priv = MV_ETH_PRIV(dev);
	struct eth_netdev *dev_priv = MV_DEV_PRIV(dev);

	/* stop upper layer */
	netif_carrier_off(dev);
	netif_stop_queue(dev);
    
	/* stop switch from forwarding packets from this VLAN toward CPU port */
	mv_switch_atu_db_flush(MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id));

	/* It is possible that the interface is in promiscuous mode */
	/* If so, the CPU port is connected with port based VLAN to the other ports, and */
	/* we must disconnect it now to stop the Switch from forwarding packets to the CPU */
	/* when the interface is down. */
	/* mv_eth_switch_set_multicast_list will be called anyway by Linux when we do ifconfig up */
	/* and will re-set the promiscuous feature if needed */
	if (dev->flags & IFF_PROMISC) { 
		if (mv_switch_promisc_set(dev_priv->vlan_grp_id, dev_priv->port_map, dev_priv->cpu_port, 0))
			printk("mv_switch_promisc_set to 0 failed\n");
	}
	mv_eth_switch_started--;
	if (mv_eth_switch_started == 0)	{
		/* first make sure that the port finished its Rx polling - see tg3 */
		/* otherwise it may cause issue in SMP, one CPU is here and the other is doing the polling */
		/* and both of it are messing with the descriptors rings!! */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		netif_poll_disable(dev);
#else
		napi_disable(&priv->napi);
#endif
		spin_lock_irqsave(priv->lock, flags);

		/* stop tx/rx activity, mask all interrupts, relese skb in rings,*/
		mv_eth_stop_internals(priv);

		del_timer(&priv->tx_done_timer);
		priv->flags &= ~MV_ETH_F_TX_DONE_TIMER;
		del_timer(&priv->cleanup_timer);
		priv->flags &= ~MV_ETH_F_CLEANUP_TIMER;
 
		spin_unlock_irqrestore(priv->lock, flags);

		if (dev->irq != 0)
			free_irq(dev->irq, priv);
	}
	else {
		if (priv->dev == dev)
		{
			struct net_device *main_dev = mv_eth_main_net_dev_get();

			if (main_dev != NULL)
				priv->dev = main_dev;
			else
				printk(KERN_ERR "No main device found\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
			netif_poll_disable(dev);
			netif_poll_enable(priv->dev);
#endif
		}
	}
	printk( KERN_NOTICE "%s: stopped\n", dev->name );

	return 0;
}

#ifdef CONFIG_MV_ETH_SWITCH_LINK

void mv_eth_switch_interrupt_unmask(int qsgmii_module, int gephy_on_port)
{
	MV_U32 reg;

	reg = MV_REG_READ(MV_ETHCOMP_INT_MAIN_MASK_REG);
 
	if (qsgmii_module) {
		reg |= (MV_ETHCOMP_PCS0_LINK_INT_MASK | 
			MV_ETHCOMP_PCS1_LINK_INT_MASK | 
			MV_ETHCOMP_PCS2_LINK_INT_MASK | 
			MV_ETHCOMP_PCS3_LINK_INT_MASK);
	}

	if (gephy_on_port >= 0) {
		reg |= MV_ETHCOMP_GEPHY_INT_MASK; 
	} 

	reg |= MV_ETHCOMP_SWITCH_INT_MASK;

	MV_REG_WRITE(MV_ETHCOMP_INT_MAIN_MASK_REG, reg);
}

void mv_eth_switch_interrupt_clear(int qsgmii_module, int gephy_on_port)
{
	MV_U32 reg;

	reg = MV_REG_READ(MV_ETHCOMP_INT_MAIN_CAUSE_REG);

	if (qsgmii_module) {
		reg &= ~(MV_ETHCOMP_PCS0_LINK_INT_MASK | 
			 MV_ETHCOMP_PCS1_LINK_INT_MASK | 
			 MV_ETHCOMP_PCS2_LINK_INT_MASK | 
			 MV_ETHCOMP_PCS3_LINK_INT_MASK);
	}

	if (gephy_on_port >= 0) {
		reg &= ~MV_ETHCOMP_GEPHY_INT_MASK;
	}

	reg &= ~MV_ETHCOMP_SWITCH_INT_MASK;

	MV_REG_WRITE(MV_ETHCOMP_INT_MAIN_CAUSE_REG, reg);
}

void mv_eth_switch_update_link(unsigned int p, unsigned int link_up)
{
	struct eth_netdev *dev_priv;
	struct eth_port *priv;
	int i = 0;
	unsigned int prev_ports_link = 0;

	for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) 
	{
		if (mv_net_devs[i] == NULL)
			break;

		dev_priv = MV_DEV_PRIV(mv_net_devs[i]);
		if (dev_priv == NULL)
			break;

		priv = MV_ETH_PRIV(mv_net_devs[i]);
		if (priv == NULL)
			break;

		if ((dev_priv->port_map & (1 << p)) == 0)
			continue;

		prev_ports_link = dev_priv->link_map;
 
		if (link_up)
			dev_priv->link_map |= (1 << p);
		else
			dev_priv->link_map &= ~(1 << p);

		if ( (prev_ports_link != 0) && (dev_priv->link_map == 0) && netif_running(mv_net_devs[i]) )
		{
			/* link down */
			netif_carrier_off(mv_net_devs[i]);
			netif_stop_queue(mv_net_devs[i]);
			printk("%s: link down\n", mv_net_devs[i]->name);
		}
		else if ( (prev_ports_link == 0) && (dev_priv->link_map != 0)  && netif_running(mv_net_devs[i]) )
		{
			/* link up */
			if (mv_eth_ctrl_is_tx_enabled(priv) == 1) {
				netif_carrier_on(mv_net_devs[i]);
				netif_wake_queue(mv_net_devs[i]);
				printk("%s: link up\n", mv_net_devs[i]->name);
			}
		}
	}
}

#endif /* CONFIG_MV_ETH_SWITCH_LINK */

int     mv_eth_switch_port_add(struct net_device *dev, int port)
{
	struct eth_netdev *dev_priv = MV_DEV_PRIV(dev);
	int i, switch_port, err = 0;

	if (dev_priv == NULL) {
		printk("%s is not connected to the switch\n", dev->name);
		return 1;
	}

	if (netif_running(dev)) {
		printk("%s must be down to change switch ports map\n", dev->name);
		return 1;
	}

	switch_port = mvBoardSwitchPortGet(0, port);

	if (switch_port < 0 ) {
		printk("Switch port %d can't be added\n", port);
		return 1;        
	}

	if (MV_BIT_CHECK(switch_enabled_ports, switch_port))
	{
		printk("Switch port %d is already enabled\n", port);
		return 0;
	}

	/* Update data base */
	dev_priv->port_map |= (1 << switch_port);
	for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) {		
		if ((mv_net_devs[i] != NULL) && (mv_net_devs[i] == dev))
			switch_net_config.board_port_map[i - mv_eth_switch_netdev_first] |= (1 << switch_port);
	}
	switch_enabled_ports |= (1 << switch_port);
	dev_priv->tx_vlan_mh = cpu_to_be16((MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id) << 12) | dev_priv->port_map);

	err = mv_switch_port_add(switch_port, dev_priv->vlan_grp_id, dev_priv->port_map);
	if (!err)
		printk("%s: Switch port #%d mapped\n", dev->name, port);

	return err;
}

int     mv_eth_switch_port_del(struct net_device *dev, int port)
{
	struct eth_netdev *dev_priv = MV_DEV_PRIV(dev);
	int i, switch_port, err = 0;

	if (dev_priv == NULL) {
		printk("%s is not connected to the switch\n", dev->name);
		return 1;
	}

	if (netif_running(dev)) {
		printk("%s must be down to change switch ports map\n", dev->name);
		return 1;
	}

	switch_port = mvBoardSwitchPortGet(0, port);

	if (switch_port < 0 ) {
		printk("Switch port %d can't be added\n", port);
		return 1;        
	}

	if (!MV_BIT_CHECK(switch_enabled_ports, switch_port))
	{
		printk("Switch port %d is already disabled\n", port);
		return 0;
	}

	if (!MV_BIT_CHECK(dev_priv->port_map, switch_port))
	{
		printk("Switch port %d is not mapped on %s\n", port, dev->name);
		return 0;
	}

	/* Update data base */
	dev_priv->port_map &= ~(1 << switch_port);
	for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) {		
		if ((mv_net_devs[i] != NULL) && (mv_net_devs[i] == dev))
			switch_net_config.board_port_map[i - mv_eth_switch_netdev_first] &= ~(1 << switch_port);
	}
	dev_priv->link_map &= ~(1 << switch_port);
	switch_enabled_ports &= ~(1 << switch_port);
	dev_priv->tx_vlan_mh = cpu_to_be16((MV_SWITCH_VLAN_TO_GROUP(dev_priv->vlan_grp_id) << 12) | dev_priv->port_map);

	err = mv_switch_port_del(switch_port, dev_priv->vlan_grp_id, dev_priv->port_map);
	if (!err)
		printk("%s: Switch port #%d unmapped\n", dev->name, port);

	return err;
}

void    mv_eth_switch_status_print(int port)
{
	int i;
	struct eth_port *pp = mv_eth_port_by_id(port);
	struct net_device *dev;

	if (pp->flags & MV_ETH_F_SWITCH) {
		printk("ethPort=%d: mv_eth_switch status - pp=%p, flags=0x%x\n", port, pp, pp->flags);

		printk("mtu=%d, netdev_max=%d, netdev_cfg=%d, first=%d, last=%d\n",  
			switch_net_config.mtu, switch_net_config.netdev_max, switch_net_config.netdev_cfg, 
			mv_eth_switch_netdev_first, mv_eth_switch_netdev_last);

		for (i = 0; i < switch_net_config.netdev_cfg; i++)
		{
			printk("MAC="MV_MACQUAD_FMT", board_port_map=0x%x\n", 
				MV_MACQUAD(switch_net_config.mac_addr[i]), switch_net_config.board_port_map[i]);
		}
		for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++)
		{
			dev = mv_eth_netdev_by_id(i);
			if (dev)
				mv_eth_netdev_print(dev);
		}
	}
	else
		printk("ethPort=%d: switch is not connected - pp=%p, flags=0x%x\n", port, pp, pp->flags);
}


