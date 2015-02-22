/*
 * Mapping for the ADI BRH board
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
#include <linux/mtd/concat.h>

#include <linux/ioport.h>

#include <asm/io.h>

#define NUM_BANKS	3
#define BUSWIDTH 	1

#define	WINDOW_ADDR	0
#define WINDOW_SIZE	16 * 1024 * 1024

static __u8 brh_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 brh_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 brh_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void brh_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void brh_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void brh_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void brh_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void brh_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static struct map_info brh_map[] = 
{
	{
		name: "ADI Flash Bank 0",
		buswidth: 	BUSWIDTH,
		read8:		brh_read8,
		read16:		brh_read16,
		read32:		brh_read32,
		copy_from:	brh_copy_from,
		write8:		brh_write8,
		write16:	brh_write16,
		write32:	brh_write32,
		copy_to:	brh_copy_to,
		size:		WINDOW_SIZE
	},
	{
		name: "ADI Flash Bank 1",
		buswidth: 	BUSWIDTH,
		read8:		brh_read8,
		read16:		brh_read16,
		read32:		brh_read32,
		copy_from:	brh_copy_from,
		write8:		brh_write8,
		write16:	brh_write16,
		write32:	brh_write32,
		copy_to:	brh_copy_to,
		size:		WINDOW_SIZE
	},
	{
		name: "ADI Flash Bank 2",
		buswidth: 	BUSWIDTH,
		read8:		brh_read8,
		read16:		brh_read16,
		read32:		brh_read32,
		copy_from:	brh_copy_from,
		write8:		brh_write8,
		write16:	brh_write16,
		write32:	brh_write32,
		copy_to:	brh_copy_to,
		size:		WINDOW_SIZE
	}
};

static struct mtd_partition brh_main_partitions[2] = {
	{
		name:		"Firmware",
		size:		0x000a0000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	}, {
		name:		"User Access",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND
	}
};

static struct mtd_partition brh_user_partitions[] = {
	{
		name:		"User Access",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND
	}
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *brh_mtd[NUM_BANKS];
static struct mtd_partition *parsed_parts;
static struct mtd_info *brh_concat;

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);


static void brh_destroy_mtd(void)
{
	int i = 0;

	if(brh_concat) mtd_concat_destroy(brh_concat);

	for(i = 0; i < NUM_BANKS; i++)
	{
		if(brh_map[i].map_priv_1)
			iounmap((void*)brh_map[i].map_priv_1);
		if(brh_map[i].map_priv_2)
			release_resource((struct resource*)brh_map[i].map_priv_2);
		if(brh_mtd[i])
			map_destroy(brh_mtd[i]);
	}
}

static int __init init_brh(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	char *part_type = "static";
	int i = 0;
	int found_banks = 0;
	int err_out = 0;

	for(i = 0; i < NUM_BANKS; i++)
	{
		unsigned long addr = WINDOW_ADDR + i * WINDOW_SIZE;
		struct resource *res;

		res = request_mem_region(addr, WINDOW_SIZE, brh_map[i].name);
		brh_map[found_banks].map_priv_2 = (unsigned long)res;

		if(!res) 
		{
			printk(KERN_ERR "Could not request region for %s\n", 
					brh_map[i].name);
			err_out = 1;
			break;
		}

		brh_map[i].map_priv_1 = 
			(unsigned long)__ioremap(addr, WINDOW_SIZE, 0);

		if(!brh_map[i].map_priv_1)
		{
			printk(KERN_ERR "Could not ioremap %s\n", 
					brh_map[i].name);
			err_out = 1;
			break;
		}

		brh_map[i].map_priv_2 = (unsigned long)res;

		brh_mtd[found_banks] = do_map_probe("cfi_probe", &brh_map[i]);
		if(brh_mtd[found_banks])
		{
			brh_mtd[found_banks]->module = THIS_MODULE;
			found_banks++;
		}
		else if(i != 0)
		{
			iounmap((void*)brh_map[i].map_priv_1);
			release_resource((struct resource*)brh_map[i].map_priv_2);
		}
		else
		{
			printk(KERN_ERR 
				"BRH MTD: No boot flash found, aborting!\n");
			err_out = 1;
		}
	}


	if(err_out)
	{
		brh_destroy_mtd();
		return -EIO;
	}

	if(found_banks == 0)
		return -NODEV;

	brh_concat = mtd_concat_create(brh_mtd, found_banks, "ADI Flash");
	if(!brh_concat)
	{
		brh_destroy_mtd();
		printk(KERN_ERR "Could not concatanate BRH Flash Banks\n");
		printk(KERN_ERR "Using static mappings\n");

		add_mtd_partitions(brh_mtd[0], brh_main_partitions, 2);

		if(brh_mtd[1])
			add_mtd_partitions(brh_mtd[1], brh_user_partitions, 1);

		if(brh_mtd[2])
			add_mtd_partitions(brh_mtd[2], brh_user_partitions, 1);

		return 0;
	}

	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(brh_concat, &parsed_parts);

		if (ret > 0) {
			part_type = "RedBoot";
			parsed_nr_parts = ret;
		}
	}

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	} else {
		parts = brh_main_partitions;
		nb_parts = NB_OF(brh_main_partitions);
	}

	add_mtd_partitions(brh_concat, parts, nb_parts);

	return 0;
}

static void __exit cleanup_brh(void)
{
	brh_destroy_mtd();
}

module_init(init_brh);
module_exit(cleanup_brh);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deepak Saxena<dsaxena@mvista.com>");
MODULE_DESCRIPTION("MTD map driver for ADI 80200EVB evaluation board");
