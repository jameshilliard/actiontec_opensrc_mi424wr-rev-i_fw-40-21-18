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
* mvSysPciIfConfig.h - Marvell PCI / Pex units specific configurations
*
* DESCRIPTION:
*       None.
*
* DEPENDENCIES:
*       None.
*
*******************************************************************************/

#include "mvSysHwConfig.h"

/*
** Base address for Pex registers.
*/
#define MV_PEX_IF_REGS_BASE(unit) 		(MV_PEX_IF_REGS_OFFSET(unit))

/* PEX-PCI\PCI-PCI Bridge*/
#define PCI0_IF_PTP		0		/* no Bridge on pciIf0*/
#define PCI1_IF_PTP		0		/* no Bridge on pciIf1*/


#if defined (MV_INCLUDE_PEX)
#define PCI_IF0_MEM0_BASE 	PEX0_MEM_BASE
#define PCI_IF0_MEM0_SIZE 	PEX0_MEM_SIZE
#define PCI_IF0_IO_BASE 	PEX0_IO_BASE
#define PCI_IF0_IO_SIZE 	PEX0_IO_SIZE

#define PCI_IF1_MEM0_BASE 	PEX1_MEM_BASE
#define PCI_IF1_MEM0_SIZE 	PEX1_MEM_SIZE
#define PCI_IF1_IO_BASE 	PEX1_IO_BASE
#define PCI_IF1_IO_SIZE 	PEX1_IO_SIZE
#endif


/* PEX Work arround */
/* the target we will use for the workarround */
#define PEX_CONFIG_RW_WA_TARGET PEX0_MEM
/*a flag that indicates if we are going to use the 
size and base of the target we using for the workarround
window */
#define PEX_CONFIG_RW_WA_USE_ORIGINAL_WIN_VALUES 1
/* if the above flag is 0 then the following values
will be used for the workarround window base and size,
otherwise the following defines will be ignored */
#define PEX_CONFIG_RW_WA_BASE 0xF3000000
#define PEX_CONFIG_RW_WA_SIZE _16M

