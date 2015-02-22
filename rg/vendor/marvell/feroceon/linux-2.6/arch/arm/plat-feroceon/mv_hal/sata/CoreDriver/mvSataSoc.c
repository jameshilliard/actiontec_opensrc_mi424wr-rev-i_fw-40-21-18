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

#include "mvSataSoc.h"
#include "mvRegs.h"

/* Calculate the base address of the registers for a SATA channel */
static MV_U32 edmaRegOffst[8] = { 0x22000, 0x24000, 0x26000, 0x28000,
	0x32000, 0x34000, 0x36000, 0x38000
};

#define getEdmaRegOffset(x) edmaRegOffst[(x)]

MV_BOOL mvSataPhyShutdown(MV_U8 port)
{
	MV_U32 regVal;
	MV_U32 adapterIoBaseAddress = MV_SATA_REGS_OFFSET - 0x20000;

	regVal = MV_REG_READ(adapterIoBaseAddress + getEdmaRegOffset(port) + MV_SATA_II_SATA_CONFIG_REG_OFFSET);
	/* Fix for 88SX60x1 FEr SATA#8 */
	/* according to the spec, bits [31:12] must be set to 0x009B1 */
	regVal &= 0x00000FFF;
	/* regVal |= MV_BIT12; */
	regVal |= 0x009B1000;

	regVal |= BIT9;
	MV_REG_WRITE(adapterIoBaseAddress + getEdmaRegOffset(port) + MV_SATA_II_SATA_CONFIG_REG_OFFSET, regVal);
	return MV_TRUE;
}

MV_BOOL mvSataPhyPowerOn(MV_U8 port)
{
	MV_U32 adapterIoBaseAddress = MV_SATA_REGS_OFFSET - 0x20000;

	MV_U32 regVal = MV_REG_READ(adapterIoBaseAddress + getEdmaRegOffset(port) + MV_SATA_II_SATA_CONFIG_REG_OFFSET);
	/* Fix for 88SX60x1 FEr SATA#8 */
	/* according to the spec, bits [31:12] must be set to 0x009B1 */
	regVal &= 0x00000FFF;
	/* regVal |= MV_BIT12; */
	regVal |= 0x009B1000;

	regVal &= ~(BIT9);
	MV_REG_WRITE(adapterIoBaseAddress + getEdmaRegOffset(port) + MV_SATA_II_SATA_CONFIG_REG_OFFSET, regVal);
	return MV_TRUE;
}
