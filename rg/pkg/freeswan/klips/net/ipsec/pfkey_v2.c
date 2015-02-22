/*
 * @(#) RFC2367 PF_KEYv2 Key management API domain socket I/F
 * Copyright (C) 1999, 2000, 2001  Richard Guy Briggs.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

/*
 *		Template from /usr/src/linux-2.0.36/net/unix/af_unix.c.
 *		Hints from /usr/src/linux-2.0.36/net/ipv4/udp.c.
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/version.h>
#include <linux/config.h>
#include <linux/kernel.h>

#include "ipsec_param.h"

#include <linux/major.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/socket.h>
#include <linux/un.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h> /* struct socket */
#include <linux/in.h>
#include <linux/fs.h>
#ifdef MALLOC_SLAB
# include <linux/slab.h> /* kmalloc() */
#else /* MALLOC_SLAB */
# include <linux/malloc.h> /* kmalloc() */
#endif /* MALLOC_SLAB */
#include <asm/segment.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <net/sock.h> /* struct sock */
/* #include <net/tcp.h> */
#include <net/af_unix.h>
#ifdef CONFIG_PROC_FS
# include <linux/proc_fs.h>
#endif /* CONFIG_PROC_FS */

#include <linux/types.h>
 
#include <freeswan.h>
#ifdef NET_21
# include <asm/uaccess.h>
# include <linux/in6.h>
#endif /* NET_21 */

#include "radij.h"
#include "ipsec_encap.h"
#include "ipsec_sa.h"
#include "ipsec_netlink.h"

#include <pfkeyv2.h>
#include <pfkey.h>

#include "ipsec_proto.h"

#include "ipsec_log.h"

#ifdef CONFIG_IPSEC_DEBUG
int debug_pfkey = 0;
extern int sysctl_ipsec_debug_verbose;
#endif /* CONFIG_IPSEC_DEBUG */

#define SENDERR(_x) do { error = -(_x); goto errlab; } while (0)

#ifndef SOCKOPS_WRAPPED
#define SOCKOPS_WRAPPED(name) name
#endif /* SOCKOPS_WRAPPED */

struct proto_ops SOCKOPS_WRAPPED(pfkey_ops);
#ifdef NET_26
static rwlock_t pfkey_sock_lock = RW_LOCK_UNLOCKED;
HLIST_HEAD(pfkey_sock_list);
static DECLARE_WAIT_QUEUE_HEAD(pfkey_sock_wait);
static atomic_t pfkey_sock_users = ATOMIC_INIT(0);
#else
struct sock *pfkey_sock_list = NULL;
#endif
struct supported_list *pfkey_supported_list[SADB_SATYPE_MAX+1];

struct socket_list *pfkey_open_sockets = NULL;
struct socket_list *pfkey_registered_sockets[SADB_SATYPE_MAX+1];

int pfkey_msg_interp(struct sock *, struct sadb_msg *, struct sadb_msg **);

#ifdef NET_26
static void pfkey_sock_list_grab(void)
{
	write_lock_bh(&pfkey_sock_lock);

	if (atomic_read(&pfkey_sock_users)) {
		DECLARE_WAITQUEUE(wait, current);

		add_wait_queue_exclusive(&pfkey_sock_wait, &wait);
		for(;;) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			if (atomic_read(&pfkey_sock_users) == 0)
				break;
			write_unlock_bh(&pfkey_sock_lock);
			schedule();
			write_lock_bh(&pfkey_sock_lock);
		}

		__set_current_state(TASK_RUNNING);
		remove_wait_queue(&pfkey_sock_wait, &wait);
	}
}

static __inline__ void pfkey_sock_list_ungrab(void)
{
	write_unlock_bh(&pfkey_sock_lock);
	wake_up(&pfkey_sock_wait);
}

static __inline__ void pfkey_lock_sock_list(void)
{
	/* read_lock() synchronizes us to pfkey_table_grab */

	read_lock(&pfkey_sock_lock);
	atomic_inc(&pfkey_sock_users);
	read_unlock(&pfkey_sock_lock);
}

static __inline__ void pfkey_unlock_sock_list(void)
{
	if (atomic_dec_and_test(&pfkey_sock_users))
		wake_up(&pfkey_sock_wait);
}
#endif

int
pfkey_list_remove_socket(struct socket *socketp, struct socket_list **sockets)
{
	struct socket_list *socket_listp,*prev;

	if(!socketp) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_remove_socket: "
			    "NULL socketp handed in, failed.\n");
		return -EINVAL;
	}

	if(!sockets) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_remove_socket: "
			    "NULL sockets list handed in, failed.\n");
		return -EINVAL;
	}

	socket_listp = *sockets;
	prev = NULL;
	
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_list_remove_socket: "
		    "removing sock=%p\n",
		    socketp);
	
	while(socket_listp != NULL) {
		if(socket_listp->socketp == socketp) {
			if(prev != NULL) {
				prev->next = socket_listp->next;
			} else {
				*sockets = socket_listp->next;
			}
			
			kfree((void*)socket_listp);
			
			break;
		}
		prev = socket_listp;
		socket_listp = socket_listp->next;
	}

	return 0;
}

int
pfkey_list_insert_socket(struct socket *socketp, struct socket_list **sockets)
{
	struct socket_list *socket_listp;

	if(!socketp) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_insert_socket: "
			    "NULL socketp handed in, failed.\n");
		return -EINVAL;
	}

	if(!sockets) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_insert_socket: "
			    "NULL sockets list handed in, failed.\n");
		return -EINVAL;
	}

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_list_insert_socket: "
		    "socketp=%p\n",socketp);
	
	if((socket_listp = (struct socket_list *)kmalloc(sizeof(struct socket_list), GFP_KERNEL)) == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_insert_socket: "
			    "memory allocation error.\n");
		return -ENOMEM;
	}
	
	socket_listp->socketp = socketp;
	socket_listp->next = *sockets;
	*sockets = socket_listp;

	return 0;
}
  
int
pfkey_list_remove_supported(struct supported *supported, struct supported_list **supported_list)
{
	struct supported_list *supported_listp = *supported_list, *prev = NULL;
	
	if(!supported) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_remove_supported: "
			    "NULL supported handed in, failed.\n");
		return -EINVAL;
	}

	if(!supported_list) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_remove_supported: "
			    "NULL supported_list handed in, failed.\n");
		return -EINVAL;
	}

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_list_remove_supported: "
		    "removing supported=%p\n",
		    supported);
	
	while(supported_listp != NULL) {
		if(supported_listp->supportedp == supported) {
			if(prev != NULL) {
				prev->next = supported_listp->next;
			} else {
				*supported_list = supported_listp->next;
			}
			
			kfree((void*)supported_listp);
			
			break;
		}
		prev = supported_listp;
		supported_listp = supported_listp->next;
	}

	return 0;
}

int
pfkey_list_insert_supported(struct supported *supported, struct supported_list **supported_list)
{
	struct supported_list *supported_listp;

	if(!supported) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_insert_supported: "
			    "NULL supported handed in, failed.\n");
		return -EINVAL;
	}

	if(!supported_list) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_insert_supported: "
			    "NULL supported_list handed in, failed.\n");
		return -EINVAL;
	}

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_list_insert_supported: "
		    "incoming, supported=%p, supported_list=%p\n",
		    supported,
		    supported_list);
	
	supported_listp = (struct supported_list *)kmalloc(sizeof(struct supported_list), GFP_KERNEL);
	if(supported_listp == NULL)	{
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_list_insert_supported: "
			    "memory allocation error.\n");
		return -ENOMEM;
	}
	
	supported_listp->supportedp = supported;
	supported_listp->next = *supported_list;
	*supported_list = supported_listp;
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_list_insert_supported: "
		    "outgoing, supported=%p, supported_list=%p\n",
		    supported,
		    supported_list);

	return 0;
}
  
#ifndef NET_21
DEBUG_NO_STATIC void
pfkey_state_change(struct sock *sk)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_state_change: .\n");
	if(!sk->dead) {
		wake_up_interruptible(sk->sleep);
	}
}
#endif /* !NET_21 */

#ifndef NET_21
DEBUG_NO_STATIC void
pfkey_data_ready(struct sock *sk, int len)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_data_ready: "
		    "sk=%p len=%d\n",
		    sk,
		    len);
	if(!sk->dead) {
		wake_up_interruptible(sk->sleep);
		sock_wake_async(sk->socket, 1);
	}
}

DEBUG_NO_STATIC void
pfkey_write_space(struct sock *sk)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_write_space: .\n");
	if(!sk->dead) {
		wake_up_interruptible(sk->sleep);
		sock_wake_async(sk->socket, 2);
	}
}
#endif /* !NET_21 */

#ifdef NET_26
DEBUG_NO_STATIC void
pfkey_insert_socket(struct sock *sk)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_insert_socket: "
		    "sk=0p%p\n",
		    sk);
	pfkey_sock_list_grab();
	sk_add_node(sk, &pfkey_sock_list);
	pfkey_sock_list_ungrab();
}

DEBUG_NO_STATIC void
pfkey_remove_socket(struct sock *sk)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_remove_socket: 0p%p\n", sk);
	pfkey_sock_list_grab();
	sk_del_node_init(sk);
	pfkey_sock_list_ungrab();
	return;
}
#else
DEBUG_NO_STATIC void
pfkey_insert_socket(struct sock *sk)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_insert_socket: "
		    "sk=%p\n",
		    sk);
	cli();
	sk->next=pfkey_sock_list;
	pfkey_sock_list=sk;
	sti();
}

DEBUG_NO_STATIC void
pfkey_remove_socket(struct sock *sk)
{
	struct sock **s;
	
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_remove_socket: .\n");
	cli();
	s=&pfkey_sock_list;

	while(*s!=NULL) {
		if(*s==sk) {
			*s=sk->next;
			sk->next=NULL;
			sti();
			KLIPS_PRINT(debug_pfkey,
				    "klips_debug:pfkey_remove_socket: "
				    "succeeded.\n");
			return;
		}
		s=&((*s)->next);
	}
	sti();
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_remove_socket: "
		    "not found.\n");
	return;
}
#endif

DEBUG_NO_STATIC void
pfkey_destroy_socket(struct sock *sk)
{
	struct sk_buff *skb;

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_destroy_socket: .\n");
	pfkey_remove_socket(sk);
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_destroy_socket: "
		    "pfkey_remove_socket called.\n");
	
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_destroy_socket: "
		    "sk(%p)->(&%p)receive_queue.{next=%p,prev=%p}.\n",
		    sk,
		    &(sk->sk_receive_queue),
		    sk->sk_receive_queue.next,
		    sk->sk_receive_queue.prev);
	while(sk && ((skb=skb_dequeue(&(sk->sk_receive_queue)))!=NULL)) {
#ifdef NET_21
#ifdef CONFIG_IPSEC_DEBUG
		if((debug_pfkey && sysctl_ipsec_debug_verbose) &&
		    ipsec_rate_limit()) {
			KLIPS_PRINT(debug_pfkey,
				    "klips_debug:pfkey_destroy_socket: "
				    "skb=%p dequeued.\n", skb);
			printk(KERN_INFO "klips_debug:pfkey_destroy_socket: "
			       "pfkey_skb contents:");
			printk(" next:%p", skb->next);
			printk(" prev:%p", skb->prev);
#ifdef HAVE_SKB_LIST
                        printk(" list:0p%p", skb->list);
#endif
			printk(" sk:%p", skb->sk);
#ifdef HAVE_TSTAMP
                        printk(" stamp:%d.%d", skb->tstamp.off_sec, skb->tstamp.off_usec);
#else
                        printk(" stamp:%ld.%ld", skb->stamp.tv_sec, skb->stamp.tv_usec);
#endif
			printk(" dev:%p", skb->dev);
			if(skb->dev) {
				if(skb->dev->name) {
					printk(" dev->name:%s", skb->dev->name);
				} else {
					printk(" dev->name:NULL?");
				}
			} else {
				printk(" dev:NULL");
			}
			printk(" h:%p", skb->h.raw);
			printk(" nh:%p", skb->nh.raw);
			printk(" mac:%p", skb->mac.raw);
			printk(" dst:%p", skb->dst);
			if(sysctl_ipsec_debug_verbose) {
				int i;
				
				printk(" cb");
				for(i=0; i<48; i++) {
					printk(":%2x", skb->cb[i]);
				}
			}
			printk(" len:%d", skb->len);
			printk(" csum:%d", skb->csum);
#ifndef NETDEV_23
			printk(" used:%d", skb->used);
			printk(" is_clone:%d", skb->is_clone);
#endif
			printk(" cloned:%d", skb->cloned);
			printk(" pkt_type:%d", skb->pkt_type);
			printk(" ip_summed:%d", skb->ip_summed);
			printk(" priority:%d", skb->priority);
			printk(" protocol:%d", skb->protocol);
#ifdef HAVE_SOCK_SECURITY
			printk(" security:%d", skb->security);
#endif
			printk(" truesize:%d", skb->truesize);
			printk(" head:%p", skb->head);
			printk(" data:%p", skb->data);
			printk(" tail:%p", skb->tail);
			printk(" end:%p", skb->end);
			if(sysctl_ipsec_debug_verbose) {
				unsigned char* i;
				printk(" data");
				for(i= skb->head; i< skb->end; i++) {
					printk(":%2x", (unsigned char)(*(i)));
				}
			}
			printk(" destructor:%p", skb->destructor);
			printk("\n");
		}
#endif /* CONFIG_IPSEC_DEBUG */
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_destroy_socket: "
			    "skb=%p freed.\n",
			    skb);
		kfree_skb(skb);

#else /* NET_21 */
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_destroy_socket: "
			    "skb=%p dequeued and freed.\n",
			    skb);
		kfree_skb(skb, FREE_WRITE);

#endif /* NET_21 */
	}

#ifdef NET_26
	sock_set_flag(sk, SOCK_DEAD);
#else
	sk->dead = 1;
#endif
	sk_free(sk);

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_destroy_socket: destroyed.\n");
}

int
pfkey_upmsg(struct socket *sock, struct sadb_msg *pfkey_msg)
{
	int error = 0;
	struct sk_buff * skb = NULL;
	struct sock *sk;

	if(sock == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_upmsg: "
			    "NULL socket passed in.\n");
		return -EINVAL;
	}

	if(pfkey_msg == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_upmsg: "
			    "NULL pfkey_msg passed in.\n");
		return -EINVAL;
	}

#ifdef NET_21
	sk = sock->sk;
#else /* NET_21 */
	sk = sock->data;
#endif /* NET_21 */

	if(sk == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_upmsg: "
			    "NULL sock passed in.\n");
		return -EINVAL;
	}

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_upmsg: "
		    "allocating %d bytes...\n",
		    pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN);
	if(!(skb = alloc_skb(pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN, GFP_ATOMIC) )) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_upmsg: "
			    "no buffers left to send up a message.\n");
		return -ENOBUFS;
	}
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_upmsg: "
		    "...allocated at %p.\n",
		    skb);
	
	skb->dev = NULL;
	
	if(skb_tailroom(skb) < pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) {
		ipsec_log(KERN_WARNING "klips_error:pfkey_upmsg: "
		       "tried to skb_put %ld, %d available.  This should never happen, please report.\n",
		       (unsigned long int)pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN,
		       skb_tailroom(skb));
		ipsec_kfree_skb(skb);
		return -ENOBUFS;
	}
	skb->h.raw = skb_put(skb, pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN);
	memcpy(skb->h.raw, pfkey_msg, pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN);

#ifndef NET_21
	skb->free = 1;
#endif /* !NET_21 */

	if((error = sock_queue_rcv_skb(sk, skb)) < 0) {
		skb->sk=NULL;
		ipsec_kfree_skb(skb);
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_upmsg: "
			    "error=%d calling sock_queue_rcv_skb with skb=%p.\n",
			    error,
			    skb);
		return error;
	}
	return error;
}

#ifdef NET_26_12_SKALLOC
static struct proto key_proto = {
	.name	  = "KEY",
	.owner	  = THIS_MODULE,
	.obj_size = sizeof(struct sock),
	
};
#endif

DEBUG_NO_STATIC int
pfkey_create(struct socket *sock, int protocol)
{
	struct sock *sk;

	if(sock == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_create: "
			    "socket NULL.\n");
		return -EINVAL;
	}

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_create: "
		    "sock=%p type:%d state:%d flags:%ld protocol:%d\n",
		    sock,
		    sock->type,
		    (unsigned int)(sock->state),
		    sock->flags, protocol);

	if(sock->type != SOCK_RAW) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_create: "
			    "only SOCK_RAW supported.\n");
		return -ESOCKTNOSUPPORT;
	}

	if(protocol != PF_KEY_V2) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_create: "
			    "protocol not PF_KEY_V2.\n");
		return -EPROTONOSUPPORT;
	}

	if((current->uid != 0)) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_create: "
			    "must be root to open pfkey sockets.\n");
		return -EACCES;
	}

#ifdef NET_21
	sock->state = SS_UNCONNECTED;
#endif /* NET_21 */
	KLIPS_INC_USE;
#ifdef NET_21
#ifdef NET_26
#ifdef NET_26_12_SKALLOC
	sk=(struct sock *)sk_alloc(PF_KEY, GFP_KERNEL, &key_proto, 1);
#else
	sk=(struct sock *)sk_alloc(PF_KEY, GFP_KERNEL, 1, NULL);
#endif
#else
	/* 2.4 interface */
	sk=(struct sock *)sk_alloc(PF_KEY, GFP_KERNEL, 1);
#endif
#else /* NET_21 */
	/* 2.2 interface */
	sk=(struct sock *)sk_alloc(GFP_KERNEL);
#endif /* NET_21 */

	if (sk == NULL)
	{
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_create: "
			    "Out of memory trying to allocate.\n");
		KLIPS_DEC_USE;
		return -ENOMEM;
	}

#ifndef NET_21
	memset(sk, 0, sizeof(*sk));
#endif /* !NET_21 */

#ifdef NET_21
	sock_init_data(sock, sk);

	sk->sk_destruct = NULL;
	sk->sk_reuse = 1;
	sock->ops = &SOCKOPS_WRAPPED(pfkey_ops);

	sk->sk_family = PF_KEY;
/*	sk->num = protocol; */
	sk->sk_protocol = protocol;
	key_pid(sk) = current->pid;
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_create: "
		    "sock->fasync_list=%p sk->sleep=%p.\n",
		    sock->fasync_list,
		    sk->sk_sleep);
#else /* NET_21 */
	sk->type=sock->type;
	init_timer(&sk->timer);
	skb_queue_head_init(&sk->write_queue);
	skb_queue_head_init(&sk->sk_receive_queue);
	skb_queue_head_init(&sk->back_log);
	sk->rcvbuf=SK_RMEM_MAX;
	sk->sndbuf=SK_WMEM_MAX;
	sk->allocation=GFP_KERNEL;
	sk->state=TCP_CLOSE;
	sk->priority=SOPRI_NORMAL;
	sk->sk_state_change=pfkey_state_change;
	sk->data_ready=pfkey_data_ready;
	sk->write_space=pfkey_write_space;
	sk->error_report=pfkey_state_change;
#ifndef NET_26
	sk->mtu=4096;
	sk->socket=sock;
#endif
	sock->data=(void *)sk;
	sk->sleep=sock->wait;
#endif /* NET_21 */

	pfkey_insert_socket(sk);
	pfkey_list_insert_socket(sock, &pfkey_open_sockets);

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_create: "
		    "Socket sock=%p sk=%p initialised.\n", sock, sk);
	return 0;
}

#ifndef NET_21
DEBUG_NO_STATIC int
pfkey_dup(struct socket *newsock, struct socket *oldsock)
{
	struct sock *sk;

	if(newsock==NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_dup: "
			    "No new socket attached.\n");
		return -EINVAL;
	}
		
	if(oldsock==NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_dup: "
			    "No old socket attached.\n");
		return -EINVAL;
	}
		
#ifdef NET_21
	sk=oldsock->sk;
#else /* NET_21 */
	sk=oldsock->data;
#endif /* NET_21 */
	
	/* May not have data attached */
	if(sk==NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_dup: "
			    "No sock attached to old socket.\n");
		return -EINVAL;
	}
		
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_dup: .\n");

	return pfkey_create(newsock, sk->protocol);
}
#endif /* !NET_21 */

DEBUG_NO_STATIC int
#ifdef NETDEV_23
pfkey_release(struct socket *sock)
#else /* NETDEV_23 */
pfkey_release(struct socket *sock, struct socket *peersock)
#endif /* NETDEV_23 */
{
	struct sock *sk;
	int i;

	if(sock==NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_release: "
			    "No socket attached.\n");
		return 0; /* -EINVAL; */
	}
		
#ifdef NET_21
	sk=sock->sk;
#else /* NET_21 */
	sk=sock->data;
#endif /* NET_21 */
	
	/* May not have data attached */
	if(sk==NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_release: "
			    "No sk attached to sock=%p.\n", sock);
		return 0; /* -EINVAL; */
	}
		
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_release: "
		    "sock=%p sk=%p\n", sock, sk);

#ifdef NET_21
	if(!sock_flag(sk, SOCK_DEAD))
#endif /* NET_21 */
		if(sk->sk_state_change) {
			sk->sk_state_change(sk);
		}

#ifdef NET_21
	sock->sk = NULL;
#else /* NET_21 */
	sock->data = NULL;
#endif /* NET_21 */

	/* Try to flush out this socket. Throw out buffers at least */
	pfkey_destroy_socket(sk);
	pfkey_list_remove_socket(sock, &pfkey_open_sockets);
	for(i = SADB_SATYPE_UNSPEC; i <= SADB_SATYPE_MAX; i++) {
		pfkey_list_remove_socket(sock, &(pfkey_registered_sockets[i]));
	}

	KLIPS_DEC_USE;
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_release: "
		    "succeeded.\n");

	return 0;
}

#ifndef NET_21
DEBUG_NO_STATIC int
pfkey_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_bind: "
		    "operation not supported.\n");
	return -EINVAL;
}

DEBUG_NO_STATIC int
pfkey_connect(struct socket *sock, struct sockaddr *uaddr, int addr_len, int flags)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_connect: "
		    "operation not supported.\n");
	return -EINVAL;
}

DEBUG_NO_STATIC int
pfkey_socketpair(struct socket *a, struct socket *b)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_socketpair: "
		    "operation not supported.\n");
	return -EINVAL;
}

DEBUG_NO_STATIC int
pfkey_accept(struct socket *sock, struct socket *newsock, int flags)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_aaccept: "
		    "operation not supported.\n");
	return -EINVAL;
}

DEBUG_NO_STATIC int
pfkey_getname(struct socket *sock, struct sockaddr *uaddr, int *uaddr_len,
		int peer)
{
	struct sockaddr *ska = (struct sockaddr*)uaddr;
	
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_getname: .\n");
	ska->sa_family = PF_KEY;
	*uaddr_len = sizeof(*ska);
	return 0;
}

DEBUG_NO_STATIC int
pfkey_select(struct socket *sock, int sel_type, select_table *wait)
{
	
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_select: "
		    ".sock=%p sk=%p sel_type=%d\n",
		    sock,
		    sock->data,
		    sel_type);
	if(sock == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_select: "
			    "Null socket passed in.\n");
		return -EINVAL;
	}
	return datagram_select(sock->data, sel_type, wait);
}

DEBUG_NO_STATIC int
pfkey_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_ioctl: "
		    "not supported.\n");
	return -EINVAL;
}

DEBUG_NO_STATIC int
pfkey_listen(struct socket *sock, int backlog)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_listen: "
		    "not supported.\n");
	return -EINVAL;
}
#endif /* !NET_21 */

DEBUG_NO_STATIC int
pfkey_shutdown(struct socket *sock, int mode)
{
	struct sock *sk;

	if(sock == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_shutdown: "
			    "NULL socket passed in.\n");
		return -EINVAL;
	}

#ifdef NET_21
	sk=sock->sk;
#else /* NET_21 */
	sk=sock->data;
#endif /* NET_21 */
	
	if(sk == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_shutdown: "
			    "No sock attached to socket.\n");
		return -EINVAL;
	}

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_shutdown: "
		    "mode=%x.\n", mode);
	mode++;
	
	if(mode&SEND_SHUTDOWN) {
		sk->sk_shutdown|=SEND_SHUTDOWN;
		sk->sk_state_change(sk);
	}

	if(mode&RCV_SHUTDOWN) {
		sk->sk_shutdown|=RCV_SHUTDOWN;
		sk->sk_state_change(sk);
	}
	return 0;
}

#ifndef NET_21
DEBUG_NO_STATIC int
pfkey_setsockopt(struct socket *sock, int level, int optname, char *optval, int optlen)
{
#ifndef NET_21
	struct sock *sk;

	if(sock == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_setsockopt: "
			    "Null socket passed in.\n");
		return -EINVAL;
	}
	
	sk=sock->data;
	
	if(sk == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_setsockopt: "
			    "Null sock passed in.\n");
		return -EINVAL;
	}
#endif /* !NET_21 */
	
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_setsockopt: .\n");
	if(level!=SOL_SOCKET) {
		return -EOPNOTSUPP;
	}
#ifdef NET_21
	return sock_setsockopt(sock, level, optname, optval, optlen);
#else /* NET_21 */
	return sock_setsockopt(sk, level, optname, optval, optlen);
#endif /* NET_21 */
}

DEBUG_NO_STATIC int
pfkey_getsockopt(struct socket *sock, int level, int optname, char *optval, int *optlen)
{
#ifndef NET_21
	struct sock *sk;

	if(sock == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_setsockopt: "
			    "Null socket passed in.\n");
		return -EINVAL;
	}
	
	sk=sock->data;
	
	if(sk == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_setsockopt: "
			    "Null sock passed in.\n");
		return -EINVAL;
	}
#endif /* !NET_21 */

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_getsockopt: .\n");
	if(level!=SOL_SOCKET) {
		return -EOPNOTSUPP;
	}
#ifdef NET_21
	return sock_getsockopt(sock, level, optname, optval, optlen);
#else /* NET_21 */
	return sock_getsockopt(sk, level, optname, optval, optlen);
#endif /* NET_21 */
}

DEBUG_NO_STATIC int
pfkey_fcntl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_fcntl: "
		    "not supported.\n");
	return -EINVAL;
}
#endif /* !NET_21 */

/*
 *	Send PF_KEY data down.
 */
		
DEBUG_NO_STATIC int
#ifdef NET_21
#ifdef NET_26
pfkey_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len)
#else
pfkey_sendmsg(struct socket *sock, struct msghdr *msg, int len, struct scm_cookie *scm)
#endif
#else /* NET_21 */
pfkey_sendmsg(struct socket *sock, struct msghdr *msg, int len, int nonblock, int flags)
#endif /* NET_21 */
{
	struct sock *sk;
	int error = 0;
	struct sadb_msg *pfkey_msg = NULL, *pfkey_reply = NULL;
	
	if(sock == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "Null socket passed in.\n");
		SENDERR(EINVAL);
	}
	
#ifdef NET_21
	sk = sock->sk;
#else /* NET_21 */
	sk = sock->data;
#endif /* NET_21 */

	if(sk == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "Null sock passed in.\n");
		SENDERR(EINVAL);
	}
	
	if(msg == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "Null msghdr passed in.\n");
		SENDERR(EINVAL);
	}

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_sendmsg: .\n");
	if(sk->sk_err) {
		error = sock_error(sk);
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "sk->err is non-zero, returns %d.\n",
			    error);
		SENDERR(-error);
	}

	if((current->uid != 0)) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "must be root to send messages to pfkey sockets.\n");
		SENDERR(EACCES);
	}

#ifdef NET_21
	if(msg->msg_control)
#else /* NET_21 */
	if(flags || msg->msg_control)
#endif /* NET_21 */
	{
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "can't set flags or set msg_control.\n");
		SENDERR(EINVAL);
	}
		
	if(sk->sk_shutdown & SEND_SHUTDOWN) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "shutdown.\n");
		send_sig(SIGPIPE, current, 0);
		SENDERR(EPIPE);
	}
	
	if(len < sizeof(struct sadb_msg)) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "bogus msg len of %d, too small.\n", len);
		SENDERR(EMSGSIZE);
	}

	if((pfkey_msg = (struct sadb_msg*)kmalloc(len, GFP_KERNEL)) == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "memory allocation error.\n");
		SENDERR(ENOBUFS);
	}

	memcpy_fromiovec((void *)pfkey_msg, msg->msg_iov, len);

	if(pfkey_msg->sadb_msg_version != PF_KEY_V2) {
		KLIPS_PRINT(1 || debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "not PF_KEY_V2 msg, found %d, should be %d.\n",
			    pfkey_msg->sadb_msg_version,
			    PF_KEY_V2);
		kfree((void*)pfkey_msg);
		return -EINVAL;
	}

	if(len != pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "bogus msg len of %d, not %d byte aligned.\n",
			    len, IPSEC_PFKEYv2_ALIGN);
		SENDERR(EMSGSIZE);
	}

#if 0
	/* This check is questionable, since a downward message could be
	   the result of an ACQUIRE either from kernel (PID==0) or
	   userspace (some other PID). */
	/* check PID */
	if(pfkey_msg->sadb_msg_pid != current->pid) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "pid (%d) does not equal sending process pid (%d).\n",
			    pfkey_msg->sadb_msg_pid, current->pid);
		SENDERR(EINVAL);
	}
#endif

	if(pfkey_msg->sadb_msg_reserved) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "reserved field must be zero, set to %d.\n",
			    pfkey_msg->sadb_msg_reserved);
		SENDERR(EINVAL);
	}
	
	if((pfkey_msg->sadb_msg_type > SADB_MAX) || (!pfkey_msg->sadb_msg_type)){
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "msg type too large or small:%d.\n",
			    pfkey_msg->sadb_msg_type);
		SENDERR(EINVAL);
	}
	
	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:pfkey_sendmsg: "
		    "msg sent for parsing.\n");
	
	if((error = pfkey_msg_interp(sk, pfkey_msg, &pfkey_reply))) {
		struct socket_list *pfkey_socketsp;

		KLIPS_PRINT(debug_pfkey, "klips_debug:pfkey_sendmsg: "
			    "pfkey_msg_parse returns %d.\n",
			    error);

		if((pfkey_reply = (struct sadb_msg*)kmalloc(sizeof(struct sadb_msg), GFP_KERNEL)) == NULL) {
			KLIPS_PRINT(debug_pfkey,
				    "klips_debug:pfkey_sendmsg: "
				    "memory allocation error.\n");
			SENDERR(ENOBUFS);
		}
		memcpy((void*)pfkey_reply, (void*)pfkey_msg, sizeof(struct sadb_msg));
		pfkey_reply->sadb_msg_errno = -error;
		pfkey_reply->sadb_msg_len = sizeof(struct sadb_msg) / IPSEC_PFKEYv2_ALIGN;

		for(pfkey_socketsp = pfkey_open_sockets;
		    pfkey_socketsp;
		    pfkey_socketsp = pfkey_socketsp->next) {
			int error_upmsg = 0;
			KLIPS_PRINT(debug_pfkey, "klips_debug:pfkey_sendmsg: "
				    "sending up error=%d message=%p to socket=%p.\n",
				    error,
				    pfkey_reply,
				    pfkey_socketsp->socketp);
			if((error_upmsg = pfkey_upmsg(pfkey_socketsp->socketp, pfkey_reply))) {
				KLIPS_PRINT(debug_pfkey, "klips_debug:pfkey_sendmsg: "
					    "sending up error message to socket=%p failed with error=%d.\n",
					    pfkey_socketsp->socketp,
					    error_upmsg);
				/* pfkey_msg_free(&pfkey_reply); */
				/* SENDERR(-error); */
			}
			KLIPS_PRINT(debug_pfkey, "klips_debug:pfkey_sendmsg: "
				    "sending up error message to socket=%p succeeded.\n",
				    pfkey_socketsp->socketp);
		}
		
		pfkey_msg_free(&pfkey_reply);
		
		SENDERR(-error);
	}

 errlab:
	if (pfkey_msg) {
		kfree((void*)pfkey_msg);
	}
	
	if(error) {
		return error;
	} else {
		return len;
	}
}

/*
 *	Receive PF_KEY data up.
 */
		
DEBUG_NO_STATIC int
#ifdef NET_21
#ifdef NET_26
pfkey_recvmsg(struct kiocb *kiocb, struct socket *sock, struct msghdr *msg, size_t size, int flags)
#else
pfkey_recvmsg(struct socket *sock, struct msghdr *msg, int size, int flags, struct scm_cookie *scm)
#endif
#else /* NET_21 */
pfkey_recvmsg(struct socket *sock, struct msghdr *msg, int size, int noblock, int flags, int *addr_len)
#endif /* NET_21 */
{
	struct sock *sk;
#ifdef NET_21
	int noblock = flags & MSG_DONTWAIT;
#endif /* NET_21 */
	struct sk_buff *skb;
	int error;

	if(sock == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_recvmsg: "
			    "Null socket passed in.\n");
		return -EINVAL;
	}

#ifdef NET_21
	sk = sock->sk;
#else /* NET_21 */
	sk = sock->data;
#endif /* NET_21 */

	if(sk == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_recvmsg: "
			    "Null sock passed in for sock=%p.\n", sock);
		return -EINVAL;
	}

	if(msg == NULL) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_recvmsg: "
			    "Null msghdr passed in for sock=%p, sk=%p.\n",
			    sock, sk);
		return -EINVAL;
	}

	KLIPS_PRINT(debug_pfkey && sysctl_ipsec_debug_verbose,
		    "klips_debug:pfkey_recvmsg: sock=%p sk=%p msg=%p size=%d.\n",
		    sock, sk, msg, size);
	if(flags & ~MSG_PEEK) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "flags (%d) other than MSG_PEEK not supported.\n",
			    flags);
		return -EOPNOTSUPP;
	}
		
#ifdef NET_21
	msg->msg_namelen = 0; /* sizeof(*ska); */
#else /* NET_21 */
	if(addr_len) {
		*addr_len = 0; /* sizeof(*ska); */
	}
#endif /* NET_21 */
		
	if(sk->sk_err) {
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:pfkey_sendmsg: "
			    "sk->err=%d.\n", sk->sk_err);
		return sock_error(sk);
	}

	if((skb = skb_recv_datagram(sk, flags, noblock, &error) ) == NULL) {
                return error;
	}

	if(size > skb->len) {
		size = skb->len;
	}
#ifdef NET_21
	else if(size <skb->len) {
		msg->msg_flags |= MSG_TRUNC;
	}
#endif /* NET_21 */

	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, size);
#ifdef HAVE_TSTAMP
	sk->sk_stamp.tv_sec = skb->tstamp.off_sec;
	sk->sk_stamp.tv_usec = skb->tstamp.off_usec;
#else
	sk->sk_stamp=skb->stamp;
#endif

	skb_free_datagram(sk, skb);
	return size;
}

#ifdef NET_21
struct net_proto_family pfkey_family_ops = {
	PF_KEY,
	pfkey_create
};

struct proto_ops SOCKOPS_WRAPPED(pfkey_ops) = {
#ifdef NETDEV_23
	family:		PF_KEY,
	release:	pfkey_release,
	bind:		sock_no_bind,
	connect:	sock_no_connect,
	socketpair:	sock_no_socketpair,
	accept:		sock_no_accept,
	getname:	sock_no_getname,
	poll:		datagram_poll,
	ioctl:		sock_no_ioctl,
	listen:		sock_no_listen,
	shutdown:	pfkey_shutdown,
	setsockopt:	sock_no_setsockopt,
	getsockopt:	sock_no_getsockopt,
	sendmsg:	pfkey_sendmsg,
	recvmsg:	pfkey_recvmsg,
	mmap:		sock_no_mmap,
#else /* NETDEV_23 */
	PF_KEY,
	sock_no_dup,
	pfkey_release,
	sock_no_bind,
	sock_no_connect,
	sock_no_socketpair,
	sock_no_accept,
	sock_no_getname,
	datagram_poll,
	sock_no_ioctl,
	sock_no_listen,
	pfkey_shutdown,
	sock_no_setsockopt,
	sock_no_getsockopt,
	sock_no_fcntl,
	pfkey_sendmsg,
	pfkey_recvmsg
#endif /* NETDEV_23 */
};

#ifdef NETDEV_23
#include <linux/smp_lock.h>
SOCKOPS_WRAP(pfkey, PF_KEY);
#endif /* NETDEV_23 */

#else /* NET_21 */
struct proto_ops pfkey_proto_ops = {
	PF_KEY,
	pfkey_create,
	pfkey_dup,
	pfkey_release,
	pfkey_bind,
	pfkey_connect,
	pfkey_socketpair,
	pfkey_accept,
	pfkey_getname,
	pfkey_select,
	pfkey_ioctl,
	pfkey_listen,
	pfkey_shutdown,
	pfkey_setsockopt,
	pfkey_getsockopt,
	pfkey_fcntl,
	pfkey_sendmsg,
	pfkey_recvmsg
};
#endif /* NET_21 */
   
#ifdef CONFIG_PROC_FS
#ifndef PROC_FS_2325
DEBUG_NO_STATIC
#endif /* PROC_FS_2325 */
int
pfkey_get_info(char *buffer, char **start, off_t offset, int length
#ifndef  PROC_NO_DUMMY
, int dummy
#endif /* !PROC_NO_DUMMY */
)
{
	const int max_content = length > 0? length-1 : 0;	/* limit of useful snprintf output */
#ifdef NET_26
	struct hlist_node *node;
#endif
	off_t begin=0;
	int len=0;
	struct sock *sk;
	
#ifdef CONFIG_IPSEC_DEBUG
	if(!sysctl_ipsec_debug_verbose) {
#endif /* CONFIG_IPSEC_DEBUG */
	len+= sprintf(buffer,
		      "    sock   pid   socket     next     prev e n p sndbf    Flags     Type St\n");
#ifdef CONFIG_IPSEC_DEBUG
	} else {
	len+= sprintf(buffer,
		      "    sock   pid d    sleep   socket     next     prev e r z n p sndbf    stamp    Flags     Type St\n");
	}
#endif /* CONFIG_IPSEC_DEBUG */
	
	sk_for_each(sk, node, &pfkey_sock_list) {
#ifdef CONFIG_IPSEC_DEBUG
		if(!sysctl_ipsec_debug_verbose) {
#endif /* CONFIG_IPSEC_DEBUG */
		len+=sprintf(buffer+len,
			     "%8p %5d %8p %d %d %5d %08lX %8X %2X\n",
			     sk,
			     key_pid(sk),
			     sk->sk_socket,
			     sk->sk_err,
			     sk->sk_protocol,
			     sk->sk_sndbuf,
			     sk->sk_socket->flags,
			     sk->sk_socket->type,
			     sk->sk_socket->state);
#ifdef CONFIG_IPSEC_DEBUG
		} else {
		len+=sprintf(buffer+len,
			     "%8p %5d %d %8p %8p %d %d %d %d %5d %d.%06d %08lX %8X %2X\n",
			     sk,
			     key_pid(sk),
			     sock_flag(sk, SOCK_DEAD),
			     sk->sk_sleep,
			     sk->sk_socket,
			     sk->sk_err,
			     sk->sk_reuse,
#ifdef HAVE_SOCK_ZAPPED
			     sock_flag(sk, SOCK_ZAPPED),
#else
			     sk->sk_zapped,
#endif
			     sk->sk_protocol,
			     sk->sk_sndbuf,
			     (unsigned int)sk->sk_stamp.tv_sec,
			     (unsigned int)sk->sk_stamp.tv_usec,
			     sk->sk_socket->flags,
			     sk->sk_socket->type,
			     sk->sk_socket->state);
		}
#endif /* CONFIG_IPSEC_DEBUG */
		
		if (len >= max_content) {
			/* we've done all that can fit -- stop loop */
			len = max_content;	/* truncate crap */
			break;
		} else {
			const off_t pos = begin + len;	/* file position of end of what we've generated */

			if (pos <= offset) {
				/* all is before first interesting character:
				 * discard, but note where we are.
				 */
				len = 0;
				begin = pos;
			}
		}
	}

	*start=buffer+(offset-begin);
	return len - (offset - begin);
}

#ifndef PROC_FS_2325
DEBUG_NO_STATIC
#endif /* PROC_FS_2325 */
int
pfkey_supported_get_info(char *buffer, char **start, off_t offset, int length
#ifndef  PROC_NO_DUMMY
, int dummy
#endif /* !PROC_NO_DUMMY */
)
{
	off_t pos=0;
	off_t begin=0;
	int len=0;
	int satype;
	struct supported_list *pfkey_supported_p;
	
	len+= sprintf(buffer,
		      "satype exttype alg_id ivlen minbits maxbits\n");
	
	for(satype = SADB_SATYPE_UNSPEC; satype <= SADB_SATYPE_MAX; satype++) {
		pfkey_supported_p = pfkey_supported_list[satype];
		while(pfkey_supported_p) {
			len+=sprintf(buffer+len,
				     "    %2d      %2d     %2d   %3d     %3d     %3d\n",
				     satype,
				     pfkey_supported_p->supportedp->supported_alg_exttype,
				     pfkey_supported_p->supportedp->supported_alg_id,
				     pfkey_supported_p->supportedp->supported_alg_ivlen,
				     pfkey_supported_p->supportedp->supported_alg_minbits,
				     pfkey_supported_p->supportedp->supported_alg_maxbits);
			
			pos=begin+len;
			if(pos<offset) {
				len=0;
				begin=pos;
			}
			if(pos>offset+length)
				break;
			pfkey_supported_p = pfkey_supported_p->next;
		}
	}
	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if(len>length)
		len=length;
	return len;
}

#ifndef PROC_FS_2325
DEBUG_NO_STATIC
#endif /* PROC_FS_2325 */
int
pfkey_registered_get_info(char *buffer, char **start, off_t offset, int length
#ifndef  PROC_NO_DUMMY
, int dummy
#endif /* !PROC_NO_DUMMY */
)
{
	off_t pos=0;
	off_t begin=0;
	int len=0;
	int satype;
	struct socket_list *pfkey_sockets;
	
	len+= sprintf(buffer,
		      "satype   socket   pid       sk\n");
	
	for(satype = SADB_SATYPE_UNSPEC; satype <= SADB_SATYPE_MAX; satype++) {
		pfkey_sockets = pfkey_registered_sockets[satype];
		while(pfkey_sockets) {
#ifdef NET_21
			len+=sprintf(buffer+len,
				     "    %2d %8p %5d %8p\n",
				     satype,
				     pfkey_sockets->socketp,
				     key_pid(pfkey_sockets->socketp->sk),
				     pfkey_sockets->socketp->sk);
#else /* NET_21 */
			len+=sprintf(buffer+len,
				     "    %2d %8p   N/A %8p\n",
				     satype,
				     pfkey_sockets->socketp,
#if 0
				     key_pid((pfkey_sockets->socketp)->data),
#endif
				     (pfkey_sockets->socketp)->data);
#endif /* NET_21 */
			
			pos=begin+len;
			if(pos<offset) {
				len=0;
				begin=pos;
			}
			if(pos>offset+length)
				break;
			pfkey_sockets = pfkey_sockets->next;
		}
	}
	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if(len>length)
		len=length;
	return len;
}

#ifndef PROC_FS_2325
struct proc_dir_entry proc_net_pfkey =
{
	0,
	6, "pf_key",
	S_IFREG | S_IRUGO, 1, 0, 0,
	0, &proc_net_inode_operations,
	pfkey_get_info
};
struct proc_dir_entry proc_net_pfkey_supported =
{
	0,
	16, "pf_key_supported",
	S_IFREG | S_IRUGO, 1, 0, 0,
	0, &proc_net_inode_operations,
	pfkey_supported_get_info
};
struct proc_dir_entry proc_net_pfkey_registered =
{
	0,
	17, "pf_key_registered",
	S_IFREG | S_IRUGO, 1, 0, 0,
	0, &proc_net_inode_operations,
	pfkey_registered_get_info
};
#endif /* !PROC_FS_2325 */
#endif /* CONFIG_PROC_FS */

DEBUG_NO_STATIC int
supported_add_all(int satype, struct supported supported[], int size)
{
	int i;
	int error = 0;

	KLIPS_PRINT(debug_pfkey,
		    "klips_debug:init_pfkey: "
		    "sizeof(supported_init_<satype=%d>)[%d]/sizeof(struct supported)[%d]=%d.\n",
		    satype,
		    size,
		    sizeof(struct supported),
		    size/sizeof(struct supported));

	for(i = 0; i < size / sizeof(struct supported); i++) {
		
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:init_pfkey: "
			    "i=%d inserting satype=%d exttype=%d id=%d ivlen=%d minbits=%d maxbits=%d.\n",
			    i,
			    satype,
			    supported[i].supported_alg_exttype,
			    supported[i].supported_alg_id,
			    supported[i].supported_alg_ivlen,
			    supported[i].supported_alg_minbits,
			    supported[i].supported_alg_maxbits);
			    
		error |= pfkey_list_insert_supported(&(supported[i]),
					    &(pfkey_supported_list[satype]));
	}
	return error;
}

DEBUG_NO_STATIC int
supported_remove_all(int satype)
{
	int error = 0;
	struct supported*supportedp;

	while(pfkey_supported_list[satype]) {
		supportedp = pfkey_supported_list[satype]->supportedp;
		KLIPS_PRINT(debug_pfkey,
			    "klips_debug:init_pfkey: "
			    "removing satype=%d exttype=%d id=%d ivlen=%d minbits=%d maxbits=%d.\n",
			    satype,
			    supportedp->supported_alg_exttype,
			    supportedp->supported_alg_id,
			    supportedp->supported_alg_ivlen,
			    supportedp->supported_alg_minbits,
			    supportedp->supported_alg_maxbits);
			    
		error |= pfkey_list_remove_supported(supportedp,
					    &(pfkey_supported_list[satype]));
	}
	return error;
}

int
pfkey_init(void)
{
	int error = 0;
	int i;
	
	static struct supported supported_init_ah[] = {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
		{SADB_EXT_SUPPORTED_AUTH, SADB_AALG_MD5HMAC, 0, 128, 128},
#endif /* CONFIG_IPSEC_AUTH_HMAC_MD5 */
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
		{SADB_EXT_SUPPORTED_AUTH, SADB_AALG_SHA1HMAC, 0, 160, 160}
#endif /* CONFIG_IPSEC_AUTH_HMAC_SHA1 */
	};
	static struct supported supported_init_esp[] = {
#ifdef CONFIG_IPSEC_AUTH_HMAC_MD5
		{SADB_EXT_SUPPORTED_AUTH, SADB_AALG_MD5HMAC, 0, 128, 128},
#endif /* CONFIG_IPSEC_AUTH_HMAC_MD5 */
#ifdef CONFIG_IPSEC_AUTH_HMAC_SHA1
		{SADB_EXT_SUPPORTED_AUTH, SADB_AALG_SHA1HMAC, 0, 160, 160},
#endif /* CONFIG_IPSEC_AUTH_HMAC_SHA1 */
#ifdef CONFIG_IPSEC_ENC_3DES
		{SADB_EXT_SUPPORTED_ENCRYPT, SADB_EALG_3DESCBC, 64, 168, 168},
#endif /* CONFIG_IPSEC_ENC_3DES */
	};
	static struct supported supported_init_ipip[] = {
		{SADB_EXT_SUPPORTED_ENCRYPT, SADB_X_TALG_IPv4_in_IPv4, 0, 32, 32}
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
		, {SADB_EXT_SUPPORTED_ENCRYPT, SADB_X_TALG_IPv6_in_IPv4, 0, 128, 32}
		, {SADB_EXT_SUPPORTED_ENCRYPT, SADB_X_TALG_IPv4_in_IPv6, 0, 32, 128}
		, {SADB_EXT_SUPPORTED_ENCRYPT, SADB_X_TALG_IPv6_in_IPv6, 0, 128, 128}
#endif /* defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE) */
	};
#ifdef CONFIG_IPSEC_IPCOMP
	static struct supported supported_init_ipcomp[] = {
		{SADB_EXT_SUPPORTED_ENCRYPT, SADB_X_CALG_DEFLATE, 0, 1, 1}
	};
#endif /* CONFIG_IPSEC_IPCOMP */

	for(i = SADB_SATYPE_UNSPEC; i <= SADB_SATYPE_MAX; i++) {
		pfkey_registered_sockets[i] = NULL;
		pfkey_supported_list[i] = NULL;
	}

	error |= supported_add_all(SADB_SATYPE_AH, supported_init_ah, sizeof(supported_init_ah));
	error |= supported_add_all(SADB_SATYPE_ESP, supported_init_esp, sizeof(supported_init_esp));
#ifdef CONFIG_IPSEC_IPCOMP
	error |= supported_add_all(SADB_X_SATYPE_COMP, supported_init_ipcomp, sizeof(supported_init_ipcomp));
#endif /* CONFIG_IPSEC_IPCOMP */
	error |= supported_add_all(SADB_X_SATYPE_IPIP, supported_init_ipip, sizeof(supported_init_ipip));

#ifdef NET_21
        error |= sock_register(&pfkey_family_ops);
#else /* NET_21 */
        error |= sock_register(pfkey_proto_ops.family, &pfkey_proto_ops);
#endif /* NET_21 */

#ifdef CONFIG_PROC_FS
#  ifndef PROC_FS_2325
#    ifdef PROC_FS_21
	error |= proc_register(proc_net, &proc_net_pfkey);
	error |= proc_register(proc_net, &proc_net_pfkey_supported);
	error |= proc_register(proc_net, &proc_net_pfkey_registered);
#    else /* PROC_FS_21 */
	error |= proc_register_dynamic(&proc_net, &proc_net_pfkey);
	error |= proc_register_dynamic(&proc_net, &proc_net_pfkey_supported);
	error |= proc_register_dynamic(&proc_net, &proc_net_pfkey_registered);
#    endif /* PROC_FS_21 */
#  else /* !PROC_FS_2325 */
	proc_net_create ("pf_key", 0, pfkey_get_info);
	proc_net_create ("pf_key_supported", 0, pfkey_supported_get_info);
	proc_net_create ("pf_key_registered", 0, pfkey_registered_get_info);
#  endif /* !PROC_FS_2325 */
#endif /* CONFIG_PROC_FS */

	return error;
}

int
pfkey_cleanup(void)
{
	int error = 0;
	
        ipsec_log(KERN_INFO "klips_info:pfkey_cleanup: "
	       "shutting down PF_KEY domain sockets.\n");
#ifdef NET_21
        error |= sock_unregister(PF_KEY);
#else /* NET_21 */
        error |= sock_unregister(pfkey_proto_ops.family);
#endif /* NET_21 */

	error |= supported_remove_all(SADB_SATYPE_AH);
	error |= supported_remove_all(SADB_SATYPE_ESP);
#ifdef CONFIG_IPSEC_IPCOMP
	error |= supported_remove_all(SADB_X_SATYPE_COMP);
#endif /* CONFIG_IPSEC_IPCOMP */
	error |= supported_remove_all(SADB_X_SATYPE_IPIP);

#ifdef CONFIG_PROC_FS
#  ifndef PROC_FS_2325
	if (proc_net_unregister(proc_net_pfkey.low_ino) != 0)
		ipsec_log("klips_debug:pfkey_cleanup: "
		       "cannot unregister /proc/net/pf_key\n");
	if (proc_net_unregister(proc_net_pfkey_supported.low_ino) != 0)
		ipsec_log("klips_debug:pfkey_cleanup: "
		       "cannot unregister /proc/net/pf_key_supported\n");
	if (proc_net_unregister(proc_net_pfkey_registered.low_ino) != 0)
		ipsec_log("klips_debug:pfkey_cleanup: "
		       "cannot unregister /proc/net/pf_key_registered\n");
#  else /* !PROC_FS_2325 */
	proc_net_remove ("pf_key");
	proc_net_remove ("pf_key_supported");
	proc_net_remove ("pf_key_registered");
#  endif /* !PROC_FS_2325 */
#endif /* CONFIG_PROC_FS */

	/* other module unloading cleanup happens here */
	return error;
}

#ifdef MODULE
#if 0
int
init_module(void)
{
	pfkey_init();
	return 0;
}

void
cleanup_module(void)
{
	pfkey_cleanup();
}
#endif /* 0 */
#else /* MODULE */
void
pfkey_proto_init(void *dummy /* struct net_proto *pro */)
{
	pfkey_init();
}
#endif /* MODULE */

