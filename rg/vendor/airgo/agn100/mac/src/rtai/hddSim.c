/*
 * Project: rtai_cpp - RTAI C++ Framework 
 *
 * File: $Id: hddSim.c,v 1.1.1.1 2007/05/07 23:33:58 jungo Exp $
 *
 * Copyright: (C) 2001,2002 Erwin Rol <erwin@muffin.org>
 *
 * Licence:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <linux/module.h>
#include <asm/unistd.h>

#include <rtai.h>
#include <rt_mem_mgr.h>
#include <rtai_sched.h>
#include "hddSim.h"
#include "../../export/aniParam.h"

#define OPENPIC_VEC_TIMER     64 

extern unsigned char BinImage[];
extern int BinImageSize;


struct macStruct
{
    int       up;
    int       started;
    int       txCount;
    int       rxCount;
    void*   (*pAlloc)(unsigned short);
    void    (*pFree)(void*);
    MBX      *pTxMbx;
    MBX      *pRxMbx;
};

struct msgStruct
{
    unsigned short   type;
    unsigned short   msgLen;
    unsigned int     body;
};

struct dbgMsgStruct
{
    unsigned short   type;
    unsigned short   msgLen;
    unsigned long    timeStamp;
    unsigned long    param1;
    unsigned long    param2;
    unsigned long    param3;
    unsigned long    param4;
    unsigned long    param5;
    unsigned long    param6;
    unsigned long    param7;
    unsigned long    param8;
    unsigned long    strLen;
};


#define MAX_MAC_NUM       2
struct macStruct mac[MAX_MAC_NUM];

static RT_TASK   hddTask;

tAniMacParam   macParam[MAX_MAC_NUM];


/*-------------------------------------------------------------------------*/
/* Send message to MAC                                                     */
/*-------------------------------------------------------------------------*/
static
void sendMsg(struct msgStruct *pMsg, int radioId)
{
    int retVal;

    rtai_print_to_screen("<HDD> Send MsgType = 0x%04x to MAC%d\n", pMsg->type,
                         radioId);

    retVal = rt_mbx_send(mac[radioId].pTxMbx, (void*)&pMsg, 4);
    if (retVal != 0)
    {
        rtai_print_to_screen("<HDD> Can't send Msg to ");
        rtai_print_to_screen("MAC%d\n",radioId);
        (mac[radioId].pFree)((void*)pMsg); 
    }
}


void macSetup(int index) 
{
    tAniMacParam      *pMac;

    if (index < MAX_MAC_NUM)
    {
        rtai_print_to_screen("<HDD> MAC%d just comes up\n", index); 
        pMac = &macParam[index];
        mac[index].up = 1;
        mac[index].pAlloc = pMac->pMsgBufAlloc;
        mac[index].pFree  = pMac->pMsgBufFree;
        mac[index].pTxMbx = (MBX*)pMac->pTxMailbox;
        mac[index].pRxMbx = (MBX*)pMac->pRxMailbox;
    }
    else
        rtai_print_to_screen("<HDD> Invalid radio ID\n");

}

void macStart(int radioId)
{
    struct msgStruct  *pMsg;

    rtai_print_to_screen("<HDD> Starting MAC%d\n", radioId); 
    pMsg = (struct msgStruct*)(mac[radioId].pAlloc)(sizeof(struct msgStruct));
    if (pMsg == 0)
    {
        rtai_print_to_screen("<HDD> Can't get buffer from MAC%d\n", radioId);
        return;
    }
    pMsg->type = MAC_NIM_MSG + 0xb4;
    pMsg->msgLen  = 0;
    rtai_print_to_screen("<HDD> Send START_IND to MAC%d\n", radioId); 
    mac[radioId].started = 1;
    sendMsg(pMsg, radioId);
}


/*-------------------------------------------------------------------------*/
/* Get station statistic                                                   */
/*-------------------------------------------------------------------------*/
static
void getStaStat(int radioId, int staId)
{
    struct msgStruct  *pNewMsg;

    rtai_print_to_screen("<HDD> Get statistics for StaId = %d ", staId);
    rtai_print_to_screen("on MAC%d\n", radioId);

    pNewMsg = (struct msgStruct*) (mac[radioId].pAlloc) 
                                  (sizeof(struct msgStruct));
    if (pNewMsg == 0)
    {
        rtai_print_to_screen("<HDD> Can't get buffer from MAC%d\n", radioId);
        return;
    }
    pNewMsg->type = WNI_CFG_GET_PER_STA_STAT_REQ;
    pNewMsg->msgLen = WNI_CFG_GET_PER_STA_STAT_REQ_LEN; 
    pNewMsg->body = staId;

    sendMsg(pNewMsg, radioId);
}


/*-------------------------------------------------------------------------*/
/* Process NIM messages                                                    */
/*-------------------------------------------------------------------------*/
static
void processNimMsg(struct msgStruct *pMsg, int radioId)
{
    rtai_print_to_screen("      MODULE:  NIM\n");
    rtai_print_to_screen("      CONTENT: ");
    switch (pMsg->type)
    {
        case (MAC_NIM_MSG + 0xb1):
            rtai_print_to_screen("SIR_NIM_APP_SETUP_NTF\n");
            break;

        default:
            rtai_print_to_screen("Unknown message type\n");
    }
}


/*-------------------------------------------------------------------------*/
/* Process CFG messages                                                    */
/*-------------------------------------------------------------------------*/
static
void processCfgMsg(struct msgStruct *pMsg, int radioId)
{
    unsigned char       *pStart, *pSrc, *pEnd;
    struct msgStruct    *pNewMsg;
    unsigned long       *pParam;

    rtai_print_to_screen("      MODULE:  CFG\n");
    rtai_print_to_screen("      CONTENT: ");

    pParam = (unsigned long*)&pMsg->body;

    switch (pMsg->type)
    {
        case (MAC_CFG_MSG + 0x00):
            rtai_print_to_screen("WNI_CFG_PARAM_UPDATE_IND\n");
            break;

        case (MAC_CFG_MSG + 0x01):
            rtai_print_to_screen("WNI_CFG_DNLD_REQ\n");
            pNewMsg = (struct msgStruct*)
                      (mac[radioId].pAlloc)(BinImageSize + 
                                            sizeof(struct msgStruct));
            if (pNewMsg == 0)
            {
                rtai_print_to_screen("<HDD> Can't get buffer from Mac%d\n",
                                     radioId);
                return;
            }
            pNewMsg->type = WNI_CFG_DNLD_RSP;
            pNewMsg->msgLen = BinImageSize + sizeof(struct msgStruct); 
            pNewMsg->body = BinImageSize;
            pStart = (unsigned char*)pNewMsg + sizeof(struct msgStruct);
            pEnd   = pStart + BinImageSize;
            pSrc   = BinImage;

            while (pStart < pEnd)
                *pStart++ = *pSrc++;

            sendMsg(pNewMsg, radioId);
            rtai_print_to_screen("<HDD> Download CFG binary to MAC%d\n",
                                 radioId);
            break;

        case (MAC_CFG_MSG + 0x02):
            rtai_print_to_screen("WNI_CFG_DNLD_CNF\n");
            break;

        case (MAC_CFG_MSG + 0x03):
            rtai_print_to_screen("WNI_CFG_GET_RSP\n");
            break;

        case (MAC_CFG_MSG + 0x04):
            rtai_print_to_screen("WNI_CFG_SET_CNF\n");
            break;

        case (MAC_CFG_MSG + 0x05):
            rtai_print_to_screen("WNI_CFG_GET_ATTRIB_RSP\n");
            break;

        case (WNI_CFG_GET_PER_STA_STAT_RSP):
            rtai_print_to_screen("WNI_CFG_GET_PER_STA_STAT_RSP\n");
            rtai_print_to_screen("      Result            = %d\n", pParam[0]);
            rtai_print_to_screen("      Station Id        = %d\n", pParam[1]);
            rtai_print_to_screen("      Sent AES Ucast Hi = %d\n", pParam[2]);
            rtai_print_to_screen("      Sent AES Ucast Lo = %d\n", pParam[3]);
            rtai_print_to_screen("      Rcvd AES Ucast Hi = %d\n", pParam[4]);
            rtai_print_to_screen("      Rcvd AES Ucast Lo = %d\n", pParam[5]);
            rtai_print_to_screen("      AES Format Err    = %d\n", pParam[6]);
            rtai_print_to_screen("      AES Format Retry  = %d\n", pParam[7]);
            rtai_print_to_screen("      AES Decrypt       = %d\n", pParam[8]);
            break;

        default:
            rtai_print_to_screen("Unknown message type\n");
    }
}


/*-------------------------------------------------------------------------*/
/* Process DBG messages                                                    */
/*-------------------------------------------------------------------------*/
static
void processDbgMsg(struct dbgMsgStruct *pMsg, int radioId)
{
    char    *pChar;

    pChar = (char*)pMsg + sizeof(struct dbgMsgStruct);

    switch (pMsg->type & 0x00FF)
    {
        case MAC_NIM_ID: 
            rtai_print_to_screen("<NIM-DBG>");
            break;

        case MAC_CFG_ID:
            rtai_print_to_screen("<CFG-DBG>");
            break;

        case MAC_MNT_ID:
            rtai_print_to_screen("<MNT-MNT>");
            break;

        case MAC_SYS_ID:
            rtai_print_to_screen("<SYS-DBG>");
            break;

        default:
            rtai_print_to_screen("<XXX-DBG>");
    }

    rtai_print_to_screen(" Timestamp = %u\n", pMsg->timeStamp);
    rtai_print_to_screen("          ");
    rtai_print_to_screen(pChar, pMsg->param1, pMsg->param2, pMsg->param3, 
                         pMsg->param4, pMsg->param5, pMsg->param6, 
                         pMsg->param7, pMsg->param8);
}


/*-------------------------------------------------------------------------*/
/* Process MAC messages                                                    */
/*-------------------------------------------------------------------------*/
static
void processMacMsg(struct msgStruct *pMsg, int radioId)
{

    if ((pMsg->type & 0xFF00) == MAC_DBG_MSG)
    {
        rtai_print_to_screen("[MAC%d] ", radioId);
        processDbgMsg((struct dbgMsgStruct*)pMsg, radioId);
    }
    else
    {
        rtai_print_to_screen("<HDD> Message received from MAC%d @ %u", 
                             radioId, (unsigned long)rt_get_time());
        rtai_print_to_screen(" ; RX Msg total = %d\n", ++mac[radioId].rxCount);
        rtai_print_to_screen("      MsgPtr = 0x%08x", (unsigned int)pMsg);
        rtai_print_to_screen(" ; type = 0x%04x", pMsg->type);
        rtai_print_to_screen(" ; length = 0x%04x\n", pMsg->msgLen);

        switch (pMsg->type & 0xFF00)
        {
            // NIM messages
            case MAC_NIM_MSG:
                processNimMsg(pMsg, radioId);
                break;

            // CFG messages
            case MAC_CFG_MSG:
                processCfgMsg(pMsg, radioId);
                break;

            default:
                rtai_print_to_screen("      Unknown message\n");
        }
    }
}


void* hddSkBufAlloc1(void)
{
    rtai_print_to_screen("<HDD> SK-buffer allocation from MAC1\n");
    return 0;
}


void hddSkBufFree1(void* pSkBuf)
{
    rtai_print_to_screen("<HDD> SK-buffer free from MAC1\n");
}


void* hddSkBufAlloc2(void)
{
    rtai_print_to_screen("<HDD> SK-buffer allocation from MAC2\n");
    return 0;
}


void hddSkBufFree2(void* pSkBuf)
{
    rtai_print_to_screen("<HDD> SK-buffer free from MAC2\n");
}


/*-------------------------------------------------------------------------*/
/* HDD main function                                                       */
/*-------------------------------------------------------------------------*/
void hddMain(int param)
{
    int    i, retVal;
    short  type;
    struct msgStruct  *pMsg;
    RTIME  time[2], now[2];

    static int    staId[2] = {1, 1};

    rtai_print_to_screen("<HDD> Starting...\n");
    now[0]  = 0;
    now[1]  = 0;
    time[0] = 0;
    time[1] = 0;

    while (1)
    {
        for (i = 0; i < MAX_MAC_NUM; i++)
        {
            // For each MAC that is operational do the following:
            //
            //   - Check message queue and process all messages
            //   - Get station statistics periodically
            //   - More test cases can be added ...
            //
       
            if ((mac[i].up) && (mac[i].started))
            { 
                if (!time[i])
                    time[i] = rt_get_time_ns();

                //
                // Check for message from MAC
                //
                retVal = 0;
                while (!retVal)
                {
                    retVal = rt_mbx_receive_if(mac[i].pRxMbx, (void*)&pMsg, 4);
                    if (retVal == 0)
                    {
                        // Process all messages
                        processMacMsg(pMsg, i);
                        type = pMsg->type;
                        (mac[i].pFree)((void*)pMsg); 
                    }
                    else if (retVal < 0)
                    {
                        rtai_print_to_screen("<HDD> Can't get Msg ");
                        rtai_print_to_screen("from MAC%d\n", i);
                    }
                }

                //
                // Get station statistics every 5 min
                //
                now[i] = rt_get_time_ns();
                if ((now[i] - time[i]) >= 6000000000 * 1)
                {
                    getStaStat(i, staId[i]);
                    if (++staId[i] >= 256)
                        staId[i] = 1;
                    time[i] = now[i];
                }
            }
            else 
            {
                if (macParam[i].pMsgBufFree != 0)
                {
                    macSetup(i);
                    macStart(i);
                }
            }

            //
            // We need to sleep between iterations so that the console task
            // can run.  Otherwise, the console will appear to be locked.
            //
            rt_sleep(7000);
        }
    }  
}


void init_module(void)
{
    int    clockTicks;

    rt_mount_rtai();

    rt_mem_init();
    rt_mmgr_stats();
	rtai_print_to_screen("HDD started...\n");
    rtai_print_to_screen("<HDD> MAC0 control pointer = 0x%08x\n", 
                         (unsigned int)&macParam[0]);
    rtai_print_to_screen("<HDD> MAC1 control pointer = 0x%08x\n", 
                         (unsigned int)&macParam[1]);

    mac[0].up = 0;
    mac[0].started = 0;
    mac[0].txCount = 0;
    mac[0].rxCount = 0;
    mac[1].up = 0;
    mac[1].started = 0;
    mac[1].txCount = 0;
    mac[1].rxCount = 0;

    // Initialize MAC parameters
    macParam[0].radioId = 0;
    macParam[0].pPacketBufAlloc  = hddSkBufAlloc1;
    macParam[0].pPacketBufFree   = hddSkBufFree1;
    macParam[0].pMsgBufAlloc = 0;
    macParam[0].pMsgBufFree  = 0;

    macParam[1].radioId = 1;
    macParam[1].pPacketBufAlloc  = hddSkBufAlloc2;
    macParam[1].pPacketBufFree   = hddSkBufFree2;
    macParam[1].pMsgBufAlloc = 0;
    macParam[1].pMsgBufFree  = 0;

    rt_task_init(&hddTask, hddMain, 0, 2000, 41, 0, 0);
    rt_task_resume(&hddTask);
}

void cleanup_module(void)
{
	rtai_print_to_screen("Cleaning HDD\n");
    rt_task_delete(&hddTask);
    rt_mem_end();
    rt_umount_rtai();
}

 

 
