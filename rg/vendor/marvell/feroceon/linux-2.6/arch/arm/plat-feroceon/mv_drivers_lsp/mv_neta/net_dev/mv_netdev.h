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
#ifndef __mv_netdev_h__
#define __mv_netdev_h__

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/mii.h>
#include <net/ip.h>

#include "mvOs.h"
#include "mvStack.h"
#include "mvCommon.h"
#include "mv802_3.h"

#include "gbe/mvNeta.h"

/******************************************************
 * driver statistics control --                       *
 ******************************************************/
#ifdef CONFIG_MV_ETH_STAT_ERR
#define STAT_ERR(c) c
#else
#define STAT_ERR(c)
#endif

#ifdef CONFIG_MV_ETH_STAT_INF
#define STAT_INFO(c) c
#else
#define STAT_INFO(c)
#endif

#ifdef CONFIG_MV_ETH_STAT_DBG
#define STAT_DBG(c) c
#else
#define STAT_DBG(c)
#endif

#ifdef CONFIG_MV_ETH_STAT_DIST
#define STAT_DIST(c) c
#else
#define STAT_DIST(c)
#endif

/****************************************************************************
 * Rx buffer size: MTU + 2(Marvell Header) + 4(VLAN) + 14(MAC hdr) + 4(CRC) *
 ****************************************************************************/
#define RX_PKT_SIZE(mtu) \
		MV_ALIGN_UP((mtu) + 2 + 4 + ETH_HLEN + 4, CPU_D_CACHE_LINE_SIZE)

#define RX_BUF_SIZE(pkt_size)   ((pkt_size) + NET_SKB_PAD)

/******************************************************
 * interrupt control --                               *
 ******************************************************/
#ifdef CONFIG_MV_ETH_TXDONE_ISR
#define MV_ETH_TXDONE_INTR_MASK       NETA_CAUSE_TXQ_SENT_DESC_ALL_MASK
#else
#define MV_ETH_TXDONE_INTR_MASK       0
#endif

#define MV_ETH_MISC_SUM_INTR_MASK     (NETA_CAUSE_TX_ERR_SUM_MASK | NETA_CAUSE_MISC_SUM_MASK)
#define MV_ETH_RX_INTR_MASK            NETA_CAUSE_RXQ_OCCUP_DESC_ALL_MASK
#define NETA_RX_FL_DESC_MASK          (NETA_RX_F_DESC_MASK|NETA_RX_L_DESC_MASK)

#define MV_ETH_TRYLOCK(lock, flags)                           \
	(in_interrupt() ? spin_trylock((lock)) :              \
		spin_trylock_irqsave((lock), (flags)))

#define MV_ETH_LOCK(lock, flags)                              \
	if (in_interrupt())                                   \
		spin_lock((lock));                            \
	else                                                  \
		spin_lock_irqsave((lock), (flags));

#define MV_ETH_UNLOCK(lock, flags)                            \
	if (in_interrupt())                                   \
		spin_unlock((lock));                          \
	else                                                  \
		spin_unlock_irqrestore((lock), (flags));

/******************************************************
 * rx / tx queues --                                  *
 ******************************************************/

/*
 * Debug statistics
 */
struct txq_stats {
#ifdef CONFIG_MV_ETH_STAT_DBG
    u32 txq_tx;
    u32 txq_err;
    u32 txq_txdone;
#endif /* CONFIG_MV_ETH_STAT_DBG */
};

struct port_stats {

#ifdef CONFIG_MV_ETH_STAT_ERR
	u32 rx_error;
	u32 tx_timeout;
	u32 netif_stop;
	u32 netif_wake;
#endif /* CONFIG_MV_ETH_STAT_ERR */

#ifdef CONFIG_MV_ETH_STAT_INF
	u32 irq;
	u32 irq_err;
	u32 poll;
	u32 poll_exit;
	u32 tx_done;
	u32 tx_done_timer;
	u32 cleanup_timer;
	u32 link;

#ifdef CONFIG_MV_ETH_RX_SPECIAL
    u32 rx_special;
#endif /* CONFIG_MV_ETH_RX_SPECIAL */

#ifdef CONFIG_MV_ETH_TX_SPECIAL
	u32	tx_special;
#endif /* CONFIG_MV_ETH_TX_SPECIAL */

#endif /* CONFIG_MV_ETH_STAT_INF */

#ifdef CONFIG_MV_ETH_STAT_DBG
	u32 rxq[CONFIG_MV_ETH_RXQ];
	u32 rxq_fill[CONFIG_MV_ETH_RXQ];
	u32 rx_netif;
	u32 rx_nfp;
	u32 rx_nfp_drop;
	u32 rx_gro;
	u32 rx_gro_bytes;
	u32 rx_drop_sw;
	u32 rx_csum_hw;
	u32 rx_csum_sw;
	u32 tx_csum_hw;
	u32 tx_csum_sw;
	u32 tx_skb_free;
	u32 tx_sg;
	u32 tx_tso;
	u32 tx_tso_bytes;
#endif /* CONFIG_MV_ETH_STAT_DBG */
};

/* Used for define type of data saved in shadow: SKB or eth_pbuf or nothing */
#define MV_ETH_SHADOW_SKB		0x01

/* Masks used for pp->flags */
#define MV_ETH_F_STARTED		0x01
#define MV_ETH_F_TX_DONE_TIMER		0x02
#define MV_ETH_F_SWITCH			0x04	/* port is connected to the Switch using the Gateway driver */
#define MV_ETH_F_MH			0x08
#define MV_ETH_F_NO_PAD			0x10
#define MV_ETH_F_DBG_RX			0x20
#define MV_ETH_F_DBG_TX			0x40
#define MV_ETH_F_EXT_SWITCH		0x80	/* port is connected to the Switch without the Gateway driver */
#define MV_ETH_F_CONNECT_LINUX		0x100   /* port is connected to Linux netdevice */
#define MV_ETH_F_LINK_UP		0x200
#define MV_ETH_F_DBG_DUMP		0x400
#define MV_ETH_F_DBG_ISR		0x800
#define MV_ETH_F_DBG_POLL		0x1000
#define MV_ETH_F_CLEANUP_TIMER		0x2000
#define MV_ETH_F_SWITCH_RG		0x4000	/* port is connected to the Switch using eth driver + OpenRG management */


/* One of three TXQ states */
#define MV_ETH_TXQ_FREE         0
#define MV_ETH_TXQ_CPU          1
#define MV_ETH_TXQ_HWF          2

#define MV_ETH_TXQ_INVALID		0xFF

struct mv_eth_tx_spec {
	u32		hw_cmd;	/* tx_desc offset = 0xC */
	u16		flags;
	u8		txp;
	u8		txq;
#ifdef CONFIG_MV_ETH_TX_SPECIAL
	void		(*tx_func) (u8 *data, int size, struct mv_eth_tx_spec *tx_spec);
#endif
};

typedef struct extradata {
	unsigned char *data;
} extradata;

struct tx_queue {
	MV_NETA_TXQ_CTRL   *q;
	u8                  cpu_owner; /* counter */
	u8                  hwf_rxp;
	u8                  txp;
	u8                  txq;
	int                 txq_size;
	int                 txq_count;
	int                 bm_only;
	u32                 *shadow_txq; /* can be MV_ETH_PKT* or struct skbuf* */
	int                 shadow_txq_put_i;
	int                 shadow_txq_get_i;
	struct txq_stats    stats;
	extradata           *extradataArr;
};

struct rx_queue {
	MV_NETA_RXQ_CTRL    *q;
	int                 rxq_size;
	int                 missed;
};

struct dist_stats {
	u32     *rx_dist;
	int     rx_dist_size;
	u32     *tx_done_dist;
	int     tx_done_dist_size;
	u32     *tx_tso_dist;
	int     tx_tso_dist_size;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#define CONFIG_NR_CPUS	1
#endif

struct eth_port {
	int                 port;
	MV_NETA_PORT_CTRL   *port_ctrl;
	struct rx_queue     *rxq_ctrl;
	struct tx_queue     *txq_ctrl;
	int                 txp_num;
	spinlock_t          *lock;
	struct timer_list   tx_done_timer;
	struct timer_list   cleanup_timer;
	struct net_device   *dev;
	struct bm_pool      *pool_long;
	int                 pool_long_num;
#ifdef CONFIG_MV_ETH_BM
	struct bm_pool      *pool_short;
	int                 pool_short_num;
#endif /* CONFIG_MV_ETH_BM */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    struct napi_struct	napi;
#endif
	unsigned int        flags;	/* MH, TIMER, etc. */
	u32                 hw_cmd;	/* offset 0xc in TX descriptor */
	int                 txp;
	int                 txq[CONFIG_NR_CPUS];
	u16                 tx_mh;	/* 2B MH */
	struct port_stats   stats;
	struct dist_stats   dist_stats;
	MV_U8               txq_tos_map[256];
#ifdef CONFIG_MII	
	struct mii_if_info  mii;
#endif
#ifdef CONFIG_MV_ETH_TOOL
	__u16               speed_cfg;
	__u8                duplex_cfg;
	__u8                autoneg_cfg;
	MV_U32              rx_coal_usec;
	MV_U32              tx_coal_usec;
#endif/* CONFIG_MV_ETH_TOOL */
#ifdef CONFIG_MV_ETH_RX_CSUM_OFFLOAD
	MV_U32              rx_csum_offload;
#endif /* CONFIG_MV_ETH_RX_CSUM_OFFLOAD */
#ifdef CONFIG_MV_ETH_RX_SPECIAL
	void    (*rx_special_proc)(int port, int rxq, struct net_device *dev,
					struct sk_buff *skb, struct neta_rx_desc *rx_desc);
#endif /* CONFIG_MV_ETH_RX_SPECIAL */
#ifdef CONFIG_MV_ETH_TX_SPECIAL
	int     (*tx_special_check)(int port, struct net_device *dev, struct sk_buff *skb,
					struct mv_eth_tx_spec *tx_spec_out);
#endif /* CONFIG_MV_ETH_TX_SPECIAL */
};

struct eth_netdev {
	u16     tx_vlan_mh;		/* 2B MH */
	u16     vlan_grp_id;		/* vlan group ID */
	u16     port_map;		/* switch port map */
	u16     link_map;		/* switch port link map */
	u16     cpu_port;		/* switch CPU port */
	u16     reserved;
};

struct eth_dev_priv {
	struct eth_port     *port_p;
	struct eth_netdev   *netdev_p;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
    struct net_device_stats	netdev_stats;
#endif
};

#define MV_ETH_PRIV(dev)        (((struct eth_dev_priv *)(netdev_priv(dev)))->port_p)
#define MV_DEV_PRIV(dev)        (((struct eth_dev_priv *)(netdev_priv(dev)))->netdev_p)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
 #define MV_DEV_STAT(dev)       (((struct eth_dev_priv *)(netdev_priv(dev)))->netdev_stats)
#else
 #define MV_DEV_STAT(dev)        ((dev)->stats)
#endif

/* define which Switch ports are relevant */
#define SWITCH_CONNECTED_PORTS_MASK	0x7F

#define MV_SWITCH_ID_0			0

struct pool_stats {
#ifdef CONFIG_MV_ETH_STAT_ERR
	u32 skb_alloc_oom;
	u32 stack_empty;
	u32 stack_full;
#endif /* CONFIG_MV_ETH_STAT_ERR */

#ifdef CONFIG_MV_ETH_STAT_DBG
	u32 bm_put;
	u32 stack_put;
	u32 stack_get;
	u32 skb_alloc_ok;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,26)
	u32 skb_recycled_ok;
	u32 skb_recycled_err;
#endif
#endif /* CONFIG_MV_ETH_STAT_DBG */
};

struct bm_pool {
	int         pool;
	int         capacity;
	int         buf_num;
	int         pkt_size;
	u32         *bm_pool;
	MV_STACK    *stack;
	spinlock_t  lock;
	int         missed;		/* FIXME: move to stats */
	struct pool_stats  stats;
};

static inline void mv_eth_interrupts_unmask(struct eth_port *pp)
{
    /* unmask interrupts */
    if (!(pp->flags & (MV_ETH_F_SWITCH | MV_ETH_F_EXT_SWITCH | MV_ETH_F_SWITCH_RG)))
	MV_REG_WRITE(NETA_INTR_MISC_MASK_REG(pp->port), NETA_CAUSE_LINK_CHANGE_MASK);

    MV_REG_WRITE(NETA_INTR_NEW_MASK_REG(pp->port),
		(MV_ETH_MISC_SUM_INTR_MASK |
		MV_ETH_TXDONE_INTR_MASK |
		MV_ETH_RX_INTR_MASK));
}

static inline int mv_eth_ctrl_is_tx_enabled(struct eth_port *pp)
{
	if (!pp)
		return -ENODEV;

	if (pp->flags & MV_ETH_F_CONNECT_LINUX)
		return 1;

	return 0;
}

#ifdef CONFIG_MV_ETH_SWITCH
struct mv_eth_switch_config {
	int             mtu;
	int             netdev_max;
	int             netdev_cfg;
	unsigned char   mac_addr[CONFIG_MV_ETH_SWITCH_NETDEV_NUM][MV_MAC_ADDR_SIZE];
	u16             board_port_map[CONFIG_MV_ETH_SWITCH_NETDEV_NUM];
};

extern int  mv_eth_switch_netdev_first, mv_eth_switch_netdev_last;
extern struct mv_eth_switch_config      switch_net_config;
extern struct net_device **mv_net_devs;

int     mv_eth_switch_config_get(int use_existing_config);
int     mv_eth_switch_set_mac_addr(struct net_device *dev, void *mac);
void    mv_eth_switch_set_multicast_list(struct net_device *dev);
int     mv_eth_switch_change_mtu(struct net_device *dev, int mtu);
int     mv_eth_switch_start(struct net_device *dev);
int     mv_eth_switch_stop(struct net_device *dev);
void    mv_eth_switch_status_print(int port);
int     mv_eth_switch_port_add(struct net_device *dev, int port);
int     mv_eth_switch_port_del(struct net_device *dev, int port);

#endif /* CONFIG_MV_ETH_SWITCH */

/******************************************************
 * Function prototypes --                             *
 ******************************************************/
int         mv_eth_stop(struct net_device *dev);
int         mv_eth_change_mtu(struct net_device *dev, int mtu);
int         mv_eth_set_mac_addr(struct net_device *dev, void *mac);
void        mv_eth_set_multicast_list(struct net_device *dev);
int         mv_eth_open(struct net_device *dev);

irqreturn_t mv_eth_isr(int irq, void *dev_id);
int         mv_eth_start_internals(struct eth_port *pp, int mtu);
int         mv_eth_stop_internals(struct eth_port *pp);
int         mv_eth_change_mtu_internals(struct net_device *netdev, int mtu);

int         mv_eth_rx_reset(int port);
int         mv_eth_txp_reset(int port, int txp);

struct eth_port     *mv_eth_port_by_id(unsigned int port);
struct net_device   *mv_eth_netdev_by_id(unsigned int idx);

void        mv_eth_mac_show(int port);
void        mv_eth_tos_map_show(int port);
int         mv_eth_rxq_tos_map_set(int port, int rxq, unsigned char tos);
int         mv_eth_txq_tos_map_set(int port, int txq, unsigned char tos);

void        mv_eth_netdev_print(struct net_device *netdev);
void        mv_eth_status_print(void);
void        mv_eth_port_status_print(unsigned int port);
void        mv_eth_port_stats_print(unsigned int port);

void        mv_eth_set_noqueue(struct net_device *dev, int enable);

void        mv_eth_ctrl_hwf(int en);
void        mv_eth_ctrl_nfp(int en);
#ifdef CONFIG_NET_SKB_RECYCLE
void        mv_eth_ctrl_recycle(int en);
#endif /* CONFIG_NET_SKB_RECYCLE */
void        mv_eth_ctrl_txdone(int num);
int         mv_eth_ctrl_tx_mh(int port, u16 mh);
int         mv_eth_ctrl_tx_cmd(int port, u32 cmd);
int         mv_eth_ctrl_txq_cpu_def(int port, int txp, int txq, int cpu);
int         mv_eth_ctrl_txq_mode_get(int port, int txp, int txq, int *rx_port);
int         mv_eth_ctrl_txq_cpu_own(int port, int txp, int txq, int add);
int         mv_eth_ctrl_txq_hwf_own(int port, int txp, int txq, int rxp);
int         mv_eth_ctrl_flag(int port, u32 flag, u32 val);
int         mv_eth_ctrl_txq_size_set(int port, int txp, int txq, int value);
int         mv_eth_ctrl_rxq_size_set(int port, int rxq, int value);
int         mv_eth_ctrl_port_buf_num_set(int port, int long_num, int short_num);

void        mv_eth_tx_desc_print(struct neta_tx_desc *desc);
void        mv_eth_pkt_print(struct eth_pbuf *pkt);
void        mv_eth_rx_desc_print(struct neta_rx_desc *desc);
void        mv_eth_skb_print(struct sk_buff *skb);
void        mv_eth_link_status_print(int port);

#ifdef CONFIG_MV_PON
void        mv_pon_ctrl_omci_type(MV_U16 type);
void        mv_pon_ctrl_omci_rx_gh(int en);
void        mv_pon_omci_print(void);
#endif /* CONFIG_MV_PON */

#ifdef CONFIG_MV_ETH_TX_SPECIAL
void mv_eth_tx_special_check_func(int port, int (*func)(int port, struct net_device *dev,
								  struct sk_buff *skb, struct mv_eth_tx_spec *tx_spec_out));
#endif /* CONFIG_MV_ETH_TX_SPECIAL */

#ifdef WAN_SWAP_FEATURE
int         mv_eth_ctrl_wan_mode(int wan_mode);
int         mv_eth_check_all_ports_down(void);
#define MV_WAN_MODE_MOCA	0
#define MV_WAN_MODE_GBE		1

#endif /* WAN_SWAP_FEATURE */

#endif /* __mv_netdev_h__ */
