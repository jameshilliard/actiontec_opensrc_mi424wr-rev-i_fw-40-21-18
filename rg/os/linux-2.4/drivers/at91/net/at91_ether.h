/*
 * Ethernet driver for the Atmel AT91RM9200 (Thunder)
 *
 * (c) SAN People (Pty) Ltd
 *
 * Based on an earlier Atmel EMAC macrocell driver by Atmel and Lineo Inc.
 * Initial version by Rick Bronson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef AT91_ETHERNET
#define AT91_ETHERNET

#undef AT91_ETHER_ADDR_CONFIGURABLE	/* MAC address can be changed? */


/* Davicom 9161 PHY */
#define MII_DM9161_ID   0x0181b880

/* Davicom specific registers */
#define MII_DSCSR_REG   17
#define MII_DSINTR_REG  21

/* ........................................................................ */

#define MAX_RBUFF_SZ	0x600		/* 1518 rounded up */
#define MAX_RX_DESCR	9		/* max number of receive buffers */

#define EMAC_DESC_DONE	0x00000001	/* bit for if DMA is done */
#define EMAC_DESC_WRAP	0x00000002	/* bit for wrap */

#define EMAC_BROADCAST	0x80000000	/* broadcast address */
#define EMAC_MULTICAST	0x40000000	/* multicast address */
#define EMAC_UNICAST	0x20000000	/* unicast address */

struct rbf_t
{
	unsigned int addr;
	unsigned long size;
};

struct recv_desc_bufs
{
	struct rbf_t descriptors[MAX_RX_DESCR];		/* must be on sizeof (rbf_t) boundary */
	char recv_buf[MAX_RX_DESCR][MAX_RBUFF_SZ];	/* must be on long boundary */
};

struct at91_private
{
	struct net_device_stats stats;
	struct mii_if_info mii;			/* ethtool support */

	/* PHY */
	spinlock_t lock;			/* lock for MDI interface */

	/* Transmit */
	struct sk_buff *skb;			/* holds skb until xmit interrupt completes */
	dma_addr_t skb_physaddr;		/* phys addr from pci_map_single */
	int skb_length;				/* saved skb length for pci_unmap_single */

	/* Receive */
	int rxBuffIndex;			/* index into receive descriptor list */
	struct recv_desc_bufs *dlist;		/* descriptor list address */
	struct recv_desc_bufs *dlist_phys;	/* descriptor list physical address */
};

#endif
