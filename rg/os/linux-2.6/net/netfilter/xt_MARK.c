/* This is a module which is used for setting the NFMARK field of an skb. */

/* (C) 1999-2001 Marc Boucher <marc@mbsi.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/checksum.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_MARK.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marc Boucher <marc@mbsi.ca>");
MODULE_DESCRIPTION("ip[6]tables MARK modification module");
MODULE_ALIAS("ipt_MARK");
MODULE_ALIAS("ip6t_MARK");

static unsigned int
target_v0(struct sk_buff **pskb,
	  const struct net_device *in,
	  const struct net_device *out,
	  unsigned int hooknum,
	  const void *targinfo,
	  void *userinfo)
{
	const struct xt_mark_target_info *markinfo = targinfo;

	if((*pskb)->nfmark != markinfo->mark)
		(*pskb)->nfmark = markinfo->mark;

	return XT_CONTINUE;
}

static unsigned int
target_v1(struct sk_buff **pskb,
	  const struct net_device *in,
	  const struct net_device *out,
	  unsigned int hooknum,
	  const void *targinfo,
	  void *userinfo)
{
	const struct xt_mark_target_info_v1 *markinfo = targinfo;
	int mark = 0;

	switch (markinfo->mode) {
	case XT_MARK_SET:
		mark = markinfo->mark;
		break;
		
	case XT_MARK_AND:
		mark = (*pskb)->nfmark & markinfo->mark;
		break;
		
	case XT_MARK_OR:
		mark = (*pskb)->nfmark | markinfo->mark;
		break;
	}

	if((*pskb)->nfmark != mark)
		(*pskb)->nfmark = mark;

	return XT_CONTINUE;
}


static int
checkentry_v0(const char *tablename,
	      const void *entry,
	      void *targinfo,
	      unsigned int targinfosize,
	      unsigned int hook_mask)
{
	struct xt_mark_target_info *markinfo = targinfo;

	if (targinfosize != XT_ALIGN(sizeof(struct xt_mark_target_info))) {
		printk(KERN_WARNING "MARK: targinfosize %u != %Zu\n",
		       targinfosize,
		       XT_ALIGN(sizeof(struct xt_mark_target_info)));
		return 0;
	}

	if (strcmp(tablename, "mangle") != 0) {
		printk(KERN_WARNING "MARK: can only be called from \"mangle\" table, not \"%s\"\n", tablename);
		return 0;
	}

	if (markinfo->mark > 0xffffffff) {
		printk(KERN_WARNING "MARK: Only supports 32bit wide mark\n");
		return 0;
	}

	return 1;
}

static int
checkentry_v1(const char *tablename,
	      const void *entry,
	      void *targinfo,
	      unsigned int targinfosize,
	      unsigned int hook_mask)
{
	struct xt_mark_target_info_v1 *markinfo = targinfo;

	if (targinfosize != XT_ALIGN(sizeof(struct xt_mark_target_info_v1))){
		printk(KERN_WARNING "MARK: targinfosize %u != %Zu\n",
		       targinfosize,
		       XT_ALIGN(sizeof(struct xt_mark_target_info_v1)));
		return 0;
	}

	if (strcmp(tablename, "mangle") != 0) {
		printk(KERN_WARNING "MARK: can only be called from \"mangle\" table, not \"%s\"\n", tablename);
		return 0;
	}

	if (markinfo->mode != XT_MARK_SET
	    && markinfo->mode != XT_MARK_AND
	    && markinfo->mode != XT_MARK_OR) {
		printk(KERN_WARNING "MARK: unknown mode %u\n",
		       markinfo->mode);
		return 0;
	}

	if (markinfo->mark > 0xffffffff) {
		printk(KERN_WARNING "MARK: Only supports 32bit wide mark\n");
		return 0;
	}

	return 1;
}

static struct xt_target ipt_mark_reg_v0 = {
	.name		= "MARK",
	.target		= target_v0,
	.checkentry	= checkentry_v0,
	.me		= THIS_MODULE,
	.revision	= 0,
};

static struct xt_target ipt_mark_reg_v1 = {
	.name		= "MARK",
	.target		= target_v1,
	.checkentry	= checkentry_v1,
	.me		= THIS_MODULE,
	.revision	= 1,
};

static struct xt_target ip6t_mark_reg_v0 = {
	.name		= "MARK",
	.target		= target_v0,
	.checkentry	= checkentry_v0,
	.me		= THIS_MODULE,
	.revision	= 0,
};

static int __init init(void)
{
	int err;

	err = xt_register_target(AF_INET, &ipt_mark_reg_v0);
	if (err)
		return err;

	err = xt_register_target(AF_INET, &ipt_mark_reg_v1);
	if (err)
		xt_unregister_target(AF_INET, &ipt_mark_reg_v0);

	err = xt_register_target(AF_INET6, &ip6t_mark_reg_v0);
	if (err) {
		xt_unregister_target(AF_INET, &ipt_mark_reg_v0);
		xt_unregister_target(AF_INET, &ipt_mark_reg_v1);
	}

	return err;
}

static void __exit fini(void)
{
	xt_unregister_target(AF_INET, &ipt_mark_reg_v0);
	xt_unregister_target(AF_INET, &ipt_mark_reg_v1);
	xt_unregister_target(AF_INET6, &ip6t_mark_reg_v0);
}

module_init(init);
module_exit(fini);
