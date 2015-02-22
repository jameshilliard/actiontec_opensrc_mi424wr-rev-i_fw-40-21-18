/*
 * linux/mm/debug.c
 *
 *  Copyright (C) 2001 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Dump out page information on SysRQ-G, and provide show_page_info()
 *
 *  You are advised to use a serial console with this patch - it
 *  saturates a 38400baud link for 1 minute on a 32MB machine.
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/sysrq.h>
#include <linux/init.h>

/*
 * We print out the following information for each page in the system:
 *	address: use count, age, mapping, [RSsr] rD [acd]
 *
 * The flags are:
 *	R - reserved
 *	S - in swapcache
 *	s - slab page
 *
 *	r - referenced
 *	D - dirty
 *
 *	a - active page
 */
static void page_detail(struct page *page)
{
	if (!page)
		return;

	printk("%p: %2d %p [%c%c%c] %c%c [%c]\n",
			page_address(page),
			page_count(page),
			page->mapping,

			PageReserved(page) ? 'R' : '-',
			PageSwapCache(page) ? 'S' : '-',
			PageSlab(page) ? 's' : '-',

			PageReferenced(page) ? 'r' : '-',
			PageDirty(page) ? 'D' : '-',

			PageActive(page) ? 'a' : '-');
}

/*
 * This version collects statistics
 */
static int anon_pages, slab_pages, resvd_pages, unused_pages;

static void page_statistics(struct page *page)
{
	if (page) {
		if (PageReserved(page))
			resvd_pages++;
		else if (PageSlab(page))
			slab_pages++;
		else if (!page_count(page))
			unused_pages ++;
		else if (!page->mapping)
			anon_pages ++;
		return;
	}

	printk("  anon: %d, slab: %d, reserved: %d, free: %d\n",
		anon_pages, slab_pages, resvd_pages, unused_pages);
}

static void show_zone_info(zone_t *zone, void (*fn)(struct page *))
{
	int i;

	printk("  total %ld, free %ld min %ld, low %ld, high %ld\n",
		zone->size, zone->free_pages, zone->pages_min,
		zone->pages_low, zone->pages_high);

	anon_pages    = 0;
	slab_pages    = 0;
	resvd_pages   = 0;
	unused_pages  = 0;

	for (i = 0; i < zone->size; i++) {
		struct page *page = zone->zone_mem_map + i;

		fn(page);
	}

	fn(NULL);
}

static void show_node_info(pg_data_t *pg, void (*fn)(struct page *))
{
	int type;

	for (type = 0; type < MAX_NR_ZONES; type++) {
		zone_t *zone = pg->node_zones + type;

		if (zone->size == 0)
			continue;

		printk("----- Zone %d ------\n", type);

		show_zone_info(zone, fn);
	}
}

static void __show_page_info(void (*fn)(struct page *))
{
	pg_data_t *pg;
	int pgdat = 0;

	for (pg = pgdat_list; pg; pg = pg->node_next) {

		printk("===== Node %d =====\n", pgdat++);

		show_node_info(pg, fn);
	}	
}

void show_page_info(void)
{
	__show_page_info(page_detail);
}

static void
show_pg_info(int key, struct pt_regs *regs, struct kbd_struct *kd,
	     struct tty_struct *tty)
{
	void (*fn)(struct page *);
	show_mem();
	if (key == 'g')
		fn = page_detail;
	else
		fn = page_statistics;
	__show_page_info(fn);
}

static struct sysrq_key_op page_info_op = {
	handler:	show_pg_info,
	help_msg:	"paGeinfo",
	action_msg:	"Page Info",
};

static int __init debug_mm_init(void)
{
	register_sysrq_key('g', &page_info_op);
	register_sysrq_key('h', &page_info_op);
	return 0;
}

__initcall(debug_mm_init);
