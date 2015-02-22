/*
 *  linux/include/linux/l3/l3.h
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * Derived from i2c.h by Simon G. Vogl
 */
#ifndef L3_H
#define L3_H

struct l3_msg {
	unsigned char	addr;	/* slave address	*/
	unsigned char	flags;		
#define L3_M_RD		0x01
#define L3_M_NOADDR	0x02
	unsigned short	len;	/* msg length		*/
	unsigned char	*buf;	/* pointer to msg data	*/
};

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/list.h>

struct l3_client;

struct l3_ops {
	int	(*open)(struct l3_client *);
	int	(*command)(struct l3_client *, int cmd, void *arg);
	void	(*close)(struct l3_client *);
};

/*
 * A driver is capable of handling one or more physical devices present on
 * L3 adapters. This information is used to inform the driver of adapter
 * events.
 */
struct l3_driver {
	/*
	 * This name is used to uniquely identify the driver.
	 * It should be the same as the module name.
	 */
	char			name[32];

	/*
	 * Notifies the driver that a new client wishes to use its
	 * services.  Note that the module use count will be increased
	 * prior to this function being called.  In addition, the
	 * clients driver and adapter fields will have been setup.
	 */
	int			(*attach_client)(struct l3_client *);

	/*
	 * Notifies the driver that the client has finished with its
	 * services, and any memory that it allocated for this client
	 * should be cleaned up.  In addition the chip should be
	 * shut down.
	 */
	void			(*detach_client)(struct l3_client *);

	/*
	 * Possible operations on the driver.
	 */
	struct l3_ops		*ops;

	/*
	 * Module structure, if any.	
	 */
	struct module		*owner;

	/*
	 * drivers list
	 */
	struct list_head	drivers;
};

struct l3_adapter;

struct l3_algorithm {
	/* textual description */
	char name[32];

	/* perform bus transactions */
	int (*xfer)(struct l3_adapter *, struct l3_msg msgs[], int num);
};

struct semaphore;

/*
 * l3_adapter is the structure used to identify a physical L3 bus along
 * with the access algorithms necessary to access it.
 */
struct l3_adapter {
	/*
	 * This name is used to uniquely identify the adapter.
	 * It should be the same as the module name.
	 */
	char			name[32];

	/*
	 * the algorithm to access the bus
	 */
	struct l3_algorithm	*algo;

	/*
	 * Algorithm specific data
	 */
	void			*algo_data;

	/*
	 * This may be NULL, or should point to the module struct
	 */
	struct module		*owner;

	/*
	 * private data for the adapter
	 */
	void			*data;

	/*
	 * Our lock.  Unlike the i2c layer, we allow this to be used for
	 * other stuff, like the i2c layer lock.  Some people implement
	 * i2c stuff using the same signals as the l3 bus.
	 */
	struct semaphore	*lock;

	/*
	 * List of attached clients.
	 */
	struct list_head	clients;

	/*
	 * List of all adapters.
	 */
	struct list_head	adapters;
};

/*
 * l3_client identifies a single device (i.e. chip) that is connected to an 
 * L3 bus. The behaviour is defined by the routines of the driver. This
 * function is mainly used for lookup & other admin. functions.
 */
struct l3_client {
	struct l3_adapter	*adapter;	/* the adapter we sit on	*/
	struct l3_driver	*driver;	/* and our access routines	*/
	void			*driver_data;	/* private driver data		*/
	struct list_head	__adap;
};


extern int l3_add_adapter(struct l3_adapter *);
extern int l3_del_adapter(struct l3_adapter *);

extern int l3_add_driver(struct l3_driver *);
extern int l3_del_driver(struct l3_driver *);

extern int l3_attach_client(struct l3_client *, const char *, const char *);
extern int l3_detach_client(struct l3_client *);

extern int l3_transfer(struct l3_adapter *, struct l3_msg msgs[], int);
extern int l3_write(struct l3_client *, int, const char *, int);
extern int l3_read(struct l3_client *, int, char *, int);

/**
 * l3_command - send a command to a L3 device driver
 * @client: registered client structure
 * @cmd: device driver command
 * @arg: device driver arguments
 *
 * Ask the L3 device driver to perform some function.  Further information
 * should be sought from the device driver in question.
 *
 * Returns negative error code on failure.
 */
static inline int l3_command(struct l3_client *clnt, int cmd, void *arg)
{
	struct l3_ops *ops = clnt->driver->ops;
	int ret = -EINVAL;

	if (ops && ops->command)
		ret = ops->command(clnt, cmd, arg);

	return ret;
}

static inline int l3_open(struct l3_client *clnt)
{
	struct l3_ops *ops = clnt->driver->ops;
	int ret = 0;

	if (ops && ops->open)
		ret = ops->open(clnt);
	return ret;
}

static inline void l3_close(struct l3_client *clnt)
{
	struct l3_ops *ops = clnt->driver->ops;
	if (ops && ops->close)
		ops->close(clnt);
}
#endif

#endif /* L3_H */
