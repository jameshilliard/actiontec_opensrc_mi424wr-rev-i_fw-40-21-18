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

#ifndef __mvNeta_h__
#define __mvNeta_h__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
#include "mvTypes.h"
#include "mvCommon.h"
#include "mvOs.h"
#include "ctrlEnv/mvCtrlEnvSpec.h"
#include "mvSysEthConfig.h"
#include "mvNetaRegs.h"
#include "mvEthRegs.h"


#define NETA_RX_IP_IS_FRAG(status)     ((status) & NETA_RX_IP4_FRAG_MASK)
#define NETA_RX_L4_CSUM_IS_OK(status)  ((status) & NETA_RX_L4_CSUM_OK_MASK)

#ifdef CONFIG_MV_ETH_PNC
#define NETA_RX_L3_IS_IP4(status)      (((status) & NETA_RX_L3_MASK) == NETA_RX_L3_IP4)
#define NETA_RX_L3_IS_IP4_ERR(status)  (((status) & NETA_RX_L3_MASK) == NETA_RX_L3_IP4_ERR)
#define NETA_RX_L3_IS_IP6(status)      (((status) & NETA_RX_L3_MASK) == NETA_RX_L3_IP6)
#define NETA_RX_L4_IS_TCP(status)      (((status) & NETA_RX_L4_MASK) == NETA_RX_L4_TCP)
#define NETA_RX_L4_IS_UDP(status)      (((status) & NETA_RX_L4_MASK) == NETA_RX_L4_UDP)
#else
#define NETA_RX_L3_IS_IP4(status)      ((status) & ETH_RX_IP_HEADER_OK_MASK)
#define NETA_RX_L3_IS_IP4_ERR(status)  (((status) & ETH_RX_IP_FRAME_TYPE_MASK) &&  \
										!((status) & ETH_RX_IP_HEADER_OK_MASK))
#define NETA_RX_L3_IS_IP6(status)      (MV_FALSE)
#define NETA_RX_L4_IS_TCP(status)      (((status) & ETH_RX_L4_TYPE_MASK) == ETH_RX_L4_TCP_TYPE)
#define NETA_RX_L4_IS_UDP(status)      (((status) & ETH_RX_L4_TYPE_MASK) == ETH_RX_L4_UDP_TYPE)
#endif				/* CONFIG_MV_ETH_PNC */


/* Default port configuration value */
#define PORT_CONFIG_VALUE(rxQ)			\
	(					\
	ETH_DEF_RX_QUEUE_MASK(rxQ) |		\
	ETH_DEF_RX_ARP_QUEUE_MASK(rxQ) |	\
	ETH_DEF_RX_TCP_QUEUE_MASK(rxQ) |	\
	ETH_DEF_RX_UDP_QUEUE_MASK(rxQ) |	\
	ETH_DEF_RX_BPDU_QUEUE_MASK(rxQ) |	\
	ETH_RX_CHECKSUM_WITH_PSEUDO_HDR		\
	)

/* Default port extend configuration value */
#define PORT_CONFIG_EXTEND_VALUE            0
#define PORT_SERIAL_CONTROL_VALUE           0


typedef enum {
	MV_ETH_SPEED_AN,
	MV_ETH_SPEED_10,
	MV_ETH_SPEED_100,
	MV_ETH_SPEED_1000
} MV_ETH_PORT_SPEED;

typedef enum {
	MV_ETH_DUPLEX_AN,
	MV_ETH_DUPLEX_HALF,
	MV_ETH_DUPLEX_FULL
} MV_ETH_PORT_DUPLEX;

typedef enum {
	MV_ETH_FC_AN_NO,
	MV_ETH_FC_AN_SYM,
	MV_ETH_FC_AN_ASYM,
	MV_ETH_FC_DISABLE,
	MV_ETH_FC_ENABLE,
	MV_ETH_FC_ACTIVE,

} MV_ETH_PORT_FC;


typedef enum {
	MV_ETH_PRIO_FIXED = 0,	/* Fixed priority mode */
	MV_ETH_PRIO_WRR = 1	/* Weighted round robin priority mode */
} MV_ETH_PRIO_MODE;

/* Ethernet port specific infomation */
typedef struct eth_link_status {
	MV_BOOL				linkup;
	MV_ETH_PORT_SPEED	speed;
	MV_ETH_PORT_DUPLEX	duplex;
	MV_ETH_PORT_FC		rxFc;
	MV_ETH_PORT_FC		txFc;

} MV_ETH_PORT_STATUS;

typedef enum {
	MV_NETA_MH_NONE = 0,
	MV_NETA_MH = 1,
	MV_NETA_DSA = 2,
	MV_NETA_DSA_EXT = 3
} MV_NETA_MH_MODE;

typedef struct {
	MV_U32 maxPort;
	MV_U32 pClk;
	MV_U32 tClk;

#ifdef CONFIG_MV_ETH_BM
	MV_ULONG bmPhysBase;
	MV_U8 *bmVirtBase;
#endif				/* CONFIG_MV_ETH_BM */

#ifdef CONFIG_MV_ETH_PNC
	MV_ULONG pncPhysBase;
	MV_U8 *pncVirtBase;
#endif				/* CONFIG_MV_ETH_PNC */
	MV_U8 maxCPUs;
} MV_NETA_HAL_DATA;

typedef struct eth_pbuf {
	void *osInfo;
	MV_ULONG physAddr;
	MV_U8 *pBuf;
	MV_U16 bytes;
	MV_U16 offset;
	MV_U32 tx_cmd;
	void *dev;
	MV_U8 pool;
	MV_U8 tos;
	MV_U16 reserved;
	MV_U32 hw_cmd;
} MV_ETH_PKT;

typedef struct {
	char *pFirst;
	int lastDesc;
	int nextToProc;
	int descSize;
	MV_BUF_INFO descBuf;

} MV_NETA_QUEUE_CTRL;

#define MV_NETA_QUEUE_DESC_PTR(pQueueCtrl, descIdx)                 \
    ((pQueueCtrl)->pFirst + ((descIdx) * NETA_DESC_ALIGNED_SIZE))

#define MV_NETA_QUEUE_NEXT_DESC(pQueueCtrl, descIdx)  \
    (((descIdx) < (pQueueCtrl)->lastDesc) ? ((descIdx) + 1) : 0)

#define MV_NETA_QUEUE_PREV_DESC(pQueueCtrl, descIdx)  \
    (((descIdx) > 0) ? ((descIdx) - 1) : (pQueueCtrl)->lastDesc)

typedef struct {
	MV_NETA_QUEUE_CTRL queueCtrl;

} MV_NETA_RXQ_CTRL;

typedef struct {
	MV_NETA_QUEUE_CTRL queueCtrl;

} MV_NETA_TXQ_CTRL;

typedef struct {
	int portNo;
	MV_NETA_RXQ_CTRL *pRxQueue;
	MV_NETA_TXQ_CTRL *pTxQueue;
	int rxqNum;
	int txpNum;
	int txqNum;
	MV_U8 mcastCount[256];
	void *osHandle;
} MV_NETA_PORT_CTRL;

extern MV_NETA_PORT_CTRL **mvNetaPortCtrl;
extern MV_NETA_HAL_DATA mvNetaHalData;

#ifdef CONFIG_MV_PON
#define MV_ETH_MAX_TCONT() 	CONFIG_MV_PON_TCONTS
#endif /* CONFIG_MV_PON */

/* Get Giga port handler */
static INLINE MV_NETA_PORT_CTRL *mvNetaPortHndlGet(int port)
{
	return mvNetaPortCtrl[port];
}

/* Get RX queue handler */
static INLINE MV_NETA_RXQ_CTRL *mvNetaRxqHndlGet(int port, int rxq)
{
	return &mvNetaPortCtrl[port]->pRxQueue[rxq];
}

/* Get TX queue handler */
static INLINE MV_NETA_TXQ_CTRL *mvNetaTxqHndlGet(int port, int txp, int txq)
{
	MV_NETA_PORT_CTRL *pPortCtrl = mvNetaPortCtrl[port];

	return &pPortCtrl->pTxQueue[txp * pPortCtrl->txqNum + txq];
}

#if defined(MV_CPU_BE)
/* Swap RX descriptor to be BE */
static INLINE void mvNetaRxqDescSwap(NETA_RX_DESC *pRxDesc)
{
	pRxDesc->status = MV_BYTE_SWAP_32BIT(pRxDesc->status);
	pRxDesc->pncInfo = MV_BYTE_SWAP_16BIT(pRxDesc->pncInfo);
	pRxDesc->dataSize =  MV_BYTE_SWAP_16BIT(pRxDesc->dataSize);
	pRxDesc->bufPhysAddr = MV_BYTE_SWAP_32BIT(pRxDesc->bufPhysAddr);
	pRxDesc->pncFlowId = MV_BYTE_SWAP_32BIT(pRxDesc->pncFlowId);
	/* pRxDesc->bufCookie = MV_BYTE_SWAP_32BIT(pRxDesc->bufCookie); */
	pRxDesc->prefetchCmd = MV_BYTE_SWAP_16BIT(pRxDesc->prefetchCmd);
	pRxDesc->csumL4 = MV_BYTE_SWAP_16BIT(pRxDesc->csumL4);
	pRxDesc->pncExtra = MV_BYTE_SWAP_32BIT(pRxDesc->pncExtra);
	pRxDesc->hw_cmd = MV_BYTE_SWAP_32BIT(pRxDesc->hw_cmd);
}

/* Swap TX descriptor to be BE */
static INLINE void mvNetaTxqDescSwap(NETA_TX_DESC *pTxDesc)
{
	pTxDesc->command = MV_BYTE_SWAP_32BIT(pTxDesc->command);
    pTxDesc->csumL4 = MV_BYTE_SWAP_16BIT(pTxDesc->csumL4);
    pTxDesc->dataSize = MV_BYTE_SWAP_16BIT(pTxDesc->dataSize);
    pTxDesc->bufPhysAddr = MV_BYTE_SWAP_32BIT(pTxDesc->bufPhysAddr);
    pTxDesc->hw_cmd = MV_BYTE_SWAP_32BIT(pTxDesc->hw_cmd);
}
#endif /* MV_CPU_BE */

/* Get number of RX descriptors occupied by received packets */
static INLINE int mvNetaRxqBusyDescNumGet(int port, int rxq)
{
	MV_U32 regVal;

	regVal = MV_REG_READ(NETA_RXQ_STATUS_REG(port, rxq));

	return (regVal & NETA_RXQ_OCCUPIED_DESC_ALL_MASK) >> NETA_RXQ_OCCUPIED_DESC_OFFS;
}

/* Get number of free RX descriptors ready to received new packets */
static INLINE int mvNetaRxqFreeDescNumGet(int port, int rxq)
{
	MV_U32 regVal;

	regVal = MV_REG_READ(NETA_RXQ_STATUS_REG(port, rxq));

	return (regVal & NETA_RXQ_NON_OCCUPIED_DESC_ALL_MASK) >> NETA_RXQ_NON_OCCUPIED_DESC_OFFS;
}

/* Update HW with number of RX descriptors processed by SW:
 *    - decrement number of occupied descriptors
 *    - increment number of Non-occupied descriptors
 */
static INLINE void mvNetaRxqDescNumUpdate(int port, int rxq, int rx_done, int rx_filled)
{
	MV_U32 regVal;

	/* Only 255 descriptors can be added at once - we don't check it for performance */
	/* Assume caller process RX desriptors in quanta less than 256 */
	regVal = (rx_done << NETA_RXQ_DEC_OCCUPIED_OFFS) | (rx_filled << NETA_RXQ_ADD_NON_OCCUPIED_OFFS);
	MV_REG_WRITE(NETA_RXQ_STATUS_UPDATE_REG(port, rxq), regVal);
}

/* Add number of descriptors are ready to receive new packets */
static INLINE void mvNetaRxqNonOccupDescAdd(int port, int rxq, int rx_desc)
{
	MV_U32 regVal;

	/* Only 255 descriptors can be added at once */
	while (rx_desc > 0xFF) {
		regVal = (0xFF << NETA_RXQ_ADD_NON_OCCUPIED_OFFS);
		MV_REG_WRITE(NETA_RXQ_STATUS_UPDATE_REG(port, rxq), regVal);
		rx_desc = rx_desc - 0xFF;
	}
	regVal = (rx_desc << NETA_RXQ_ADD_NON_OCCUPIED_OFFS);
	MV_REG_WRITE(NETA_RXQ_STATUS_UPDATE_REG(port, rxq), regVal);
}

/* Get number of TX descriptors already sent by HW */
static INLINE int mvNetaTxqSentDescNumGet(int port, int txp, int txq)
{
	MV_U32 regVal;

	regVal = MV_REG_READ(NETA_TXQ_STATUS_REG(port, txp, txq));

	return (regVal & NETA_TXQ_SENT_DESC_MASK) >> NETA_TXQ_SENT_DESC_OFFS;
}

/* Get number of TX descriptors didn't send by HW yet and waiting for TX */
static INLINE int mvNetaTxqPendDescNumGet(int port, int txp, int txq)
{
	MV_U32 regVal;

	regVal = MV_REG_READ(NETA_TXQ_STATUS_REG(port, txp, txq));

	return (regVal & NETA_TXQ_PENDING_DESC_MASK) >> NETA_TXQ_PENDING_DESC_OFFS;
}

/* Update HW with number of TX descriptors to be sent */
static INLINE void mvNetaTxqPendDescAdd(int port, int txp, int txq, int pend_desc)
{
	MV_U32 regVal;

	/* Only 255 descriptors can be added at once - we don't check it for performance */
	/* Assume caller process TX desriptors in quanta less than 256 */
	regVal = (pend_desc << NETA_TXQ_ADD_PENDING_OFFS);
	MV_REG_WRITE(NETA_TXQ_UPDATE_REG(port, txp, txq), regVal);
}

/* Decrement sent descriptors counter */
static INLINE void mvNetaTxqSentDescDec(int port, int txp, int txq, int sent_desc)
{
	MV_U32 regVal;

	/* Only 255 TX descriptors can be updated at once */
	while (sent_desc > 0xFF) {
		regVal = (0xFF << NETA_TXQ_DEC_SENT_OFFS);
		MV_REG_WRITE(NETA_TXQ_UPDATE_REG(port, txp, txq), regVal);
		sent_desc = sent_desc - 0xFF;
	}
	regVal = (sent_desc << NETA_TXQ_DEC_SENT_OFFS);
	MV_REG_WRITE(NETA_TXQ_UPDATE_REG(port, txp, txq), regVal);
}

/* Get pointer to next RX descriptor to be processed by SW */
static INLINE NETA_RX_DESC *mvNetaRxqNextDescGet(MV_NETA_RXQ_CTRL *pRxq)
{
	NETA_RX_DESC	*pRxDesc;
	int				rxDesc = pRxq->queueCtrl.nextToProc;

	pRxq->queueCtrl.nextToProc = MV_NETA_QUEUE_NEXT_DESC(&(pRxq->queueCtrl), rxDesc);

	pRxDesc = ((NETA_RX_DESC *)pRxq->queueCtrl.pFirst) + rxDesc;

	mvOsCacheLineInv(NULL, pRxDesc);

#if defined(MV_CPU_BE)
	mvNetaRxqDescSwap(pRxDesc);
#endif /* MV_CPU_BE */

	return pRxDesc;
}

/* Refill RX descriptor (when BM is not supported) */
static INLINE void mvNetaRxDescFill(NETA_RX_DESC *pRxDesc, MV_U32 physAddr, MV_U32 cookie)
{
	pRxDesc->bufCookie = (MV_U32)cookie;

	pRxDesc->bufPhysAddr = MV_32BIT_LE(physAddr);

	mvOsCacheLineFlush(NULL, pRxDesc);
}

/* Get pointer to next TX descriptor to be processed (send) by HW */
static INLINE NETA_TX_DESC *mvNetaTxqNextDescGet(MV_NETA_TXQ_CTRL *pTxq)
{
	int txDesc = pTxq->queueCtrl.nextToProc;

	pTxq->queueCtrl.nextToProc = MV_NETA_QUEUE_NEXT_DESC(&(pTxq->queueCtrl), txDesc);

	return ((NETA_TX_DESC *) pTxq->queueCtrl.pFirst) + txDesc;
}

/* Get pointer to previous TX descriptor in the ring for rollback when needed */
static INLINE NETA_TX_DESC *mvNetaTxqPrevDescGet(MV_NETA_TXQ_CTRL *pTxq)
{
	int txDesc = pTxq->queueCtrl.nextToProc;

	pTxq->queueCtrl.nextToProc = MV_NETA_QUEUE_PREV_DESC(&(pTxq->queueCtrl), txDesc);

	return ((NETA_TX_DESC *) pTxq->queueCtrl.pFirst) + txDesc;
}
static INLINE MV_ULONG netaDescVirtToPhys(MV_NETA_QUEUE_CTRL *pQueueCtrl, MV_U8 *pDesc)
{
	return (pQueueCtrl->descBuf.bufPhysAddr + (pDesc - pQueueCtrl->descBuf.bufVirtPtr));
}

/* Function prototypes */
MV_STATUS 	mvNetaHalInit(MV_NETA_HAL_DATA *halData);

MV_STATUS 	mvNetaWinInit(MV_U32 port, MV_UNIT_WIN_INFO *addrWinMap);
MV_STATUS 	mvNetaWinWrite(MV_U32 port, MV_U32 winNum, MV_UNIT_WIN_INFO *pAddrDecWin);
MV_STATUS 	mvNetaWinRead(MV_U32 port, MV_U32 winNum, MV_UNIT_WIN_INFO *pAddrDecWin);
MV_STATUS 	mvNetaWinEnable(MV_U32 port, MV_U32 winNum, MV_BOOL enable);

int 		mvNetaAccMode(void);
MV_STATUS 	mvNetaMemMapGet(MV_ULONG physAddr, MV_U8 *pTarget, MV_U8 *pAttr);

void 			*mvNetaPortInit(int port, void *osHandle);
MV_NETA_TXQ_CTRL 	*mvNetaTxqInit(int port, int txp, int queue, int descrNum);
MV_NETA_RXQ_CTRL 	*mvNetaRxqInit(int port, int queue, int descrNum);

void mvNetaRxReset(int port);
void mvNetaTxpReset(int port, int txp);

MV_STATUS	mvNetaPortDisable(int port);
MV_STATUS	mvNetaPortEnable(int port);
MV_STATUS	mvNetaPortUp(int port);
MV_STATUS	mvNetaPortDown(int port);

MV_BOOL		mvNetaLinkIsUp(int port);
MV_STATUS	mvNetaLinkStatus(int port, MV_ETH_PORT_STATUS *pStatus);
MV_STATUS	mvNetaDefaultsSet(int port);
MV_STATUS	mvNetaRxUnicastPromiscSet(int port, MV_BOOL isPromisc);
MV_STATUS	mvNetaForceLinkModeSet(int portNo, MV_BOOL force_link_pass, MV_BOOL force_link_fail);
MV_STATUS	mvNetaSpeedDuplexSet(int portNo, MV_ETH_PORT_SPEED speed, MV_ETH_PORT_DUPLEX duplex);

void mvNetaSetOtherMcastTable(int portNo, int queue);
void mvNetaSetUcastTable(int port, int queue);
void mvNetaSetSpecialMcastTable(int portNo, int queue);

MV_STATUS mvNetaMcastAddrSet(int port, MV_U8 *pAddr, int queue);
MV_STATUS mvNetaMacAddrGet(int portNo, unsigned char *pAddr);

void 		mvNetaPhyAddrSet(int port, int phyAddr);
int 		mvNetaPhyAddrGet(int port);

void 		mvNetaPortPowerDown(int port);
void 		mvNetaPortPowerUp(int port);

/* Interrupt Coalesting functions */
MV_STATUS mvNetaRxTimeCoalSet(int port, MV_U32 uSec);
MV_STATUS mvNetaRxPktsCoalSet(int port, int rxq, MV_U32 pkts);
MV_STATUS mvNetaTxDonePktsCoalSet(int port, int txp, int txq, MV_U32 pkts);
MV_U32 mvNetaRxTimeCoalGet(int port);
MV_U32 mvNetaRxPktsCoalGet(int port, int rxq);
MV_U32 mvNetaTxDonePktsCoalGet(int port, int txp, int txq);

MV_STATUS mvNetaRxqBufSizeSet(int port, int rxq, int bufSize);
MV_STATUS mvNetaMhSet(int port, MV_NETA_MH_MODE mh);
MV_STATUS mvNetaMaxRxSizeSet(int port, int maxRxSize);
MV_STATUS mvNetaMacAddrSet(int port, unsigned char *pAddr, int queue);

MV_STATUS mvNetaRxqOffsetSet(int port, int rxq, int offset);
MV_STATUS mvNetaBmPoolBufSizeSet(int port, int pool, int bufsize);
MV_STATUS mvNetaRxqBmEnable(int port, int rxq, int smallPool, int largePool);
MV_STATUS mvNetaRxqBmDisable(int port, int rxq);

MV_STATUS mvNetaTcpRxq(int port, int rxq);
MV_STATUS mvNetaUdpRxq(int port, int rxq);
MV_STATUS mvNetaArpRxq(int port, int rxq);
MV_STATUS mvNetaBpduRxq(int port, int rxq);

MV_STATUS mvNetaTxpEjpSet(int port, int txp, int enable);
MV_STATUS mvNetaTxqFixPrioSet(int port, int txp, int txq);
MV_STATUS mvNetaTxqWrrPrioSet(int port, int txp, int txq, int weight);
MV_STATUS mvNetaTxpMaxTxSizeSet(int port, int txp, int maxTxSize);
MV_STATUS mvNetaTxpRateSet(int port, int txp, int bw);
MV_STATUS mvNetaTxqRateSet(int port, int txp, int txq, int bw);
MV_STATUS mvNetaTxpBurstSet(int port, int txp, int burst);
MV_STATUS mvNetaTxqBurstSet(int port, int txp, int txq, int burst);
MV_STATUS mvNetaTxpEjpBurstRateSet(int port, int txp, int txq, int rate);
MV_STATUS mvNetaTxpEjpMaxPktSizeSet(int port, int txp, int type, int size);
MV_STATUS mvNetaTxpEjpTxSpeedSet(int port, int txp, int type, int speed);

int mvNetaPortCheck(int port);
int mvNetaTxpCheck(int port, int txp);
int mvNetaMaxCheck(int num, int limit);

void mvNetaMibCountersClear(int port, int txp);
MV_U32 mvNetaMibCounterRead(int port, int txp, unsigned int mibOffset, MV_U32 *pHigh32);

void mvEthPortRegs(int port);
void mvEthPortUcastShow(int port);
void mvEthPortMcastShow(int port);
void mvNetaPortRegs(int port);
void mvNetaPncRegs(void);
void mvNetaTxpRegs(int port, int txp);
void mvNetaRxqRegs(int port, int rxq);
void mvNetaTxqRegs(int port, int txp, int txq);
void mvNetaPortStatus(int port);
void mvNetaRxqShow(int port, int rxq, int mode);
void mvNetaTxqShow(int port, int txp, int txq, int mode);

void mvEthTxpWrrRegs(int port, int txp);
void mvEthRegs(int port);
void mvEthPortCounters(int port, int mib);
void mvEthPortRmonCounters(int port, int mib);

MV_STATUS mvNetaFlowCtrlSet(int port, MV_ETH_PORT_FC flowControl);
MV_STATUS mvNetaFlowCtrlGet(int port, MV_ETH_PORT_FC *flowControl);

#ifdef MV_ETH_GMAC_NEW
MV_STATUS       mvEthGmacRgmiiSet(int port, int enable);
MV_STATUS	mvNetaGmacLpiSet(int port, int mode);
void	mvNetaGmacRegs(int port);
#endif /* MV_ETH_GMAC_NEW */

#ifdef CONFIG_MV_PON
MV_STATUS   mvNetaPonRxMibDefault(int mib);
MV_STATUS   mvNetaPonRxMibGemPid(int mib, MV_U16 gemPid);
#endif /* CONFIG_MV_PON */

#ifdef CONFIG_MV_ETH_HWF
MV_STATUS mvNetaHwfInit(int port);
MV_STATUS mvNetaHwfEnable(int port, int enable);
MV_STATUS mvNetaHwfTxqInit(int p, int txp, int txq);
MV_STATUS mvNetaHwfTxqEnable(int port, int p, int txp, int txq, int enable);
MV_STATUS mvNetaHwfTxqDropSet(int port, int p, int txp, int txq, int thresh, int bits);
void mvNetaHwfRxpRegs(int port);
void mvNetaHwfTxpRegs(int port, int p, int txp);
void mvNetaHwfTxpCntrs(int port, int p, int txp);
#endif /* CONFIG_MV_ETH_HWF */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __mvNeta_h__ */
