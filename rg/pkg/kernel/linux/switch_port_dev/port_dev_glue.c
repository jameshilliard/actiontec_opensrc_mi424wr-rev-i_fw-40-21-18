/****************************************************************************
 *
 * rg/pkg/kernel/linux/switch_port_dev/port_dev_glue.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <linux/module.h>
#include <linux/netdevice.h>

/* The code in this file is linked statically to Linux, and serves as a glue
 * layer between the switch port virtual device, and the physical switch device
 * drivers */

/* switch_port_dev_update_stats (if not NULL) is used to update stats by
 * network drivers. It will examine the skb, and may decide that not the
 * calling device's stats should be updated, but rather another (probably
 * virtual) device's. If it decides that the calling device's stats should be
 * updated it will use physical_stats for it, otherwise physical_stats is not
 * updated and another, calculated, stats struct is updated instead.
 */
void (*switch_port_dev_update_stats)(struct sk_buff *skb,
    struct net_device_stats *physical_stats,
    int rx_packets, int tx_packets, int rx_bytes, int tx_bytes,
    int rx_dropped, int rx_errors);
EXPORT_SYMBOL(switch_port_dev_update_stats);

/* switch_port_dev_handle_received_packet (if not NULL) serves as an
 * alternative to netif_rx and friends, to allow re-routing of the packet to a
 * virtual device, and process the packet further, as required. The physical
 * switch device driver should call this function, and only if it returns zero
 * (not handled) call netif_rx to pass the packet to the kernel. */
int (*switch_port_dev_handle_received_packet)(struct sk_buff *skb);
EXPORT_SYMBOL(switch_port_dev_handle_received_packet);
