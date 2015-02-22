/*
 * Ethernet driver for the Atmel AT91RM9200 (Thunder)
 *
 * (c) SAN People (Pty) Ltd
 *
 * Based on an earlier Atmel EMAC macrocell driver by Atmel and Lineo Inc.
 * Initial version by Rick Bronson 01/11/2003
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/mii.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <asm/io.h>
#include <linux/pci.h>
#include <linux/crc32.h>
#include <asm/uaccess.h>
#include <linux/ethtool.h>

#include <asm/arch/AT91RM9200_EMAC.h>
#include <asm/arch/pio.h>
#include "at91_ether.h"

static struct net_device at91_dev;

/* ........................... PHY INTERFACE ........................... */

/*
 * Enable the MDIO bit in MAC control register
 * When not called from an interrupt-handler, access to the PHY must be
 *  protected by a spinlock.
 */
static void enable_mdi(AT91PS_EMAC regs)
{
	regs->EMAC_CTL |= AT91C_EMAC_MPE;	/* enable management port */
}

/*
 * Disable the MDIO bit in the MAC control register
 */
static void disable_mdi(AT91PS_EMAC regs)
{
	regs->EMAC_CTL &= ~AT91C_EMAC_MPE;	/* disable management port */
}

/*
 * Write value to the a PHY register
 * Note: MDI interface is assumed to already have been enabled.
 */
static void write_phy(AT91PS_EMAC regs, unsigned char address, unsigned int value)
{
	regs->EMAC_MAN = (AT91C_EMAC_HIGH | AT91C_EMAC_CODE_802_3 | AT91C_EMAC_RW_W
		| (address << 18)) + (value & 0xffff);

	/* Wait until IDLE bit in Network Status register is cleared */
	// TODO: Enforce some maximum loop-count?
	while (!(regs->EMAC_SR & AT91C_EMAC_IDLE)) { barrier(); }
}

/*
 * Read value stored in a PHY register.
 * Note: MDI interface is assumed to already have been enabled.
 */
static void read_phy(AT91PS_EMAC regs, unsigned char address, unsigned int *value)
{
	regs->EMAC_MAN = AT91C_EMAC_HIGH | AT91C_EMAC_CODE_802_3 | AT91C_EMAC_RW_R
		| (address << 18);

	/* Wait until IDLE bit in Network Status register is cleared */
	// TODO: Enforce some maximum loop-count?
	while (!(regs->EMAC_SR & AT91C_EMAC_IDLE)) { barrier(); }

	*value = (regs->EMAC_MAN & 0x0000ffff);
}

/* ........................... PHY MANAGEMENT .......................... */

/*
 * Access the PHY to determine the current Link speed and Mode, and update the
 * MAC accordingly.
 * If no link or auto-negotiation is busy, then no changes are made.
 * Returns:  0 : OK
 *          -1 : No link
 *          -2 : AutoNegotiation still in progress
 */
static int update_linkspeed(struct net_device *dev, AT91PS_EMAC regs) {
	unsigned int bmsr, bmcr, lpa, mac_cfg;
	unsigned int speed, duplex;

	/* Link status is latched, so read twice to get current value */
	read_phy(regs, MII_BMSR, &bmsr);
	read_phy(regs, MII_BMSR, &bmsr);
	if (!(bmsr & BMSR_LSTATUS)) return -1;			/* no link */

	read_phy(regs, MII_BMCR, &bmcr);
	if (bmcr & BMCR_ANENABLE) {				/* AutoNegotiation is enabled */
		if (!(bmsr & BMSR_ANEGCOMPLETE)) return -2;	/* auto-negotitation in progress */

		read_phy(regs, MII_LPA, &lpa);
		if ((lpa & LPA_100FULL) || (lpa & LPA_100HALF)) speed = SPEED_100;
		else speed = SPEED_10;
		if ((lpa & LPA_100FULL) || (lpa & LPA_10FULL)) duplex = DUPLEX_FULL;
		else duplex = DUPLEX_HALF;
	} else {
		speed = (bmcr & BMCR_SPEED100) ? SPEED_100 : SPEED_10;
		duplex = (bmcr & BMCR_FULLDPLX) ? DUPLEX_FULL : DUPLEX_HALF;
	}

	/* Update the MAC */
	mac_cfg = regs->EMAC_CFG & ~(AT91C_EMAC_SPD | AT91C_EMAC_FD);
	if (speed == SPEED_100) {
		if (duplex == DUPLEX_FULL)		/* 100 Full Duplex */
			regs->EMAC_CFG = mac_cfg | AT91C_EMAC_SPD | AT91C_EMAC_FD;
		else					/* 100 Half Duplex */
			regs->EMAC_CFG = mac_cfg | AT91C_EMAC_SPD;
	} else {
		if (duplex == DUPLEX_FULL)		/* 10 Full Duplex */
			regs->EMAC_CFG = mac_cfg | AT91C_EMAC_FD;
		else					/* 10 Half Duplex */
			regs->EMAC_CFG = mac_cfg;
	}

	printk(KERN_INFO "%s: Link now %i-%s\n", dev->name, speed, (duplex == DUPLEX_FULL) ? "FullDuplex" : "HalfDuplex");
	return 0;
}

/*
 * Handle interrupts from the PHY
 */
void at91ether_phy_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	AT91PS_EMAC emac = (AT91PS_EMAC) dev->base_addr;
	int status;
	unsigned int phy;

	enable_mdi(emac);

	read_phy(emac, MII_DSINTR_REG, &phy);	/* acknowledge interrupt in PHY */
	status = AT91_SYS->PIOC_ISR;		/* acknowledge interrupt in PIO */

	status = update_linkspeed(dev, emac);
	if (status == -1) {			/* link is down */
		netif_carrier_off(dev);
		printk(KERN_INFO "%s: Link down.\n", dev->name);
	} else if (status == -2) {		/* auto-negotiation in progress */
		/* Do nothing - another interrupt generated when negotiation complete */
	} else {				/* link is operational */
		netif_carrier_on(dev);
	}
	disable_mdi(emac);
}

/*
 * Initialize and enable the PHY interrupt when link-state changes
 */
void enable_phyirq(struct net_device *dev, AT91PS_EMAC regs)
{
	struct at91_private *lp = (struct at91_private *) dev->priv;
	unsigned int dsintr, status;

	static int first_init = 0;

	if (first_init == 0) {
		// TODO: Check error code.  Really need a generic PIO (interrupt)
		// layer since we're really only interested in the PC4 line.
		(void) request_irq(4, at91ether_phy_interrupt, 0, dev->name, dev);

		AT91_SYS->PIOC_ODR = AT91C_PIO_PC4;	/* Configure as input */

		first_init = 1;
	}
	else {
		status = AT91_SYS->PIOC_ISR;		/* clear any pending PIO interrupts */
		AT91_SYS->PIOC_IER = AT91C_PIO_PC4;	/* Enable interrupt */

		spin_lock_irq(&lp->lock);
		enable_mdi(regs);
		read_phy(regs, MII_DSINTR_REG, &dsintr);
		dsintr = dsintr & ~0xf00;		/* clear bits 8..11 */
		write_phy(regs, MII_DSINTR_REG, dsintr);
		disable_mdi(regs);
		spin_unlock_irq(&lp->lock);
	}
}

/*
 * Disable the PHY interrupt
 */
void disable_phyirq(struct net_device *dev, AT91PS_EMAC regs)
{
	struct at91_private *lp = (struct at91_private *) dev->priv;
	unsigned int dsintr;

	spin_lock_irq(&lp->lock);
	enable_mdi(regs);
	read_phy(regs, MII_DSINTR_REG, &dsintr);
	dsintr = dsintr | 0xf00;			/* set bits 8..11 */
	write_phy(regs, MII_DSINTR_REG, dsintr);
	disable_mdi(regs);
	spin_unlock_irq(&lp->lock);

	AT91_SYS->PIOC_IDR = AT91C_PIO_PC4;		/* Disable interrupt */
}

/* ......................... ADDRESS MANAGEMENT ........................ */

/*
 * Set the ethernet MAC address in dev->dev_addr
 */
void get_mac_address(struct net_device *dev) {
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;
	char addr[6];
	unsigned int hi, lo;

	/* Check if bootloader set address in Specific-Address 1 */
	hi = regs->EMAC_SA1H;
	lo = regs->EMAC_SA1L;
	addr[0] = (lo & 0xff);
	addr[1] = (lo & 0xff00) >> 8;
	addr[2] = (lo & 0xff0000) >> 16;
	addr[3] = (lo & 0xff000000) >> 24;
	addr[4] = (hi & 0xff);
	addr[5] = (hi & 0xff00) >> 8;

	if (is_valid_ether_addr(addr)) {
		memcpy(dev->dev_addr, &addr, 6);
		return;
	}

	/* Check if bootloader set address in Specific-Address 2 */
	hi = regs->EMAC_SA2H;
	lo = regs->EMAC_SA2L;
	addr[0] = (lo & 0xff);
	addr[1] = (lo & 0xff00) >> 8;
	addr[2] = (lo & 0xff0000) >> 16;
	addr[3] = (lo & 0xff000000) >> 24;
	addr[4] = (hi & 0xff);
	addr[5] = (hi & 0xff00) >> 8;

	if (is_valid_ether_addr(addr)) {
		memcpy(dev->dev_addr, &addr, 6);
		return;
	}
}

/*
 * Program the hardware MAC address from dev->dev_addr.
 */
static void update_mac_address(struct net_device *dev)
{
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;

	regs->EMAC_SA1L = (dev->dev_addr[3] << 24) | (dev->dev_addr[2] << 16) | (dev->dev_addr[1] << 8) | (dev->dev_addr[0]);
	regs->EMAC_SA1H = (dev->dev_addr[5] << 8) | (dev->dev_addr[4]);
}

#ifdef AT91_ETHER_ADDR_CONFIGURABLE
/*
 * Store the new hardware address in dev->dev_addr, and update the MAC.
 */
static int set_mac_address(struct net_device *dev, void* addr)
{
	struct sockaddr *address = addr;

	if (!is_valid_ether_addr(address->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, address->sa_data, dev->addr_len);
	update_mac_address(dev);

	printk("%s: Setting MAC address to %02x:%02x:%02x:%02x:%02x:%02x\n", dev->name,
		dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
		dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

	return 0;
}
#endif

/*
 * Add multicast addresses to the internal multicast-hash table.
 */
static void at91ether_sethashtable(struct net_device *dev, AT91PS_EMAC regs)
{
	struct dev_mc_list *curr;
	unsigned char mc_filter[2];
	unsigned int i, bitnr;

	mc_filter[0] = mc_filter[1] = 0;

	curr = dev->mc_list;
	for (i = 0; i < dev->mc_count; i++, curr = curr->next) {
		if (!curr) break;	/* unexpected end of list */

		bitnr = ether_crc(ETH_ALEN, curr->dmi_addr) >> 26;
		mc_filter[bitnr >> 5] |= 1 << (bitnr & 31);
	}

	regs->EMAC_HSH = mc_filter[1];
	regs->EMAC_HSL = mc_filter[0];
}

/*
 * Enable/Disable promiscuous and multicast modes.
 */
static void at91ether_set_rx_mode(struct net_device *dev)
{
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;

	if (dev->flags & IFF_PROMISC) {			/* Enable promiscuous mode */
		regs->EMAC_CFG |= AT91C_EMAC_CAF;
	} else if (dev->flags & (~IFF_PROMISC)) {	/* Disable promiscuous mode */
		regs->EMAC_CFG &= ~AT91C_EMAC_CAF;
	}

	if (dev->flags & IFF_ALLMULTI) {		/* Enable all multicast mode */
		regs->EMAC_HSH = -1;
		regs->EMAC_HSL = -1;
		regs->EMAC_CFG |= AT91C_EMAC_MTI;
	} else if (dev->mc_count > 0) {			/* Enable specific multicasts */
		at91ether_sethashtable(dev, regs);
		regs->EMAC_CFG |= AT91C_EMAC_MTI;
	} else if (dev->flags & (~IFF_ALLMULTI)) {	/* Disable all multicast mode */
		regs->EMAC_HSH = 0;
		regs->EMAC_HSL = 0;
		regs->EMAC_CFG &= ~AT91C_EMAC_MTI;
	}
}

/* ............................... IOCTL ............................... */

static int mdio_read(struct net_device *dev, int phy_id, int location)
{
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;
	unsigned int value;

	read_phy(regs, location, &value);
	return value;
}

static void mdio_write(struct net_device *dev, int phy_id, int location, int value)
{
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;

	write_phy(regs, location, value);
}

/*
 * ethtool support.
 */
static int at91ether_ethtool_ioctl (struct net_device *dev, void *useraddr)
{
	struct at91_private *lp = (struct at91_private *) dev->priv;
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;
	u32 ethcmd;
	int res = 0;

	if (copy_from_user (&ethcmd, useraddr, sizeof (ethcmd)))
		return -EFAULT;

	spin_lock_irq(&lp->lock);
	enable_mdi(regs);

	switch (ethcmd) {
	case ETHTOOL_GSET: {
		struct ethtool_cmd ecmd = { ETHTOOL_GSET };
		res = mii_ethtool_gset(&lp->mii, &ecmd);
		if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
			res = -EFAULT;
		break;
	}
	case ETHTOOL_SSET: {
		struct ethtool_cmd ecmd;
		if (copy_from_user(&ecmd, useraddr, sizeof(ecmd)))
			res = -EFAULT;
		else
			res = mii_ethtool_sset(&lp->mii, &ecmd);
		break;
	}
	case ETHTOOL_NWAY_RST: {
		res = mii_nway_restart(&lp->mii);
		break;
	}
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = { ETHTOOL_GLINK };
		edata.data = mii_link_ok(&lp->mii);
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			res = -EFAULT;
		break;
	}
	default:
		res = -EOPNOTSUPP;
	}

	disable_mdi(regs);
	spin_unlock_irq(&lp->lock);

	return res;
}

/*
 * User-space ioctl interface.
 */
static int at91ether_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	switch(cmd) {
	case SIOCETHTOOL:
		return at91ether_ethtool_ioctl(dev, (void *) rq->ifr_data);
	default:
		return -EOPNOTSUPP;
	}
}

/* ................................ MAC ................................ */

/*
 * Initialize and start the Receiver and Transmit subsystems
 */
static void at91ether_start(struct net_device *dev)
{
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;
	struct at91_private *lp = (struct at91_private *) dev->priv;
	int i;
	struct recv_desc_bufs *dlist, *dlist_phys;

	dlist = lp->dlist;
	dlist_phys = lp->dlist_phys;

	for (i = 0; i < MAX_RX_DESCR; i++) {
		dlist->descriptors[i].addr = (unsigned int) &dlist_phys->recv_buf[i][0];
		dlist->descriptors[i].size = 0;
	}

	/* Set the Wrap bit on the last descriptor */
	dlist->descriptors[i-1].addr |= EMAC_DESC_WRAP;

	/* Reset buffer index */
	lp->rxBuffIndex = 0;

	/* Program address of descriptor list in Rx Buffer Queue register */
	regs->EMAC_RBQP = (AT91_REG) dlist_phys;

	/* Enable Receive and Transmit */
	regs->EMAC_CTL |= (AT91C_EMAC_RE | AT91C_EMAC_TE);
}

/*
 * Open the ethernet interface
 */
static int at91ether_open(struct net_device *dev)
{
	struct at91_private *lp = (struct at91_private *) dev->priv;
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;

        if (!is_valid_ether_addr(dev->dev_addr))
        	return -EADDRNOTAVAIL;

	AT91_SYS->PMC_PCER = 1 << AT91C_ID_EMAC;	/* Re-enable Peripheral clock */
	regs->EMAC_CTL |= AT91C_EMAC_CSR;	/* Clear internal statistics */

	/* Enable PHY interrupt */
	enable_phyirq(dev, regs);

	/* Enable MAC interrupts */
	regs->EMAC_IER = AT91C_EMAC_RCOM | AT91C_EMAC_RBNA
			| AT91C_EMAC_TUND | AT91C_EMAC_RTRY | AT91C_EMAC_TCOM
			| AT91C_EMAC_ROVR | AT91C_EMAC_HRESP;

	/* Determine current link speed */
	spin_lock_irq(&lp->lock);
	enable_mdi(regs);
	(void) update_linkspeed(dev, regs);
	disable_mdi(regs);
	spin_unlock_irq(&lp->lock);

	at91ether_start(dev);
	netif_start_queue(dev);
	return 0;
}

/*
 * Close the interface
 */
static int at91ether_close(struct net_device *dev)
{
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;

	/* Disable Receiver and Transmitter */
	regs->EMAC_CTL &= ~(AT91C_EMAC_TE | AT91C_EMAC_RE);

	/* Disable PHY interrupt */
	disable_phyirq(dev, regs);

	/* Disable MAC interrupts */
	regs->EMAC_IDR = AT91C_EMAC_RCOM | AT91C_EMAC_RBNA
			| AT91C_EMAC_TUND | AT91C_EMAC_RTRY | AT91C_EMAC_TCOM
			| AT91C_EMAC_ROVR | AT91C_EMAC_HRESP;

	netif_stop_queue(dev);

	AT91_SYS->PMC_PCDR = 1 << AT91C_ID_EMAC;	/* Disable Peripheral clock */

	return 0;
}

/*
 * Transmit packet.
 */
static int at91ether_tx(struct sk_buff *skb, struct net_device *dev)
{
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;
	struct at91_private *lp = (struct at91_private *) dev->priv;

	if (regs->EMAC_TSR & AT91C_EMAC_BNQ) {
		netif_stop_queue(dev);

		/* Store packet information (to free when Tx completed) */
		lp->skb = skb;
		lp->skb_length = skb->len;
		lp->skb_physaddr = pci_map_single(NULL, skb->data, skb->len, PCI_DMA_TODEVICE);
		lp->stats.tx_bytes += skb->len;

		/* Set address of the data in the Transmit Address register */
		regs->EMAC_TAR = lp->skb_physaddr;
		/* Set length of the packet in the Transmit Control register */
		regs->EMAC_TCR = skb->len;

		dev->trans_start = jiffies;
	} else {
		printk(KERN_ERR "at91_ether.c: at91ether_tx() called, but device is busy!\n");
		return 1;	/* if we return anything but zero, dev.c:1055 calls kfree_skb(skb)
				on this skb, he also reports -ENETDOWN and printk's, so either
				we free and return(0) or don't free and return 1 */
	}

	return 0;
}

/*
 * Update the current statistics from the internal statistics registers.
 */
static struct net_device_stats *at91ether_stats(struct net_device *dev)
{
	struct at91_private *lp = (struct at91_private *) dev->priv;
	AT91PS_EMAC regs = (AT91PS_EMAC) dev->base_addr;
	int ale, lenerr, seqe, lcol, ecol;

	if (netif_running(dev)) {
		lp->stats.rx_packets += regs->EMAC_OK;			/* Good frames received */
		ale = regs->EMAC_ALE;
		lp->stats.rx_frame_errors += ale;			/* Alignment errors */
		lenerr = regs->EMAC_ELR + regs->EMAC_USF;
		lp->stats.rx_length_errors += lenerr;			/* Excessive Length or Undersize Frame error */
		seqe = regs->EMAC_SEQE;
		lp->stats.rx_crc_errors += seqe;			/* CRC error */
		lp->stats.rx_fifo_errors += regs->EMAC_DRFC;		/* Receive buffer not available */
		lp->stats.rx_errors += (ale + lenerr + seqe + regs->EMAC_CDE + regs->EMAC_RJB);

		lp->stats.tx_packets += regs->EMAC_FRA;			/* Frames successfully transmitted */
		lp->stats.tx_fifo_errors += regs->EMAC_TUE;		/* Transmit FIFO underruns */
		lp->stats.tx_carrier_errors += regs->EMAC_CSE;		/* Carrier Sense errors */
		lp->stats.tx_heartbeat_errors += regs->EMAC_SQEE;	/* Heartbeat error */

		lcol = regs->EMAC_LCOL;
		ecol = regs->EMAC_ECOL;
		lp->stats.tx_window_errors += lcol;			/* Late collisions */
		lp->stats.tx_aborted_errors += ecol;			/* 16 collisions */

		lp->stats.collisions += (regs->EMAC_SCOL + regs->EMAC_MCOL + lcol + ecol);
	}
	return &lp->stats;
}

/*
 * Extract received frame from buffer descriptors and sent to upper layers.
 * (Called from interrupt context)
 */
static void at91ether_rx(struct net_device *dev)
{
	struct at91_private *lp = (struct at91_private *) dev->priv;
	struct recv_desc_bufs *dlist;
	unsigned char *p_recv;
	struct sk_buff *skb;
	unsigned int pktlen;

	dlist = lp->dlist;
	while (dlist->descriptors[lp->rxBuffIndex].addr & EMAC_DESC_DONE) {
		p_recv = dlist->recv_buf[lp->rxBuffIndex];
		pktlen = dlist->descriptors[lp->rxBuffIndex].size & 0x7ff;	/* Length of frame including FCS */
		skb = alloc_skb(pktlen + 2, GFP_ATOMIC);
		if (skb != NULL) {
			skb_reserve(skb, 2);
			memcpy(skb_put(skb, pktlen), p_recv, pktlen);

			skb->dev = dev;
			skb->protocol = eth_type_trans(skb, dev);
			skb->len = pktlen;
			dev->last_rx = jiffies;
			lp->stats.rx_bytes += pktlen;
			netif_rx(skb);
		}
		else {
			lp->stats.rx_dropped += 1;
			printk(KERN_NOTICE "%s: Memory squeeze, dropping packet.\n", dev->name);
		}

		if (dlist->descriptors[lp->rxBuffIndex].size & EMAC_MULTICAST)
			lp->stats.multicast++;

		dlist->descriptors[lp->rxBuffIndex].addr &= ~EMAC_DESC_DONE;	/* reset ownership bit */
		if (lp->rxBuffIndex == MAX_RX_DESCR-1)				/* wrap after last buffer */
			lp->rxBuffIndex = 0;
		else
			lp->rxBuffIndex++;
	}
}

/*
 * MAC interrupt handler
 */
static void at91ether_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct at91_private *lp = (struct at91_private *) dev->priv;
	AT91PS_EMAC emac = (AT91PS_EMAC) dev->base_addr;
	unsigned long intstatus;

	/* MAC Interrupt Status register indicates what interrupts are pending.
	   It is automatically cleared once read. */
	intstatus = emac->EMAC_ISR;

	if (intstatus & AT91C_EMAC_RCOM)		/* Receive complete */
		at91ether_rx(dev);

	if (intstatus & AT91C_EMAC_TCOM) {		/* Transmit complete */
		/* The TCOM bit is set even if the transmission failed. */
		if (intstatus & (AT91C_EMAC_TUND | AT91C_EMAC_RTRY))
			lp->stats.tx_errors += 1;

		dev_kfree_skb_irq(lp->skb);
		pci_unmap_single(NULL, lp->skb_physaddr, lp->skb_length, PCI_DMA_TODEVICE);
		netif_wake_queue(dev);
	}

	if (intstatus & AT91C_EMAC_RBNA)
		printk("%s: RBNA error\n", dev->name);
	if (intstatus & AT91C_EMAC_ROVR)
		printk("%s: ROVR error\n", dev->name);
}

/*
 * Initialize the ethernet interface
 */
static int at91ether_setup(struct net_device *dev)
{
	struct at91_private *lp;
	AT91PS_EMAC regs;
	static int already_initialized = 0;

	if (already_initialized)
		return 0;

	dev = init_etherdev(dev, sizeof(struct at91_private));
	dev->base_addr = AT91C_VA_BASE_EMAC;
	dev->irq = AT91C_ID_EMAC;
	SET_MODULE_OWNER(dev);

	/* Install the interrupt handler */
	if (request_irq(dev->irq, at91ether_interrupt, 0, dev->name, dev))
		return -EBUSY;

	/* Allocate memory for private data structure */
	lp = (struct at91_private *) kmalloc(sizeof(struct at91_private), GFP_KERNEL);
	if (lp == NULL) {
		free_irq(dev->irq, dev);
		return -ENOMEM;
	}
	memset(lp, 0, sizeof(struct at91_private));
	dev->priv = lp;

	/* Allocate memory for DMA Receive descriptors */
	lp->dlist = (struct recv_desc_bufs *) consistent_alloc(GFP_DMA | GFP_KERNEL, sizeof(struct recv_desc_bufs), (dma_addr_t *) &lp->dlist_phys);
	if (lp->dlist == NULL) {
		kfree(dev->priv);
		free_irq(dev->irq, dev);
		return -ENOMEM;
	}

	spin_lock_init(&lp->lock);

	ether_setup(dev);
	dev->open = at91ether_open;
	dev->stop = at91ether_close;
	dev->hard_start_xmit = at91ether_tx;
	dev->get_stats = at91ether_stats;
	dev->set_multicast_list = at91ether_set_rx_mode;
	dev->do_ioctl = at91ether_ioctl;

#ifdef AT91_ETHER_ADDR_CONFIGURABLE
	dev->set_mac_address = set_mac_address;
#endif

	get_mac_address(dev);		/* Get ethernet address and store it in dev->dev_addr */
	update_mac_address(dev);	/* Program ethernet address into MAC */

	regs = (AT91PS_EMAC) dev->base_addr;
	regs->EMAC_CTL = 0;

#ifdef CONFIG_AT91_ETHER_RMII
	regs->EMAC_CFG = AT91C_EMAC_RMII;
#else
	regs->EMAC_CFG = 0;
#endif

	lp->mii.dev = dev;		/* Support for ethtool */
	lp->mii.mdio_read = mdio_read;
	lp->mii.mdio_write = mdio_write;

	enable_phyirq(dev, regs);

	/* Determine current link speed */
	spin_lock_irq(&lp->lock);
	enable_mdi(regs);
	(void) update_linkspeed(dev, regs);
	disable_mdi(regs);
	spin_unlock_irq(&lp->lock);

	/* Display ethernet banner */
	printk(KERN_INFO "%s: AT91 ethernet at 0x%08x int=%d %s%s (%02x:%02x:%02x:%02x:%02x:%02x)\n",
		dev->name, (uint) dev->base_addr, dev->irq,
		regs->EMAC_CFG & AT91C_EMAC_SPD ? "100-" : "10-",
		regs->EMAC_CFG & AT91C_EMAC_FD ? "FullDuplex" : "HalfDuplex",
		dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
		dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

	already_initialized = 1;
	return 0;
}

/*
 * Detect MAC and PHY and perform initialization
 */
int at91ether_probe(struct net_device *dev)
{
	AT91PS_EMAC regs = (AT91PS_EMAC) AT91C_VA_BASE_EMAC;
	unsigned int phyid1, phyid2;
	int detected = -1;

	/* Configure the hardware - RMII vs MII mode */
#ifdef CONFIG_AT91_ETHER_RMII
	AT91_CfgPIO_EMAC_RMII();
#else
	AT91_CfgPIO_EMAC_MII();
#endif

	AT91_SYS->PMC_PCER = 1 << AT91C_ID_EMAC;	/* Enable Peripheral clock */

	/* Read the PHY ID registers */
	enable_mdi(regs);
	read_phy(regs, MII_PHYSID1, &phyid1);
	read_phy(regs, MII_PHYSID2, &phyid2);
	disable_mdi(regs);

	/* Davicom 9161: PHY_ID1 = 0x181  PHY_ID2 = B881 */
	if (((phyid1 << 16) | (phyid2 & 0xfff0)) == MII_DM9161_ID) {
		detected = at91ether_setup(dev);
	}

	AT91_SYS->PMC_PCDR = 1 << AT91C_ID_EMAC;	/* Disable Peripheral clock */

	return detected;
}

static int __init at91ether_init(void)
{
	if (!at91ether_probe(&at91_dev))
		return register_netdev(&at91_dev);

	return -1;
}

static void __exit at91ether_exit(void)
{
	unregister_netdev(&at91_dev);
}

module_init(at91ether_init)
module_exit(at91ether_exit)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AT91RM9200 EMAC Ethernet driver");
MODULE_AUTHOR("Andrew Victor");
