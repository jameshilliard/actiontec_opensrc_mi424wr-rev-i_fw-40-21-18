/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		This file implements the various access functions for the
 *		PROC file system.  This is very similar to the IPv4 version,
 *		except it reports the sockets in the INET6 address family.
 *
 * Version:	$Id: proc.c,v 1.1.1.1 2007/05/07 23:29:16 jungo Exp $
 *
 * Authors:	David S. Miller (davem@caip.rutgers.edu)
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#include <linux/sched.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/in6.h>
#include <linux/stddef.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/transp_v6.h>
#include <net/ipv6.h>
#include <net/addrconf.h>
#include <linux/usagi-version.h>

static int fold_prot_inuse(struct proto *proto)
{
	int res = 0;
	int cpu;

	for (cpu=0; cpu<smp_num_cpus; cpu++)
		res += proto->stats[cpu_logical_map(cpu)].inuse;

	return res;
}

int afinet6_getversion(char *buffer, char **start, off_t offset, int length)
{
	int len = 0;
	len += sprintf(buffer+len, "%s\n", USAGI_RELEASE);
	*start = buffer + offset;
	len -= offset;
	if (len > length)
		len = length;
	return len;
}

int afinet6_get_info(char *buffer, char **start, off_t offset, int length)
{
	int len = 0;
	len += sprintf(buffer+len, "TCP6: inuse %d\n",
		       fold_prot_inuse(&tcpv6_prot));
	len += sprintf(buffer+len, "UDP6: inuse %d\n",
		       fold_prot_inuse(&udpv6_prot));
	len += sprintf(buffer+len, "RAW6: inuse %d\n",
		       fold_prot_inuse(&rawv6_prot));
	len += sprintf(buffer+len, "FRAG6: inuse %d memory %d\n",
		       ip6_frag_nqueues, atomic_read(&ip6_frag_mem));
	*start = buffer + offset;
	len -= offset;
	if(len > length)
		len = length;
	return len;
}

#define OFFSETOF(s,m)	((size_t)(&((s *)NULL)->m))

struct snmp6_item
{
	char *name;
	size_t offset;
} snmp6_list_ipv6[] = {
/* ipv6 mib according to RFC2465 */
#define SNMP6_GEN(x) { #x , OFFSETOF(struct ipv6_mib, x) }
	SNMP6_GEN(Ip6LastChange),
	SNMP6_GEN(Ip6InReceives),
	SNMP6_GEN(Ip6InHdrErrors),
	SNMP6_GEN(Ip6InTooBigErrors),
	SNMP6_GEN(Ip6InNoRoutes),
	SNMP6_GEN(Ip6InAddrErrors),
	SNMP6_GEN(Ip6InUnknownProtos),
	SNMP6_GEN(Ip6InTruncatedPkts),
	SNMP6_GEN(Ip6InDiscards),
	SNMP6_GEN(Ip6InDelivers),
	SNMP6_GEN(Ip6OutForwDatagrams),
	SNMP6_GEN(Ip6OutRequests),
	SNMP6_GEN(Ip6OutDiscards),
	SNMP6_GEN(Ip6OutNoRoutes),
	SNMP6_GEN(Ip6ReasmTimeout),
	SNMP6_GEN(Ip6ReasmReqds),
	SNMP6_GEN(Ip6ReasmOKs),
	SNMP6_GEN(Ip6ReasmFails),
	SNMP6_GEN(Ip6FragOKs),
	SNMP6_GEN(Ip6FragFails),
	SNMP6_GEN(Ip6FragCreates),
	SNMP6_GEN(Ip6InMcastPkts),
	SNMP6_GEN(Ip6OutMcastPkts),
#undef SNMP6_GEN
};
struct snmp6_item snmp6_list_icmpv6[] = {
/* icmpv6 mib according to RFC2466 */
#define SNMP6_GEN(x) { #x , OFFSETOF(struct icmpv6_mib, x) }
	SNMP6_GEN(Icmp6InMsgs),
	SNMP6_GEN(Icmp6InErrors),
	SNMP6_GEN(Icmp6InDestUnreachs),
	SNMP6_GEN(Icmp6InAdminProhibs),
	SNMP6_GEN(Icmp6InTimeExcds),
	SNMP6_GEN(Icmp6InParmProblems),
	SNMP6_GEN(Icmp6InPktTooBigs),
	SNMP6_GEN(Icmp6InEchos),
	SNMP6_GEN(Icmp6InEchoReplies),
	SNMP6_GEN(Icmp6InRouterSolicits),
	SNMP6_GEN(Icmp6InRouterAdvertisements),
	SNMP6_GEN(Icmp6InNeighborSolicits),
	SNMP6_GEN(Icmp6InNeighborAdvertisements),
	SNMP6_GEN(Icmp6InRedirects),
	SNMP6_GEN(Icmp6InGroupMembQueries),
	SNMP6_GEN(Icmp6InGroupMembResponses),
	SNMP6_GEN(Icmp6InGroupMembReductions),
	SNMP6_GEN(Icmp6OutMsgs),
	SNMP6_GEN(Icmp6OutErrors),
	SNMP6_GEN(Icmp6OutDestUnreachs),
	SNMP6_GEN(Icmp6OutAdminProhibs),
	SNMP6_GEN(Icmp6OutTimeExcds),
	SNMP6_GEN(Icmp6OutParmProblems),
	SNMP6_GEN(Icmp6OutPktTooBigs),
	SNMP6_GEN(Icmp6OutEchos),
	SNMP6_GEN(Icmp6OutEchoReplies),
	SNMP6_GEN(Icmp6OutRouterSolicits),
	SNMP6_GEN(Icmp6OutRouterAdvertisements),
	SNMP6_GEN(Icmp6OutNeighborSolicits),
	SNMP6_GEN(Icmp6OutNeighborAdvertisements),
	SNMP6_GEN(Icmp6OutRedirects),
	SNMP6_GEN(Icmp6OutGroupMembQueries),
	SNMP6_GEN(Icmp6OutGroupMembResponses),
	SNMP6_GEN(Icmp6OutGroupMembReductions),
#undef SNMP6_GEN
};
struct snmp6_item snmp6_list_udp[] = {
#define SNMP6_GEN(x) { "Udp6" #x , OFFSETOF(struct udp_mib, Udp##x) }
	SNMP6_GEN(InDatagrams),
	SNMP6_GEN(NoPorts),
	SNMP6_GEN(InErrors),
	SNMP6_GEN(OutDatagrams)
#undef SNMP6_GEN
};

#define fold_field(base,offset) ({ \
	unsigned long res = 0;			\
	int cpu;					\
	for (cpu=0 ; cpu<smp_num_cpus; cpu++) {	\
		res += *((unsigned long *)(((char *)&base[2*cpu_logical_map(cpu)]) + offset)) +	\
		       *((unsigned long *)(((char *)&base[2*cpu_logical_map(cpu)+1]) + offset));	\
	}					\
	res;					\
})

#define snmp6_explore(_base,_list,_offset,_length,_done,_buffer,_start) ({			\
	off_t _d = 0;										\
	int _i, _l;										\
	for (_i=0; _i<sizeof(_list)/sizeof(_list[0]); _i++){					\
		if ((_done+_d) >= _offset + _length)						\
			break;									\
		_l = sprintf((_done+_d) <= _offset ? _buffer : *_start + (_done+_d) - _offset,	\
			     "%-32s\t%ld\n", _list[_i].name,					\
			     fold_field(_base,_list[_i].offset));				\
		if ((_done+_d) <= _offset && (_done+_d)+_l >= _offset)				\
			*_start = _buffer + _offset - (_done+_d);				\
		_d += _l;									\
	}											\
	_d;											\
})
#define snmp6_explore_dev(_base,_list,_index,_offset,_length,_done,_buffer,_start) ({		\
	char _pbuf[256];	/*XXX*/								\
	off_t _d = 0;										\
	int _i, _l;										\
	for (_i=0; _i<sizeof(_list)/sizeof(_list[0]); _i++){					\
		if ((_done+_d) >= _offset + _length)						\
			break;									\
		sprintf(_pbuf, "%s.%u", _list[_i].name, _index);				\
		_l = sprintf((_done+_d) <= _offset ? _buffer : *_start + (_done+_d) - _offset,	\
			     "%-32s\t%ld\n", _pbuf,						\
			     fold_field(_base,_list[_i].offset));				\
		if ((_done+_d) <= _offset && (_done+_d)+_l >= _offset)				\
			*_start = _buffer + _offset - (_done+_d);				\
		_d += _l;									\
	}											\
	_d;											\
})

int afinet6_get_snmp(char *buffer, char **start, off_t offset, int length)
{
	int len = 0;
	off_t done = 0;

	done += snmp6_explore(ipv6_statistics,snmp6_list_ipv6,offset,length,done,buffer,start);
	done += snmp6_explore(icmpv6_statistics,snmp6_list_icmpv6,offset,length,done,buffer,start);
	done += snmp6_explore(udp_stats_in6,snmp6_list_udp,offset,length,done,buffer,start);

	len = done - offset;

	if (len > length)
		len = length;
	if (len < 0)
		len = 0;

	return len;
}

int afinet6_read_devsnmp(char *buffer, char **start, off_t offset, int length, int *eof, void *data)
{
	int len = 0;
	struct net_device *dev;
	struct inet6_dev *idev = data;
	off_t done = 0;

	if (idev){
		done += snmp6_explore(idev->stats.ipv6,snmp6_list_ipv6,offset,length,done,buffer,start);
		done += snmp6_explore(idev->stats.icmpv6,snmp6_list_icmpv6,offset,length,done,buffer,start);
	}
	else{
		read_lock(&dev_base_lock);
		for (dev=dev_base; dev; dev=dev->next) {
			struct inet6_dev *idev = in6_dev_get(dev);
			if (!idev)
				continue;
			done += snmp6_explore_dev(idev->stats.ipv6,snmp6_list_ipv6,dev->ifindex,offset,length,done,buffer,start);
			done += snmp6_explore_dev(idev->stats.icmpv6,snmp6_list_icmpv6,dev->ifindex,offset,length,done,buffer,start);
			in6_dev_put(idev);
		}
		read_unlock(&dev_base_lock);
	}

	if (done > offset){
		len = done - offset;
		if (len > length)
			len = length;
		else if (len < length)
			*eof = 1;
	}
	else{
		len = 0;
		*eof = 1;
	}

	return len;
}
