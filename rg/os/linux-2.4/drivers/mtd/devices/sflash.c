/*
 * Broadcom SiliconBackplane chipcommon serial flash interface
 *
 * Copyright 2002, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Broadcom Corporation.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/mtd/compatmac.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <asm/io.h>

#include <typedefs.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <sbconfig.h>
#include <sbchipc.h>
#include <sflash.h>

struct sflash_mtd {
	chipcregs_t *cc;
	struct semaphore lock;
	struct mtd_info mtd;
	struct mtd_erase_region_info regions[SFLASH_MAX_BANKS * SFLASH_MAX_CHIPS];
	struct mtd_partition parts[2];
};

/* Private global state */
static struct sflash_mtd sflash;

static int
sflash_mtd_poll(struct sflash_mtd *sflash, unsigned int offset, int timeout)
{
	int now = jiffies;
	int ret = 0;

	for (;;) {
		if (!sflash_poll(sflash->cc, offset)) {
			ret = 0;
			break;
		}
		if (time_after(jiffies, now + timeout)) {
			printk(KERN_ERR "sflash: timeout\n");
			ret = -ETIMEDOUT;
			break;
		}
		if (current->need_resched) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(timeout / 10);
		} else
			udelay(1);
	}

	return ret;
}

static int
sflash_mtd_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct sflash_mtd *sflash = (struct sflash_mtd *) mtd->priv;
	int bytes, ret = 0;

	/* Check address range */
	if (!len)
		return 0;
	if ((from + len) > mtd->size)
		return -EINVAL;
	
	down(&sflash->lock);

	*retlen = 0;
	while (*retlen < len) {
		if ((bytes = sflash_read(sflash->cc, (uint) from, len, buf)) < 0) {
			ret = bytes;
			break;
		}
		from += (loff_t) bytes;
		len -= bytes;
		buf += bytes;
		*retlen += bytes;
	}

	up(&sflash->lock);

	return ret;
}

static int
sflash_mtd_write(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	struct sflash_mtd *sflash = (struct sflash_mtd *) mtd->priv;
	int bytes, ret = 0;

	/* Check address range */
	if (!len)
		return 0;
	if ((to + len) > mtd->size)
		return -EINVAL;

	down(&sflash->lock);

	*retlen = 0;
	while (*retlen < len) {
		if ((bytes = sflash_write(sflash->cc, (uint) to, len, buf)) < 0) {
			ret = bytes;
			break;
		}
		if ((ret = sflash_mtd_poll(sflash, (unsigned int) to, HZ / 10)))
			break;
		to += (loff_t) bytes;
		len -= bytes;
		buf += bytes;
		*retlen += bytes;
	}

	up(&sflash->lock);

	return ret;
}

static int
sflash_mtd_erase(struct mtd_info *mtd, struct erase_info *erase)
{
	struct sflash_mtd *sflash = (struct sflash_mtd *) mtd->priv;
	int i, j, ret = 0;
	unsigned int addr, len;

	/* Check address range */
	if (!erase->len)
		return 0;
	if ((erase->addr + erase->len) > mtd->size)
		return -EINVAL;

	addr = erase->addr;
	len = erase->len;

	down(&sflash->lock);

	/* Ensure that requested region is aligned */
	for (i = 0; i < mtd->numeraseregions; i++) {
		for (j = 0; j < mtd->eraseregions[i].numblocks; j++) {
			if (addr == mtd->eraseregions[i].offset + mtd->eraseregions[i].erasesize * j &&
			    len >= mtd->eraseregions[i].erasesize) {
				if ((ret = sflash_erase(sflash->cc, addr)) < 0)
					break;
				if ((ret = sflash_mtd_poll(sflash, addr, 10 * HZ)))
					break;
				addr += mtd->eraseregions[i].erasesize;
				len -= mtd->eraseregions[i].erasesize;
			}
		}
		if (ret)
			break;
	}

	up(&sflash->lock);

	/* Set erase status */
	if (ret)
		erase->state = MTD_ERASE_FAILED;
	else 
		erase->state = MTD_ERASE_DONE;

	/* Call erase callback */
	if (erase->callback)
		erase->callback(erase);

	return ret;
}

#if LINUX_VERSION_CODE < 0x20212 && defined(MODULE)
#define sflash_mtd_init init_module
#define sflash_mtd_exit cleanup_module
#endif

mod_init_t
sflash_mtd_init(void)
{
	struct pci_dev *pdev;
	int ret = 0;
	struct sflash *info;
	uint bank, chip, i;

	if (!(pdev = pci_find_device(VENDOR_BROADCOM, SB_CC, NULL))) {
		printk(KERN_ERR "sflash: chipcommon not found\n");
		return -ENODEV;
	}

	memset(&sflash, 0, sizeof(struct sflash_mtd));
	init_MUTEX(&sflash.lock);

	/* Map registers and flash base */
	if (!(sflash.cc = ioremap_nocache(pci_resource_start(pdev, 0),
					  pci_resource_len(pdev, 0)))) {
		printk(KERN_ERR "sflash: error mapping registers\n");
		ret = -EIO;
		goto fail;
	}

	/* Initialize serial flash access */
	sflash_init(sflash.cc);
	info = sflash_info();

	/* Setup banks */
	i = 0;
	for (bank = 0; bank < SFLASH_MAX_BANKS; bank++) {
		for (chip = 0; chip < SFLASH_MAX_CHIPS; chip++) {
			sflash.regions[i].offset = info->banks[bank].chips[chip].offset;
			sflash.regions[i].erasesize = info->banks[bank].chips[chip].erasesize;
			sflash.regions[i].numblocks = info->banks[bank].chips[chip].numblocks;
			if (sflash.regions[i].erasesize > sflash.mtd.erasesize)
				sflash.mtd.erasesize = sflash.regions[i].erasesize;
			if (sflash.regions[i].erasesize * sflash.regions[i].numblocks) {
				sflash.mtd.size += sflash.regions[i].erasesize * sflash.regions[i].numblocks;
				i++;
			}
		}
	}
	sflash.mtd.numeraseregions = i;
	ASSERT(sflash.mtd.size == info->size);

	/* Register with MTD */
	sflash.mtd.name = "sflash";
	sflash.mtd.type = MTD_NORFLASH;
	sflash.mtd.flags = MTD_CAP_NORFLASH;
	sflash.mtd.eraseregions = sflash.regions;
	sflash.mtd.module = THIS_MODULE;
	sflash.mtd.erase = sflash_mtd_erase;
	sflash.mtd.read = sflash_mtd_read;
	sflash.mtd.write = sflash_mtd_write;
	sflash.mtd.priv = &sflash;

	/* Add two partitions */
	sflash.parts[0].name = "linux";
	sflash.parts[0].offset = 0;
	sflash.parts[0].size = sflash.mtd.size - ROUNDUP(NVRAM_SPACE, sflash.mtd.erasesize);
	sflash.parts[1].name = "nvram";
	sflash.parts[1].offset = sflash.parts[0].size;
	sflash.parts[1].size = sflash.mtd.size - sflash.parts[0].size;
	if ((ret = add_mtd_partitions(&sflash.mtd, sflash.parts, 2))) {
		printk(KERN_ERR "sflash: add_mtd_partitions failed\n");
		goto fail;
	}

	return 0;

 fail:
	if (sflash.cc)
		iounmap((void *) sflash.cc);
	return ret;
}

mod_exit_t
sflash_mtd_exit(void)
{
	del_mtd_partitions(&sflash.mtd);
	iounmap((void *) sflash.cc);
}

module_init(sflash_mtd_init);
module_exit(sflash_mtd_exit);
