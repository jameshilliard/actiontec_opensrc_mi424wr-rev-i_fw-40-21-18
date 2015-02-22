/*
 * User-space interface to the SPI bus on Atmel AT91RM9200
 *
 * (c) SAN People (Pty) Ltd
 *
 * Based on SPI driver by Rick Bronson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/iobuf.h>
#include <linux/highmem.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include "at91_spi.h"

#undef DEBUG_SPIDEV

#ifdef CONFIG_DEVFS_FS
static devfs_handle_t devfs_handle = NULL;
static devfs_handle_t devfs_spi[NR_SPI_DEVICES];
#endif

/* ......................................................................... */

/*
 * Read or Write to SPI bus.
 */
static ssize_t spidev_rd_wr(struct file *file, char *buf, size_t count, loff_t *offset)
{
	unsigned int spi_device = (unsigned int) file->private_data;
	struct kiobuf *iobuf;
	unsigned int ofs, pagelen;
	int res, i;

	struct spi_transfer_list* list = kmalloc(sizeof(struct spi_transfer_list), GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	res = alloc_kiovec(1, &iobuf);
	if (res) {
		kfree(list);
		return res;
	}

	res = map_user_kiobuf(READ, iobuf, (unsigned long) buf, count);
	if (res) {
		free_kiovec(1, &iobuf);
		kfree(list);
		return res;
	}

	/* More pages than transfer slots in spi_transfer_list */
	if (iobuf->nr_pages >= MAX_SPI_TRANSFERS) {
		unmap_kiobuf(iobuf);
		free_kiovec(1, &iobuf);
		kfree(list);
		return -EFBIG;
	}

#ifdef DEBUG_SPIDEV
	printk("spidev_rd_rw: %i %i\n", count, iobuf->nr_pages);
#endif

	/* Set default return value = transfer length */
	res = count;

	/*
	 * At this point, the virtual area buf[0] .. buf[count-1] will have
	 * corresponding pages mapped in the physical memory and locked until
	 * we unmap the kiobuf.  The pages cannot be swapped out or moved
	 * around.
	 */
	ofs = iobuf->offset;
	pagelen = PAGE_SIZE - iobuf->offset;
	if (count < pagelen)
		pagelen = count;

	for (i = 0; i < iobuf->nr_pages; i++) {
		list->tx[i] = list->rx[i] = page_address(iobuf->maplist[i]) + ofs;
		list->txlen[i] = list->rxlen[i] = pagelen;

#ifdef DEBUG_SPIDEV
		printk("  %i: %x  (%i)\n", i, list->tx[i], list->txlen[i]);
#endif

		ofs = 0;	/* all subsequent transfers start at beginning of a page */
		count = count - pagelen;
		pagelen = (count < PAGE_SIZE) ? count : PAGE_SIZE;
	}
	list->nr_transfers = iobuf->nr_pages;

	/* Perform transfer on SPI bus */
	spi_access_bus(spi_device);
	spi_transfer(list);
	spi_release_bus(spi_device);

	unmap_kiobuf(iobuf);
	free_kiovec(1, &iobuf);
	kfree(list);

	return res;
}

int spidev_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int spi_device = MINOR(inode->i_rdev);

	if (spi_device >= NR_SPI_DEVICES)
		return -ENODEV;

	// TODO: This interface can be used to configure the SPI bus.
	// Configurable options could include: Speed, Clock Polarity, Clock Phase

	switch(cmd) {
		default:
			return -ENOIOCTLCMD;
	}
}

/*
 * Open the SPI device
 */
int spidev_open(struct inode *inode, struct file *file)
{
	unsigned int spi_device = MINOR(inode->i_rdev);

	if (spi_device >= NR_SPI_DEVICES)
		return -ENODEV;

	MOD_INC_USE_COUNT;

	/*
	 * 'private_data' is actually a pointer, but we overload it with the
	 * value we want to store.
	 */
	(unsigned int) file->private_data = spi_device;

	return 0;
}

/*
 * Close the SPI device
 */
static int spidev_close(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}

/* ......................................................................... */

static struct file_operations spidev_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		spidev_rd_wr,
	write:		spidev_rd_wr,
	ioctl:		spidev_ioctl,
	open:		spidev_open,
	release:	spidev_close,
};

/*
 * Install the SPI /dev interface driver
 */
static int __init at91_spidev_init(void)
{
	int i;
	char name[3];

#ifdef CONFIG_DEVFS_FS
	if (devfs_register_chrdev(SPI_MAJOR, "spi", &spidev_fops)) {
#else
	if (register_chrdev(SPI_MAJOR, "spi", &spidev_fops)) {
#endif
		printk(KERN_ERR "at91_spidev: Unable to get major %d for SPI bus\n", SPI_MAJOR);
		return -EIO;
	}

#ifdef CONFIG_DEVFS_FS
	devfs_handle = devfs_mk_dir(NULL, "spi", NULL);

	for (i = 0; i < NR_SPI_DEVICES; i++) {
		sprintf (name, "%d", i);
		devfs_spi[i] = devfs_register (devfs_handle, name,
			DEVFS_FL_DEFAULT, SPI_MAJOR, i, S_IFCHR | S_IRUSR | S_IWUSR,
			&spidev_fops, NULL);
	}
#endif
	printk(KERN_INFO "AT91 SPI driver loaded\n");

	return 0;
}

/*
 * Remove the SPI /dev interface driver
 */
static void at91_spidev_exit(void)
{
#ifdef CONFIG_DEVFS_FS
	devfs_unregister(devfs_handle);
	if (devfs_unregister_chrdev(SPI_MAJOR, "spi")) {
#else
	if (unregister_chrdev(SPI_MAJOR,"spi")) {
#endif
		printk(KERN_ERR "at91_spidev: Unable to release major %d for SPI bus\n", SPI_MAJOR);
		return;
	}
}

module_init(at91_spidev_init);
module_exit(at91_spidev_exit);

MODULE_LICENSE("GPL")
MODULE_AUTHOR("Andrew Victor")
MODULE_DESCRIPTION("SPI /dev interface for Atmel AT91RM9200")
