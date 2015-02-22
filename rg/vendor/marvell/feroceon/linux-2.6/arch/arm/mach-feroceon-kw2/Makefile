#*******************************************************************************
# Marvell GPL License Option
#
# If you received this File from Marvell, you may opt to use, redistribute and/or 
# modify this File in accordance with the terms and conditions of the General 
# Public License Version 2, June 1991 (the "GPL License"), a copy of which is 
# available along with the File in the license.txt file or by writing to the Free 
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or 
# on the worldwide web at http://www.gnu.org/licenses/gpl.txt. 
#
# THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY 
# DISCLAIMED.  The GPL License provides additional details about this warranty 
# disclaimer.
#*******************************************************************************/
TOPDIR=$(shell pwd)
include 	  $(TOPDIR)/arch/arm/mach-feroceon-kw2/config/mvRules.mk

# Objects list
COMMON_OBJS	= $(COMMON_DIR)/mvDebug.o $(COMMON_DIR)/mvCommon.o $(COMMON_DIR)/mvStack.o $(COMMON_DIR)/mvList.o

OSSERVICES_OBJS	= $(OSSERV_DIR)/mvOs.o

HAL_OBJS	= $(HAL_RTC_DIR)/mvRtc.o  $(HAL_DRAM_SPD_DIR)/mvSpd.o		\
                  $(HAL_CNTMR_DIR)/mvCntmr.o					\
		  $(HAL_TWSI_DIR)/mvTwsi.o					\
                  $(HAL_UART_DIR)/mvUart.o $(HAL_GPP_DIR)/mvGpp.o               \
                  $(HAL_DRAM_DIR)/mvDramIf.o  					\
		  $(HAL_IF_DIR)/mvSysDdr.o

KW_FAM_OBJS	= $(BOARD_ENV_DIR)/mvBoardEnvSpec.o $(SOC_ENV_DIR)/mvCtrlEnvLib.o	\
		  $(BOARD_ENV_DIR)/mvBoardEnvLib.o $(SOC_ENV_DIR)/mvCtrlEnvAddrDec.o 	\
		  $(SOC_SYS_DIR)/mvAhbToMbus.o $(SOC_SYS_DIR)/mvCpuIf.o 		\
		  $(SOC_CPU_DIR)/mvCpu.o $(SOC_DEVICE_DIR)/mvDevice.o			\
		  $(SOC_ENV_DIR)/mvCtrlEthCompLib.o
                  
QD_OBJS		= $(HAL_QD_DIR)/src/driver/gtDrvConfig.o $(HAL_QD_DIR)/src/driver/gtDrvEvents.o \
                  $(HAL_QD_DIR)/src/driver/gtHwCntl.o $(HAL_QD_DIR)/src/platform/gtMiiSmiIf.o	\
                  $(HAL_QD_DIR)/src/platform/platformDeps.o $(HAL_QD_DIR)/src/platform/gtSem.o	\
                  $(HAL_QD_DIR)/src/platform/gtDebug.o $(HAL_QD_DIR)/src/msapi/gtBrgFdb.o 	\
                  $(HAL_QD_DIR)/src/msapi/gtBrgStp.o $(HAL_QD_DIR)/src/msapi/gtBrgVlan.o 	\
                  $(HAL_QD_DIR)/src/msapi/gtEvents.o $(HAL_QD_DIR)/src/msapi/gtPortCtrl.o 	\
                  $(HAL_QD_DIR)/src/msapi/gtPortStat.o $(HAL_QD_DIR)/src/msapi/gtPortStatus.o 	\
                  $(HAL_QD_DIR)/src/msapi/gtQosMap.o $(HAL_QD_DIR)/src/msapi/gtPIRL.o  		\
                  $(HAL_QD_DIR)/src/msapi/gtPhyCtrl.o $(HAL_QD_DIR)/src/msapi/gtPhyInt.o 	\
                  $(HAL_QD_DIR)/src/msapi/gtSysConfig.o $(HAL_QD_DIR)/src/msapi/gtSysCtrl.o	\
                  $(HAL_QD_DIR)/src/msapi/gtVersion.o $(HAL_QD_DIR)/src/msapi/gtUtils.o 	\
                  $(HAL_QD_DIR)/src/msapi/gtBrgVtu.o $(HAL_QD_DIR)/src/msapi/gtPortRmon.o 	\
                  $(HAL_QD_DIR)/src/msapi/gtSysStatus.o $(HAL_QD_DIR)/src/msapi/gtPortRateCtrl.o\
                  $(HAL_QD_DIR)/src/msapi/gtPortPav.o $(HAL_QD_DIR)/src/msapi/gtVct.o		\
		  $(HAL_QD_DIR)/src/msapi/gtPIRL2.o $(HAL_QD_DIR)/src/msapi/gtCCPVT.o		\
		  $(HAL_QD_DIR)/src/msapi/gtPCSCtrl.o

NOR_OBJS	= $(HAL_NOR_DIR)/mvAmdFlash.o $(HAL_NOR_DIR)/mvFlash.o				\
		  $(HAL_NOR_DIR)/mvFlashCom.o $(HAL_NOR_DIR)/mvIntelFlash.o

LSP_OBJS        = $(LSP_DIR)/core.o $(LSP_DIR)/irq.o $(LSP_DIR)/time.o  	\
                  $(LSP_DIR)/leds.o $(LSP_DIR)/sysmap.o  $(LSP_DIR)/rtc.o 	\
		  $(LSP_DIR)/export.o

obj-y   				:=  feroceon.o
feroceon-objs  				:=$(LSP_OBJS) $(COMMON_OBJS) $(OSSERVICES_OBJS) $(HAL_OBJS) 	\
					  $(KW_FAM_OBJS) $(NOR_OBJS)

feroceon-$(CONFIG_MV_INCLUDE_SDIO) 	+= $(HAL_SDMMC_DIR)/mvSdmmcAddrDec.o
feroceon-$(CONFIG_MV_INCLUDE_XOR) 	+= $(HAL_XOR_DIR)/mvXor.o $(HAL_XOR_DIR)/mvXorAddrDec.o		\
					   $(HAL_IF_DIR)/mvSysXor.o
feroceon-$(CONFIG_MV_INCLUDE_PEX) 	+= $(HAL_PEX_DIR)/mvPex.o					\
					   $(HAL_IF_DIR)/mvSysPex.o $(HAL_PEX_DIR)/mvPexAddrDec.o
feroceon-$(CONFIG_MV_INCLUDE_USB) 	+= $(HAL_USB_DIR)/mvUsb.o $(HAL_USB_DIR)/mvUsbAddrDec.o		\
					   $(HAL_IF_DIR)/mvSysUsb.o
feroceon-y				+= $(HAL_ETHPHY_DIR)/mvEthPhy.o $(HAL_IF_DIR)/mvSysEthPhy.o

# Legacy Giga driver
ifeq ($(CONFIG_MV_ETH_LEGACY),y) 
feroceon-$(CONFIG_MV_ETH_LEGACY)	+= $(HAL_ETH_GBE_DIR)/mvEth.o $(HAL_ETH_GBE_DIR)/mvEthDebug.o \
					   $(HAL_ETH_GBE_DIR)/mvEthAddrDec.o $(HAL_IF_DIR)/mvSysEth.o
feroceon-$(CONFIG_MV_ETH_NFP)           += $(HAL_ETH_NFP_DIR)/mvNfp.o 
feroceon-$(CONFIG_MV_ETH_NFP_NAT) 	+= $(HAL_ETH_NFP_DIR)/mvNfpNat.o
feroceon-$(CONFIG_MV_ETH_NFP_FDB) 	+= $(HAL_ETH_NFP_DIR)/mvNfpFdb.o  
feroceon-$(CONFIG_MV_ETH_NFP_PPP) 	+= $(HAL_ETH_NFP_DIR)/mvNfpPpp.o  
feroceon-$(CONFIG_MV_ETH_NFP_SEC)	+= $(HAL_ETH_NFP_DIR)/mvNfpSec.o
endif

# NETA Giga driver
ifeq ($(CONFIG_MV_ETH_NETA),y)
feroceon-$(CONFIG_MV_ETH_NETA)	        += $(HAL_ETH_GBE_DIR)/mvNeta.o $(HAL_ETH_GBE_DIR)/mvNetaDebug.o \
					   $(HAL_ETH_GBE_DIR)/mvNetaAddrDec.o $(HAL_IF_DIR)/mvSysNeta.o
feroceon-$(CONFIG_MV_ETH_PNC)    	+= $(HAL_ETH_PNC_DIR)/mvTcam.o
feroceon-$(CONFIG_MV_ETH_PNC_PARSER)	+= $(HAL_ETH_PNC_DIR)/mvPnc.o 
feroceon-$(CONFIG_MV_ETH_PNC_AGING)     += $(HAL_ETH_PNC_DIR)/mvPncAging.o
feroceon-$(CONFIG_MV_ETH_BM) 	        += $(HAL_ETH_BM_DIR)/mvBm.o
feroceon-$(CONFIG_MV_ETH_PMT)	        += $(HAL_ETH_PMT_DIR)/mvPmt.o 
feroceon-$(CONFIG_MV_ETH_HWF)           += $(HAL_ETH_GBE_DIR)/mvHwf.o
feroceon-$(CONFIG_MV_ETH_NFP)		+= $(HAL_ETH_NFP_DIR)/mvNfp.o
feroceon-$(CONFIG_MV_ETH_NFP_NAT) 	+= $(HAL_ETH_NFP_DIR)/mvNfpNat.o
feroceon-$(CONFIG_MV_ETH_NFP_SWF) 	+= $(HAL_ETH_NFP_DIR)/mvNfpSwf.o
feroceon-$(CONFIG_MV_ETH_NFP_FIB) 	+= $(HAL_ETH_NFP_DIR)/mvNfpFib.o
feroceon-$(CONFIG_MV_ETH_NFP_PNC) 	+= $(HAL_ETH_NFP_DIR)/mvNfpPnc.o
feroceon-$(CONFIG_MV_ETH_NFP_PMT)       += $(HAL_ETH_NFP_DIR)/mvNfpPmt.o
feroceon-$(CONFIG_MV_ETH_NFP_SEC) 	+= $(HAL_ETH_NFP_DIR)/mvNfpSec.o
endif

feroceon-$(CONFIG_MV_INCLUDE_CESA) 	+= $(HAL_CESA_DIR)/mvCesa.o $(HAL_CESA_DIR)/mvCesaIf.o		 \
					   $(HAL_CESA_DIR)/mvCesaDebug.o $(HAL_CESA_DIR)/mvCesaAddrDec.o \
                        		   $(HAL_CESA_DIR)/mvMD5.o $(HAL_CESA_DIR)/mvSHA1.o 		\
					   $(HAL_CESA_AES_DIR)/mvAesAlg.o $(HAL_CESA_AES_DIR)/mvAesApi.o\
					   $(HAL_IF_DIR)/mvSysCesa.o
feroceon-$(CONFIG_MV_CESA_TEST) 	+= $(HAL_CESA_DIR)/mvCesaTest.o
feroceon-$(CONFIG_MV_INCLUDE_INTEG_SATA)+= $(HAL_IF_DIR)/mvSysSata.o $(HAL_SATA_DIR)/mvSataSoc.o	\
					   $(HAL_SATA_DIR)/mvSataAddrDec.o
feroceon-$(CONFIG_MV_INCLUDE_SPI) 	+= $(HAL_SPI_DIR)/mvSpi.o $(HAL_SPI_DIR)/mvSpiCmnd.o 		\
                         		   $(HAL_SFLASH_DIR)/mvSFlash.o $(HAL_IF_DIR)/mvSysSFlash.o	\
					   $(HAL_IF_DIR)/mvSysSpi.o
feroceon-$(CONFIG_MV_INCLUDE_AUDIO) 	+= $(HAL_AUDIO_DIR)/mvAudio.o $(HAL_IF_DIR)/mvSysAudio.o	\
					   $(HAL_AUDIO_DIR)/mvAudioAddrDec.o
feroceon-$(CONFIG_MV_INCLUDE_TS) 	+= $(HAL_TS_DIR)/mvTsu.o $(HAL_IF_DIR)/mvSysTs.o		\
					   $(HAL_TS_DIR)/mvTsuAddrDec.o
feroceon-$(CONFIG_MV_CPU_PERF_CNTRS)    += $(HAL_CPU_DIR)/mvCpuCntrs.o $(HAL_CPU_DIR)/feroceon/mvFeroceonCntrs.o
feroceon-$(CONFIG_PCIE_VIRTUAL_BRIDGE_SUPPORT) += $(HAL_PEX_DIR)/mvVrtBrgPex.o
feroceon-$(CONFIG_MV_CPU_L2_PERF_CNTRS) += $(HAL_CPU_DIR)/mvCpuL2Cntrs.o

#feroceon-$(CONFIG_MV_INCLUDE_PON)	+= $(HAL_PON_DIR)/mvPonOnuRegs.o $(HAL_PON_DIR)/mvPonOnuMac.o

feroceon-$(CONFIG_MV_INCLUDE_SWITCH) 	+= $(QD_OBJS)

# drivers part
# Legacy Giga driver
ifeq ($(CONFIG_MV_ETH_LEGACY),y)
obj-$(CONFIG_MV_ETH_NFP) 	        += $(LSP_NFP_MGR_DIR)/mv_nfp_mgr.o
obj-$(CONFIG_MV_ETH_NFP_SEC)            += $(LSP_NFP_MGR_DIR)/mv_nfp_sec.o
endif

ifeq ($(CONFIG_MV_ETH_NETA),y)
obj-y	        			+= $(LSP_NFP_MGR_DIR)/
obj-$(CONFIG_MV_ETH_PNC)                += $(LSP_PNC_DIR)/
obj-$(CONFIG_MV_ETH_PMT)		+= $(LSP_PMT_DIR)/
obj-$(CONFIG_MV_ETH_HWF)        	+= $(LSP_HWF_DIR)/
endif

obj-$(CONFIG_MV_USE_XOR_ENGINE) 	+= ../plat-feroceon/mv_drivers_lsp/mv_xor/
obj-$(CONFIG_MV_CESA) 			+= ../plat-feroceon/mv_drivers_lsp/mv_cesa/
obj-$(CONFIG_MV_TSU) 			+= ../plat-feroceon/mv_drivers_lsp/mv_tsu/
#obj-y					+= ../plat-feroceon/mv_drivers_lsp/mv_btns/
##obj-y					+= ../plat-feroceon/mv_drivers_lsp/mv_gpio/
obj-$(CONFIG_MV_INCLUDE_SWITCH)         += $(LSP_SWITCH_DIR)/

#PON
obj-$(CONFIG_MV_GPON) += ../plat-feroceon/mv_drivers_lsp/mv_pon/
obj-$(CONFIG_MV_EPON) += ../plat-feroceon/mv_drivers_lsp/mv_pon/

# The rest of the drivers are compiled through the driver dir directly.


# LSP part
feroceon-$(CONFIG_MV_INCLUDE_USB)    	+= $(LSP_DIR)/usb.o
feroceon-$(CONFIG_PCI)  		+= $(LSP_DIR)/pci.o
feroceon-$(CONFIG_FEROCEON_PROC) 	+= ../plat-feroceon/mv_drivers_lsp/mv_proc/proc.o
feroceon-$(CONFIG_FEROCEON_PROC) 	+= ../plat-feroceon/mv_drivers_lsp/mv_proc/proc_eth.o
feroceon-$(CONFIG_MV_DBG_TRACE) 	+= $(LSP_DIR)/dbg-trace.o
feroceon-$(CONFIG_CPU_FREQ_FEROCEON_KW2) += $(LSP_DIR)/cpufreq.o
feroceon-$(CONFIG_CPU_IDLE)		+= $(LSP_DIR)/cpuidle.o
feroceon-$(CONFIG_CPU_IDLE) 		+= $(LSP_DIR)/pm.o
feroceon-$(CONFIG_CPU_IDLE) 		+= $(LSP_DIR)/sleep.o
feroceon-$(CONFIG_MV_WATCHDOG) 		+= ../plat-feroceon/mv_drivers_lsp/mv_watchdog/feroceon_wdt.o

