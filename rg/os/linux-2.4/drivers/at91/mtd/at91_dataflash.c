/*
 * Atmel DataFlash driver for Atmel AT91RM9200 (Thunder)
 *
 * (c) SAN People (Pty) Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include <asm/arch/AT91RM9200_SPI.h>
#include <asm/arch/pio.h>
#include "at91_dataflash.h"
#include "../spi/at91_spi.h"

#undef DEBUG_DATAFLASH

/* Detected DataFlash devices */
static struct mtd_info* mtd_devices[DATAFLASH_MAX_DEVICES];
static int nr_devices = 0;

/* ......................................................................... */

#ifdef CONFIG_MTD_PARTITIONS

static struct mtd_partition *mtd_parts = 0;
static int mtd_parts_nr = 0;

#define NB_OF(x) (sizeof(x)/sizeof(x[0]))

static struct mtd_partition static_partitions[] =
{
	{
		name:		"bootloader",
		offset:		0,
		size:		64 * 1024,		/* 64 Kb */
		mask_flags:	MTD_WRITEABLE		/* read-only */
	},
	{
		name:		"kernel",
		offset:		MTDPART_OFS_NXTBLK,
		size:		768 *1024,		/* 768 Kb */
	},
	{
		name:		"filesystem",
		offset:		MTDPART_OFS_NXTBLK,
		size:		MTDPART_SIZ_FULL,
	}
};

int parse_cmdline_partitions(struct mtd_info *master,
		struct mtd_partition **pparts, const char *mtd_id);

#endif

/* ......................................................................... */

/* Allocate a single SPI transfer descriptor.  We're assuming that if multiple
   SPI transfers occur at the same time, spi_access_bus() will serialize them.
   If this is not valid, then either (i) each dataflash 'priv' structure
   needs it's own transfer descriptor, (ii) we lock this one, or (iii) use
   another mechanism.   */
struct spi_transfer_list* spi_transfer_desc;

/*
 * Perform a SPI transfer to access the DataFlash device.
 */
int do_spi_transfer(int nr, char* tx, int tx_len, char* rx, int rx_len,
		char* txnext, int txnext_len, char* rxnext, int rxnext_len)
{
	struct spi_transfer_list* list = spi_transfer_desc;

	list->tx[0] = tx;	list->txlen[0] = tx_len;
	list->rx[0] = rx;	list->rxlen[0] = rx_len;

	list->tx[1] = txnext; 	list->txlen[1] = txnext_len;
	list->rx[1] = rxnext;	list->rxlen[1] = rxnext_len;

	list->nr_transfers = nr;

	return spi_transfer(list);
}

/* ......................................................................... */

/*
 * Poll the DataFlash device until it is READY.
 */
void at91_dataflash_waitready(void)
{
	char* command = kmalloc(2, GFP_KERNEL);

	if (!command)
		return;

	do {
		command[0] = OP_READ_STATUS;
		command[1] = 0;

		do_spi_transfer(1, command, 2, command, 2, NULL, 0, NULL, 0);
	} while ((command[1] & 0x80) == 0);

	kfree(command);
}

/*
 * Return the status of the DataFlash device.
 */
unsigned short at91_dataflash_status(void)
{
	unsigned short status;
	char* command = kmalloc(2, GFP_KERNEL);

	if (!command)
		return 0;

	command[0] = OP_READ_STATUS;
	command[1] = 0;

	do_spi_transfer(1, command, 2, command, 2, NULL, 0, NULL, 0);
	status = command[1];

	kfree(command);
	return status;
}

/* ......................................................................... */

/*
 * Erase a block of flash.
 */
int at91_dataflash_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct dataflash_local *priv = (struct dataflash_local *) mtd->priv;
	unsigned int pageaddr;
	char* command;

#ifdef DEBUG_DATAFLASH
	printk("dataflash_erase: addr=%i len=%i\n", instr->addr, instr->len);
#endif

	/* Sanity checks */
	if (instr->addr + instr->len > mtd->size)
		return -EINVAL;
	if ((instr->len != mtd->erasesize) || (instr->len != priv->page_size))
		return -EINVAL;
	if ((instr->addr % priv->page_size) != 0)
		return -EINVAL;

	command = kmalloc(4, GFP_KERNEL);
	if (!command)
		return -ENOMEM;

	/* Calculate flash page address */
	pageaddr = (instr->addr / priv->page_size) << priv->page_offset;

	command[0] = OP_ERASE_PAGE;
	command[1] = (pageaddr & 0x00FF0000) >> 16;
	command[2] = (pageaddr & 0x0000FF00) >> 8;
	command[3] = 0;
#ifdef DEBUG_DATAFLASH
	printk("ERASE: (%x) %x %x %x [%i]\n", command[0], command[1], command[2], command[3], pageaddr);
#endif

	/* Send command to SPI device */
	spi_access_bus(priv->spi);
	do_spi_transfer(1, command, 4, command, 4, NULL, 0, NULL, 0);

	at91_dataflash_waitready();	/* poll status until ready */
	spi_release_bus(priv->spi);

	kfree(command);

	/* Inform MTD subsystem that erase is complete */
	instr->state = MTD_ERASE_DONE;
	if (instr->callback)
		instr->callback(instr);

	return 0;
}

/*
 * Read from the DataFlash device.
 *   from   : Start offset in flash device
 *   len    : Amount to read
 *   retlen : About of data actually read
 *   buf    : Buffer containing the data
 */
int at91_dataflash_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct dataflash_local *priv = (struct dataflash_local *) mtd->priv;
	unsigned int addr;
	char* command;

#ifdef DEBUG_DATAFLASH
	printk("dataflash_read: %lli .. %lli\n", from, from+len);
#endif

	*retlen = 0;

	/* Sanity checks */
	if (!len)
		return 0;
	if (from + len > mtd->size)
		return -EINVAL;

	/* Calculate flash page/byte address */
	addr = (((unsigned)from / priv->page_size) << priv->page_offset) + ((unsigned)from % priv->page_size);

	command = kmalloc(8, GFP_KERNEL);
	if (!command)
		return -ENOMEM;

	command[0] = OP_READ_CONTINUOUS;
	command[1] = (addr & 0x00FF0000) >> 16;
	command[2] = (addr & 0x0000FF00) >> 8;
	command[3] = (addr & 0x000000FF);
#ifdef DEBUG_DATAFLASH
	printk("READ: (%x) %x %x %x\n", command[0], command[1], command[2], command[3]);
#endif

	/* Send command to SPI device */
	spi_access_bus(priv->spi);
	do_spi_transfer(2, command, 8, command, 8, buf, len, buf, len);
	spi_release_bus(priv->spi);

	*retlen = len;
	kfree(command);
	return 0;
}

/*
 * Write to the DataFlash device.
 *   to     : Start offset in flash device
 *   len    : Amount to write
 *   retlen : Amount of data actually written
 *   buf    : Buffer containing the data
 */
int at91_dataflash_write(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	struct dataflash_local *priv = (struct dataflash_local *) mtd->priv;
	unsigned int pageaddr, addr, offset, writelen;
	size_t remaining;
	char *writebuf;
	unsigned short status;
	int res = 0;
	char* command;

#ifdef DEBUG_DATAFLASH
	printk("dataflash_write: %lli .. %lli\n", to, to+len);
#endif

	*retlen = 0;

	/* Sanity checks */
	if (!len)
		return 0;
	if (to + len > mtd->size)
		return -EINVAL;

	command = kmalloc(4, GFP_KERNEL);
	if (!command)
		return -ENOMEM;

	pageaddr = ((unsigned)to / priv->page_size);
	offset = ((unsigned)to % priv->page_size);
	if (offset + len > priv->page_size)
		writelen = priv->page_size - offset;
	else
		writelen = len;
	writebuf = buf;
	remaining = len;

	/* Gain access to the SPI bus */
	spi_access_bus(priv->spi);

	while (remaining > 0) {
#ifdef DEBUG_DATAFLASH
		printk("write @ %i:%i len=%i\n", pageaddr, offset, writelen);
#endif

		/* (1) Transfer to Buffer1 */
		if (writelen != priv->page_size) {
			addr = pageaddr << priv->page_offset;
			command[0] = OP_TRANSFER_BUF1;
			command[1] = (addr & 0x00FF0000) >> 16;
			command[2] = (addr & 0x0000FF00) >> 8;
			command[3] = 0;
#ifdef DEBUG_DATAFLASH
			printk("TRANSFER: (%x) %x %x %x\n", command[0], command[1], command[2], command[3]);
#endif
			do_spi_transfer(1, command, 4, command, 4, NULL, 0, NULL, 0);
			at91_dataflash_waitready();
		}

		/* (2) Program via Buffer1 */
		addr = (pageaddr << priv->page_offset) + offset;
		command[0] = OP_PROGRAM_VIA_BUF1;
		command[1] = (addr & 0x00FF0000) >> 16;
		command[2] = (addr & 0x0000FF00) >> 8;
		command[3] = (addr & 0x000000FF);
#ifdef DEBUG_DATAFLASH
		printk("PROGRAM: (%x) %x %x %x\n", command[0], command[1], command[2], command[3]);
#endif
		do_spi_transfer(2, command, 4, command, 4, writebuf, writelen, writebuf, writelen);
		at91_dataflash_waitready();

		/* (3) Compare to Buffer1 */
		addr = pageaddr << priv->page_offset;
		command[0] = OP_COMPARE_BUF1;
		command[1] = (addr & 0x00FF0000) >> 16;
		command[2] = (addr & 0x0000FF00) >> 8;
		command[3] = 0;
#ifdef DEBUG_DATAFLASH
		printk("COMPARE: (%x) %x %x %x\n", command[0], command[1], command[2], command[3]);
#endif
		do_spi_transfer(1, command, 4, command, 4, NULL, 0, NULL, 0);
		at91_dataflash_waitready();

		/* Get result of the compare operation */
		status = at91_dataflash_status();
		if ((status & 0x40) == 1) {
			printk("at91_dataflash: Write error on page %i\n", pageaddr);
			remaining = 0;
			res = -EIO;
		}

		remaining = remaining - writelen;
		pageaddr++;
		offset = 0;
		writebuf += writelen;
		*retlen += writelen;

		if (remaining > priv->page_size)
			writelen = priv->page_size;
		else
			writelen = remaining;
	}

	/* Release SPI bus */
	spi_release_bus(priv->spi);

	kfree(command);
	return res;
}

/* ......................................................................... */

/*
 * Initialize and register DataFlash device with MTD subsystem.
 */
int add_dataflash(int channel, char *name, int size, int pagesize, int pageoffset)
{
	struct mtd_info *device;
	struct dataflash_local *priv;
#ifdef CONFIG_MTD_CMDLINE_PARTS
	char mtdID[14];
#endif

	if (nr_devices >= DATAFLASH_MAX_DEVICES) {
		printk(KERN_ERR "at91_dataflash: Too many devices detected\n");
		return 0;
	}

	device = (struct mtd_info *) kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (!device)
		return -ENOMEM;
	memset(device, 0, sizeof(struct mtd_info));

	device->name = name;
	device->size = size;
	device->erasesize = pagesize;
	device->module = THIS_MODULE;
	device->type = MTD_NORFLASH;
	device->flags = MTD_CAP_NORFLASH;
	device->erase = at91_dataflash_erase;
	device->read = at91_dataflash_read;
	device->write = at91_dataflash_write;

	priv = (struct dataflash_local *) kmalloc(sizeof(struct dataflash_local), GFP_KERNEL);
	if (!priv) {
		kfree(device);
		return -ENOMEM;
	}
	memset(priv, 0, sizeof(struct dataflash_local));

	priv->spi = channel;
	priv->page_size = pagesize;
	priv->page_offset = pageoffset;
	device->priv = priv;

	mtd_devices[nr_devices] = device;
	nr_devices++;
	printk("at91_dataflash: %s detected [spi%i] (%i bytes)\n", name, channel, size);

#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
	sprintf(mtdID, "dataflash%i", nr_devices-1);
	mtd_parts_nr = parse_cmdline_partitions(device, &mtd_parts, mtdID);
#endif
	if (mtd_parts_nr <= 0) {
		mtd_parts = static_partitions;
		mtd_parts_nr = NB_OF(static_partitions);
	}

	return add_mtd_partitions(device, mtd_parts, mtd_parts_nr);
#else
	return add_mtd_device(device);
#endif
}

/*
 * Detect and initialize DataFlash device connected to specified SPI channel.
 */
int at91_dataflash_detect(int channel)
{
	int res = 0;
	unsigned short status;

	spi_access_bus(channel);
	status = at91_dataflash_status();
	if (status != 0xff) {			/* no dataflash device there */
		switch (status & 0x3c) {
			case 0x2c:	/* 1 0 1 1 */
				res = add_dataflash(channel, "Atmel AT45DB161B", 4096*528, 528, 10);
				break;
			case 0x34:	/* 1 1 0 1 */
				res = add_dataflash(channel, "Atmel AT45DB321B", 8192*528, 528, 10);
				break;
			case 0x3c:	/* 1 1 1 1 */
				res = add_dataflash(channel, "Atmel AT45DB642", 8192*1056, 1056, 11);
				break;
			default:
				printk(KERN_ERR "at91_dataflash: Unknown device (%x)\n", status & 0x3c);
		}
	}
	spi_release_bus(channel);

	return res;
}

int __init at91_dataflash_init(void)
{
	spi_transfer_desc = kmalloc(sizeof(struct spi_transfer_list), GFP_KERNEL);
	if (!spi_transfer_desc)
		return -ENOMEM;

	/* DataFlash (SPI chip select 0) */
	at91_dataflash_detect(0);

#ifdef CONFIG_MTD_AT91_DATAFLASH_CARD
	/* DataFlash card (SPI chip select 3) */
	AT91_CfgPIO_DataFlashCard();
	at91_dataflash_detect(3);
#endif

	return 0;
}

void __exit at91_dataflash_exit(void)
{
	int i;

	for (i = 0; i < DATAFLASH_MAX_DEVICES; i++) {
		if (mtd_devices[i]) {
#ifdef CONFIG_MTD_PARTITIONS
			del_mtd_partitions(mtd_devices[i]);
#else
			del_mtd_device(mtd_devices[i]);
#endif
			kfree(mtd_devices[i]->priv);
			kfree(mtd_devices[i]);
		}
	}
	nr_devices = 0;
	kfree(spi_transfer_desc);
}


EXPORT_NO_SYMBOLS;

module_init(at91_dataflash_init);
module_exit(at91_dataflash_exit);

MODULE_LICENSE("GPL")
MODULE_AUTHOR("Andrew Victor")
MODULE_DESCRIPTION("DataFlash driver for Atmel AT91RM9200")
