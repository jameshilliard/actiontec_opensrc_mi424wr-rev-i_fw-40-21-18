/*
 *  linux/fs/partitions/acorn.c
 *
 *  Copyright (c) 1996-2000 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Scan ADFS partitions on hard disk drives.  Unfortunately, there
 *  isn't a standard for partitioning drives on Acorn machines, so
 *  every single manufacturer of SCSI and IDE cards created their own
 *  method.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/adfs_fs.h>

#include "check.h"
#include "acorn.h"

/*
 * Partition types. (Oh for reusability)
 */
#define PARTITION_RISCIX_MFM	1
#define PARTITION_RISCIX_SCSI	2
#define PARTITION_LINUX		9

static void
adfspart_setgeometry(kdev_t dev, unsigned int secspertrack, unsigned int heads)
{
#ifdef CONFIG_BLK_DEV_MFM
	extern void xd_set_geometry(kdev_t dev, unsigned char, unsigned char,
				    unsigned long, unsigned int);

	if (MAJOR(dev) == MFM_ACORN_MAJOR) {
		unsigned long totalblocks = hd->part[MINOR(dev)].nr_sects;
		xd_set_geometry(dev, secspertrack, heads, totalblocks, 1);
	}
#endif
}

static struct adfs_discrecord *
adfs_partition(struct gendisk *hd, char *name, char *data,
	       unsigned long first_sector, int minor)
{
	struct adfs_discrecord *dr;
	unsigned int nr_sects;

	if (adfs_checkbblk(data))
		return NULL;

	dr = (struct adfs_discrecord *)(data + 0x1c0);

	if (dr->disc_size == 0 && dr->disc_size_high == 0)
		return NULL;

	nr_sects = (le32_to_cpu(dr->disc_size_high) << 23) |
		   (le32_to_cpu(dr->disc_size) >> 9);

	if (name)
		printk(" [%s]", name);
	add_gd_partition(hd, minor, first_sector, nr_sects);
	return dr;
}

#ifdef CONFIG_ACORN_PARTITION_RISCIX

struct riscix_part {
	__u32	start;
	__u32	length;
	__u32	one;
	char	name[16];
};

struct riscix_record {
	__u32	magic;
#define RISCIX_MAGIC	(0x4a657320)
	__u32	date;
	struct riscix_part part[8];
};

static int
riscix_partition(struct gendisk *hd, struct block_device *bdev,
		unsigned long first_sect, int minor, unsigned long nr_sects)
{
	Sector sect;
	struct riscix_record *rr;
	
	rr = (struct riscix_record *)read_dev_sector(bdev, first_sect, &sect);
	if (!rr)
		return -1;

	printk(" [RISCiX]");


	if (rr->magic == RISCIX_MAGIC) {
		unsigned long size = nr_sects > 2 ? 2 : nr_sects;
		int part;

		printk(" <");

		add_gd_partition(hd, minor++, first_sect, size);
		for (part = 0; part < 8; part++) {
			if (rr->part[part].one &&
			    memcmp(rr->part[part].name, "All\0", 4)) {
				add_gd_partition(hd, minor++,
						le32_to_cpu(rr->part[part].start),
						le32_to_cpu(rr->part[part].length));
				printk("(%s)", rr->part[part].name);
			}
		}

		printk(" >\n");
	} else {
		add_gd_partition(hd, minor++, first_sect, nr_sects);
	}

	put_dev_sector(sect);
	return minor;
}
#endif

#define LINUX_NATIVE_MAGIC 0xdeafa1de
#define LINUX_SWAP_MAGIC   0xdeafab1e

struct linux_part {
	__u32 magic;
	__u32 start_sect;
	__u32 nr_sects;
};

static int
linux_partition(struct gendisk *hd, struct block_device *bdev,
		unsigned long first_sect, int minor, unsigned long nr_sects)
{
	Sector sect;
	struct linux_part *linuxp;
	unsigned int mask = (1 << hd->minor_shift) - 1;
	unsigned long size = nr_sects > 2 ? 2 : nr_sects;

	printk(" [Linux]");

	add_gd_partition(hd, minor++, first_sect, size);

	linuxp = (struct linux_part *)read_dev_sector(bdev, first_sect, &sect);
	if (!linuxp)
		return -1;

	printk(" <");
	while (linuxp->magic == cpu_to_le32(LINUX_NATIVE_MAGIC) ||
	       linuxp->magic == cpu_to_le32(LINUX_SWAP_MAGIC)) {
		if (!(minor & mask))
			break;
		add_gd_partition(hd, minor++, first_sect +
				 le32_to_cpu(linuxp->start_sect),
				 le32_to_cpu(linuxp->nr_sects));
		linuxp ++;
	}
	printk(" >");

	put_dev_sector(sect);
	return minor;
}

#ifdef CONFIG_ACORN_PARTITION_CUMANA
int
adfspart_check_CUMANA(struct gendisk *hd, struct block_device *bdev,
		      unsigned long first_sector, int minor)
{
	unsigned int start_blk = 0, mask = (1 << hd->minor_shift) - 1;
	Sector sect;
	unsigned char *data;
	char *name = "CUMANA/ADFS";
	int first = 1;

	/*
	 * Try Cumana style partitions - sector 6 contains ADFS boot block
	 * with pointer to next 'drive'.
	 *
	 * There are unknowns in this code - is the 'cylinder number' of the
	 * next partition relative to the start of this one - I'm assuming
	 * it is.
	 *
	 * Also, which ID did Cumana use?
	 *
	 * This is totally unfinished, and will require more work to get it
	 * going. Hence it is totally untested.
	 */
	do {
		struct adfs_discrecord *dr;
		unsigned int nr_sects;

		data = read_dev_sector(bdev, start_blk * 2 + 6, &sect);
		if (!data)
			return -1;

		if (!(minor & mask))
			break;

		dr = adfs_partition(hd, name, data, first_sector, minor++);
		if (!dr)
			break;

		name = NULL;

		nr_sects = (data[0x1fd] + (data[0x1fe] << 8)) *
			   (dr->heads + (dr->lowsector & 0x40 ? 1 : 0)) *
			   dr->secspertrack;

		if (!nr_sects)
			break;

		first = 0;
		first_sector += nr_sects;
		start_blk += nr_sects >> (BLOCK_SIZE_BITS - 9);
		nr_sects = 0; /* hmm - should be partition size */

		switch (data[0x1fc] & 15) {
		case 0: /* No partition / ADFS? */
			break;

#ifdef CONFIG_ACORN_PARTITION_RISCIX
		case PARTITION_RISCIX_SCSI:
			/* RISCiX - we don't know how to find the next one. */
			minor = riscix_partition(hd, bdev, first_sector,
						 minor, nr_sects);
			break;
#endif

		case PARTITION_LINUX:
			minor = linux_partition(hd, bdev, first_sector,
						minor, nr_sects);
			break;
		}
		put_dev_sector(sect);
		if (minor == -1)
			return minor;
	} while (1);
	put_dev_sector(sect);
	return first ? 0 : 1;
}
#endif

#ifdef CONFIG_ACORN_PARTITION_ADFS
/*
 * Purpose: allocate ADFS partitions.
 *
 * Params : hd		- pointer to gendisk structure to store partition info.
 *	    dev		- device number to access.
 *	    first_sector- first readable sector on the device.
 *	    minor	- first available minor on device.
 *
 * Returns: -1 on error, 0 for no ADFS boot sector, 1 for ok.
 *
 * Alloc  : hda  = whole drive
 *	    hda1 = ADFS partition on first drive.
 *	    hda2 = non-ADFS partition.
 */
int
adfspart_check_ADFS(struct gendisk *hd, struct block_device *bdev,
		   unsigned long first_sector, int minor)
{
	unsigned long start_sect, nr_sects, sectscyl, heads;
	Sector sect;
	unsigned char *data;
	struct adfs_discrecord *dr;
	unsigned char id;

	data = read_dev_sector(bdev, 6, &sect);
	if (!data)
		return -1;

	dr = adfs_partition(hd, "ADFS", data, first_sector, minor++);
	if (!dr) {
		put_dev_sector(sect);
    		return 0;
	}

	heads = dr->heads + ((dr->lowsector >> 6) & 1);
	sectscyl = dr->secspertrack * heads;
	start_sect = ((data[0x1fe] << 8) + data[0x1fd]) * sectscyl;
	id = data[0x1fc] & 15;
	put_dev_sector(sect);

	adfspart_setgeometry(to_kdev_t(bdev->bd_dev), dr->secspertrack, heads);
	invalidate_bdev(bdev, 1);
	truncate_inode_pages(bdev->bd_inode->i_mapping, 0);

	/*
	 * Work out start of non-adfs partition.
	 */
	nr_sects = hd->part[MINOR(to_kdev_t(bdev->bd_dev))].nr_sects - start_sect;

	if (start_sect) {
		first_sector += start_sect;

		switch (id) {
#ifdef CONFIG_ACORN_PARTITION_RISCIX
		case PARTITION_RISCIX_SCSI:
		case PARTITION_RISCIX_MFM:
			minor = riscix_partition(hd, bdev, first_sector,
						 minor, nr_sects);
			break;
#endif

		case PARTITION_LINUX:
			minor = linux_partition(hd, bdev, first_sector,
						minor, nr_sects);
			break;
		}
	}
	printk("\n");
	return 1;
}
#endif

#ifdef CONFIG_ACORN_PARTITION_ICS

struct ics_part {
	__u32 start;
	__s32 size;
};

static int adfspart_check_ICSLinux(struct block_device *bdev, unsigned long block)
{
	Sector sect;
	unsigned char *data = read_dev_sector(bdev, block, &sect);
	int result = 0;

	if (data) {
		if (memcmp(data, "LinuxPart", 9) == 0)
			result = 1;
		put_dev_sector(sect);
	}

	return result;
}

/*
 * Check for a valid ICS partition using the checksum.
 */
static inline int valid_ics_sector(const unsigned char *data)
{
	unsigned long sum;
	int i;

	for (i = 0, sum = 0x50617274; i < 508; i++)
		sum += data[i];

	sum -= le32_to_cpu(*(__u32 *)(&data[508]));

	return sum == 0;
}

/*
 * Purpose: allocate ICS partitions.
 * Params : hd		- pointer to gendisk structure to store partition info.
 *	    dev		- device number to access.
 *	    first_sector- first readable sector on the device.
 *	    minor	- first available minor on device.
 * Returns: -1 on error, 0 for no ICS table, 1 for partitions ok.
 * Alloc  : hda  = whole drive
 *	    hda1 = ADFS partition 0 on first drive.
 *	    hda2 = ADFS partition 1 on first drive.
 *		..etc..
 */
int
adfspart_check_ICS(struct gendisk *hd, struct block_device *bdev,
		   unsigned long first_sector, int minor)
{
	const unsigned char *data;
	const struct ics_part *p;
	unsigned int mask = (1 << hd->minor_shift) - 1;
	Sector sect;

	/*
	 * Try ICS style partitions - sector 0 contains partition info.
	 */
	data = read_dev_sector(bdev, 0, &sect);
	if (!data)
	    	return -1;

	if (!valid_ics_sector(data)) {
		put_dev_sector(sect);
		return 0;
	}

	printk(" [ICS]");

	for (p = (const struct ics_part *)data; p->size; p++) {
		unsigned long start;
		long size;

		if ((minor & mask) == 0)
			break;

		start = le32_to_cpu(p->start);
		size  = le32_to_cpu(p->size);

		/*
		 * Negative sizes tell the RISC OS ICS driver to ignore
		 * this partition - in effect it says that this does not
		 * contain an ADFS filesystem.
		 */
		if (size < 0) {
			size = -size;

			/*
			 * Our own extension - We use the first sector
			 * of the partition to identify what type this
			 * partition is.  We must not make this visible
			 * to the filesystem.
			 */
			if (size > 1 && adfspart_check_ICSLinux(bdev, start)) {
				start += 1;
				size -= 1;
			}
		}

		if (size) {
			add_gd_partition(hd, minor, first_sector + start, size);
			minor++;
		}
	}

	put_dev_sector(sect);
	printk("\n");
	return 1;
}
#endif

#ifdef CONFIG_ACORN_PARTITION_POWERTEC
struct ptec_part {
	__u32 unused1;
	__u32 unused2;
	__u32 start;
	__u32 size;
	__u32 unused5;
	char type[8];
};

static inline int valid_ptec_sector(const unsigned char *data)
{
	unsigned char checksum = 0x2a;
	int i;

	for (i = 0; i < 511; i++)
		checksum += data[i];

	return checksum == data[511];
}

/*
 * Purpose: allocate ICS partitions.
 * Params : hd		- pointer to gendisk structure to store partition info.
 *	    dev		- device number to access.
 *	    first_sector- first readable sector on the device.
 *	    minor	- first available minor on device.
 * Returns: -1 on error, 0 for no ICS table, 1 for partitions ok.
 * Alloc  : hda  = whole drive
 *	    hda1 = ADFS partition 0 on first drive.
 *	    hda2 = ADFS partition 1 on first drive.
 *		..etc..
 */
int
adfspart_check_POWERTEC(struct gendisk *hd, struct block_device *bdev,
			unsigned long first_sector, int minor)
{
	Sector sect;
	const unsigned char *data;
	const struct ptec_part *p;
	int i;

	data = read_dev_sector(bdev, 0, &sect);
	if (!data)
		return -1;

	if (!valid_ptec_sector(data)) {
		put_dev_sector(sect);
		return 0;
	}

	printk(" [POWERTEC]");

	for (i = 0, p = (const struct ptec_part *)data; i < 12; i++, p++) {
		unsigned long start;
		unsigned long size;

		start = le32_to_cpu(p->start);
		size  = le32_to_cpu(p->size);

		if (size)
			add_gd_partition(hd, minor, first_sector + start,
					 size);
		minor++;
	}

	put_dev_sector(sect);
	printk("\n");
	return 1;
}
#endif

#ifdef CONFIG_ACORN_PARTITION_EESOX
struct eesox_part {
	char	magic[6];
	char	name[10];
	u32	start;
	u32	unused6;
	u32	unused7;
	u32	unused8;
};

/*
 * Guess who created this format?
 */
static const char eesox_name[] = {
	'N', 'e', 'i', 'l', ' ',
	'C', 'r', 'i', 't', 'c', 'h', 'e', 'l', 'l', ' ', ' '
};

/*
 * EESOX SCSI partition format.
 *
 * This is a goddamned awful partition format.  We don't seem to store
 * the size of the partition in this table, only the start addresses.
 *
 * There are two possibilities where the size comes from:
 *  1. The individual ADFS boot block entries that are placed on the disk.
 *  2. The start address of the next entry.
 */
int
adfspart_check_EESOX(struct gendisk *hd, struct block_device *bdev,
		     unsigned long first_sector, int minor)
{
	Sector sect;
	const unsigned char *data;
	unsigned char buffer[256];
	struct eesox_part *p;
	u32 start = first_sector;
	int i;

	data = read_dev_sector(bdev, 7, &sect);
	if (!data)
		return -1;

	/*
	 * "Decrypt" the partition table.  God knows why...
	 */
	for (i = 0; i < 256; i++)
		buffer[i] = data[i] ^ eesox_name[i & 15];

	put_dev_sector(sect);

	for (i = 0, p = (struct eesox_part *)buffer; i < 8; i++, p++) {
		u32 next;

		if (memcmp(p->magic, "Eesox", 6))
			break;

		next = le32_to_cpu(p->start) + first_sector;
		if (i)
			add_gd_partition(hd, minor++, start, next - start);
		start = next;
	}

	if (i != 0) {
		unsigned long size;

		size = hd->part[MINOR(to_kdev_t(bdev->bd_dev))].nr_sects;
		add_gd_partition(hd, minor++, start, size - start);
		printk("\n");
	}

	return i ? 1 : 0;
}
#endif
