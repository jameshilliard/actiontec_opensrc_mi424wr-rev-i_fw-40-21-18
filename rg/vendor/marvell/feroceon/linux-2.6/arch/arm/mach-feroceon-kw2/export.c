/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/types.h>
#include <linux/kernel.h>

#include "ctrlEnv/mvCtrlEnvLib.h"
#include "ctrlEnv/sys/mvCpuIf.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "mvDebug.h"
#include "mvSysHwConfig.h"
#include "pex/mvPexRegs.h"
#include "cntmr/mvCntmr.h"
#include "ctrlEnv/mvCtrlEnvLib.h"
#include "mvOs.h"
#include "eth-phy/mvEthPhy.h"

/*************************************************************************************************************
 * Environment 
 *************************************************************************************************************/
extern u32 mvTclk;
extern u32 mvSysclk;
#ifdef CONFIG_MV_INCLUDE_GIG_ETH
extern u8  mvMacAddr[CONFIG_MV_ETH_PORTS_NUM][6];
#endif

EXPORT_SYMBOL(mvEthPhyRegRead);
EXPORT_SYMBOL(mvEthPhyRegWrite);
EXPORT_SYMBOL(mv_early_printk);
EXPORT_SYMBOL(mvCtrlPwrClckGet);
EXPORT_SYMBOL(mvCtrlModelRevGet);
EXPORT_SYMBOL(mvTclk);
EXPORT_SYMBOL(mvSysclk);
EXPORT_SYMBOL(mvCtrlModelGet);
EXPORT_SYMBOL(mvOsIoUncachedMalloc);
EXPORT_SYMBOL(mvOsIoUncachedFree);
EXPORT_SYMBOL(mvOsIoCachedMalloc);
EXPORT_SYMBOL(mvOsIoCachedFree);
EXPORT_SYMBOL(mvDebugMemDump);
EXPORT_SYMBOL(mvHexToBin);
EXPORT_SYMBOL(mvBinToHex);
EXPORT_SYMBOL(mvSizePrint);
EXPORT_SYMBOL(mvDebugPrintMacAddr);
EXPORT_SYMBOL(mvCtrlEthMaxPortGet);
EXPORT_SYMBOL(mvCtrlTargetNameGet);
EXPORT_SYMBOL(mvBoardIdGet);
EXPORT_SYMBOL(mvBoardPhyAddrGet);
EXPORT_SYMBOL(mvCpuIfTargetWinGet);
EXPORT_SYMBOL(mvMacStrToHex);
EXPORT_SYMBOL(mvBoardTclkGet);
EXPORT_SYMBOL(mvBoardMacSpeedGet);
#ifdef CONFIG_MV_INCLUDE_TDM
EXPORT_SYMBOL(mvCtrlTdmUnitIrqGet);
#endif
EXPORT_SYMBOL(mvWinOverlapTest);
#ifdef CONFIG_MV_INCLUDE_TDM
EXPORT_SYMBOL(mvCtrlTdmUnitTypeGet);
#endif
EXPORT_SYMBOL(mvCtrlAddrWinMapBuild);
EXPORT_SYMBOL(mvBoardTdmSpiModeGet);
EXPORT_SYMBOL(mvBoardTdmSpiCsGet);
EXPORT_SYMBOL(mvBoardTdmDevicesCountGet);
#ifdef CONFIG_MV_INCLUDE_GIG_ETH
EXPORT_SYMBOL(mvMacAddr);
#endif

#ifdef CONFIG_MV_INCLUDE_SPI
#include "spi/mvSpiCmnd.h"
EXPORT_SYMBOL(mvSpiWriteThenWrite);
EXPORT_SYMBOL(mvSpiWriteThenRead);
#include "spi/mvSpi.h"
EXPORT_SYMBOL(mvSpiParamsSet);
#endif
#include "gpp/mvGpp.h"
EXPORT_SYMBOL(mvGppTypeSet);
EXPORT_SYMBOL(mvGppValueSet);
/*************************************************************************************************************
 * Audio
 *************************************************************************************************************/
#ifdef CONFIG_MV_INCLUDE_AUDIO
#include "audio/mvAudio.h"
#include "mvSysAudioApi.h"
EXPORT_SYMBOL(mvSPDIFRecordTclockSet);
EXPORT_SYMBOL(mvSPDIFPlaybackCtrlSet);
EXPORT_SYMBOL(mvI2SPlaybackCtrlSet);
EXPORT_SYMBOL(mvAudioPlaybackControlSet);
EXPORT_SYMBOL(mvAudioDCOCtrlSet);
EXPORT_SYMBOL(mvI2SRecordCntrlSet);
EXPORT_SYMBOL(mvAudioRecordControlSet);
EXPORT_SYMBOL(mvSysAudioInit);
EXPORT_SYMBOL(mvBoardA2DTwsiAddrGet);
EXPORT_SYMBOL(mvBoardA2DTwsiAddrTypeGet);
#endif

/*************************************************************************************************************
 * USB
 *************************************************************************************************************/
#ifdef CONFIG_MV_INCLUDE_USB
extern u32 mvIsUsbHost;

#include "usb/mvUsb.h"
EXPORT_SYMBOL(mvIsUsbHost);
EXPORT_SYMBOL(mvCtrlUsbMaxGet);
EXPORT_SYMBOL(mvUsbGetCapRegAddr);
#ifdef MV_USB_VOLTAGE_FIX
EXPORT_SYMBOL(mvUsbGppInit);
EXPORT_SYMBOL(mvUsbBackVoltageUpdate);
#endif
#endif /* CONFIG_MV_INCLUDE_USB */

/*************************************************************************************************************
 * CESA
 *************************************************************************************************************/
#ifdef CONFIG_MV_INCLUDE_CESA
#include "mvSysCesaApi.h"
#include "cesa/mvCesa.h"
#include "cesa/mvMD5.h"
#include "cesa/mvSHA1.h"
extern unsigned char*  mv_sram_usage_get(int* sram_size_ptr);

EXPORT_SYMBOL(mvSysCesaInit);
EXPORT_SYMBOL(mvCesaSessionOpen);
EXPORT_SYMBOL(mvCesaSessionClose);
EXPORT_SYMBOL(mvCesaAction);
EXPORT_SYMBOL(mvCesaReadyGet);
EXPORT_SYMBOL(mvCesaCopyFromMbuf);
EXPORT_SYMBOL(mvCesaCopyToMbuf);
EXPORT_SYMBOL(mvCesaMbufCopy);
EXPORT_SYMBOL(mvCesaCryptoIvSet);
EXPORT_SYMBOL(mvMD5);
EXPORT_SYMBOL(mvSHA1);

EXPORT_SYMBOL(mvCesaDebugQueue);
EXPORT_SYMBOL(mvCesaDebugSram);
EXPORT_SYMBOL(mvCesaDebugSAD);
EXPORT_SYMBOL(mvCesaDebugStatus);
EXPORT_SYMBOL(mvCesaDebugMbuf);
EXPORT_SYMBOL(mvCesaDebugSA);
EXPORT_SYMBOL(mv_sram_usage_get);

extern u32 mv_crypto_base_get(void);
EXPORT_SYMBOL(mv_crypto_base_get);
EXPORT_SYMBOL(cesaReqResources);
EXPORT_SYMBOL(mvCesaFinish);

#ifdef CONFIG_MV_CESA_OCF
extern void cesa_ocf_debug(void);
EXPORT_SYMBOL(cesa_ocf_debug);
#endif

#endif

/*************************************************************************************************************
 * Flashes
 *************************************************************************************************************/
#if defined (CONFIG_MV_INCLUDE_SPI)
#include <sflash/mvSFlash.h>
#include <sflash/mvSFlashSpec.h>
EXPORT_SYMBOL(mvSFlashInit);
EXPORT_SYMBOL(mvSFlashSectorErase);
EXPORT_SYMBOL(mvSFlashChipErase);
EXPORT_SYMBOL(mvSFlashBlockRd);
EXPORT_SYMBOL(mvSFlashBlockWr);
EXPORT_SYMBOL(mvSFlashIdGet);
EXPORT_SYMBOL(mvSFlashWpRegionSet);
EXPORT_SYMBOL(mvSFlashWpRegionGet);
EXPORT_SYMBOL(mvSFlashStatRegLock);
EXPORT_SYMBOL(mvSFlashSizeGet);
EXPORT_SYMBOL(mvSFlashPowerSaveEnter);
EXPORT_SYMBOL(mvSFlashPowerSaveExit);
EXPORT_SYMBOL(mvSFlashModelGet);
#endif


/*************************************************************************************************************
 * SATA
 *************************************************************************************************************/
#ifdef CONFIG_MV_INCLUDE_INTEG_SATA
#include <sata/CoreDriver/mvSata.h>
EXPORT_SYMBOL(mvSataWinInit);
#endif

/*************************************************************************************************************
 * DMA/XOR
 *************************************************************************************************************/
#if defined (CONFIG_MV_XOR_MEMCOPY) || defined (CONFIG_MV_IDMA_MEMCOPY)
EXPORT_SYMBOL(asm_memcpy);
#endif

#ifdef CONFIG_MV_SP_I_FTCH_DB_INV 
EXPORT_SYMBOL(mv_l2_inv_range);
#endif

/*************************************************************************************************************
 * Marvell TRACE
 *************************************************************************************************************/
#ifdef CONFIG_MV_DBG_TRACE
#include "dbg-trace.h"
EXPORT_SYMBOL(TRC_INIT);
EXPORT_SYMBOL(TRC_REC);
EXPORT_SYMBOL(TRC_OUTPUT);
EXPORT_SYMBOL(TRC_START);
EXPORT_SYMBOL(TRC_RELEASE);
#endif


/*************************************************************************************************************
 * PON
 *************************************************************************************************************/
#ifdef CONFIG_MV_PON
#include "net_dev/mv_netdev.h"
#endif /* CONFIG_MV_PON */
