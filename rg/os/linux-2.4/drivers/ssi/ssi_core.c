/*
 *  linux/drivers/ssi/ssi_core.c
 *
 * This file provides a common framework to allow multiple SSI devices
 * to work together on a single SSI bus.
 *
 * You can use this in two ways:
 *  1. select the device, queue up data, flush the data to the device,
 *     (optionally) purge the received data, deselect the device.
 *  2. select the device, queue up one data word, flush to the device
 *     read data word, queue up next data word, flush to the device...
 *     deselect the device.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/init.h>

#include <asm/errno.h>

#include "ssi_bus.h"
#include "ssi_dev.h"

#define DEBUG

/**
 *	ssi_core_rcv - pass received SSI data to the device
 *	@bus: the bus that the data was received from
 *	@data: the data word that was received
 *
 *	This function is intended to be called by SSI bus device
 *	drivers to pass received data to the device driver.
 */
int ssi_core_rcv(struct ssi_bus *bus, u_int data)
{
	struct ssi_dev *dev = bus->dev;

	if (dev && dev->rcv)
		dev->rcv(dev, data);

	return 0;
}

/**
 *	ssi_transmit_data - queue SSI data for later transmission
 *	@dev: device requesting data to be transmitted
 *	@data: data word to be transmitted.
 *
 *	Queue one data word of SSI data for later transmission.
 */
int ssi_transmit_data(struct ssi_dev *dev, u_int data)
{
	struct ssi_bus *bus = dev->bus;

	/*
	 * Make sure that we currently own the bus
	 */
	if (bus->dev != dev)
		BUG();

	bus->trans(bus, data);
	return 0;
}

/**
 *	ssi_select_device - select a SSI device for later transactions
 *	@dev: device to be selected
 */
int ssi_select_device(struct ssi_bus *bus, struct ssi_dev *dev)
{
	int retval;

#ifdef DEBUG
	printk("SSI: selecting device %s on bus %s\n",
		dev ? dev->name : "<none>", bus->name);
#endif

	/*
	 * Select the device if it wasn't already selected.
	 */
	retval = 0;
	if (bus->dev != dev) {
		retval = bus->select(bus, dev);
		bus->dev = dev;
	}

	return retval;
}

/**
 *	ssi_register_device - register a SSI device with a SSI bus
 *	@bus: bus
 *	@dev: SSI device
 */
int ssi_register_device(struct ssi_bus *bus, struct ssi_dev *dev)
{
	int retval;

	dev->bus = bus;
	bus->devices++;
	retval = dev->init(dev);
	if (retval != 0) {
		dev->bus = NULL;
		bus->devices--;
	} else {
#ifdef DEBUG
		printk("SSI: registered new device %s on bus %s\n", dev->name, bus->name);
#endif
	}
	return retval;
}

/**
 *	ssi_unregister_device - unregister a SSI device from a SSI bus
 *	@dev: SSI device
 */
int ssi_unregister_device(struct ssi_dev *dev)
{
	struct ssi_bus *bus = dev->bus;

	if (bus->dev == dev)
		bus->dev = NULL;

	dev->bus = NULL;
	bus->devices--;
#ifdef DEBUG
	printk("SSI: unregistered device %s on bus %s\n", dev->name, bus->name);
#endif
	return 0;
}

/**
 *	ssi_register_bus - register a SSI bus driver
 *	@bus: bus
 */
int ssi_register_bus(struct ssi_bus *bus)
{
	int retval;

	retval = bus->init(bus);
	if (retval == 0) {
		bus->devices = 0;
#ifdef DEBUG
		printk("SSI: registered new bus %s\n", bus->name);
#endif
	}

	return retval;
}

/**
 *	ssi_unregister_bus - unregister a SSI bus driver
 *	@bus: bus
 */
int ssi_unregister_bus(struct ssi_bus *bus)
{
	int retval = -EBUSY;
	if (bus->devices == 0) {
		retval = 0;
	}
	return retval;
}

static int __init ssi_init(void)
{
	return 0;
}

static void __exit ssi_exit(void)
{
}

module_init(ssi_init);
module_exit(ssi_exit);
