/*
 * Atmel DataFlash driver for the Atmel AT91RM9200 (Thunder)
 *
 * (c) SAN People (Pty) Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef AT91_DATAFLASH_H
#define AT91_DATAFLASH_H

#define DATAFLASH_MAX_DEVICES	4	/* max number of dataflash devices */

#define OP_READ_CONTINUOUS	0xE8
#define OP_READ_PAGE		0xD2
#define OP_READ_BUFFER1		0xD4
#define OP_READ_BUFFER2		0xD6
#define OP_READ_STATUS		0xD7

#define OP_ERASE_PAGE		0x81
#define OP_ERASE_BLOCK		0x50

#define OP_TRANSFER_BUF1	0x53
#define OP_TRANSFER_BUF2	0x55
#define OP_COMPARE_BUF1		0x60
#define OP_COMPARE_BUF2		0x61

#define OP_PROGRAM_VIA_BUF1	0x82
#define OP_PROGRAM_VIA_BUF2	0x85

struct dataflash_local
{
	int spi;			/* SPI chip-select number */

	unsigned int page_size;		/* number of bytes per page */
	unsigned short page_offset;	/* page offset in flash address */
};

#endif
