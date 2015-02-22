/*
 * linux/fs/partitions/acorn.h
 *
 * Copyright (C) 1996-2001 Russell King.
 *
 *  I _hate_ this partitioning mess - why can't we have one defined
 *  format, and everyone stick to it?
 */

int
adfspart_check_CUMANA(struct gendisk *hd, struct block_device *bdev,
		      unsigned long first_sector, int minor);

int
adfspart_check_ADFS(struct gendisk *hd, struct block_device *bdev,
		   unsigned long first_sector, int minor);

int
adfspart_check_ICS(struct gendisk *hd, struct block_device *bdev,
		   unsigned long first_sector, int minor);

int
adfspart_check_POWERTEC(struct gendisk *hd, struct block_device *bdev,
			unsigned long first_sector, int minor);

int
adfspart_check_EESOX(struct gendisk *hd, struct block_device *bdev,
		     unsigned long first_sector, int minor);
