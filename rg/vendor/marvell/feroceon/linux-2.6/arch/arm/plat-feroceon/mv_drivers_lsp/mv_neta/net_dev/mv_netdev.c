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
#include <linux/if_vlan.h>
#include <net/ip.h>
#include <net/ipv6.h>

#include "mvOs.h"
#include "mvDebug.h"
#include "dbg-trace.h"
#include "mvSysHwConfig.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "ctrlEnv/mvCtrlEnvLib.h"
#include "eth-phy/mvEthPhy.h"
#include "mvSysEthPhyApi.h"
#include "mvSysNetaApi.h"
#include "gpp/mvGppRegs.h"

#include "gbe/mvNeta.h"
#include "bm/mvBm.h"
#include "pnc/mvPnc.h"
#include "pnc/mvTcam.h"
#include "pmt/mvPmt.h"
#include "nfp/mvNfp.h"

#include "mv_switch.h"

#include "nfp_mgr/mv_nfp_mgr.h"

#include "mv_netdev.h"
#include "mv_eth_tool.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
#ifdef CONFIG_MV_ETH_GRO
#include <linux/inetdevice.h>
#endif
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29) */

#include "cpu/mvCpuCntrs.h"

#ifdef CONFIG_MV_CPU_PERF_CNTRS
MV_CPU_CNTRS_EVENT	*event0 = NULL;
MV_CPU_CNTRS_EVENT	*event1 = NULL;
MV_CPU_CNTRS_EVENT	*event2 = NULL;
MV_CPU_CNTRS_EVENT	*event3 = NULL;
MV_CPU_CNTRS_EVENT	*event4 = NULL;
MV_CPU_CNTRS_EVENT	*event5 = NULL;
#endif /* CONFIG_MV_CPU_PERF_CNTRS */

#ifdef CONFIG_MV_ETH_BM
#define MV_ETH_BM_POOLS		MV_BM_POOLS
#define MV_ETH_SHORT_BM_POOL    (MV_BM_POOLS - 1)
#define mv_eth_pool_bm(p)       (p->bm_pool)
#define mv_eth_txq_bm(q)        (q->bm_only)
#else
#define MV_ETH_BM_POOLS		CONFIG_MV_ETH_PORTS_NUM
#define mv_eth_pool_bm(p)       0
#define mv_eth_txq_bm(q)        0
#endif /* CONFIG_MV_ETH_BM */

#define MV_ETH_MOCA_PRIORITY_SUPPORT

static spinlock_t mv_eth_global_lock;

spinlock_t mv_eth_mii_lock;

/* uncomment if you want to debug the SKB recycle feature */
/* #define ETH_SKB_DEBUG */

#ifdef CONFIG_MV_ETH_NFP
static int mv_ctrl_nfp = CONFIG_MV_ETH_NFP_DEF;

void mv_eth_ctrl_nfp(int en)
{
	mv_ctrl_nfp = en;
}

static MV_STATUS mv_eth_nfp(struct eth_port *pp, struct neta_rx_desc *rx_desc, struct eth_pbuf *pkt);
extern void nfp_port_register(int, int);
#endif /* CONFIG_MV_ETH_NFP */

#ifdef CONFIG_NET_SKB_RECYCLE
static int mv_ctrl_recycle = CONFIG_NET_SKB_RECYCLE_DEF;

void mv_eth_ctrl_recycle(int en)
{
	mv_ctrl_recycle = en;
}

#define mv_eth_is_recycle()     (mv_ctrl_recycle)
#else
#define mv_eth_is_recycle()     0
#endif /* CONFIG_NET_SKB_RECYCLE */

extern u8 mvMacAddr[CONFIG_MV_ETH_PORTS_NUM][MV_MAC_ADDR_SIZE];
extern u16 mvMtu[CONFIG_MV_ETH_PORTS_NUM];

extern unsigned int switch_enabled_ports;
/*
 * Static declarations
 */
static struct eth_port **mv_eth_ports;
static int mv_eth_ports_num = 0;
static int mv_net_devs_num = 0;
static int mv_net_devs_max = 0;

static int mv_eth_initialized = 0;
#ifdef WAN_SWAP_FEATURE
static int g_wan_mode = MV_WAN_MODE_MOCA;
static int g_external_switch_mode = 0;
#endif /* WAN_SWAP_FEATURE */

static struct bm_pool mv_eth_pool[MV_ETH_BM_POOLS];
static int mv_ctrl_txdone = CONFIG_MV_ETH_TXDONE_COAL_PKTS;

struct net_device **mv_net_devs;

/*
 * Local functions
 */
static void mv_eth_txq_delete(struct eth_port *pp, struct tx_queue *txq_ctrl);
static void mv_eth_tx_timeout(struct net_device *dev);
static int mv_eth_tx(struct sk_buff *skb, struct net_device *dev);
static struct sk_buff *mv_eth_skb_alloc(struct bm_pool *bm, struct eth_pbuf *pkt);
static void mv_eth_tx_frag_process(struct eth_port *pp, struct sk_buff *skb, struct tx_queue *txq_ctrl, u16 flags);
static inline u32 mv_eth_txq_done(struct eth_port *pp, struct tx_queue *txq_ctrl);
static inline MV_U32 mv_eth_tx_csum(struct eth_port *pp, struct sk_buff *skb);
static inline int mv_eth_refill(struct eth_port *pp, int rxq, struct eth_pbuf *pkt,
				struct bm_pool *pool, struct neta_rx_desc *rx_desc);

static void mv_eth_config_show(void);
static void mv_eth_priv_init(struct eth_port *pp, int port);
static void mv_eth_priv_cleanup(struct eth_port *pp);
static int mv_eth_config_get(struct eth_port *pp, u8 *mac);
static int mv_eth_hal_init(struct eth_port *pp);
struct net_device *mv_eth_netdev_init(struct eth_port *pp, int mtu, u8 *mac);
static void mv_eth_netdev_update(int dev_index, struct eth_port *pp);
static void mv_eth_netdev_set_features(struct net_device *dev);

static MV_STATUS mv_eth_pool_create(int pool, int capacity);
static int mv_eth_pool_add(int pool, int buf_num);
static int mv_eth_pool_free(int pool, int num);
static int mv_eth_pool_destroy(int pool);

#ifdef CONFIG_MV_ETH_TSO
struct tx_queue *mv_eth_tx_tso(struct sk_buff *skb, struct net_device *dev, struct mv_eth_tx_spec *tx_spec, int *frags);
#endif

/* Get the configuration string from the Kernel Command Line */
static char *port0_config_str = NULL, *port1_config_str = NULL, *port2_config_str = NULL, *port3_config_str = NULL;
int mv_eth_cmdline_port0_config(char *s);
__setup("mv_port0_config=", mv_eth_cmdline_port0_config);
int mv_eth_cmdline_port1_config(char *s);
__setup("mv_port1_config=", mv_eth_cmdline_port1_config);
int mv_eth_cmdline_port2_config(char *s);
__setup("mv_port2_config=", mv_eth_cmdline_port2_config);
int mv_eth_cmdline_port3_config(char *s);
__setup("mv_port3_config=", mv_eth_cmdline_port3_config);

int mv_eth_cmdline_port0_config(char *s)
{
	port0_config_str = s;
	return 1;
}

int mv_eth_cmdline_port1_config(char *s)
{
	port1_config_str = s;
	return 1;
}

int mv_eth_cmdline_port2_config(char *s)
{
	port2_config_str = s;
	return 1;
}

int mv_eth_cmdline_port3_config(char *s)
{
	port3_config_str = s;
	return 1;
}

static int mv_eth_port_config_parse(struct eth_port *pp)
{
	char *str;

	printk("\n");
	if (pp == NULL) {
		printk("  o mv_eth_port_config_parse: got NULL pp\n");
		return -1;
	}

	switch (pp->port) {
	case 0:
		str = port0_config_str;
		break;
	case 1:
		str = port1_config_str;
		break;
	case 2:
		str = port2_config_str;
		break;
	case 3:
		str = port3_config_str;
		break;
	default:
		printk("  o mv_eth_port_config_parse: got unknown port %d\n", pp->port);
		return -1;
	}

	if (str != NULL) {
		if ((!strcmp(str, "disconnected")) || (!strcmp(str, "Disconnected"))) {
			printk("  o Port %d is disconnected from Linux netdevice\n", pp->port);
			pp->flags &= ~MV_ETH_F_CONNECT_LINUX;
			return 0;
		}
	}

	printk("  o Port %d is connected to Linux netdevice\n", pp->port);
	pp->flags |= MV_ETH_F_CONNECT_LINUX;
	return 0;
}


static void mv_eth_add_cleanup_timer(struct eth_port *pp)
{
	if (!(pp->flags & MV_ETH_F_CLEANUP_TIMER)) {
		pp->cleanup_timer.expires = jiffies + ((HZ * CONFIG_MV_ETH_CLEANUP_TIMER_PERIOD) / 1000); /* ms */
		add_timer(&pp->cleanup_timer);
		pp->flags |= MV_ETH_F_CLEANUP_TIMER;
	}
}

static void mv_eth_add_tx_done_timer(struct eth_port *pp)
{
	if (!(pp->flags & MV_ETH_F_TX_DONE_TIMER)) {
		pp->tx_done_timer.expires = jiffies + ((HZ * CONFIG_MV_ETH_TX_DONE_TIMER_PERIOD) / 1000); /* ms */
		add_timer(&pp->tx_done_timer);
		pp->flags |= MV_ETH_F_TX_DONE_TIMER;
	}
}


#ifdef ETH_SKB_DEBUG
struct sk_buff *mv_eth_skb_debug[MV_BM_POOL_CAP_MAX * MV_ETH_BM_POOLS];
static spinlock_t skb_debug_lock;

void mv_eth_skb_check(struct sk_buff *skb)
{
	int i;
	struct sk_buff *temp;
	unsigned long flags;

	if (skb == NULL)
		printk("mv_eth_skb_check: got NULL SKB\n");

	spin_lock_irqsave(&skb_debug_lock, flags);

	i = *((u32 *)&skb->cb[0]);

	if ((i >= 0) && (i < MV_BM_POOL_CAP_MAX * MV_ETH_BM_POOLS)) {
		temp = mv_eth_skb_debug[i];
		if (mv_eth_skb_debug[i] != skb) {
			printk("mv_eth_skb_check: Unexpected skb: %p (%d) != %p (%d)\n",
			       skb, i, temp, *((u32 *)&temp->cb[0]));
		}
		mv_eth_skb_debug[i] = NULL;
	} else {
		printk("mv_eth_skb_check: skb->cb=%d is out of range\n", i);
	}

	spin_unlock_irqrestore(&skb_debug_lock, flags);
}

void mv_eth_skb_save(struct sk_buff *skb, const char *s)
{
	int i;
	int saved = 0;
	unsigned long flags;

	spin_lock_irqsave(&skb_debug_lock, flags);

	for (i = 0; i < MV_BM_POOL_CAP_MAX * MV_ETH_BM_POOLS; i++) {
		if (mv_eth_skb_debug[i] == skb) {
			printk("%s: mv_eth_skb_debug Duplicate: i=%d, skb=%p\n", s, i, skb);
			mv_eth_skb_print(skb);
		}

		if ((!saved) && (mv_eth_skb_debug[i] == NULL)) {
			mv_eth_skb_debug[i] = skb;
			*((u32 *)&skb->cb[0]) = i;
			saved = 1;
		}
	}

	spin_unlock_irqrestore(&skb_debug_lock, flags);

	if ((i == MV_BM_POOL_CAP_MAX * MV_ETH_BM_POOLS) && (!saved))
		printk("mv_eth_skb_debug is FULL, skb=%p\n", skb);
}
#endif /* ETH_SKB_DEBUG */

#ifdef CONFIG_MV_ETH_GRO
static INLINE unsigned int mv_eth_dev_ip(struct net_device *dev)
{
	struct in_device *ip = dev->ip_ptr;
	if (ip && ip->ifa_list)
		return ip->ifa_list->ifa_address;

	return 0;

}
#endif /* CONFIG_MV_ETH_GRO */

struct eth_port *mv_eth_port_by_id(unsigned int port)
{
	if (port < mv_eth_ports_num)
		return mv_eth_ports[port];

	return NULL;
}

struct net_device *mv_eth_netdev_by_id(unsigned int idx)
{
	if (idx < mv_net_devs_num)
		return mv_net_devs[idx];

	return NULL;
}

static inline void mv_eth_shadow_inc_get(struct tx_queue *txq)
{
	txq->shadow_txq_get_i++;
	if (txq->shadow_txq_get_i == txq->txq_size)
		txq->shadow_txq_get_i = 0;
}

static inline void mv_eth_shadow_inc_put(struct tx_queue *txq)
{
	txq->shadow_txq_put_i++;
	if (txq->shadow_txq_put_i == txq->txq_size)
		txq->shadow_txq_put_i = 0;
}

static inline void mv_eth_shadow_dec_put(struct tx_queue *txq)
{
	if (txq->shadow_txq_put_i == 0)
		txq->shadow_txq_put_i = txq->txq_size - 1;
	else
		txq->shadow_txq_put_i--;
}

static inline int mv_eth_skb_mh_add(struct sk_buff *skb, u16 mh)
{
	/* sanity: Check that there is place for MH in the buffer */
	if (skb_headroom(skb) < MV_ETH_MH_SIZE) {
		printk("%s: skb (%p) doesn't have place for MH, head=%p, data=%p\n",
		       __func__, skb, skb->head, skb->data);
		return 1;
	}

	/* Prepare place for MH header */
	skb->len += MV_ETH_MH_SIZE;
	skb->data -= MV_ETH_MH_SIZE;
	*((u16 *) skb->data) = mh;

	return 0;
}

static inline struct neta_tx_desc *mv_eth_tx_desc_get(struct tx_queue *txq_ctrl, int num)
{
	/* Is enough TX descriptors to send packet */
	if ((txq_ctrl->txq_count + num) >= txq_ctrl->txq_size) {
		/*
		printk("eth_tx: txq_ctrl->txq=%d - no_resource: txq_count=%d, txq_size=%d, num=%d\n",
			txq_ctrl->txq, txq_ctrl->txq_count, txq_ctrl->txq_size, num);
		*/
		STAT_DBG(txq_ctrl->stats.txq_err++);
		return NULL;
	}
	return mvNetaTxqNextDescGet(txq_ctrl->q);
}

static inline void mv_eth_tx_desc_flush(struct neta_tx_desc *tx_desc)
{
#if defined(MV_CPU_BE)
	mvNetaTxqDescSwap(tx_desc);
#endif /* MV_CPU_BE */

	mvOsCacheLineFlush(NULL, tx_desc);
}

void mv_eth_ctrl_txdone(int num)
{
	mv_ctrl_txdone = num;
}

int mv_eth_ctrl_flag(int port, u32 flag, u32 val)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (!pp)
		return -ENODEV;

	if ((flag == MV_ETH_F_MH) && (pp->flags & MV_ETH_F_SWITCH)) {
		printk("Error: cannot change Marvell Header on a port used by the Gateway driver\n");
		return -EPERM;
	}

	if (val)
		pp->flags |= flag;
	else
		pp->flags &= ~flag;

	if (flag == MV_ETH_F_MH) {
		mvNetaMhSet(pp->port, val ? MV_NETA_MH : MV_NETA_MH_NONE);

#ifdef CONFIG_MV_ETH_NFP
		mvNfpPortCapSet(pp->port, NFP_P_MH, val);
#endif
	}
	return 0;
}

int mv_eth_ctrl_port_buf_num_set(int port, int long_num, int short_num)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp->flags & MV_ETH_F_STARTED) {
		printk("Port %d must be stopped before\n", port);
		return -EINVAL;
	}
	if (pp->pool_long != NULL) {
		/* Update number of buffers in existing pool (allocate or free) */
		if (pp->pool_long_num > long_num)
			mv_eth_pool_free(pp->pool_long->pool, pp->pool_long_num - long_num);
		else if (long_num > pp->pool_long_num)
			mv_eth_pool_add(pp->pool_long->pool, long_num - pp->pool_long_num);
	}
	pp->pool_long_num = long_num;

#ifdef CONFIG_MV_ETH_BM
	if (pp->pool_short != NULL) {
		/* Update number of buffers in existing pool (allocate or free) */
		if (pp->pool_short_num > short_num)
			mv_eth_pool_free(pp->pool_short->pool, pp->pool_short_num - short_num);
		else if (short_num > pp->pool_short_num)
			mv_eth_pool_add(pp->pool_short->pool, short_num - pp->pool_short_num);
	}
	pp->pool_short_num = short_num;
#endif /* CONFIG_MV_ETH_BM */

	return 0;
}

int mv_eth_ctrl_rxq_size_set(int port, int rxq, int value)
{
	struct eth_port *pp = mv_eth_port_by_id(port);
	struct rx_queue	*rxq_ctrl;

	if (pp->flags & MV_ETH_F_STARTED) {
		printk("Port %d must be stopped before\n", port);
		return -EINVAL;
	}
	rxq_ctrl = &pp->rxq_ctrl[rxq];
	if ((rxq_ctrl->q) && (rxq_ctrl->rxq_size != value)) {
		/* Reset is required when RXQ ring size is changed */
		mv_eth_rx_reset(pp->port);

		mvNetaRxqDelete(pp->port, rxq);
		rxq_ctrl->q = NULL;
	}
	pp->rxq_ctrl[rxq].rxq_size = value;

	/* New RXQ will be created during mv_eth_start_internals */
	return 0;
}

int mv_eth_ctrl_txq_size_set(int port, int txp, int txq, int value)
{
	struct tx_queue *txq_ctrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp->flags & MV_ETH_F_STARTED) {
		printk("Port %d must be stopped before\n", port);
		return -EINVAL;
	}
	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
	if ((txq_ctrl->q) && (txq_ctrl->txq_size != value)) {
		mv_eth_txq_delete(pp, txq_ctrl);
		/* Reset of port/txp is required when TXQ ring size is changed */
		/* Reset done before as part of stop_internals function */
	}
	txq_ctrl->txq_size = value;

	/* New TXQ will be created during mv_eth_start_internals */
	return 0;
}

int mv_eth_ctrl_txq_mode_get(int port, int txp, int txq, int *value)
{
	int mode = MV_ETH_TXQ_FREE, val = 0;
	struct tx_queue *txq_ctrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
	if (txq_ctrl->cpu_owner) {
		mode = MV_ETH_TXQ_CPU;
		val = txq_ctrl->cpu_owner;
	} else if (txq_ctrl->hwf_rxp < (MV_U8) mv_eth_ports_num) {
		mode = MV_ETH_TXQ_HWF;
		val = txq_ctrl->hwf_rxp;
	}
	if (value)
		*value = val;

	return mode;
}

/* Increment/Decrement CPU ownership for this TXQ */
int mv_eth_ctrl_txq_cpu_own(int port, int txp, int txq, int add)
{
	int mode;
	struct tx_queue *txq_ctrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if ((pp == NULL) || (pp->txq_ctrl == NULL))
		return -ENODEV;

	/* Check that new txp/txq can be allocated for CPU */
	mode = mv_eth_ctrl_txq_mode_get(port, txp, txq, NULL);

	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
	if (add) {
		if ((mode != MV_ETH_TXQ_CPU) && (mode != MV_ETH_TXQ_FREE))
			return -EINVAL;

		txq_ctrl->cpu_owner++;
	} else {
		if (mode != MV_ETH_TXQ_CPU)
			return -EINVAL;

		txq_ctrl->cpu_owner--;
	}
	return 0;
}

/* Set TXQ ownership to HWF from the RX port.  rxp=-1 - free TXQ ownership */
int mv_eth_ctrl_txq_hwf_own(int port, int txp, int txq, int rxp)
{
	int mode;
	struct tx_queue *txq_ctrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if ((pp == NULL) || (pp->txq_ctrl == NULL))
		return -ENODEV;

	/* Check that new txp/txq can be allocated for HWF */
	mode = mv_eth_ctrl_txq_mode_get(port, txp, txq, NULL);

	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

	if (rxp == -1) {
		if (mode != MV_ETH_TXQ_HWF)
			return -EINVAL;
	} else {
		if ((mode != MV_ETH_TXQ_HWF) && (mode != MV_ETH_TXQ_FREE))
			return -EINVAL;
	}

	txq_ctrl->hwf_rxp = (MV_U8) rxp;

	return 0;
}

/* Set TXQ for CPU originated packets */
int mv_eth_ctrl_txq_cpu_def(int port, int txp, int txq, int cpu)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (cpu >= CONFIG_NR_CPUS) {
		printk("cpu #%d is out of range: from 0 to %d\n",
			cpu, CONFIG_NR_CPUS - 1);
		return -EINVAL;
	}

	if (mvNetaTxpCheck(port, txp))
		return -EINVAL;

	if ((pp == NULL) || (pp->txq_ctrl == NULL))
		return -ENODEV;

	/* Decrement CPU ownership for old txq */
	mv_eth_ctrl_txq_cpu_own(port, pp->txp, pp->txq[cpu], 0);

	if (txq != -1) {
		if (mvNetaMaxCheck(txq, CONFIG_MV_ETH_TXQ))
			return -EINVAL;

		/* Increment CPU ownership for new txq */
		if (mv_eth_ctrl_txq_cpu_own(port, txp, txq, 1))
			return -EINVAL;
	}
	pp->txp = txp;
	pp->txq[cpu] = txq;

	return 0;
}

int mv_eth_ctrl_tx_cmd(int port, u32 tx_cmd)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (!pp)
		return -ENODEV;

	pp->hw_cmd = tx_cmd;

	return 0;
}

int mv_eth_ctrl_tx_mh(int port, u16 mh)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (!pp)
		return -ENODEV;

	pp->tx_mh = mh;

	return 0;
}

#ifdef CONFIG_MV_ETH_TX_SPECIAL
/* Register special transmit check function */
void mv_eth_tx_special_check_func(int port,
					int (*func)(int port, struct net_device *dev, struct sk_buff *skb,
								struct mv_eth_tx_spec *tx_spec_out))
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	pp->tx_special_check = func;
}
#endif /* CONFIG_MV_ETH_TX_SPECIAL */

#ifdef CONFIG_MV_ETH_RX_SPECIAL
/* Register special transmit check function */
void mv_eth_rx_special_proc_func(int port, void (*func)(int port, int rxq, struct net_device *dev,
							struct sk_buff *skb, struct neta_rx_desc *rx_desc))
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	pp->rx_special_proc = func;
}
#endif /* CONFIG_MV_ETH_RX_SPECIAL */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
static const struct net_device_ops mv_eth_netdev_ops = {
	.ndo_open = mv_eth_open,
	.ndo_stop = mv_eth_stop,
	.ndo_start_xmit = mv_eth_tx,
#if defined(CONFIG_MV_ETH_PNC_PARSER) || defined(CONFIG_MV_ETH_LEGACY_PARSER)
	.ndo_set_multicast_list = mv_eth_set_multicast_list,
#endif /* CONFIG_MV_ETH_PNC_PARSER || CONFIG_MV_ETH_LEGACY_PARSER */
	.ndo_set_mac_address = mv_eth_set_mac_addr,
	.ndo_change_mtu = mv_eth_change_mtu,
	.ndo_tx_timeout = mv_eth_tx_timeout,
};
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) */

#ifdef CONFIG_MV_ETH_SWITCH

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
static const struct net_device_ops mv_switch_netdev_ops = {
	.ndo_open = mv_eth_switch_start,
	.ndo_stop = mv_eth_switch_stop,
	.ndo_start_xmit = mv_eth_tx,
	.ndo_set_multicast_list = mv_eth_switch_set_multicast_list,
	.ndo_set_mac_address = mv_eth_switch_set_mac_addr,
	.ndo_change_mtu = mv_eth_switch_change_mtu,
	.ndo_tx_timeout = mv_eth_tx_timeout,
};
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) */

int mv_eth_switch_netdev_first = 0;
int mv_eth_switch_netdev_last = 0;

static inline struct net_device *mv_eth_switch_netdev_get(struct eth_port *pp, struct eth_pbuf *pkt)
{
	MV_U8 *data;
	int db_num;

	if (pp->flags & MV_ETH_F_SWITCH) {
		data = pkt->pBuf + pkt->offset;

		/* bits[4-7] of MSB in Marvell header */
		db_num = ((*data) >> 4);

		return mv_net_devs[mv_eth_switch_netdev_first + db_num];
	}
	return pp->dev;
}


void mv_eth_switch_priv_update(struct net_device *netdev, int i)
{
	struct eth_netdev *dev_priv;
	int print_flag, port, switch_port;

	/* Update dev_priv structure */
	dev_priv = MV_DEV_PRIV(netdev);
	dev_priv->port_map = 0;
	dev_priv->link_map = 0;

	print_flag = 1;
	for (port = 0; port < BOARD_ETH_SWITCH_PORT_NUM; port++) {
		if (switch_net_config.board_port_map[i] & (1 << port)) {
			if (print_flag) {
				printk(". Interface ports: ");
				print_flag = 0;
			}
			printk("%d ", port);
			switch_port = mvBoardSwitchPortGet(MV_SWITCH_ID_0, port);
			if (switch_port >= 0) {
				dev_priv->port_map |= (1 << switch_port);
				switch_enabled_ports |= (1 << switch_port);
			}
		}
	}
	printk("\n");
	dev_priv->vlan_grp_id = MV_SWITCH_GROUP_VLAN_ID(i);	/* e.g. 0x100, 0x200... */
	dev_priv->tx_vlan_mh = cpu_to_be16((i << 12) | dev_priv->port_map);
	dev_priv->cpu_port = mvBoardSwitchCpuPortGet(MV_SWITCH_ID_0);

	mv_eth_switch_vlan_set(dev_priv->vlan_grp_id, dev_priv->port_map, dev_priv->cpu_port);
}


int mv_eth_switch_netdev_init(struct eth_port *pp, int dev_i)
{
	int i;
	struct net_device *netdev;

	switch_enabled_ports = 0;

	for (i = 0; i < switch_net_config.netdev_max; i++) {
		netdev = mv_eth_netdev_init(pp, switch_net_config.mtu, switch_net_config.mac_addr[i]);
		if (netdev == NULL) {
			printk("mv_eth_switch_netdev_init: can't create netdevice\n");
			break;
		}
		mv_net_devs[dev_i++] = netdev;

		mv_eth_switch_priv_update(netdev, i);

	}
	return dev_i;
}

#endif /* CONFIG_MV_ETH_SWITCH */

MV_STATUS mvGppValueSet(MV_U32 group, MV_U32 mask, MV_U32 value);

#define AEI_LED_MASK ((1 << 23) | (1 << 24))
static int aei_led_value;
static struct timer_list aei_led_timer;

static void aei_led_off(void)
{
    MV_U32 val;

    val = MV_REG_READ(GPP_DATA_OUT_REG(0));
    val |= AEI_LED_MASK;
    MV_REG_WRITE(GPP_DATA_OUT_REG(0), val);
}

static void aei_led_on(void)
{
    MV_U32 val;

    val = MV_REG_READ(GPP_DATA_OUT_REG(0));
    val &= ~AEI_LED_MASK;
    val |= (aei_led_value & AEI_LED_MASK);
    MV_REG_WRITE(GPP_DATA_OUT_REG(0), val);
}

static void aei_led_timer_cb(unsigned long data)
{
    static int cd;

    if (!cd)
    {
	cd = 1;
	aei_led_on();
	mod_timer(&aei_led_timer, jiffies + 4);
    }
    else
	cd = 0;
}

static void aei_led_blink(int port, struct sk_buff *skb)
{
    if (port != 1 || g_wan_mode != MV_WAN_MODE_GBE)
        return;

    if (!timer_pending(&aei_led_timer))
    {
	aei_led_off();
	mod_timer(&aei_led_timer, jiffies + 6);
    }
}

static void aei_led_set(int port)
{
    u32 status;

    if (port != 1 || g_wan_mode != MV_WAN_MODE_GBE)
        return;

    status = MV_REG_READ(NETA_GMAC_STATUS_REG(port));

    if (!(status & NETA_GMAC_LINK_UP_MASK))
	aei_led_value = (1 << 23) | (1 << 24);
    else if (status & NETA_GMAC_SPEED_1000_MASK)
	aei_led_value = 0;
    else if (status & NETA_GMAC_SPEED_100_MASK)
	aei_led_value = (1 << 23);
    else
	aei_led_value = (1 << 24);

    aei_led_on();
}


void mv_eth_link_status_print(int port)
{
	MV_ETH_PORT_STATUS link;

	mvNetaLinkStatus(port, &link);

	if (link.linkup) {
		printk("link up");
		printk(", %s duplex", (link.duplex == MV_ETH_DUPLEX_FULL) ? "full" : "half");
		printk(", speed ");

		if (link.speed == MV_ETH_SPEED_1000)
			printk("1 Gbps\n");
		else if (link.speed == MV_ETH_SPEED_100)
			printk("100 Mbps\n");
		else
			printk("10 Mbps\n");
	} else
		printk("link down\n");
}

static void mv_eth_rx_error(struct eth_port *pp, struct neta_rx_desc *rx_desc)
{
	u32 status = rx_desc->status;
	int port = pp->port;

	STAT_ERR(pp->stats.rx_error++);

	if (pp->dev)
		MV_DEV_STAT(pp->dev).rx_errors++;
#ifdef ACTION_TEC
	//The error packets status could be saw by ifconfig devname,
	//print log in net_rx_action would be attacked by DoS
	return;
#else
	if (!printk_ratelimit())
		return;

	if ((status & NETA_RX_FL_DESC_MASK) != NETA_RX_FL_DESC_MASK) {
		printk("giga #%d: bad rx status %08x (buffer oversize), size=%d\n",
				port, status, rx_desc->dataSize);
		return;
	}

	switch (status & NETA_RX_ERR_CODE_MASK) {
	case NETA_RX_ERR_CRC:
		printk("giga #%d: bad rx status %08x (crc error), size=%d\n", port, status, rx_desc->dataSize);
		break;
	case NETA_RX_ERR_OVERRUN:
		printk("giga #%d: bad rx status %08x (overrun error), size=%d\n", port, status, rx_desc->dataSize);
		break;
	case NETA_RX_ERR_LEN:
		printk("giga #%d: bad rx status %08x (max frame length error), size=%d\n", port, status, rx_desc->dataSize);
		break;
	case NETA_RX_ERR_RESOURCE:
		printk("giga #%d: bad rx status %08x (resource error), size=%d\n", port, status, rx_desc->dataSize);
		break;
	}
#ifdef CONFIG_MV_ETH_DEBUG_CODE
	mv_eth_rx_desc_print(rx_desc);
#endif /* CONFIG_MV_ETH_DEBUG_CODE */
#endif /* ACTION_TEC */
}

void mv_eth_skb_print(struct sk_buff *skb)
{
	printk("skb=%p: head=%p data=%p tail=%p end=%p\n", skb, skb->head, skb->data, skb->tail, skb->end);
	printk("\t truesize=%d len=%d, data_len=%d mac_len=%d\n", skb->truesize, skb->len, skb->data_len, skb->mac_len);
	printk("\t users=%d dataref=%d nr_frags=%d tso_size=%d tso_segs=%d\n",
	       atomic_read(&skb->users), atomic_read(&skb_shinfo(skb)->dataref),
	       skb_shinfo(skb)->nr_frags, skb_shinfo(skb)->tso_size, skb_shinfo(skb)->tso_segs);
	printk("\t proto=%d, ip_summed=%d\n", ntohs(skb->protocol), skb->ip_summed);
#ifdef CONFIG_NET_SKB_RECYCLE
	printk("\t skb_recycle=%p hw_cookie=%p\n", skb->skb_recycle, skb->hw_cookie);
#endif /* CONFIG_NET_SKB_RECYCLE */
}

void mv_eth_rx_desc_print(struct neta_rx_desc *desc)
{
	int i;
	u32 *words = (u32 *) desc;

	printk("RX desc - %p: ", desc);
	for (i = 0; i < 8; i++)
		printk("%8.8x ", *words++);
	printk("\n");

	if (desc->status & NETA_RX_IP4_FRAG_MASK)
		printk("Frag, ");

	printk("size=%d, L3_offs=%d, IP_hlen=%d, L4_csum=%s, L3=",
	       desc->dataSize,
	       (desc->status & NETA_RX_L3_OFFSET_MASK) >> NETA_RX_L3_OFFSET_OFFS,
	       (desc->status & NETA_RX_IP_HLEN_MASK) >> NETA_RX_IP_HLEN_OFFS,
	       (desc->status & NETA_RX_L4_CSUM_OK_MASK) ? "Ok" : "Bad");

	if (NETA_RX_L3_IS_IP4(desc->status))
		printk("IPv4, ");
	else if (NETA_RX_L3_IS_IP4_ERR(desc->status))
		printk("IPv4 bad, ");
	else if (NETA_RX_L3_IS_IP6(desc->status))
		printk("IPv6, ");
	else
		printk("Unknown, ");

	printk("L4=");
	if (NETA_RX_L4_IS_TCP(desc->status))
		printk("TCP");
	else if (NETA_RX_L4_IS_UDP(desc->status))
		printk("UDP");
	else
		printk("Unknown");
	printk("\n");

#ifdef CONFIG_MV_ETH_PNC
	printk("RINFO: ");
	if (desc->pncInfo & NETA_PNC_DA_MC)
		printk("DA_MC, ");
	if (desc->pncInfo & NETA_PNC_DA_BC)
		printk("DA_BC, ");
	if (desc->pncInfo & NETA_PNC_DA_UC)
		printk("DA_UC, ");
	if (desc->pncInfo & NETA_PNC_IGMP)
		printk("IGMP, ");
	if (desc->pncInfo & NETA_PNC_SNAP)
		printk("SNAP, ");
	if (desc->pncInfo & NETA_PNC_VLAN)
		printk("VLAN, ");
	if (desc->pncInfo & NETA_PNC_PPPOE)
		printk("PPPOE, ");
	if (desc->pncInfo & NETA_PNC_NFP)
		printk("NFP, ");
	if (desc->pncInfo & NETA_PNC_SWF)
		printk("SWF, ");
#endif /* CONFIG_MV_ETH_PNC */

	printk("\n");
}

void mv_eth_tx_desc_print(struct neta_tx_desc *desc)
{
	int i;
	u32 *words = (u32 *) desc;

	printk("TX desc - %p: ", desc);
	for (i = 0; i < 8; i++)
		printk("%8.8x ", *words++);
	printk("\n");
}

void mv_eth_pkt_print(struct eth_pbuf *pkt)
{
	printk("pkt: len=%d off=%d cmd=0x%x pool=%d "
	       "tos=%d gpon=0x%x skb=%p pa=%lx buf=%p\n",
	       pkt->bytes, pkt->offset, pkt->tx_cmd, pkt->pool,
	       pkt->tos, pkt->hw_cmd, pkt->osInfo, pkt->physAddr, pkt->pBuf);
/*
	mvDebugMemDump(pkt->pBuf + pkt->offset, 64, 1);
    mvOsCacheInvalidate(NULL, pkt->pBuf + pkt->offset, pkt->bytes);
*/
}

static inline void mv_eth_rx_csum(struct eth_port *pp, struct neta_rx_desc *rx_desc, struct sk_buff *skb)
{
#if defined(CONFIG_MV_ETH_RX_CSUM_OFFLOAD)
	if (pp->rx_csum_offload &&
	    ((NETA_RX_L3_IS_IP4(rx_desc->status) ||
	      NETA_RX_L3_IS_IP6(rx_desc->status)) && (rx_desc->status & NETA_RX_L4_CSUM_OK_MASK))) {
		skb->csum = 0;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		STAT_DBG(pp->stats.rx_csum_hw++);
		return;
	}
#endif /* CONFIG_MV_ETH_RX_CSUM_OFFLOAD */

	skb->ip_summed = CHECKSUM_NONE;
	STAT_DBG(pp->stats.rx_csum_sw++);
}

static inline int mv_eth_pool_put(struct bm_pool *pool, struct eth_pbuf *pkt)
{
	unsigned long flags = 0;

	MV_ETH_LOCK(&pool->lock, flags);
	if (mvStackIsFull(pool->stack)) {
		STAT_ERR(pool->stats.stack_full++);
		MV_ETH_UNLOCK(&pool->lock, flags);

		/* free pkt+skb */
		dev_kfree_skb_any((struct sk_buff *)pkt->osInfo);
		mvOsFree(pkt);
		return 1;
	}
	mvStackPush(pool->stack, (MV_U32) pkt);
	STAT_DBG(pool->stats.stack_put++);
	MV_ETH_UNLOCK(&pool->lock, flags);
	return 0;
}

static inline struct eth_pbuf *mv_eth_pool_get(struct bm_pool *pool)
{
	struct eth_pbuf *pkt = NULL;
	struct sk_buff *skb;
	unsigned long flags = 0;

	MV_ETH_LOCK(&pool->lock, flags);

	if (mvStackIndex(pool->stack) > 0) {
		STAT_DBG(pool->stats.stack_get++);
		pkt = (struct eth_pbuf *)mvStackPop(pool->stack);
	} else
		STAT_ERR(pool->stats.stack_empty++);

	MV_ETH_UNLOCK(&pool->lock, flags);
	if (pkt)
		return pkt;

	/* Try to allocate new pkt + skb */
	pkt = mvOsMalloc(sizeof(struct eth_pbuf));
	if (pkt) {
		skb = mv_eth_skb_alloc(pool, pkt);
		if (!skb) {
			mvOsFree(pkt);
			pkt = NULL;
		}
	}
	return pkt;
}

/* Pass pkt to BM Pool or RXQ ring */
static inline void mv_eth_rxq_refill(struct eth_port *pp, int rxq,
				     struct eth_pbuf *pkt, struct bm_pool *pool, struct neta_rx_desc *rx_desc)
{
	if (mv_eth_pool_bm(pool)) {
		/* Refill BM pool */
		STAT_DBG(pool->stats.bm_put++);
		mvBmPoolPut(pkt->pool, (MV_ULONG) pkt->physAddr);
		mvOsCacheLineInv(NULL, rx_desc);
	} else {
		/* Refill Rx descriptor */
		STAT_DBG(pp->stats.rxq_fill[rxq]++);
		mvNetaRxDescFill(rx_desc, pkt->physAddr, (MV_U32)pkt);
	}
}

static inline int mv_eth_tx_done_policy(u32 cause)
{
	return fls(cause >> NETA_CAUSE_TXQ_SENT_DESC_OFFS) - 1;
}

static inline int mv_eth_rx_policy(u32 cause)
{
	return fls(cause >> NETA_CAUSE_RXQ_OCCUP_DESC_OFFS) - 1;
}

static inline int mv_eth_txq_tos_map_get(struct eth_port *pp, MV_U8 tos)
{
	MV_U8 q = pp->txq_tos_map[tos];

	if (q == MV_ETH_TXQ_INVALID)
		return pp->txq[smp_processor_id()];

	return q;
}

static inline int mv_eth_tx_policy(struct eth_port *pp, struct sk_buff *skb)
{
	int txq = pp->txq[smp_processor_id()];

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	if (skb->nh.iph)
#else
	if (ip_hdr(skb)) 
#endif
	{
		MV_U8   tos;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		tos = skb->nh.iph->tos;
#else
		tos = ip_hdr(skb)->tos;
#endif
		txq = mv_eth_txq_tos_map_get(pp, tos);
	}
	return txq;
}

#ifdef CONFIG_NET_SKB_RECYCLE
static int mv_eth_skb_recycle(struct sk_buff *skb)
{
	struct eth_pbuf *pkt = skb->hw_cookie;
	struct bm_pool *pool = &mv_eth_pool[pkt->pool];
	int status = 0;

	if (skb_recycle_check(skb, pool->pkt_size)) {

#ifdef CONFIG_MV_ETH_DEBUG_CODE
		/* Sanity check */
		if (skb->truesize != ((skb->end - skb->head) + sizeof(struct sk_buff))) {
			mv_eth_skb_print(skb);
		}
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

		STAT_DBG(pool->stats.skb_recycled_ok++);
		mvOsCacheInvalidate(NULL, skb->head, RX_BUF_SIZE(pool->pkt_size));

		status = mv_eth_pool_put(pool, pkt);

#ifdef ETH_SKB_DEBUG
		if (status == 0)
			mv_eth_skb_save(skb, "recycle");
#endif /* ETH_SKB_DEBUG */

		return 0;
	}
	mvOsFree(pkt);
	skb->hw_cookie = NULL;

	STAT_DBG(pool->stats.skb_recycled_err++);
	/* printk("mv_eth_skb_recycle failed: pkt=%p, skb=%p\n", pkt, skb); */
	return 1;
}
#endif /* CONFIG_NET_SKB_RECYCLE */

static struct sk_buff *mv_eth_skb_alloc(struct bm_pool *pool, struct eth_pbuf *pkt)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(pool->pkt_size);
	if (!skb) {
		STAT_ERR(pool->stats.skb_alloc_oom++);
		return NULL;
	}
	STAT_DBG(pool->stats.skb_alloc_ok++);

#ifdef ETH_SKB_DEBUG
	mv_eth_skb_save(skb, "alloc");
#endif /* ETH_SKB_DEBUG */

#ifdef CONFIG_MV_ETH_BM
	/* Save pkt as first 4 bytes in the buffer */
	*((MV_U32 *) skb->head) = (MV_U32) pkt;
	mvOsCacheLineFlush(NULL, skb->head);
#endif /* CONFIG_MV_ETH_BM */

	pkt->osInfo = (void *)skb;
	pkt->pBuf = skb->head;
	pkt->physAddr = mvOsCacheInvalidate(NULL, skb->head, RX_BUF_SIZE(pool->pkt_size));
	pkt->offset = NET_SKB_PAD;
	pkt->pool = pool->pool;

	return skb;
}

#ifdef CONFIG_MV_ETH_RX_DESC_PREFETCH
static inline struct neta_rx_desc *mv_eth_rx_prefetch(struct eth_port *pp, MV_NETA_RXQ_CTRL *rx_ctrl,
									  int rx_done, int rx_todo)
{
	struct neta_rx_desc	*rx_desc, *next_desc;

	rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
	if (rx_done == 0) {
		/* First descriptor in the NAPI loop */
		mvOsCacheLineInv(NULL, rx_desc);
		prefetch(rx_desc);
	}
	if ((rx_done + 1) == rx_todo) {
		/* Last descriptor in the NAPI loop - prefetch are not needed */
		return rx_desc;
	}
	/* Prefetch next descriptor */
	next_desc = mvNetaRxqDescGet(rx_ctrl);
	mvOsCacheLineInv(NULL, next_desc);
	prefetch(next_desc);

	return rx_desc;
}
#endif /* CONFIG_MV_ETH_RX_DESC_PREFETCH */

static inline int mv_eth_rx(struct eth_port *pp, int rx_todo, int rxq)
{
	struct net_device *dev;
	MV_NETA_RXQ_CTRL *rx_ctrl = pp->rxq_ctrl[rxq].q;
	int rx_done, rx_filled, err;
	struct neta_rx_desc *rx_desc;
	u32 rx_status;
	int rx_bytes;
	struct eth_pbuf *pkt;
	struct sk_buff *skb;
	struct bm_pool *pool;

	/* Get number of received packets */
	rx_done = mvNetaRxqBusyDescNumGet(pp->port, rxq);

	if (rx_todo > rx_done)
		rx_todo = rx_done;

	rx_done = 0;
	rx_filled = 0;

	/* Fairness NAPI loop */
	while (rx_done < rx_todo) {

#ifdef CONFIG_MV_ETH_RX_DESC_PREFETCH
		rx_desc = mv_eth_rx_prefetch(pp, rx_ctrl, rx_done, rx_todo);
#else
		rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
		mvOsCacheLineInv(NULL, rx_desc);
		prefetch(rx_desc);
#endif /* CONFIG_MV_ETH_RX_DESC_PREFETCH */

		rx_done++;
		rx_filled++;

#if defined(MV_CPU_BE)
		mvNetaRxqDescSwap(rx_desc);
#endif /* MV_CPU_BE */

#ifdef CONFIG_MV_ETH_DEBUG_CODE
		if (pp->flags & MV_ETH_F_DBG_RX) {
			printk("\n");
			mv_eth_rx_desc_print(rx_desc);
		}
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

		rx_status = rx_desc->status;
		pkt = (struct eth_pbuf *)rx_desc->bufCookie;

		/* Speculative ICache prefetch WA: should be replaced with dma_unmap_single (invalidate l2) */
		/*mvOsCacheInvalidate(NULL, pkt->pBuf + pkt->offset, rx_desc->dataSize);*/
		mvOsCacheMultiLineInv(NULL, pkt->pBuf + pkt->offset, rx_desc->dataSize);

#ifdef CONFIG_MV_ETH_RX_PKT_PREFETCH
		prefetch(pkt->pBuf + pkt->offset);
		prefetch(pkt->pBuf + pkt->offset + CPU_D_CACHE_LINE_SIZE);
#endif /* CONFIG_MV_ETH_RX_PKT_PREFETCH */

		pool = &mv_eth_pool[pkt->pool];

		if (((rx_status & NETA_RX_FL_DESC_MASK) != NETA_RX_FL_DESC_MASK) ||
			(rx_status & NETA_RX_ES_MASK)) {

			mv_eth_rx_error(pp, rx_desc);

			mv_eth_rxq_refill(pp, rxq, pkt, pool, rx_desc);
			continue;
		}

#ifdef CONFIG_MV_ETH_SWITCH
		dev = mv_eth_switch_netdev_get(pp, pkt);
#else
		dev = pp->dev;
#endif /* CONFIG_MV_ETH_SWITCH */

		STAT_DBG(pp->stats.rxq[rxq]++);
		MV_DEV_STAT(dev).rx_packets++;


		rx_bytes = rx_desc->dataSize - (MV_ETH_CRC_SIZE + MV_ETH_MH_SIZE);
		MV_DEV_STAT(dev).rx_bytes += rx_bytes;

#ifdef CONFIG_MV_ETH_DEBUG_CODE
		if (pp->flags & MV_ETH_F_DBG_RX)
			mvDebugMemDump(pkt->pBuf + pkt->offset, 64, 1);
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

#if defined(CONFIG_MV_ETH_PNC) && defined(CONFIG_MV_ETH_RX_SPECIAL)
		/* Special RX processing */
		if (rx_desc->pncInfo & NETA_PNC_RX_SPECIAL) {
			if (pp->rx_special_proc) {
				pp->rx_special_proc(pp->port, rxq, dev, (struct sk_buff *)(pkt->osInfo), rx_desc);
				STAT_INFO(pp->stats.rx_special++);

				/* Refill processing */
				err = mv_eth_refill(pp, rxq, pkt, pool, rx_desc);
				if (err) {
					printk("Linux processing - Can't refill\n");
					pp->rxq_ctrl[rxq].missed++;
					rx_filled--;
				}
				continue;
			}
		}
#endif /* CONFIG_MV_ETH_PNC && CONFIG_MV_ETH_RX_SPECIAL */

#ifdef CONFIG_MV_ETH_NFP
		if (mv_ctrl_nfp) {
			MV_STATUS status;

			pkt->bytes = rx_bytes + MV_ETH_MH_SIZE;
			pkt->offset = NET_SKB_PAD;
			pkt->tx_cmd = NETA_TX_L4_CSUM_NOT;
			pkt->hw_cmd = 0;

			status = mv_eth_nfp(pp, rx_desc, pkt);
			if (status == MV_OK) {
				STAT_DBG(pp->stats.rx_nfp++);

				/* Packet moved to transmit - refill now */
				if (!mv_eth_pool_bm(pool)) {
					/* No BM, refill descriptor from rx pool */
					pkt = mv_eth_pool_get(pool);
					if (pkt) {
						STAT_DBG(pp->stats.rxq_fill[rxq]++);
						mvNetaRxDescFill(rx_desc, pkt->physAddr, (MV_U32)pkt);
					} else {
						/* RX resource error - RX descriptor is done but not refilled */
						/* FIXME: remember to refill later */

						printk("Alloc failed - Can't refill\n");
						rx_filled--;
						pp->rxq_ctrl[rxq].missed++;
						mv_eth_add_cleanup_timer(pp);
						break;
					}
				} else {
					/* BM - no refill */
					mvOsCacheLineInv(NULL, rx_desc);
				}
				continue;
			} else if (status == MV_DROPPED) {
				/* Refill the same buffer - ??? reset pkt and skb */
				STAT_DBG(pp->stats.rx_nfp_drop++);
				mv_eth_rxq_refill(pp, rxq, pkt, pool, rx_desc);
				continue;
			}
			/* MV_TERMINATE - packet returned to slow path */
		}
#endif /* CONFIG_MV_ETH_NFP */

		/* Linux processing */
		skb = (struct sk_buff *)(pkt->osInfo);

		skb->data += MV_ETH_MH_SIZE;
		skb->tail += (rx_bytes + MV_ETH_MH_SIZE);
		skb->len = rx_bytes;

#ifdef ETH_SKB_DEBUG
		mv_eth_skb_check(skb);
#endif /* ETH_SKB_DEBUG */

#ifdef CONFIG_VERIZON_HPM
		{
		    struct vlan_ethhdr *vlan_eth_hdr = (struct vlan_ethhdr *)skb->data;

		    /* Remove VLAN header if vid is 0. */
		    if ((ntohs(vlan_eth_hdr->h_vlan_proto) == ETH_P_8021Q) &&
			((vlan_eth_hdr->h_vlan_TCI & htons(VLAN_VID_MASK)) == 0))
		    {
			memmove(skb->data + 4, skb->data, ETH_ALEN * 2);
			skb_pull(skb, 4);
		    }
		}
#endif /* CONFIG_VERIZON_HPM */

		skb->protocol = eth_type_trans(skb, dev);

		mv_eth_rx_csum(pp, rx_desc, skb);

#ifdef CONFIG_NET_SKB_RECYCLE
		if (mv_eth_is_recycle()) {
			skb->skb_recycle = mv_eth_skb_recycle;
			skb->hw_cookie = pkt;
			pkt = NULL;
		}
#endif /* CONFIG_NET_SKB_RECYCLE */

#ifdef CONFIG_MV_ETH_GRO
		if (dev->features & NETIF_F_GRO) {
			if (!(rx_status & NETA_RX_IP4_FRAG_MASK) && (NETA_RX_L4_IS_TCP(rx_status))) {
				struct iphdr *iph = (struct iphdr *)skb->data;
				if (iph->daddr == mv_eth_dev_ip(dev)) {
					rx_status = 0;
					STAT_DBG(pp->stats.rx_gro++);
					STAT_DBG(pp->stats.rx_gro_bytes += skb->len);

					napi_gro_receive(&pp->napi, skb);
					skb = NULL;
				}
			}
		}
#endif /* CONFIG_MV_ETH_GRO */

		if (skb) {
		    static unsigned char __fc_mac[] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x01};
		    struct ethhdr *eth = eth_hdr(skb);

		    STAT_DBG(pp->stats.rx_netif++);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
			skb->dev = dev;
#endif
			aei_led_blink(pp->port, skb);

			/* Check for Pause Frames and drop it */
			if (skb->pkt_type == PACKET_MULTICAST && !compare_ether_addr(eth->h_dest, __fc_mac))
			    dev_kfree_skb(skb);
			else
			    rx_status = netif_receive_skb(skb);

			STAT_DBG(if (rx_status)	(pp->stats.rx_drop_sw++));
		}

		/* Refill processing: */
		err = mv_eth_refill(pp, rxq, pkt, pool, rx_desc);
		if (err) {
			printk("Linux processing - Can't refill\n");
			pp->rxq_ctrl[rxq].missed++;
			mv_eth_add_cleanup_timer(pp);
			rx_filled--;
		}
	}

	/* Update RxQ management counters */
	mvNetaRxqDescNumUpdate(pp->port, rxq, rx_done, rx_filled);

	return rx_done;
}

#ifdef CONFIG_MV_ETH_NFP
static MV_STATUS mv_eth_nfp_tx(struct neta_rx_desc *rx_desc, struct eth_pbuf *pkt)
{
	struct net_device *dev = (struct net_device *)pkt->dev;
	struct eth_port *pp = MV_ETH_PRIV(dev);
	struct neta_tx_desc *tx_desc;
	u32 tx_cmd;
#ifndef CONFIG_MV_ETH_TXDONE_ISR
	u32 tx_done = 0;
#endif
	int txq, txp;
	struct tx_queue *txq_ctrl;

	/* Get TxQ to send packet with BM if supported */
	txq = mv_eth_txq_tos_map_get(pp, pkt->tos);
	txp = pp->txp;
	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

#ifndef CONFIG_MV_ETH_TXDONE_ISR
	if (txq_ctrl->txq_count >= mv_ctrl_txdone)
		tx_done = mv_eth_txq_done(pp, txq_ctrl);

	STAT_DIST(if (tx_done < pp->dist_stats.tx_done_dist_size)
			pp->dist_stats.tx_done_dist[tx_done]++);

	/* If after calling mv_eth_txq_done, txq_ctrl->txq_count is still > 0, we need to set the timer */
	if ((txq_ctrl->txq_count > 0)) {
		mv_eth_add_tx_done_timer(pp);
	}
#endif

	/* Get next descriptor for tx, single buffer, so FIRST & LAST */
	tx_desc = mv_eth_tx_desc_get(txq_ctrl, 1);
	if (tx_desc == NULL) {

		/* No resources: Drop */
		MV_DEV_STAT(dev).tx_dropped++;
		return MV_DROPPED;
	}
	txq_ctrl->txq_count++;

	/* tx_cmd - word accumulated by NFP processing */
	tx_cmd = pkt->tx_cmd;

#ifdef CONFIG_MV_ETH_BM
	tx_cmd |= NETA_TX_BM_ENABLE_MASK | NETA_TX_BM_POOL_ID_MASK(pkt->pool);
	txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = (u32) NULL;
	mv_eth_shadow_inc_put(txq_ctrl);
#else
	/* Remember pkt in separate TxQ shadow */
	txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = (u32) pkt;
	mv_eth_shadow_inc_put(txq_ctrl);
#endif /* CONFIG_MV_ETH_BM */

	tx_desc->command = tx_cmd | NETA_TX_FLZ_DESC_MASK;
	tx_desc->dataSize = pkt->bytes;
	tx_desc->bufPhysAddr = pkt->physAddr;

	/* FIXME: PON only? --BK */
	tx_desc->hw_cmd = pkt->hw_cmd;

#ifdef CONFIG_MV_ETH_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_TX) {
		printk("\n");
		mv_eth_tx_desc_print(tx_desc);
		mv_eth_pkt_print(pkt);
	}
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

	mv_eth_tx_desc_flush(tx_desc);

	/* Enable transmit by update PENDING counter */
	mvNetaTxqPendDescAdd(pp->port, txp, txq, 1);

	/* FIXME: stats includes MH --BK */
	MV_DEV_STAT(dev).tx_packets++;
	MV_DEV_STAT(dev).tx_bytes += pkt->bytes;
	STAT_DBG(txq_ctrl->stats.txq_tx++);

	return MV_OK;
}

/* Function returns the following error codes:
 *  MV_OK - packet processed and sent successfully by NFP
 *  MV_TERMINATE - packet can't be processed by NFP - pass to Linux processing
 *  MV_DROPPED - packet processed by NFP, but not sent (dropped)
 */
static MV_STATUS mv_eth_nfp(struct eth_port *pp, struct neta_rx_desc *rx_desc, struct eth_pbuf *pkt)
{
	MV_STATUS status;

#ifdef CONFIG_MV_ETH_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_RX) {
		mv_eth_rx_desc_print(rx_desc);
		mv_eth_pkt_print(pkt);
	}
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

#ifdef CONFIG_MV_ETH_NFP_SWF
	if (rx_desc->pncInfo & NETA_PNC_SWF) {
		status = mvNfpSwf(pp->port, rx_desc, pkt);
		goto tx;
	}
#endif /* CONFIG_MV_ETH_NFP_SWF */

#ifdef NFP_PNC
	if (rx_desc->pncInfo & NETA_PNC_NFP) {
		status = mvNfpPnC(pp->port, rx_desc, pkt);
		goto tx;
	}
#endif /* NFP_PNC */

	status = mvNfpRx(pp->port, rx_desc, pkt);
#if defined (CONFIG_MV_ETH_NFP_SWF) || defined (NFP_PNC)
tx:
#endif
	if (status == MV_OK)
		status = mv_eth_nfp_tx(rx_desc, pkt);
	return status;
}
#endif /* CONFIG_MV_ETH_NFP */

/* Reuse pkt if possible, allocate new skb and move BM pool or RXQ ring */
static inline int mv_eth_refill(struct eth_port *pp, int rxq,
				struct eth_pbuf *pkt, struct bm_pool *pool, struct neta_rx_desc *rx_desc)
{
	if (pkt == NULL) {
		pkt = mv_eth_pool_get(pool);
		if (pkt == NULL)
			return 1;
	} else {
		struct sk_buff *skb;

		/* No recycle -  alloc new skb */
		skb = mv_eth_skb_alloc(pool, pkt);
		if (!skb) {
			mvOsFree(pkt);
			pool->missed++;
			mv_eth_add_cleanup_timer(pp);
			return 1;
		}
	}
	mv_eth_rxq_refill(pp, rxq, pkt, pool, rx_desc);

	return 0;
}

#ifdef MV_ETH_MOCA_PRIORITY_SUPPORT
/*
 * This routine adds a VLAN Header with 802.1p priority set to the value
 * in skb->priority for all MoCA packets
 */
static int mv_add_vlan_tag_for_moca_pkt(struct sk_buff **pskb, struct net_device *dev)
{
    struct sk_buff *skb = *pskb;
    struct vlan_ethhdr *veth;

    veth = (struct vlan_ethhdr *)(skb->data + MV_ETH_MH_SIZE);
    if (veth->h_vlan_proto != __constant_htons(ETH_P_8021Q)) 
    {
	unsigned char *bufhdr;

	if (skb_headroom(skb) < VLAN_HLEN) 
	{
	    struct sk_buff *sk_tmp = skb;
	    skb = skb_realloc_headroom(sk_tmp, VLAN_HLEN);
	    dev_kfree_skb(sk_tmp);
	    if (!skb) 
		return -1;
	}
	else
	{
	    skb = skb_unshare(skb, GFP_ATOMIC);
	    if (!skb) 
		return -1;
	}

	bufhdr = skb_push(skb, VLAN_HLEN);

	/* Move the mac addresses to the beginning of the new header. */
	memmove(skb->data, skb->data + VLAN_HLEN, 2*VLAN_ETH_ALEN+MV_ETH_MH_SIZE);

	veth = (struct vlan_ethhdr *)(bufhdr+MV_ETH_MH_SIZE);
	veth->h_vlan_proto = __constant_htons(ETH_P_8021Q);

	/* Set VLAN ID as 0 for this packet */
	veth->h_vlan_TCI = 0;

	skb->protocol = __constant_htons(ETH_P_8021Q);
	skb->mac.raw -= VLAN_HLEN;
	skb->nh.raw -= VLAN_HLEN;
    }

    veth->h_vlan_TCI &= ~(__constant_htons(0xE000));
    veth->h_vlan_TCI |= htons((skb->priority << 13) & 0xE000);

    *pskb = skb;

    return 0;
}
#endif /* MV_ETH_MOCA_PRIORITY_SUPPORT */

static int mv_eth_tx(struct sk_buff *skb , struct net_device *dev)
{
	struct eth_port *pp = MV_ETH_PRIV(dev);
	struct eth_netdev *dev_priv = MV_DEV_PRIV(dev);
	unsigned long flags = 0;
	int frags = 0;
	bool tx_spec_ready = false;
	struct mv_eth_tx_spec tx_spec;
	u32 tx_cmd;
	u16 mh;
	struct tx_queue *txq_ctrl = NULL;
	struct neta_tx_desc *tx_desc;

	if (!MV_ETH_TRYLOCK(pp->lock, flags))
		return NETDEV_TX_LOCKED;

	aei_led_blink(pp->port, skb);

#ifdef MV_ETH_MOCA_PRIORITY_SUPPORT
	if ((pp->flags & MV_ETH_F_SWITCH_RG) && (skb->priority != 0))
	    mv_add_vlan_tag_for_moca_pkt(&skb, dev);
#endif /* MV_ETH_MOCA_PRIORITY_SUPPORT */

#if defined(CONFIG_MV_ETH_TX_SPECIAL)
	if (pp->tx_special_check) {

		if (pp->tx_special_check(pp->port, dev, skb, &tx_spec)) {
			STAT_INFO(pp->stats.tx_special++);
			if (tx_spec.tx_func) {
				tx_spec.tx_func(skb->data, skb->len, &tx_spec);
				goto out;
			} else {
				/* Check validity of tx_spec txp/txq must be CPU owned */
				tx_spec_ready = true;
			}
		}
	}
#endif /* CONFIG_MV_ETH_TX_SPECIAL */

	/* Get TXQ (without BM) to send packet generated by Linux */
	if (tx_spec_ready == false) {
		tx_spec.txp = pp->txp;
		tx_spec.txq = mv_eth_tx_policy(pp, skb);
		tx_spec.hw_cmd = pp->hw_cmd;
		tx_spec.flags = pp->flags;
	}

#ifdef CONFIG_MV_ETH_TSO
	/* GSO/TSO */
	if (skb_is_gso(skb)) {
		txq_ctrl = mv_eth_tx_tso(skb, dev, &tx_spec, &frags);
		goto out;
	}
#endif /* CONFIG_MV_ETH_TSO */

	txq_ctrl = &pp->txq_ctrl[tx_spec.txp * CONFIG_MV_ETH_TXQ + tx_spec.txq];

	frags = skb_shinfo(skb)->nr_frags + 1;

	if (tx_spec.flags & MV_ETH_F_MH) {
		if (tx_spec.flags & MV_ETH_F_SWITCH)
			mh = dev_priv->tx_vlan_mh;
		else
			mh = pp->tx_mh;

		if (mv_eth_skb_mh_add(skb, mh)) {
			frags = 0;
			goto out;
		}
	}

	tx_desc = mv_eth_tx_desc_get(txq_ctrl, frags);
	if (tx_desc == NULL) {
		frags = 0;
		goto out;
	}

	/* Don't use BM for Linux packets: NETA_TX_BM_ENABLE_MASK = 0 */
	/* NETA_TX_PKT_OFFSET_MASK = 0 - for all descriptors */
	tx_cmd = mv_eth_tx_csum(pp, skb);

#ifdef CONFIG_MV_PON
	tx_desc->hw_cmd = tx_spec.hw_cmd;
#endif

	/* FIXME: beware of nonlinear --BK */
	tx_desc->dataSize = skb_headlen(skb);

	tx_desc->bufPhysAddr = mvOsCacheFlush(NULL, skb->data, tx_desc->dataSize);

	if (frags == 1) {
		/*
		 * First and Last descriptor
		 */
		if (tx_spec.flags & MV_ETH_F_NO_PAD)
			tx_cmd |= NETA_TX_F_DESC_MASK | NETA_TX_L_DESC_MASK;
		else
			tx_cmd |= NETA_TX_FLZ_DESC_MASK;

		tx_desc->command = tx_cmd;
		mv_eth_tx_desc_flush(tx_desc);

		txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = ((MV_ULONG) skb | MV_ETH_SHADOW_SKB);
		mv_eth_shadow_inc_put(txq_ctrl);
	} else {

		/* First but not Last */
		tx_cmd |= NETA_TX_F_DESC_MASK;

		txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = 0;
		mv_eth_shadow_inc_put(txq_ctrl);

		tx_desc->command = tx_cmd;
		mv_eth_tx_desc_flush(tx_desc);

		/* Continue with other skb fragments */
		mv_eth_tx_frag_process(pp, skb, txq_ctrl, tx_spec.flags);
		STAT_DBG(pp->stats.tx_sg++);
	}
/*
    printk("tx: frags=%d, tx_desc[0x0]=%x [0xc]=%x, wr_id=%d, rd_id=%d, skb=%p\n",
			frags, tx_desc->command,tx_desc->hw_cmd,
			txq_ctrl->shadow_txq_put_i, txq_ctrl->shadow_txq_get_i, skb);
*/
	txq_ctrl->txq_count += frags;

#ifdef CONFIG_MV_ETH_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_TX) {
		printk("\n");
		printk("%s - eth_tx_%lu: port=%d, txp=%d, txq=%d, skb=%p, head=%p, data=%p, size=%d\n",
		       dev->name, MV_DEV_STAT(dev).tx_packets, pp->port, tx_spec.txp, tx_spec.txq, skb,
			   skb->head, skb->data, skb->len);
		mv_eth_tx_desc_print(tx_desc);
		mv_eth_skb_print(skb);
		mvDebugMemDump(skb->data, 64, 1);
	}
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

	/* Enable transmit */
	mvNetaTxqPendDescAdd(pp->port, tx_spec.txp, tx_spec.txq, frags);

	STAT_DBG(txq_ctrl->stats.txq_tx += frags);

out:
	if (frags > 0) {
		MV_DEV_STAT(dev).tx_packets++;
		MV_DEV_STAT(dev).tx_bytes += skb->len;
	} else {
		MV_DEV_STAT(dev).tx_dropped++;
		dev_kfree_skb_any(skb);
	}

#ifndef CONFIG_MV_ETH_TXDONE_ISR
	if (txq_ctrl) {
		if (txq_ctrl->txq_count >= mv_ctrl_txdone) {
			STAT_DIST(u32 tx_done =) mv_eth_txq_done(pp, txq_ctrl);

			STAT_DIST(if (tx_done < pp->dist_stats.tx_done_dist_size)
					pp->dist_stats.tx_done_dist[tx_done]++);
		}
		/* If after calling mv_eth_txq_done, txq_ctrl->txq_count is still > 0, we need to set the timer */
		if ((txq_ctrl->txq_count > 0) && (frags > 0))
			mv_eth_add_tx_done_timer(pp);
	}
#endif /* CONFIG_MV_ETH_TXDONE_ISR */

#if (CONFIG_MV_ETH_TXQ == 1)
	/* If the number of available Tx descriptors left is less than MAX_SKB_FRAGS, */
	/* stop the stack. If multiple queues are used, don't stop the stack just because one queue is full */
	if ((txq_ctrl) && ((txq_ctrl->txq_size - txq_ctrl->txq_count) <= MAX_SKB_FRAGS)) {
		netif_stop_queue(dev);
		/* printk(KERN_ERR "mv_eth_tx: calling netif_stop_queue() for %s\n", dev->name); */
		STAT_ERR(pp->stats.netif_stop++);
	}
#endif /* CONFIG_MV_ETH_TXQ == 1 */

	MV_ETH_UNLOCK(pp->lock, flags);

	return NETDEV_TX_OK;
}

#ifdef CONFIG_MV_ETH_TSO
/* Validate TSO */
static inline int mv_eth_tso_validate(struct sk_buff *skb, struct net_device *dev)
{
	if (!(dev->features & NETIF_F_TSO)) {
		printk("error: (skb_is_gso(skb) returns true but features is not NETIF_F_TSO\n");
		return 1;
	}

	if (skb_shinfo(skb)->frag_list != NULL) {
		printk("***** ERROR: frag_list is not null\n");
		return 1;
	}

	if (skb_shinfo(skb)->tso_segs == 1) {
		printk("***** ERROR: only one TSO segment\n");
		return 1;
	}

	if (skb->len <= skb_shinfo(skb)->tso_size) {
		printk("***** ERROR: total_len (%d) less than tso_size (%d)\n", skb->len, skb_shinfo(skb)->tso_size);
		return 1;
	}
	if( (htons(ETH_P_IP) != skb->protocol) || 
		(skb->nh.iph->protocol != IPPROTO_TCP) ||
		(tcp_hdr(skb) == NULL) )
	{
		printk("***** ERROR: Protocol is not TCP over IP\n");
		return 1;
	}
	return 0;
}

static inline void mv_eth_tso_build_hdr_desc(struct neta_tx_desc *tx_desc, struct eth_port *priv, struct sk_buff *skb,
					     struct tx_queue *txq_ctrl, int hdr_len, int size,
					     MV_U32 tcp_seq, MV_U16 ip_id, int left_len)
{
	struct iphdr *iph;
	struct tcphdr *tcph;
	MV_U8 *data;
	int mac_hdr_len = (skb->nh.raw - skb->data);

	tx_desc->command = mv_eth_tx_csum(priv, skb);
	tx_desc->command |= NETA_TX_F_DESC_MASK;
	tx_desc->dataSize = hdr_len;

	data = txq_ctrl->extradataArr[txq_ctrl->shadow_txq_put_i].data;
	memcpy(data, skb->data, hdr_len);

	iph = (struct iphdr *)(data + mac_hdr_len);

	if (iph) {
		iph->id = htons(ip_id);
		iph->tot_len = htons(size + hdr_len - mac_hdr_len);
	}

	tcph = (struct tcphdr *)(data + skb_transport_offset(skb));
	tcph->seq = htonl(tcp_seq);

	if (left_len) {
		/* Clear all special flags for not last packet */
		tcph->psh = 0;
		tcph->fin = 0;
		tcph->rst = 0;
	}

	tx_desc->bufPhysAddr = mvOsCacheFlush(NULL, data, tx_desc->dataSize);
	txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = 0;
	mv_eth_shadow_inc_put(txq_ctrl);

	mv_eth_tx_desc_flush(tx_desc);
}

static inline int mv_eth_tso_build_data_desc(struct neta_tx_desc *tx_desc, struct sk_buff *skb,
					     struct tx_queue *txq_ctrl, char *frag_ptr,
					     int frag_size, int data_left, int total_left)
{
	int size;

	size = MV_MIN(frag_size, data_left);

	tx_desc->dataSize = size;
	tx_desc->bufPhysAddr = mvOsCacheFlush(NULL, frag_ptr, size);
	tx_desc->command = 0;
	txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = 0;

	if (size == data_left) {
		/* last descriptor in the TCP packet */
		tx_desc->command = NETA_TX_L_DESC_MASK;

		if (total_left == 0) {
			/* last descriptor in SKB */
			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = ((MV_ULONG) skb | MV_ETH_SHADOW_SKB);
		}
	}
	mv_eth_shadow_inc_put(txq_ctrl);
	mv_eth_tx_desc_flush(tx_desc);

	return size;
}

/***********************************************************
 * mv_eth_tx_tso --                                        *
 *   send a packet.                                        *
 ***********************************************************/
struct tx_queue *mv_eth_tx_tso(struct sk_buff *skb, struct net_device *dev,
				struct  mv_eth_tx_spec *tx_spec, int *frags)
{
	int frag = 0;
	int total_len, hdr_len, size, frag_size, data_left;
	char *frag_ptr;
	int totalDescNum;
	struct neta_tx_desc *tx_desc;
	struct tx_queue *txq_ctrl;
	MV_U16 ip_id;
	MV_U32 tcp_seq = 0;
	skb_frag_t *skb_frag_ptr;
	const struct tcphdr *th = tcp_hdr(skb);
	struct eth_port *priv = MV_ETH_PRIV(dev);
	int i;

	STAT_DBG(priv->stats.tx_tso++);
	*frags = 0;
/*
	printk("mv_eth_tx_tso_%d ENTER: skb=%p, total_len=%d\n", priv->stats.tx_tso, skb, skb->len);
*/
	if (mv_eth_tso_validate(skb, dev))
		return NULL;

	txq_ctrl = &priv->txq_ctrl[tx_spec->txp * CONFIG_MV_ETH_TXQ + tx_spec->txq];
	if (txq_ctrl == NULL) {
		printk("%s: invalidate txp/txq (%d/%d)\n", __func__, tx_spec->txp, tx_spec->txq);
		return NULL;
	}

	/* Calculate expected number of TX descriptors */
	totalDescNum = skb_shinfo(skb)->tso_segs * 2 + skb_shinfo(skb)->nr_frags;

	if ((txq_ctrl->txq_count + totalDescNum) >= txq_ctrl->txq_size) {
/*
		printk("%s: no TX descriptors - txq_count=%d, len=%d, nr_frags=%d, tso_segs=%d\n",
					__func__, txq_ctrl->txq_count, skb->len, skb_shinfo(skb)->nr_frags,
				skb_shinfo(skb)->tso_segs);
*/
		STAT_DBG(txq_ctrl->stats.txq_err++);
		return txq_ctrl;
	}

	total_len = skb->len;
	hdr_len = (skb_transport_offset(skb) + tcp_hdrlen(skb));

	total_len -= hdr_len;
	ip_id = ntohs(skb->nh.iph->id);
	tcp_seq = ntohl(th->seq);

	frag_size = skb_headlen(skb);
	frag_ptr = skb->data;

	if (frag_size < hdr_len) {
		printk("***** ERROR: frag_size=%d, hdr_len=%d\n", frag_size, hdr_len);
		return txq_ctrl;
	}

	frag_size -= hdr_len;
	frag_ptr += hdr_len;
	if (frag_size == 0) {
		skb_frag_ptr = &skb_shinfo(skb)->frags[frag];

		/* Move to next segment */
		frag_size = skb_frag_ptr->size;
		frag_ptr = page_address(skb_frag_ptr->page) + skb_frag_ptr->page_offset;
		frag++;
	}
	totalDescNum = 0;

	while (total_len > 0) {
		data_left = MV_MIN(skb_shinfo(skb)->tso_size, total_len);

		tx_desc = mv_eth_tx_desc_get(txq_ctrl, 1);
		if (tx_desc == NULL)
			goto outNoTxDesc;

		totalDescNum++;
		total_len -= data_left;
		txq_ctrl->txq_count++;

		/* prepare packet headers: MAC + IP + TCP */
		mv_eth_tso_build_hdr_desc(tx_desc, priv, skb, txq_ctrl, hdr_len, data_left, tcp_seq, ip_id, total_len);
/*
		printk("Header desc: tx_desc=%p, skb=%p, hdr_len=%d, data_left=%d\n",
						tx_desc, skb, hdr_len, data_left);
*/
		ip_id++;

		while (data_left > 0) {
			tx_desc = mv_eth_tx_desc_get(txq_ctrl, 1);
			if (tx_desc == NULL)
				goto outNoTxDesc;

			totalDescNum++;
			txq_ctrl->txq_count++;

			size = mv_eth_tso_build_data_desc(tx_desc, skb, txq_ctrl, frag_ptr,
							  frag_size, data_left, total_len);

/*
			printk("Data desc: tx_desc=%p, skb=%p, size=%d, frag_size=%d, data_left=%d\n",
							tx_desc, skb, size, frag_size, data_left);
 */
			data_left -= size;
			tcp_seq += size;

			frag_size -= size;
			frag_ptr += size;

			if ((frag_size == 0) && (frag < skb_shinfo(skb)->nr_frags)) {
				skb_frag_ptr = &skb_shinfo(skb)->frags[frag];

				/* Move to next segment */
				frag_size = skb_frag_ptr->size;
				frag_ptr = page_address(skb_frag_ptr->page) + skb_frag_ptr->page_offset;
				frag++;
			}
		}		/* of while data_left > 0 */
	}			/* of while (total_len>0) */

	STAT_DBG(priv->stats.tx_tso_bytes += skb->len);

	STAT_DBG(txq_ctrl->stats.txq_tx += totalDescNum);
	*frags = totalDescNum;

	mvNetaTxqPendDescAdd(priv->port, tx_spec->txp, tx_spec->txq, totalDescNum);
/*
	printk("mv_eth_tx_tso EXIT: totalDescNum=%d\n", totalDescNum);
*/
	return txq_ctrl;

outNoTxDesc:
	/* No enough TX descriptors for the whole skb - rollback */
	printk("%s: No TX descriptors - rollback %d, txq_count=%d, nr_frags=%d, skb=%p, len=%d, tso_segs=%d\n",
			__func__, totalDescNum, txq_ctrl->txq_count, skb_shinfo(skb)->nr_frags,
			skb, skb->len, skb_shinfo(skb)->tso_segs);

	for (i = 0; i < totalDescNum; i++) {
		txq_ctrl->txq_count--;
		mv_eth_shadow_dec_put(txq_ctrl);
		mvNetaTxqPrevDescGet(txq_ctrl->q);
	}
	return txq_ctrl;
}
#endif /* CONFIG_MV_ETH_TSO */


static inline void mv_eth_txq_bufs_free(struct eth_port *pp, struct tx_queue *txq_ctrl, int num)
{
	u32 shadow;
	int i;

	/* Free buffers that was not freed automatically by BM */
	for (i = 0; i < num; i++) {
		shadow = txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_get_i];
		mv_eth_shadow_inc_get(txq_ctrl);

		if (!shadow)
			continue;
/*
		printk("tx_done: p=%d, txp=%d, txq=%d, writeIdx=%d, readIdx=%d, shadow=0x%x\n",
					pp->port, txq_ctrl->txp, txq_ctrl->txq, txq_ctrl->shadow_txq_put_i,
					txq_ctrl->shadow_txq_get_i, shadow);
*/
		if (shadow & MV_ETH_SHADOW_SKB) {
			shadow &= ~MV_ETH_SHADOW_SKB;
			dev_kfree_skb_any((struct sk_buff *)shadow);
			STAT_DBG(pp->stats.tx_skb_free++);
		} else {	/* packet from NFP without BM */
			struct eth_pbuf *pkt = (struct eth_pbuf *)shadow;
			struct bm_pool *pool = &mv_eth_pool[pkt->pool];

			mv_eth_pool_put(pool, pkt);
		}
	}
}

/* Drop packets received by the RXQ and free buffers */
static void mv_eth_rxq_drop_pkts(struct eth_port *pp, int rxq)
{
	struct neta_rx_desc *rx_desc;
	struct eth_pbuf     *pkt;
	struct bm_pool      *pool;
	int	                rx_done, i;
	MV_NETA_RXQ_CTRL    *rx_ctrl = pp->rxq_ctrl[rxq].q;

	if (rx_ctrl == NULL)
		return;

	rx_done = mvNetaRxqBusyDescNumGet(pp->port, rxq);

	for (i = 0; i < rx_done; i++) {
		rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
		mvOsCacheLineInv(NULL, rx_desc);

#if defined(MV_CPU_BE)
		mvNetaRxqDescSwap(rx_desc);
#endif /* MV_CPU_BE */

		pkt = (struct eth_pbuf *)rx_desc->bufCookie;
		pool = &mv_eth_pool[pkt->pool];
		mv_eth_rxq_refill(pp, rxq, pkt, pool, rx_desc);
	}
	if (rx_done)
		mvNetaRxqDescNumUpdate(pp->port, rxq, rx_done, rx_done);
}

static void mv_eth_txq_done_force(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
	int tx_done = txq_ctrl->txq_count;

	mv_eth_txq_bufs_free(pp, txq_ctrl, tx_done);

	STAT_DBG(txq_ctrl->stats.txq_txdone += tx_done);

	/* reset txq */
	txq_ctrl->txq_count = 0;
	txq_ctrl->shadow_txq_put_i = 0;
	txq_ctrl->shadow_txq_get_i = 0;
}


#if (CONFIG_MV_ETH_TXQ == 1)
static inline void mv_eth_netif_wake(struct net_device *dev)
{
	struct eth_port *pp = MV_ETH_PRIV(dev);

	/* if transmission was previously stopped, now it can be restarted */
	if (netif_queue_stopped(dev) && (dev->flags & IFF_UP)) {
		netif_wake_queue(dev);
		/* printk(KERN_ERR "mv_eth_txq_done: calling netif_wake_queue() for %s\n", dev->name); */
		STAT_ERR(pp->stats.netif_wake++);
	}
}
#endif /* CONFIG_MV_ETH_TXQ == 1 */


static inline u32 mv_eth_txq_done(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
	int tx_done;

	tx_done = mvNetaTxqSentDescProc(pp->port, txq_ctrl->txp, txq_ctrl->txq);
	if (!tx_done)
		return tx_done;
/*
	printk("tx_done: txq_count=%d, port=%d, txp=%d, txq=%d, tx_done=%d\n",
			txq_ctrl->txq_count, pp->port, txq_ctrl->txp, txq_ctrl->txq, tx_done);
*/
	if (!mv_eth_txq_bm(txq_ctrl))
		mv_eth_txq_bufs_free(pp, txq_ctrl, tx_done);

	txq_ctrl->txq_count -= tx_done;
	STAT_DBG(txq_ctrl->stats.txq_txdone += tx_done);

#if (CONFIG_MV_ETH_TXQ == 1)
	/* Note: can change the threshold to MAX_SKB_FRAGS */
	if (tx_done  > 0) {
#ifdef CONFIG_MV_ETH_SWITCH
		if (!(pp->flags & MV_ETH_F_SWITCH)) {
			mv_eth_netif_wake(pp->dev);
		} else {
			struct eth_port *priv;
			int i;

			for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) {
				if (mv_net_devs[i] == NULL)
					break;

				priv = MV_ETH_PRIV(mv_net_devs[i]);
				if (priv == NULL)
					break;

				if (priv->port == pp->port)
					mv_eth_netif_wake(mv_net_devs[i]);
			}
		}
#else
		mv_eth_netif_wake(pp->dev);
#endif /* CONFIG_MV_ETH_SWITCH */
	}

#endif /* CONFIG_MV_ETH_TXQ == 1 */

	return tx_done;
}


static inline u32 mv_eth_tx_done_pon(struct eth_port *pp, int *tx_todo)
{
	int txp, txq;
	struct tx_queue *txq_ctrl;
	u32 tx_done = 0;

	*tx_todo = 0;

	STAT_INFO(pp->stats.tx_done++);

	/* simply go over all TX ports and TX queues */
	txp = pp->txp_num;
	while (txp--) {
		txq = CONFIG_MV_ETH_TXQ;

		while (txq--) {
			txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
			if ((txq_ctrl) && (txq_ctrl->txq_count)) {
				tx_done += mv_eth_txq_done(pp, txq_ctrl);
				*tx_todo += txq_ctrl->txq_count;
			}
		}
	}

	STAT_DIST(if (tx_done < pp->dist_stats.tx_done_dist_size)
			pp->dist_stats.tx_done_dist[tx_done]++);

	return tx_done;
}


static inline u32 mv_eth_tx_done_gbe(struct eth_port *pp, u32 cause_tx_done, int *tx_todo)
{
	int txp, txq;
	struct tx_queue *txq_ctrl;
	u32 tx_done = 0;

	*tx_todo = 0;

	STAT_INFO(pp->stats.tx_done++);

	while (cause_tx_done != 0) {
		/* For GbE ports we get TX Buffers Threshold Cross per queue in bits [7:0] */
		txp = pp->txp_num; /* 1 for GbE ports */
		while (txp--) {
			txq = mv_eth_tx_done_policy(cause_tx_done);
			if (txq == -1)
				break;

			txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
			if ((txq_ctrl) && (txq_ctrl->txq_count)) {
				tx_done += mv_eth_txq_done(pp, txq_ctrl);
				*tx_todo += txq_ctrl->txq_count;
			}			

			cause_tx_done &= ~((1 << txq) << NETA_CAUSE_TXQ_SENT_DESC_OFFS);
		}
	}

	STAT_DIST(if (tx_done < pp->dist_stats.tx_done_dist_size)
			pp->dist_stats.tx_done_dist[tx_done]++);

	return tx_done;
}

static inline MV_U32 mv_eth_tx_csum(struct eth_port *pp, struct sk_buff *skb)
{
#ifdef CONFIG_MV_ETH_TX_CSUM_OFFLOAD
	if (skb->ip_summed == CHECKSUM_HW) {
		MV_U8 l4_proto;
		MV_U32 command;

		/* fields: L3_offset, IP_hdrlen, L3_type, G_IPv4_chk, G_L4_chk, L4_type */
		/* only if checksum calculation required */
		command =  ((skb->nh.raw - skb->data) << NETA_TX_L3_OFFSET_OFFS);

		if (skb->protocol == htons(ETH_P_IP)) {
			struct iphdr *ip4h = skb->nh.iph;  

			/* Calculate IPv4 checksum and L4 checksum */
			command |= (ip4h->ihl << NETA_TX_IP_HLEN_OFFS) | NETA_TX_L3_IP4 | NETA_TX_IP_CSUM_MASK;
			l4_proto = ip4h->protocol;
		} else {
			/* If not IPv4 - must be ETH_P_IPV6 - Calculate only L4 checksum */
			struct ipv6hdr *ip6h = skb->nh.ipv6h;

			command |= NETA_TX_L3_IP6;
			if ((skb->h.raw - skb->nh.raw) > 0)
				command |= ( ((skb->h.raw - skb->nh.raw) >> 2) << NETA_TX_IP_HLEN_OFFS);
			/* Read l4_protocol from one of IPv6 extra headers ?????? */
			l4_proto = ip6h->nexthdr;
		}
		if (l4_proto == IPPROTO_TCP)
			command |= (NETA_TX_L4_TCP | NETA_TX_L4_CSUM_FULL);
		else if (l4_proto == IPPROTO_UDP)
			command |= (NETA_TX_L4_UDP | NETA_TX_L4_CSUM_FULL);
		else
			command |= NETA_TX_L4_CSUM_NOT;

		STAT_DBG(pp->stats.tx_csum_hw++);
		return command;
	}
#endif /* CONFIG_MV_ETH_TX_CSUM_OFFLOAD */

	STAT_DBG(pp->stats.tx_csum_sw++);
	return NETA_TX_L4_CSUM_NOT;
}

static void mv_eth_tx_frag_process(struct eth_port *pp, struct sk_buff *skb, struct tx_queue *txq_ctrl,	u16 flags)
{
	int i;
	struct neta_tx_desc *tx_desc;

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		tx_desc = mvNetaTxqNextDescGet(txq_ctrl->q);

		/* NETA_TX_BM_ENABLE_MASK = 0 */
		/* NETA_TX_PKT_OFFSET_MASK = 0 */
		tx_desc->dataSize = frag->size;
		tx_desc->bufPhysAddr = mvOsCacheFlush(NULL, page_address(frag->page) + frag->page_offset,
						      tx_desc->dataSize);

		if (i == (skb_shinfo(skb)->nr_frags - 1)) {
			/* Last descriptor */
			if (flags & MV_ETH_F_NO_PAD)
				tx_desc->command = NETA_TX_L_DESC_MASK;
			else
				tx_desc->command = (NETA_TX_L_DESC_MASK | NETA_TX_Z_PAD_MASK);

			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = ((MV_ULONG) skb | MV_ETH_SHADOW_SKB);
			mv_eth_shadow_inc_put(txq_ctrl);
		} else {
			/* Descriptor in the middle: Not First, Not Last */
			tx_desc->command = 0;

			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = 0;
			mv_eth_shadow_inc_put(txq_ctrl);
		}

		mv_eth_tx_desc_flush(tx_desc);
	}
}


/* Free "num" buffers from the pool */
static int mv_eth_pool_free(int pool, int num)
{
	struct eth_pbuf *pkt;
	int i = 0;
	struct bm_pool *ppool = &mv_eth_pool[pool];
#ifdef CONFIG_MV_ETH_BM
	u32 reg_val;
#endif
	unsigned long flags = 0;

	MV_ETH_LOCK(&ppool->lock, flags);

	if (num >= ppool->buf_num) {
		num = ppool->buf_num;

#ifdef CONFIG_MV_ETH_BM
		reg_val = MV_REG_READ(MV_BM_CONFIG_REG);
		reg_val |= MV_BM_EMPTY_LIMIT_MASK;
		MV_REG_WRITE(MV_BM_CONFIG_REG, reg_val);
#endif /* CONFIG_MV_ETH_BM */
	}

	while (i < num) {
		pkt = NULL;

		if (mv_eth_pool_bm(ppool)) {
#ifdef CONFIG_MV_ETH_BM
			MV_U32 *va;
			MV_U32 pa = mvBmPoolGet(pool);

			if (pa != 0) {
				va = phys_to_virt(pa);
				pkt = (struct eth_pbuf *)*va;
/*
			printk("mv_eth_pool_free_%d: pool=%d, pkt=%p, head=%p (%x)\n",
				i, pool, pkt, va, pa);
*/
			}
#endif /* CONFIG_MV_ETH_BM */
		} else {
			if (mvStackIndex(ppool->stack) > 0)
				pkt = (struct eth_pbuf *)mvStackPop(ppool->stack);
		}
		if (pkt) {
			struct sk_buff *skb = (struct sk_buff *)pkt->osInfo;

#ifdef CONFIG_NET_SKB_RECYCLE
			skb->skb_recycle = NULL;
			skb->hw_cookie = NULL;
#endif /* CONFIG_NET_SKB_RECYCLE */

			i++;
			dev_kfree_skb_any(skb);
			mvOsFree(pkt);
		} else
			break;

	}
	printk("pool #%d: pkt_size=%d, buf_size=%d - %d of %d buffers free\n",
	       pool, ppool->pkt_size, RX_BUF_SIZE(ppool->pkt_size), i, num);

	ppool->buf_num -= num;

#ifdef CONFIG_MV_ETH_BM
	reg_val = MV_REG_READ(MV_BM_CONFIG_REG);
	reg_val &= ~MV_BM_EMPTY_LIMIT_MASK;
	MV_REG_WRITE(MV_BM_CONFIG_REG, reg_val);
#endif /* CONFIG_MV_ETH_BM */

	MV_ETH_UNLOCK(&ppool->lock, flags);

	return i;
}


static int mv_eth_pool_destroy(int pool)
{
	int num, status = 0;
	struct bm_pool *ppool = &mv_eth_pool[pool];
	
	num = mv_eth_pool_free(pool, ppool->buf_num);
	if (num != ppool->buf_num) {
		printk("Warning: could not free all buffers in pool %d while destroying pool\n", pool);
		return MV_ERROR;
	}

	status = mvStackDelete(ppool->stack);

#ifdef CONFIG_MV_ETH_BM
	mvBmPoolDisable(pool);

	/* Note: we don't free the bm_pool here ! */
	if (ppool->bm_pool)
		mvOsFree(ppool->bm_pool);
#endif

	memset(ppool, 0, sizeof(struct bm_pool));

	return status;
}


static int mv_eth_pool_add(int pool, int buf_num)
{
	struct bm_pool *bm_pool;
	struct sk_buff *skb;
	struct eth_pbuf *pkt;
	int i;
	unsigned long flags = 0;

	if ((pool < 0) || (pool >= MV_ETH_BM_POOLS)) {
		printk("%s: invalid pool number %d\n", __func__, pool);
		return 0;
	}

	bm_pool = &mv_eth_pool[pool];

	/* Check buffer size */
	if (bm_pool->pkt_size == 0) {
		printk("%s: invalid pool #%d state: pkt_size=%d, buf_size=%d, buf_num=%d\n",
		       __func__, pool, bm_pool->pkt_size, RX_BUF_SIZE(bm_pool->pkt_size), bm_pool->buf_num);
		return 0;
	}

	/* Insure buf_num is smaller than capacity */
	if ((buf_num < 0) || ((buf_num + bm_pool->buf_num) > (bm_pool->capacity))) {

		printk("%s: can't add %d buffers into bm_pool=%d: capacity=%d, buf_num=%d\n",
		       __func__, buf_num, pool, bm_pool->capacity, bm_pool->buf_num);
		return 0;
	}

	MV_ETH_LOCK(&bm_pool->lock, flags);

	for (i = 0; i < buf_num; i++) {
		pkt = mvOsMalloc(sizeof(struct eth_pbuf));
		if (!pkt) {
			printk("%s: can't allocate %d bytes\n", __func__, sizeof(struct eth_pbuf));
			break;
		}

		skb = mv_eth_skb_alloc(bm_pool, pkt);
		if (!skb) {
			kfree(pkt);
			break;
		}
/*
	printk("skb_alloc_%d: pool=%d, skb=%p, pkt=%p, head=%p (%lx), skb->truesize=%d\n",
				i, bm_pool->pool, skb, pkt, pkt->pBuf, pkt->physAddr, skb->truesize);
*/

#ifdef CONFIG_MV_ETH_BM
		mvBmPoolPut(pool, (MV_ULONG) pkt->physAddr);
		STAT_DBG(bm_pool->stats.bm_put++);
#else
		mvStackPush(bm_pool->stack, (MV_U32) pkt);
		STAT_DBG(bm_pool->stats.stack_put++);
#endif
	}
	bm_pool->buf_num += i;

	printk("pool #%d: pkt_size=%d, buf_size=%d - %d of %d buffers added\n",
	       pool, bm_pool->pkt_size, RX_BUF_SIZE(bm_pool->pkt_size), i, buf_num);

	MV_ETH_UNLOCK(&bm_pool->lock, flags);

	return i;
}

static MV_STATUS mv_eth_pool_create(int pool, int capacity)
{
	struct bm_pool *bm_pool;

	if ((pool < 0) || (pool >= MV_ETH_BM_POOLS)) {
		printk("%s: pool=%d is out of range\n", __func__, pool);
		return MV_BAD_VALUE;
	}

	bm_pool = &mv_eth_pool[pool];
	memset(bm_pool, 0, sizeof(struct bm_pool));

#ifdef CONFIG_MV_ETH_BM
	{
		MV_ULONG pool_phys_addr;
		MV_UNIT_WIN_INFO winInfo;
		MV_STATUS status;

		/* Allocate physical memory for BM pool */
		bm_pool->bm_pool = mvOsMalloc(capacity * sizeof(MV_U32));

		if (bm_pool->bm_pool == NULL) {
			printk("Can't allocate %d bytes for bm_pool #%d\n", capacity * sizeof(MV_U32), pool);
			return MV_OUT_OF_CPU_MEM;
		}
		/* Pool address must be MV_BM_POOL_PTR_ALIGN bytes aligned */
		if (MV_IS_NOT_ALIGN((unsigned)bm_pool->bm_pool, MV_BM_POOL_PTR_ALIGN)) {
			printk("memory allocate for bm_pool #%d is not %d bytes aligned\n", pool, MV_BM_POOL_PTR_ALIGN);
			return MV_NOT_ALIGNED;
		}
		pool_phys_addr = mvOsCacheInvalidate(NULL, bm_pool->bm_pool, capacity * sizeof(MV_U32));
		status = mvBmPoolInit(pool, pool_phys_addr, capacity);
		if (status != MV_OK) {
			printk("%s: Can't init #%d BM pool. status=%d\n", __func__, pool, status);
			return status;
		}
		status = mvCtrlAddrWinInfoGet(&winInfo, pool_phys_addr);
		if (status != MV_OK) {
			printk("%s: Can't map BM pool #%d. phys_addr=0x%x, status=%d\n",
			       __func__, pool, (unsigned)pool_phys_addr, status);
			return status;
		}
		mvBmPoolTargetSet(pool, winInfo.targetId, winInfo.attrib);
		mvBmPoolEnable(pool);
	}
#endif /* CONFIG_MV_ETH_BM */

	/* Create Stack as container of alloacted skbs for SKB_RECYCLE and for RXQs working without BM support */
	bm_pool->stack = mvStackCreate(capacity);

	if (bm_pool->stack == NULL) {
		printk("Can't create MV_STACK structure for %d elements\n", capacity);
		return MV_OUT_OF_CPU_MEM;
	}

	bm_pool->pool = pool;
	bm_pool->capacity = capacity;
	bm_pool->pkt_size = 0;
	bm_pool->buf_num = 0;
	spin_lock_init(&bm_pool->lock);

	return MV_OK;
}

/* Interrupt handling */
irqreturn_t mv_eth_isr(int irq, void *dev_id)
{
	struct eth_port *pp = (struct eth_port *)dev_id;

#ifdef CONFIG_MV_ETH_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_ISR) {
		printk("%s: port=%d, cpu=%d, mask=0x%x, cause=0x%x\n",
			__func__, pp->port, smp_processor_id(),
			MV_REG_READ(NETA_INTR_NEW_MASK_REG(pp->port)), MV_REG_READ(NETA_INTR_NEW_CAUSE_REG(pp->port)));
	}
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

	STAT_INFO(pp->stats.irq++);

	/* Mask all interrupts */
	MV_REG_WRITE(NETA_INTR_NEW_MASK_REG(pp->port), 0);
	/* To be sure that itterrupt already masked Dummy read is required */
	/* MV_REG_READ(NETA_INTR_NEW_MASK_REG(pp->port));*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	if (netif_rx_schedule_prep(pp->dev)) {
		__netif_rx_schedule(pp->dev);
	}
#else
	/* Verify that the device not already on the polling list */
	if (napi_schedule_prep(&pp->napi)) {
		/* schedule the work (rx+txdone+link) out of interrupt contxet */
		__napi_schedule(&pp->napi);
	}
#endif
	else {
		STAT_INFO(pp->stats.irq_err++);
#ifdef CONFIG_MV_ETH_STAT_INF
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		if (netif_running(pp->dev))
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24) */
			printk("mv_eth_isr ERROR: %d, dev = %s\n", pp->stats.irq_err, pp->dev->name);
#endif
	}
	return IRQ_HANDLED;
}

static void mv_eth_link_event(struct eth_port *pp, int print)
{
	struct net_device *dev = pp->dev;

	STAT_INFO(pp->stats.link++);

	/* Check Link status on ethernet port */
	if (mvNetaLinkIsUp(pp->port)) {
		mvNetaPortUp(pp->port);
		pp->flags |= MV_ETH_F_LINK_UP;

		if (mv_eth_ctrl_is_tx_enabled(pp)) {
			if (dev) {
				netif_carrier_on(dev);
				netif_wake_queue(dev);
			}
		}
	} else {
		if (dev) {
			netif_carrier_off(dev);
			netif_stop_queue(dev);
		}
		mvNetaPortDown(pp->port);
		pp->flags &= ~MV_ETH_F_LINK_UP;
	}

	if (print) {
		if (dev)
			printk("%s: ", dev->name);
		else
			printk("%s: ", "none");

		mv_eth_link_status_print(pp->port);
	}

	aei_led_set(pp->port);
}

/***********************************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
static int mv_eth_poll(struct net_device *dev, int *in_budget)
#else
static int mv_eth_poll(struct napi_struct *napi, int budget)
#endif
{
	int rx_done = 0, tx_todo = 0;
	MV_U32 causeRxTx;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	struct eth_port	*pp = MV_ETH_PRIV(dev);
	int budget = *in_budget;
#else
	struct net_device*  dev;
	struct eth_port     *pp = container_of(napi, struct eth_port, napi);

	dev = pp->dev;
#endif

#ifdef CONFIG_MV_ETH_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_POLL) {
		printk("%s_%d ENTER: port=%d, cpu=%d, mask=0x%x, cause=0x%x\n",
			__func__, pp->stats.poll, pp->port, smp_processor_id(),
			MV_REG_READ(NETA_INTR_NEW_MASK_REG(pp->port)), MV_REG_READ(NETA_INTR_NEW_CAUSE_REG(pp->port)));
	}
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

	STAT_INFO(pp->stats.poll++);

	/* Read cause register */
	causeRxTx = MV_REG_READ(NETA_INTR_NEW_CAUSE_REG(pp->port)) &
	    (MV_ETH_MISC_SUM_INTR_MASK | MV_ETH_TXDONE_INTR_MASK | MV_ETH_RX_INTR_MASK);

	if (causeRxTx & MV_ETH_MISC_SUM_INTR_MASK) {
		MV_U32 causeMisc;

		/* Process MISC events - Link, etc ??? */
		causeRxTx &= ~MV_ETH_MISC_SUM_INTR_MASK;
		causeMisc = MV_REG_READ(NETA_INTR_MISC_CAUSE_REG(pp->port));

		if (causeMisc & NETA_CAUSE_LINK_CHANGE_MASK) {
			mv_eth_link_event(pp, 1);
		}

		MV_REG_WRITE(NETA_INTR_MISC_CAUSE_REG(pp->port), 0);
	}

#ifdef CONFIG_MV_ETH_TXDONE_ISR
	if (causeRxTx & MV_ETH_TXDONE_INTR_MASK) {
		/* TX_DONE process */
		spin_lock(pp->lock);

		if (MV_PON_PORT(pp->port))
			mv_eth_tx_done_pon(pp, &tx_todo);
		else
			mv_eth_tx_done_gbe(pp, (causeRxTx & MV_ETH_TXDONE_INTR_MASK), &tx_todo);

		spin_unlock(pp->lock);

		causeRxTx &= ~MV_ETH_TXDONE_INTR_MASK;
	}
#endif /* CONFIG_MV_ETH_TXDONE_ISR */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	budget = min(budget, dev->quota);
#endif
#if (CONFIG_MV_ETH_RXQ > 1)
	while ((causeRxTx != 0) && (budget > 0)) {
		int count, rx_queue;

		rx_queue = mv_eth_rx_policy(causeRxTx);
		if (rx_queue == -1)
			break;

		causeRxTx &= ~((1 << rx_queue) << NETA_CAUSE_RXQ_OCCUP_DESC_OFFS);

		count = mv_eth_rx(pp, budget, rx_queue);
		rx_done += count;
		budget -= count;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		dev->quota -= count;
#endif
	}
#else
	rx_done = mv_eth_rx(pp, budget, CONFIG_MV_ETH_RXQ_DEF);
	budget -= rx_done;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	dev->quota -= rx_done;
#endif
#endif /* (CONFIG_MV_ETH_RXQ > 1) */

	STAT_DIST(if (rx_done < pp->dist_stats.rx_dist_size)
			pp->dist_stats.rx_dist[rx_done]++);

#ifdef CONFIG_MV_ETH_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_POLL) {
		printk("%s_%d  EXIT: port=%d, cpu=%d, budget=%d, rx_done=%d\n",
			__func__, pp->stats.poll, pp->port, smp_processor_id(), budget, rx_done);
	}
#endif /* CONFIG_MV_ETH_DEBUG_CODE */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	if (!netif_running(dev) || budget > 0) 
#else
	if (budget > 0)
#endif
	{
		unsigned long flags;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		netif_rx_complete(dev);
#else
		napi_complete(&pp->napi);
#endif
		STAT_INFO(pp->stats.poll_exit++);

		local_irq_save(flags);
		MV_REG_WRITE(NETA_INTR_NEW_MASK_REG(pp->port),
			     (MV_ETH_MISC_SUM_INTR_MASK | MV_ETH_TXDONE_INTR_MASK | MV_ETH_RX_INTR_MASK));

		local_irq_restore(flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		*in_budget = budget;
		return 0;
#else
		return rx_done;
#endif
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	*in_budget = budget;
	return 1;
#else
	return rx_done;
#endif
}

static void mv_eth_cpu_counters_init(void)
{
#ifdef CONFIG_MV_CPU_PERF_CNTRS

	mvCpuCntrsInitialize();

#ifdef CONFIG_PLAT_ARMADA
	/*  cycles counter via special CCNT counter */
	mvCpuCntrsProgram(0, MV_CPU_CNTRS_CYCLES, "Cycles", 13);

	/* instruction counters */
	mvCpuCntrsProgram(1, MV_CPU_CNTRS_INSTRUCTIONS, "Instr", 13);
	/* mvCpuCntrsProgram(0, MV_CPU_CNTRS_DCACHE_READ_HIT, "DcRdHit", 0); */

	/* ICache misses counter */
	mvCpuCntrsProgram(2, MV_CPU_CNTRS_ICACHE_READ_MISS, "IcMiss", 0);

	/* DCache read misses counter */
	mvCpuCntrsProgram(3, MV_CPU_CNTRS_DCACHE_READ_MISS, "DcRdMiss", 0);

	/* DCache write misses counter */
	mvCpuCntrsProgram(4, MV_CPU_CNTRS_DCACHE_WRITE_MISS, "DcWrMiss", 0);

	/* DTLB Miss counter */
	mvCpuCntrsProgram(5, MV_CPU_CNTRS_DTLB_MISS, "dTlbMiss", 0);

	/* mvCpuCntrsProgram(3, MV_CPU_CNTRS_TLB_MISS, "TlbMiss", 0); */
#else /* CONFIG_FEROCEON */
	/* 0 - instruction counters */
	mvCpuCntrsProgram(0, MV_CPU_CNTRS_INSTRUCTIONS, "Instr", 16);
	/* mvCpuCntrsProgram(0, MV_CPU_CNTRS_DCACHE_READ_HIT, "DcRdHit", 0); */

	/* 1 - ICache misses counter */
	mvCpuCntrsProgram(1, MV_CPU_CNTRS_ICACHE_READ_MISS, "IcMiss", 0);

	/* 2 - cycles counter */
	mvCpuCntrsProgram(2, MV_CPU_CNTRS_CYCLES, "Cycles", 18);

	/* 3 - DCache read misses counter */
	mvCpuCntrsProgram(3, MV_CPU_CNTRS_DCACHE_READ_MISS, "DcRdMiss", 0);
	/* mvCpuCntrsProgram(3, MV_CPU_CNTRS_TLB_MISS, "TlbMiss", 0); */
#endif /* CONFIG_PLAT_ARMADA */

	event0 = mvCpuCntrsEventCreate("RX_DESC_PREF", 100000);
	event1 = mvCpuCntrsEventCreate("RX_DESC_READ", 100000);
	event2 = mvCpuCntrsEventCreate("RX_BUF_INV", 100000);
	event3 = mvCpuCntrsEventCreate("RX_DESC_FILL", 100000);
	event4 = mvCpuCntrsEventCreate("TX_START", 100000);
	event5 = mvCpuCntrsEventCreate("RX_BUF_INV", 100000);
	if ((event0 == NULL) || (event1 == NULL) || (event2 == NULL) ||
		(event3 == NULL) || (event4 == NULL) || (event5 == NULL))
		printk("Can't create cpu counter events\n");
#endif /* CONFIG_MV_CPU_PERF_CNTRS */
}

#ifdef CONFIG_MV_ETH_NFP
static MV_STATUS mv_eth_register_nfp_devices(void)
{
	int i;
	MV_STATUS status = MV_OK;
	struct eth_port *curr_pp;

	/* Register all network devices mapped to NETA as internal for NFP */
	for (i = 0; i < mv_net_devs_num; i++) {
		if (mv_net_devs[i] != NULL) {
			curr_pp = MV_ETH_PRIV(mv_net_devs[i]);
			if (curr_pp->flags & MV_ETH_F_CONNECT_LINUX) {
				status = nfp_mgr_if_register(mv_net_devs[i]->ifindex,
								MV_NFP_IF_INT,
								mv_net_devs[i],	curr_pp);
				if (status != MV_OK) {
					printk("fp_mgr_if_register failed\n");
					return MV_ERROR;
				}
			}
		}
	}
	return MV_OK;
}


static MV_STATUS mv_eth_unregister_nfp_devices(void)
{
	nfp_eth_dev_db_clear();

	return MV_OK;
}
#endif /* CONFIG_MV_ETH_NFP */


static void mv_eth_port_promisc_set(int port, int queue)
{
#ifdef CONFIG_MV_ETH_PNC_PARSER
	/* Accept all */
	pnc_mac_me(port, NULL, queue);
	pnc_mcast_all(port, 1);
#endif /* CONFIG_MV_ETH_PNC_PARSER */

#ifdef CONFIG_MV_ETH_LEGACY_PARSER
	mvNetaRxUnicastPromiscSet(port, MV_TRUE);
	mvNetaSetUcastTable(port, queue);
	mvNetaSetSpecialMcastTable(port, queue);
	mvNetaSetOtherMcastTable(port, queue);
#endif /* CONFIG_MV_ETH_LEGACY_PARSER */
}

static void mv_eth_port_filtering_cleanup(int port)
{
#ifdef CONFIG_MV_ETH_PNC_PARSER
	if (port == 0)
		tcam_hw_init(); /* clean TCAM only one, no need to do this per port. Assume this function is called with port 0 */
#endif /* CONFIG_MV_ETH_PNC_PARSER */

#ifdef CONFIG_MV_ETH_LEGACY_PARSER
	mvNetaRxUnicastPromiscSet(port, MV_FALSE);
	mvNetaSetUcastTable(port, -1);
	mvNetaSetSpecialMcastTable(port, -1);
	mvNetaSetOtherMcastTable(port, -1);
#endif /* CONFIG_MV_ETH_LEGACY_PARSER */
}


static MV_STATUS mv_eth_bm_pools_init(void)
{
	int i, j;
	MV_STATUS status;

	/* Create all pools with maximum capacity */
	for (i = 0; i < MV_ETH_BM_POOLS; i++) {
		status = mv_eth_pool_create(i, MV_BM_POOL_CAP_MAX);
		if (status != MV_OK) {
			printk("%s: can't create bm_pool=%d - capacity=%d\n", __func__, i, MV_BM_POOL_CAP_MAX);
			for (j = 0; j < i; j++)
				mv_eth_pool_destroy(j);
			return status;
		}
	}
#ifdef CONFIG_MV_ETH_BM
	mvBmControl(MV_START);
	mv_eth_pool[MV_ETH_SHORT_BM_POOL].pkt_size = CONFIG_MV_ETH_SHORT_PKT_SIZE;

#ifdef CONFIG_MV_ETH_POOL_PREDEFINED
	/* use predefined pools */
	mv_eth_pool[0].pkt_size = RX_PKT_SIZE(CONFIG_MV_ETH_POOL_0_MTU);
	mv_eth_pool[1].pkt_size = RX_PKT_SIZE(CONFIG_MV_ETH_POOL_1_MTU);
	mv_eth_pool[2].pkt_size = RX_PKT_SIZE(CONFIG_MV_ETH_POOL_2_MTU);
#endif /* CONFIG_MV_ETH_POOL_PREDEFINED */

#endif /* CONFIG_MV_ETH_BM */

	return 0;
}

/* Note: call this function only after mv_eth_ports_num is initialized */
static int mv_eth_load_network_interfaces(void)
{
	u32 port, dev_i = 0;
	struct eth_port *pp;
	int mtu;
	u8 mac[MV_MAC_ADDR_SIZE];
#ifdef CONFIG_MV_ETH_SWITCH
	int i;
	MV_STATUS status;
#endif /* CONFIG_MV_ETH_SWITCH */

	printk("  o Loading network interface(s)\n");

	for (port = 0; port < mv_eth_ports_num; port++) {
		if (!mvCtrlPwrClckGet(ETH_GIG_UNIT_ID, port)) {
			printk("\n  o Warning: GbE port %d is powered off\n\n", port);
			continue;
		}
		if (!MV_PON_PORT(port) && !mvBoardIsGbEPortConnected(port)) {
			printk
			    ("\n  o Warning: GbE port %d is not connected to PHY/RGMII/Switch, skipping initialization\n\n",
			     port);
			continue;
		}

		pp = mv_eth_ports[port] = mvOsMalloc(sizeof(struct eth_port));
		if (!pp) {
			printk("Error: failed to allocate memory for port %d\n", port);
			return -ENOMEM;
		}

		mv_eth_priv_init(pp, port);

#ifdef CONFIG_MV_ETH_PMT
		if (MV_PON_PORT(port))
			mvNetaPmtInit(port, (MV_NETA_PMT *)ioremap(PMT_PON_PHYS_BASE, PMT_MEM_SIZE));
		else
			mvNetaPmtInit(port, (MV_NETA_PMT *)ioremap(PMT_GIGA_PHYS_BASE + port * 0x40000, PMT_MEM_SIZE));
#endif /* CONFIG_MV_ETH_PMT */

#ifdef CONFIG_MV_ETH_SWITCH
		if (pp->flags & (MV_ETH_F_SWITCH | MV_ETH_F_EXT_SWITCH)) {
			status = mv_eth_switch_config_get(mv_eth_initialized);

			if (status < 0) {
				printk("\nWarning: port %d - Invalid netconfig string\n", port);
				mv_eth_priv_cleanup(pp);
				continue;
			} else if (status == 0) {	/* User selected to work with Gateway driver    */
				pp->flags &= ~MV_ETH_F_EXT_SWITCH;
			} else if (status == 1) {
				/* User selected to work without Gateway driver */
				pp->flags &= ~MV_ETH_F_SWITCH;
				printk("  o Working in External Switch mode\n");
				g_external_switch_mode = 1;
				mv_switch_link_detection_init();
			}
		}

		if (pp->flags & MV_ETH_F_SWITCH) {
			pp->flags |= MV_ETH_F_MH;
			mtu = switch_net_config.mtu;
			if (mv_switch_init(RX_PKT_SIZE(mtu), SWITCH_CONNECTED_PORTS_MASK)) {
				printk("\nWarning: port %d - Switch initialization failed\n", port);
				mv_eth_priv_cleanup(pp);
				continue;
			}
		} else
#endif /* CONFIG_MV_ETH_SWITCH */
			mtu = mv_eth_config_get(pp, mac);

		printk("\t%s p=%d: mtu=%d, mac=%p\n", MV_PON_PORT(port) ? "pon" : "giga", port, mtu, mac);

		if (mv_eth_hal_init(pp)) {
			printk("%s: can't init eth hal\n", __func__);
			mv_eth_priv_cleanup(pp);
			return -EIO;
		}
#ifdef CONFIG_MV_ETH_SWITCH
		if (pp->flags & MV_ETH_F_SWITCH) {
			int queue = CONFIG_MV_ETH_RXQ_DEF;

			mv_eth_switch_netdev_first = dev_i;
			dev_i = mv_eth_switch_netdev_init(pp, dev_i);
			if (dev_i < (mv_eth_switch_netdev_first + switch_net_config.netdev_max)) {
				printk("%s: can't create netdevice for switch\n", __func__);
				mv_eth_priv_cleanup(pp);
				return -EIO;
			}
			mv_eth_switch_netdev_last = dev_i - 1;

			/* set this port to be in promiscuous mode. MAC filtering is performed by the Switch */
			mv_eth_port_promisc_set(pp->port, queue);

			continue;
		}
#endif /* CONFIG_MV_ETH_SWITCH */

		mv_net_devs[dev_i] = mv_eth_netdev_init(pp, mtu, mac);
		if (!mv_net_devs[dev_i]) {
			printk("%s: can't create netdevice\n", __func__);
			mv_eth_priv_cleanup(pp);
			return -EIO;
		}
		pp->dev = mv_net_devs[dev_i];
		dev_i++;
	}

	mv_net_devs_num = dev_i;

	return 0;
}

#ifdef WAN_SWAP_FEATURE
/* Note: call this function only after mv_eth_ports_num is initialized */
static int mv_eth_reload_network_interfaces(void)
{
	u32 port;
	struct eth_port *pp;
#ifdef CONFIG_MV_ETH_SWITCH
	u32 dev_i = 0;
	int i, mtu;
	MV_STATUS status;
#endif /* CONFIG_MV_ETH_SWITCH */

	printk("  o Loading network interface(s)\n");

	for (port = 0; port < mv_eth_ports_num; port++) {
		if (!mvCtrlPwrClckGet(ETH_GIG_UNIT_ID, port)) {
			printk("\n  o Warning: GbE port %d is powered off\n\n", port);
			continue;
		}
		if (!MV_PON_PORT(port) && !mvBoardIsGbEPortConnected(port)) {
			printk
			    ("\n  o Warning: GbE port %d is not connected to PHY/RGMII/Switch, skipping initialization\n\n",
			     port);
			continue;
		}

		pp = mv_eth_ports[port] = mvOsMalloc(sizeof(struct eth_port));
		if (!pp) {
			printk("Error: failed to allocate memory for port %d\n", port);
			return -ENOMEM;
		}

		mv_eth_priv_init(pp, port);

#ifdef CONFIG_MV_ETH_SWITCH
		if (pp->flags & (MV_ETH_F_SWITCH | MV_ETH_F_EXT_SWITCH)) {
			status = mv_eth_switch_config_get(mv_eth_initialized);

			if (status < 0) {
				printk("\nWarning: port %d - Invalid netconfig string\n", port);
				mv_eth_priv_cleanup(pp);
				continue;
			} else if (status == 0) {	/* User selected to work with Gateway driver    */
				pp->flags &= ~MV_ETH_F_EXT_SWITCH;
			} else if (status == 1) {
				/* User selected to work without Gateway driver */
				pp->flags &= ~MV_ETH_F_SWITCH;
				printk("  o Working in External Switch mode\n");
				g_external_switch_mode = 1;
				mv_switch_link_detection_init();
			}
		}

		if (pp->flags & MV_ETH_F_SWITCH) {
			pp->flags |= MV_ETH_F_MH;
			mtu = switch_net_config.mtu;
			if (mv_switch_init(RX_PKT_SIZE(mtu), SWITCH_CONNECTED_PORTS_MASK)) {
				printk("\nWarning: port %d - Switch initialization failed\n", port);
				mv_eth_priv_cleanup(pp);
				continue;
			}
		}
#endif /* CONFIG_MV_ETH_SWITCH */

		if (mv_eth_hal_init(pp)) {
			printk("%s: can't init eth hal\n", __func__);
			mv_eth_priv_cleanup(pp);
			return -EIO;
		}
#ifdef CONFIG_MV_ETH_SWITCH
		if (pp->flags & MV_ETH_F_SWITCH) {
			int queue = CONFIG_MV_ETH_RXQ_DEF;

			for (i = mv_eth_switch_netdev_first; i <= mv_eth_switch_netdev_last; i++) {
				if (mv_net_devs[i] != NULL)
					mv_eth_netdev_update(i, pp);
			}

			/* set this port to be in promiscuous mode. MAC filtering is performed by the Switch */
			mv_eth_port_promisc_set(pp->port, queue);

			continue;
		}
#endif /* CONFIG_MV_ETH_SWITCH */

#ifdef CONFIG_MV_ETH_SWITCH
		if (g_external_switch_mode == 0) {
			if ((dev_i >= mv_eth_switch_netdev_first) && 
		    	    (dev_i <= mv_eth_switch_netdev_last)  && 
		    	    (mv_eth_switch_netdev_first < mv_eth_switch_netdev_last))
				dev_i = (mv_eth_switch_netdev_last + 1);

			pp->dev = mv_net_devs[dev_i];
			mv_eth_netdev_update(dev_i, pp);
			dev_i++;
		}
		else
#endif /* CONFIG_MV_ETH_SWITCH */
		{
			if (MV_PON_PORT(port)) {
				pp->dev = mv_net_devs[(mv_net_devs_num - 1)];
				mv_eth_netdev_update((mv_net_devs_num - 1), pp);
			}
			else {
				if (g_wan_mode == MV_WAN_MODE_MOCA) {
					pp->dev = mv_net_devs[port];
					mv_eth_netdev_update(port, pp);
				}
				else {
					pp->dev = mv_net_devs[(1 - port)];
					mv_eth_netdev_update((1 - port), pp);
				}
			}
		}
	}

	return 0;
}
#endif /* WAN_SWAP_FEATURE */


/***********************************************************
 * mv_eth_init_module --                                   *
 *   main driver initialization. loading the interfaces.   *
 ***********************************************************/
static int mv_eth_init_module(void)
{
	u32 port;
	struct eth_port *pp;
	int size;
	MV_STATUS status = MV_OK;

#ifdef ETH_SKB_DEBUG
	memset(mv_eth_skb_debug, 0, sizeof(mv_eth_skb_debug));
	spin_lock_init(&skb_debug_lock);
#endif

	if (!mv_eth_initialized) {
		init_timer(&aei_led_timer);
		aei_led_timer.function = aei_led_timer_cb;

		mvSysNetaInit(); /* init MAC Unit */
	
		mv_eth_ports_num = mvCtrlEthMaxPortGet();
		if (mv_eth_ports_num > CONFIG_MV_ETH_PORTS_NUM)
			mv_eth_ports_num = CONFIG_MV_ETH_PORTS_NUM;

		mv_net_devs_max = mv_eth_ports_num;

#ifdef CONFIG_MV_ETH_SWITCH
		mv_net_devs_max += (CONFIG_MV_ETH_SWITCH_NETDEV_NUM - 1);
#endif /* CONFIG_MV_ETH_SWITCH */

		mv_eth_config_show();

		size = mv_eth_ports_num * sizeof(struct eth_port *);
		mv_eth_ports = mvOsMalloc(size);
		if (!mv_eth_ports)
			goto oom;

		memset(mv_eth_ports, 0, size);
		
		/* Allocate array of pointers to struct net_device */
		size = mv_net_devs_max * sizeof(struct net_device *);
		mv_net_devs = mvOsMalloc(size);
		if (!mv_net_devs)
			goto oom;

		memset(mv_net_devs, 0, size);
	}

	spin_lock_init(&mv_eth_global_lock);
	spin_lock_init(&mv_eth_mii_lock);

#ifdef CONFIG_MV_ETH_PNC_PARSER
	status = pnc_default_init();
	if (status)
		printk("%s: Warning PNC init failed %d\n", __func__, status);
#endif /* CONFIG_MV_ETH_PNC_PARSER */

	if (mv_eth_bm_pools_init())
		goto oom;

#ifdef CONFIG_MV_INCLUDE_SWITCH
	if ((mvBoardSwitchConnectedPortGet(0) != -1) || (mvBoardSwitchConnectedPortGet(1) != -1)) {
		if (mv_switch_load(SWITCH_CONNECTED_PORTS_MASK)) {
			printk("\nWarning: Switch load failed\n");
		}
	}
#endif /* CONFIG_MV_INCLUDE_SWITCH */

	if (!mv_eth_initialized) {
		if (mv_eth_load_network_interfaces())
			goto oom;
	}
#ifdef WAN_SWAP_FEATURE
	else {
		if (mv_eth_reload_network_interfaces())
			goto oom;
	}
#endif /* WAN_SWAP_FEATURE */

#ifdef CONFIG_MV_ETH_NFP
	status = mv_eth_register_nfp_devices();
	if (status) {
		/* FIXME: cleanup */
		printk("Error: mv_eth_register_nfp_devices failed\n");
		return status;
	}
#endif /* CONFIG_MV_ETH_NFP */

#ifdef CONFIG_MV_ETH_HWF
	for (port = 0; port < mv_eth_ports_num; port++) {
		if (mv_eth_ports[port]) {
			mvNetaHwfInit(port);
		}
	}
#endif /* CONFIG_MV_ETH_HWF */

	/* Call mv_eth_open specifically for ports not connected to Linux netdevice */
	for (port = 0; port < mv_eth_ports_num; port++) {
		pp = mv_eth_port_by_id(port);
		if (pp) {
			if (!(pp->flags & MV_ETH_F_CONNECT_LINUX)) /* TODO, check this is not a Switch port */
				mv_eth_open(pp->dev);
		}
	}

	if (!mv_eth_initialized)
		mv_eth_cpu_counters_init();

	printk("\n");

	mv_eth_initialized = 1;

	return 0;
oom:
	if (mv_eth_ports)
		mvOsFree(mv_eth_ports);
	
	if (mv_net_devs)
		mvOsFree(mv_net_devs);

	printk("%s: out of memory\n", __func__);
	return -ENOMEM;
}


static int mv_eth_config_get(struct eth_port *pp, MV_U8 *mac_addr)
{
	char *mac_str = NULL;
	u8 zero_mac[MV_MAC_ADDR_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int mtu;

	switch (pp->port) {
	case 0:
		if (mvMtu[0] != 0)
			mtu = mvMtu[0];
		else
			mtu = CONFIG_MV_ETH_0_MTU;

		/* use default MAC address from Kconfig only if the MAC address we got is all 0 */
		if (memcmp(mvMacAddr[0], zero_mac, MV_MAC_ADDR_SIZE) == 0)
			mac_str = CONFIG_MV_ETH_0_MACADDR;
		else
			memcpy(mac_addr, mvMacAddr[0], MV_MAC_ADDR_SIZE);

		break;

#if (CONFIG_MV_ETH_PORTS_NUM > 1)
	case 1:
		if (mvMtu[1] != 0)
			mtu = mvMtu[1];
		else
			mtu = CONFIG_MV_ETH_1_MTU;

		/* use default MAC address from Kconfig only if the MAC address we got is all 0 */
		if (memcmp(mvMacAddr[1], zero_mac, MV_MAC_ADDR_SIZE) == 0)
			mac_str = CONFIG_MV_ETH_1_MACADDR;
		else
			memcpy(mac_addr, mvMacAddr[1], MV_MAC_ADDR_SIZE);

		break;
#endif /* CONFIG_MV_ETH_PORTS_NUM > 1 */

#if (CONFIG_MV_ETH_PORTS_NUM > 2)
	case 2:
		if (mvMtu[2] != 0)
			mtu = mvMtu[2];
		else
			mtu = CONFIG_MV_ETH_2_MTU;

		/* use default MAC address from Kconfig only if the MAC address we got is all 0 */
		if (memcmp(mvMacAddr[2], zero_mac, MV_MAC_ADDR_SIZE) == 0)
			mac_str = CONFIG_MV_ETH_2_MACADDR;
		else
			memcpy(mac_addr, mvMacAddr[2], MV_MAC_ADDR_SIZE);
		break;
#endif /* CONFIG_MV_ETH_PORTS_NUM > 2 */

#if (CONFIG_MV_ETH_PORTS_NUM > 3)
	case 3:
		if (mvMtu[3] != 0)
			mtu = mvMtu[3];
		else
			mtu = CONFIG_MV_ETH_3_MTU;

		/* use default MAC address from Kconfig only if the MAC address we got is all 0 */
		if (memcmp(mvMacAddr[3], zero_mac, MV_MAC_ADDR_SIZE) == 0)
			mac_str = CONFIG_MV_ETH_3_MACADDR;
		else
			memcpy(mac_addr, mvMacAddr[3], MV_MAC_ADDR_SIZE);

		break;
#endif /* CONFIG_MV_ETH_PORTS_NUM > 3 */

	default:
		printk("eth_get_config: Unexpected port number %d\n", pp->port);
		return -1;
	}
	if ((mac_str != NULL) && (mac_addr != NULL))
		mvMacStrToHex(mac_str, mac_addr);

	return mtu;
}

/***********************************************************
 * mv_eth_tx_timeout --                                    *
 *   nothing to be done (?)                                *
 ***********************************************************/
static void mv_eth_tx_timeout(struct net_device *dev)
{
#ifdef CONFIG_MV_ETH_STAT_ERR
	struct eth_port *pp = MV_ETH_PRIV(dev);

	pp->stats.tx_timeout++;
#endif /* #ifdef CONFIG_MV_ETH_STAT_ERR */

	/* printk(KERN_INFO "%s: tx timeout\n", dev->name); */
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
/*********************************************************** 
 * mv_eth_get_stats --                                      *
 *   return the device statistics.                         *
 *   print private statistics if compile flag set.         *
 ***********************************************************/
static struct net_device_stats* mv_eth_get_stats( struct net_device *dev )
{
    return &(MV_DEV_STAT(dev));
}
#endif

/******************************************************************************
* mv_eth_tool_read_mdio
* Description:
*	MDIO read implementation for kernel core MII calls
* INPUT:
*	netdev		Network device structure pointer
*	addr		PHY address
*	reg		PHY register number (offset)
* OUTPUT
*	Register value or -1 on error
*
*******************************************************************************/
int mv_read_mdio(struct net_device *netdev, int addr, int reg)
{
	unsigned long 	flags;
	unsigned short 	value;
	MV_STATUS 	status;

#ifdef CONFIG_MV_ETH_SWITCH
	struct eth_port 	*priv = MV_ETH_PRIV(netdev);
	if (isSwitch(priv))
		 return -EPERM;
#endif /* CONFIG_MV_ETH_SWITCH */

	spin_lock_irqsave(&mv_eth_mii_lock, flags);
	status = mvEthPhyRegRead(addr, reg, &value);
	spin_unlock_irqrestore(&mv_eth_mii_lock, flags);

	if (status == MV_OK)
		return value;

	return -1;
}

/******************************************************************************
* mv_eth_tool_write_mdio
* Description:
*	MDIO write implementation for kernel core MII calls
* INPUT:
*	netdev		Network device structure pointer
*	addr		PHY address
*	reg		PHY register number (offset)
*	data		Data to be written into PHY register
* OUTPUT
*	None
*
*******************************************************************************/
void mv_write_mdio(struct net_device *netdev, int addr, int reg, int data)
{
	unsigned long   flags;
	unsigned short  tmp   = (unsigned short)data;
#ifdef CONFIG_MV_ETH_SWITCH
	struct eth_port 	*priv = MV_ETH_PRIV(netdev);
	if (isSwitch(priv)) {
		printk("mv_eth_tool_write_mdio() is not supported on a switch port\n");
		return;
	}
#endif /* CONFIG_MV_ETH_SWITCH */

	spin_lock_irqsave(&mv_eth_mii_lock, flags);
	mvEthPhyRegWrite(addr, reg, tmp);
	spin_unlock_irqrestore(&mv_eth_mii_lock, flags);
}

static int mv_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
    	struct eth_port *pp = MV_ETH_PRIV(dev);
	struct mii_ioctl_data* data = if_mii(rq);

    	return generic_mii_ioctl(&pp->mii, data, cmd, NULL);
}

static void mv_mii_init(struct net_device *dev, struct eth_port *pp)
{
#ifdef CONFIG_MII
	pp->mii.dev = dev;
	pp->mii.phy_id = mvBoardPhyAddrGet(pp->port);
	pp->mii.phy_id_mask = 0xff;
	pp->mii.supports_gmii = 1;
	pp->mii.reg_num_mask = 0x1f;
	pp->mii.mdio_read = mv_read_mdio;
	pp->mii.mdio_write = mv_write_mdio;
#endif
}

/***************************************************************
 * mv_eth_netdev_init -- Allocate and initialize net_device    *
 *                   structure                                 *
 ***************************************************************/
struct net_device *mv_eth_netdev_init(struct eth_port *pp, int mtu, u8 *mac)
{
	int weight;
	struct net_device *dev;
	struct eth_dev_priv *dev_priv;

	dev = alloc_etherdev(sizeof(struct eth_dev_priv));
	if (!dev)
		return NULL;

	sprintf(dev->name, "eth%d", 1 - pp->port);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	dev_priv = (struct eth_dev_priv *)dev->priv;
#else
	dev_priv = (struct eth_dev_priv *)netdev_priv(dev);
#endif    
	if (!dev_priv)
		return NULL;

	memset(dev_priv, 0, sizeof(struct eth_dev_priv));
	dev_priv->port_p = pp;

	dev->irq = NET_TH_RXTX_IRQ_NUM(pp->port);

	dev->mtu = mtu;
	memcpy(dev->dev_addr, mac, MV_MAC_ADDR_SIZE);

	weight = CONFIG_MV_ETH_RXQ_DESC / 2;
	/* Keep weight less than 255 packets */
	if (weight > 255)
		weight = 255;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	dev->weight = weight;
#endif
	dev->tx_queue_len = CONFIG_MV_ETH_TXQ_DESC;
	dev->watchdog_timeo = 5 * HZ;

#ifdef CONFIG_MV_ETH_SWITCH
	if (pp->flags & MV_ETH_F_SWITCH) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		dev->hard_start_xmit = mv_eth_tx;
		dev->tx_timeout = mv_eth_tx_timeout;
		dev->poll = mv_eth_poll;
		dev->open = mv_eth_switch_start;
		dev->stop = mv_eth_switch_stop;
		dev->set_mac_address = mv_eth_switch_set_mac_addr;
		dev->set_multicast_list = mv_eth_switch_set_multicast_list;
		dev->change_mtu = &mv_eth_switch_change_mtu; 
		dev->do_ioctl = mv_ioctl;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		dev->get_stats = mv_eth_get_stats;
		memset(&(dev_priv->netdev_stats), 0, sizeof(struct net_device_stats));
#endif /* 2.6.22 */

#else
		dev->netdev_ops = &mv_switch_netdev_ops;
#endif /* 2.6.24 */
		dev_priv->netdev_p = mvOsMalloc(sizeof(struct eth_netdev));
		if (!dev_priv->netdev_p) {
			printk(KERN_ERR "failed to allocate eth_netdev\n");
			free_netdev(dev);
			return NULL;
		}
		memset(dev_priv->netdev_p, 0, sizeof(struct eth_netdev));
	} else
#endif /* CONFIG_MV_ETH_SWITCH */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	{
		dev->hard_start_xmit = mv_eth_tx;
		dev->tx_timeout = mv_eth_tx_timeout;
		dev->poll = mv_eth_poll;
		dev->open = mv_eth_open;
		dev->stop = mv_eth_stop;
		dev->set_mac_address = mv_eth_set_mac_addr;
		dev->set_multicast_list = mv_eth_set_multicast_list;
		dev->change_mtu = &mv_eth_change_mtu; 
		dev->do_ioctl = mv_ioctl;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		dev->get_stats = mv_eth_get_stats;
		memset(&(dev_priv->netdev_stats), 0, sizeof(struct net_device_stats));
#endif
	}
#else
		dev->netdev_ops = &mv_eth_netdev_ops;
#endif /* KERNEL_VERSION(2,6,24)*/

	mv_mii_init(dev, pp);

#ifdef CONFIG_MV_ETH_TOOL
	SET_ETHTOOL_OPS(dev, &mv_eth_tool_ops);
#endif

	if (pp->flags & MV_ETH_F_CONNECT_LINUX) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		dev->poll = mv_eth_poll;		
#else
		netif_napi_add(dev, &pp->napi, mv_eth_poll, weight);
#endif
	}
	pp->tx_done_timer.data = (unsigned long)dev;
	pp->cleanup_timer.data = (unsigned long)dev;

	mv_eth_netdev_set_features(dev);

	if (pp->flags & MV_ETH_F_CONNECT_LINUX) {
		if (register_netdev(dev)) {
			printk(KERN_ERR "failed to register %s\n", dev->name);
			free_netdev(dev);
			return NULL;
		} else {
			printk("    o %s, ifindex = %d, GbE port = %d", dev->name, dev->ifindex, pp->port);
#ifdef CONFIG_MV_ETH_SWITCH
			if (!(pp->flags & MV_ETH_F_SWITCH))
				printk("\n");
#else
			printk("\n");
#endif
		}
	}
	return dev;
}


static void mv_eth_netdev_update(int dev_index, struct eth_port *pp)
{
	int weight;
	struct eth_dev_priv *dev_priv;
#ifdef CONFIG_MV_ETH_SWITCH
	struct eth_netdev *eth_netdev_priv;
#endif /* CONFIG_MV_ETH_SWITCH */
	struct net_device *dev = mv_net_devs[dev_index];

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	dev_priv = (struct eth_dev_priv *)dev->priv;
#else
	dev_priv = (struct eth_dev_priv *)netdev_priv(dev); /* assuming dev_priv has to be valid here */
#endif

	dev_priv->port_p = pp;

	dev->irq = NET_TH_RXTX_IRQ_NUM(pp->port);
	weight = CONFIG_MV_ETH_RXQ_DESC / 2;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	dev->weight = weight;
#endif

	if (pp->flags & MV_ETH_F_CONNECT_LINUX) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		dev->poll = mv_eth_poll;		
#else
		netif_napi_add(dev, &pp->napi, mv_eth_poll, weight);
#endif
	}
	pp->tx_done_timer.data = (unsigned long)dev;
	pp->cleanup_timer.data = (unsigned long)dev;

	mv_mii_init(dev, pp);

	printk("    o %s, ifindex = %d, GbE port = %d", dev->name, dev->ifindex, pp->port);

#ifdef CONFIG_MV_ETH_SWITCH
	if (pp->flags & MV_ETH_F_SWITCH) {
		eth_netdev_priv = MV_DEV_PRIV(dev);
		mv_eth_switch_priv_update(dev, MV_SWITCH_VLAN_TO_GROUP(eth_netdev_priv->vlan_grp_id));
	}
	else {
		printk("\n");
	}	
#else
	printk("\n");
#endif /* CONFIG_MV_ETH_SWITCH */
}


int mv_eth_hal_init(struct eth_port *pp)
{
	int rxq, txp, txq, size;
	struct tx_queue *txq_ctrl;

	if (!MV_PON_PORT(pp->port)) {
		int phyAddr;

		/* Set the board information regarding PHY address */
		phyAddr = mvBoardPhyAddrGet(pp->port);
		mvNetaPhyAddrSet(pp->port, phyAddr);
	}

	/* Init port */
	pp->port_ctrl = mvNetaPortInit(pp->port, NULL);
	if (!pp->port_ctrl) {
		printk(KERN_ERR "%s: failed to load port=%d\n", __func__, pp->port);
		return -ENODEV;
	}

	size = pp->txp_num * CONFIG_MV_ETH_TXQ * sizeof(struct tx_queue);
	pp->txq_ctrl = mvOsMalloc(size);
	if (!pp->txq_ctrl)
		goto oom;

	memset(pp->txq_ctrl, 0, size);

	/* Create TX descriptor rings */
	for (txp = 0; txp < pp->txp_num; txp++) {
		for (txq = 0; txq < CONFIG_MV_ETH_TXQ; txq++) {
			txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

			txq_ctrl->q = NULL;
			txq_ctrl->cpu_owner = 0;
			txq_ctrl->hwf_rxp = 0xFF;
			txq_ctrl->txp = txp;
			txq_ctrl->txq = txq;
			txq_ctrl->txq_size = CONFIG_MV_ETH_TXQ_DESC;
			txq_ctrl->txq_count = 0;
			txq_ctrl->bm_only = MV_FALSE;

			txq_ctrl->shadow_txq_put_i = 0;
			txq_ctrl->shadow_txq_get_i = 0;
		}
	}

	pp->rxq_ctrl = mvOsMalloc(CONFIG_MV_ETH_RXQ * sizeof(struct rx_queue));
	if (!pp->rxq_ctrl)
		goto oom;

	memset(pp->rxq_ctrl, 0, CONFIG_MV_ETH_RXQ * sizeof(struct rx_queue));

	/* Create Rx descriptor rings */
	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		pp->rxq_ctrl[rxq].rxq_size = CONFIG_MV_ETH_RXQ_DESC;
	}

	if (pp->flags & (MV_ETH_F_MH | MV_ETH_F_SWITCH_RG)) {
		mvNetaMhSet(pp->port, MV_NETA_MH);
#ifdef CONFIG_MV_ETH_NFP
		mvNfpPortCapSet(pp->port, NFP_P_MH, MV_TRUE);
#endif
	}

	return 0;
      oom:
	printk("%s: port=%d: out of memory\n", __func__, pp->port);
	return -ENODEV;
}

/* Show network driver configuration */
void mv_eth_config_show(void)
{
	/* Check restrictions */
#if (CONFIG_MV_ETH_PORTS_NUM > MV_ETH_MAX_PORTS)
#   error "CONFIG_MV_ETH_PORTS_NUM is large than MV_ETH_MAX_PORTS"
#endif

#if (CONFIG_MV_ETH_RXQ > MV_ETH_MAX_RXQ)
#   error "CONFIG_MV_ETH_RXQ is large than MV_ETH_MAX_RXQ"
#endif

#if CONFIG_MV_ETH_TXQ > MV_ETH_MAX_TXQ
#   error "CONFIG_MV_ETH_TXQ is large than MV_ETH_MAX_TXQ"
#endif

#if defined(CONFIG_MV_ETH_TSO) && !defined(CONFIG_MV_ETH_TX_CSUM_OFFLOAD)
#   error "If GSO enabled - TX checksum offload must be enabled too"
#endif

	printk("  o %d Giga ports supported\n", CONFIG_MV_ETH_PORTS_NUM);

#ifdef CONFIG_MV_PON
	printk("  o Giga PON port is #%d: - %d TCONTs supported\n", MV_PON_PORT_ID, MV_ETH_MAX_TCONT());
#endif

#ifdef CONFIG_NET_SKB_RECYCLE
	printk("  o SKB recycle supported (%s)\n", mv_ctrl_recycle ? "Enabled" : "Disabled");
#endif

#ifdef CONFIG_MV_ETH_NETA
	printk("  o NETA acceleration mode %d\n", mvNetaAccMode());
#endif

#ifdef CONFIG_MV_ETH_BM
	printk("  o BM supported: short buffer size is %d bytes\n", CONFIG_MV_ETH_SHORT_PKT_SIZE);
#endif

#ifdef CONFIG_MV_ETH_PNC
	printk("  o PnC supported\n");
#endif

#ifdef CONFIG_MV_ETH_HWF
	printk("  o HWF supported\n");
#endif

#ifdef CONFIG_MV_ETH_PMT
	printk("  o PMT supported\n");
#endif

	printk("  o RX Queue support: %d Queues * %d Descriptors\n", CONFIG_MV_ETH_RXQ, CONFIG_MV_ETH_RXQ_DESC);

	printk("  o TX Queue support: %d Queues * %d Descriptors\n", CONFIG_MV_ETH_TXQ, CONFIG_MV_ETH_TXQ_DESC);

#if defined(CONFIG_MV_ETH_TSO)
	printk("  o GSO supported\n");
#endif /* CONFIG_MV_ETH_TSO */

#if defined(CONFIG_MV_ETH_GRO)
	printk("  o GRO supported\n");
#endif /* CONFIG_MV_ETH_GRO */

#if defined(CONFIG_MV_ETH_RX_CSUM_OFFLOAD)
	printk("  o Receive checksum offload supported\n");
#endif
#if defined(CONFIG_MV_ETH_TX_CSUM_OFFLOAD)
	printk("  o Transmit checksum offload supported\n");
#endif

#ifdef CONFIG_MV_ETH_NFP
	printk("  o Network Fast Processing (Routing) supported\n");

#ifdef CONFIG_MV_ETH_NFP_NAT
	printk("  o Network Fast Processing (NAT) supported\n");
#endif /* CONFIG_MV_ETH_NFP_NAT */

#endif /* CONFIG_MV_ETH_NFP */

#ifdef CONFIG_MV_ETH_STAT_ERR
	printk("  o Driver ERROR statistics enabled\n");
#endif

#ifdef CONFIG_MV_ETH_STAT_INF
	printk("  o Driver INFO statistics enabled\n");
#endif

#ifdef CONFIG_MV_ETH_STAT_DBG
	printk("  o Driver DEBUG statistics enabled\n");
#endif

#ifdef ETH_DEBUG
	printk("  o Driver debug messages enabled\n");
#endif

#ifdef CONFIG_MV_ETH_PROC
	printk("  o Proc tool API enabled\n");
#endif

#if defined(CONFIG_MV_ETH_SWITCH)
	printk("  o Switch support enabled\n");

#ifdef CONFIG_MV_ETH_IGMP
	printk("     o IGMP special processing support\n");
#endif /* CONFIG_MV_ETH_IGMP */

#endif /* CONFIG_MV_ETH_SWITCH */

	printk("\n");
}

static void mv_eth_netdev_set_features(struct net_device *dev)
{
	dev->features = NETIF_F_SG | NETIF_F_LLTX;

#ifdef CONFIG_MV_ETH_TX_CSUM_OFFLOAD_DEF
	if (dev->mtu <= MV_ETH_TX_CSUM_MAX_SIZE) {
		dev->features |= NETIF_F_IP_CSUM;
	}
#endif /* CONFIG_MV_ETH_TX_CSUM_OFFLOAD_DEF */

#ifdef CONFIG_MV_ETH_GRO_DEF
	dev->features |= NETIF_F_GRO;
#endif /* CONFIG_MV_ETH_GRO_DEF */

#ifdef CONFIG_MV_ETH_TSO_DEF
	dev->features |= NETIF_F_TSO;
#endif /* CONFIG_MV_ETH_TSO_DEF */

}

static void mv_eth_priv_cleanup(struct eth_port *pp)
{
	/* TODO */
}

static inline struct bm_pool *mv_eth_pool_find(struct eth_port *pp, int pkt_size)
{
	int pool, i;
	struct bm_pool	*bm_pool, *temp_pool = NULL;

	/* look for free pool pkt_size == 0. First check pool == pp->port */
	/* if no free pool choose larger than required */
	for (i = 0; i < MV_ETH_BM_POOLS; i++) {
		pool = (pp->port + i) % MV_ETH_BM_POOLS;
		bm_pool = &mv_eth_pool[pool];
		if (bm_pool->pkt_size == 0) {
			/* found free pool */
			bm_pool->pkt_size = pkt_size;
			return bm_pool;
		}
		if (bm_pool->pkt_size >= pkt_size) {
			if (temp_pool == NULL)
				temp_pool = bm_pool;
			else if (bm_pool->pkt_size < temp_pool->pkt_size)
				temp_pool = bm_pool;
		}
	}
	return temp_pool;
}

static int mv_eth_rxq_fill(struct eth_port *pp, int rxq, int num)
{
	int i;

#ifndef CONFIG_MV_ETH_BM
	struct eth_pbuf *pkt;
	struct bm_pool *bm_pool;
	MV_NETA_RXQ_CTRL *rx_ctrl;
	struct neta_rx_desc *rx_desc;

	bm_pool = pp->pool_long;

	rx_ctrl = pp->rxq_ctrl[rxq].q;
	if (!rx_ctrl) {
		printk("%s: rxq %d is not initialized\n", __func__, rxq);
		return 0;
	}

	for (i = 0; i < num; i++) {
		pkt = mv_eth_pool_get(bm_pool);
		if (pkt) {
			rx_desc = (struct neta_rx_desc *)MV_NETA_QUEUE_DESC_PTR(&rx_ctrl->queueCtrl, i);
			memset(rx_desc, 0, sizeof(rx_desc));

			mvNetaRxDescFill(rx_desc, pkt->physAddr, (MV_U32)pkt);
		} else {
			printk("%s: rxq %d, %d of %d buffers are filled\n", __func__, rxq, i, num);
			break;
		}
	}
#else
	i = num;
#endif /* CONFIG_MV_ETH_BM */

	mvNetaRxqNonOccupDescAdd(pp->port, rxq, i);

	return i;
}

static int mv_eth_txq_create(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
#ifdef CONFIG_MV_ETH_TSO
	int i;
#endif /* CONFIG_MV_ETH_TSO */

	txq_ctrl->q = mvNetaTxqInit(pp->port, txq_ctrl->txp, txq_ctrl->txq, txq_ctrl->txq_size);
	if (txq_ctrl->q == NULL) {
		printk("%s: can't create TxQ - port=%d, txp=%d, txq=%d, desc=%d\n",
		       __func__, pp->port, txq_ctrl->txp, txq_ctrl->txp, txq_ctrl->txq_size);
		return -ENODEV;
	}

	txq_ctrl->shadow_txq = mvOsMalloc(txq_ctrl->txq_size * sizeof(MV_ULONG));
	if (txq_ctrl->shadow_txq == NULL) {
		goto no_mem;
	}
#ifdef CONFIG_MV_ETH_TSO
	/* 114 = 14 for mac + 60 for max IP hdr + 40 for max TCP */
	/* MV_ETH_MH_SIZE is also added */
	txq_ctrl->extradataArr = mvOsMalloc(txq_ctrl->txq_size * sizeof(struct extradata));
	if (!txq_ctrl->extradataArr) {
		goto no_mem;
	}

	for (i = 0; i < txq_ctrl->txq_size; i++) {
		txq_ctrl->extradataArr[i].data = mvOsMalloc(114 + MV_ETH_MH_SIZE);
		if (!txq_ctrl->extradataArr[i].data) {
			goto no_mem;
		}
	}
#endif /* CONFIG_MV_ETH_TSO */

	/* reset txq */
	txq_ctrl->txq_count = 0;
	txq_ctrl->shadow_txq_put_i = 0;
	txq_ctrl->shadow_txq_get_i = 0;

#ifdef CONFIG_MV_ETH_HWF
	mvNetaHwfTxqInit(pp->port, txq_ctrl->txp, txq_ctrl->txq);
#endif /* CONFIG_MV_ETH_HWF */

	return 0;

no_mem:
	mv_eth_txq_delete(pp, txq_ctrl);
	return -ENOMEM;
}


static int mv_force_port_link_speed_fc(int port, MV_ETH_PORT_SPEED port_speed, int en_force)
{
	if (en_force) {
		if (mvNetaForceLinkModeSet(port, 1, 0)) {
			printk(KERN_ERR "mvNetaForceLinkModeSet failed\n");
			return -EIO;
		}
		if (mvNetaSpeedDuplexSet(port, port_speed, MV_ETH_DUPLEX_FULL)) {
			printk(KERN_ERR "mvNetaSpeedDuplexSet failed\n");
			return -EIO;
		}
		if (mvNetaFlowCtrlSet(port, MV_ETH_FC_ENABLE)) {
			printk(KERN_ERR "mvNetaFlowCtrlSet failed\n");
			return -EIO;
		}
	}
	else {
		if (mvNetaForceLinkModeSet(port, 0, 0)) {
			printk(KERN_ERR "mvNetaForceLinkModeSet failed\n");
			return -EIO;
		}
		if (mvNetaSpeedDuplexSet(port, MV_ETH_SPEED_AN, MV_ETH_DUPLEX_AN)) {
			printk(KERN_ERR "mvNetaSpeedDuplexSet failed\n");
			return -EIO;
		}
		if (mvNetaFlowCtrlSet(port, MV_ETH_FC_AN_SYM)) {
			printk(KERN_ERR "mvNetaFlowCtrlSet failed\n");
			return -EIO;
		}
	}
	return 0;
}

static void mv_eth_txq_delete(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
#ifdef CONFIG_MV_ETH_TSO
	int i;
	if (txq_ctrl->extradataArr) {
		for (i = 0; i < txq_ctrl->txq_size; i++) {
			if (txq_ctrl->extradataArr[i].data)
				mvOsFree(txq_ctrl->extradataArr[i].data);
		}
		mvOsFree(txq_ctrl->extradataArr);
		txq_ctrl->extradataArr = NULL;
	}
#endif /* CONFIG_MV_ETH_TSO */

	if (txq_ctrl->shadow_txq) {
		mvOsFree(txq_ctrl->shadow_txq);
		txq_ctrl->shadow_txq = NULL;
	}

	if (txq_ctrl->q) {
		mvNetaTxqDelete(pp->port, txq_ctrl->txp, txq_ctrl->txq);
		txq_ctrl->q = NULL;
	}
}

/* Free all packets pending transmit from all TXQs and reset TX port */
int mv_eth_txp_reset(int port, int txp)
{
	struct eth_port *pp = mv_eth_port_by_id(port);
	int queue;

	if (pp->flags & MV_ETH_F_STARTED) {
		printk("Port %d must be stopped before\n", port);
		return -EINVAL;
	}

	/* free the skb's in the hal tx ring */
	for (queue = 0; queue < CONFIG_MV_ETH_TXQ; queue++) {
		struct tx_queue *txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + queue];

		if (txq_ctrl->q)
			mv_eth_txq_done_force(pp, txq_ctrl);
	}
	mvNetaTxpReset(port, txp);
	return 0;
}

/* Free received packets from all RXQs and reset RX of the port */
int mv_eth_rx_reset(int port)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp->flags & MV_ETH_F_STARTED) {
		printk("Port %d must be stopped before\n", port);
		return -EINVAL;
	}

#ifndef CONFIG_MV_ETH_BM
	{
		int rxq = 0;

		for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
			struct eth_pbuf *pkt;
			struct neta_rx_desc *rx_desc;
			struct bm_pool *pool;
			int i, rx_done;
			MV_NETA_RXQ_CTRL *rx_ctrl = pp->rxq_ctrl[rxq].q;

			if (rx_ctrl == NULL)
				continue;

			rx_done = mvNetaRxqFreeDescNumGet(pp->port, rxq);
			for (i = 0; i < rx_done; i++) {
				rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
				mvOsCacheLineInv(NULL, rx_desc);

#if defined(MV_CPU_BE)
				mvNetaRxqDescSwap(rx_desc);
#endif /* MV_CPU_BE */

				pkt = (struct eth_pbuf *)rx_desc->bufCookie;
				pool = &mv_eth_pool[pkt->pool];
				mv_eth_pool_put(pool, pkt);
			}
		}
	}
#endif /* CONFIG_MV_ETH_BM */

	mvNetaRxReset(port);
	return 0;
}

/***********************************************************
 * mv_eth_start_internals --                               *
 *   fill rx buffers. start rx/tx activity. set coalesing. *
 *   clear and unmask interrupt bits                       *
 ***********************************************************/
int mv_eth_start_internals(struct eth_port *pp, int mtu)
{
	unsigned long flags;
	unsigned int status;
	int rxq, txp, txq, num, err = 0;
	int pkt_size = RX_PKT_SIZE(mtu);
	MV_BOARD_MAC_SPEED mac_speed;

	spin_lock_irqsave(pp->lock, flags);

	if (mvNetaMaxRxSizeSet(pp->port, RX_PKT_SIZE(mtu))) {
		printk("%s: can't set maxRxSize=%d for port=%d, mtu=%d\n",
		       __func__, RX_PKT_SIZE(mtu), pp->port, mtu);
		err = -EINVAL;
		goto out;
	}

	if (mv_eth_ctrl_is_tx_enabled(pp)) {
		int cpu;
		for_each_possible_cpu(cpu) {
			if (mv_eth_ctrl_txq_cpu_own(pp->port, pp->txp, pp->txq[cpu], 1) < 0) {
				err = -EINVAL;
				goto out;
			}
		}
	}

	/* Allocate buffers for Long buffers pool */
	if (pp->pool_long == NULL) {
		pp->pool_long = mv_eth_pool_find(pp, pkt_size);

		if (pp->pool_long == NULL) {
			printk("%s FAILED: port=%d, Can't find pool for pkt_size=%d\n",
			       __func__, pp->port, pkt_size);
			err = -ENOMEM;
			goto out;
		}

		num = mv_eth_pool_add(pp->pool_long->pool, pp->pool_long_num);
		if (num != pp->pool_long_num) {
			printk("%s FAILED: pool=%d, pkt_size=%d, only %d of %d allocated\n",
			       __func__, pp->pool_long->pool, pkt_size, num, pp->pool_long_num);
			err = -ENOMEM;
			goto out;
		}
	}
#ifdef CONFIG_MV_ETH_BM
	/* Allocate packets for short pool */
	if (pp->pool_short == NULL) {
		pp->pool_short = &mv_eth_pool[MV_ETH_SHORT_BM_POOL];
		num = mv_eth_pool_add(pp->pool_short->pool, pp->pool_short_num);
		if (num != pp->pool_short_num) {
			printk("%s FAILED: pool=%d, pkt_size=%d - %d of %d buffers added\n",
			       __func__, MV_ETH_SHORT_BM_POOL, CONFIG_MV_ETH_SHORT_PKT_SIZE,
			       num, pp->pool_short_num);
			err = -ENOMEM;
			goto out;
		}
	}
	mvNetaBmPoolBufSizeSet(pp->port, pp->pool_short->pool, RX_BUF_SIZE(CONFIG_MV_ETH_SHORT_PKT_SIZE));
	mvNetaBmPoolBufSizeSet(pp->port, pp->pool_long->pool, RX_BUF_SIZE(pkt_size));
#endif /* CONFIG_MV_ETH_BM */

	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		if (pp->rxq_ctrl[rxq].q == NULL) {
			pp->rxq_ctrl[rxq].q = mvNetaRxqInit(pp->port, rxq, pp->rxq_ctrl[rxq].rxq_size);
			if (!pp->rxq_ctrl[rxq].q) {
				printk("%s: can't create RxQ port=%d, rxq=%d, desc=%d\n",
				       __func__, pp->port, rxq, pp->rxq_ctrl[rxq].rxq_size);
				err = -ENODEV;
				goto out;
			}
		}

		/* Set Offset */
		mvNetaRxqOffsetSet(pp->port, rxq, NET_SKB_PAD);
		/* Set coalescing pkts and time */
		mvNetaRxqPktsCoalSet(pp->port, rxq, CONFIG_MV_ETH_RX_COAL_PKTS);
		mvNetaRxqTimeCoalSet(pp->port, rxq, CONFIG_MV_ETH_RX_COAL_USEC);

#if defined(CONFIG_MV_ETH_BM)
		/* Enable / Disable - BM support */
		mvNetaRxqBmEnable(pp->port, rxq, pp->pool_short->pool, pp->pool_long->pool);
#else
		/* Fill RXQ with buffers from RX pool */
		mvNetaRxqBufSizeSet(pp->port, rxq, RX_BUF_SIZE(pkt_size));
		mvNetaRxqBmDisable(pp->port, rxq);
#endif /* CONFIG_MV_ETH_BM */

		if (mvNetaRxqFreeDescNumGet(pp->port, rxq) == 0)
			mv_eth_rxq_fill(pp, rxq, pp->rxq_ctrl[rxq].rxq_size);
	}

	for (txp = 0; txp < pp->txp_num; txp++) {
		for (txq = 0; txq < CONFIG_MV_ETH_TXQ; txq++) {
			struct tx_queue *txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

			if ((txq_ctrl->q == NULL) && (txq_ctrl->txq_size > 0)) {
				err = mv_eth_txq_create(pp, txq_ctrl);
				if (err)
					goto out;
			}
			mvNetaTxDonePktsCoalSet(pp->port, txp, txq, mv_ctrl_txdone);
		}
		mvNetaTxpMaxTxSizeSet(pp->port, txp, RX_PKT_SIZE(mtu));
	}

#ifdef CONFIG_MV_ETH_HWF
	mvNetaHwfEnable(pp->port, 1);
#endif /* CONFIG_MV_ETH_HWF */

	/* force link, speed and duplex if necessary (e.g. Switch is connected) based on board information */
	mac_speed = mvBoardMacSpeedGet(pp->port);
	switch (mac_speed) {
	case BOARD_MAC_SPEED_10M:
		err = mv_force_port_link_speed_fc(pp->port, MV_ETH_SPEED_10, 1);
		if (err)
			goto out;
		break;
	case BOARD_MAC_SPEED_100M:
		err = mv_force_port_link_speed_fc(pp->port, MV_ETH_SPEED_100, 1);
		if (err)
			goto out;
		break;
	case BOARD_MAC_SPEED_1000M:
		err = mv_force_port_link_speed_fc(pp->port, MV_ETH_SPEED_1000, 1);
		if (err)
			goto out;
		break;
	case BOARD_MAC_SPEED_AUTO:
	default:
		/* do nothing */
		break;
	}

	/* start the hal - rx/tx activity */
	status = mvNetaPortEnable(pp->port);
	if (status == MV_OK) {
		pp->flags |= MV_ETH_F_LINK_UP;
	} else if (MV_PON_PORT(pp->port)) {
		mvNetaPortUp(pp->port);
		pp->flags |= MV_ETH_F_LINK_UP;
	}
	pp->flags |= MV_ETH_F_STARTED;

 out:
	spin_unlock_irqrestore(pp->lock, flags);
	printk("port %d: setting MAC to %#x, status: %#x\n", pp->port, mac_speed, MV_REG_READ(NETA_GMAC_STATUS_REG(pp->port)));
	return err;
}

/***********************************************************
 * mv_eth_stop_internals --                                *
 *   stop port rx/tx activity. free skb's from rx/tx rings.*
 ***********************************************************/
int mv_eth_stop_internals(struct eth_port *pp)
{
	unsigned long flags;
	int queue;

	spin_lock_irqsave(pp->lock, flags);

	pp->flags &= ~MV_ETH_F_STARTED;

	/* stop the port activity, mask all interrupts */
	if (mvNetaPortDisable(pp->port) != MV_OK) {
		printk(KERN_ERR "GbE port %d: ethPortDisable failed\n", pp->port);
		goto error;
	}

	/* clear all ethernet port interrupts ???? */
	MV_REG_WRITE(NETA_INTR_MISC_CAUSE_REG(pp->port), 0);
	MV_REG_WRITE(NETA_INTR_OLD_CAUSE_REG(pp->port), 0);

	/* mask all ethernet port interrupts ???? */
	MV_REG_WRITE(NETA_INTR_NEW_MASK_REG(pp->port), 0);
	MV_REG_WRITE(NETA_INTR_OLD_MASK_REG(pp->port), 0);
	MV_REG_WRITE(NETA_INTR_MISC_MASK_REG(pp->port), 0);

#ifndef CONFIG_MV_ETH_HWF
	{
		int txp;
		/* Reset TX port here. If HWF is supported reset must be called externally */
		for (txp = 0; txp < pp->txp_num; txp++)
			mv_eth_txp_reset(pp->port, txp);
	}
#endif /* !CONFIG_MV_ETH_HWF */

	if (mv_eth_ctrl_is_tx_enabled(pp)) {
		int cpu;
		for_each_possible_cpu(cpu)
			mv_eth_ctrl_txq_cpu_own(pp->port, pp->txp, pp->txq[cpu], 0);
	}

	/* free the skb's in the hal rx ring */
	for (queue = 0; queue < CONFIG_MV_ETH_RXQ; queue++) {
		mv_eth_rxq_drop_pkts(pp, queue);
	}
	spin_unlock_irqrestore(pp->lock, flags);

	return 0;

error:
	printk(KERN_ERR "GbE port %d: stop internals failed\n", pp->port);
	spin_unlock_irqrestore(pp->lock, flags);
	return -1;
}

/***********************************************************
 * mv_eth_change_mtu_internals --                          *
 *   stop port activity. release skb from rings. set new   *
 *   mtu in device and hw. restart port activity and       *
 *   and fill rx-buiffers with size according to new mtu.  *
 ***********************************************************/
int mv_eth_change_mtu_internals(struct net_device *dev, int mtu)
{
	struct bm_pool	*new_pool = NULL;
	struct eth_port *pp = MV_ETH_PRIV(dev);
	unsigned long	flags;

	if (mtu > 9676 /* 9700 - 20 and rounding to 8 */) {
		printk("%s: Illegal MTU value %d, ", dev->name, mtu);
		mtu = 9676;
		printk(" rounding MTU to: %d \n", mtu);
	}

	if (MV_IS_NOT_ALIGN(RX_PKT_SIZE(mtu), 8)) {
		printk("%s: Illegal MTU value %d, ", dev->name, mtu);
		mtu = MV_ALIGN_UP(RX_PKT_SIZE(mtu), 8);
		printk(" rounding MTU to: %d \n", mtu);
	}

	if (mtu != dev->mtu) {

		spin_lock_irqsave(pp->lock, flags);

		mv_eth_rx_reset(pp->port);

#ifdef CONFIG_MV_ETH_POOL_PREDEFINED
		/* Check if new MTU require pool change */
		new_pool = mv_eth_pool_find(pp, RX_PKT_SIZE(mtu));
		if (new_pool == NULL)
			printk("%s FAILED: port=%d, Can't find pool for pkt_size=%d\n",
					__func__, pp->port, RX_PKT_SIZE(mtu));
#endif /* CONFIG_MV_ETH_POOL_PREDEFINED */

		/* Free all buffers from long pool */
		if ((pp->pool_long) && (pp->pool_long != new_pool)) {
			mv_eth_pool_free(pp->pool_long->pool, pp->pool_long_num);

			/* redefine pool pkt_size */
			if (new_pool == NULL)
				pp->pool_long->pkt_size = 0;

			pp->pool_long = NULL;
		}

		/* DIMA debug; Free all buffers from short pool */
/*
		if(pp->pool_short) {
			mv_eth_pool_free(pp->pool_short->pool, pp->pool_short_num);
			pp->pool_short = NULL;
		}
*/
		spin_unlock_irqrestore(pp->lock, flags);
	}

	dev->mtu = mtu;

	mv_eth_netdev_set_features(dev);

	return 0;
}

/***********************************************************
 * mv_eth_tx_done_timer_callback --			   *
 *   N msec periodic callback for tx_done                  *
 ***********************************************************/
static void mv_eth_tx_done_timer_callback(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct eth_port *pp = MV_ETH_PRIV(dev);
	int tx_done = 0, tx_todo = 0;

	STAT_INFO(pp->stats.tx_done_timer++);

	pp->flags &= ~MV_ETH_F_TX_DONE_TIMER;

	spin_lock(pp->lock);

	if (MV_PON_PORT(pp->port))
		tx_done = mv_eth_tx_done_pon(pp, &tx_todo);
	else
		/* check all possible queues, as there is no indication from interrupt */
		tx_done = mv_eth_tx_done_gbe(pp, (((1 << CONFIG_MV_ETH_TXQ) - 1) & NETA_CAUSE_TXQ_SENT_DESC_ALL_MASK), &tx_todo);

	spin_unlock(pp->lock);

	if (tx_todo > 0)
		mv_eth_add_tx_done_timer(pp);
}

/***********************************************************
 * mv_eth_cleanup_timer_callback --			   *
 *   N msec periodic callback for error cleanup            *
 ***********************************************************/
static void mv_eth_cleanup_timer_callback(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct eth_port *pp = MV_ETH_PRIV(dev);

	STAT_INFO(pp->stats.cleanup_timer++);

	spin_lock(pp->lock);

	/* FIXME: check bm_pool->missed and pp->rxq_ctrl[rxq].missed counters and allocate */
	/* re-add timer if necessary (check bm_pool->missed and pp->rxq_ctrl[rxq].missed   */

	pp->flags &= ~MV_ETH_F_CLEANUP_TIMER;

	spin_unlock(pp->lock);
}

void mv_eth_mac_show(int port)
{
#ifdef CONFIG_MV_ETH_PNC_PARSER
	mvOsPrintf("PnC MAC Rules - port #%d:\n", port);
	pnc_mac_show();
#endif /* CONFIG_MV_ETH_PNC_PARSER */

#ifdef CONFIG_MV_ETH_LEGACY_PARSER
	mvEthPortUcastShow(port);
	mvEthPortMcastShow(port);
#endif /* CONFIG_MV_ETH_LEGACY_PARSER */
}

void mv_eth_tos_map_show(int port)
{
	int tos, txq;
	struct eth_port *pp = mv_eth_port_by_id(port);

#ifdef CONFIG_MV_ETH_PNC_PARSER
	pnc_ipv4_dscp_show();
#endif /* CONFIG_MV_ETH_PNC_PARSER */

#ifdef CONFIG_MV_ETH_LEGACY_PARSER
	for (tos = 0; tos < 0xFF; tos += 0x4) {
		int rxq;

		rxq = mvNetaTosToRxqGet(port, tos);
		if (rxq > 0)
			printk("tos=0x%02x: codepoint=0x%02x, rxq=%d\n",
					tos, tos >> 2, rxq);
	}
#endif /* CONFIG_MV_ETH_LEGACY_PARSER */

	printk("\n");
	printk(" TOS <=> TXQ map for port #%d\n\n", port);

	for (tos = 0; tos < sizeof(pp->txq_tos_map); tos++) {
		txq = pp->txq_tos_map[tos];
		if (txq != MV_ETH_TXQ_INVALID)
			printk("0x%02x <=> %d\n", tos, txq);
	}
}

int mv_eth_rxq_tos_map_set(int port, int rxq, unsigned char tos)
{
	int status = 1;

#ifdef CONFIG_MV_ETH_PNC_PARSER
	status = pnc_ip4_dscp(tos, 0xFF, rxq);
#endif /* CONFIG_MV_ETH_PNC_PARSER */

#ifdef CONFIG_MV_ETH_LEGACY_PARSER
	status = mvNetaTosToRxqSet(port, tos, rxq);
#endif /* CONFIG_MV_ETH_LEGACY_PARSER */

	if (status == 0)
		printk("Succeeded\n");
	else if (status == 1)
		printk("Not supported\n");
	else
		printk("Failed\n");

	return status;
}

/* Set TXQ for special TOS value. txq=-1 - use default TXQ for this port */
int mv_eth_txq_tos_map_set(int port, int txq, unsigned char tos)
{
	MV_U8 old_txq;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (mvNetaPortCheck(port))
		return -EINVAL;

	if ((pp == NULL) || (pp->txq_ctrl == NULL))
		return -ENODEV;

	old_txq = pp->txq_tos_map[tos];

	if (old_txq != MV_ETH_TXQ_INVALID) {
		if (old_txq == (MV_U8) txq)
			return 0;

		if (mv_eth_ctrl_txq_cpu_own(port, pp->txp, old_txq, 0))
			return -EINVAL;
	}

	if (txq == -1) {
		pp->txq_tos_map[tos] = MV_ETH_TXQ_INVALID;
		return 0;
	}

	if (mvNetaMaxCheck(txq, CONFIG_MV_ETH_TXQ))
		return -EINVAL;

	if (mv_eth_ctrl_txq_cpu_own(port, pp->txp, txq, 1))
		return -EINVAL;

	pp->txq_tos_map[tos] = (MV_U8)txq;   

	return 0;
}

static void mv_eth_priv_init(struct eth_port *pp, int port)
{
	int i;

	TRC_INIT(0, 0, 0, 0);
	TRC_START();

	memset(pp, 0, sizeof(struct eth_port));

	pp->port = port;
	pp->txp_num = 1;
	pp->txp = 0;
	for_each_possible_cpu(i)
		pp->txq[i] = CONFIG_MV_ETH_TXQ_DEF;

	pp->flags = 0;
	pp->pool_long_num = CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC * CONFIG_MV_ETH_MTU_PKT_MULT;

#ifdef CONFIG_MV_ETH_BM
	if (pp->pool_long_num > MV_BM_POOL_CAP_MAX)
		pp->pool_long_num = MV_BM_POOL_CAP_MAX;

	pp->pool_short_num = CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC * CONFIG_MV_ETH_SHORT_PKT_MULT;
	if (pp->pool_short_num > MV_BM_POOL_CAP_MAX / CONFIG_MV_ETH_PORTS_NUM)
		pp->pool_short_num = MV_BM_POOL_CAP_MAX / CONFIG_MV_ETH_PORTS_NUM;
#endif /* CONFIG_MV_ETH_BM */

#ifdef CONFIG_MV_ETH_POOL_PREDEFINED
	if (pp->pool_long_num > MV_BM_POOL_CAP_MAX / CONFIG_MV_ETH_PORTS_NUM)
		pp->pool_long_num = MV_BM_POOL_CAP_MAX / CONFIG_MV_ETH_PORTS_NUM;
#endif /* CONFIG_MV_ETH_POOL_PREDEFINED */

	for (i = 0; i < 256; i++) {
		pp->txq_tos_map[i] = MV_ETH_TXQ_INVALID;

#ifdef CONFIG_MV_ETH_TX_SPECIAL
		pp->tx_special_check = NULL;
#endif /* CONFIG_MV_ETH_TX_SPECIAL */
	}

	mv_eth_port_config_parse(pp);

#ifdef CONFIG_MV_PON
	if (MV_PON_PORT(port)) {
		pp->flags |= MV_ETH_F_MH;
		pp->txp_num = MV_ETH_MAX_TCONT();
		pp->txp = CONFIG_MV_PON_TXP_DEF;
		for_each_possible_cpu(i)
			pp->txq[i] = CONFIG_MV_PON_TXQ_DEF;
	}
#endif /* CONFIG_MV_PON */

#if defined(CONFIG_MV_ETH_RX_CSUM_OFFLOAD_DEF)
	pp->rx_csum_offload = 1;
#endif /* CONFIG_MV_ETH_RX_CSUM_OFFLOAD_DEF */

	if (mvBoardSwitchConnectedPortGet(port) != -1) {
#ifdef CONFIG_MV_INCLUDE_SWITCH
		pp->flags |= (MV_ETH_F_SWITCH | MV_ETH_F_EXT_SWITCH);
#else
		pp->flags |= MV_ETH_F_SWITCH_RG;
#endif /* CONFIG_MV_INCLUDE_SWITCH */
	}

	memset(&pp->tx_done_timer, 0, sizeof(struct timer_list));
	pp->tx_done_timer.function = mv_eth_tx_done_timer_callback;
	init_timer(&pp->tx_done_timer);
	pp->flags &= ~MV_ETH_F_TX_DONE_TIMER;
	memset(&pp->cleanup_timer, 0, sizeof(struct timer_list));
	pp->cleanup_timer.function = mv_eth_cleanup_timer_callback;
	init_timer(&pp->cleanup_timer);
	pp->flags &= ~MV_ETH_F_CLEANUP_TIMER;

	pp->lock = &mv_eth_global_lock;

#ifdef CONFIG_MV_ETH_STAT_DIST
	pp->dist_stats.rx_dist = mvOsMalloc(sizeof(u32) * (CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC + 1));
	if (pp->dist_stats.rx_dist != NULL) {
		pp->dist_stats.rx_dist_size = CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC + 1;
		memset(pp->dist_stats.rx_dist, 0, sizeof(u32) * pp->dist_stats.rx_dist_size);
	} else
		printk("ethPort #%d: Can't allocate %d bytes for rx_dist\n",
		       pp->port, sizeof(u32) * (CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC + 1));

	pp->dist_stats.tx_done_dist =
	    mvOsMalloc(sizeof(u32) * (pp->txp_num * CONFIG_MV_ETH_TXQ * CONFIG_MV_ETH_TXQ_DESC + 1));
	if (pp->dist_stats.tx_done_dist != NULL) {
		pp->dist_stats.tx_done_dist_size = pp->txp_num * CONFIG_MV_ETH_TXQ * CONFIG_MV_ETH_TXQ_DESC + 1;
		memset(pp->dist_stats.tx_done_dist, 0, sizeof(u32) * pp->dist_stats.tx_done_dist_size);
	} else
		printk("ethPort #%d: Can't allocate %d bytes for tx_done_dist\n",
		       pp->port, sizeof(u32) * (pp->txp_num * CONFIG_MV_ETH_TXQ * CONFIG_MV_ETH_TXQ_DESC + 1));
#endif /* CONFIG_MV_ETH_STAT_DIST */
}

/***********************************************************************************
 ***  noqueue net device
 ***********************************************************************************/
extern struct Qdisc noop_qdisc;
void mv_eth_set_noqueue(struct net_device *dev, int enable)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
    if (dev->flags & IFF_UP) {
            printk(KERN_ERR "%s: device or resource busy, take it down\n", dev->name);
            return;
    }
    dev->tx_queue_len = enable ? 0 : CONFIG_MV_ETH_TXQ_DESC;

    dev->qdisc_sleeping = &noop_qdisc;
#else

	struct netdev_queue *txq = netdev_get_tx_queue(dev, 0);

	if (dev->flags & IFF_UP) {
		printk(KERN_ERR "%s: device or resource busy, take it down\n", dev->name);
		return;
	}
	dev->tx_queue_len = enable ? 0 : CONFIG_MV_ETH_TXQ_DESC;

	if (txq)
		txq->qdisc_sleeping = &noop_qdisc;
	else
		printk(KERN_ERR "%s: txq #0 is NULL\n", dev->name);

	printk(KERN_ERR "%s: device tx queue len is %d\n", dev->name, (int)dev->tx_queue_len);
#endif
}

/***********************************************************************************
 ***  print RX bm_pool status
 ***********************************************************************************/
void mv_eth_pool_status_print(int pool)
{
	struct bm_pool *bm_pool = &mv_eth_pool[pool];

	printk("\nRX Pool #%d: pkt_size=%d, BM-HW support - %s\n",
	       pool, bm_pool->pkt_size, mv_eth_pool_bm(bm_pool) ? "Yes" : "No");

	printk("bm_pool=%p, stack=%p, capacity=%d, buf_num=%d, missed=%d\n",
	       bm_pool->bm_pool, bm_pool->stack, bm_pool->capacity, bm_pool->buf_num, bm_pool->missed);

#ifdef CONFIG_MV_ETH_STAT_ERR
	printk("Errors: skb_alloc_oom=%u, stack_empty=%u, stack_full=%u\n",
	       bm_pool->stats.skb_alloc_oom, bm_pool->stats.stack_empty, bm_pool->stats.stack_full);
#endif /* #ifdef CONFIG_MV_ETH_STAT_ERR */

#ifdef CONFIG_MV_ETH_STAT_DBG
	printk("skb_alloc_ok=%u, bm_put=%u, stack_put=%u, stack_get=%u\n",
	       bm_pool->stats.skb_alloc_ok, bm_pool->stats.bm_put, bm_pool->stats.stack_put, bm_pool->stats.stack_get);
#endif /* CONFIG_MV_ETH_STAT_DBG */

	if (bm_pool->stack)
		mvStackStatus(bm_pool->stack, 0);

	memset(&bm_pool->stats, 0, sizeof(bm_pool->stats));
}

/***********************************************************************************
 ***  print net device status
 ***********************************************************************************/
void mv_eth_netdev_print(struct net_device *dev)
{
	struct eth_netdev *dev_priv = MV_DEV_PRIV(dev);

	printk("%s net_device status: dev=%p, pp=%p\n\n", dev->name, dev, MV_ETH_PRIV(dev));
	printk("ifIdx=%d, features=0x%x, flags=0x%x, mtu=%u, size=%d, MAC=" MV_MACQUAD_FMT "\n",
	       dev->ifindex, (unsigned int)(dev->features), (unsigned int)(dev->flags),
	       dev->mtu, RX_PKT_SIZE(dev->mtu), MV_MACQUAD(dev->dev_addr));

	if (dev_priv)
		printk("tx_vlan_mh=0x%04x, switch_port_map=0x%x, switch_port_link_map=0x%x\n",
		       dev_priv->tx_vlan_mh, dev_priv->port_map, dev_priv->link_map);
}

void mv_eth_status_print(void)
{
	printk("totals: ports=%d, devs=%d\n", mv_eth_ports_num, mv_net_devs_num);

#ifdef CONFIG_MV_ETH_NFP
	printk("NFP         = %s\n", mv_ctrl_nfp ? "Enabled" : "Disabled");
#endif

#ifdef CONFIG_NET_SKB_RECYCLE
	printk("SKB recycle = %s\n", mv_ctrl_recycle ? "Enabled" : "Disabled");
#endif
}

/***********************************************************************************
 ***  print Ethernet port status
 ***********************************************************************************/
void mv_eth_port_status_print(unsigned int port)
{
	int txp, q;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (!pp) {
		return;
	}

	printk("\n");
	printk("port=%d, flags=0x%x\n", port, pp->flags);
	if ((!(pp->flags & MV_ETH_F_SWITCH)) && (pp->flags & MV_ETH_F_CONNECT_LINUX))
		printk("%s: ", pp->dev->name);
	else
		printk("port %d: ", port);

	mv_eth_link_status_print(port);

	printk("rxq_coal(pkts)[ q]   = ");
	for (q = 0; q < CONFIG_MV_ETH_RXQ; q++)
		printk("%3d ", mvNetaRxqPktsCoalGet(port, q));

	printk("\n");
	printk("rxq_coal(usec)[ q]   = ");
	for (q = 0; q < CONFIG_MV_ETH_RXQ; q++)
		printk("%3d ", mvNetaRxqTimeCoalGet(port, q));

	printk("\n");
	printk("rxq_desc(num)[ q]    = ");
	for (q = 0; q < CONFIG_MV_ETH_RXQ; q++)
		printk("%3d ", pp->rxq_ctrl[q].rxq_size);

	printk("\n");

	for (txp = 0; txp < pp->txp_num; txp++) {
		printk("txq_coal(pkts)[%2d.q] = ", txp);
		for (q = 0; q < CONFIG_MV_ETH_TXQ; q++) {
			printk("%3d ", mvNetaTxDonePktsCoalGet(port, txp, q));
		}
		printk("\n");

		printk("txq_mod(F,C,H)[%2d.q] = ", txp);
		for (q = 0; q < CONFIG_MV_ETH_TXQ; q++) {
			int val, mode;

			mode = mv_eth_ctrl_txq_mode_get(port, txp, q, &val);
			if (mode == MV_ETH_TXQ_CPU)
				printk(" C%-d ", val);
			else if (mode == MV_ETH_TXQ_HWF)
				printk(" H%-d ", val);
			else
				printk("  F ");
		}
		printk("\n");

		printk("txq_desc(num) [%2d.q] = ", txp);
		for (q = 0; q < CONFIG_MV_ETH_TXQ; q++) {
			struct tx_queue *txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + q];
			printk("%3d ", txq_ctrl->txq_size);
		}
		printk("\n");
	}
	printk("\n");

#ifdef CONFIG_MV_ETH_TXDONE_ISR
	printk("Do tx_done in NAPI context triggered by ISR\n");
	for (txp = 0; txp < pp->txp_num; txp++) {
		printk("txcoal(pkts)[%2d.q] = ", txp);
		for (q = 0; q < CONFIG_MV_ETH_TXQ; q++) {
			printk("%3d ", mvNetaTxDonePktsCoalGet(port, txp, q));
		}
		printk("\n");
	}
	printk("\n");
#else
	printk("Do tx_done in TX or Timer context: tx_done_threshold=%d\n", mv_ctrl_txdone);
#endif /* CONFIG_MV_ETH_TXDONE_ISR */

	printk("txp=%d, zero_pad=%s, mh_en=%s (0x%04x), tx_cmd=0x%08x\n",
	       pp->txp, (pp->flags & MV_ETH_F_NO_PAD) ? "Disabled" : "Enabled",
	       (pp->flags & MV_ETH_F_MH) ? "Enabled" : "Disabled", pp->tx_mh, pp->hw_cmd);

	printk("txq_def:");
	{
		int cpu;
		for_each_possible_cpu(cpu)
			printk(" cpu%d=>txq%d,", cpu, pp->txq[cpu]);
	}
	printk("\n");

#ifdef CONFIG_MV_ETH_SWITCH
	if (pp->flags & MV_ETH_F_SWITCH)
		mv_eth_switch_status_print(port);
#endif /* CONFIG_MV_ETH_SWITCH */
}

/***********************************************************************************
 ***  print port statistics
 ***********************************************************************************/

void mv_eth_port_stats_print(unsigned int port)
{
	struct eth_port *pp = mv_eth_port_by_id(port);
	struct port_stats *stat = NULL;
	int queue;

	TRC_OUTPUT();

	if (pp == NULL) {
		printk("eth_stats_print: wrong port number %d\n", port);
		return;
	}
	stat = &(pp->stats);

#ifdef CONFIG_MV_ETH_STAT_ERR
	printk("\n====================================================\n");
	printk("ethPort_%d: Errors", port);
	printk("\n-------------------------------\n");
	printk("rx_error......................%10u\n", stat->rx_error);
	printk("tx_timeout....................%10u\n", stat->tx_timeout);
	printk("tx_netif_stop.................%10u\n", stat->netif_stop);
	printk("netif_wake....................%10u\n", stat->netif_wake);
#endif /* CONFIG_MV_ETH_STAT_ERR */

	for (queue = 0; queue < CONFIG_MV_ETH_RXQ; queue++) {
		if (pp->rxq_ctrl[queue].missed > 0)
			printk("RXQ_%d:  %d missed\n", queue, pp->rxq_ctrl[queue].missed);
	}

#ifdef CONFIG_MV_ETH_STAT_INF
	printk("\n====================================================\n");
	printk("ethPort_%d: interrupt statistics", port);
	printk("\n-------------------------------\n");
	printk("irq...........................%10u\n", stat->irq);
	printk("irq_err.......................%10u\n", stat->irq_err);

	printk("\n====================================================\n");
	printk("ethPort_%d: Events", port);
	printk("\n-------------------------------\n");
	printk("poll..........................%10u\n", stat->poll);
	printk("poll_exit.....................%10u\n", stat->poll_exit);
	printk("tx_done_event.................%10u\n", stat->tx_done);
	printk("tx_done_timer_event...........%10u\n", stat->tx_done_timer);
	printk("cleanup_timer_event...........%10u\n", stat->cleanup_timer);
	printk("link..........................%10u\n", stat->link);
#ifdef CONFIG_MV_ETH_RX_SPECIAL
	printk("rx_special....................%10u\n", stat->rx_special);
#endif /* CONFIG_MV_ETH_RX_SPECIAL */
#ifdef CONFIG_MV_ETH_TX_SPECIAL
	printk("tx_special....................%10u\n", stat->tx_special);
#endif /* CONFIG_MV_ETH_TX_SPECIAL */
#endif /* CONFIG_MV_ETH_STAT_INF */

#ifdef CONFIG_MV_ETH_STAT_DBG
	{
		u32 total_rx_ok, total_rx_fill_ok;
		u32 txp;
		struct tx_queue *txq_ctrl;

		printk("\n====================================================\n");
		printk("ethPort_%d: Debug statistics", port);
		printk("\n-------------------------------\n");

		printk("\n");
		total_rx_ok = total_rx_fill_ok = 0;
		printk("RXQ:      rx_ok       rx_fill_ok\n\n");
		for (queue = 0; queue < CONFIG_MV_ETH_RXQ; queue++) {
			printk("%3d:  %10u  %10u\n", queue, stat->rxq[queue], stat->rxq_fill[queue]);
			total_rx_ok += stat->rxq[queue];
			total_rx_fill_ok += stat->rxq_fill[queue];
		}
		printk("SUM:  %10u  %10u\n", total_rx_ok, total_rx_fill_ok);

		printk("\n");

		printk("rx_nfp....................%10u\n", stat->rx_nfp);
		printk("rx_nfp_drop...............%10u\n", stat->rx_nfp_drop);

		printk("rx_gro....................%10u\n", stat->rx_gro);
		printk("rx_gro_bytes .............%10u\n", stat->rx_gro_bytes);

		printk("tx_tso....................%10u\n", stat->tx_tso);
		printk("tx_tso_bytes .............%10u\n", stat->tx_tso_bytes);

		printk("rx_netif..................%10u\n", stat->rx_netif);
		printk("rx_drop_sw................%10u\n", stat->rx_drop_sw);
		printk("rx_csum_hw................%10u\n", stat->rx_csum_hw);
		printk("rx_csum_sw................%10u\n", stat->rx_csum_sw);

		printk("\n");
		printk("TXP-TXQ:      count       send        done      no_resource\n\n");

		for (txp = 0; txp < pp->txp_num; txp++) {
			for (queue = 0; queue < CONFIG_MV_ETH_TXQ; queue++) {
				txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + queue];

				printk("%d-%d:  %10d  %10u  %10u  %10u\n",
				       txp, queue, txq_ctrl->txq_count, txq_ctrl->stats.txq_tx,
				       txq_ctrl->stats.txq_txdone, txq_ctrl->stats.txq_err);

				memset(&txq_ctrl->stats, 0, sizeof(txq_ctrl->stats));
			}
		}
		printk("\n");

		printk("tx_skb_free...............%10u\n", stat->tx_skb_free);
		printk("tx_sg.....................%10u\n", stat->tx_sg);
		printk("tx_csum_hw................%10u\n", stat->tx_csum_hw);
		printk("tx_csum_sw................%10u\n", stat->tx_csum_sw);
		printk("\n");
	}
#endif /* CONFIG_MV_ETH_STAT_DBG */
	printk("\n");

	memset(stat, 0, sizeof(struct port_stats));

	/* RX pool statistics */
#ifdef CONFIG_MV_ETH_BM
	if (pp->pool_short)
		mv_eth_pool_status_print(pp->pool_short->pool);
#endif /* CONFIG_MV_ETH_BM */
	if (pp->pool_long)
		mv_eth_pool_status_print(pp->pool_long->pool);

#ifdef CONFIG_MV_ETH_STAT_DIST
	{
		int i;
		struct dist_stats *dist_stats = &(pp->dist_stats);

		if (dist_stats->rx_dist) {
			printk("\n      Linux Path RX distribution\n");
			for (i = 0; i < dist_stats->rx_dist_size; i++) {
				if (dist_stats->rx_dist[i] != 0) {
					printk("%3d RxPkts - %u times\n", i, dist_stats->rx_dist[i]);
					dist_stats->rx_dist[i] = 0;
				}
			}
		}

		if (dist_stats->tx_done_dist) {
			printk("\n      tx-done distribution\n");
			for (i = 0; i < dist_stats->tx_done_dist_size; i++) {
				if (dist_stats->tx_done_dist[i] != 0) {
					printk("%3d TxDoneDesc - %u times\n", i, dist_stats->tx_done_dist[i]);
					dist_stats->tx_done_dist[i] = 0;
				}
			}
		}
#ifdef CONFIG_MV_ETH_TSO
		if (dist_stats->tx_tso_dist) {
			printk("\n      TSO stats\n");
			for (i = 0; i < dist_stats->tx_tso_dist_size; i++) {
				if (dist_stats->tx_tso_dist[i] != 0) {
					printk("%3d KBytes - %u times\n", i, dist_stats->tx_tso_dist[i]);
					dist_stats->tx_tso_dist[i] = 0;
				}
			}
		}
#endif /* CONFIG_MV_ETH_TSO */
	}
#endif /* CONFIG_MV_ETH_STAT_DIST */
}


static int mv_eth_port_cleanup(int port)
{
	int txp, txq, rxq, cpu;
	struct eth_port *pp;
	struct tx_queue *txq_ctrl;
	struct rx_queue *rxq_ctrl;

	pp = mv_eth_port_by_id(port);

	if (pp == NULL) {
		return -1;
	}

	if (pp->flags & MV_ETH_F_STARTED) {
		printk("%s: port %d is started, cannot cleanup\n", __func__, port);
		return -1;
	}

	/* Reset Tx ports */
	for (txp = 0; txp < pp->txp_num; txp++) {
		if (mv_eth_txp_reset(port, txp))
			printk("Warning: Port %d Tx port %d reset failed\n", port, txp);
	}

	/* Delete Tx queues */
	for (txp = 0; txp < pp->txp_num; txp++) {
		for (txq = 0; txq < CONFIG_MV_ETH_TXQ; txq++) {
			txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
			mv_eth_txq_delete(pp, txq_ctrl);
		}
	}

	mvOsFree(pp->txq_ctrl);
	pp->txq_ctrl = NULL;

#ifdef CONFIG_MV_ETH_STAT_DIST
	/* Free Tx Done distribution statistics */
	mvOsFree(pp->dist_stats.tx_done_dist);
#endif

	/* Reset RX ports */
	if (mv_eth_rx_reset(port))
		printk("Warning: Rx port %d reset failed\n", port);

	/* Delete Rx queues */
	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		rxq_ctrl = &pp->rxq_ctrl[rxq];
		mvNetaRxqDelete(pp->port, rxq);
		rxq_ctrl->q = NULL;
	}

	mvOsFree(pp->rxq_ctrl);
	pp->rxq_ctrl = NULL;

#ifdef CONFIG_MV_ETH_STAT_DIST
	/* Free Rx distribution statistics */
	mvOsFree(pp->dist_stats.rx_dist);
#endif

	/* Free buffer pools */
	if (pp->pool_long) {
		mv_eth_pool_free(pp->pool_long->pool, pp->pool_long_num);
		pp->pool_long = NULL;
	}
#ifdef CONFIG_MV_ETH_BM
	if (pp->pool_short) {
		mv_eth_pool_free(pp->pool_short->pool, pp->pool_short_num);
		pp->pool_short = NULL;
	}
#endif /* CONFIG_MV_ETH_BM */

	/* Clear Marvell Header related modes - will be set again if needed on re-init */
	mvNetaMhSet(port, MV_NETA_MH_NONE);
#ifdef CONFIG_MV_ETH_NFP
	mvNfpPortCapSet(port, NFP_P_MH, MV_FALSE);
#endif

	/* Clear any forced link, speed and duplex */
	mv_force_port_link_speed_fc(port, MV_ETH_SPEED_AN, 0);

	mvNetaPortDestroy(port);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	if (pp->flags & MV_ETH_F_CONNECT_LINUX) {
		for_each_possible_cpu(cpu) {
			netif_napi_del(&pp->napi[cpu]);
		}
	}
#endif

	return 0;
}


static int mv_eth_all_ports_cleanup(void)
{
	int port, pool, status = 0;

	for (port = 0; port < mv_eth_ports_num; port++) {
		status = mv_eth_port_cleanup(port);
		if (status != 0) {
			printk("Error: mv_eth_port_cleanup failed on port %d, stopping all ports cleanup\n", port);
			return status;
		}
	}

	for (pool = 0; pool < MV_ETH_BM_POOLS; pool++) {
		mv_eth_pool_destroy(pool);
	}

	for (port = 0; port < mv_eth_ports_num; port++) {
		if (mv_eth_ports[port])
			mvOsFree(mv_eth_ports[port]);
	}

	memset(mv_eth_ports, 0, (mv_eth_ports_num * sizeof(struct eth_port *)));
	/* Note: not freeing mv_eth_ports - we will reuse them */

	return 0;
}


#ifdef WAN_SWAP_FEATURE
/* WAN swap feature */

static void mv_eth_swap_mtu_values(void)
{
	/* Note: this code assumes there are two ports */
	u16 tmp = mvMtu[0];
	mvMtu[0] = mvMtu[1];
	mvMtu[1] = tmp;
}

static void mv_eth_swap_mac_values(void)
{
	/* Note: this code assumes there are two ports */
	u8 tmp_mac[MV_MAC_ADDR_SIZE];
	memcpy(tmp_mac, mvMacAddr[0], MV_MAC_ADDR_SIZE);
	memcpy(mvMacAddr[0], mvMacAddr[1], MV_MAC_ADDR_SIZE);
	memcpy(mvMacAddr[1], tmp_mac, MV_MAC_ADDR_SIZE);
}

int mv_eth_check_all_ports_down(void)
{
	int port;
	struct eth_port *pp;

	for (port = 0; port < mv_eth_ports_num; port++) {
		pp = mv_eth_port_by_id(port);

		if (pp == NULL) {
			printk("%s: got NULL port (%d)\n", __func__, port);
			return -1;
		}

		if (pp->flags & MV_ETH_F_STARTED) {
			printk("%s: port %d is started, cannot swap WAN mode\n", __func__, port);
			return -1;
		}
	}

	return 0;
}

int mv_eth_ctrl_wan_mode(int wan_mode)
{
	int port;

	/* WAN mode: 0 - MoCA, 1 - GbE */

	aei_led_off();

	printk("WAN swap requested: new WAN mode is ");
	if (wan_mode == MV_WAN_MODE_MOCA)
		printk("MoCA\n");
	else
		printk("GbE\n");

	if (mv_eth_check_all_ports_down())
		return -1;

	for (port = 0; port < mv_eth_ports_num; port++)
		mv_eth_port_filtering_cleanup(port);

	if (mv_eth_all_ports_cleanup())
		printk("Error: mv_eth_all_ports_cleanup failed\n");

	mv_eth_swap_mtu_values();

	mv_eth_swap_mac_values();

#ifdef CONFIG_MV_ETH_NFP
	if (mv_eth_unregister_nfp_devices())
		printk("Warning: un-registering NFP devices failed\n");
#endif

#ifdef CONFIG_MV_INCLUDE_SWITCH
	if ((mvBoardSwitchConnectedPortGet(0) != -1) || (mvBoardSwitchConnectedPortGet(1) != -1)) {
		if (mv_switch_unload(SWITCH_CONNECTED_PORTS_MASK))
			printk("Warning: Switch unload failed\n");
	}
#endif /* CONFIG_MV_INCLUDE_SWITCH */

	g_wan_mode = wan_mode;

	if (mv_eth_init_module())
		printk("Error: re-initialization of Marvell Ethernet Driver failed\n");


	return 0;	
}
#endif /* WAN_SWAP_FEATURE */

module_init(mv_eth_init_module);
MODULE_DESCRIPTION("Marvell Ethernet Driver - www.marvell.com");
MODULE_AUTHOR("Dmitri Epshtein <dima@marvell.com>");
MODULE_LICENSE("GPL");
