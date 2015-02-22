/*
 * Mapping for the ADI 80200EVB evaluation board
 *
 * Author:	Deepak Saxena
 * Copyright:	(C) 2001 MontaVista Software Inc.
 *
 * Based on iq80310 map written by Nicolas Pitre
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <linux/ioport.h>

#include <asm/io.h>

#define WINDOW_ADDR 	0
#define BUSWIDTH 	1

#define WINDOW_SIZE	4 * 1024 * 1024

static struct mtd_info *mymtd;

static __u8 adi_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 adi_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 adi_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void adi_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void adi_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void adi_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void adi_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void adi_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info adi_map = {
	name: "ADI Flash",
	buswidth: BUSWIDTH,
	read8:		adi_read8,
	read16:		adi_read16,
	read32:		adi_read32,
	copy_from:	adi_copy_from,
	write8:		adi_write8,
	write16:	adi_write16,
	write32:	adi_write32,
	copy_to:	adi_copy_to
};

static struct mtd_partition adi_partitions[2] = {
	{
		name:		"Firmware",
		size:		0x00080000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	}, {
		name:		"User Access",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND
	}
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *mymtd;
static struct mtd_partition *parsed_parts;
static struct resource *mtd_resource;

static int __init init_adi_evb(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;

	mtd_resource = 
		request_mem_region(WINDOW_ADDR, WINDOW_SIZE, "ADI Flash");

	if(!mtd_resource)
	{
		printk(KERN_ERR "ADI Flash: Could not request mem region\n");
		return -ENOMEM;
	}

	adi_map.map_priv_1 = (unsigned long)__ioremap(WINDOW_ADDR, WINDOW_SIZE, 0);
	if (!adi_map.map_priv_1) {
		printk("ADI Flash: Failed to ioremap\n");
		return -EIO;
	}

	adi_map.size = WINDOW_SIZE;

	mymtd = do_map_probe("cfi_probe", &adi_map);
	if (!mymtd) {
		iounmap((void *)adi_map.map_priv_1);
		return -ENXIO;
	}
	mymtd->module = THIS_MODULE;

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	} else {
		parts = adi_partitions;
		nb_parts = NB_OF(adi_partitions);
	}
	add_mtd_partitions(mymtd, parts, nb_parts);
	return 0;
}

static void __exit cleanup_adi_evb(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (adi_map.map_priv_1)
		iounmap((void *)adi_map.map_priv_1);
	if(mtd_resource)
		release_mem_region(WINDOW_ADDR, WINDOW_SIZE);
}

module_init(init_adi_evb);
module_exit(cleanup_adi_evb);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Saxena<dsaxena@mvista.com>");
MODULE_DESCRIPTION("MTD map driver for ADI 80200EVB evaluation board");
