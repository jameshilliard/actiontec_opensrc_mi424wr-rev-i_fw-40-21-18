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
* mv_nfp_mgr.c - Marvell Network Fast Processing Manager
*
* DESCRIPTION:
*
*       Supported Features:
*
*******************************************************************************/

#include "mvCommon.h"
#include "mvTypes.h"
#include <linux/kernel.h>
#include <linux/ip.h>
#include <net/arp.h>
#include <net/route.h>

#include "mvDebug.h"
#include "mvOs.h"
#include "mvSysHwConfig.h"
#include "ctrlEnv/mvCtrlEnvLib.h"
#include "gbe/mvNeta.h"
#include "nfp/mvNfp.h"
#include "net_dev/mv_netdev.h"
#include "mv_nfp_mgr.h"

#ifdef NFP_NAT
#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/netfilter/nf_nat.h>

static int nfp_nat_cap[MV_ETH_MAX_PORTS];

#endif

#include "net/ndisc.h"

//#define NFP_DBG(x...) printk(x)
#define NFP_DBG(x...)

//#define NFP_WARN(x...)  printk(x)
#define NFP_WARN(x...)

struct mgr_stats {
	MV_U32 add;
	MV_U32 del;
	MV_U32 age;
};


typedef struct nfp_netdev_map {
	struct nfp_netdev_map	*next;
	MV_NFP_IF_TYPE	        if_type;
	int	                if_index;
	struct net_device       *dev;
	struct eth_port         *pp;
} MV_NFP_NETDEV_MAP;


typedef struct nfp_ppp_rule_map {
	struct nfp_ppp_rule_map *next;
	MV_RULE_PPP rule_ppp;
} MV_NFP_PPP_RULE_MAP;

#ifdef CONFIG_MV_ETH_NFP_HWF
typedef struct nfp_hwf_map
{
	int txp;
	int txq;

} NFP_HWF_MAP;

static NFP_HWF_MAP	nfp_hwf_txq_map[MV_ETH_MAX_PORTS][MV_ETH_MAX_PORTS];
static void     	nfp_fib_rule_hwf_set(struct eth_port *in_pp, struct eth_port *out_pp,
						NFP_RULE_FIB *rule);
#endif /* CONFIG_MV_ETH_NFP_HWF */


#define NFP_DEV_HASH_BITS	7
#define NFP_DEV_HASH_SZ		(1 << NFP_DEV_HASH_BITS)
#define NFP_DEV_HASH_MASK	(NFP_DEV_HASH_SZ - 1)

static MV_NFP_NETDEV_MAP** 	nfp_net_devs;

#ifdef CONFIG_MV_ETH_NFP_PPP
static MV_NFP_PPP_RULE_MAP** 	nfp_half_ppp_rules;
static MV_NFP_PPP_RULE_MAP** 	nfp_open_ppp_rules;
static int halfCurrentIndex=0;
static MV_NFP_PPP_RULE_MAP* nfp_ppp_rule_find(int if_index);
#endif /* CONFIG_MV_ETH_NFP_PPP */

static int          		nfp_enabled;
static spinlock_t   		nfp_lock;

static struct mgr_stats arp_stats;
static struct mgr_stats fib_stats;
static struct mgr_stats ipt_stats;
static struct mgr_stats nat_stats;
static struct mgr_stats swf_stats;


int __init nfp_mgr_init(void)
{
	nfp_net_devs = (MV_NFP_NETDEV_MAP**)kmalloc(sizeof(MV_NFP_NETDEV_MAP*) * NFP_DEV_HASH_SZ, GFP_ATOMIC);
	if (nfp_net_devs == NULL) {
		printk("nfp_mgr_init: Error allocating memory for NFP Manager Interface Database\n");
		return -ENOMEM;
	}
	memset(nfp_net_devs, 0, sizeof(MV_NFP_NETDEV_MAP*) * NFP_DEV_HASH_SZ);

	memset(&arp_stats, 0, sizeof(struct mgr_stats));
	memset(&fib_stats, 0, sizeof(struct mgr_stats));
	memset(&ipt_stats, 0, sizeof(struct mgr_stats));
	memset(&nat_stats, 0, sizeof(struct mgr_stats));
	memset(&swf_stats, 0, sizeof(struct mgr_stats));

#ifdef CONFIG_MV_ETH_NFP_HWF
	memset(nfp_hwf_txq_map, 0xFF, sizeof(nfp_hwf_txq_map));
#endif /* CONFIG_MV_ETH_NFP_HWF */

#ifdef NFP_NAT
	memset(nfp_nat_cap, 0, sizeof(nfp_nat_cap));
#endif /* NFP_NAT */

  	spin_lock_init(&nfp_lock);

	mvNfpInit();
#ifdef NFP_FIB
	mvNfpFibInit();
#endif
#ifdef NFP_NAT
	mvNfpNatInit();
#endif
#ifdef NFP_SWF
	mvNfpSwfInit();
#endif
#ifdef NFP_PNC
	mvNfpPncInit();
#endif

#ifdef CONFIG_MV_ETH_NFP_PMT
	mvNfpPmtInit();
#endif /* CONFIG_MV_ETH_NFP_PMT */

    return 0;
}

module_init(nfp_mgr_init);


void nfp_eth_dev_db_clear(void)
{
	int                 i = 0;
	MV_NFP_NETDEV_MAP   *curr_dev = NULL, *tmp_dev = NULL;

	for (i = 0; i < NFP_DEV_HASH_SZ; i++) {
		curr_dev = nfp_net_devs[i];
		while (curr_dev != NULL) {
			tmp_dev = curr_dev;
			curr_dev = curr_dev->next;
			kfree(tmp_dev);
		}
		nfp_net_devs[i] = NULL;
	}
}


static void __exit nfp_mgr_exit(void)
{
	nfp_eth_dev_db_clear();
	if (nfp_net_devs)
		kfree(nfp_net_devs);
#ifdef NFP_FIB
	mvNfpFibClean();
	mvNfpFibDestroy();
#endif

#ifdef NFP_NAT
	mvNfpNatClean();
	mvNfpNatDestroy();
#endif
	/* TODO: add clean and destroy for SWF, PNC etc. */
#ifdef NFP_PNC
	mvNfpPncDestroy();
#endif
}

module_exit(nfp_mgr_exit);

static MV_NFP_NETDEV_MAP* nfp_eth_dev_find(int if_index)
{
	int                 h_index = 0;
	MV_NFP_NETDEV_MAP*  curr_dev = NULL;

	/* sanity checks */
	if((nfp_net_devs == NULL) || (if_index <= 0))
		return NULL;

	h_index = (if_index & NFP_DEV_HASH_MASK);

	for (curr_dev = nfp_net_devs[h_index]; curr_dev != NULL; curr_dev = curr_dev->next) {
		if (curr_dev->if_index == if_index)
			return curr_dev;
	}
	return NULL;
}

/* Register a network interface that works with NFP */
int nfp_mgr_if_register(int if_index, MV_NFP_IF_TYPE if_type, struct net_device* dev,
						struct eth_port *pp)
{
	int                 h_index = 0;
	MV_NFP_NETDEV_MAP   *new_dev = NULL, *curr_dev = NULL;

	/* sanity checks */
	if ((nfp_net_devs == NULL)	||
		(if_index <= 0)		||
		(nfp_eth_dev_find(if_index) != NULL))
	{
		printk("nfp_eth_dev_find() did not find device: if_index=%d  %s\n",if_index,__FUNCTION__);
		return -1;
	}
	h_index = (if_index & NFP_DEV_HASH_MASK);

	new_dev = kmalloc(sizeof(MV_NFP_NETDEV_MAP), GFP_ATOMIC);
	if (new_dev == NULL)
		return -ENOMEM;

	new_dev->pp       = pp;
	new_dev->next     = NULL;
	new_dev->if_type  = if_type;
	new_dev->if_index = if_index;
	new_dev->dev      = dev;

	if (nfp_net_devs[h_index] == NULL)
	{
		nfp_net_devs[h_index] = new_dev;
	}
	else
	{
		for (curr_dev = nfp_net_devs[h_index]; curr_dev->next != NULL; curr_dev = curr_dev->next);
		{
			curr_dev->next = new_dev;
		}
	}

	return 0;
}


int nfp_mgr_if_unregister(int if_index)
{
	int                 h_index = 0;
	MV_NFP_NETDEV_MAP   *curr_dev = NULL, *prev_dev = NULL;

	if ((nfp_net_devs == NULL) || (if_index <=0))
		return -1;

	h_index = (if_index & NFP_DEV_HASH_MASK);

	for (curr_dev = nfp_net_devs[h_index]; curr_dev != NULL; prev_dev = curr_dev, curr_dev = curr_dev->next) {
		if (curr_dev->if_index == if_index) {
			if (prev_dev == NULL)
				nfp_net_devs[h_index] = curr_dev->next;
			else
				prev_dev->next = curr_dev->next;

			kfree(curr_dev);
			return 0;
		}
	}

	return 0;
}


static int nfp_port_get(int if_index)
{
    MV_NFP_NETDEV_MAP   *pdev = nfp_eth_dev_find(if_index);

	if (pdev == NULL)
	{
		NFP_DBG("nfp_eth_dev_find returned NULL in %s\n",__FUNCTION__);
		return -1;
	}
	return pdev->pp->port;
}

static int is_valid_index(int if_index)
{
	if (nfp_eth_dev_find(if_index) != NULL)
		return 1;

	return 0;
}


/* NFP on/off */
int nfp_on(void)
{
	return nfp_enabled;
}

#ifdef CONFIG_MV_ETH_NFP_HWF
int		nfp_hwf_txq_set(int rxp, int port, int txp, int txq)
{
	if (mvNetaPortCheck(rxp))
        return 1;

	if (mvNetaTxpCheck(port, txp))
        return 1;

    if(mvNetaMaxCheck(txq, CONFIG_MV_ETH_TXQ))
        return 1;

	nfp_hwf_txq_map[rxp][port].txp = txp;
	nfp_hwf_txq_map[rxp][port].txq = txq;

	return 0;
}

int 	nfp_hwf_txq_get(int rxp, int port, int *txp, int *txq)
{
	if (mvNetaPortCheck(rxp))
        return 1;

	if (mvNetaPortCheck(port))
        return 1;

	if(txp)
		*txp = nfp_hwf_txq_map[rxp][port].txp;

	if(txq)
		*txq = nfp_hwf_txq_map[rxp][port].txq;

	return 0;
}

static void	nfp_fib_rule_hwf_set(struct eth_port *in_pp, struct eth_port *out_pp,
				NFP_RULE_FIB *rule)				
{
	int	hwf_txp, hwf_txq;

#ifdef NFP_NAT
	/* Temporary HWF supported only for IPv4 without NAT - 2 tuple rules */
	if (!nfp_nat_cap[out_pp->port] && !nfp_nat_cap[in_pp->port])
			return;
#endif /* NFP_NAT */

	if( nfp_hwf_txq_get(in_pp->port, out_pp->port, &hwf_txp, &hwf_txq) )
		return;

	if( (hwf_txq != -1) && (hwf_txp != -1) )
	{
		rule->flags |= NFP_F_HWF;
		rule->hwf_txp = (MV_U8)hwf_txp;
		rule->hwf_txq = (MV_U8)hwf_txq;
	}
}
#endif /* CONFIG_MV_ETH_NFP_HWF */


#ifdef NFP_FIB
int nfp_arp_is_confirmed(int family, const u8 *ip)
{
	NFP_RULE_FIB	rule;
	unsigned long	flags;

	if (!nfp_enabled)
		return 0;

	l3_addr_copy(family, rule.defGtwL3, ip);

	spin_lock_irqsave(&nfp_lock, flags);
	mvNfpFibRuleArpAge(family, &rule);
	arp_stats.age++;
	spin_unlock_irqrestore(&nfp_lock, flags);

	return rule.age;
}

/* Delete neighbour for IP */
int nfp_arp_delete(int family, const u8 *ip)
{
	NFP_RULE_FIB	rule;
	unsigned long	flags;

	if (!nfp_enabled)
		return 1;

	l3_addr_copy(family, rule.defGtwL3, ip);

	spin_lock_irqsave(&nfp_lock, flags);
	mvNfpFibRuleArpDel(family, &rule);
	arp_stats.del++;
	spin_unlock_irqrestore(&nfp_lock, flags);

	return 0;
}

int nfp_arp_add(int family, int if_index, const u8 *ip, u8 *mac)
{
	NFP_RULE_FIB	rule;
	unsigned long	flags;

	if (!nfp_enabled)
		return 1;

	if (!is_valid_index(if_index))
		return 1;

	NFP_DBG("%s: %s, if=%d, "MV_MACQUAD_FMT"\n",
		        __FUNCTION__, (family == MV_INET) ? "IPv4" : "IPv6", if_index, MV_MACQUAD(mac));

	memcpy(rule.da, mac, 6);

	l3_addr_copy(family, rule.defGtwL3, ip);
    spin_lock_irqsave(&nfp_lock, flags);
	mvNfpFibRuleArpAdd(family, &rule);
	arp_stats.add++;
	spin_unlock_irqrestore(&nfp_lock, flags);

	return 0;
}


static int nfp_fib_arp_query(int family, unsigned char *haddr, u8 *paddrL3,
							struct net_device *dev)
{
	struct neighbour	*neighbor_entry;
	int					ret = 1;
	MV_U32				paddr32 = *((u32 *)paddrL3);


	NFP_DBG("%s: %s, if=%s\n",
		        __FUNCTION__, (family == MV_INET) ? "IPv4" : "IPv6", dev->name);

	if (family == MV_INET)
	{
		neighbor_entry = neigh_lookup(&arp_tbl, &paddr32 , dev);
	}
	else
	{
#ifdef CONFIG_IPV6
		neighbor_entry = neigh_lookup(&nd_tbl, paddrL3, dev);
#else
		printk("your kernel was built without IPV6 support! (check CONFIG_IPV6) %s\n",__FUNCTION__);
		return 1;
#endif
	}

	if (neighbor_entry != NULL) {
		neighbor_entry->used = jiffies;
		if (neighbor_entry->nud_state & NUD_VALID) {
			memcpy(haddr, neighbor_entry->ha, dev->addr_len);
			ret = 0;
		}
		else
			ret = neighbor_entry->nud_state;

		neigh_release(neighbor_entry);
	}

/*
	if(ret)
		printk("%s Failed: ret=%d, if=%s\n", __FUNCTION__, ret, dev->name);
	else
		printk("%s Found: ret=%d, "MV_MACQUAD_FMT", if=%s\n",
			__FUNCTION__, ret, MV_MACQUAD(haddr), dev->name);
*/
	return ret;
}




void nfp_fib_rule_add(int family, u8 *src_l3, u8 *dst_l3,
				      u8 *gw, int iif, int oif)
{
	NFP_RULE_FIB        rule;
	struct net_device   *in_dev, *out_dev;
	struct eth_port     *out_pp;
	struct eth_port     *in_pp;
	struct eth_netdev*  dev_priv = NULL;
	unsigned long       flags;
	MV_NFP_NETDEV_MAP   *pdevOut = NULL;
	MV_NFP_NETDEV_MAP   *pdevIn	= NULL;

  	if (!nfp_enabled)
		return;

	NFP_DBG("%s - iif=%d oif=%d in %s\n",
			(family == MV_INET) ? "IPv4" : "IPv6", iif, oif, __FUNCTION__);

    pdevIn = nfp_eth_dev_find(iif);
    if(pdevIn == NULL)
    {
		NFP_DBG("%d invalid index for iif in %s\n", iif, __FUNCTION__);

		return;
    }
    in_dev = pdevIn->dev;
    in_pp = pdevIn->pp;

    pdevOut = nfp_eth_dev_find(oif);
	if (pdevOut == NULL)
    {
		NFP_DBG("%d invalid index for oif in %s\n", oif, __FUNCTION__);
		return;
    }
    out_dev = pdevOut->dev;
    out_pp = pdevOut->pp;

/*
	if (family == MV_INET)
		printk("%s: "MV_IPQUAD_FMT"->"MV_IPQUAD_FMT" iif=%d, oif=%d\n",
				__FUNCTION__, MV_IPQUAD(*((u32 *)src_l3)), MV_IPQUAD(*((u32 *)dst_l3)), iif, oif);
	else
		printk("%s: "MV_IP6_FMT"->"MV_IP6_FMT" iif=%d, oif=%d\n",
				__FUNCTION__, MV_IP6_ARG(src_l3), MV_IP6_ARG(src_l3), iif, oif);
*/
    memset(&rule, 0, sizeof(NFP_RULE_FIB));
    rule.flags = NFP_F_INV | NFP_F_DYNAMIC;

#ifdef CONFIG_MV_ETH_NFP_PPP
	if (pdevOut->if_type == MV_NFP_IF_PPP)
	{
		MV_NFP_PPP_RULE_MAP*    ruleMap = nfp_ppp_rule_find(oif);

		if (ruleMap)
		{
			out_dev =  ruleMap->rule_ppp.dev;
			rule.sid = ruleMap->rule_ppp.sid;
			memcpy(rule.da, ruleMap->rule_ppp.da, MV_MAC_ADDR_SIZE);
		}
		rule.flags &= ~NFP_F_INV;
		rule.flags |= NFP_F_PPPOE_ADD;
		NFP_DBG("setting NFP_F_PPPOE_ADD in %s\n", __FUNCTION__);
	}

	if (pdevIn->if_type == MV_NFP_IF_PPP)
	{
		MV_NFP_PPP_RULE_MAP*    ruleMap = nfp_ppp_rule_find(iif);

		if (ruleMap)
			in_dev = ruleMap->rule_ppp.dev;
		rule.flags |= NFP_F_PPPOE_REMOVE;
		NFP_DBG("setting NFP_F_PPPOE_REMOVE in %s\n", __FUNCTION__);
	}
#endif /* CONFIG_MV_ETH_NFP_PPP */

	if( (in_dev->mtu != out_dev->mtu) ||
		(out_dev->mtu > MV_ETH_TX_CSUM_MAX_SIZE))
	{
		NFP_DBG("%s: Not adding FIB rule due to MTU\n", __FUNCTION__);
		return;
	}
	rule.outdev = out_dev;
	rule.outport = out_pp->port;
	rule.family = family;

	l3_addr_copy(family, rule.srcL3, src_l3);
	l3_addr_copy(family, rule.dstL3, dst_l3);
	l3_addr_copy(family, rule.defGtwL3, gw);

	dev_priv = MV_DEV_PRIV(out_dev);
	if (dev_priv)
		rule.mh = dev_priv->tx_vlan_mh;
	else
		rule.mh = out_pp->tx_mh;

	if ( (out_dev->dev_addr != NULL)
		&& (out_dev->addr_len == 6) )
		memcpy(rule.sa, out_dev->dev_addr, 6);

	if (pdevOut->if_type != MV_NFP_IF_PPP)
	{
		if (!nfp_fib_arp_query(family, rule.da, gw, out_dev))
			rule.flags &= ~NFP_F_INV;
	}

#ifdef CONFIG_MV_ETH_NFP_HWF
	/* enable PMT / HWF capabilities for IPv4 */
	if (family == MV_INET)	{
		nfp_fib_rule_hwf_set(in_pp, out_pp, &rule);
	}
#endif /* CONFIG_MV_ETH_NFP_HWF */

    spin_lock_irqsave(&nfp_lock, flags);
    if(mvNfpFibRuleAdd(family, &rule) == MV_OK)
     	fib_stats.add++;
    else
	{
        NFP_WARN("%s failed: %s, iif=%d, oif=%d\n",
		        __FUNCTION__, (family == MV_INET) ? "IPv4" : "IPv6", iif, oif);

/*
		if (family == MV_INET)
			printk("%s failed: "MV_IPQUAD_FMT"->"MV_IPQUAD_FMT" iif=%d, oif=%d\n",
					__FUNCTION__, MV_IPQUAD(*((u32 *)src_l3)), MV_IPQUAD(*((u32 *)dst_l3)), iif, oif);
		else
			printk("%s failed: "MV_IP6_FMT"->"MV_IP6_FMT" iif=%d, oif=%d\n",
					__FUNCTION__, MV_IP6_ARG(src_l3), MV_IP6_ARG(src_l3), iif, oif);
*/
	}

    spin_unlock_irqrestore(&nfp_lock, flags);
}



void nfp_fib_rule_del(int family, u8 *src_l3, u8 *dst_l3, int iif, int oif)
{
	NFP_RULE_FIB rule;
	unsigned long flags;

   	if (!nfp_enabled)
		return;

	if (!is_valid_index(iif))
		return;

	if (!is_valid_index(oif))
		return;

	NFP_DBG("%s: %s, iif=%d, oif=%d\n",
		        __FUNCTION__, (family == MV_INET) ? "IPv4" : "IPv6", iif, oif);

/*
	if (family == MV_INET)
		printk("%s: "MV_IPQUAD_FMT"->"MV_IPQUAD_FMT" iif=%d, oif=%d\n",
				__FUNCTION__, MV_IPQUAD(*((u32 *)src_l3)), MV_IPQUAD(*((u32 *)dst_l3)), iif, oif);
	else
		printk("%s: "MV_IP6_FMT"->"MV_IP6_FMT" iif=%d, oif=%d\n",
				__FUNCTION__, MV_IP6_ARG(src_l3), MV_IP6_ARG(src_l3), iif, oif);
*/
	memset(&rule, 0, sizeof(NFP_RULE_FIB));
	l3_addr_copy(family, rule.srcL3, src_l3);
	l3_addr_copy(family, rule.dstL3, dst_l3);

	rule.flags = NFP_F_DYNAMIC;

	spin_lock_irqsave(&nfp_lock, flags);
    	if (mvNfpFibRuleDel(family, &rule) == MV_OK) {
		fib_stats.del++;
		NFP_DBG("%s: %s, iif=%d, oif=%d\n", __func__, (family == MV_INET) ? "IPv4" : "IPv6", iif, oif);
	}
	else {
        	/* FIXME: hook should not call us in such case */
        	NFP_WARN("%s failed: %s, iif=%d, oif=%d\n",
		        __func__, (family == MV_INET) ? "IPv4" : "IPv6", iif, oif);
	}
	spin_unlock_irqrestore(&nfp_lock, flags);
}

int nfp_fib_rule_age(int family, u8 *src_l3, u8 *dst_l3, int iif, int oif)
{
	NFP_RULE_FIB rule;
	unsigned long flags;

	if (!nfp_enabled)
		return 0;

	if (!is_valid_index(iif))
		return 0;

	if (!is_valid_index(oif))
		return 0;

	l3_addr_copy(family, rule.srcL3, src_l3);
	l3_addr_copy(family, rule.dstL3, dst_l3);
    rule.age = 0;

	spin_lock_irqsave(&nfp_lock, flags);

    if(mvNfpFibRuleAge(family, &rule) == MV_OK) {
      	fib_stats.age++;
	    NFP_DBG("%s: %s, age=%d\n",
                __FUNCTION__, (family == MV_INET) ? "IPv4" : "IPv6", rule.age);
    }
    else {
        /* FIXME: hook should not call us in such case*/
        NFP_WARN("%s failed: %s, iif=%d, oif=%d\n",
		        __FUNCTION__, (family == MV_INET) ? "IPv4" : "IPv6", iif, oif);
    }
	spin_unlock_irqrestore(&nfp_lock, flags);

	return rule.age;
}

#endif /* NFP_FIB */

#ifdef NFP_NAT
/* NAT on/off */
int nfp_nat_ct_on(void)
{
	return nfp_enabled;
}

/* add 5-tuple connection tracking */
int nfp_nat_ct_add(u32 sip, u32 dip, u16 sport, u16 dport, u8 proto,
			       u32 new_sip, u32 new_dip, u16 new_sport, u16 new_dport,
			       int ifindex, int maniptype)
{
	NFP_RULE_NAT rule;
	MV_STATUS status;
	unsigned long flags;

	if (!nfp_enabled)
		return 1;

	if (!is_valid_index(ifindex))
		return 1;

	memset(&rule, 0, sizeof(NFP_RULE_NAT));
	rule.sip = sip;
	rule.dip = dip;
	rule.ports = (dport << 16) | sport;
	rule.proto = proto;
	rule.nport = (maniptype == IP_NAT_MANIP_SRC) ? new_sport : new_dport;
	rule.nip = (maniptype == IP_NAT_MANIP_SRC) ? new_sip : new_dip;
	rule.flags = (maniptype == IP_NAT_MANIP_SRC) ? NFP_F_SNAT : NFP_F_DNAT;

    NFP_DBG("%s: "MV_IPQUAD_FMT" 0x%04x -> "MV_IPQUAD_FMT" 0x%04x, proto=%d, iif=%d, manip=%d\n",
		        __FUNCTION__, MV_IPQUAD(sip), sport, MV_IPQUAD(dip), dport, proto, ifindex, maniptype);
    NFP_DBG("new: "MV_IPQUAD_FMT" 0x%04x\n", MV_IPQUAD(rule.nip), rule.nport);

    spin_lock_irqsave(&nfp_lock, flags);
    status = mvNfpNatRuleAdd(&rule);
	if (status == MV_OK)
        nat_stats.add++;
    else
        printk("%s failed: "MV_IPQUAD_FMT" 0x%04x->"MV_IPQUAD_FMT" 0x%04x, proto=%d, iif=%d, manip=%d\n",
		        __FUNCTION__, MV_IPQUAD(sip), sport, MV_IPQUAD(dip), dport, proto, ifindex, maniptype);

    spin_unlock_irqrestore(&nfp_lock, flags);
	return status;
}

/* del 5-tuple connection tracking */
void nfp_nat_ct_del(u32 sip, u32 dip, u16 sport, u16 dport, u8 proto)
{
	NFP_RULE_NAT rule;
	unsigned long flags;

	if (!nfp_enabled)
		return;

	NFP_DBG("%s: %pI4:%hu -> %pI4:%hu\n", __FUNCTION__,
	       &sip, ntohs(sport), &dip, ntohs(dport));

	memset(&rule, 0, sizeof(NFP_RULE_NAT));
	rule.sip = sip;
	rule.dip = dip;
	rule.ports = (dport << 16) | sport;
	rule.proto = proto;
	rule.flags = NFP_F_DYNAMIC;

	spin_lock_irqsave(&nfp_lock, flags);

	if(mvNfpNatRuleDel(&rule) == MV_OK)
	    nat_stats.del++;
    else
        /* FIXME: hook should not call us in such case*/
        NFP_WARN("%s failed: "MV_IPQUAD_FMT" 0x%04x->"MV_IPQUAD_FMT" 0x%04x, proto=%d\n",
		        __FUNCTION__, MV_IPQUAD(sip), sport, MV_IPQUAD(dip), dport, proto);

	spin_unlock_irqrestore(&nfp_lock, flags);
}

int nfp_nat_ct_age(u32 sip, u32 dip, u16 sport, u16 dport, u8 proto)
{
	NFP_RULE_NAT    rule;
	unsigned long   flags;

	if (!nfp_enabled)
		return 0;

	NFP_DBG("%s: %pI4:%hu -> %pI4:%hu\n", __FUNCTION__,
	       &sip, ntohs(sport), &dip, ntohs(dport));

	memset(&rule, 0, sizeof(NFP_RULE_NAT));
	rule.sip = sip;
	rule.dip = dip;
	rule.ports = (dport << 16) | sport;
	rule.proto = proto;

	spin_lock_irqsave(&nfp_lock, flags);
	if(mvNfpNatRuleAge(&rule) == MV_OK)
		nat_stats.age++;
	else
		/* FIXME: hook should not call us in such case*/
		NFP_WARN("%s failed: "MV_IPQUAD_FMT":%d->"MV_IPQUAD_FMT":%d, proto=%d\n",
			__func__, MV_IPQUAD(sip), MV_16BIT_BE(sport), MV_IPQUAD(dip), MV_16BIT_BE(dport), proto);

	spin_unlock_irqrestore(&nfp_lock, flags);

	return rule.age;
}

/* admit masquerading on a port */
void nfp_ipt_iface(struct ipt_entry* e, int onoff)
{
	struct net_device *dev;
	struct ipt_entry_target *t = ipt_get_target(e);
	int port;

	/* set NAT nat_port_cap even if NFP is disabled */

	if (!t || strcmp(t->u.kernel.target->name, "MASQUERADE"))
		return;

	if (!e->ip.outiface[0]) {
		printk("%s: %s %s->%s is not supported\n", __FUNCTION__,
				t->u.kernel.target->name,
				e->ip.iniface,
				e->ip.outiface);
		return;
	}

	dev = __dev_get_by_name(&init_net, e->ip.outiface);
	if (dev == NULL)
		return;

	port = nfp_port_get(dev->ifindex);
	if (port < 0)
		return;

	if (onoff) {
		nfp_nat_cap[port]++;
		ipt_stats.add++;
	}
	else {
		nfp_nat_cap[port]--;
		ipt_stats.add++;
	}

	mvNfpPortCapSet(port, NFP_P_NAT, nfp_nat_cap[port]);
}
#endif /* NAT */


#ifdef NFP_SWF
int nfp_swf_rule_add(u32 flowid, u32 txp, u32 txq, u32 mh_sel)
{
	unsigned long   flags;
	NFP_RULE_SWF    rule;

	swf_stats.add++;

	memset(&rule, 0, sizeof(NFP_RULE_SWF));
	rule.flowid = flowid;
	rule.txp = txp;
	rule.txq = txq;
	rule.mh_sel = mh_sel;

	spin_lock_irqsave(&nfp_lock, flags);
	mvNfpSwfRuleAdd(&rule);
	spin_unlock_irqrestore(&nfp_lock, flags);

	return 0;
}

int nfp_swf_rule_del(u32 flowid)
{
	unsigned long   flags;
	NFP_RULE_SWF    rule;

    swf_stats.del++;

	memset(&rule, 0, sizeof(NFP_RULE_SWF));
	rule.flowid = flowid;

	spin_lock_irqsave(&nfp_lock, flags);
	mvNfpSwfRuleDel(&rule);
	spin_unlock_irqrestore(&nfp_lock, flags);

	return 0;
}
#endif /* NFP_SWF */

void nfp_mgr_enable(int en)
{
    if(!en)
    {
        /* NFP disabled: delete all rules */
#ifdef NFP_FIB
        mvNfpFibClean();
#endif
#ifdef NFP_NAT
        mvNfpNatClean();
#endif
    }
    else
    {
        /* NFP enabled: flush rt_cache */
        rt_cache_flush(&init_net, 0);
    }
	nfp_enabled = en;
}

void nfp_mgr_dump(void)
{
	unsigned long flags;

	spin_lock_irqsave(&nfp_lock, flags);

#ifdef NFP_FIB
	mvNfpFibDump();
#endif
#ifdef NFP_NAT
	mvNfpNatDump();
#endif
#ifdef NFP_SWF
	mvNfpSwfDump();
#endif
#ifdef NFP_PNC
	mvNfpPncDump();
#endif

	spin_unlock_irqrestore(&nfp_lock, flags);
}


void nfp_mgr_stats(void)
{
	int					i;
	MV_NFP_NETDEV_MAP	*curr_dev = NULL;
	u32					cap;

	printk("(interfaces)\n");
	for (i = 0; i < NFP_DEV_HASH_SZ; i++) {
		curr_dev = nfp_net_devs[i];
		while (curr_dev != NULL) {
			if (curr_dev->pp==NULL)
				continue;
			cap = mvNfpPortCapGet(curr_dev->pp->port);
			printk(" [%2d]: if=%d ", curr_dev->pp->port, curr_dev->if_index);
			if (cap & NFP_P_NAT)
				printk(" NAT");
			printk("\n");

			curr_dev = curr_dev->next;
		}
	}

#ifdef CONFIG_MV_ETH_NFP_HWF
	printk("HWF map: \n");
	printk("inp -> outp : txp  txq\n");
	for(i=0; i<MV_ETH_MAX_PORTS; i++)
	{
		int j;

		for (j=0; j<MV_ETH_MAX_PORTS; j++)
		{
			if(nfp_hwf_txq_map[i][j].txq != -1)
			{
				printk("  %d ->  %d   :  %d    %d\n", i, j,
						nfp_hwf_txq_map[i][j].txp, nfp_hwf_txq_map[i][j].txq);
			}
		}
	}
#endif /* CONFIG_MV_ETH_NFP_HWF */

	printk("(mgr)\n");
	printk("     add del age\n");

	printk("arp: %3d %3d %3d\n",
		arp_stats.add, arp_stats.del, arp_stats.age);
	printk("fib: %3d %3d %3d\n",
		fib_stats.add, fib_stats.del, fib_stats.age);
	printk("nat: %3d %3d %3d\n",
		nat_stats.add, nat_stats.del, nat_stats.age);
	printk("swf: %3d %3d %3d\n",
		swf_stats.add, swf_stats.del, swf_stats.age);
/*
	memset(&arp_stats, 0, sizeof(struct mgr_stats));
	memset(&fib_stats, 0, sizeof(struct mgr_stats));
	memset(&nat_stats, 0, sizeof(struct mgr_stats));
	memset(&swf_stats, 0, sizeof(struct mgr_stats));
*/
}


#ifdef CONFIG_MV_ETH_NFP_PPP

static MV_NFP_PPP_RULE_MAP* nfp_ppp_rule_find(int if_index)
{
	int                 h_index = 0;
	MV_NFP_PPP_RULE_MAP*  curr_rule = NULL;

	/* sanity checks */
	if((nfp_open_ppp_rules == NULL) || (if_index <= 0))
		return NULL;

	h_index = (if_index & NFP_DEV_HASH_MASK);

	for (curr_rule = nfp_open_ppp_rules[h_index]; curr_rule != NULL; curr_rule = curr_rule->next) {
		if (curr_rule->rule_ppp.iif == if_index)
			return curr_rule;
	}
	return NULL;
}

int nfp_ppp_info_set_half(u16 sid, u32 chan, struct net_device *eth_dev, char *remoteMac)
{
	MV_RULE_PPP rule;
	int                 h_index = 0;
	MV_NFP_PPP_RULE_MAP *new_rule = NULL, *curr_rule = NULL;

	memset(&rule, 0, sizeof(MV_RULE_PPP));

	rule.sid = sid;
	rule.dev = eth_dev;
	rule.chan = chan;
	if (remoteMac)
		memcpy(rule.da, remoteMac, 6);

	new_rule = kmalloc(sizeof(MV_NFP_PPP_RULE_MAP), GFP_ATOMIC);
	if (new_rule == NULL)
		return -ENOMEM;

	new_rule->rule_ppp = rule;
	h_index = (halfCurrentIndex & NFP_DEV_HASH_MASK);
	if (nfp_half_ppp_rules[h_index] == NULL)
		nfp_half_ppp_rules[h_index] = new_rule;
	else
	{
		curr_rule = nfp_half_ppp_rules[h_index];
		if (curr_rule->next==NULL)
		{
			curr_rule->next = new_rule;
		}
		else
		{
			for (curr_rule = nfp_half_ppp_rules[h_index]; curr_rule->next != NULL; curr_rule = curr_rule->next);
				{
				if (curr_rule != NULL)
					curr_rule->next = new_rule;
				}
		}
	}
	halfCurrentIndex++;
	return 0;
}


int nfp_ppp_info_set_complete(u32 chan, struct net_device *ppp_dev)
{
	int i = NFP_DEV_HASH_SZ;
	int res;
	int iif = ppp_dev->ifindex;
	struct eth_dev_priv* dev_priv;
	MV_NFP_PPP_RULE_MAP *curr_ppp_rule = NULL;

	while(i--)
	{
		curr_ppp_rule = nfp_half_ppp_rules[i];
		if (curr_ppp_rule == NULL)
			continue;

		if ((curr_ppp_rule != NULL) &&
			(curr_ppp_rule->rule_ppp.chan == chan))	{

			dev_priv = (struct eth_dev_priv*)netdev_priv(curr_ppp_rule->rule_ppp.dev);
			res = nfp_mgr_if_register(iif, MV_NFP_IF_PPP, ppp_dev, dev_priv->port_p);

			if (res != 0) {
				NFP_DBG("fp_mgr_if_register failed in %s\n",__FUNCTION__);
				return -1;
			}
			/* keep original iif of ppp device */
			curr_ppp_rule->rule_ppp.iif = iif;

			/* iif can grow beyond NFP_DEV_HASH_SZ */
			iif = iif & NFP_DEV_HASH_MASK;
			nfp_open_ppp_rules[iif] = curr_ppp_rule;

			break;
		} else {
			/* go to last entry ion the list */
			for (curr_ppp_rule = nfp_half_ppp_rules[i]; curr_ppp_rule->next != NULL; curr_ppp_rule = curr_ppp_rule->next)
				;

			if ((curr_ppp_rule != NULL) &&
				(curr_ppp_rule->rule_ppp.chan == chan)) {

				dev_priv = (struct eth_dev_priv*)netdev_priv(curr_ppp_rule->rule_ppp.dev);
				res = nfp_mgr_if_register(iif, MV_NFP_IF_PPP, ppp_dev, dev_priv->port_p);
				if (res != 0) {
					NFP_DBG("fp_mgr_if_register failed in %s\n",__FUNCTION__);
					return -1;
				}
				/* keep original iif of ppp device */
				curr_ppp_rule->rule_ppp.iif = iif;

				/* iif can grow beyond NFP_DEV_HASH_SZ */
				iif = iif & NFP_DEV_HASH_MASK;

				nfp_open_ppp_rules[iif]=curr_ppp_rule;

				break;
			}
		}	/* of else */
	} /* of while */
	return MV_OK;
}



int nfp_ppp_info_del(u32 channel)
{
	int                     i = NFP_DEV_HASH_SZ;
	MV_NFP_PPP_RULE_MAP	    *curr_rul;

	while(i--)
	{
		curr_rul = nfp_open_ppp_rules[i];
		if (!curr_rul)
			continue;

		if (curr_rul->rule_ppp.chan == channel)
		{
			nfp_mgr_if_unregister(i);
			break;
		}
		else
		{
			for (curr_rul = nfp_open_ppp_rules[i]; curr_rul != NULL; curr_rul = curr_rul->next)
			{
				if (curr_rul && (curr_rul->rule_ppp.chan == channel))
				{
					nfp_mgr_if_unregister(i);
					break;
				}
			}
		}
	} /* of while */

	return MV_OK;
}



/* Initialize Rule Database */
int nfp_ppp_db_init(void)
{
	NFP_DBG("FP_MGR: Initializing PPPoE Rule Database\n");
	nfp_half_ppp_rules = (MV_NFP_PPP_RULE_MAP**)kmalloc(sizeof(MV_NFP_PPP_RULE_MAP*) * NFP_DEV_HASH_SZ, GFP_ATOMIC);
	if (nfp_half_ppp_rules == NULL)
	{
		NFP_DBG("nfp_mgr_init: Error allocating memory for NFP Manager Half PPP rules Database\n");
		return -ENOMEM;
	}
	memset(nfp_half_ppp_rules, 0, sizeof(MV_NFP_PPP_RULE_MAP*) * NFP_DEV_HASH_SZ);

	nfp_open_ppp_rules = (MV_NFP_PPP_RULE_MAP**)kmalloc(sizeof(MV_NFP_PPP_RULE_MAP*) * NFP_DEV_HASH_SZ, GFP_ATOMIC);
	if (nfp_open_ppp_rules == NULL)
	{
		NFP_DBG("nfp_mgr_init: Error allocating memory for NFP Manager Half PPP rules Database\n");
		return -ENOMEM;
	}
	memset(nfp_open_ppp_rules, 0, sizeof(MV_NFP_PPP_RULE_MAP*) * NFP_DEV_HASH_SZ);


	return MV_OK;
}


/* Clear Fast Path PPP Rule Database */
int nfp_ppp_db_clear(void)
{
	unsigned long flags;

	spin_lock_irqsave(&nfp_lock, flags);
	memset(nfp_half_ppp_rules, 0, sizeof(MV_NFP_PPP_RULE_MAP) * NFP_DEV_HASH_SZ);
	memset(nfp_open_ppp_rules, 0, sizeof(MV_NFP_PPP_RULE_MAP*) * NFP_DEV_HASH_SZ);
	spin_unlock_irqrestore(&nfp_lock, flags);
	return MV_OK;

}
#endif /* CONFIG_MV_ETH_NFP_PPP */
