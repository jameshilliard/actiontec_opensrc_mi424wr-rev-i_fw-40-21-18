/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.

********************************************************************************
Marvell Commercial License Option

If you received this File from Marvell and you have entered into a commercial
license agreement (a "Commercial License") with Marvell, the File is licensed
to you under the terms of the applicable Commercial License.

********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File in accordance with the terms and conditions of the General
Public License Version 2, June 1991 (the "GPL License"), a copy of which is
available along with the File in the license.txt file or by writing to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
on the worldwide web at http://www.gnu.org/licenses/gpl.txt.

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
DISCLAIMED.  The GPL License provides additional details about this warranty
disclaimer.
********************************************************************************
Marvell BSD License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File under the following licensing terms.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
	this list of conditions and the following disclaimer.

    *   Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

    *   Neither the name of Marvell nor the names of its contributors may be
	used to endorse or promote products derived from this software without
	specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "mvCommon.h"
#include "mvOs.h"
#include "mvCesaIf.h"

/* Globals */
MV_CESA_FLOW_TYPE chanFlowType[MV_CESA_CHANNELS];
MV_U64 chanWeight[MV_CESA_CHANNELS];
MV_STATUS readyStatus[MV_CESA_CHANNELS];
MV_CESA_RESULT **pResQueue;
MV_CESA_RESULT *resQueue;
MV_CESA_POLICY currCesaPolicy;
MV_U8 splitChanId;
MV_U32 resQueueDepth;
MV_U32 currReqId;
MV_U32 gReqId;
MV_U32 reqEmpty;

MV_STATUS mvCesaIfHalInit(int numOfSession, int queueDepth, void *osHandle, MV_CESA_HAL_DATA *halData)
{
	/* Init globals */
	memset(chanWeight, 0, (MV_CESA_CHANNELS * sizeof(MV_U64)));
	memset(chanFlowType, 0, (MV_CESA_CHANNELS * sizeof(MV_CESA_FLOW_TYPE)));
	currCesaPolicy = CESA_NULL_POLICY;
	splitChanId = 0;

	if(MV_CESA_CHANNELS > 1) {
		currReqId = 0;
		gReqId = 0;
		reqEmpty = 0;
		resQueueDepth = ((MV_CESA_CHANNELS * queueDepth * 2));
		
		/* Allocate reordered results queue */
		pResQueue = (MV_CESA_RESULT**)mvOsMalloc(resQueueDepth * sizeof(MV_CESA_RESULT*));
		if(pResQueue == NULL) {
			mvOsPrintf("%s: Error, pResQueue malloc failed\n", __func__);
			return MV_ERROR;
		}

		resQueue = (MV_CESA_RESULT*)mvOsMalloc(resQueueDepth * sizeof(MV_CESA_RESULT));
		if(resQueue == NULL) {
			mvOsPrintf("%s: Error, resQueue malloc failed\n", __func__);
			return MV_ERROR;
		}

		memset(pResQueue, 0, (resQueueDepth * sizeof(MV_CESA_RESULT*)));
		memset(resQueue, 0, (resQueueDepth * sizeof(MV_CESA_RESULT)));
		memset(readyStatus, MV_TRUE, (MV_CESA_CHANNELS * sizeof(MV_STATUS)));
	}

	return mvCesaHalInit(numOfSession, queueDepth, osHandle, halData);
}

MV_STATUS mvCesaIfTdmaWinInit(MV_U8 chan, MV_UNIT_WIN_INFO *addrWinMap)
{
	return mvCesaTdmaWinInit(chan, addrWinMap);
}

MV_STATUS mvCesaIfFinish(void)
{
	if(MV_CESA_CHANNELS > 1) {
		/* Free results queues */
		mvOsFree(pResQueue);
		mvOsFree(resQueue);
	}

	return mvCesaFinish();
}

MV_STATUS mvCesaIfSessionOpen(MV_CESA_OPEN_SESSION *pSession, short *pSid)
{
	return mvCesaSessionOpen(pSession, pSid);
}

MV_STATUS mvCesaIfSessionClose(short sid)
{
	return mvCesaSessionClose(sid);
}

MV_STATUS mvCesaIfAction(MV_CESA_COMMAND *pCmd)
{
	MV_U8 chan = 0, chanIndex = 0;
	MV_U64 min;

	if(MV_CESA_CHANNELS > 1) {
		switch(currCesaPolicy) {
			case CESA_WEIGHTED_CHAN_POLICY:
			case CESA_NULL_POLICY:
				min = chanWeight[0];
				for(chan = 1; chan < MV_CESA_CHANNELS; chan++) {
					if(chanWeight[chan] < min) {
						min = chanWeight[chan];
						chanIndex = chan;
					}
				}
				chanWeight[chanIndex] += pCmd->pSrc->mbufSize;
				break;

			case CESA_FLOW_ASSOC_CHAN_POLICY:
				/* TBD - handle policy */
				break;

			case CESA_SINGLE_CHAN_POLICY:
				break;

			default:
				mvOsPrintf("%s: Error, policy not supported\n", __func__);
				return MV_ERROR;		
		}

		/* Check if we need to handle SPLIT case */
		if(pCmd->split != MV_CESA_SPLIT_NONE) {
			if(pCmd->split == MV_CESA_SPLIT_FIRST)
				splitChanId = chanIndex;
			else /* MV_CESA_SPLIT_SECOND */
				chanIndex = splitChanId;
		}

		/* In case of 2 channels or more, update request id */
		pCmd->reqId = gReqId;
		gReqId = ((gReqId+1) % resQueueDepth);
	}

	return mvCesaAction(chanIndex, pCmd);
}

MV_STATUS mvCesaIfReadyGet(MV_U8 chan, MV_CESA_RESULT *pResult)
{
	MV_STATUS status;
	MV_CESA_RESULT *pRes = NULL;

	/* Skip policy handling for single channel */
	if(MV_CESA_CHANNELS == 1)
		return mvCesaReadyGet(chan, pResult);

	while(MV_TRUE) {
		
		if(readyStatus[chan] == MV_FALSE)
			break;

		pRes = &resQueue[reqEmpty];

		/* Get next request */
		status = mvCesaReadyGet(chan, pRes);

		if(status != MV_OK) {
			readyStatus[chan] = MV_FALSE;
			break;
		}

		switch(currCesaPolicy) {
			case CESA_WEIGHTED_CHAN_POLICY:
			case CESA_NULL_POLICY:
				chanWeight[chan] -= pRes->mbufSize;
				break;

			case CESA_FLOW_ASSOC_CHAN_POLICY:
				/* TBD - handle policy */
				break;

			case CESA_SINGLE_CHAN_POLICY:
				break;

			default:
				mvOsPrintf("%s: Error, policy not supported\n", __func__);
				return MV_ERROR;		
		}

		if(pResQueue[pRes->reqId] != NULL) {
			mvOsPrintf("%s: Warning, results queue entry is being override(reqId=%d)\n", __func__, pRes->reqId);
			TRC_OUTPUT();
		}

		pResQueue[pRes->reqId] = pRes;
		reqEmpty = ((reqEmpty+1) % resQueueDepth);
	}

	if(pResQueue[currReqId] == NULL) {
		readyStatus[chan] = MV_TRUE;
		return MV_NOT_READY;
	}
	else {
		/* Release results in order */
		readyStatus[chan] = MV_FALSE;
		/*memcpy(pResult, pResQueue[currReqId], sizeof(MV_CESA_RESULT));*/
		pResult->retCode = ((MV_CESA_RESULT*)pResQueue[currReqId])->retCode;
		pResult->pReqPrv = ((MV_CESA_RESULT*)pResQueue[currReqId])->pReqPrv;
		pResult->sessionId = ((MV_CESA_RESULT*)pResQueue[currReqId])->sessionId;
		pResult->mbufSize = ((MV_CESA_RESULT*)pResQueue[currReqId])->mbufSize;
		pResult->reqId = ((MV_CESA_RESULT*)pResQueue[currReqId])->reqId;
		pResQueue[currReqId] = NULL;
		currReqId = ((currReqId+1) % resQueueDepth);
		return MV_OK;
	}
}

MV_STATUS mvCesaIfPolicySet(MV_CESA_POLICY cesaPolicy, MV_CESA_FLOW_TYPE flowType)
{
	MV_U8 chan = 0;

	if(currCesaPolicy == CESA_NULL_POLICY) {
		currCesaPolicy = cesaPolicy;
	}
	else {
		/* Check if more than 1 policy was requested */
		if(currCesaPolicy != cesaPolicy) {
			mvOsPrintf("%s: Error, can not support multiple policies\n", __func__);
			return MV_ERROR;
		}
	}

	if(cesaPolicy == CESA_FLOW_ASSOC_CHAN_POLICY) {

		if(flowType == CESA_NULL_FLOW_TYPE) {
			mvOsPrintf("%s: Error, bad policy configuration\n", __func__);
			return MV_ERROR;
		}

		/* Find next empty entry */
		for(chan = 0; chan < MV_CESA_CHANNELS; chan++) {
			if(chanFlowType[chan] == CESA_NULL_FLOW_TYPE)
				chanFlowType[chan] = flowType;
		}

		if(chan == MV_CESA_CHANNELS) {
			mvOsPrintf("%s: Error, no empty entry is available\n", __func__);
			return MV_ERROR;
		}

	}

	return MV_OK;
}

MV_STATUS mvCesaIfPolicyGet(MV_CESA_POLICY *pCesaPolicy)
{
	*pCesaPolicy = currCesaPolicy;

	return MV_OK;
}

MV_VOID mvCesaIfDebugMbuf(const char *str, MV_CESA_MBUF *pMbuf, int offset, int size)
{
	return mvCesaIfDebugMbuf(str, pMbuf, offset, size);
}
