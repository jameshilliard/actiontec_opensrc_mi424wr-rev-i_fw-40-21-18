/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * dlmrecovery.c
 *
 * recovery stuff
 *
 * Copyright (C) 2004 Oracle.  All rights reserved.
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
 *
 */


#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/utsname.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/random.h>
#include <linux/blkdev.h>
#include <linux/socket.h>
#include <linux/inet.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/delay.h>


#include "cluster/heartbeat.h"
#include "cluster/nodemanager.h"
#include "cluster/tcp.h"

#include "dlmapi.h"
#include "dlmcommon.h"
#include "dlmdomain.h"

#define MLOG_MASK_PREFIX (ML_DLM|ML_DLM_RECOVERY)
#include "cluster/masklog.h"

static void dlm_do_local_recovery_cleanup(struct dlm_ctxt *dlm, u8 dead_node);

static int dlm_recovery_thread(void *data);
void dlm_complete_recovery_thread(struct dlm_ctxt *dlm);
int dlm_launch_recovery_thread(struct dlm_ctxt *dlm);
static void dlm_kick_recovery_thread(struct dlm_ctxt *dlm);
static int dlm_do_recovery(struct dlm_ctxt *dlm);

static int dlm_pick_recovery_master(struct dlm_ctxt *dlm);
static int dlm_remaster_locks(struct dlm_ctxt *dlm, u8 dead_node);
static int dlm_init_recovery_area(struct dlm_ctxt *dlm, u8 dead_node);
static int dlm_request_all_locks(struct dlm_ctxt *dlm,
				 u8 request_from, u8 dead_node);
static void dlm_destroy_recovery_area(struct dlm_ctxt *dlm, u8 dead_node);

static inline int dlm_num_locks_in_lockres(struct dlm_lock_resource *res);
static void dlm_init_migratable_lockres(struct dlm_migratable_lockres *mres,
					const char *lockname, int namelen,
					int total_locks, u64 cookie,
					u8 flags, u8 master);
static int dlm_send_mig_lockres_msg(struct dlm_ctxt *dlm,
				    struct dlm_migratable_lockres *mres,
				    u8 send_to,
				    struct dlm_lock_resource *res,
				    int total_locks);
static int dlm_lockres_master_requery(struct dlm_ctxt *dlm,
				      struct dlm_lock_resource *res,
				      u8 *real_master);
static int dlm_process_recovery_data(struct dlm_ctxt *dlm,
				     struct dlm_lock_resource *res,
				     struct dlm_migratable_lockres *mres);
static int dlm_do_master_requery(struct dlm_ctxt *dlm,
				 struct dlm_lock_resource *res,
				 u8 nodenum, u8 *real_master);
static int dlm_send_finalize_reco_message(struct dlm_ctxt *dlm);
static int dlm_send_all_done_msg(struct dlm_ctxt *dlm,
				 u8 dead_node, u8 send_to);
static int dlm_send_begin_reco_message(struct dlm_ctxt *dlm, u8 dead_node);
static void dlm_move_reco_locks_to_list(struct dlm_ctxt *dlm,
					struct list_head *list, u8 dead_node);
static void dlm_finish_local_lockres_recovery(struct dlm_ctxt *dlm,
					      u8 dead_node, u8 new_master);
static void dlm_reco_ast(void *astdata);
static void dlm_reco_bast(void *astdata, int blocked_type);
static void dlm_reco_unlock_ast(void *astdata, enum dlm_status st);
static void dlm_request_all_locks_worker(struct dlm_work_item *item,
					 void *data);
static void dlm_mig_lockres_worker(struct dlm_work_item *item, void *data);

static u64 dlm_get_next_mig_cookie(void);

static spinlock_t dlm_reco_state_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t dlm_mig_cookie_lock = SPIN_LOCK_UNLOCKED;
static u64 dlm_mig_cookie = 1;

static u64 dlm_get_next_mig_cookie(void)
{
	u64 c;
	spin_lock(&dlm_mig_cookie_lock);
	c = dlm_mig_cookie;
	if (dlm_mig_cookie == (~0ULL))
		dlm_mig_cookie = 1;
	else
		dlm_mig_cookie++;
	spin_unlock(&dlm_mig_cookie_lock);
	return c;
}

static inline void dlm_reset_recovery(struct dlm_ctxt *dlm)
{
	spin_lock(&dlm->spinlock);
	clear_bit(dlm->reco.dead_node, dlm->recovery_map);
	dlm->reco.dead_node = O2NM_INVALID_NODE_NUM;
	dlm->reco.new_master = O2NM_INVALID_NODE_NUM;
	spin_unlock(&dlm->spinlock);
}

/* Worker function used during recovery. */
void dlm_dispatch_work(void *data)
{
	struct dlm_ctxt *dlm = (struct dlm_ctxt *)data;
	LIST_HEAD(tmp_list);
	struct list_head *iter, *iter2;
	struct dlm_work_item *item;
	dlm_workfunc_t *workfunc;

	spin_lock(&dlm->work_lock);
	list_splice_init(&dlm->work_list, &tmp_list);
	spin_unlock(&dlm->work_lock);

	list_for_each_safe(iter, iter2, &tmp_list) {
		item = list_entry(iter, struct dlm_work_item, list);
		workfunc = item->func;
		list_del_init(&item->list);

		/* already have ref on dlm to avoid having
		 * it disappear.  just double-check. */
		BUG_ON(item->dlm != dlm);

		/* this is allowed to sleep and
		 * call network stuff */
		workfunc(item, item->data);

		dlm_put(dlm);
		kfree(item);
	}
}

/*
 * RECOVERY THREAD
 */

static void dlm_kick_recovery_thread(struct dlm_ctxt *dlm)
{
	/* wake the recovery thread
	 * this will wake the reco thread in one of three places
	 * 1) sleeping with no recovery happening
	 * 2) sleeping with recovery mastered elsewhere
	 * 3) recovery mastered here, waiting on reco data */

	wake_up(&dlm->dlm_reco_thread_wq);
}

/* Launch the recovery thread */
int dlm_launch_recovery_thread(struct dlm_ctxt *dlm)
{
	mlog(0, "starting dlm recovery thread...\n");

	dlm->dlm_reco_thread_task = kthread_run(dlm_recovery_thread, dlm,
						"dlm_reco_thread");
	if (IS_ERR(dlm->dlm_reco_thread_task)) {
		mlog_errno(PTR_ERR(dlm->dlm_reco_thread_task));
		dlm->dlm_reco_thread_task = NULL;
		return -EINVAL;
	}

	return 0;
}

void dlm_complete_recovery_thread(struct dlm_ctxt *dlm)
{
	if (dlm->dlm_reco_thread_task) {
		mlog(0, "waiting for dlm recovery thread to exit\n");
		kthread_stop(dlm->dlm_reco_thread_task);
		dlm->dlm_reco_thread_task = NULL;
	}
}



/*
 * this is lame, but here's how recovery works...
 * 1) all recovery threads cluster wide will work on recovering
 *    ONE node at a time
 * 2) negotiate who will take over all the locks for the dead node.
 *    thats right... ALL the locks.
 * 3) once a new master is chosen, everyone scans all locks
 *    and moves aside those mastered by the dead guy
 * 4) each of these locks should be locked until recovery is done
 * 5) the new master collects up all of secondary lock queue info
 *    one lock at a time, forcing each node to communicate back
 *    before continuing
 * 6) each secondary lock queue responds with the full known lock info
 * 7) once the new master has run all its locks, it sends a ALLDONE!
 *    message to everyone
 * 8) upon receiving this message, the secondary queue node unlocks
 *    and responds to the ALLDONE
 * 9) once the new master gets responses from everyone, he unlocks
 *    everything and recovery for this dead node is done
 *10) go back to 2) while there are still dead nodes
 *
 */


#define DLM_RECO_THREAD_TIMEOUT_MS (5 * 1000)

static int dlm_recovery_thread(void *data)
{
	int status;
	struct dlm_ctxt *dlm = data;
	unsigned long timeout = msecs_to_jiffies(DLM_RECO_THREAD_TIMEOUT_MS);

	mlog(0, "dlm thread running for %s...\n", dlm->name);

	while (!kthread_should_stop()) {
		if (dlm_joined(dlm)) {
			status = dlm_do_recovery(dlm);
			if (status == -EAGAIN) {
				/* do not sleep, recheck immediately. */
				continue;
			}
			if (status < 0)
				mlog_errno(status);
		}

		wait_event_interruptible_timeout(dlm->dlm_reco_thread_wq,
						 kthread_should_stop(),
						 timeout);
	}

	mlog(0, "quitting DLM recovery thread\n");
	return 0;
}

/* returns true when the recovery master has contacted us */
static int dlm_reco_master_ready(struct dlm_ctxt *dlm)
{
	int ready;
	spin_lock(&dlm->spinlock);
	ready = (dlm->reco.new_master != O2NM_INVALID_NODE_NUM);
	spin_unlock(&dlm->spinlock);
	return ready;
}

/* returns true if node is no longer in the domain
 * could be dead or just not joined */
int dlm_is_node_dead(struct dlm_ctxt *dlm, u8 node)
{
	int dead;
	spin_lock(&dlm->spinlock);
	dead = test_bit(node, dlm->domain_map);
	spin_unlock(&dlm->spinlock);
	return dead;
}

int dlm_wait_for_node_death(struct dlm_ctxt *dlm, u8 node, int timeout)
{
	if (timeout) {
		mlog(ML_NOTICE, "%s: waiting %dms for notification of "
		     "death of node %u\n", dlm->name, timeout, node);
		wait_event_timeout(dlm->dlm_reco_thread_wq,
			   dlm_is_node_dead(dlm, node),
			   msecs_to_jiffies(timeout));
	} else {
		mlog(ML_NOTICE, "%s: waiting indefinitely for notification "
		     "of death of node %u\n", dlm->name, node);
		wait_event(dlm->dlm_reco_thread_wq,
			   dlm_is_node_dead(dlm, node));
	}
	/* for now, return 0 */
	return 0;
}

/* callers of the top-level api calls (dlmlock/dlmunlock) should
 * block on the dlm->reco.event when recovery is in progress.
 * the dlm recovery thread will set this state when it begins
 * recovering a dead node (as the new master or not) and clear
 * the state and wake as soon as all affected lock resources have
 * been marked with the RECOVERY flag */
static int dlm_in_recovery(struct dlm_ctxt *dlm)
{
	int in_recovery;
	spin_lock(&dlm->spinlock);
	in_recovery = !!(dlm->reco.state & DLM_RECO_STATE_ACTIVE);
	spin_unlock(&dlm->spinlock);
	return in_recovery;
}


void dlm_wait_for_recovery(struct dlm_ctxt *dlm)
{
	wait_event(dlm->reco.event, !dlm_in_recovery(dlm));
}

static void dlm_begin_recovery(struct dlm_ctxt *dlm)
{
	spin_lock(&dlm->spinlock);
	BUG_ON(dlm->reco.state & DLM_RECO_STATE_ACTIVE);
	dlm->reco.state |= DLM_RECO_STATE_ACTIVE;
	spin_unlock(&dlm->spinlock);
}

static void dlm_end_recovery(struct dlm_ctxt *dlm)
{
	spin_lock(&dlm->spinlock);
	BUG_ON(!(dlm->reco.state & DLM_RECO_STATE_ACTIVE));
	dlm->reco.state &= ~DLM_RECO_STATE_ACTIVE;
	spin_unlock(&dlm->spinlock);
	wake_up(&dlm->reco.event);
}

static int dlm_do_recovery(struct dlm_ctxt *dlm)
{
	int status = 0;
	int ret;

	spin_lock(&dlm->spinlock);

	/* check to see if the new master has died */
	if (dlm->reco.new_master != O2NM_INVALID_NODE_NUM &&
	    test_bit(dlm->reco.new_master, dlm->recovery_map)) {
		mlog(0, "new master %u died while recovering %u!\n",
		     dlm->reco.new_master, dlm->reco.dead_node);
		/* unset the new_master, leave dead_node */
		dlm->reco.new_master = O2NM_INVALID_NODE_NUM;
	}

	/* select a target to recover */
	if (dlm->reco.dead_node == O2NM_INVALID_NODE_NUM) {
		int bit;

		bit = find_next_bit (dlm->recovery_map, O2NM_MAX_NODES+1, 0);
		if (bit >= O2NM_MAX_NODES || bit < 0)
			dlm->reco.dead_node = O2NM_INVALID_NODE_NUM;
		else
			dlm->reco.dead_node = bit;
	} else if (!test_bit(dlm->reco.dead_node, dlm->recovery_map)) {
		/* BUG? */
		mlog(ML_ERROR, "dead_node %u no longer in recovery map!\n",
		     dlm->reco.dead_node);
		dlm->reco.dead_node = O2NM_INVALID_NODE_NUM;
	}

	if (dlm->reco.dead_node == O2NM_INVALID_NODE_NUM) {
		// mlog(0, "nothing to recover!  sleeping now!\n");
		spin_unlock(&dlm->spinlock);
		/* return to main thread loop and sleep. */
		return 0;
	}
	mlog(0, "recovery thread found node %u in the recovery map!\n",
	     dlm->reco.dead_node);
	spin_unlock(&dlm->spinlock);

	/* take write barrier */
	/* (stops the list reshuffling thread, proxy ast handling) */
	dlm_begin_recovery(dlm);

	if (dlm->reco.new_master == dlm->node_num)
		goto master_here;

	if (dlm->reco.new_master == O2NM_INVALID_NODE_NUM) {
		/* choose a new master, returns 0 if this node
		 * is the master, -EEXIST if it's another node.
		 * this does not return until a new master is chosen
		 * or recovery completes entirely. */
		ret = dlm_pick_recovery_master(dlm);
		if (!ret) {
			/* already notified everyone.  go. */
			goto master_here;
		}
		mlog(0, "another node will master this recovery session.\n");
	}
	mlog(0, "dlm=%s, new_master=%u, this node=%u, dead_node=%u\n",
	     dlm->name, dlm->reco.new_master,
	     dlm->node_num, dlm->reco.dead_node);

	/* it is safe to start everything back up here
	 * because all of the dead node's lock resources
	 * have been marked as in-recovery */
	dlm_end_recovery(dlm);

	/* sleep out in main dlm_recovery_thread loop. */
	return 0;

master_here:
	mlog(0, "mastering recovery of %s:%u here(this=%u)!\n",
	     dlm->name, dlm->reco.dead_node, dlm->node_num);

	status = dlm_remaster_locks(dlm, dlm->reco.dead_node);
	if (status < 0) {
		mlog(ML_ERROR, "error %d remastering locks for node %u, "
		     "retrying.\n", status, dlm->reco.dead_node);
		/* yield a bit to allow any final network messages
		 * to get handled on remaining nodes */
		msleep(100);
	} else {
		/* success!  see if any other nodes need recovery */
		mlog(0, "DONE mastering recovery of %s:%u here(this=%u)!\n",
		     dlm->name, dlm->reco.dead_node, dlm->node_num);
		dlm_reset_recovery(dlm);
	}
	dlm_end_recovery(dlm);

	/* continue and look for another dead node */
	return -EAGAIN;
}

static int dlm_remaster_locks(struct dlm_ctxt *dlm, u8 dead_node)
{
	int status = 0;
	struct dlm_reco_node_data *ndata;
	struct list_head *iter;
	int all_nodes_done;
	int destroy = 0;
	int pass = 0;

	status = dlm_init_recovery_area(dlm, dead_node);
	if (status < 0)
		goto leave;

	/* safe to access the node data list without a lock, since this
	 * process is the only one to change the list */
	list_for_each(iter, &dlm->reco.node_data) {
		ndata = list_entry (iter, struct dlm_reco_node_data, list);
		BUG_ON(ndata->state != DLM_RECO_NODE_DATA_INIT);
		ndata->state = DLM_RECO_NODE_DATA_REQUESTING;

		mlog(0, "requesting lock info from node %u\n",
		     ndata->node_num);

		if (ndata->node_num == dlm->node_num) {
			ndata->state = DLM_RECO_NODE_DATA_DONE;
			continue;
		}

		status = dlm_request_all_locks(dlm, ndata->node_num, dead_node);
		if (status < 0) {
			mlog_errno(status);
			if (dlm_is_host_down(status))
				ndata->state = DLM_RECO_NODE_DATA_DEAD;
			else {
				destroy = 1;
				goto leave;
			}
		}

		switch (ndata->state) {
			case DLM_RECO_NODE_DATA_INIT:
			case DLM_RECO_NODE_DATA_FINALIZE_SENT:
			case DLM_RECO_NODE_DATA_REQUESTED:
				BUG();
				break;
			case DLM_RECO_NODE_DATA_DEAD:
				mlog(0, "node %u died after requesting "
				     "recovery info for node %u\n",
				     ndata->node_num, dead_node);
				// start all over
				destroy = 1;
				status = -EAGAIN;
				goto leave;
			case DLM_RECO_NODE_DATA_REQUESTING:
				ndata->state = DLM_RECO_NODE_DATA_REQUESTED;
				mlog(0, "now receiving recovery data from "
				     "node %u for dead node %u\n",
				     ndata->node_num, dead_node);
				break;
			case DLM_RECO_NODE_DATA_RECEIVING:
				mlog(0, "already receiving recovery data from "
				     "node %u for dead node %u\n",
				     ndata->node_num, dead_node);
				break;
			case DLM_RECO_NODE_DATA_DONE:
				mlog(0, "already DONE receiving recovery data "
				     "from node %u for dead node %u\n",
				     ndata->node_num, dead_node);
				break;
		}
	}

	mlog(0, "done requesting all lock info\n");

	/* nodes should be sending reco data now
	 * just need to wait */

	while (1) {
		/* check all the nodes now to see if we are
		 * done, or if anyone died */
		all_nodes_done = 1;
		spin_lock(&dlm_reco_state_lock);
		list_for_each(iter, &dlm->reco.node_data) {
			ndata = list_entry (iter, struct dlm_reco_node_data, list);

			mlog(0, "checking recovery state of node %u\n",
			     ndata->node_num);
			switch (ndata->state) {
				case DLM_RECO_NODE_DATA_INIT:
				case DLM_RECO_NODE_DATA_REQUESTING:
					mlog(ML_ERROR, "bad ndata state for "
					     "node %u: state=%d\n",
					     ndata->node_num, ndata->state);
					BUG();
					break;
				case DLM_RECO_NODE_DATA_DEAD:
					mlog(ML_NOTICE, "node %u died after "
					     "requesting recovery info for "
					     "node %u\n", ndata->node_num,
					     dead_node);
					spin_unlock(&dlm_reco_state_lock);
					// start all over
					destroy = 1;
					status = -EAGAIN;
					/* instead of spinning like crazy here,
					 * wait for the domain map to catch up
					 * with the network state.  otherwise this
					 * can be hit hundreds of times before
					 * the node is really seen as dead. */
					wait_event_timeout(dlm->dlm_reco_thread_wq,
							   dlm_is_node_dead(dlm,
								ndata->node_num),
							   msecs_to_jiffies(1000));
					mlog(0, "waited 1 sec for %u, "
					     "dead? %s\n", ndata->node_num,
					     dlm_is_node_dead(dlm, ndata->node_num) ?
					     "yes" : "no");
					goto leave;
				case DLM_RECO_NODE_DATA_RECEIVING:
				case DLM_RECO_NODE_DATA_REQUESTED:
					all_nodes_done = 0;
					break;
				case DLM_RECO_NODE_DATA_DONE:
					break;
				case DLM_RECO_NODE_DATA_FINALIZE_SENT:
					break;
			}
		}
		spin_unlock(&dlm_reco_state_lock);

		mlog(0, "pass #%d, all_nodes_done?: %s\n", ++pass,
		     all_nodes_done?"yes":"no");
		if (all_nodes_done) {
			int ret;

			/* all nodes are now in DLM_RECO_NODE_DATA_DONE state
	 		 * just send a finalize message to everyone and
	 		 * clean up */
			mlog(0, "all nodes are done! send finalize\n");
			ret = dlm_send_finalize_reco_message(dlm);
			if (ret < 0)
				mlog_errno(ret);

			spin_lock(&dlm->spinlock);
			dlm_finish_local_lockres_recovery(dlm, dead_node,
							  dlm->node_num);
			spin_unlock(&dlm->spinlock);
			mlog(0, "should be done with recovery!\n");

			mlog(0, "finishing recovery of %s at %lu, "
			     "dead=%u, this=%u, new=%u\n", dlm->name,
			     jiffies, dlm->reco.dead_node,
			     dlm->node_num, dlm->reco.new_master);
			destroy = 1;
			status = ret;
			/* rescan everything marked dirty along the way */
			dlm_kick_thread(dlm, NULL);
			break;
		}
		/* wait to be signalled, with periodic timeout
		 * to check for node death */
		wait_event_interruptible_timeout(dlm->dlm_reco_thread_wq,
					 kthread_should_stop(),
					 msecs_to_jiffies(DLM_RECO_THREAD_TIMEOUT_MS));

	}

leave:
	if (destroy)
		dlm_destroy_recovery_area(dlm, dead_node);

	mlog_exit(status);
	return status;
}

static int dlm_init_recovery_area(struct dlm_ctxt *dlm, u8 dead_node)
{
	int num=0;
	struct dlm_reco_node_data *ndata;

	spin_lock(&dlm->spinlock);
	memcpy(dlm->reco.node_map, dlm->domain_map, sizeof(dlm->domain_map));
	/* nodes can only be removed (by dying) after dropping
	 * this lock, and death will be trapped later, so this should do */
	spin_unlock(&dlm->spinlock);

	while (1) {
		num = find_next_bit (dlm->reco.node_map, O2NM_MAX_NODES, num);
		if (num >= O2NM_MAX_NODES) {
			break;
		}
		BUG_ON(num == dead_node);

		ndata = kcalloc(1, sizeof(*ndata), GFP_KERNEL);
		if (!ndata) {
			dlm_destroy_recovery_area(dlm, dead_node);
			return -ENOMEM;
		}
		ndata->node_num = num;
		ndata->state = DLM_RECO_NODE_DATA_INIT;
		spin_lock(&dlm_reco_state_lock);
		list_add_tail(&ndata->list, &dlm->reco.node_data);
		spin_unlock(&dlm_reco_state_lock);
		num++;
	}

	return 0;
}

static void dlm_destroy_recovery_area(struct dlm_ctxt *dlm, u8 dead_node)
{
	struct list_head *iter, *iter2;
	struct dlm_reco_node_data *ndata;
	LIST_HEAD(tmplist);

	spin_lock(&dlm_reco_state_lock);
	list_splice_init(&dlm->reco.node_data, &tmplist);
	spin_unlock(&dlm_reco_state_lock);

	list_for_each_safe(iter, iter2, &tmplist) {
		ndata = list_entry (iter, struct dlm_reco_node_data, list);
		list_del_init(&ndata->list);
		kfree(ndata);
	}
}

static int dlm_request_all_locks(struct dlm_ctxt *dlm, u8 request_from,
				 u8 dead_node)
{
	struct dlm_lock_request lr;
	enum dlm_status ret;

	mlog(0, "\n");


	mlog(0, "dlm_request_all_locks: dead node is %u, sending request "
		  "to %u\n", dead_node, request_from);

	memset(&lr, 0, sizeof(lr));
	lr.node_idx = dlm->node_num;
	lr.dead_node = dead_node;

	// send message
	ret = DLM_NOLOCKMGR;
	ret = o2net_send_message(DLM_LOCK_REQUEST_MSG, dlm->key,
				 &lr, sizeof(lr), request_from, NULL);

	/* negative status is handled by caller */
	if (ret < 0)
		mlog_errno(ret);

	// return from here, then
	// sleep until all received or error
	return ret;

}

int dlm_request_all_locks_handler(struct o2net_msg *msg, u32 len, void *data)
{
	struct dlm_ctxt *dlm = data;
	struct dlm_lock_request *lr = (struct dlm_lock_request *)msg->buf;
	char *buf = NULL;
	struct dlm_work_item *item = NULL;

	if (!dlm_grab(dlm))
		return -EINVAL;

	BUG_ON(lr->dead_node != dlm->reco.dead_node);

	item = kcalloc(1, sizeof(*item), GFP_KERNEL);
	if (!item) {
		dlm_put(dlm);
		return -ENOMEM;
	}

	/* this will get freed by dlm_request_all_locks_worker */
	buf = (char *) __get_free_page(GFP_KERNEL);
	if (!buf) {
		kfree(item);
		dlm_put(dlm);
		return -ENOMEM;
	}

	/* queue up work for dlm_request_all_locks_worker */
	dlm_grab(dlm);  /* get an extra ref for the work item */
	dlm_init_work_item(dlm, item, dlm_request_all_locks_worker, buf);
	item->u.ral.reco_master = lr->node_idx;
	item->u.ral.dead_node = lr->dead_node;
	spin_lock(&dlm->work_lock);
	list_add_tail(&item->list, &dlm->work_list);
	spin_unlock(&dlm->work_lock);
	schedule_work(&dlm->dispatched_work);

	dlm_put(dlm);
	return 0;
}

static void dlm_request_all_locks_worker(struct dlm_work_item *item, void *data)
{
	struct dlm_migratable_lockres *mres;
	struct dlm_lock_resource *res;
	struct dlm_ctxt *dlm;
	LIST_HEAD(resources);
	struct list_head *iter;
	int ret;
	u8 dead_node, reco_master;

	dlm = item->dlm;
	dead_node = item->u.ral.dead_node;
	reco_master = item->u.ral.reco_master;
	mres = (struct dlm_migratable_lockres *)data;

	if (dead_node != dlm->reco.dead_node ||
	    reco_master != dlm->reco.new_master) {
		/* show extra debug info if the recovery state is messed */
		mlog(ML_ERROR, "%s: bad reco state: reco(dead=%u, master=%u), "
		     "request(dead=%u, master=%u)\n",
		     dlm->name, dlm->reco.dead_node, dlm->reco.new_master,
		     dead_node, reco_master);
		mlog(ML_ERROR, "%s: name=%.*s master=%u locks=%u/%u flags=%u "
		     "entry[0]={c=%"MLFu64",l=%u,f=%u,t=%d,ct=%d,hb=%d,n=%u}\n",
		     dlm->name, mres->lockname_len, mres->lockname, mres->master,
		     mres->num_locks, mres->total_locks, mres->flags,
		     mres->ml[0].cookie, mres->ml[0].list, mres->ml[0].flags,
		     mres->ml[0].type, mres->ml[0].convert_type,
		     mres->ml[0].highest_blocked, mres->ml[0].node);
		BUG();
	}
	BUG_ON(dead_node != dlm->reco.dead_node);
	BUG_ON(reco_master != dlm->reco.new_master);

	/* lock resources should have already been moved to the
 	 * dlm->reco.resources list.  now move items from that list
 	 * to a temp list if the dead owner matches.  note that the
	 * whole cluster recovers only one node at a time, so we
	 * can safely move UNKNOWN lock resources for each recovery
	 * session. */
	dlm_move_reco_locks_to_list(dlm, &resources, dead_node);

	/* now we can begin blasting lockreses without the dlm lock */
	list_for_each(iter, &resources) {
		res = list_entry (iter, struct dlm_lock_resource, recovering);
		ret = dlm_send_one_lockres(dlm, res, mres, reco_master,
				   	DLM_MRES_RECOVERY);
		if (ret < 0)
			mlog_errno(ret);
	}

	/* move the resources back to the list */
	spin_lock(&dlm->spinlock);
	list_splice_init(&resources, &dlm->reco.resources);
	spin_unlock(&dlm->spinlock);

	ret = dlm_send_all_done_msg(dlm, dead_node, reco_master);
	if (ret < 0)
		mlog_errno(ret);

	free_page((unsigned long)data);
}


static int dlm_send_all_done_msg(struct dlm_ctxt *dlm, u8 dead_node, u8 send_to)
{
	int ret, tmpret;
	struct dlm_reco_data_done done_msg;

	memset(&done_msg, 0, sizeof(done_msg));
	done_msg.node_idx = dlm->node_num;
	done_msg.dead_node = dead_node;
	mlog(0, "sending DATA DONE message to %u, "
	     "my node=%u, dead node=%u\n", send_to, done_msg.node_idx,
	     done_msg.dead_node);

	ret = o2net_send_message(DLM_RECO_DATA_DONE_MSG, dlm->key, &done_msg,
				 sizeof(done_msg), send_to, &tmpret);
	/* negative status is ignored by the caller */
	if (ret >= 0)
		ret = tmpret;
	return ret;
}


int dlm_reco_data_done_handler(struct o2net_msg *msg, u32 len, void *data)
{
	struct dlm_ctxt *dlm = data;
	struct dlm_reco_data_done *done = (struct dlm_reco_data_done *)msg->buf;
	struct list_head *iter;
	struct dlm_reco_node_data *ndata = NULL;
	int ret = -EINVAL;

	if (!dlm_grab(dlm))
		return -EINVAL;

	mlog(0, "got DATA DONE: dead_node=%u, reco.dead_node=%u, "
	     "node_idx=%u, this node=%u\n", done->dead_node,
	     dlm->reco.dead_node, done->node_idx, dlm->node_num);
	BUG_ON(done->dead_node != dlm->reco.dead_node);

	spin_lock(&dlm_reco_state_lock);
	list_for_each(iter, &dlm->reco.node_data) {
		ndata = list_entry (iter, struct dlm_reco_node_data, list);
		if (ndata->node_num != done->node_idx)
			continue;

		switch (ndata->state) {
			/* should have moved beyond INIT but not to FINALIZE yet */
			case DLM_RECO_NODE_DATA_INIT:
			case DLM_RECO_NODE_DATA_DEAD:
			case DLM_RECO_NODE_DATA_FINALIZE_SENT:
				mlog(ML_ERROR, "bad ndata state for node %u:"
				     " state=%d\n", ndata->node_num,
				     ndata->state);
				BUG();
				break;
			/* these states are possible at this point, anywhere along
			 * the line of recovery */
			case DLM_RECO_NODE_DATA_DONE:
			case DLM_RECO_NODE_DATA_RECEIVING:
			case DLM_RECO_NODE_DATA_REQUESTED:
			case DLM_RECO_NODE_DATA_REQUESTING:
				mlog(0, "node %u is DONE sending "
					  "recovery data!\n",
					  ndata->node_num);

				ndata->state = DLM_RECO_NODE_DATA_DONE;
				ret = 0;
				break;
		}
	}
	spin_unlock(&dlm_reco_state_lock);

	/* wake the recovery thread, some node is done */
	if (!ret)
		dlm_kick_recovery_thread(dlm);

	if (ret < 0)
		mlog(ML_ERROR, "failed to find recovery node data for node "
		     "%u\n", done->node_idx);
	dlm_put(dlm);

	mlog(0, "leaving reco data done handler, ret=%d\n", ret);
	return ret;
}

static void dlm_move_reco_locks_to_list(struct dlm_ctxt *dlm,
					struct list_head *list,
				       	u8 dead_node)
{
	struct dlm_lock_resource *res;
	struct list_head *iter, *iter2;
	struct dlm_lock *lock;

	spin_lock(&dlm->spinlock);
	list_for_each_safe(iter, iter2, &dlm->reco.resources) {
		res = list_entry (iter, struct dlm_lock_resource, recovering);
		/* always prune any $RECOVERY entries for dead nodes,
		 * otherwise hangs can occur during later recovery */
		if (dlm_is_recovery_lock(res->lockname.name,
					 res->lockname.len)) {
			spin_lock(&res->spinlock);
			list_for_each_entry(lock, &res->granted, list) {
				if (lock->ml.node == dead_node) {
					mlog(0, "AHA! there was "
					     "a $RECOVERY lock for dead "
					     "node %u (%s)!\n", 
					     dead_node, dlm->name);
					list_del_init(&lock->list);
					dlm_lock_put(lock);
					break;
				}
			}
			spin_unlock(&res->spinlock);
			continue;
		}

		if (res->owner == dead_node) {
			mlog(0, "found lockres owned by dead node while "
				  "doing recovery for node %u. sending it.\n",
				  dead_node);
			list_del_init(&res->recovering);
			list_add_tail(&res->recovering, list);
		} else if (res->owner == DLM_LOCK_RES_OWNER_UNKNOWN) {
			mlog(0, "found UNKNOWN owner while doing recovery "
				  "for node %u. sending it.\n", dead_node);
			list_del_init(&res->recovering);
			list_add_tail(&res->recovering, list);
		}
	}
	spin_unlock(&dlm->spinlock);
}

static inline int dlm_num_locks_in_lockres(struct dlm_lock_resource *res)
{
	int total_locks = 0;
	struct list_head *iter, *queue = &res->granted;
	int i;

	for (i=0; i<3; i++) {
		list_for_each(iter, queue)
			total_locks++;
		queue++;
	}
	return total_locks;
}


static int dlm_send_mig_lockres_msg(struct dlm_ctxt *dlm,
				      struct dlm_migratable_lockres *mres,
				      u8 send_to,
				      struct dlm_lock_resource *res,
				      int total_locks)
{
	u64 mig_cookie = be64_to_cpu(mres->mig_cookie);
	int mres_total_locks = be32_to_cpu(mres->total_locks);
	int sz, ret = 0, status = 0;
	u8 orig_flags = mres->flags,
	   orig_master = mres->master;

	BUG_ON(mres->num_locks > DLM_MAX_MIGRATABLE_LOCKS);
	if (!mres->num_locks)
		return 0;

	sz = sizeof(struct dlm_migratable_lockres) +
		(mres->num_locks * sizeof(struct dlm_migratable_lock));

	/* add an all-done flag if we reached the last lock */
	orig_flags = mres->flags;
	BUG_ON(total_locks > mres_total_locks);
	if (total_locks == mres_total_locks)
		mres->flags |= DLM_MRES_ALL_DONE;

	/* send it */
	ret = o2net_send_message(DLM_MIG_LOCKRES_MSG, dlm->key, mres,
				 sz, send_to, &status);
	if (ret < 0) {
		/* XXX: negative status is not handled.
		 * this will end up killing this node. */
		mlog_errno(ret);
	} else {
		/* might get an -ENOMEM back here */
		ret = status;
		if (ret < 0) {
			mlog_errno(ret);

			if (ret == -EFAULT) {
				mlog(ML_ERROR, "node %u told me to kill "
				     "myself!\n", send_to);
				BUG();
			}
		}
	}

	/* zero and reinit the message buffer */
	dlm_init_migratable_lockres(mres, res->lockname.name,
				    res->lockname.len, mres_total_locks,
				    mig_cookie, orig_flags, orig_master);
	return ret;
}

static void dlm_init_migratable_lockres(struct dlm_migratable_lockres *mres,
					const char *lockname, int namelen,
					int total_locks, u64 cookie,
					u8 flags, u8 master)
{
	/* mres here is one full page */
	memset(mres, 0, PAGE_SIZE);
	mres->lockname_len = namelen;
	memcpy(mres->lockname, lockname, namelen);
	mres->num_locks = 0;
	mres->total_locks = cpu_to_be32(total_locks);
	mres->mig_cookie = cpu_to_be64(cookie);
	mres->flags = flags;
	mres->master = master;
}


/* returns 1 if this lock fills the network structure,
 * 0 otherwise */
static int dlm_add_lock_to_array(struct dlm_lock *lock,
				 struct dlm_migratable_lockres *mres, int queue)
{
	struct dlm_migratable_lock *ml;
	int lock_num = mres->num_locks;

	ml = &(mres->ml[lock_num]);
	ml->cookie = lock->ml.cookie;
	ml->type = lock->ml.type;
	ml->convert_type = lock->ml.convert_type;
	ml->highest_blocked = lock->ml.highest_blocked;
	ml->list = queue;
	if (lock->lksb) {
		ml->flags = lock->lksb->flags;
		/* send our current lvb */
		if (ml->type == LKM_EXMODE ||
		    ml->type == LKM_PRMODE) {
			/* if it is already set, this had better be a PR
			 * and it has to match */
			if (mres->lvb[0] && (ml->type == LKM_EXMODE ||
			    memcmp(mres->lvb, lock->lksb->lvb, DLM_LVB_LEN))) {
				mlog(ML_ERROR, "mismatched lvbs!\n");
				__dlm_print_one_lock_resource(lock->lockres);
				BUG();
			}
			memcpy(mres->lvb, lock->lksb->lvb, DLM_LVB_LEN);
		}
	}
	ml->node = lock->ml.node;
	mres->num_locks++;
	/* we reached the max, send this network message */
	if (mres->num_locks == DLM_MAX_MIGRATABLE_LOCKS)
		return 1;
	return 0;
}


int dlm_send_one_lockres(struct dlm_ctxt *dlm, struct dlm_lock_resource *res,
			 struct dlm_migratable_lockres *mres,
			 u8 send_to, u8 flags)
{
	struct list_head *queue, *iter;
	int total_locks, i;
	u64 mig_cookie = 0;
	struct dlm_lock *lock;
	int ret = 0;

	BUG_ON(!(flags & (DLM_MRES_RECOVERY|DLM_MRES_MIGRATION)));

	mlog(0, "sending to %u\n", send_to);

	total_locks = dlm_num_locks_in_lockres(res);
	if (total_locks > DLM_MAX_MIGRATABLE_LOCKS) {
		/* rare, but possible */
		mlog(0, "argh.  lockres has %d locks.  this will "
			  "require more than one network packet to "
			  "migrate\n", total_locks);
		mig_cookie = dlm_get_next_mig_cookie();
	}

	dlm_init_migratable_lockres(mres, res->lockname.name,
				    res->lockname.len, total_locks,
				    mig_cookie, flags, res->owner);

	total_locks = 0;
	for (i=DLM_GRANTED_LIST; i<=DLM_BLOCKED_LIST; i++) {
		queue = dlm_list_idx_to_ptr(res, i);
		list_for_each(iter, queue) {
			lock = list_entry (iter, struct dlm_lock, list);

			/* add another lock. */
			total_locks++;
			if (!dlm_add_lock_to_array(lock, mres, i))
				continue;

			/* this filled the lock message,
			 * we must send it immediately. */
			ret = dlm_send_mig_lockres_msg(dlm, mres, send_to,
						       res, total_locks);
			if (ret < 0) {
				// TODO
				mlog(ML_ERROR, "dlm_send_mig_lockres_msg "
				     "returned %d, TODO\n", ret);
				BUG();
			}
		}
	}
	/* flush any remaining locks */
	ret = dlm_send_mig_lockres_msg(dlm, mres, send_to, res, total_locks);
	if (ret < 0) {
		// TODO
		mlog(ML_ERROR, "dlm_send_mig_lockres_msg returned %d, "
		     "TODO\n", ret);
		BUG();
	}
	return ret;
}



/*
 * this message will contain no more than one page worth of
 * recovery data, and it will work on only one lockres.
 * there may be many locks in this page, and we may need to wait
 * for additional packets to complete all the locks (rare, but
 * possible).
 */
/*
 * NOTE: the allocation error cases here are scary
 * we really cannot afford to fail an alloc in recovery
 * do we spin?  returning an error only delays the problem really
 */

int dlm_mig_lockres_handler(struct o2net_msg *msg, u32 len, void *data)
{
	struct dlm_ctxt *dlm = data;
	struct dlm_migratable_lockres *mres =
		(struct dlm_migratable_lockres *)msg->buf;
	int ret = 0;
	u8 real_master;
	char *buf = NULL;
	struct dlm_work_item *item = NULL;
	struct dlm_lock_resource *res = NULL;

	if (!dlm_grab(dlm))
		return -EINVAL;

	BUG_ON(!(mres->flags & (DLM_MRES_RECOVERY|DLM_MRES_MIGRATION)));

	real_master = mres->master;
	if (real_master == DLM_LOCK_RES_OWNER_UNKNOWN) {
		/* cannot migrate a lockres with no master */
		BUG_ON(!(mres->flags & DLM_MRES_RECOVERY));
	}

	mlog(0, "%s message received from node %u\n",
		  (mres->flags & DLM_MRES_RECOVERY) ?
		  "recovery" : "migration", mres->master);
	if (mres->flags & DLM_MRES_ALL_DONE)
		mlog(0, "all done flag.  all lockres data received!\n");

	ret = -ENOMEM;
	buf = kmalloc(be16_to_cpu(msg->data_len), GFP_KERNEL);
	item = kcalloc(1, sizeof(*item), GFP_KERNEL);
	if (!buf || !item)
		goto leave;

	/* lookup the lock to see if we have a secondary queue for this
	 * already...  just add the locks in and this will have its owner
	 * and RECOVERY flag changed when it completes. */
	res = dlm_lookup_lockres(dlm, mres->lockname, mres->lockname_len);
	if (res) {
	 	/* this will get a ref on res */
		/* mark it as recovering/migrating and hash it */
		spin_lock(&res->spinlock);
		if (mres->flags & DLM_MRES_RECOVERY) {
			res->state |= DLM_LOCK_RES_RECOVERING;
		} else {
			if (res->state & DLM_LOCK_RES_MIGRATING) {
				/* this is at least the second
				 * lockres message */
				mlog(0, "lock %.*s is already migrating\n",
					  mres->lockname_len,
					  mres->lockname);
			} else if (res->state & DLM_LOCK_RES_RECOVERING) {
				/* caller should BUG */
				mlog(ML_ERROR, "node is attempting to migrate "
				     "lock %.*s, but marked as recovering!\n",
				     mres->lockname_len, mres->lockname);
				ret = -EFAULT;
				spin_unlock(&res->spinlock);
				goto leave;
			}
			res->state |= DLM_LOCK_RES_MIGRATING;
		}
		spin_unlock(&res->spinlock);
	} else {
		/* need to allocate, just like if it was
		 * mastered here normally  */
		res = dlm_new_lockres(dlm, mres->lockname, mres->lockname_len);
		if (!res)
			goto leave;

		/* to match the ref that we would have gotten if
		 * dlm_lookup_lockres had succeeded */
		dlm_lockres_get(res);

		/* mark it as recovering/migrating and hash it */
		if (mres->flags & DLM_MRES_RECOVERY)
			res->state |= DLM_LOCK_RES_RECOVERING;
		else
			res->state |= DLM_LOCK_RES_MIGRATING;

		spin_lock(&dlm->spinlock);
		__dlm_insert_lockres(dlm, res);
		spin_unlock(&dlm->spinlock);

		/* now that the new lockres is inserted,
		 * make it usable by other processes */
		spin_lock(&res->spinlock);
		res->state &= ~DLM_LOCK_RES_IN_PROGRESS;
		spin_unlock(&res->spinlock);

		/* add an extra ref for just-allocated lockres 
		 * otherwise the lockres will be purged immediately */
		dlm_lockres_get(res);

	}

	/* at this point we have allocated everything we need,
	 * and we have a hashed lockres with an extra ref and
	 * the proper res->state flags. */
	ret = 0;
	if (mres->master == DLM_LOCK_RES_OWNER_UNKNOWN) {
		/* migration cannot have an unknown master */
		BUG_ON(!(mres->flags & DLM_MRES_RECOVERY));
		mlog(0, "recovery has passed me a lockres with an "
			  "unknown owner.. will need to requery: "
			  "%.*s\n", mres->lockname_len, mres->lockname);
	} else {
		spin_lock(&res->spinlock);
		dlm_change_lockres_owner(dlm, res, dlm->node_num);
		spin_unlock(&res->spinlock);
	}

	/* queue up work for dlm_mig_lockres_worker */
	dlm_grab(dlm);  /* get an extra ref for the work item */
	memcpy(buf, msg->buf, be16_to_cpu(msg->data_len));  /* copy the whole message */
	dlm_init_work_item(dlm, item, dlm_mig_lockres_worker, buf);
	item->u.ml.lockres = res; /* already have a ref */
	item->u.ml.real_master = real_master;
	spin_lock(&dlm->work_lock);
	list_add_tail(&item->list, &dlm->work_list);
	spin_unlock(&dlm->work_lock);
	schedule_work(&dlm->dispatched_work);

leave:
	dlm_put(dlm);
	if (ret < 0) {
		if (buf)
			kfree(buf);
		if (item)
			kfree(item);
	}

	mlog_exit(ret);
	return ret;
}


static void dlm_mig_lockres_worker(struct dlm_work_item *item, void *data)
{
	struct dlm_ctxt *dlm = data;
	struct dlm_migratable_lockres *mres;
	int ret = 0;
	struct dlm_lock_resource *res;
	u8 real_master;

	dlm = item->dlm;
	mres = (struct dlm_migratable_lockres *)data;

	res = item->u.ml.lockres;
	real_master = item->u.ml.real_master;

	if (real_master == DLM_LOCK_RES_OWNER_UNKNOWN) {
		/* this case is super-rare. only occurs if
		 * node death happens during migration. */
again:
		ret = dlm_lockres_master_requery(dlm, res, &real_master);
		if (ret < 0) {
			mlog(0, "dlm_lockres_master_requery ret=%d\n",
				  ret);
			goto again;
		}
		if (real_master == DLM_LOCK_RES_OWNER_UNKNOWN) {
			mlog(0, "lockres %.*s not claimed.  "
				   "this node will take it.\n",
				   res->lockname.len, res->lockname.name);
		} else {
			mlog(0, "master needs to respond to sender "
				  "that node %u still owns %.*s\n",
				  real_master, res->lockname.len,
				  res->lockname.name);
			/* cannot touch this lockres */
			goto leave;
		}
	}

	ret = dlm_process_recovery_data(dlm, res, mres);
	if (ret < 0)
		mlog(0, "dlm_process_recovery_data returned  %d\n", ret);
	else
		mlog(0, "dlm_process_recovery_data succeeded\n");

	if ((mres->flags & (DLM_MRES_MIGRATION|DLM_MRES_ALL_DONE)) ==
	                   (DLM_MRES_MIGRATION|DLM_MRES_ALL_DONE)) {
		ret = dlm_finish_migration(dlm, res, mres->master);
		if (ret < 0)
			mlog_errno(ret);
	}

leave:
	kfree(data);
	mlog_exit(ret);
}



static int dlm_lockres_master_requery(struct dlm_ctxt *dlm,
				      struct dlm_lock_resource *res,
				      u8 *real_master)
{
	struct dlm_node_iter iter;
	int nodenum;
	int ret = 0;

	*real_master = DLM_LOCK_RES_OWNER_UNKNOWN;

	/* we only reach here if one of the two nodes in a
	 * migration died while the migration was in progress.
	 * at this point we need to requery the master.  we
	 * know that the new_master got as far as creating
	 * an mle on at least one node, but we do not know
	 * if any nodes had actually cleared the mle and set
	 * the master to the new_master.  the old master
	 * is supposed to set the owner to UNKNOWN in the
	 * event of a new_master death, so the only possible
	 * responses that we can get from nodes here are
	 * that the master is new_master, or that the master
	 * is UNKNOWN.
	 * if all nodes come back with UNKNOWN then we know
	 * the lock needs remastering here.
	 * if any node comes back with a valid master, check
	 * to see if that master is the one that we are
	 * recovering.  if so, then the new_master died and
	 * we need to remaster this lock.  if not, then the
	 * new_master survived and that node will respond to
	 * other nodes about the owner.
	 * if there is an owner, this node needs to dump this
	 * lockres and alert the sender that this lockres
	 * was rejected. */
	spin_lock(&dlm->spinlock);
	dlm_node_iter_init(dlm->domain_map, &iter);
	spin_unlock(&dlm->spinlock);

	while ((nodenum = dlm_node_iter_next(&iter)) >= 0) {
		/* do not send to self */
		if (nodenum == dlm->node_num)
			continue;
		ret = dlm_do_master_requery(dlm, res, nodenum, real_master);
		if (ret < 0) {
			mlog_errno(ret);
			BUG();
			/* TODO: need to figure a way to restart this */
		}
		if (*real_master != DLM_LOCK_RES_OWNER_UNKNOWN) {
			mlog(0, "lock master is %u\n", *real_master);
			break;
		}
	}
	return ret;
}


static int dlm_do_master_requery(struct dlm_ctxt *dlm,
				 struct dlm_lock_resource *res,
				 u8 nodenum, u8 *real_master)
{
	int ret = -EINVAL;
	struct dlm_master_requery req;
	int status = DLM_LOCK_RES_OWNER_UNKNOWN;

	memset(&req, 0, sizeof(req));
	req.node_idx = dlm->node_num;
	req.namelen = res->lockname.len;
	memcpy(req.name, res->lockname.name, res->lockname.len);

	ret = o2net_send_message(DLM_MASTER_REQUERY_MSG, dlm->key,
				 &req, sizeof(req), nodenum, &status);
	/* XXX: negative status not handled properly here. */
	if (ret < 0)
		mlog_errno(ret);
	else {
		BUG_ON(status < 0);
		BUG_ON(status > DLM_LOCK_RES_OWNER_UNKNOWN);
		*real_master = (u8) (status & 0xff);
		mlog(0, "node %u responded to master requery with %u\n",
			  nodenum, *real_master);
		ret = 0;
	}
	return ret;
}


/* this function cannot error, so unless the sending
 * or receiving of the message failed, the owner can
 * be trusted */
int dlm_master_requery_handler(struct o2net_msg *msg, u32 len, void *data)
{
	struct dlm_ctxt *dlm = data;
	struct dlm_master_requery *req = (struct dlm_master_requery *)msg->buf;
	struct dlm_lock_resource *res = NULL;
	int master = DLM_LOCK_RES_OWNER_UNKNOWN;
	u32 flags = DLM_ASSERT_MASTER_REQUERY;

	if (!dlm_grab(dlm)) {
		/* since the domain has gone away on this
		 * node, the proper response is UNKNOWN */
		return master;
	}

	spin_lock(&dlm->spinlock);
	res = __dlm_lookup_lockres(dlm, req->name, req->namelen);
	if (res) {
		spin_lock(&res->spinlock);
		master = res->owner;
		if (master == dlm->node_num) {
			int ret = dlm_dispatch_assert_master(dlm, res,
							     0, 0, flags);
			if (ret < 0) {
				mlog_errno(-ENOMEM);
				/* retry!? */
				BUG();
			}
		}
		spin_unlock(&res->spinlock);
	}
	spin_unlock(&dlm->spinlock);

	dlm_put(dlm);
	return master;
}

static inline struct list_head *
dlm_list_num_to_pointer(struct dlm_lock_resource *res, int list_num)
{
	struct list_head *ret;
	BUG_ON(list_num < 0);
	BUG_ON(list_num > 2);
	ret = &(res->granted);
	ret += list_num;
	return ret;
}
/* TODO: do ast flush business
 * TODO: do MIGRATING and RECOVERING spinning
 */

/*
* NOTE about in-flight requests during migration:
*
* Before attempting the migrate, the master has marked the lockres as
* MIGRATING and then flushed all of its pending ASTS.  So any in-flight
* requests either got queued before the MIGRATING flag got set, in which
* case the lock data will reflect the change and a return message is on
* the way, or the request failed to get in before MIGRATING got set.  In
* this case, the caller will be told to spin and wait for the MIGRATING
* flag to be dropped, then recheck the master.
* This holds true for the convert, cancel and unlock cases, and since lvb
* updates are tied to these same messages, it applies to lvb updates as
* well.  For the lock case, there is no way a lock can be on the master
* queue and not be on the secondary queue since the lock is always added
* locally first.  This means that the new target node will never be sent
* a lock that he doesn't already have on the list.
* In total, this means that the local lock is correct and should not be
* updated to match the one sent by the master.  Any messages sent back
* from the master before the MIGRATING flag will bring the lock properly
* up-to-date, and the change will be ordered properly for the waiter.
* We will *not* attempt to modify the lock underneath the waiter.
*/

static int dlm_process_recovery_data(struct dlm_ctxt *dlm,
				     struct dlm_lock_resource *res,
				     struct dlm_migratable_lockres *mres)
{
	struct dlm_migratable_lock *ml;
	struct list_head *queue;
	struct dlm_lock *newlock = NULL;
	struct dlm_lockstatus *lksb = NULL;
	int ret = 0;
	int i;
	struct list_head *iter;
	struct dlm_lock *lock = NULL;

	mlog(0, "running %d locks for this lockres\n", mres->num_locks);
	for (i=0; i<mres->num_locks; i++) {
		ml = &(mres->ml[i]);
		BUG_ON(ml->highest_blocked != LKM_IVMODE);
		newlock = NULL;
		lksb = NULL;

		queue = dlm_list_num_to_pointer(res, ml->list);

		/* if the lock is for the local node it needs to
		 * be moved to the proper location within the queue.
		 * do not allocate a new lock structure. */
		if (ml->node == dlm->node_num) {
			/* MIGRATION ONLY! */
			BUG_ON(!(mres->flags & DLM_MRES_MIGRATION));

			spin_lock(&res->spinlock);
			list_for_each(iter, queue) {
				lock = list_entry (iter, struct dlm_lock, list);
				if (lock->ml.cookie != ml->cookie)
					lock = NULL;
				else
					break;
			}

			/* lock is always created locally first, and
			 * destroyed locally last.  it must be on the list */
			if (!lock) {
				mlog(ML_ERROR, "could not find local lock "
					       "with cookie %"MLFu64"!\n",
				     ml->cookie);
				BUG();
			}
			BUG_ON(lock->ml.node != ml->node);

			/* see NOTE above about why we do not update
			 * to match the master here */

			/* move the lock to its proper place */
			/* do not alter lock refcount.  switching lists. */
			list_del_init(&lock->list);
			list_add_tail(&lock->list, queue);
			spin_unlock(&res->spinlock);

			mlog(0, "just reordered a local lock!\n");
			continue;
		}

		/* lock is for another node. */
		newlock = dlm_new_lock(ml->type, ml->node,
				       be64_to_cpu(ml->cookie), NULL);
		if (!newlock) {
			ret = -ENOMEM;
			goto leave;
		}
		lksb = newlock->lksb;
		dlm_lock_attach_lockres(newlock, res);

		if (ml->convert_type != LKM_IVMODE) {
			BUG_ON(queue != &res->converting);
			newlock->ml.convert_type = ml->convert_type;
		}
		lksb->flags |= (ml->flags &
				(DLM_LKSB_PUT_LVB|DLM_LKSB_GET_LVB));
			
		if (mres->lvb[0]) {
			if (lksb->flags & DLM_LKSB_PUT_LVB) {
				/* other node was trying to update
				 * lvb when node died.  recreate the
				 * lksb with the updated lvb. */
				memcpy(lksb->lvb, mres->lvb, DLM_LVB_LEN);
			} else {
				/* otherwise, the node is sending its 
				 * most recent valid lvb info */
				BUG_ON(ml->type != LKM_EXMODE &&
				       ml->type != LKM_PRMODE);
				if (res->lvb[0] && (ml->type == LKM_EXMODE ||
				    memcmp(res->lvb, mres->lvb, DLM_LVB_LEN))) {
					mlog(ML_ERROR, "received bad lvb!\n");
					__dlm_print_one_lock_resource(res);
					BUG();
				}
				memcpy(res->lvb, mres->lvb, DLM_LVB_LEN);
			}
		}


		/* NOTE:
		 * wrt lock queue ordering and recovery:
		 *    1. order of locks on granted queue is
		 *       meaningless.
		 *    2. order of locks on converting queue is
		 *       LOST with the node death.  sorry charlie.
		 *    3. order of locks on the blocked queue is
		 *       also LOST.
		 * order of locks does not affect integrity, it
		 * just means that a lock request may get pushed
		 * back in line as a result of the node death.
		 * also note that for a given node the lock order
		 * for its secondary queue locks is preserved
		 * relative to each other, but clearly *not*
		 * preserved relative to locks from other nodes.
		 */
		spin_lock(&res->spinlock);
		dlm_lock_get(newlock);
		list_add_tail(&newlock->list, queue);
		spin_unlock(&res->spinlock);
	}
	mlog(0, "done running all the locks\n");

leave:
	if (ret < 0) {
		mlog_errno(ret);
		if (newlock)
			dlm_lock_put(newlock);
	}

	mlog_exit(ret);
	return ret;
}

void dlm_move_lockres_to_recovery_list(struct dlm_ctxt *dlm,
				       struct dlm_lock_resource *res)
{
	int i;
	struct list_head *queue, *iter, *iter2;
	struct dlm_lock *lock;

	res->state |= DLM_LOCK_RES_RECOVERING;
	if (!list_empty(&res->recovering))
		list_del_init(&res->recovering);
	list_add_tail(&res->recovering, &dlm->reco.resources);

	/* find any pending locks and put them back on proper list */
	for (i=DLM_BLOCKED_LIST; i>=DLM_GRANTED_LIST; i--) {
		queue = dlm_list_idx_to_ptr(res, i);
		list_for_each_safe(iter, iter2, queue) {
			lock = list_entry (iter, struct dlm_lock, list);
			dlm_lock_get(lock);
			if (lock->convert_pending) {
				/* move converting lock back to granted */
				BUG_ON(i != DLM_CONVERTING_LIST);
				mlog(0, "node died with convert pending "
				     "on %.*s. move back to granted list.\n",
				     res->lockname.len, res->lockname.name);
				dlm_revert_pending_convert(res, lock);
				lock->convert_pending = 0;
			} else if (lock->lock_pending) {
				/* remove pending lock requests completely */
				BUG_ON(i != DLM_BLOCKED_LIST);
				mlog(0, "node died with lock pending "
				     "on %.*s. remove from blocked list and skip.\n",
				     res->lockname.len, res->lockname.name);
				/* lock will be floating until ref in
				 * dlmlock_remote is freed after the network
				 * call returns.  ok for it to not be on any
				 * list since no ast can be called
				 * (the master is dead). */
				dlm_revert_pending_lock(res, lock);
				lock->lock_pending = 0;
			} else if (lock->unlock_pending) {
				/* if an unlock was in progress, treat as
				 * if this had completed successfully
				 * before sending this lock state to the
				 * new master.  note that the dlm_unlock
				 * call is still responsible for calling
				 * the unlockast.  that will happen after
				 * the network call times out.  for now,
				 * just move lists to prepare the new
				 * recovery master.  */
				BUG_ON(i != DLM_GRANTED_LIST);
				mlog(0, "node died with unlock pending "
				     "on %.*s. remove from blocked list and skip.\n",
				     res->lockname.len, res->lockname.name);
				dlm_commit_pending_unlock(res, lock);
				lock->unlock_pending = 0;
			} else if (lock->cancel_pending) {
				/* if a cancel was in progress, treat as
				 * if this had completed successfully
				 * before sending this lock state to the
				 * new master */
				BUG_ON(i != DLM_CONVERTING_LIST);
				mlog(0, "node died with cancel pending "
				     "on %.*s. move back to granted list.\n",
				     res->lockname.len, res->lockname.name);
				dlm_commit_pending_cancel(res, lock);
				lock->cancel_pending = 0;
			}
			dlm_lock_put(lock);
		}
	}
}



/* removes all recovered locks from the recovery list.
 * sets the res->owner to the new master.
 * unsets the RECOVERY flag and wakes waiters. */
static void dlm_finish_local_lockres_recovery(struct dlm_ctxt *dlm,
					      u8 dead_node, u8 new_master)
{
	int i;
	struct list_head *iter, *iter2;
	struct hlist_node *hash_iter;
	struct hlist_head *bucket;

	struct dlm_lock_resource *res;

	mlog_entry_void();

	assert_spin_locked(&dlm->spinlock);

	list_for_each_safe(iter, iter2, &dlm->reco.resources) {
		res = list_entry (iter, struct dlm_lock_resource, recovering);
		if (res->owner == dead_node) {
			list_del_init(&res->recovering);
			spin_lock(&res->spinlock);
			dlm_change_lockres_owner(dlm, res, new_master);
			res->state &= ~DLM_LOCK_RES_RECOVERING;
			__dlm_dirty_lockres(dlm, res);
			spin_unlock(&res->spinlock);
			wake_up(&res->wq);
		}
	}

	/* this will become unnecessary eventually, but
	 * for now we need to run the whole hash, clear
	 * the RECOVERING state and set the owner
	 * if necessary */
	for (i = 0; i < DLM_HASH_BUCKETS; i++) {
		bucket = &(dlm->lockres_hash[i]);
		hlist_for_each_entry(res, hash_iter, bucket, hash_node) {
			if (res->state & DLM_LOCK_RES_RECOVERING) {
				if (res->owner == dead_node) {
					mlog(0, "(this=%u) res %.*s owner=%u "
					     "was not on recovering list, but "
					     "clearing state anyway\n",
					     dlm->node_num, res->lockname.len,
					     res->lockname.name, new_master);
				} else if (res->owner == dlm->node_num) {
					mlog(0, "(this=%u) res %.*s owner=%u "
					     "was not on recovering list, "
					     "owner is THIS node, clearing\n",
					     dlm->node_num, res->lockname.len,
					     res->lockname.name, new_master);
				} else
					continue;

				spin_lock(&res->spinlock);
				dlm_change_lockres_owner(dlm, res, new_master);
				res->state &= ~DLM_LOCK_RES_RECOVERING;
				__dlm_dirty_lockres(dlm, res);
				spin_unlock(&res->spinlock);
				wake_up(&res->wq);
			}
		}
	}
}

static inline int dlm_lvb_needs_invalidation(struct dlm_lock *lock, int local)
{
	if (local) {
		if (lock->ml.type != LKM_EXMODE &&
		    lock->ml.type != LKM_PRMODE)
			return 1;
	} else if (lock->ml.type == LKM_EXMODE)
		return 1;
	return 0;
}

static void dlm_revalidate_lvb(struct dlm_ctxt *dlm,
			       struct dlm_lock_resource *res, u8 dead_node)
{
	struct list_head *iter, *queue;
	struct dlm_lock *lock;
	int blank_lvb = 0, local = 0;
	int i;
	u8 search_node;

	assert_spin_locked(&dlm->spinlock);
	assert_spin_locked(&res->spinlock);

	if (res->owner == dlm->node_num)
		/* if this node owned the lockres, and if the dead node 
		 * had an EX when he died, blank out the lvb */
		search_node = dead_node;
	else {
		/* if this is a secondary lockres, and we had no EX or PR
		 * locks granted, we can no longer trust the lvb */
		search_node = dlm->node_num;
		local = 1;  /* check local state for valid lvb */
	}

	for (i=DLM_GRANTED_LIST; i<=DLM_CONVERTING_LIST; i++) {
		queue = dlm_list_idx_to_ptr(res, i);
		list_for_each(iter, queue) {
			lock = list_entry (iter, struct dlm_lock, list);
			if (lock->ml.node == search_node) {
				if (dlm_lvb_needs_invalidation(lock, local)) {
					/* zero the lksb lvb and lockres lvb */
					blank_lvb = 1;
					memset(lock->lksb->lvb, 0, DLM_LVB_LEN);
				}
			}
		}
	}

	if (blank_lvb) {
		mlog(0, "clearing %.*s lvb, dead node %u had EX\n",
		     res->lockname.len, res->lockname.name, dead_node);
		memset(res->lvb, 0, DLM_LVB_LEN);
	}
}

static void dlm_free_dead_locks(struct dlm_ctxt *dlm,
				struct dlm_lock_resource *res, u8 dead_node)
{
	struct list_head *iter, *tmpiter;
	struct dlm_lock *lock;

	/* this node is the lockres master:
	 * 1) remove any stale locks for the dead node
	 * 2) if the dead node had an EX when he died, blank out the lvb 
	 */
	assert_spin_locked(&dlm->spinlock);
	assert_spin_locked(&res->spinlock);

	/* TODO: check pending_asts, pending_basts here */
	list_for_each_safe(iter, tmpiter, &res->granted) {
		lock = list_entry (iter, struct dlm_lock, list);
		if (lock->ml.node == dead_node) {
			list_del_init(&lock->list);
			dlm_lock_put(lock);
		}
	}
	list_for_each_safe(iter, tmpiter, &res->converting) {
		lock = list_entry (iter, struct dlm_lock, list);
		if (lock->ml.node == dead_node) {
			list_del_init(&lock->list);
			dlm_lock_put(lock);
		}
	}
	list_for_each_safe(iter, tmpiter, &res->blocked) {
		lock = list_entry (iter, struct dlm_lock, list);
		if (lock->ml.node == dead_node) {
			list_del_init(&lock->list);
			dlm_lock_put(lock);
		}
	}

	/* do not kick thread yet */
	__dlm_dirty_lockres(dlm, res);
}

/* if this node is the recovery master, and there are no
 * locks for a given lockres owned by this node that are in
 * either PR or EX mode, zero out the lvb before requesting.
 *
 */


static void dlm_do_local_recovery_cleanup(struct dlm_ctxt *dlm, u8 dead_node)
{
	struct hlist_node *iter;
	struct dlm_lock_resource *res;
	int i;
	struct hlist_head *bucket;
	struct dlm_lock *lock;


	/* purge any stale mles */
	dlm_clean_master_list(dlm, dead_node);

	/*
	 * now clean up all lock resources.  there are two rules:
	 *
	 * 1) if the dead node was the master, move the lockres
	 *    to the recovering list.  set the RECOVERING flag.
	 *    this lockres needs to be cleaned up before it can
	 *    be used further.
	 *
	 * 2) if this node was the master, remove all locks from
	 *    each of the lockres queues that were owned by the
	 *    dead node.  once recovery finishes, the dlm thread
	 *    can be kicked again to see if any ASTs or BASTs
	 *    need to be fired as a result.
	 */
	for (i = 0; i < DLM_HASH_BUCKETS; i++) {
		bucket = &(dlm->lockres_hash[i]);
		hlist_for_each_entry(res, iter, bucket, hash_node) {
 			/* always prune any $RECOVERY entries for dead nodes,
 			 * otherwise hangs can occur during later recovery */
			if (dlm_is_recovery_lock(res->lockname.name,
						 res->lockname.len)) {
				spin_lock(&res->spinlock);
				list_for_each_entry(lock, &res->granted, list) {
					if (lock->ml.node == dead_node) {
						mlog(0, "AHA! there was "
						     "a $RECOVERY lock for dead "
						     "node %u (%s)!\n",
						     dead_node, dlm->name);
						list_del_init(&lock->list);
						dlm_lock_put(lock);
						break;
					}
				}
				spin_unlock(&res->spinlock);
				continue;
			}			
			spin_lock(&res->spinlock);
			/* zero the lvb if necessary */
			dlm_revalidate_lvb(dlm, res, dead_node);
			if (res->owner == dead_node)
				dlm_move_lockres_to_recovery_list(dlm, res);
			else if (res->owner == dlm->node_num) {
				dlm_free_dead_locks(dlm, res, dead_node);
				__dlm_lockres_calc_usage(dlm, res);
			}
			spin_unlock(&res->spinlock);
		}
	}

}

static void __dlm_hb_node_down(struct dlm_ctxt *dlm, int idx)
{
	assert_spin_locked(&dlm->spinlock);

	/* check to see if the node is already considered dead */
	if (!test_bit(idx, dlm->live_nodes_map)) {
		mlog(0, "for domain %s, node %d is already dead. "
		     "another node likely did recovery already.\n",
		     dlm->name, idx);
		return;
	}

	/* check to see if we do not care about this node */
	if (!test_bit(idx, dlm->domain_map)) {
		/* This also catches the case that we get a node down
		 * but haven't joined the domain yet. */
		mlog(0, "node %u already removed from domain!\n", idx);
		return;
	}

	clear_bit(idx, dlm->live_nodes_map);

	/* Clean up join state on node death. */
	if (dlm->joining_node == idx) {
		mlog(0, "Clearing join state for node %u\n", idx);
		__dlm_set_joining_node(dlm, DLM_LOCK_RES_OWNER_UNKNOWN);
	}

	/* make sure local cleanup occurs before the heartbeat events */
	if (!test_bit(idx, dlm->recovery_map))
		dlm_do_local_recovery_cleanup(dlm, idx);

	/* notify anything attached to the heartbeat events */
	dlm_hb_event_notify_attached(dlm, idx, 0);

	mlog(0, "node %u being removed from domain map!\n", idx);
	clear_bit(idx, dlm->domain_map);
	/* wake up migration waiters if a node goes down.
	 * perhaps later we can genericize this for other waiters. */
	wake_up(&dlm->migration_wq);

	if (test_bit(idx, dlm->recovery_map))
		mlog(0, "domain %s, node %u already added "
		     "to recovery map!\n", dlm->name, idx);
	else
		set_bit(idx, dlm->recovery_map);
}

void dlm_hb_node_down_cb(struct o2nm_node *node, int idx, void *data)
{
	struct dlm_ctxt *dlm = data;

	if (!dlm_grab(dlm))
		return;

	spin_lock(&dlm->spinlock);
	__dlm_hb_node_down(dlm, idx);
	spin_unlock(&dlm->spinlock);

	dlm_put(dlm);
}

void dlm_hb_node_up_cb(struct o2nm_node *node, int idx, void *data)
{
	struct dlm_ctxt *dlm = data;

	if (!dlm_grab(dlm))
		return;

	spin_lock(&dlm->spinlock);
	set_bit(idx, dlm->live_nodes_map);
	/* do NOT notify mle attached to the heartbeat events.
	 * new nodes are not interesting in mastery until joined. */
	spin_unlock(&dlm->spinlock);

	dlm_put(dlm);
}

static void dlm_reco_ast(void *astdata)
{
	struct dlm_ctxt *dlm = astdata;
	mlog(0, "ast for recovery lock fired!, this=%u, dlm=%s\n",
	     dlm->node_num, dlm->name);
}
static void dlm_reco_bast(void *astdata, int blocked_type)
{
	struct dlm_ctxt *dlm = astdata;
	mlog(0, "bast for recovery lock fired!, this=%u, dlm=%s\n",
	     dlm->node_num, dlm->name);
}
static void dlm_reco_unlock_ast(void *astdata, enum dlm_status st)
{
	mlog(0, "unlockast for recovery lock fired!\n");
}

/*
 * dlm_pick_recovery_master will continually attempt to use
 * dlmlock() on the special "$RECOVERY" lockres with the
 * LKM_NOQUEUE flag to get an EX.  every thread that enters
 * this function on each node racing to become the recovery
 * master will not stop attempting this until either:
 * a) this node gets the EX (and becomes the recovery master),
 * or b) dlm->reco.new_master gets set to some nodenum 
 * != O2NM_INVALID_NODE_NUM (another node will do the reco).
 * so each time a recovery master is needed, the entire cluster
 * will sync at this point.  if the new master dies, that will
 * be detected in dlm_do_recovery */
static int dlm_pick_recovery_master(struct dlm_ctxt *dlm)
{
	enum dlm_status ret;
	struct dlm_lockstatus lksb;
	int status = -EINVAL;

	mlog(0, "starting recovery of %s at %lu, dead=%u, this=%u\n",
	     dlm->name, jiffies, dlm->reco.dead_node, dlm->node_num);
again:	
	memset(&lksb, 0, sizeof(lksb));

	ret = dlmlock(dlm, LKM_EXMODE, &lksb, LKM_NOQUEUE|LKM_RECOVERY,
		      DLM_RECOVERY_LOCK_NAME, dlm_reco_ast, dlm, dlm_reco_bast);

	mlog(0, "%s: dlmlock($RECOVERY) returned %d, lksb=%d\n",
	     dlm->name, ret, lksb.status);

	if (ret == DLM_NORMAL) {
		mlog(0, "dlm=%s dlmlock says I got it (this=%u)\n",
		     dlm->name, dlm->node_num);
		
		/* got the EX lock.  check to see if another node 
		 * just became the reco master */
		if (dlm_reco_master_ready(dlm)) {
			mlog(0, "%s: got reco EX lock, but %u will "
			     "do the recovery\n", dlm->name,
			     dlm->reco.new_master);
			status = -EEXIST;
		} else {
			status = 0;

			/* see if recovery was already finished elsewhere */
			spin_lock(&dlm->spinlock);
			if (dlm->reco.dead_node == O2NM_INVALID_NODE_NUM) {
				status = -EINVAL;	
				mlog(0, "%s: got reco EX lock, but "
				     "node got recovered already\n", dlm->name);
				if (dlm->reco.new_master != O2NM_INVALID_NODE_NUM) {
					mlog(ML_ERROR, "%s: new master is %u "
					     "but no dead node!\n", 
					     dlm->name, dlm->reco.new_master);
					BUG();
				}
			}
			spin_unlock(&dlm->spinlock);
		}

		/* if this node has actually become the recovery master,
		 * set the master and send the messages to begin recovery */
		if (!status) {
			mlog(0, "%s: dead=%u, this=%u, sending "
			     "begin_reco now\n", dlm->name, 
			     dlm->reco.dead_node, dlm->node_num);
			status = dlm_send_begin_reco_message(dlm,
				      dlm->reco.dead_node);
			/* this always succeeds */
			BUG_ON(status);

			/* set the new_master to this node */
			spin_lock(&dlm->spinlock);
			dlm->reco.new_master = dlm->node_num;
			spin_unlock(&dlm->spinlock);
		}

		/* recovery lock is a special case.  ast will not get fired,
		 * so just go ahead and unlock it. */
		ret = dlmunlock(dlm, &lksb, 0, dlm_reco_unlock_ast, dlm);
		if (ret == DLM_DENIED) {
			mlog(0, "got DLM_DENIED, trying LKM_CANCEL\n");
			ret = dlmunlock(dlm, &lksb, LKM_CANCEL, dlm_reco_unlock_ast, dlm);
		}
		if (ret != DLM_NORMAL) {
			/* this would really suck. this could only happen
			 * if there was a network error during the unlock
			 * because of node death.  this means the unlock
			 * is actually "done" and the lock structure is
			 * even freed.  we can continue, but only
			 * because this specific lock name is special. */
			mlog(ML_ERROR, "dlmunlock returned %d\n", ret);
		}
	} else if (ret == DLM_NOTQUEUED) {
		mlog(0, "dlm=%s dlmlock says another node got it (this=%u)\n",
		     dlm->name, dlm->node_num);
		/* another node is master. wait on
		 * reco.new_master != O2NM_INVALID_NODE_NUM 
		 * for at most one second */
		wait_event_timeout(dlm->dlm_reco_thread_wq,
					 dlm_reco_master_ready(dlm),
					 msecs_to_jiffies(1000));
		if (!dlm_reco_master_ready(dlm)) {
			mlog(0, "%s: reco master taking awhile\n",
			     dlm->name);
			goto again;
		}
		/* another node has informed this one that it is reco master */
		mlog(0, "%s: reco master %u is ready to recover %u\n",
		     dlm->name, dlm->reco.new_master, dlm->reco.dead_node);
		status = -EEXIST;
	} else {
		struct dlm_lock_resource *res;

		/* dlmlock returned something other than NOTQUEUED or NORMAL */
		mlog(ML_ERROR, "%s: got %s from dlmlock($RECOVERY), "
		     "lksb.status=%s\n", dlm->name, dlm_errname(ret),
		     dlm_errname(lksb.status));
		res = dlm_lookup_lockres(dlm, DLM_RECOVERY_LOCK_NAME,
					 DLM_RECOVERY_LOCK_NAME_LEN);
		if (res) {
			dlm_print_one_lock_resource(res);
			dlm_lockres_put(res);
		} else {
			mlog(ML_ERROR, "recovery lock not found\n");
		}
		BUG();
	}

	return status;
}

static int dlm_send_begin_reco_message(struct dlm_ctxt *dlm, u8 dead_node)
{
	struct dlm_begin_reco br;
	int ret = 0;
	struct dlm_node_iter iter;
	int nodenum;
	int status;

	mlog_entry("%u\n", dead_node);

	mlog(0, "dead node is %u\n", dead_node);

	spin_lock(&dlm->spinlock);
	dlm_node_iter_init(dlm->domain_map, &iter);
	spin_unlock(&dlm->spinlock);

	clear_bit(dead_node, iter.node_map);

	memset(&br, 0, sizeof(br));
	br.node_idx = dlm->node_num;
	br.dead_node = dead_node;

	while ((nodenum = dlm_node_iter_next(&iter)) >= 0) {
		ret = 0;
		if (nodenum == dead_node) {
			mlog(0, "not sending begin reco to dead node "
				  "%u\n", dead_node);
			continue;
		}
		if (nodenum == dlm->node_num) {
			mlog(0, "not sending begin reco to self\n");
			continue;
		}
retry:
		ret = -EINVAL;
		mlog(0, "attempting to send begin reco msg to %d\n",
			  nodenum);
		ret = o2net_send_message(DLM_BEGIN_RECO_MSG, dlm->key,
					 &br, sizeof(br), nodenum, &status);
		/* negative status is handled ok by caller here */
		if (ret >= 0)
			ret = status;
		if (dlm_is_host_down(ret)) {
			/* node is down.  not involved in recovery
			 * so just keep going */
			mlog(0, "%s: node %u was down when sending "
			     "begin reco msg (%d)\n", dlm->name, nodenum, ret);
			ret = 0;
		}
		if (ret < 0) {
			struct dlm_lock_resource *res;
			/* this is now a serious problem, possibly ENOMEM 
			 * in the network stack.  must retry */
			mlog_errno(ret);
			mlog(ML_ERROR, "begin reco of dlm %s to node %u "
			    " returned %d\n", dlm->name, nodenum, ret);
			res = dlm_lookup_lockres(dlm, DLM_RECOVERY_LOCK_NAME,
						 DLM_RECOVERY_LOCK_NAME_LEN);
			if (res) {
				dlm_print_one_lock_resource(res);
				dlm_lockres_put(res);
			} else {
				mlog(ML_ERROR, "recovery lock not found\n");
			}
			/* sleep for a bit in hopes that we can avoid 
			 * another ENOMEM */
			msleep(100);
			goto retry;
		}
	}

	return ret;
}

int dlm_begin_reco_handler(struct o2net_msg *msg, u32 len, void *data)
{
	struct dlm_ctxt *dlm = data;
	struct dlm_begin_reco *br = (struct dlm_begin_reco *)msg->buf;

	/* ok to return 0, domain has gone away */
	if (!dlm_grab(dlm))
		return 0;

	mlog(0, "node %u wants to recover node %u\n",
		  br->node_idx, br->dead_node);

	dlm_fire_domain_eviction_callbacks(dlm, br->dead_node);

	spin_lock(&dlm->spinlock);
	if (dlm->reco.new_master != O2NM_INVALID_NODE_NUM) {
		if (test_bit(dlm->reco.new_master, dlm->recovery_map)) {
			mlog(0, "%s: new_master %u died, changing "
			     "to %u\n", dlm->name, dlm->reco.new_master,
			     br->node_idx);
		} else {
			mlog(0, "%s: new_master %u NOT DEAD, changing "
			     "to %u\n", dlm->name, dlm->reco.new_master,
			     br->node_idx);
			/* may not have seen the new master as dead yet */
		}
	}
	if (dlm->reco.dead_node != O2NM_INVALID_NODE_NUM) {
		mlog(ML_NOTICE, "%s: dead_node previously set to %u, "
		     "node %u changing it to %u\n", dlm->name, 
		     dlm->reco.dead_node, br->node_idx, br->dead_node);
	}
	dlm->reco.new_master = br->node_idx;
	dlm->reco.dead_node = br->dead_node;
	if (!test_bit(br->dead_node, dlm->recovery_map)) {
		mlog(0, "recovery master %u sees %u as dead, but this "
		     "node has not yet.  marking %u as dead\n",
		     br->node_idx, br->dead_node, br->dead_node);
		if (!test_bit(br->dead_node, dlm->domain_map) ||
		    !test_bit(br->dead_node, dlm->live_nodes_map))
			mlog(0, "%u not in domain/live_nodes map "
			     "so setting it in reco map manually\n",
			     br->dead_node);
		set_bit(br->dead_node, dlm->recovery_map);
		__dlm_hb_node_down(dlm, br->dead_node);
	}
	spin_unlock(&dlm->spinlock);

	dlm_kick_recovery_thread(dlm);
	dlm_put(dlm);
	return 0;
}

static int dlm_send_finalize_reco_message(struct dlm_ctxt *dlm)
{
	int ret = 0;
	struct dlm_finalize_reco fr;
	struct dlm_node_iter iter;
	int nodenum;
	int status;

	mlog(0, "finishing recovery for node %s:%u\n",
	     dlm->name, dlm->reco.dead_node);

	spin_lock(&dlm->spinlock);
	dlm_node_iter_init(dlm->domain_map, &iter);
	spin_unlock(&dlm->spinlock);

	memset(&fr, 0, sizeof(fr));
	fr.node_idx = dlm->node_num;
	fr.dead_node = dlm->reco.dead_node;

	while ((nodenum = dlm_node_iter_next(&iter)) >= 0) {
		if (nodenum == dlm->node_num)
			continue;
		ret = o2net_send_message(DLM_FINALIZE_RECO_MSG, dlm->key,
					 &fr, sizeof(fr), nodenum, &status);
		if (ret >= 0) {
			ret = status;
			if (dlm_is_host_down(ret)) {
				/* this has no effect on this recovery 
				 * session, so set the status to zero to 
				 * finish out the last recovery */
				mlog(ML_ERROR, "node %u went down after this "
				     "node finished recovery.\n", nodenum);
				ret = 0;
			}
		}
		if (ret < 0) {
			mlog_errno(ret);
			break;
		}
	}

	return ret;
}

int dlm_finalize_reco_handler(struct o2net_msg *msg, u32 len, void *data)
{
	struct dlm_ctxt *dlm = data;
	struct dlm_finalize_reco *fr = (struct dlm_finalize_reco *)msg->buf;

	/* ok to return 0, domain has gone away */
	if (!dlm_grab(dlm))
		return 0;

	mlog(0, "node %u finalizing recovery of node %u\n",
	     fr->node_idx, fr->dead_node);

	spin_lock(&dlm->spinlock);

	if (dlm->reco.new_master != fr->node_idx) {
		mlog(ML_ERROR, "node %u sent recovery finalize msg, but node "
		     "%u is supposed to be the new master, dead=%u\n",
		     fr->node_idx, dlm->reco.new_master, fr->dead_node);
		BUG();
	}
	if (dlm->reco.dead_node != fr->dead_node) {
		mlog(ML_ERROR, "node %u sent recovery finalize msg for dead "
		     "node %u, but node %u is supposed to be dead\n",
		     fr->node_idx, fr->dead_node, dlm->reco.dead_node);
		BUG();
	}

	dlm_finish_local_lockres_recovery(dlm, fr->dead_node, fr->node_idx);

	spin_unlock(&dlm->spinlock);

	dlm_reset_recovery(dlm);

	dlm_kick_recovery_thread(dlm);
	dlm_put(dlm);
	return 0;
}
