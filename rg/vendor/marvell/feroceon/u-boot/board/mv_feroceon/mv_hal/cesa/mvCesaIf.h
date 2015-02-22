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

/*******************************************************************************
* mvCesa.h - Header File for Cryptographic Engines and Security Accelerator
*
* DESCRIPTION:
*       This header file contains macros typedefs and function declaration for
*       the Marvell Cryptographic Engines and Security Accelerator.
*
*******************************************************************************/

#ifndef __mvCesaIf_h__
#define __mvCesaIf_h__

#ifdef __cplusplus
extern "C" {
#endif

#include "ctrlEnv/mvCtrlEnvSpec.h"
#include "mvSysCesaConfig.h"
#include "mvCesa.h"
#include "mvCesaRegs.h"

typedef enum {
	CESA_NULL_POLICY = 0,
	CESA_SINGLE_CHAN_POLICY,
	CESA_WEIGHTED_CHAN_POLICY,
	CESA_FLOW_ASSOC_CHAN_POLICY
} MV_CESA_POLICY;

typedef enum {
	CESA_NULL_FLOW_TYPE = 0,
	CESA_IPSEC_FLOW_TYPE,
	CESA_SSL_FLOW_TYPE,
	CESA_DISK_FLOW_TYPE,
	CESA_NFPSEC_FLOW_TYPE
} MV_CESA_FLOW_TYPE;

	MV_STATUS mvCesaIfHalInit(int numOfSession, int queueDepth, void *osHandle, MV_CESA_HAL_DATA *halData);
	MV_STATUS mvCesaIfTdmaWinInit(MV_U8 chan, MV_UNIT_WIN_INFO *addrWinMap);
	MV_STATUS mvCesaIfFinish(void);
	MV_STATUS mvCesaIfSessionOpen(MV_CESA_OPEN_SESSION *pSession, short *pSid);
	MV_STATUS mvCesaIfSessionClose(short sid);
	MV_STATUS mvCesaIfAction(MV_CESA_COMMAND *pCmd);
	MV_STATUS mvCesaIfReadyGet(MV_U8 chan, MV_CESA_RESULT *pResult);
	MV_STATUS mvCesaIfPolicySet(MV_CESA_POLICY cesaPolicy, MV_CESA_FLOW_TYPE flowType);
	MV_STATUS mvCesaIfPolicyGet(MV_CESA_POLICY *pCesaPolicy);
	MV_VOID mvCesaIfDebugMbuf(const char *str, MV_CESA_MBUF *pMbuf, int offset, int size);

#ifdef __cplusplus
}
#endif

#endif /* __mvCesaIf_h__ */
