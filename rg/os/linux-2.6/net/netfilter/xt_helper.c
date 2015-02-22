/* iptables module to match on related connections */
/*
 * (C) 2001 Martin Josefsson <gandalf@wlug.westbo.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *   19 Mar 2002 Harald Welte <laforge@gnumonks.org>:
 *   		 - Port to newnat infrastructure
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#if defined(CONFIG_IP_NF_CONNTRACK) || defined(CONFIG_IP_NF_CONNTRACK_MODULE)
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#else
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_helper.h>
#endif
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_helper.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Josefsson <gandalf@netfilter.org>");
MODULE_DESCRIPTION("iptables helper match module");
MODULE_ALIAS("ipt_helper");
MODULE_ALIAS("ip6t_helper");

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#if defined(CONFIG_IP_NF_CONNTRACK) || defined(CONFIG_IP_NF_CONNTRACK_MODULE)
static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      unsigned int protoff,
      int *hotdrop)
{
	const struct xt_helper_info *info = matchinfo;
	struct ip_conntrack *ct;
	enum ip_conntrack_info ctinfo;
	int ret = info->invert;
	
	ct = ip_conntrack_get((struct sk_buff *)skb, &ctinfo);
	if (!ct) {
		DEBUGP("xt_helper: Eek! invalid conntrack?\n");
		return ret;
	}

	if (!ct->master) {
		DEBUGP("xt_helper: conntrack %p has no master\n", ct);
		return ret;
	}

	read_lock_bh(&ip_conntrack_lock);
	if (!ct->master->helper) {
		DEBUGP("xt_helper: master ct %p has no helper\n", 
			exp->expectant);
		goto out_unlock;
	}

	DEBUGP("master's name = %s , info->name = %s\n", 
		ct->master->helper->name, info->name);

	if (info->name[0] == '\0')
		ret ^= 1;
	else
		ret ^= !strncmp(ct->master->helper->name, info->name, 
		                strlen(ct->master->helper->name));
out_unlock:
	read_unlock_bh(&ip_conntrack_lock);
	return ret;
}

#else /* CONFIG_IP_NF_CONNTRACK */

static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      unsigned int protoff,
      int *hotdrop)
{
	const struct xt_helper_info *info = matchinfo;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	int ret = info->invert;
	
	ct = nf_ct_get((struct sk_buff *)skb, &ctinfo);
	if (!ct) {
		DEBUGP("xt_helper: Eek! invalid conntrack?\n");
		return ret;
	}

	if (!ct->master) {
		DEBUGP("xt_helper: conntrack %p has no master\n", ct);
		return ret;
	}

	read_lock_bh(&nf_conntrack_lock);
	if (!ct->master->helper) {
		DEBUGP("xt_helper: master ct %p has no helper\n", 
			exp->expectant);
		goto out_unlock;
	}

	DEBUGP("master's name = %s , info->name = %s\n", 
		ct->master->helper->name, info->name);

	if (info->name[0] == '\0')
		ret ^= 1;
	else
		ret ^= !strncmp(ct->master->helper->name, info->name, 
		                strlen(ct->master->helper->name));
out_unlock:
	read_unlock_bh(&nf_conntrack_lock);
	return ret;
}
#endif

static int check(const char *tablename,
		 const void *inf,
		 void *matchinfo,
		 unsigned int matchsize,
		 unsigned int hook_mask)
{
	struct xt_helper_info *info = matchinfo;

	info->name[29] = '\0';

	/* verify size */
	if (matchsize != XT_ALIGN(sizeof(struct xt_helper_info)))
		return 0;

	return 1;
}

static struct xt_match helper_match = {
	.name		= "helper",
	.match		= &match,
	.checkentry	= &check,
	.me		= THIS_MODULE,
};
static struct xt_match helper6_match = {
	.name		= "helper",
	.match		= &match,
	.checkentry	= &check,
	.me		= THIS_MODULE,
};

static int __init init(void)
{
	int ret;
	need_conntrack();

	ret = xt_register_match(AF_INET, &helper_match);
	if (ret < 0)
		return ret;

	ret = xt_register_match(AF_INET6, &helper6_match);
	if (ret < 0)
		xt_unregister_match(AF_INET, &helper_match);

	return ret;
}

static void __exit fini(void)
{
	xt_unregister_match(AF_INET, &helper_match);
	xt_unregister_match(AF_INET6, &helper6_match);
}

module_init(init);
module_exit(fini);

