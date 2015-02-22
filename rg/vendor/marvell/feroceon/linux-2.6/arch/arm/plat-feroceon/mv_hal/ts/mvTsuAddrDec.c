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
#include "ctrlEnv/mvCtrlEnvSpec.h"
#include "mvTsuRegs.h"
#include "mvTsu.h"

/* defines  */
#ifdef MV_DEBUG
	#define DB(x)	x
#else
	#define DB(x)
#endif	


static MV_STATUS tsuWinOverlapDetect(MV_U32 winNum, MV_ADDR_WIN *pAddrWin);

MV_TARGET tsuAddrDecPrioTap[] = 
{
#if defined(MV_INCLUDE_SDRAM_CS0)
    SDRAM_CS0,
#endif
#if defined(MV_INCLUDE_SDRAM_CS1)
    SDRAM_CS1,
#endif
#if defined(MV_INCLUDE_SDRAM_CS2)
    SDRAM_CS2,
#endif
#if defined(MV_INCLUDE_SDRAM_CS3)
    SDRAM_CS3,
#endif
    TBL_TERM
};

/*******************************************************************************
* mvTsuWinInit
*
* DESCRIPTION:
* 	Initialize the TSU unit address decode windows.
*
* INPUT:
*	addWinMap: An array holding the address decoding information for the
*		    system.
* OUTPUT:
*	None.
* RETURN:
*       MV_OK	- on success,
*
*******************************************************************************/
MV_STATUS mvTsuWinInit(MV_UNIT_WIN_INFO *addrWinMap)
{
	MV_U32		winNum;
	MV_UNIT_WIN_INFO	*addrDecWin;
	MV_U32		winPrioIndex=0;

	/* First disable all address decode windows */
	for(winNum = 0; winNum < TSU_MAX_DECODE_WIN; winNum++)
	{
		MV_REG_BIT_RESET(MV_TSU_WIN_CTRL_REG(winNum),
				 TSU_WIN_CTRL_EN_MASK);
	}

	/* Go through all windows in user table until table terminator      */
	for(winNum = 0; ((tsuAddrDecPrioTap[winPrioIndex] != TBL_TERM) &&
			 (winNum < TSU_MAX_DECODE_WIN));)
	{
		addrDecWin = &addrWinMap[tsuAddrDecPrioTap[winPrioIndex]];
		if (addrDecWin->enable == MV_TRUE) {
			if (MV_OK != mvTsuWinWrite(0, winNum, addrDecWin)) {
				DB(mvOsPrintf("mvTsuWinInit: ERR. mvTsuWinWrite failed\n"));
				return MV_ERROR;
			}
			winNum++;
		}
		winPrioIndex++;
	}

	return MV_OK;
}


/*******************************************************************************
* mvTsuWinWrite
*
* DESCRIPTION:
*       This function sets a peripheral target (e.g. SDRAM bank0, PCI_MEM0)
*       address window, also known as address decode window.
*       After setting this target window, the TSU will be able to access the
*       target within the address window.
*
* INPUT:
*	unit	    - The Unit ID.
*       winNum      - TSU to target address decode window number.
*       pAddrDecWin - TSU target window data structure.
*
* OUTPUT:
*       None.
*
* RETURN:
*       MV_ERROR	- if address window overlapps with other address decode 
*			windows.
*       MV_BAD_PARAM	- if base address is invalid parameter or target is
*       		unknown.
*
*******************************************************************************/
MV_STATUS mvTsuWinWrite(MV_U32 unit, MV_U32 winNum, MV_UNIT_WIN_INFO *pAddrDecWin)
{
	MV_U32  sizeReg, baseReg;

	/* Parameter checking   */
	if(winNum >= TSU_MAX_DECODE_WIN)
	{
		mvOsPrintf("mvTsuWinSet: ERR. Invalid win num %d\n",winNum);
		return MV_BAD_PARAM;
	}

	/* Check if the requested window overlapps with current windows     */
	if(MV_TRUE == tsuWinOverlapDetect(winNum, &pAddrDecWin->addrWin))
	{
		mvOsPrintf("mvTsuWinSet: ERR. Window %d overlap\n", winNum);
		return MV_ERROR;
	}

	/* check if address is aligned to the size */
	if(MV_IS_NOT_ALIGN(pAddrDecWin->addrWin.baseLow,pAddrDecWin->addrWin.size))
	{
		mvOsPrintf("mvTsuWinSet: Error setting TSU window %d.\n"
			   "Address 0x%08x is unaligned to size 0x%x.\n",
			   winNum,
			   pAddrDecWin->addrWin.baseLow,
			   pAddrDecWin->addrWin.size);
		return MV_ERROR;
	}

	baseReg = pAddrDecWin->addrWin.baseLow & TSU_WIN_BASE_MASK;
	sizeReg = (pAddrDecWin->addrWin.size / TSU_WIN_SIZE_ALIGN) - 1;
	sizeReg = (sizeReg << TSU_WIN_CTRL_SIZE_OFFS) & TSU_WIN_CTRL_SIZE_MASK;

	/* set attributes */
	baseReg &= ~TSU_WIN_CTRL_ATTR_MASK;
	baseReg |= pAddrDecWin->attrib << TSU_WIN_CTRL_ATTR_OFFS;
	/* set target ID */
	baseReg &= ~TSU_WIN_CTRL_TARGET_MASK;
	baseReg |= pAddrDecWin->targetId << TSU_WIN_CTRL_TARGET_OFFS;

	/* for the safe side we disable the window before writing the new */
	/* values */
	mvTsuWinEnable(winNum, MV_FALSE);
	MV_REG_WRITE(MV_TSU_WIN_CTRL_REG(winNum), sizeReg);

	/* Write to address decode Size Register                            */
	MV_REG_WRITE(MV_TSU_WIN_BASE_REG(winNum), baseReg);

	/* Enable address decode target window                              */
	if(pAddrDecWin->enable == MV_TRUE)
	{
		mvTsuWinEnable(winNum,MV_TRUE);
	}

	return MV_OK;
}

/*******************************************************************************
* mvTsuWinRead
*
* DESCRIPTION:
*	Get TSU peripheral target address window.
*
* INPUT:
*	unit   - The Unit ID.
*	winNum - TSU to target address decode window number.
*
* OUTPUT:
*       pAddrDecWin - TSU target window data structure.
*
* RETURN:
*       MV_ERROR if register parameters are invalid.
*
*******************************************************************************/
MV_STATUS mvTsuWinRead(MV_U32 unit, MV_U32 winNum, MV_UNIT_WIN_INFO *pAddrDecWin)
{
	MV_U32 baseReg, sizeReg;

	/* Parameter checking   */
	if(winNum >= TSU_MAX_DECODE_WIN)
	{
		mvOsPrintf("mvTsuWinGet: ERR. Invalid winNum %d\n", winNum);
		return MV_NOT_SUPPORTED;
	}

	baseReg = MV_REG_READ(MV_TSU_WIN_BASE_REG(winNum));                                                                           
	sizeReg = MV_REG_READ(MV_TSU_WIN_CTRL_REG(winNum));

	pAddrDecWin->addrWin.size = (sizeReg & TSU_WIN_CTRL_SIZE_MASK) >> TSU_WIN_CTRL_SIZE_OFFS;
	pAddrDecWin->addrWin.size = (pAddrDecWin->addrWin.size + 1) * TSU_WIN_SIZE_ALIGN;

	pAddrDecWin->addrWin.baseLow = baseReg & TSU_WIN_BASE_MASK;
	pAddrDecWin->addrWin.baseHigh =  0;

	/* attrib and targetId */
	pAddrDecWin->attrib = 
		(sizeReg & TSU_WIN_CTRL_ATTR_MASK) >> TSU_WIN_CTRL_ATTR_OFFS;
	pAddrDecWin->targetId = 
		(sizeReg & TSU_WIN_CTRL_TARGET_MASK) >> TSU_WIN_CTRL_TARGET_OFFS;

	/* Check if window is enabled   */
	if((MV_REG_READ(MV_TSU_WIN_CTRL_REG(winNum)) & TSU_WIN_CTRL_EN_MASK))
		pAddrDecWin->enable = MV_TRUE;
	else
		pAddrDecWin->enable = MV_FALSE;

	return MV_OK;
}


/*******************************************************************************
* mvTsuWinEnable - Enable/disable a TS address decode window
*
* DESCRIPTION:
*       This function enable/disable a TS address decode window.
*
* INPUT:
*       winNum - Decode window number.
*       enable - Enable/disable parameter.
*
* OUTPUT:
*       None.
*
* RETURN:
*       MV_BAD_PARAM if parameters to function invalid, MV_OK otherwise.
*
*******************************************************************************/
MV_STATUS mvTsuWinEnable(MV_U32 winNum, MV_BOOL enable)
{
    MV_UNIT_WIN_INFO	addrDecWin;

    /* Parameter checking   */               
    if (winNum >= TSU_MAX_DECODE_WIN) {
        DB(mvOsPrintf("%s: ERR. Invalid winNum%d\n", __FUNCTION__, winNum));
        return MV_ERROR;
    }

    if (enable == MV_TRUE) {
	/* Get current window */
	if (MV_OK != mvTsuWinRead(0, winNum, &addrDecWin)) {
	    DB(mvOsPrintf("%s: ERR. targetWinGet fail\n", __FUNCTION__));
	    return MV_ERROR;
	}

	/* Check for overlapping */
	if (MV_TRUE == tsuWinOverlapDetect(winNum, &(addrDecWin.addrWin))) {
	    /* Overlap detected	*/
	    DB(mvOsPrintf("%s: ERR. Overlap detected\n", __FUNCTION__));
	    return MV_ERROR;
	}

	/* No Overlap. Enable address decode target window */
	MV_REG_BIT_SET(MV_TSU_WIN_CTRL_REG(winNum), TSU_WIN_CTRL_EN_MASK);
    } else {
	/* Disable address decode target window */
	MV_REG_BIT_RESET(MV_TSU_WIN_CTRL_REG(winNum), TSU_WIN_CTRL_EN_MASK);
    }

    return MV_OK;                  
}

/*******************************************************************************
* tsuWinOverlapDetect - Detect TS address windows overlaping
*
* DESCRIPTION:
*       An unpredicted behaviour is expected in case TS address decode 
*       windows overlaps.
*       This function detects TS address decode windows overlaping of a 
*       specified window. The function does not check the window itself for 
*       overlaping. The function also skipps disabled address decode windows.
*
* INPUT:
*       winNum      - address decode window number.
*       pAddrDecWin - An address decode window struct.
*
* OUTPUT:
*       None.
*
* RETURN:
*       MV_TRUE if the given address window overlap current address
*       decode map, MV_FALSE otherwise, MV_ERROR if reading invalid data 
*       from registers.
*
*******************************************************************************/
static MV_STATUS tsuWinOverlapDetect(MV_U32 winNum, MV_ADDR_WIN *pAddrWin)
{
    MV_U32		baseAddrEnableReg;
    MV_U32		winNumIndex;
    MV_UNIT_WIN_INFO	addrDecWin;

    if (pAddrWin == NULL) {
	DB(mvOsPrintf("%s: ERR. pAddrWin is NULL pointer\n", __FUNCTION__ ));
	return MV_BAD_PTR;
    }

    for (winNumIndex = 0; winNumIndex < TSU_MAX_DECODE_WIN; winNumIndex++) {
	/* Do not check window itself */
	if (winNumIndex == winNum)
	    continue;

	/* Read base address enable register. Do not check disabled windows	*/
	baseAddrEnableReg = MV_REG_READ(MV_TSU_WIN_CTRL_REG(winNumIndex));

	/* Do not check disabled windows */
	if ((baseAddrEnableReg & TSU_WIN_CTRL_EN_MASK) == 0)
	    continue;

	/* Get window parameters */
	if (MV_OK != mvTsuWinRead(0, winNumIndex, &addrDecWin)) {
	    DB(mvOsPrintf("%s: ERR. TargetWinGet failed\n", __FUNCTION__ ));
	    return MV_ERROR;
	}

	if (MV_TRUE == mvWinOverlapTest(pAddrWin, &(addrDecWin.addrWin)))
	    return MV_TRUE;
    }

    return MV_FALSE;
}

