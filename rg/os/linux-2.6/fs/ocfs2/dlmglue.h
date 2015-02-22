/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * dlmglue.h
 *
 * description here
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */


#ifndef DLMGLUE_H
#define DLMGLUE_H

#define OCFS2_LVB_VERSION 2

struct ocfs2_meta_lvb {
	__be32       lvb_version;
	__be32       lvb_iclusters;
	__be32       lvb_iuid;
	__be32       lvb_igid;
	__be64       lvb_iatime_packed;
	__be64       lvb_ictime_packed;
	__be64       lvb_imtime_packed;
	__be64       lvb_isize;
	__be16       lvb_imode;
	__be16       lvb_inlink;
	__be32       lvb_reserved[3];
};

/* ocfs2_meta_lock_full() and ocfs2_data_lock_full() 'arg_flags' flags */
/* don't wait on recovery. */
#define OCFS2_META_LOCK_RECOVERY	(0x01)
/* Instruct the dlm not to queue ourselves on the other node. */
#define OCFS2_META_LOCK_NOQUEUE		(0x02)
/* don't block waiting for the vote thread, instead return -EAGAIN */
#define OCFS2_LOCK_NONBLOCK		(0x04)

int ocfs2_dlm_init(struct ocfs2_super *osb);
void ocfs2_dlm_shutdown(struct ocfs2_super *osb);
void ocfs2_lock_res_init_once(struct ocfs2_lock_res *res);
void ocfs2_inode_lock_res_init(struct ocfs2_lock_res *res,
			       enum ocfs2_lock_type type,
			       struct inode *inode);
void ocfs2_lock_res_free(struct ocfs2_lock_res *res);
int ocfs2_create_new_inode_locks(struct inode *inode);
int ocfs2_drop_inode_locks(struct inode *inode);
int ocfs2_data_lock_full(struct inode *inode,
			 int write,
			 int arg_flags);
#define ocfs2_data_lock(inode, write) ocfs2_data_lock_full(inode, write, 0)
int ocfs2_data_lock_with_page(struct inode *inode,
			      int write,
			      struct page *page);
void ocfs2_data_unlock(struct inode *inode,
		       int write);
int ocfs2_rw_lock(struct inode *inode, int write);
void ocfs2_rw_unlock(struct inode *inode, int write);
int ocfs2_meta_lock_full(struct inode *inode,
			 struct ocfs2_journal_handle *handle,
			 struct buffer_head **ret_bh,
			 int ex,
			 int arg_flags);
int ocfs2_meta_lock_with_page(struct inode *inode,
			      struct ocfs2_journal_handle *handle,
			      struct buffer_head **ret_bh,
			      int ex,
			      struct page *page);
/* 99% of the time we don't want to supply any additional flags --
 * those are for very specific cases only. */
#define ocfs2_meta_lock(i, h, b, e) ocfs2_meta_lock_full(i, h, b, e, 0)
void ocfs2_meta_unlock(struct inode *inode,
		       int ex);
int ocfs2_super_lock(struct ocfs2_super *osb,
		     int ex);
void ocfs2_super_unlock(struct ocfs2_super *osb,
			int ex);
int ocfs2_rename_lock(struct ocfs2_super *osb);
void ocfs2_rename_unlock(struct ocfs2_super *osb);
void ocfs2_mark_lockres_freeing(struct ocfs2_lock_res *lockres);

/* for the vote thread */
void ocfs2_process_blocked_lock(struct ocfs2_super *osb,
				struct ocfs2_lock_res *lockres);

struct ocfs2_dlm_debug *ocfs2_new_dlm_debug(void);
void ocfs2_put_dlm_debug(struct ocfs2_dlm_debug *dlm_debug);

/* aids in debugging and tracking lvbs */
void ocfs2_dump_meta_lvb_info(u64 level,
			      const char *function,
			      unsigned int line,
			      struct ocfs2_lock_res *lockres);
#define mlog_meta_lvb(__level, __lockres) ocfs2_dump_meta_lvb_info(__level, __PRETTY_FUNCTION__, __LINE__, __lockres)

#endif	/* DLMGLUE_H */
