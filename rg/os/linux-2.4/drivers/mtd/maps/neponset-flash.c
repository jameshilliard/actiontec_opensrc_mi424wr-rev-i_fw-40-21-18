/*
 * Flash memory access on SA11x0 based devices
 * 
 * (C) 2000 Nicolas Pitre <nico@cam.org>
 * 
 * $Id: neponset-flash.c,v 1.1.1.1 2007/05/07 23:29:27 jungo Exp $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/assabet.h>

static __u8 read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

static __u16 read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

static __u32 read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

#define MAX_SZ (32 * 1024 * 1024)

static struct map_info neponset_map = {
	name:		"Neponset",
	size:		MAX_SZ,
	buswidth:	4,
	read8:		read8,
	read16:		read16,
	read32:		read32,
	copy_from:	copy_from,
	write8:		write8,
	write16:	write16,
	write32:	write32,
	copy_to:	copy_to,
};

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);
extern int parse_bootldr_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static struct mtd_info *neponset_mtd;

int __init neponset_mtd_init(void)
{
	if (!machine_is_assabet() || !machine_has_neponset())
		return -ENODEV;

	neponset_map.map_priv_1 = (unsigned int)ioremap(0x08000000, MAX_SZ);
	if (!neponset_map.map_priv_1)
		return -ENOMEM;

	neponset_mtd = do_map_probe("cfi_probe", &neponset_map);
	if (!neponset_mtd)
		return -ENXIO;
	neponset_mtd->module = THIS_MODULE;
	add_mtd_device(neponset_mtd);
	return 0;
}

static void __exit neponset_mtd_cleanup(void)
{
	if (neponset_mtd)
		map_destroy(neponset_mtd);
	if (neponset_map.map_priv_1)
		iounmap((void *)neponset_map.map_priv_1);
}

module_init(neponset_mtd_init);
module_exit(neponset_mtd_cleanup);
