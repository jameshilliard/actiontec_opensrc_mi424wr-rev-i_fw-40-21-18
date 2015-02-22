/*
 *  linux/drivers/l3/l3-core.c
 *
 *  Copyright (C) 2001 Russell King
 *
 *  General structure taken from i2c-core.c by Simon G. Vogl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 *  See linux/Documentation/l3 for further documentation.
 */
#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/l3/l3.h>

static DECLARE_MUTEX(adapter_lock);
static LIST_HEAD(adapter_list);

static DECLARE_MUTEX(driver_lock);
static LIST_HEAD(driver_list);

/**
 * l3_add_adapter - register a new L3 bus adapter
 * @adap: l3_adapter structure for the registering adapter
 *
 * Make the adapter available for use by clients using name adap->name.
 * The adap->adapters list is initialised by this function.
 *
 * Returns 0;
 */
int l3_add_adapter(struct l3_adapter *adap)
{
	INIT_LIST_HEAD(&adap->clients);
	down(&adapter_lock);
	list_add(&adap->adapters, &adapter_list);
	up(&adapter_lock);
	return 0;	
}

/**
 * l3_del_adapter - unregister a L3 bus adapter
 * @adap: l3_adapter structure to unregister
 *
 * Remove an adapter from the list of available L3 Bus adapters.
 *
 * Returns 0;
 */
int l3_del_adapter(struct l3_adapter *adap)
{
	down(&adapter_lock);
	list_del(&adap->adapters);
	up(&adapter_lock);
	return 0;
}

static struct l3_adapter *__l3_get_adapter(const char *name)
{
	struct list_head *l;

	list_for_each(l, &adapter_list) {
		struct l3_adapter *adap = list_entry(l, struct l3_adapter, adapters);

		if (strcmp(adap->name, name) == 0)
			return adap;
	}

	return NULL;
}

/**
 * l3_get_adapter - get a reference to an adapter
 * @name: driver name
 *
 * Obtain a l3_adapter structure for the specified adapter.  If the adapter
 * is not currently load, then load it.  The adapter will be locked in core
 * until all references are released via l3_put_adapter.
 */
struct l3_adapter *l3_get_adapter(const char *name)
{
	struct l3_adapter *adap;
	int try;

	for (try = 0; try < 2; try ++) {
		down(&adapter_lock);
		adap = __l3_get_adapter(name);
		if (adap && !try_inc_mod_count(adap->owner))
			adap = NULL;
		up(&adapter_lock);

		if (adap)
			break;

		if (try == 0)
			request_module(name);
	}

	return adap;
}

/**
 * l3_put_adapter - release a reference to an adapter
 * @adap: driver to release reference
 *
 * Indicate to the L3 core that you no longer require the adapter reference.
 * The adapter module may be unloaded when there are no references to its
 * data structure.
 *
 * You must not use the reference after calling this function.
 */
void l3_put_adapter(struct l3_adapter *adap)
{
	if (adap && adap->owner)
		__MOD_DEC_USE_COUNT(adap->owner);
}

/**
 * l3_add_driver - register a new L3 device driver
 * @driver - driver structure to make available
 *
 * Make the driver available for use by clients using name driver->name.
 * The driver->drivers list is initialised by this function.
 *
 * Returns 0;
 */
int l3_add_driver(struct l3_driver *driver)
{
	down(&driver_lock);
	list_add(&driver->drivers, &driver_list);
	up(&driver_lock);
	return 0;
}

/**
 * l3_del_driver - unregister a L3 device driver
 * @driver: driver to remove
 *
 * Remove an driver from the list of available L3 Bus device drivers.
 *
 * Returns 0;
 */
int l3_del_driver(struct l3_driver *driver)
{
	down(&driver_lock);
	list_del(&driver->drivers);
	up(&driver_lock);
	return 0;
}

static struct l3_driver *__l3_get_driver(const char *name)
{
	struct list_head *l;

	list_for_each(l, &driver_list) {
		struct l3_driver *drv = list_entry(l, struct l3_driver, drivers);

		if (strcmp(drv->name, name) == 0)
			return drv;
	}

	return NULL;
}

/**
 * l3_get_driver - get a reference to a driver
 * @name: driver name
 *
 * Obtain a l3_driver structure for the specified driver.  If the driver is
 * not currently load, then load it.  The driver will be locked in core
 * until all references are released via l3_put_driver.
 */
struct l3_driver *l3_get_driver(const char *name)
{
	struct l3_driver *drv;
	int try;

	for (try = 0; try < 2; try ++) {
		down(&adapter_lock);
		drv = __l3_get_driver(name);
		if (drv && !try_inc_mod_count(drv->owner))
			drv = NULL;
		up(&adapter_lock);

		if (drv)
			break;

		if (try == 0)
			request_module(name);
	}

	return drv;
}

/**
 * l3_put_driver - release a reference to a driver
 * @drv: driver to release reference
 *
 * Indicate to the L3 core that you no longer require the driver reference.
 * The driver module may be unloaded when there are no references to its
 * data structure.
 *
 * You must not use the reference after calling this function.
 */
void l3_put_driver(struct l3_driver *drv)
{
	if (drv && drv->owner)
		__MOD_DEC_USE_COUNT(drv->owner);
}

/**
 * l3_attach_client - attach a client to an adapter and driver
 * @client: client structure to attach
 * @adap: adapter (module) name
 * @drv: driver (module) name
 *
 * Attempt to attach a client (a user of a device driver) to a particular
 * driver and adapter.  If the specified driver or adapter aren't registered,
 * request_module is used to load the relevant modules.
 *
 * Returns 0 on success, or negative error code.
 */
int l3_attach_client(struct l3_client *client, const char *adap, const char *drv)
{
	struct l3_adapter *adapter = l3_get_adapter(adap);
	struct l3_driver  *driver = l3_get_driver(drv);
	int ret = -ENOENT;

	if (!adapter)
		printk(KERN_ERR "%s: unable to get adapter: %s\n",
			__FUNCTION__, adap);
	if (!driver)
		printk(KERN_ERR "%s: unable to get driver: %s\n",
			__FUNCTION__, drv);

	if (adapter && driver) {
		ret = 0;

		client->adapter = adapter;
		client->driver  = driver;

		list_add(&client->__adap, &adapter->clients);

		if (driver->attach_client)
			ret = driver->attach_client(client);
	}

	if (ret) {
		l3_put_driver(driver);
		l3_put_adapter(adapter);
	}
	return ret;
}

/**
 * l3_detach_client - detach a client from an adapter and driver
 * @client: client structure to detach
 *
 * Detach the client from the adapter and driver.
 */
int l3_detach_client(struct l3_client *client)
{
	struct l3_adapter *adapter = client->adapter;
	struct l3_driver  *driver = client->driver;

	driver->detach_client(client);

	client->adapter = NULL;
	client->driver  = NULL;

	l3_put_driver(driver);
	l3_put_adapter(adapter);

	list_del(&client->__adap);

	return 0;
}

/**
 * l3_transfer - transfer information on an L3 bus
 * @adap: adapter structure to perform transfer on
 * @msgs: array of l3_msg structures describing transfer
 * @num: number of l3_msg structures
 *
 * Transfer the specified messages to/from a device on the L3 bus.
 *
 * Returns number of messages successfully transferred, otherwise negative
 * error code.
 */
int l3_transfer(struct l3_adapter *adap, struct l3_msg msgs[], int num)
{
	int ret = -ENOSYS;

	if (adap->algo->xfer) {
		down(adap->lock);
		ret = adap->algo->xfer(adap, msgs, num);
		up(adap->lock);
	}
	return ret;
}

/**
 * l3_write - send data to a device on an L3 bus
 * @client: registered client structure
 * @addr: L3 bus address
 * @buf: buffer for bytes to send
 * @len: number of bytes to send
 *
 * Send len bytes pointed to by buf to device address addr on the L3 bus
 * described by client.
 *
 * Returns the number of bytes transferred, or negative error code.
 */
int l3_write(struct l3_client *client, int addr, const char *buf, int len)
{
	struct l3_adapter *adap = client->adapter;
	struct l3_msg msg;
	int ret;

	msg.addr = addr;
	msg.flags = 0;
	msg.buf = (char *)buf;
	msg.len = len;

	ret = l3_transfer(adap, &msg, 1);
	return ret == 1 ? len : ret;
}

/**
 * l3_read - receive data from a device on an L3 bus
 * @client: registered client structure
 * @addr: L3 bus address
 * @buf: buffer for bytes to receive
 * @len: number of bytes to receive
 *
 * Receive len bytes from device address addr on the L3 bus described by
 * client to a buffer pointed to by buf.
 *
 * Returns the number of bytes transferred, or negative error code.
 */
int l3_read(struct l3_client *client, int addr, char *buf, int len)
{
	struct l3_adapter *adap = client->adapter;
	struct l3_msg msg;
	int ret;

	msg.addr = addr;
	msg.flags = L3_M_RD;
	msg.buf = buf;
	msg.len = len;

	ret = l3_transfer(adap, &msg, 1);
	return ret == 1 ? len : ret;
}

EXPORT_SYMBOL(l3_add_adapter);
EXPORT_SYMBOL(l3_del_adapter);
EXPORT_SYMBOL(l3_get_adapter);
EXPORT_SYMBOL(l3_put_adapter);

EXPORT_SYMBOL(l3_add_driver);
EXPORT_SYMBOL(l3_del_driver);
EXPORT_SYMBOL(l3_get_driver);
EXPORT_SYMBOL(l3_put_driver);

EXPORT_SYMBOL(l3_attach_client);
EXPORT_SYMBOL(l3_detach_client);

EXPORT_SYMBOL(l3_transfer);
EXPORT_SYMBOL(l3_write);
EXPORT_SYMBOL(l3_read);
