/*
 * Serial Peripheral Interface (SPI) driver for the Atmel AT91RM9200 (Thunder)
 *
 * (c) SAN People (Pty) Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <asm/semaphore.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/completion.h>

#include <asm/arch/AT91RM9200_SPI.h>
#include <asm/arch/pio.h>
#include "at91_spi.h"

#undef DEBUG_SPI

static struct spi_local spi_dev[NR_SPI_DEVICES];	/* state of the SPI devices */
static int spi_enabled = 0;
static struct semaphore spi_lock;			/* protect access to SPI bus */
static int current_device = -1;				/* currently selected SPI device */

DECLARE_COMPLETION(transfer_complete);

/* SPI controller device */
AT91PS_SPI controller = (AT91PS_SPI) AT91C_VA_BASE_SPI;

/* ......................................................................... */

/*
 * Access and enable the SPI bus.
 * This MUST be called before any transfers are performed.
 */
void spi_access_bus(short device)
{
	/* Ensure that requested device is valid */
	if ((device < 0) || (device >= NR_SPI_DEVICES))
		panic("at91_spi: spi_access_bus called with invalid device");

	if (spi_enabled == 0) {
		AT91_SYS->PMC_PCER = 1 << AT91C_ID_SPI;	/* Enable Peripheral clock */
		controller->SPI_CR = AT91C_SPI_SPIEN;	/* Enable SPI */
#ifdef DEBUG_SPI
		printk("SPI on\n");
#endif
	}
	MOD_INC_USE_COUNT;
	spi_enabled++;

	/* Lock the SPI bus */
	down(&spi_lock);
	current_device = device;

	/* Enable PIO */
	if (!spi_dev[device].pio_enabled) {
		switch (device) {
			case 0: AT91_CfgPIO_SPI_CS0();
			case 1: AT91_CfgPIO_SPI_CS1();
			case 2: AT91_CfgPIO_SPI_CS2();
			case 3: AT91_CfgPIO_SPI_CS3();
		}
		spi_dev[device].pio_enabled = 1;
#ifdef DEBUG_SPI
		printk("SPI CS%i enabled\n", device);
#endif
	}

	/* Configure SPI bus for device */
	controller->SPI_MR = AT91C_SPI_MSTR | AT91C_SPI_MODFDIS | (spi_dev[device].pcs << 16);
}

/*
 * Relinquish control of the SPI bus.
 */
void spi_release_bus(short device)
{
	if (device != current_device)
		panic("at91_spi: spi_release called with invalid device");

	/* Release the SPI bus */
	current_device = -1;
	up(&spi_lock);

	spi_enabled--;
	MOD_DEC_USE_COUNT;
	if (spi_enabled == 0) {
		controller->SPI_CR = AT91C_SPI_SPIDIS;	/* Disable SPI */
		AT91_SYS->PMC_PCER = 1 << AT91C_ID_SPI;	/* Disable Peripheral clock */
#ifdef DEBUG_SPI
		printk("SPI off\n");
#endif
	}
}

/*
 * Perform a data transfer over the SPI bus
 */
int spi_transfer(struct spi_transfer_list* list)
{
	struct spi_local *device = (struct spi_local *) &spi_dev[current_device];

	if (!list)
		panic("at91_spi: spi_transfer called with NULL transfer list");
	if (current_device == -1)
		panic("at91_spi: spi_transfer called without acquiring bus");

#ifdef DEBUG_SPI
	printk("SPI transfer start [%i]\n", list->nr_transfers);
#endif

	/* Store transfer list */
	device->xfers = list;
	list->curr = 0;

	/* Assume there must be at least one transfer */
	device->tx = pci_map_single(NULL, list->tx[0], list->txlen[0], PCI_DMA_TODEVICE);
	device->rx = pci_map_single(NULL, list->rx[0], list->rxlen[0], PCI_DMA_FROMDEVICE);

	/* Program PDC registers */
	controller->SPI_TPR = device->tx;
	controller->SPI_RPR = device->rx;
	controller->SPI_TCR = list->txlen[0];
	controller->SPI_RCR = list->rxlen[0];

	/* Is there a second transfer? */
	if (list->nr_transfers > 1) {
		device->txnext = pci_map_single(NULL, list->tx[1], list->txlen[1], PCI_DMA_TODEVICE);
		device->rxnext = pci_map_single(NULL, list->rx[1], list->rxlen[1], PCI_DMA_FROMDEVICE);

		/* Program Next PDC registers */
		controller->SPI_TNPR = device->txnext;
		controller->SPI_RNPR = device->rxnext;
		controller->SPI_TNCR = list->txlen[1];
		controller->SPI_RNCR = list->rxlen[1];
	}
	else {
		device->txnext = 0;
		device->rxnext = 0;
		controller->SPI_TNCR = 0;
		controller->SPI_RNCR = 0;
	}

	// TODO: If we are doing consecutive transfers (at high speed, or
	//   small buffers), then it might be worth modifying the 'Delay between
	//   Consecutive Transfers' in the CSR registers.
	//   This is an issue if we cannot chain the next buffer fast enough
	//   in the interrupt handler.

	/* Enable transmitter and receiver */
	controller->SPI_PTCR = AT91C_PDC_RXTEN | AT91C_PDC_TXTEN;

	controller->SPI_IER = AT91C_SPI_SPENDRX;	/* enable buffer complete interrupt */
	wait_for_completion(&transfer_complete);

#ifdef DEBUG_SPI
	printk("SPI transfer end\n");
#endif

	return 0;
}

/* ......................................................................... */

/*
 * Handle interrupts from the SPI controller.
 */
void spi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
 	unsigned int status;
	struct spi_local *device = (struct spi_local *) &spi_dev[current_device];
	struct spi_transfer_list *list = device->xfers;

#ifdef DEBUG_SPI
	printk("SPI interrupt %i\n", current_device);
#endif

	if (!list)
		panic("at91_spi: spi_interrupt with a NULL transfer list");

       	status = controller->SPI_SR & controller->SPI_IMR;	/* read status */

	pci_unmap_single(NULL, device->tx, list->txlen[list->curr], PCI_DMA_TODEVICE);
	pci_unmap_single(NULL, device->rx, list->rxlen[list->curr], PCI_DMA_FROMDEVICE);

	device->tx = device->txnext;	/* move next transfer to current transfer */
	device->rx = device->rxnext;

	list->curr = list->curr + 1;
	if (list->curr == list->nr_transfers) {		/* all transfers complete */
		controller->SPI_IDR = AT91C_SPI_SPENDRX;	/* disable interrupt */

		/* Disable transmitter and receiver */
		controller->SPI_PTCR = AT91C_PDC_RXTDIS | AT91C_PDC_TXTDIS;

		device->xfers = NULL;
		complete(&transfer_complete);
	}
	else if (list->curr+1 == list->nr_transfers) {	/* no more next transfers */
		device->txnext = 0;
		device->rxnext = 0;
		controller->SPI_TNCR = 0;
		controller->SPI_RNCR = 0;
	}
	else {
		int i = (list->curr)+1;

		device->txnext = pci_map_single(NULL, list->tx[i], list->txlen[i], PCI_DMA_TODEVICE);
		device->rxnext = pci_map_single(NULL, list->rx[i], list->rxlen[i], PCI_DMA_FROMDEVICE);
		controller->SPI_TNPR = device->txnext;
		controller->SPI_RNPR = device->rxnext;
		controller->SPI_TNCR = list->txlen[i];
		controller->SPI_RNCR = list->rxlen[i];
	}
}

/* ......................................................................... */

/*
 * Initialize the SPI controller
 */
static int __init at91_spi_init(void)
{
	init_MUTEX(&spi_lock);

	AT91_CfgPIO_SPI();

	controller->SPI_CR = AT91C_SPI_SWRST;	/* software reset of SPI controller */

	/* Set Chip Select registers to good defaults */
	controller->SPI_CSR0 = AT91C_SPI_CPOL | AT91C_SPI_BITS_8 | (16 << 16) | (DEFAULT_SPI_BAUD << 8);
	controller->SPI_CSR1 = AT91C_SPI_CPOL | AT91C_SPI_BITS_8 | (16 << 16) | (DEFAULT_SPI_BAUD << 8);
	controller->SPI_CSR2 = AT91C_SPI_CPOL | AT91C_SPI_BITS_8 | (16 << 16) | (DEFAULT_SPI_BAUD << 8);
	controller->SPI_CSR3 = AT91C_SPI_CPOL | AT91C_SPI_BITS_8 | (16 << 16) | (DEFAULT_SPI_BAUD << 8);

	controller->SPI_PTCR = AT91C_PDC_RXTDIS | AT91C_PDC_TXTDIS;

	memset(&spi_dev, 0, sizeof(spi_dev));
	spi_dev[0].pcs = 0xE;
	spi_dev[1].pcs = 0xD;
	spi_dev[2].pcs = 0xB;
	spi_dev[3].pcs = 0x7;

	if (request_irq(AT91C_ID_SPI, spi_interrupt, 0, "spi", NULL))
		return -EBUSY;

	controller->SPI_CR = AT91C_SPI_SPIEN;		/* Enable SPI */

	return 0;
}

static void at91_spi_exit(void)
{
	controller->SPI_CR = AT91C_SPI_SPIDIS;		/* Disable SPI */

	free_irq(AT91C_ID_SPI, 0);
}


EXPORT_SYMBOL(spi_access_bus);
EXPORT_SYMBOL(spi_release_bus);
EXPORT_SYMBOL(spi_transfer);

module_init(at91_spi_init);
module_exit(at91_spi_exit);

MODULE_LICENSE("GPL")
MODULE_AUTHOR("Andrew Victor")
MODULE_DESCRIPTION("SPI driver for Atmel AT91RM9200")
