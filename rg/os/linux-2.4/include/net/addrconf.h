#ifndef _ADDRCONF_H
#define _ADDRCONF_H

#include <net/ipv6.h>

#define RETRANS_TIMER	HZ

#define MAX_RTR_SOLICITATIONS		3
#define RTR_SOLICITATION_INTERVAL	(4*HZ)

#define TEMP_VALID_LIFETIME		(7*86400)	/* 1 week */
#define TEMP_PREFERRED_LIFETIME		(86400)		/* 1 day */
#define REGEN_MAX_RETRY			(5)		/* 5 times */
#define MAX_DESYNC_FACTOR		(600)		/* 10 minutes */

#define PINFO_VALID_LIFETIME_INFINITE	0xffffffff	/* infinite lifetime */

#define ADDR_CHECK_FREQUENCY		(120*HZ)

struct prefix_info {
	__u8			type;
	__u8			length;
	__u8			prefix_len;

#if defined(__BIG_ENDIAN_BITFIELD)
	__u8			onlink : 1,
			 	autoconf : 1,
				router_address : 1,
				reserved : 5;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	__u8			reserved : 5,
				router_address : 1,
				autoconf : 1,
				onlink : 1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	__u32			valid;
	__u32			prefered;
	__u32			reserved2;

	struct in6_addr		prefix;
};

#ifdef __KERNEL__

#include <linux/in6.h>
#include <linux/netdevice.h>
#include <net/if_inet6.h>

#define IN6_ADDR_HSIZE		16

struct prefix_element {
	struct list_head list;
	struct prefix_info pinfo;
	unsigned long timestamp;
};

extern void			addrconf_init(void);
extern void			addrconf_cleanup(void);

extern int		        addrconf_notify(struct notifier_block *this, 
						unsigned long event, 
						void * data);

extern int			addrconf_add_ifaddr(void *arg);
extern int			addrconf_del_ifaddr(void *arg);
extern int			addrconf_set_dstaddr(void *arg);

extern int			ipv6_chk_addr(struct in6_addr *addr,
					      struct net_device *dev);
extern int			ipv6_chk_same_addr(struct in6_addr *addr,
					      struct net_device *dev);
extern struct inet6_ifaddr *	ipv6_get_ifaddr(struct in6_addr *addr,
						struct net_device *dev);
extern int			ipv6_dev_get_saddr(struct net_device *dev,
						   struct in6_addr *daddr,
						   struct in6_addr *saddr,
						   int pref_privacy);
extern int			ipv6_get_saddr(struct dst_entry *dst, 
					       struct in6_addr *daddr,
					       struct in6_addr *saddr,
					       int pref_privacy);
extern int			ipv6_get_lladdr(struct net_device *dev, struct in6_addr *);
extern void			addrconf_join_solict(struct net_device *dev,
					struct in6_addr *addr);
extern void			addrconf_leave_solict(struct net_device *dev,
					struct in6_addr *addr);

#define IPV6_ADDR_SCOPE_RESERVED	-2
#define IPV6_ADDR_SCOPE_ANY		-1

#ifdef CONFIG_IPV6_NODEINFO	/* icmp.c */
extern void			icmpv6_ni_join_nigroup_dev(struct net_device *dev);
#endif

/*
 *	multicast prototypes (mcast.c)
 */
extern int			ipv6_sock_mc_join(struct sock *sk, 
						  int ifindex, 
						  struct in6_addr *addr);
extern int			ipv6_sock_mc_drop(struct sock *sk,
						  int ifindex, 
						  struct in6_addr *addr);
extern void			ipv6_sock_mc_close(struct sock *sk);
extern int			inet6_mc_check(struct sock *sk, struct in6_addr *addr);

extern int			ipv6_dev_mc_inc(struct net_device *dev,
						struct in6_addr *addr);
extern int			ipv6_dev_mc_dec(struct net_device *dev,
						struct in6_addr *addr);
extern void			ipv6_mc_up(struct inet6_dev *idev);
extern void			ipv6_mc_down(struct inet6_dev *idev);
extern void			ipv6_mc_init_dev(struct inet6_dev *idev);
extern void			ipv6_mc_destroy_dev(struct inet6_dev *idev);
extern void			addrconf_dad_failure(struct inet6_ifaddr *ifp);

extern int			ipv6_chk_mcast_addr(struct net_device *dev,
						    struct in6_addr *addr);

extern void			addrconf_prefix_rcv(struct net_device *dev,
						    u8 *opt, int len);

/*
 *	anycast prototypes (anycast.c)
 */
extern int			ipv6_sock_ac_join(struct sock *sk, 
						  int ifindex, 
						  struct in6_addr *addr);
extern int			ipv6_sock_ac_drop(struct sock *sk,
						  int ifindex, 
						  struct in6_addr *addr);
extern void			ipv6_sock_ac_close(struct sock *sk);
extern int			inet6_ac_check(struct sock *sk, struct in6_addr *addr, int ifindex);

extern int			ipv6_dev_ac_inc(struct net_device *dev,
						struct in6_addr *addr);
extern int			ipv6_dev_ac_dec(struct net_device *dev,
						struct in6_addr *addr);
extern int			ipv6_chk_acast_addr(struct net_device *dev,
						struct in6_addr *addr);


/*
 *  anycast prototypes (anycast.c) with CONFIG_IPV6_ANYCAST
 */
extern int			ipv6_sock_ac_join(struct sock *sk,
						  int ifindex,
						  struct in6_addr *addr);
extern int			ipv6_sock_ac_drop(struct sock *sk,
						  int ifindex,
						  struct in6_addr *addr);
extern void			ipv6_sock_ac_close(struct sock *sk);
extern int			inet6_ac_check(struct sock *sk, struct in6_addr *addr, int ifindex);

extern int			ipv6_dev_ac_inc(struct net_device *dev,
						struct in6_addr *addr);
extern int			ipv6_dev_ac_dec(struct net_device *dev,
						struct in6_addr *addr);
extern int			ipv6_chk_acast_addr(struct net_device *dev,
						    struct in6_addr *addr);


extern int			addrconf_pfx_adv_rcv(struct net_device *dev,
						     struct prefix_info *pinfo,
						     struct in6_addr *home_addr);

extern int ipv6_get_prefix_entries(struct prefix_info **plist,
				   int ifindex, int plen);

extern int addrconf_pfx_adv_rcv(struct net_device *dev,
				struct prefix_info *pinfo,
				struct in6_addr *home_addr);


/* Device notifier */
extern int register_inet6addr_notifier(struct notifier_block *nb);
extern int unregister_inet6addr_notifier(struct notifier_block *nb);

#ifndef CONFIG_IPV6_DEBUG
static inline struct inet6_dev *
__in6_dev_get(struct net_device *dev)
{
	return (struct inet6_dev *)dev->ip6_ptr;
}
#else
#define __in6_dev_get(dev)						\
({									\
	struct net_device *_dev = (dev);				\
	if (!_dev)							\
		panic("__in6_dev_get(dev=NULL): %s\n", __FUNCTION__);	\
	(struct inet6_dev *)_dev->ip6_ptr;				\
})
#endif

extern rwlock_t addrconf_lock;

#ifndef CONFIG_IPV6_DEBUG
static inline struct inet6_dev *
in6_dev_get(struct net_device *dev)
{
	struct inet6_dev *idev = NULL;
	read_lock(&addrconf_lock);
	idev = dev->ip6_ptr;
	if (idev)
		atomic_inc(&idev->refcnt);
	read_unlock(&addrconf_lock);
	return idev;
}
#else
#define in6_dev_get(dev)						\
({									\
	struct net_device *_dev = (dev);				\
	struct inet6_dev *_idev;					\
	if (!_dev)							\
		panic("in6_dev_get(dev=NULL): %s\n", __FUNCTION__);	\
	read_lock(&addrconf_lock);					\
	_idev = _dev->ip6_ptr;						\
	if (_idev)							\
		atomic_inc(&_idev->refcnt);				\
	read_unlock(&addrconf_lock);					\
	_idev;								\
})
#endif

extern void in6_dev_finish_destroy(struct inet6_dev *idev);

#ifndef CONFIG_IPV6_DEBUG
static inline void
in6_dev_put(struct inet6_dev *idev)
{
	if (atomic_dec_and_test(&idev->refcnt))
		in6_dev_finish_destroy(idev);
}
#else
#define in6_dev_put(idev)						\
({									\
	struct inet6_dev *_idev = (idev);				\
	if (!_idev)							\
		panic("in6_dev_put(idev=NULL): %s\n",			\
			__FUNCTION__);					\
	if (atomic_dec_and_test(&_idev->refcnt))			\
		in6_dev_finish_destroy(_idev);				\
})
#endif

#ifndef CONFIG_IPV6_DEBUG
#define __in6_dev_put(idev)  atomic_dec(&(idev)->refcnt)
#define in6_dev_hold(idev)   atomic_inc(&(idev)->refcnt)
#else
#define __in6_dev_put(idev)						\
({									\
	struct inet6_dev *_idev = (idev);				\
	if (!_idev)							\
		panic("__in6_dev_put(idev=NULL): %s\n", __FUNCTION__);	\
	atomic_dec(&_idev->refcnt);					\
})
#define in6_dev_hold(idev)						\
({									\
	struct inet6_dev *_idev = (idev);				\
	if (!_idev)							\
		panic("in6_dev_hold(idev=NULL): %s\n", __FUNCTION__);	\
	atomic_inc(&_idev->refcnt);					\
})
#endif


extern void inet6_ifa_finish_destroy(struct inet6_ifaddr *ifp);

#ifndef CONFIG_IPV6_DEBUG
static inline void in6_ifa_put(struct inet6_ifaddr *ifp)
{
	if (atomic_dec_and_test(&ifp->refcnt))
		inet6_ifa_finish_destroy(ifp);
}
#else
#define in6_ifa_put(ifp)						\
({									\
	struct inet6_ifaddr *_ifp = (ifp);				\
	if (!_ifp)							\
		panic("in6_ifa_put(ifp=NULL): %s\n", __FUNCTION__);	\
	if (atomic_dec_and_test(&_ifp->refcnt))				\
		inet6_ifa_finish_destroy(_ifp);				\
})
#endif

#ifndef CONFIG_IPV6_DEBUG
#define __in6_ifa_put(ifp)  atomic_dec(&(ifp)->refcnt)
#define in6_ifa_hold(ifp)   atomic_inc(&(ifp)->refcnt)
#else
#define __in6_ifa_put(ifp)						\
({									\
	struct inet6_ifaddr *_ifp = (ifp);				\
	if (!_ifp)							\
		panic("__in6_ifa_put(ifp=NULL): %s\n", __FUNCTION__);	\
	atomic_dec(&_ifp->refcnt);					\
})
#define in6_ifa_hold(ifp)						\
({									\
	struct inet6_ifaddr *_ifp = (ifp);				\
	if (!_ifp)							\
		panic("in6_ifa_hold(ifp=NULL): %s\n", __FUNCTION__);	\
	atomic_inc(&_ifp->refcnt);					\
})
#endif


extern void			addrconf_forwarding_on(void);
/*
 *	Hash function taken from net_alias.c
 */

static __inline__ u8 ipv6_addr_hash(const struct in6_addr *addr)
{	
	__u32 word;

	/* 
	 * We perform the hash function over the last 64 bits of the address
	 * This will include the IEEE address token on links that support it.
	 */

	word = addr->s6_addr[2] ^ addr->s6_addr32[3];
	word  ^= (word>>16);
	word ^= (word >> 8);

	return ((word ^ (word >> 4)) & 0x0f);
}

/*
 *	compute link-local solicited-node multicast address
 */

static inline void addrconf_addr_solict_mult_old(struct in6_addr *addr,
						     struct in6_addr *solicited)
{
	ipv6_addr_set(solicited,
		      __constant_htonl(0xFF020000), 0,
		      __constant_htonl(0x1), addr->s6_addr32[3]);
}

static inline void addrconf_addr_solict_mult(const struct in6_addr *addr,
					     struct in6_addr *solicited)
{
	ipv6_addr_set(solicited,
		      __constant_htonl(0xFF020000), 0,
		      __constant_htonl(0x1),
		      __constant_htonl(0xFF000000) | addr->s6_addr32[3]);
}


static inline void ipv6_addr_all_nodes(struct in6_addr *addr)
{
	ipv6_addr_set(addr,
		      __constant_htonl(0xFF020000), 0, 0,
		      __constant_htonl(0x1));
}

static inline void ipv6_addr_all_routers(struct in6_addr *addr)
{
	ipv6_addr_set(addr,
		      __constant_htonl(0xFF020000), 0, 0,
		      __constant_htonl(0x2));
}

static inline int ipv6_addr_is_multicast(const struct in6_addr *addr)
{
	return (addr->s6_addr32[0] & __constant_htonl(0xFF000000)) == __constant_htonl(0xFF000000);
}

static inline int ipv6_addr_is_ll_all_nodes(const struct in6_addr *addr)
{
	return (addr->s6_addr32[0] == htonl(0xff020000) &&
		addr->s6_addr32[1] == 0 &&
		addr->s6_addr32[2] == 0 &&
		addr->s6_addr32[3] == htonl(0x00000001));
}

static inline int ipv6_addr_is_ll_all_routers(const struct in6_addr *addr)
{
	return (addr->s6_addr32[0] == htonl(0xff020000) &&
		addr->s6_addr32[1] == 0 &&
		addr->s6_addr32[2] == 0 &&
		addr->s6_addr32[3] == htonl(0x00000002));
}

#endif
#endif
