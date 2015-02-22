/*
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id: ib_srp.h,v 1.1.1.1 2007/05/07 23:30:53 jungo Exp $
 */

#ifndef IB_SRP_H
#define IB_SRP_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_sa.h>
#include <rdma/ib_cm.h>

enum {
	SRP_PATH_REC_TIMEOUT_MS	= 1000,
	SRP_ABORT_TIMEOUT_MS	= 5000,

	SRP_PORT_REDIRECT	= 1,
	SRP_DLID_REDIRECT	= 2,

	SRP_MAX_LUN		= 512,
	SRP_MAX_IU_LEN		= 256,

	SRP_RQ_SHIFT    	= 6,
	SRP_RQ_SIZE		= 1 << SRP_RQ_SHIFT,
	SRP_SQ_SIZE		= SRP_RQ_SIZE - 1,
	SRP_CQ_SIZE		= SRP_SQ_SIZE + SRP_RQ_SIZE,

	SRP_TAG_TSK_MGMT	= 1 << (SRP_RQ_SHIFT + 1)
};

#define SRP_OP_RECV		(1 << 31)
#define SRP_MAX_INDIRECT	((SRP_MAX_IU_LEN -			\
				  sizeof (struct srp_cmd) -		\
				  sizeof (struct srp_indirect_buf)) / 16)

enum srp_target_state {
	SRP_TARGET_LIVE,
	SRP_TARGET_CONNECTING,
	SRP_TARGET_DEAD,
	SRP_TARGET_REMOVED
};

struct srp_host {
	u8			initiator_port_id[16];
	struct ib_device       *dev;
	u8                      port;
	struct ib_pd	       *pd;
	struct ib_mr	       *mr;
	struct class_device	class_dev;
	struct list_head	target_list;
	struct mutex            target_mutex;
	struct completion	released;
	struct list_head	list;
};

struct srp_request {
	struct list_head	list;
	struct scsi_cmnd       *scmnd;
	struct srp_iu	       *cmd;
	struct srp_iu	       *tsk_mgmt;
	DECLARE_PCI_UNMAP_ADDR(direct_mapping)
	struct completion	done;
	short			next;
	u8			cmd_done;
	u8			tsk_status;
};

struct srp_target_port {
	__be64			id_ext;
	__be64			ioc_guid;
	__be64			service_id;
	struct srp_host	       *srp_host;
	struct Scsi_Host       *scsi_host;
	char			target_name[32];
	unsigned int		scsi_id;

	struct ib_sa_path_rec	path;
	struct ib_sa_query     *path_query;
	int			path_query_id;

	struct ib_cm_id	       *cm_id;
	struct ib_cq	       *cq;
	struct ib_qp	       *qp;

	int			max_ti_iu_len;
	s32			req_lim;

	unsigned		rx_head;
	struct srp_iu	       *rx_ring[SRP_RQ_SIZE];

	unsigned		tx_head;
	unsigned		tx_tail;
	struct srp_iu	       *tx_ring[SRP_SQ_SIZE + 1];

	int			req_head;
	struct list_head	req_queue;
	struct srp_request	req_ring[SRP_SQ_SIZE];

	struct work_struct	work;

	struct list_head	list;
	struct completion	done;
	int			status;
	enum srp_target_state	state;
};

struct srp_iu {
	dma_addr_t		dma;
	void		       *buf;
	size_t			size;
	enum dma_data_direction	direction;
};

#endif /* IB_SRP_H */
