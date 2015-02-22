/*
 * Serial Peripheral Interface (SPI) driver for the Atmel AT91RM9200
 *
 * (c) SAN People (Pty) Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef AT91_SPI_H
#define AT91_SPI_H

/* Maximum number of buffers in a single SPI transfer.
 *  DataFlash uses maximum of 2
 *  spidev interface supports up to 8.
 */
#define MAX_SPI_TRANSFERS	8

#define NR_SPI_DEVICES  	4	/* number of devices on SPI bus */

#define DATAFLASH_CLK		6000000
#define DEFAULT_SPI_BAUD	AT91C_MASTER_CLOCK / (2 * DATAFLASH_CLK)

#define SPI_MAJOR		153	/* registered device number */

/*
 * Describes the buffers for a SPI transfer.
 * A transmit & receive buffer must be specified for each transfer
 */
struct spi_transfer_list {
	void* tx[MAX_SPI_TRANSFERS];	/* transmit */
	int txlen[MAX_SPI_TRANSFERS];
	void* rx[MAX_SPI_TRANSFERS];	/* receive */
	int rxlen[MAX_SPI_TRANSFERS];
	int nr_transfers;		/* number of transfers */
	int curr;			/* current transfer */
};

struct spi_local {
	unsigned int pcs;		/* Peripheral Chip Select value */
	short pio_enabled;		/* has PIO been enabled? */

	struct spi_transfer_list *xfers;	/* current transfer list */
	dma_addr_t tx, rx;		/* DMA address for current transfer */
	dma_addr_t txnext, rxnext;	/* DMA address for next transfer */
};


/* Exported functions */
extern void spi_access_bus(short device);
extern void spi_release_bus(short device);
extern int spi_transfer(struct spi_transfer_list* list);

#endif
