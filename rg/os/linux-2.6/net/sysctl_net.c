/* -*- linux-c -*-
 * sysctl_net.c: sysctl interface to net subsystem.
 *
 * Begun April 1, 1996, Mike Shaver.
 * Added /proc/sys/net directories for each protocol family. [MS]
 *
 * $Log: sysctl_net.c,v $
 * Revision 1.1.1.1  2007/05/07 23:30:07  jungo
 * Import of Jungo RG SDK 4.6.2
 *
 * Revision 1.3  2006/06/15 21:51:23  itay
 * B33147 Upgrade kernel to linux-2.6.16.14
 * OPTIONS: novalidate
 *
 * Revision 1.2.34.1  2006/06/14 11:30:25  itay
 * B33147 Upgrade kernel to linux-2.6.16.14 and add support for broadcom 6358 (merge from branch-4_2_3_2)
 * OPTIONS: novalidate
 *
 * Revision 1.2.28.1  2006/05/17 15:47:16  itay
 * B33147 merge changes between linux-2_6_12 and linux-2_6_16_14
 * NOTIFY: cancel
 * OPTIONS: novalidate
 *
 * Revision 1.2  2005/08/08 07:34:47  noams
 * B24862 Initial import of linux-2.6.12
 *
 * Revision 1.1.2.1  2005/08/07 13:36:58  noams
 * B24862 Initial import of linux-2.6.12
 * Revision 1.1.2.2  2006/02/03 08:17:39  igor
 * Import linux kernel 2.6.15 to vendor branch
 *
 * Revision 1.2  1996/05/08  20:24:40  shaver
 * Added bits for NET_BRIDGE and the NET_IPV4_ARP stuff and
 * NET_IPV4_IP_FORWARD.
 *
 *
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/sysctl.h>

#include <net/sock.h>

#ifdef CONFIG_INET
#include <net/ip.h>
#endif

#ifdef CONFIG_NET
#include <linux/if_ether.h>
#endif

#ifdef CONFIG_TR
#include <linux/if_tr.h>
#endif

struct ctl_table net_table[] = {
	{
		.ctl_name	= NET_CORE,
		.procname	= "core",
		.mode		= 0555,
		.child		= core_table,
	},
#ifdef CONFIG_NET
	{
		.ctl_name	= NET_ETHER,
		.procname	= "ethernet",
		.mode		= 0555,
		.child		= ether_table,
	},
#endif
#ifdef CONFIG_INET
	{
		.ctl_name	= NET_IPV4,
		.procname	= "ipv4",
		.mode		= 0555,
		.child		= ipv4_table
	},
#endif
#ifdef CONFIG_TR
	{
		.ctl_name	= NET_TR,
		.procname	= "token-ring",
		.mode		= 0555,
		.child		= tr_table,
	},
#endif
	{ 0 },
};
