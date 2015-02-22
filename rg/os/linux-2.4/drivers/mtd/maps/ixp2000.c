/*
 * Mapping for the Intel XScale IXP2000 based systems
 * Copyright:	(C) 2002 Intel Corp.
 * Author: 	Naeem M Afzal <naeem.m.afzal@intel.com>
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>


#define WINDOW_ADDR 	0xc4000000
#define WINDOW_SIZE 	16*1024*1024
#define BUSWIDTH 	1

#ifdef __ARMEB__
/*
 * Rev A0 and A1 of IXP2400 silicon have a broken addressing unit which 
 * causes the lower address bits to be XORed with 0x11 on 8 bit accesses 
 * and XORed with 0x10 on 16 bit accesses. See the spec update, erratta 44.
 */
static inline unsigned long errata44_workaround8(unsigned long addr)
{
	unsigned long tmp_addr = (addr & 0x3) ^ 0x3;
	unsigned long tmp_addr2 = addr & ~0x3;

	return (tmp_addr | tmp_addr2);
}

static inline unsigned long errata44_workaround16(addr)
{
	unsigned long tmp_addr = (addr & 0x2) ^ 0x2;
	unsigned long tmp_addr2 = addr & ~0x2;

	return (tmp_addr | tmp_addr2);
}
#else

#define errata44_workaround8(x)		(x)
#define errata44_workaround16(x)	(x)

#endif

static __u8 ixp2000_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(errata44_workaround8(map->map_priv_1 + ofs));
}

static __u16 ixp2000_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(errata44_workaround16(map->map_priv_1 + ofs));
}

/*
 * We can't use the standard memcpy due to the broken SlowPort
 * address translation on rev A0 and A1 silicon. Once B0 silicon
 * is available and I can prove that the errata is fixed, I'll take
 * this out.
 */
static void ixp2000_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
#ifdef __ARMEB__
	while(len--) 
		*(__u8 *)to++ = *(__u8 *)(errata44_workaround8(map->map_priv_1 + from++));
#else
	memcpy(to, (void *)(map->map_priv_1 + from), len);
#endif
}

static void ixp2000_write8(struct map_info *map, __u8 d, unsigned long ofs)
{
	*(__u8 *)(errata44_workaround8(map->map_priv_1 + ofs)) = d;
}

static void ixp2000_write16(struct map_info *map, __u16 d, unsigned long ofs)
{
	*(__u16 *)(errata44_workaround16(map->map_priv_1 + ofs)) = d;
}

static void ixp2000_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
#ifdef __ARMEB__
	while(len--) {
		unsigned long tmp = errata44_workaround8(map->map_priv_1 + to++);
		*(__u8 *)(tmp) = *(__u8 *)(from++);
	}
#else
	memcpy((void *)(map->map_priv_1 + to), from, len);
#endif
}

static struct map_info ixp2000_map = {
	name: "IXP2000 flash",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8:		ixp2000_read8,
	read16:		ixp2000_read16,
	copy_from:	ixp2000_copy_from,
	write8:		ixp2000_write8,
	write16:	ixp2000_write16,
	copy_to:	ixp2000_copy_to
};

#ifdef CONFIG_ARCH_IXDP2400
static struct mtd_partition ixp2000_partitions[4] = {
	{
		name:           "RedBoot",
		size:           0x00040000,
		offset:         0,
		mask_flags:     MTD_WRITEABLE  /* force read-only */
	},{
		name:           "System Log",
		size:           0x00020000,
		offset:         0x00fa0000,
	},{
		name:           "linux",
		size:           0x100000,
		offset:         0x00100000,
	},{
		name:           "ramdisk",
		size:           0x400000,
		offset:         0x00200000,
	}
};
#elif defined(CONFIG_ARCH_IXDP2800)
static struct mtd_partition ixp2000_partitions[] = {
	{
		name:           "vBOOT",
		size:           0x00100000,
		offset:         0,
		mask_flags:     MTD_WRITEABLE  /* force read-only */
	},{
		name:           "vWARE FFS",
		size:           0x00700000,
		offset:         0x00100000,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:           "vWARE free",
		size:           0x00400000,
		offset:         0x00800000,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		"free",
		size:		0x00400000,
		offset:		0x00c00000,
	}
};

#else 
#error No Architecture defined for MTD partition
#endif

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *mymtd;
static struct mtd_partition *parsed_parts;

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static int __init init_ixp2000(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	char *part_type = "Static";

	ixp2000_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);
	if (!ixp2000_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}

	mymtd = do_map_probe("cfi_probe", &ixp2000_map);
	if (!mymtd) {
		iounmap((void *)ixp2000_map.map_priv_1);
		return -ENXIO;
	}

	mymtd->module = THIS_MODULE;
	mymtd->priv = &ixp2000_map;

#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(mymtd, &parsed_parts);

		if (ret > 0) {
			part_type = "RedBoot";
			parsed_nr_parts = ret;
		}
	}
#endif

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	} else {
		parts = ixp2000_partitions;
		nb_parts = NB_OF(ixp2000_partitions);
	}
	printk(KERN_NOTICE "Using %s partition definition\n", part_type);
	add_mtd_partitions(mymtd, parts, nb_parts);
	return 0;
}

static void __exit cleanup_ixp2000(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (ixp2000_map.map_priv_1)
		iounmap((void *)ixp2000_map.map_priv_1);
}

module_init(init_ixp2000);
module_exit(cleanup_ixp2000);
MODULE_LICENSE("GPL");

