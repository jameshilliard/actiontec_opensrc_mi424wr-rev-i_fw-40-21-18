/* Kernel module to match NFMARK values. */

/* (C) 1999-2001 Marc Boucher <marc@mbsi.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>

#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter/x_tables.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marc Boucher <marc@mbsi.ca>");
MODULE_DESCRIPTION("iptables mark matching module");
MODULE_ALIAS("ipt_mark");
MODULE_ALIAS("ip6t_mark");

static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      unsigned int protoff,
      int *hotdrop)
{
	const struct xt_mark_info *info = matchinfo;

	return ((skb->nfmark & info->mask) == info->mark) ^ info->invert;
}

static int
checkentry(const char *tablename,
           const void *entry,
           void *matchinfo,
           unsigned int matchsize,
           unsigned int hook_mask)
{
	struct xt_mark_info *minfo = (struct xt_mark_info *) matchinfo;

	if (matchsize != XT_ALIGN(sizeof(struct xt_mark_info)))
		return 0;

	if (minfo->mark > 0xffffffff || minfo->mask > 0xffffffff) {
		printk(KERN_WARNING "mark: only supports 32bit mark\n");
		return 0;
	}

	return 1;
}

static struct xt_match mark_match = {
	.name		= "mark",
	.match		= &match,
	.checkentry	= &checkentry,
	.me		= THIS_MODULE,
};

static struct xt_match mark6_match = {
	.name		= "mark",
	.match		= &match,
	.checkentry	= &checkentry,
	.me		= THIS_MODULE,
};

static int __init init(void)
{
	int ret;
	ret = xt_register_match(AF_INET, &mark_match);
	if (ret)
		return ret;

	ret = xt_register_match(AF_INET6, &mark6_match);
	if (ret)
		xt_unregister_match(AF_INET, &mark_match);

	return ret;
}

static void __exit fini(void)
{
	xt_unregister_match(AF_INET, &mark_match);
	xt_unregister_match(AF_INET6, &mark6_match);
}

module_init(init);
module_exit(fini);
