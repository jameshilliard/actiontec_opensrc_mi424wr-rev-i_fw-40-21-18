#
# Makefile for the Marvell egiga ethernet driver
#
ifeq ($(CONFIG_ARCH_FEROCEON),y)
	include $(srctree)/$(MACHINE)/config/mvRules.mk
endif

IAL_OBJS        := mvLinuxIalLib.o mvLinuxIalHt.o mvLinuxIalOs.o mvIALCommon.o mvIALCommonUtils.o mvLinuxIalSmart.o
SAL_OBJS        := mvScsiAtaLayer.o

obj-$(CONFIG_SCSI_MVSATA) := mvsata.o
mvsata-y        := $(IAL_OBJS) $(SAL_OBJS) ../../$(HAL_SATA_DIR)/mvSata.o ../../$(HAL_SATA_DIR)/mvStorageDev.o		\
                  ../../$(HAL_SATA_DIR)/mvLog.o



INCLUDE_DIRS    := -Idrivers/scsi
EXTRA_CFLAGS += -DLINUX -D__LINUX__ $(INCLUDE_DIRS)
