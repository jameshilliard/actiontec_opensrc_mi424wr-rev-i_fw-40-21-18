/* IPsec Security Association Implementation */
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

#ifndef __SADB_H
#define __SADB_H

#include <linux/types.h>
#include <linux/socket.h>	/* sockaddr_storage */
#include <linux/list.h>		/* list_entry */
#include <linux/spinlock.h>	/* rw_lock_t */
#include <linux/crypto.h>
#include <asm/atomic.h>

/* internally reserved SPI values */
#define IPSEC_SPI_DROP		256
#define IPSEC_SPI_BYPASS	257

#ifdef __KERNEL__

#ifdef CONFIG_IPSEC_DEBUG
# ifdef CONFIG_SYSCTL
#  define SADB_DEBUG(fmt, args...)					\
do {									\
	if (sysctl_ipsec_debug_sadb) {					\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args);	\
	}								\
} while(0)
# else
#  define SADB_DEBUG(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
# endif /* CONFIG_SYSCTL */
#else
# define SADB_DEBUG(fmt, args...)
#endif


#define KEY_LEN_MAX      24
#define AUTH_KEY_LEN_MAX 20         /* enough for HMAC SHA1 key length */
#define ESP_KEY_LEN_MAX  384        /* enough for 3DES key length */
#define ESP_IV_LEN_MAX   16
#define IPSEC_SPI_ANY    0xFFFFFFFF

extern struct list_head sadb_list;
extern rwlock_t sadb_lock; 

/* The sa_replay_window holds information for a replay window.*/
struct sa_replay_window{
	__u8	overflow;
	__u8	size;
	__u32   seq_num;
	__u32	last_seq;
	__u32	bitmap;
};

/* The sa_lifetime holds lifetime for a specific SA. */
struct sa_lifetime{
	__u64	bytes;
	__u64	addtime;
	__u64	usetime;
	__u32	allocations;
};

/* We use kerneli transformation. */
struct auth_algo_info{
	__u8	algo;
	__u8	*key;
	__u16	key_len; /* key length in byte */
	__u16	digest_len;
	struct digest_context *dx;
	
};

struct esp_algo_info{
	__u8	algo;
	__u8	*key;
	__u16	key_len; /* key length in byte */
	__u32   *iv;
	struct cipher_context *cx;
};

/* Security Association (SA)
 *
 * RFC2401 ch4.4 says
 * 
 *    Each interface for which IPsec is enabled requires nominally separate
 *    inbound vs. outbound databases (SAD and SPD), because of the
 *    directionality of many of the fields that are used as selectors.
 *
 * But we don't care whether SA is inbound or outbound.
 * Because it is enough to check src/dst address pair of SA. */
struct ipsec_sa {

	/* Never touch entry without lock sadb_lock */
	struct list_head entry;

	/* list entry for temporary work */
	struct list_head tmp_entry;

	/* reference count describes the number of sa_index_t */
	atomic_t refcnt;

	/* This lock only affect elements except for entry */
	rwlock_t lock;

	/* The sadb_address_proto field is normally zero,
	   which MUST be filled with the transport protocol's number. */
	__u8 proto;

	/* SA status */
	__u8 state;

	/* tunnel mode */
	__u8 tunnel_mode;

	/* ipsec protocol like AH, ESP, IPCOMP */
	__u8 ipsec_proto;

	/* SPI: Security Parameter Index (network byte order) 
	 * or 
	 * CPI: Compression Prameter Index 
	 *      IPcomp algorithm(CPI): deflate=2, (lhz=3, lzjh) (network byte order) 
	 *      in case of CPI, we use lower u16 as CPI. */ 
	__u32 spi;
	 
	/* SA destination address */
	struct sockaddr_storage dst;

	/* SA source address */
	struct sockaddr_storage src;

	/* SA proxy address (RFC specified but we doesn't used) */
	/* struct sockaddr_storage pxy; */

	/* SA source address prefix len */
	__u8 prefixlen_s;

	/* SA dst address prefix len */
	__u8 prefixlen_d;

	/* SA proxy address prefix len (not use) */
	/* __u8 prefixlen_p; */

	/* replay window */
	struct sa_replay_window replay_window;

	/* liftetime for this SA, giving an upper limit
	   for the sequence number */
	struct sa_lifetime lifetime_s; /* soft lifetime */
	struct sa_lifetime lifetime_h; /* hard lifetime */
	struct sa_lifetime lifetime_c; /* current lifetime */
	/* internal use, because uint64 and jiffies are different unit */
	unsigned long init_time;       /* initial time (unit: jiffies) */
	unsigned long fuse_time;       /* first use time (unit: jiffies) */

	struct timer_list timer;

	/* algo is the number of the algorithm in use */
	struct auth_algo_info auth_algo;
	struct esp_algo_info esp_algo;
};

struct sa_index{

        struct list_head entry;

	/* SA doesn't always exist. 
	 * If SA is null, it means suitable SA doesn't exist 
	 * in SA list(it consists of struct ipsec_sa_t). */
        struct ipsec_sa *sa;

	struct sockaddr_storage dst;
	__u32  spi;
	__u8	prefixlen_d;
	__u8	ipsec_proto;
};

int sadb_init(void);
int sadb_cleanup(void);

/* The function sadb_append copies entry to own list. */
/* It is able to free the parameter entry. */
struct ipsec_sa* ipsec_sa_kmalloc(void);
int ipsec_sa_init(struct ipsec_sa *sa);
void ipsec_sa_kfree(struct ipsec_sa *sa);
int ipsec_sa_copy(struct ipsec_sa *dst, struct ipsec_sa *src);
void ipsec_sa_lifetime_check(unsigned long data);
void ipsec_sa_put(struct ipsec_sa *sa);
static inline void ipsec_sa_hold(struct ipsec_sa *sa)
{
	if(sa) atomic_inc(&sa->refcnt);
};

int sadb_append(struct ipsec_sa *sa);
void sadb_remove(struct ipsec_sa *sa);
void sadb_clear_db(void);
int sadb_flush_sa(int satype);

/* These functions expect the parameter SA has been allocated. */
struct ipsec_sa* sadb_find_by_sa_index(struct sa_index *sa_index);
int sadb_find_by_address_proto_spi(struct sockaddr *src, __u8 prefixlen_s, 
					struct sockaddr *dst, __u8 prefixlen_d,
					__u8 ipsec_proto, __u32 spi, struct ipsec_sa **sa);

/* The struct sa_index is a glue between SA and Policy */
struct sa_index* sa_index_kmalloc(void);
int sa_index_init(struct sa_index *sa);
void sa_index_kfree(struct sa_index *sa);
int sa_index_compare(struct sa_index *sa_idx1, struct sa_index *sa_idx2);
int sa_index_copy(struct sa_index *dst, struct sa_index *src);
void ipsec_sa_mod_timer(struct ipsec_sa *sa);

extern int sadb_init(void);
extern int sadb_cleanup(void);

#endif /* __KERNEL__ */
#endif /* __SADB_H */

