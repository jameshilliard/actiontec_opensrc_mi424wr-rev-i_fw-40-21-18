#ifndef __LINUX_EPXAPLD_H
#define __LINUX_EPXAPLD_H

/*
 *  linux/drivers/char/pld/epxapld.h
 *
 *  Pld driver for Altera EPXA Excalibur devices
 *
 *  Copyright 2001 Altera Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id: pld_epxa.h,v 1.1.1.1 2007/05/07 23:29:55 jungo Exp $
 *
 */
#define PLD_IOC_MAGIC 'p'
#if !defined(KERNEL) || defined(CONFIG_PLD_HOTSWAP)
#define PLD_IOC_ADD_PLD_DEV _IOW(PLD_IOC_MAGIC, 0xa0, struct pldhs_dev_desc)
#define PLD_IOC_REMOVE_PLD_DEVS _IO(PLD_IOC_MAGIC, 0xa1)
#define PLD_IOC_SET_INT_MODE _IOW(PLD_IOC_MAGIC, 0xa2, int)
#endif

#endif
