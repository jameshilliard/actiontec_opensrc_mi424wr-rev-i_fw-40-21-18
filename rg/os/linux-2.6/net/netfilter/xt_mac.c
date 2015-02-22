/* Kernel module to match MAC address parameters. */

/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter/xt_mac.h>
#include <linux/netfilter/x_tables.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Netfilter Core Team <coreteam@netfilter.org>");
MODULE_DESCRIPTION("iptables mac matching module");
MODULE_ALIAS("ipt_mac");
MODULE_ALIAS("ip6t_mac");

static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      unsigned int protoff,
      int *hotdrop)
{
    const struct xt_mac_info *info = matchinfo;

    /* Is mac pointer valid? */
    return (skb->mac.raw >= skb->head
	    && (skb->mac.raw + ETH_HLEN) <= skb->data
	    /* If so, compare... */
	    && ((!compare_ether_addr(eth_hdr(skb)->h_source, info->srcaddr))
		^ info->invert));
}

static int
ipt_mac_checkentry(const char *tablename,
		   const void *inf,
		   void *matchinfo,
		   unsigned int matchsize,
		   unsigned int hook_mask)
{
	/* FORWARD isn't always valid, but it's nice to be able to do --RR */
	if (hook_mask
	    & ~((1 << NF_IP_PRE_ROUTING) | (1 << NF_IP_LOCAL_IN)
		| (1 << NF_IP_FORWARD))) {
		printk("xt_mac: only valid for PRE_ROUTING, LOCAL_IN or FORWARD.\n");
		return 0;
	}

	if (matchsize != XT_ALIGN(sizeof(struct xt_mac_info)))
		return 0;

	return 1;
}

static struct xt_match mac_match = {
	.name		= "mac",
	.match		= &match,
	.checkentry	= &ipt_mac_checkentry,
	.me		= THIS_MODULE,
};
static struct xt_match mac6_match = {
	.name		= "mac",
	.match		= &match,
	.checkentry	= &ipt_mac_checkentry,
	.me		= THIS_MODULE,
};

static int __init init(void)
{
	int ret;
	ret = xt_register_match(AF_INET, &mac_match);
	if (ret)
		return ret;

	ret = xt_register_match(AF_INET6, &mac6_match);
	if (ret)
		xt_unregister_match(AF_INET, &mac_match);

	return ret;
}

static void __exit fini(void)
{
	xt_unregister_match(AF_INET, &mac_match);
	xt_unregister_match(AF_INET6, &mac6_match);
}

module_init(init);
module_exit(fini);
