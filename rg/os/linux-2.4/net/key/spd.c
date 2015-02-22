/* $USAGI: spd.c,v 1.12 2002/08/13 10:51:12 miyazawa Exp $ */
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
/*
 * spd.c provide manipulatoin routines for IPsec SPD.
 * struct ipsec_sp represent a policy in IPsec SPD.
 * struct ipsec_sp refers IPsec SA by struct sa_index.
 */

#ifdef __KERNEL__

#ifdef MODULE
#  include <linux/module.h>
#  ifdef MODVERSIONS
#    include <linux/modversions.h>
#  endif /* MODVERSIONS */
#endif /* MODULE */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/socket.h> /* sa_family_t */
#include <linux/skbuff.h> /* sk_buff */
#include <linux/ipsec.h>

#include <net/pfkeyv2.h>
#include <net/spd.h>
#include <net/sadb.h>

#include "spd_utils.h"
#include "sadb_utils.h"

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif /* CONFIG_PROC_FS */

#endif /* __KERNEL */

#define BUFSIZE 64

/* 
 * spd_list represents SPD as a list.
 * spd_lock is a lock for SPD.
 * If you access a element in SPD,
 * you must lock and unlock each element before access.
 *
 * 
 * struct list_head *pos = NULL;
 * struct ipsec_sp *tmp = NULL;
 *                                
 * read_lock(&spd_lock);
 * list_for_each(pos, &spd_list){
 *      tmp = list_entry(pos, struct ipsec_sp, entry);
 *      read_lock(&tmp->lock);
 *
 *      some operation
 *
 *      read_unlock(&tmp->lock);
 * }
 * read_unlock(&spd_lock);
 *
 * ## If you want to change the list or a element,
 *    you read these code with write_lock, write_unlock
 *    instead of read_lock, read_unlock. 
 */

LIST_HEAD(spd_list);
rwlock_t spd_lock = RW_LOCK_UNLOCKED;

struct ipsec_sp* ipsec_sp_kmalloc()
{
	struct ipsec_sp* sp = NULL;

	sp = (struct ipsec_sp*)kmalloc(sizeof(struct ipsec_sp), GFP_KERNEL);

	if (!sp) {
		SPD_DEBUG("entry couldn\'t be allocated.\n");
		return NULL;
	}

	ipsec_sp_init(sp);

	return sp;
}

int ipsec_sp_init(struct ipsec_sp *policy)
{
	if (!policy) {
		SPD_DEBUG("policy is null\n");
		return -EINVAL;
	}

	memset(policy, 0, sizeof(struct ipsec_sp));
	policy->auth_sa_idx = NULL;
	policy->esp_sa_idx = NULL;
	policy->comp_sa_idx = NULL;
	atomic_set(&policy->refcnt,1);
	policy->lock = RW_LOCK_UNLOCKED;

	return 0;
}

void ipsec_sp_kfree(struct ipsec_sp *policy)
{
	if (!policy) {
		SPD_DEBUG("entry is null\n");
		return;
	}

	if (atomic_read(&policy->refcnt)) {
		SPD_DEBUG("policy has been referenced\n");
		return;
	}

	if (policy->auth_sa_idx) sa_index_kfree(policy->auth_sa_idx);
	if (policy->esp_sa_idx) sa_index_kfree(policy->esp_sa_idx);
	if (policy->comp_sa_idx) sa_index_kfree(policy->comp_sa_idx);

	kfree(policy);
}

int ipsec_sp_copy(struct ipsec_sp *dst, struct ipsec_sp *src)
{
	int error = 0;

	if (!dst || !src) {
		SPD_DEBUG("dst or src is null\n");
		error = -EINVAL;
		goto err;
	}

	memcpy(&dst->selector, &src->selector, sizeof(struct selector));

	if (dst->auth_sa_idx) sa_index_kfree(dst->auth_sa_idx);
	if (dst->esp_sa_idx) sa_index_kfree(dst->esp_sa_idx);
	if (dst->comp_sa_idx) sa_index_kfree(dst->comp_sa_idx);

	if (src->auth_sa_idx) {
		dst->auth_sa_idx = sa_index_kmalloc();
		memcpy(dst->auth_sa_idx, src->auth_sa_idx, sizeof(struct sa_index));
	}

	if (src->esp_sa_idx) {
		dst->esp_sa_idx = sa_index_kmalloc();
		memcpy(dst->esp_sa_idx, src->esp_sa_idx, sizeof(struct sa_index));
	}

	if (src->comp_sa_idx) {
		dst->comp_sa_idx = sa_index_kmalloc();
		memcpy(dst->comp_sa_idx, src->comp_sa_idx, sizeof(struct sa_index));
	}

	dst->policy_action = src->policy_action;

	atomic_set(&dst->refcnt, 1);
err:
	return error;
}

int ipsec_sp_put(struct ipsec_sp *policy)
{
	int error = 0;

	if (!policy) {
		SPD_DEBUG("policy is null\n");
		error = -EINVAL;
		goto err;
	}

	write_lock_bh(&policy->lock);
	SPD_DEBUG("ptr=%p,refcnt=%d\n",
			policy, atomic_read(&policy->refcnt));

	if (atomic_dec_and_test(&policy->refcnt)) {

		SPD_DEBUG("ptr=%p,refcnt=%d\n",
			policy, atomic_read(&policy->refcnt));

		write_unlock_bh(&policy->lock);

		ipsec_sp_kfree(policy);

		return 0;
	}

	write_unlock_bh(&policy->lock);

err:
	return error;
}

void ipsec_sp_release_invalid_sa(struct ipsec_sp *policy, struct ipsec_sa *sa)
{
	if (!policy) {
		SPD_DEBUG("spd_check_sa_list: policy is null\n");
		return;
	}

	if (policy->auth_sa_idx && policy->auth_sa_idx->sa == sa) {
		ipsec_sa_put(policy->auth_sa_idx->sa);
		policy->auth_sa_idx->sa = NULL;
	}

	if (policy->esp_sa_idx && policy->esp_sa_idx->sa == sa) {
		ipsec_sa_put(policy->esp_sa_idx->sa);
		policy->esp_sa_idx->sa = NULL;
	}

	if (policy->comp_sa_idx && policy->comp_sa_idx->sa == sa) {
		ipsec_sa_put(policy->comp_sa_idx->sa);
		policy->comp_sa_idx->sa = NULL;
	}
}

int spd_append(struct ipsec_sp *policy)
{
	int error = 0;
	struct ipsec_sp *new = NULL;

	if (!policy) {
		SPD_DEBUG("policy is null\n");
		error = -EINVAL;
		goto err;
	}

	new = ipsec_sp_kmalloc();
	if (!new) {
		SPD_DEBUG("ipsec_sp_kmalloc failed\n");
		error = -ENOMEM;
		goto err;
	}

	error = ipsec_sp_init(new);
	if (error) {
		SPD_DEBUG("ipsec_sp_init failed\n");
		goto err;
	}

	error = ipsec_sp_copy(new, policy);
	if (error) {
		SPD_DEBUG("ipsec_sp_copy failed\n");
		goto err;
	}

	write_lock_bh(&spd_lock);
	list_add_tail(&new->entry, &spd_list);
	write_unlock_bh(&spd_lock);
err:
	return error;
}

int spd_remove(struct selector *selector)
{
	int error = -ESRCH;
	struct list_head *pos = NULL;
	struct list_head *next = NULL;
	struct ipsec_sp *tmp_sp = NULL;

	if (!selector) {
		SPD_DEBUG("selector is null\n");
		error = -EINVAL;
		goto err;
	}

	write_lock_bh(&spd_lock);
	list_for_each_safe(pos, next, &spd_list){
		tmp_sp = list_entry(pos, struct ipsec_sp, entry);
		write_lock_bh(&tmp_sp->lock);
		if (!compare_selector(selector, &tmp_sp->selector)) {
			SPD_DEBUG("found matched element\n");
			error = 0;
			list_del(&tmp_sp->entry);
			write_unlock_bh(&tmp_sp->lock);
			ipsec_sp_put(tmp_sp);
			break;
		}
		write_unlock_bh(&tmp_sp->lock);
	}
	write_unlock_bh(&spd_lock);

err:
	SPD_DEBUG("error=%d\n", error);
	return error;
}

int spd_find_by_selector(struct selector *selector, struct ipsec_sp **policy)
{
	int error = -ESRCH;
	struct list_head *pos = NULL;
	struct ipsec_sp *tmp_sp = NULL;

	if (!selector) {
		SPD_DEBUG("selector is null\n");
		error = -EINVAL;
		goto err;
	}
	
	read_lock(&spd_lock);
	list_for_each(pos, &spd_list){
		tmp_sp = list_entry(pos, struct ipsec_sp, entry);
		read_lock_bh(&tmp_sp->lock);
		if (!compare_selector(selector, &tmp_sp->selector)) {
			SPD_DEBUG("found matched element\n");
			error = -EEXIST;
			*policy = tmp_sp;
			atomic_inc(&(*policy)->refcnt);
			read_unlock_bh(&tmp_sp->lock);
			break;
		}
		read_unlock_bh(&tmp_sp->lock);
	}
	read_unlock(&spd_lock);
	

err:
	return error;
}

void spd_clear_db()
{
	struct list_head *pos;
	struct list_head *next;
	struct ipsec_sp *policy;

	write_lock_bh(&spd_lock);
	list_for_each_safe(pos, next, &spd_list){
		policy = list_entry(pos, struct ipsec_sp, entry);
		list_del(&policy->entry);
		ipsec_sp_kfree(policy);		
	}
	write_unlock_bh(&spd_lock);
}


#ifdef CONFIG_PROC_FS
static int spd_get_info(char *buffer, char **start, off_t offset, int length)
{
	int error = 0;
	int count = 0;
	int len = 0;
        off_t pos=0;
        off_t begin=0;
        char buf[BUFSIZE]; 
        struct list_head *list_pos = NULL;
        struct ipsec_sp *tmp_sp = NULL;
	
        read_lock_bh(&spd_lock);
        list_for_each(list_pos, &spd_list){
#if 0
		SPD_DEBUG("list_pos->prev=%p\n", list_pos->prev);
		SPD_DEBUG("list_pos->next=%p\n", list_pos->next);
#endif
		count = 0;
                tmp_sp = list_entry(list_pos, struct ipsec_sp, entry);
		read_lock_bh(&tmp_sp->lock);

		len += sprintf(buffer + len, "spd:%p\n", tmp_sp);
                memset(buf, 0, BUFSIZE);
                sockaddrtoa((struct sockaddr*)&tmp_sp->selector.src, buf, BUFSIZE);
                len += sprintf(buffer + len, "%s/%u ", buf, tmp_sp->selector.prefixlen_s);
		sockporttoa((struct sockaddr *)&tmp_sp->selector.src, buf, BUFSIZE);
		len += sprintf(buffer + len, "%s ", buf);
                memset(buf, 0, BUFSIZE);
                sockaddrtoa((struct sockaddr*)&tmp_sp->selector.dst, buf, BUFSIZE);
                len += sprintf(buffer + len, "%s/%u ", buf, tmp_sp->selector.prefixlen_d);
		sockporttoa((struct sockaddr *)&tmp_sp->selector.dst, buf, BUFSIZE);
		len += sprintf(buffer + len, "%s ", buf);
		len += sprintf(buffer + len, "%u ", tmp_sp->selector.proto);
#ifdef CONFIG_IPSEC_TUNNEL
		len += sprintf(buffer + len, "%u ", tmp_sp->selector.mode);
#endif
		len += sprintf(buffer + len, "%u\n", tmp_sp->policy_action);

		if (tmp_sp->auth_sa_idx) {
			len += sprintf(buffer + len, "sa(ah):%p ", tmp_sp->auth_sa_idx->sa);
			sockaddrtoa((struct sockaddr*)&tmp_sp->auth_sa_idx->dst, buf, BUFSIZE);
			len += sprintf(buffer + len, "%s/%d ", buf, tmp_sp->auth_sa_idx->prefixlen_d);
			len += sprintf(buffer + len, "%u ",  tmp_sp->auth_sa_idx->ipsec_proto);
			len += sprintf(buffer + len, "0x%x\n", htonl(tmp_sp->auth_sa_idx->spi));
		}

		if (tmp_sp->esp_sa_idx) {
			len += sprintf(buffer + len, "sa(esp):%p ", tmp_sp->esp_sa_idx->sa);
			sockaddrtoa((struct sockaddr*)&tmp_sp->esp_sa_idx->dst, buf, BUFSIZE);
			len += sprintf(buffer + len, "%s/%d ", buf, tmp_sp->esp_sa_idx->prefixlen_d);
			len += sprintf(buffer + len, "%u ",  tmp_sp->esp_sa_idx->ipsec_proto);
			len += sprintf(buffer + len, "0x%x\n", htonl(tmp_sp->esp_sa_idx->spi));
		}

		if (tmp_sp->comp_sa_idx) {
			len += sprintf(buffer + len, "sa(comp):%p ", tmp_sp->comp_sa_idx->sa);
			sockaddrtoa((struct sockaddr*)&tmp_sp->comp_sa_idx->dst, buf, BUFSIZE);
			len += sprintf(buffer + len, "%s/%d ", buf, tmp_sp->comp_sa_idx->prefixlen_d);
			len += sprintf(buffer + len, "%u ",  tmp_sp->comp_sa_idx->ipsec_proto);
			len += sprintf(buffer + len, "0x%x\n", htonl(tmp_sp->comp_sa_idx->spi));
		}

		read_unlock_bh(&tmp_sp->lock);
		len += sprintf(buffer + len, "\n");

                pos=begin+len;
                if (pos<offset) {
                        len=0;
                        begin=pos;
                }
                if (pos>offset+length) {
                        read_unlock_bh(&spd_lock);
                        goto done;
                }
        }       
        read_unlock_bh(&spd_lock);
done:

        *start=buffer+(offset-begin);
        len-=(offset-begin);
        if (len>length)
                len=length;
        if (len<0)
                len=0;
        return len;

	goto err;
err:
	return error;
}
#endif /* CONFIG_PROC_FS */

int spd_init(void)
{
        int error = 0;

	INIT_LIST_HEAD(&spd_list);
	SPD_DEBUG("spd_list.prev=%p\n", spd_list.prev);
	SPD_DEBUG("spd_list.next=%p\n", spd_list.next);
#ifdef CONFIG_PROC_FS
        proc_net_create("spd", 0, spd_get_info);
#endif /* CONFIG_PROC_FS */

	pr_info("spd_init: SPD initialized\n");
        return error;
}

int spd_cleanup(void)
{
        int error = 0;

#ifdef CONFIG_PROC_FS
        proc_net_remove("spd");
#endif /* CONFIG_PROC_FS */

        spd_clear_db();

	pr_info("spd_cleanup: SPD cleaned up\n");
        return error;
}

