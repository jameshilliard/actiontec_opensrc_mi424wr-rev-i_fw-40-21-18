/* $USAGI: af_key.c,v 1.7 2002/05/16 16:37:03 miyazawa Exp $ */

/* this file derived from FreeS/WAN-1.9. (mk@linux-ipv6.org) */
/*
 * RFC2367 PF_KEYv2 Key management API domain socket I/F
 * Copyright (C) 1999, 2000  Richard Guy Briggs.
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


#ifdef MODULE
#include <linux/module.h>
#ifdef MODVERSIONS
# include <linux/modversions.h>
#endif /* MODVERSIONS */
#endif /* MODULE */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>

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
#include <linux/slab.h>
#include <asm/segment.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <net/sock.h> /* struct sock */
#include <net/af_unix.h>
#include <linux/spinlock.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif /* CONFIG_PROC_FS */

#include <linux/types.h>
 
#include <asm/uaccess.h>
#include <linux/in6.h>

#include <net/pfkeyv2.h>
#include <net/pfkey.h>
#include <net/spd.h>

#include <linux/ipsec.h>

#include "pfkey_v2_msg.h"

#define SENDERR(_x) do { error = -(_x); goto errlab; } while (0)

struct proto_ops SOCKOPS_WRAPPED(pfkey_ops);
struct sock *pfkey_sock_list = NULL;

struct socket_list *pfkey_open_sockets = NULL;
struct socket_list *pfkey_registered_sockets[SADB_SATYPE_MAX+1];
rwlock_t pfkey_sk_lock = RW_LOCK_UNLOCKED;

#ifdef CONFIG_SYSCTL
extern void ipsec_sysctl_register(void);
extern void ipsec_sysctl_unregister(void);
#endif /* CONFIG_SYSCTL */

/* derived from FreeS/WAN-1.9 pfkey_v2_parse.c (mk) */
int
pfkey_msg_interp(struct sock *sk, struct sadb_msg *pfkey_msg)
{
	int error = 0;
	struct sadb_msg *pfkey_reply = NULL;
	struct sadb_ext *reply_ext_msgs[SADB_EXT_MAX+1];
	struct socket_list *pfkey_socketsp = 0;

	if (pfkey_msg->sadb_msg_satype > SADB_SATYPE_MAX) {
		return -EINVAL;
	}
	
	memset(reply_ext_msgs, 0, SADB_EXT_MAX+1);

	switch (pfkey_msg->sadb_msg_type) {
	case SADB_GETSPI:
		error = sadb_msg_add_parse(sk, pfkey_msg, &pfkey_reply);
		PFKEY_DEBUG("PF_KEY:interp: SADB_GETSPI\n");
		break;
	case SADB_UPDATE:
		error = -EINVAL;/* XXX */
		PFKEY_DEBUG("PF_KEY:interp: SADB_UPDATE\n");
		break;
	case SADB_ADD:
		error = sadb_msg_add_parse(sk, pfkey_msg, &pfkey_reply);
		PFKEY_DEBUG("PF_KEY:interp: SADB_ADD\n");
		break;
	case SADB_DELETE:
		error = sadb_msg_delete_parse(sk, pfkey_msg, &pfkey_reply);
		PFKEY_DEBUG("PF_KEY:interp: SADB_DELETE\n");
		break;
	case SADB_FLUSH:
		error = sadb_msg_flush_parse(sk, pfkey_msg, &pfkey_reply);
		PFKEY_DEBUG("PF_KEY:interp: SADB_FLUSH\n");
		break;
	case SADB_REGISTER:
		error = sadb_msg_register_parse(sk, pfkey_msg, &pfkey_reply);
		PFKEY_DEBUG("PF_KEY:interp: SADB_REGISTER\n");
		break;
	case SADB_X_GRPSA:
		error =  -EINVAL;/* XXX */
		PFKEY_DEBUG("PF_KEY:interp: SADB_X_GRPSA\n");
		break;
	case SADB_X_ADDFLOW:
		error = sadb_msg_addflow_parse(sk, pfkey_msg, &pfkey_reply);
		PFKEY_DEBUG("PF_KEY:interp: SADB_ADDFLOW\n");
		break;
	case SADB_X_DELFLOW:
		error = sadb_msg_delflow_parse(sk, pfkey_msg, &pfkey_reply);
		PFKEY_DEBUG("PF_KEY:interp: SADB_DELFLOW\n");
		break;

	case SADB_X_FLUSH_SP:
		error = sadb_msg_flush_sp_parse(sk, pfkey_msg, &pfkey_reply);
		PFKEY_DEBUG("PFKEY:interp: SADB_X_FLUSH_SP\n");
		break;
	default:
		error = -EINVAL;
		break;
	}
	
	if (error) {
		PFKEY_DEBUG("PFKEY:interp: parse routine return error=%d\n", error);
		goto err;
	}

	switch (pfkey_msg->sadb_msg_type) {

	case SADB_GETSPI:
	case SADB_UPDATE:
	case SADB_ADD:
	case SADB_DELETE:
	case SADB_FLUSH:
	case SADB_X_ADDFLOW:
	case SADB_X_DELFLOW:
	case SADB_X_FLUSH_SP:
		write_lock_bh(&pfkey_sk_lock);
		for (pfkey_socketsp = pfkey_open_sockets;
	    		pfkey_socketsp;
	    			pfkey_socketsp = pfkey_socketsp->next)
		{
			pfkey_upmsg(pfkey_socketsp->socketp, pfkey_reply);
		}
		write_unlock_bh(&pfkey_sk_lock);
		break;

	case SADB_GET:
	case SADB_DUMP:
	case SADB_REGISTER:
		write_lock_bh(&pfkey_sk_lock);
		pfkey_upmsg(sk->socket, pfkey_reply);
		write_unlock_bh(&pfkey_sk_lock);
		break;

	case SADB_ACQUIRE:
		write_lock_bh(&pfkey_sk_lock);
		for (pfkey_socketsp = pfkey_registered_sockets[pfkey_msg->sadb_msg_satype];
			pfkey_socketsp;
				pfkey_socketsp = pfkey_socketsp->next)
		{
			pfkey_upmsg(pfkey_socketsp->socketp, pfkey_reply);
		}
		write_unlock_bh(&pfkey_sk_lock);
		break;

	default:
		error = -EINVAL;
		goto err;
	}

	if (pfkey_reply)
		kfree(pfkey_reply);

	return 0;
err:
	if (pfkey_reply)
		kfree(pfkey_reply);

	pfkey_reply = kmalloc(sizeof(struct sadb_msg), GFP_KERNEL);
	if (!pfkey_reply){
		return -ENOMEM;
	}
	memcpy(pfkey_reply, pfkey_msg, sizeof(pfkey_reply));
	pfkey_reply->sadb_msg_errno = error;

	write_lock_bh(&pfkey_sk_lock);
	pfkey_upmsg(sk->socket, pfkey_reply);
	for (pfkey_socketsp = pfkey_registered_sockets[pfkey_msg->sadb_msg_satype];
		pfkey_socketsp;
			pfkey_socketsp = pfkey_socketsp->next)
	{
		pfkey_upmsg(pfkey_socketsp->socketp, pfkey_reply);
	}
	write_unlock_bh(&pfkey_sk_lock);

	if (pfkey_reply) {
		kfree(pfkey_reply);
	}

	return error;
}

int
pfkey_list_remove_socket(struct socket *socketp, struct socket_list **sockets)
{
	struct socket_list *socket_listp,*prev;

	PFKEY_DEBUG("called\n");
	if (!socketp) {
		return -EINVAL;
	}

	if (!sockets) {
		return -EINVAL;
	}

	socket_listp = *sockets;
	prev = NULL;
	
	while (socket_listp != NULL) {
		if (socket_listp->socketp == socketp) {
			if (prev != NULL) {
				prev->next = socket_listp->next;
			} else {
				*sockets = socket_listp->next;
			}
			
			kfree((void*)socket_listp);
			PFKEY_DEBUG("removed sock=%p\n", socketp);
			
			break;
		}
		prev = socket_listp;
		socket_listp = socket_listp->next;
	}
	PFKEY_DEBUG("end\n");

	return 0;
}

int
pfkey_list_insert_socket(struct socket *socketp, struct socket_list **sockets)
{
	struct socket_list *socket_listp;

	PFKEY_DEBUG("called\n");
	if (!socketp) {
		return -EINVAL;
	}

	if (!sockets) {
		return -EINVAL;
	}

	if ((socket_listp = (struct socket_list *)kmalloc(sizeof(struct socket_list), GFP_KERNEL)) == NULL) {
		return -ENOMEM;
	}
	
	socket_listp->socketp = socketp;
	socket_listp->next = *sockets;
	*sockets = socket_listp;
	PFKEY_DEBUG("inserted sock=%p\n", socketp);
	PFKEY_DEBUG("end\n");

	return 0;
}
  
static void
pfkey_insert_socket(struct sock *sk)
{
	PFKEY_DEBUG("called\n");
	write_lock_bh(&pfkey_sk_lock);
	sk->next=pfkey_sock_list;
	pfkey_sock_list=sk;
	write_unlock_bh(&pfkey_sk_lock);
	PFKEY_DEBUG("end\n");
}

static void
pfkey_remove_socket(struct sock *sk)
{
	struct sock **s;
	
	PFKEY_DEBUG("called\n");
	s=&pfkey_sock_list;

	while (*s!=NULL) {
		if (*s==sk) {
			*s=sk->next;
			sk->next=NULL;
			goto final;
		}
		s=&((*s)->next);
	}

	PFKEY_DEBUG("end\n");
final:
	return;
}

static void
pfkey_destroy_socket(struct sock *sk)
{
	struct sk_buff *skb;

	PFKEY_DEBUG("called\n");
	pfkey_remove_socket(sk);

	while (sk && (skb=skb_dequeue(&sk->receive_queue)) !=NULL ) {
#if defined(CONFIG_IPSEC_DEBUG) && defined(CONFIG_SYSCTL)
		if (sysctl_ipsec_debug_pfkey) {
			printk(KERN_DEBUG "pfkey_destroy_socket: pfkey_skb contents:");
			printk(" next:%p", skb->next);
			printk(" prev:%p", skb->prev);
			printk(" list:%p", skb->list);
			printk(" sk:%p", skb->sk);
			printk(" stamp:%ld.%ld", skb->stamp.tv_sec, skb->stamp.tv_usec);
			printk(" dev:%p", skb->dev);
			if (skb->dev) {
				if (skb->dev->name) {
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
			{
				int i;
				
				printk(" cb");
				for (i=0; i<48; i++) {
					printk(":%2x", skb->cb[i]);
				}
			}
			printk(" len:%d", skb->len);
			printk(" csum:%d", skb->csum);
			printk(" cloned:%d", skb->cloned);
			printk(" pkt_type:%d", skb->pkt_type);
			printk(" ip_summed:%d", skb->ip_summed);
			printk(" priority:%d", skb->priority);
			printk(" protocol:%d", skb->protocol);
			printk(" security:%d", skb->security);
			printk(" truesize:%d", skb->truesize);
			printk(" head:%p", skb->head);
			printk(" data:%p", skb->data);
			printk(" tail:%p", skb->tail);
			printk(" end:%p", skb->end);
			{
				unsigned int i;
				printk(" data");
				for (i=(unsigned int)(skb->head); i<(unsigned int)(skb->end); i++) {
					printk(":%2x", (unsigned char)(*(char*)(i)));
				}
			}
			printk(" destructor:%p", skb->destructor);
			printk("\n");
		}
#endif /* CONFIG_IPSEC_DEBUG and CONFIG_SYSCTL */
		PFKEY_DEBUG("skb=%p freed.\n", skb);
		kfree_skb(skb);
	}

	sk->dead = 1;
	sk_free(sk);
	PFKEY_DEBUG("end\n");
}

int
pfkey_upmsg(struct socket *sock, struct sadb_msg *pfkey_msg)
{
	int error;
	struct sk_buff * skb = NULL;
	struct sock *sk;

	PFKEY_DEBUG("called\n");
	if (sock == NULL) {
		return -EINVAL;
	}

	if (pfkey_msg == NULL) {
		return -EINVAL;
	}

	sk = sock->sk;

	if (sk == NULL) {
		return -EINVAL;
	}

	if (!(skb = alloc_skb(pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN, GFP_ATOMIC) )) {
		return -ENOBUFS;
	}
	
	skb->dev = NULL;
	
	if (skb_tailroom(skb) < pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) {
		printk(KERN_WARNING "klips_error:pfkey_upmsg: "
		       "tried to skb_put %ld, %d available.  This should never happen, please report.\n",
		       (unsigned long int)pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN,
		       skb_tailroom(skb));
		kfree_skb(skb);

		return -ENOBUFS;
	}
	skb->h.raw = skb_put(skb, pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN);
	memcpy(skb->h.raw, pfkey_msg, pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN);

	if ((error = sock_queue_rcv_skb(sk, skb)) < 0) {
		skb->sk=NULL;
		kfree_skb(skb);
		return error;
	}
	PFKEY_DEBUG("end\n");
	return 0;
}

static int
pfkey_create(struct socket *sock, int protocol)
{
	struct sock *sk;

	PFKEY_DEBUG("called\n");
	if (sock == NULL) {
		return -EINVAL;
	}

	if (sock->type != SOCK_RAW) {
		return -ESOCKTNOSUPPORT;
	}

	if (protocol != PF_KEY_V2) {
		return -EPROTONOSUPPORT;
	}

	if ((current->uid != 0)) {
		return -EACCES;
	}

	sock->state = SS_UNCONNECTED;
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif /* MODULE */

	if ((sk=(struct sock *)sk_alloc(PF_KEY, GFP_KERNEL, 1)) == NULL)
	{
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif /* MODULE */
		return -ENOMEM;
	}

	sock_init_data(sock, sk);

	sk->destruct = NULL;
	sk->reuse = 1;
	sock->ops = &SOCKOPS_WRAPPED(pfkey_ops);

	sk->zapped=0;
	sk->family = PF_KEY;
	sk->protocol = protocol;
	key_pid(sk) = current->pid;

	pfkey_insert_socket(sk);
	pfkey_list_insert_socket(sock, &pfkey_open_sockets);

	PFKEY_DEBUG("end\n");
	return 0;
}

static int
pfkey_release(struct socket *sock)
{
	struct sock *sk;
	int i;

	PFKEY_DEBUG("called\n");
	if (sock==NULL) {
		return 0; /* -EINVAL; */
	}
		
	sk=sock->sk;
	
	/* May not have data attached */
	if (sk==NULL) {
		return 0; /* -EINVAL; */
	}
		
	if (!sk->dead)
		if (sk->state_change) {
			sk->state_change(sk);
		}

	sock->sk = NULL;

	/* Try to flush out this socket. Throw out buffers at least */
	write_lock_bh(&pfkey_sk_lock);
	pfkey_destroy_socket(sk);
	pfkey_list_remove_socket(sock, &pfkey_open_sockets);
	for (i = SADB_SATYPE_UNSPEC; i <= SADB_SATYPE_MAX; i++) {
		pfkey_list_remove_socket(sock, &pfkey_registered_sockets[i]);
		PFKEY_DEBUG("socket=%p is released, SA type is %d\n", sock, i);
	}
	write_unlock_bh(&pfkey_sk_lock);

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif /* MODULE */

	PFKEY_DEBUG("called\n");
	return 0;
}

static int
pfkey_shutdown(struct socket *sock, int mode)
{
	struct sock *sk;

	PFKEY_DEBUG("called\n");
	if (sock == NULL) {
		return -EINVAL;
	}

	sk=sock->sk;
	
	if (sk == NULL) {
		return -EINVAL;
	}

	mode++;
	
	if (mode&SEND_SHUTDOWN) {
		sk->shutdown|=SEND_SHUTDOWN;
		sk->state_change(sk);
	}

	if (mode&RCV_SHUTDOWN) {
		sk->shutdown|=RCV_SHUTDOWN;
		sk->state_change(sk);
	}
	PFKEY_DEBUG("end\n");
	return 0;
}

/*
 *	Send PF_KEY data down.
 */
		
static int
pfkey_sendmsg(struct socket *sock, struct msghdr *msg, int len, struct scm_cookie *scm)
{
	struct sock *sk;
	int error = 0;
	struct sadb_msg *pfkey_msg = NULL;

	PFKEY_DEBUG("called\n");
	if (sock == NULL) {
		SENDERR(EINVAL);
	}
	sk = sock->sk;

	if (sk == NULL) {
		SENDERR(EINVAL);
	}
	
	if (msg == NULL) {
		SENDERR(EINVAL);
	}

	if (sk->err) {
		error = sock_error(sk);
		SENDERR(-error);
	}

	if ((current->uid != 0)) {
		SENDERR(EACCES);
	}

	if (msg->msg_control) {
		SENDERR(EINVAL);
	}
		
	if (sk->shutdown & SEND_SHUTDOWN) {
		send_sig(SIGPIPE, current, 0);
		SENDERR(EPIPE);
	}
	
	if (len < sizeof(struct sadb_msg)) {
		SENDERR(EMSGSIZE);
	}

	if ((pfkey_msg = (struct sadb_msg*)kmalloc(len, GFP_KERNEL)) == NULL) {
		SENDERR(ENOBUFS);
	}

	memcpy_fromiovec((void *)pfkey_msg, msg->msg_iov, len);

	if (pfkey_msg->sadb_msg_version != PF_KEY_V2) {
		kfree((void*)pfkey_msg);
		return -EINVAL;
	}

	if (len != pfkey_msg->sadb_msg_len * IPSEC_PFKEYv2_ALIGN) {
		SENDERR(EMSGSIZE);
	}

#if 0
	/* This check is questionable, since a downward message could be
	   the result of an ACQUIRE either from kernel (PID==0) or
	   userspace (some other PID). */
	/* check PID */
	if (pfkey_msg->sadb_msg_pid != current->pid) {
		SENDERR(EINVAL);
	}
#endif

	if (pfkey_msg->sadb_msg_reserved) {
		SENDERR(EINVAL);
	}
	
	if ((pfkey_msg->sadb_msg_type > SADB_MAX) || (!pfkey_msg->sadb_msg_type)) {
		SENDERR(EINVAL);
	}
	
	error = pfkey_msg_interp(sk, pfkey_msg);
	if (error) {
		SENDERR(-error);
	}
	PFKEY_DEBUG("end\n");

 errlab:
	if (pfkey_msg) {
		kfree((void*)pfkey_msg);
	}
	
	if (error) {
		return error;
	} else {
		return len;
	}
}

/*
 *	Receive PF_KEY data up.
 */
		
static int
pfkey_recvmsg(struct socket *sock, struct msghdr *msg, int size, int flags, struct scm_cookie *scm)
{
	struct sock *sk;
	int noblock = flags & MSG_DONTWAIT;
	struct sk_buff *skb;
	int error;

	PFKEY_DEBUG("called\n");
	if (sock == NULL) {
		return -EINVAL;
	}

	sk = sock->sk;

	if (sk == NULL) {
		return -EINVAL;
	}

	if (msg == NULL) {
		return -EINVAL;
	}

	if (flags & ~MSG_PEEK) {
		return -EOPNOTSUPP;
	}
		
	msg->msg_namelen = 0; /* sizeof(*ska); */
		
	if (sk->err) {
		return sock_error(sk);
	}

	if ((skb = skb_recv_datagram(sk, flags, noblock, &error) ) == NULL) {
                return error;
	}

	if (size > skb->len) {
		size = skb->len;
	}

	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, size);
        sk->stamp=skb->stamp;

	skb_free_datagram(sk, skb);
	PFKEY_DEBUG("end\n");
	return size;
}

struct net_proto_family pfkey_family_ops = {
	PF_KEY,
	pfkey_create
};

struct proto_ops SOCKOPS_WRAPPED(pfkey_ops) = {
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
};

#include <linux/smp_lock.h>
#if 0
	SOCKOPS_WRAP(pfkey, PF_KEY);
#endif

   
#ifdef CONFIG_PROC_FS
int
pfkey_get_info(char *buffer, char **start, off_t offset, int length)
{
	off_t pos=0;
	off_t begin=0;
	int len=0;
	struct sock *sk=pfkey_sock_list;
	
	len+= sprintf(buffer,
		      "    sock   pid   socket     next     prev e n p sndbf    Flags     Type St\n");
	
	while (sk!=NULL) {
		len+=sprintf(buffer+len,
			     "%8p %5d %8p %8p %8p %d %d %d %5d %08lX %8X %2X\n",
			     sk,
			     key_pid(sk),
			     sk->socket,
			     sk->next,
			     sk->prev,
			     sk->err,
			     sk->num,
			     sk->protocol,
			     sk->sndbuf,
			     sk->socket->flags,
			     sk->socket->type,
			     sk->socket->state);
		pos=begin+len;
		if (pos<offset) {
			len=0;
			begin=pos;
		}
		if (pos>offset+length)
			break;
		sk=sk->next;
	}
	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if (len>length)
		len=length;
	return len;
}

int
pfkey_registered_get_info(char *buffer, char **start, off_t offset, int length)
{
	off_t pos=0;
	off_t begin=0;
	int len=0;
	int satype;
	struct socket_list *pfkey_sockets;
	
	len+= sprintf(buffer,
		      "satype   socket   pid       sk\n");
	
	for (satype = SADB_SATYPE_UNSPEC; satype <= SADB_SATYPE_MAX; satype++) {
		pfkey_sockets = pfkey_registered_sockets[satype];
		while (pfkey_sockets) {
			len+=sprintf(buffer+len,
				     "    %2d %8p %5d %8p\n",
				     satype,
				     pfkey_sockets->socketp,
				     key_pid(pfkey_sockets->socketp->sk),
				     pfkey_sockets->socketp->sk);
			
			pos=begin+len;
			if (pos<offset) {
				len=0;
				begin=pos;
			}
			if (pos>offset+length)
				break;
			pfkey_sockets = pfkey_sockets->next;
		}
	}
	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if (len>length)
		len=length;
	return len;
}
#endif /* CONFIG_PROC_FS */

int
pfkey_init(void)
{
	int error = 0;

        sock_register(&pfkey_family_ops);

#ifdef CONFIG_PROC_FS
	proc_net_create ("pf_key", 0, pfkey_get_info);
	proc_net_create ("pf_key_registered", 0, pfkey_registered_get_info);
#endif          /* CONFIG_PROC_FS */


	error = sadb_init();
	if (error) {
		PFKEY_DEBUG("sadb_init failed\n");
		goto err;
	}
	error = spd_init();
	if (error) {
		PFKEY_DEBUG("spd_init faild\n");
		goto err;
	}
#ifdef CONFIG_SYSCTL
	ipsec_sysctl_register();
#endif /* CONFIG_SYSCTL */

	printk(KERN_INFO "IPsec PF_KEY V2: initialized\n");

err:
	return error;
}

int
pfkey_cleanup(void)
{
	int error = 0;
	
	error = spd_cleanup();
	if (error) {
		PFKEY_DEBUG("spd_cleanup failed\n");
		goto err;
	}
	error = sadb_cleanup();
	if (error) {
		PFKEY_DEBUG("sadb_cleanup failed\n");
		goto err;
	}

	printk(KERN_INFO "pfkey_cleanup: shutting down PF_KEY domain sockets.\n");

        sock_unregister(PF_KEY);


#ifdef CONFIG_PROC_FS
	proc_net_remove ("pf_key");
	proc_net_remove ("pf_key_registered");
#endif          /* CONFIG_PROC_FS */

#ifdef CONFIG_SYSCTL
	ipsec_sysctl_unregister();
#endif  

	/* other module unloading cleanup happens here */
err:
	return error;
}

#ifdef MODULE
static int __init pfkey_module_init(void)
{
	int err = pfkey_init();
	return err;
}

static void __exit pfkey_module_cleanup(void)
{
	pfkey_cleanup();
}

module_init(pfkey_module_init);
module_exit(pfkey_module_cleanup);

#else /* MODULE */
void
pfkey_proto_init(struct net_proto *pro)
{
	pfkey_init();
}
#endif /* MODULE */

