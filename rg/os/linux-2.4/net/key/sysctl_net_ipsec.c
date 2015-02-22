/* $USAGI: sysctl_net_ipsec.c,v 1.2.6.1 2002/09/02 04:05:33 miyazawa Exp $ */

/*
 * Copyright (C)2001 USAGI/WIDE Project
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
 */

#include <linux/config.h>
#include <linux/sysctl.h>

/* extern */ int sysctl_ipsec_replay_window = 1;
/* extern */ int sysctl_ipsec_debug_ipv6 = 0;
/* extern */ int sysctl_ipsec_debug_pfkey= 0;
/* extern */ int sysctl_ipsec_debug_sadb = 0;
/* extern */ int sysctl_ipsec_debug_spd = 0;

ctl_table ipsec_table[] = {
	{NET_IPSEC_REPLAY_WINDOW, "replay_window_check", &sysctl_ipsec_replay_window, sizeof(int), 0600, NULL, proc_dointvec},
#ifdef CONFIG_IPSEC_DEBUG
	{NET_IPSEC_DEBUG_IPV6, "debug_ipv6", &sysctl_ipsec_debug_ipv6, sizeof(int), 0600, NULL, proc_dointvec},
	{NET_IPSEC_DEBUG_PFKEY, "debug_pfkey", &sysctl_ipsec_debug_pfkey, sizeof(int), 0600, NULL, proc_dointvec},
	{NET_IPSEC_DEBUG_SADB, "debug_sadb", &sysctl_ipsec_debug_sadb, sizeof(int), 0600, NULL, proc_dointvec},
	{NET_IPSEC_DEBUG_SPD, "debug_spd", &sysctl_ipsec_debug_spd, sizeof(int), 0600, NULL, proc_dointvec},
#endif /* CONFIG_IPSEC_DEBUG */
	{0},
};

static ctl_table ipsec_net_table[] = {
	{NET_IPSEC, "ipsec", NULL, 0, 0555, ipsec_table},
	{0}
};

static ctl_table ipsec_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, ipsec_net_table},
	{0}
};

static struct ctl_table_header *ipsec_sysctl_header;

void ipsec_sysctl_register(void)
{
	ipsec_sysctl_header = register_sysctl_table(ipsec_root_table, 0);
}

void ipsec_sysctl_unregister(void)
{
	unregister_sysctl_table(ipsec_sysctl_header);
}

