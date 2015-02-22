/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg_ipmate/linux/drivers/dwc_otg_driver.c $
 * $Revision: 1.1.2.1.2.1 $
 * $Date: 2007/12/23 11:01:26 $
 * $Change: 543544 $
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */

/** @file
 * The dwc_otg_driver module provides the initialization and cleanup entry
 * points for the DWC_otg driver. This module will be dynamically installed
 * after Linux is booted using the insmod command. When the module is
 * installed, the dwc_otg_driver_init function is called. When the module is
 * removed (using rmmod), the dwc_otg_driver_cleanup function is called.
 *
 * This module also defines a data structure for the dwc_otg_driver, which is
 * used in conjunction with the standard device structure. These
 * structures allow the OTG driver to comply with the standard Linux driver
 * model in which devices and drivers are registered with a bus driver. This
 * has the benefit that Linux can expose attributes of the driver and device
 * in its special sysfs file system. Users can then read or write files in
 * this file system to perform diagnostics on the driver components or the
 * device.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/stat.h>  /* permission constants */
#include <linux/platform_device.h>

#include <asm/io.h>

#include "dwc_otg_plat.h"
#include "dwc_otg_attr.h"
#include "dwc_otg_driver.h"
#include "dwc_otg_cil.h"
#include "dwc_otg_pcd.h"
#include "dwc_otg_hcd.h"

#define	DWC_DRIVER_VERSION	"2.40a 10-APR-2006"
#define	DWC_DRIVER_DESC		"HS OTG USB Controller driver"

static const char dwc_driver_name[] = "dwc_otg";




/*-------------------------------------------------------------------------*/
/* Encapsulate the module parameter settings */

static dwc_otg_core_params_t dwc_otg_module_params = {
        .opt = -1,
        .otg_cap = -1,
        .dma_enable = -1,
	.dma_burst_size = -1,
	.speed = -1,
	.host_support_fs_ls_low_power = -1,
	.host_ls_low_power_phy_clk = -1,
	.enable_dynamic_fifo = -1,
	.data_fifo_size = -1,
	.dev_rx_fifo_size = -1,
	.dev_nperio_tx_fifo_size = -1,
	.dev_perio_tx_fifo_size = {-1,   /* dev_perio_tx_fifo_size_1 */
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1,
				   -1},  /* 15 */
	.host_rx_fifo_size = -1,
	.host_nperio_tx_fifo_size = -1,
	.host_perio_tx_fifo_size = -1,
	.max_transfer_size = -1,
	.max_packet_count = -1,
	.host_channels = -1,
	.dev_endpoints = -1,
	.phy_type = -1,
        .phy_utmi_width = -1,
        .phy_ulpi_ddr = -1,
        .phy_ulpi_ext_vbus = -1,
	.i2c_enable = -1,
	.ulpi_fs_ls = -1,
	.ts_dline = -1,
};

/**
 * This function shows the Driver Version.
 */
static ssize_t version_show(struct device_driver *dev, char *buf)
{
        return snprintf(buf, sizeof(DWC_DRIVER_VERSION)+2,"%s\n",
                        DWC_DRIVER_VERSION);
}
static DRIVER_ATTR(version, S_IRUGO, version_show, NULL);

/**
 * Global Debug Level Mask.
 */
uint32_t g_dbg_lvl = 0; /* OFF */

/**
 * This function shows the driver Debug Level.
 */
static ssize_t dbg_level_show(struct device_driver *_drv, char *_buf)
{
        return sprintf(_buf, "0x%0x\n", g_dbg_lvl);
}
/**
 * This function stores the driver Debug Level.
 */
static ssize_t dbg_level_store(struct device_driver *_drv, const char *_buf,
                               size_t _count)
{
	g_dbg_lvl = simple_strtoul(_buf, NULL, 16);
        return _count;
}
static DRIVER_ATTR(debuglevel, S_IRUGO|S_IWUSR, dbg_level_show, dbg_level_store);


/**
 * This function is called during module intialization to verify that
 * the module parameters are in a valid state.
 */
static int check_parameters(dwc_otg_core_if_t *core_if)
{
	int i;
	int retval = 0;

/* Checks if the parameter is outside of its valid range of values */
#define DWC_OTG_PARAM_TEST(_param_,_low_,_high_) \
	((dwc_otg_module_params._param_ < (_low_)) || \
         (dwc_otg_module_params._param_ > (_high_)))

/* If the parameter has been set by the user, check that the parameter value is
 * within the value range of values.  If not, report a module error. */
#define DWC_OTG_PARAM_ERR(_param_,_low_,_high_,_string_) \
        do { \
		if (dwc_otg_module_params._param_ != -1) { \
			if (DWC_OTG_PARAM_TEST(_param_,(_low_),(_high_))) { \
				DWC_ERROR("`%d' invalid for parameter `%s'\n", \
					  dwc_otg_module_params._param_, _string_); \
				dwc_otg_module_params._param_ = dwc_param_##_param_##_default; \
				retval ++; \
			} \
		} \
	} while (0)

	DWC_OTG_PARAM_ERR(opt,0,1,"opt");
	DWC_OTG_PARAM_ERR(otg_cap,0,2,"otg_cap");
        DWC_OTG_PARAM_ERR(dma_enable,0,1,"dma_enable");
	DWC_OTG_PARAM_ERR(speed,0,1,"speed");
	DWC_OTG_PARAM_ERR(host_support_fs_ls_low_power,0,1,"host_support_fs_ls_low_power");
	DWC_OTG_PARAM_ERR(host_ls_low_power_phy_clk,0,1,"host_ls_low_power_phy_clk");
	DWC_OTG_PARAM_ERR(enable_dynamic_fifo,0,1,"enable_dynamic_fifo");
	DWC_OTG_PARAM_ERR(data_fifo_size,32,32768,"data_fifo_size");
	DWC_OTG_PARAM_ERR(dev_rx_fifo_size,16,32768,"dev_rx_fifo_size");
	DWC_OTG_PARAM_ERR(dev_nperio_tx_fifo_size,16,32768,"dev_nperio_tx_fifo_size");
	DWC_OTG_PARAM_ERR(host_rx_fifo_size,16,32768,"host_rx_fifo_size");
	DWC_OTG_PARAM_ERR(host_nperio_tx_fifo_size,16,32768,"host_nperio_tx_fifo_size");
	DWC_OTG_PARAM_ERR(host_perio_tx_fifo_size,16,32768,"host_perio_tx_fifo_size");
	DWC_OTG_PARAM_ERR(max_transfer_size,2047,524288,"max_transfer_size");
	DWC_OTG_PARAM_ERR(max_packet_count,15,511,"max_packet_count");
	DWC_OTG_PARAM_ERR(host_channels,1,16,"host_channels");
	DWC_OTG_PARAM_ERR(dev_endpoints,1,15,"dev_endpoints");
	DWC_OTG_PARAM_ERR(phy_type,0,2,"phy_type");
        DWC_OTG_PARAM_ERR(phy_ulpi_ddr,0,1,"phy_ulpi_ddr");
        DWC_OTG_PARAM_ERR(phy_ulpi_ext_vbus,0,1,"phy_ulpi_ext_vbus");
	DWC_OTG_PARAM_ERR(i2c_enable,0,1,"i2c_enable");
	DWC_OTG_PARAM_ERR(ulpi_fs_ls,0,1,"ulpi_fs_ls");
	DWC_OTG_PARAM_ERR(ts_dline,0,1,"ts_dline");

	if (dwc_otg_module_params.dma_burst_size != -1) {
		if (DWC_OTG_PARAM_TEST(dma_burst_size,1,1) &&
		    DWC_OTG_PARAM_TEST(dma_burst_size,4,4) &&
		    DWC_OTG_PARAM_TEST(dma_burst_size,8,8) &&
		    DWC_OTG_PARAM_TEST(dma_burst_size,16,16) &&
		    DWC_OTG_PARAM_TEST(dma_burst_size,32,32) &&
		    DWC_OTG_PARAM_TEST(dma_burst_size,64,64) &&
		    DWC_OTG_PARAM_TEST(dma_burst_size,128,128) &&
		    DWC_OTG_PARAM_TEST(dma_burst_size,256,256))
		{
			DWC_ERROR("`%d' invalid for parameter `dma_burst_size'\n",
				  dwc_otg_module_params.dma_burst_size);
			dwc_otg_module_params.dma_burst_size = 32;
			retval ++;
		}
	}

	if (dwc_otg_module_params.phy_utmi_width != -1) {
		if (DWC_OTG_PARAM_TEST(phy_utmi_width,8,8) &&
		    DWC_OTG_PARAM_TEST(phy_utmi_width,16,16))
		{
			DWC_ERROR("`%d' invalid for parameter `phy_utmi_width'\n",
				  dwc_otg_module_params.phy_utmi_width);
			dwc_otg_module_params.phy_utmi_width = 16;
			retval ++;
		}
	}

	for (i=0; i<15; i++) {
		/** @todo should be like above */
		//DWC_OTG_PARAM_ERR(dev_perio_tx_fifo_size[i],4,768,"dev_perio_tx_fifo_size");
		if (dwc_otg_module_params.dev_perio_tx_fifo_size[i] != (unsigned)-1) {
			if (DWC_OTG_PARAM_TEST(dev_perio_tx_fifo_size[i],4,768)) {
				DWC_ERROR("`%d' invalid for parameter `%s_%d'\n",
					  dwc_otg_module_params.dev_perio_tx_fifo_size[i], "dev_perio_tx_fifo_size", i);
				dwc_otg_module_params.dev_perio_tx_fifo_size[i] = dwc_param_dev_perio_tx_fifo_size_default;
				retval ++;
			}
		}
	}


	/* At this point, all module parameters that have been set by the user
	 * are valid, and those that have not are left unset.  Now set their
	 * default values and/or check the parameters against the hardware
	 * configurations of the OTG core. */



/* This sets the parameter to the default value if it has not been set by the
 * user */
#define DWC_OTG_PARAM_SET_DEFAULT(_param_) \
	({ \
		int changed = 1; \
		if (dwc_otg_module_params._param_ == -1) { \
			changed = 0; \
			dwc_otg_module_params._param_ = dwc_param_##_param_##_default; \
		} \
		changed; \
	})

/* This checks the macro agains the hardware configuration to see if it is
 * valid.  It is possible that the default value could be invalid.  In this
 * case, it will report a module error if the user touched the parameter.
 * Otherwise it will adjust the value without any error. */
#define DWC_OTG_PARAM_CHECK_VALID(_param_,_str_,_is_valid_,_set_valid_) \
	({ \
	        int changed = DWC_OTG_PARAM_SET_DEFAULT(_param_); \
		int error = 0; \
		if (!(_is_valid_)) { \
			if (changed) { \
				DWC_ERROR("`%d' invalid for parameter `%s'.  Check HW configuration.\n", dwc_otg_module_params._param_,_str_); \
				error = 1; \
			} \
			dwc_otg_module_params._param_ = (_set_valid_); \
		} \
		error; \
	})

	/* OTG Cap */
	retval += DWC_OTG_PARAM_CHECK_VALID(otg_cap,"otg_cap",
                  ({
			  int valid;
			  valid = 1;
			  switch (dwc_otg_module_params.otg_cap) {
			  case DWC_OTG_CAP_PARAM_HNP_SRP_CAPABLE:
				  if (core_if->hwcfg2.b.op_mode != DWC_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG) valid = 0;
				  break;
			  case DWC_OTG_CAP_PARAM_SRP_ONLY_CAPABLE:
				  if ((core_if->hwcfg2.b.op_mode != DWC_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG) &&
				      (core_if->hwcfg2.b.op_mode != DWC_HWCFG2_OP_MODE_SRP_ONLY_CAPABLE_OTG) &&
				      (core_if->hwcfg2.b.op_mode != DWC_HWCFG2_OP_MODE_SRP_CAPABLE_DEVICE) &&
				      (core_if->hwcfg2.b.op_mode != DWC_HWCFG2_OP_MODE_SRP_CAPABLE_HOST))
				  {
					  valid = 0;
				  }
				  break;
			  case DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE:
				  /* always valid */
				  break;
			  }
			  valid;
		  }),
                  (((core_if->hwcfg2.b.op_mode == DWC_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG) ||
		    (core_if->hwcfg2.b.op_mode == DWC_HWCFG2_OP_MODE_SRP_ONLY_CAPABLE_OTG) ||
		    (core_if->hwcfg2.b.op_mode == DWC_HWCFG2_OP_MODE_SRP_CAPABLE_DEVICE) ||
		    (core_if->hwcfg2.b.op_mode == DWC_HWCFG2_OP_MODE_SRP_CAPABLE_HOST)) ?
		   DWC_OTG_CAP_PARAM_SRP_ONLY_CAPABLE :
		   DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE));
	
	retval += DWC_OTG_PARAM_CHECK_VALID(dma_enable,"dma_enable",
					    ((dwc_otg_module_params.dma_enable == 1) && (core_if->hwcfg2.b.architecture == 0)) ? 0 : 1,
					    0);

	retval += DWC_OTG_PARAM_CHECK_VALID(opt,"opt",
					    1,
					    0);

	DWC_OTG_PARAM_SET_DEFAULT(dma_burst_size);

	retval += DWC_OTG_PARAM_CHECK_VALID(host_support_fs_ls_low_power,
					    "host_support_fs_ls_low_power",
					    1, 0);

	retval += DWC_OTG_PARAM_CHECK_VALID(enable_dynamic_fifo,
				  "enable_dynamic_fifo",
				  ((dwc_otg_module_params.enable_dynamic_fifo == 0) ||
				   (core_if->hwcfg2.b.dynamic_fifo == 1)), 0);


	retval += DWC_OTG_PARAM_CHECK_VALID(data_fifo_size,
				  "data_fifo_size",
				  (dwc_otg_module_params.data_fifo_size <= core_if->hwcfg3.b.dfifo_depth),
				  core_if->hwcfg3.b.dfifo_depth);

	retval += DWC_OTG_PARAM_CHECK_VALID(dev_rx_fifo_size,
				  "dev_rx_fifo_size",
				  (dwc_otg_module_params.dev_rx_fifo_size <= dwc_read_reg32(&core_if->core_global_regs->grxfsiz)),
				  dwc_read_reg32(&core_if->core_global_regs->grxfsiz));

	retval += DWC_OTG_PARAM_CHECK_VALID(dev_nperio_tx_fifo_size,
				  "dev_nperio_tx_fifo_size",
				  (dwc_otg_module_params.dev_nperio_tx_fifo_size <= (dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz) >> 16)),
				  (dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz) >> 16));

	retval += DWC_OTG_PARAM_CHECK_VALID(host_rx_fifo_size,
					    "host_rx_fifo_size",
					    (dwc_otg_module_params.host_rx_fifo_size <= dwc_read_reg32(&core_if->core_global_regs->grxfsiz)),
					    dwc_read_reg32(&core_if->core_global_regs->grxfsiz));


	retval += DWC_OTG_PARAM_CHECK_VALID(host_nperio_tx_fifo_size,
				  "host_nperio_tx_fifo_size",
				  (dwc_otg_module_params.host_nperio_tx_fifo_size <= (dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz) >> 16)),
				  (dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz) >> 16));

	retval += DWC_OTG_PARAM_CHECK_VALID(host_perio_tx_fifo_size,
					    "host_perio_tx_fifo_size",
					    (dwc_otg_module_params.host_perio_tx_fifo_size <= ((dwc_read_reg32(&core_if->core_global_regs->hptxfsiz) >> 16))),
					    ((dwc_read_reg32(&core_if->core_global_regs->hptxfsiz) >> 16)));

	retval += DWC_OTG_PARAM_CHECK_VALID(max_transfer_size,
				  "max_transfer_size",
				  (dwc_otg_module_params.max_transfer_size < (1 << (core_if->hwcfg3.b.xfer_size_cntr_width + 11))),
				  ((1 << (core_if->hwcfg3.b.xfer_size_cntr_width + 11)) - 1));

	retval += DWC_OTG_PARAM_CHECK_VALID(max_packet_count,
				  "max_packet_count",
				  (dwc_otg_module_params.max_packet_count < (1 << (core_if->hwcfg3.b.packet_size_cntr_width + 4))),
				  ((1 << (core_if->hwcfg3.b.packet_size_cntr_width + 4)) - 1));

	retval += DWC_OTG_PARAM_CHECK_VALID(host_channels,
				  "host_channels",
				  (dwc_otg_module_params.host_channels <= (core_if->hwcfg2.b.num_host_chan + 1)),
				  (core_if->hwcfg2.b.num_host_chan + 1));

	retval += DWC_OTG_PARAM_CHECK_VALID(dev_endpoints,
				  "dev_endpoints",
				  (dwc_otg_module_params.dev_endpoints <= (core_if->hwcfg2.b.num_dev_ep)),
				  core_if->hwcfg2.b.num_dev_ep);

/*
 * Define the following to disable the FS PHY Hardware checking.  This is for
 * internal testing only.
 *
 * #define NO_FS_PHY_HW_CHECKS
 */

#ifdef NO_FS_PHY_HW_CHECKS
	retval += DWC_OTG_PARAM_CHECK_VALID(phy_type,
					    "phy_type", 1, 0);
#else
	retval += DWC_OTG_PARAM_CHECK_VALID(phy_type,
					    "phy_type",
					    ({
						    int valid = 0;
						    if ((dwc_otg_module_params.phy_type == DWC_PHY_TYPE_PARAM_UTMI) &&
							((core_if->hwcfg2.b.hs_phy_type == 1) ||
							 (core_if->hwcfg2.b.hs_phy_type == 3)))
						    {
							    valid = 1;
						    }
						    else if ((dwc_otg_module_params.phy_type == DWC_PHY_TYPE_PARAM_ULPI) &&
							     ((core_if->hwcfg2.b.hs_phy_type == 2) ||
							      (core_if->hwcfg2.b.hs_phy_type == 3)))
						    {
							    valid = 1;
						    }
						    else if ((dwc_otg_module_params.phy_type == DWC_PHY_TYPE_PARAM_FS) &&
							     (core_if->hwcfg2.b.fs_phy_type == 1))
						    {
							    valid = 1;
						    }
						    valid;
					    }),
					    ({
						    int set = DWC_PHY_TYPE_PARAM_FS;
						    if (core_if->hwcfg2.b.hs_phy_type) {
							    if ((core_if->hwcfg2.b.hs_phy_type == 3) ||
								(core_if->hwcfg2.b.hs_phy_type == 1)) {
								    set = DWC_PHY_TYPE_PARAM_UTMI;
							    }
							    else {
								    set = DWC_PHY_TYPE_PARAM_ULPI;
							    }
						    }
						    set;
					    }));
#endif

	retval += DWC_OTG_PARAM_CHECK_VALID(speed,"speed",
					    (dwc_otg_module_params.speed == 0) && (dwc_otg_module_params.phy_type == DWC_PHY_TYPE_PARAM_FS) ? 0 : 1,
					    dwc_otg_module_params.phy_type == DWC_PHY_TYPE_PARAM_FS ? 1 : 0);

	retval += DWC_OTG_PARAM_CHECK_VALID(host_ls_low_power_phy_clk,
					    "host_ls_low_power_phy_clk",
					    ((dwc_otg_module_params.host_ls_low_power_phy_clk == DWC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_48MHZ) && (dwc_otg_module_params.phy_type == DWC_PHY_TYPE_PARAM_FS) ? 0 : 1),
					    ((dwc_otg_module_params.phy_type == DWC_PHY_TYPE_PARAM_FS) ? DWC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_6MHZ : DWC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_48MHZ));

        DWC_OTG_PARAM_SET_DEFAULT(phy_ulpi_ddr);
        DWC_OTG_PARAM_SET_DEFAULT(phy_ulpi_ext_vbus);
        DWC_OTG_PARAM_SET_DEFAULT(phy_utmi_width);
        DWC_OTG_PARAM_SET_DEFAULT(ulpi_fs_ls);
        DWC_OTG_PARAM_SET_DEFAULT(ts_dline);

#ifdef NO_FS_PHY_HW_CHECKS
	retval += DWC_OTG_PARAM_CHECK_VALID(i2c_enable,
					    "i2c_enable", 1, 0);
#else
	retval += DWC_OTG_PARAM_CHECK_VALID(i2c_enable,
					    "i2c_enable",
					    (dwc_otg_module_params.i2c_enable == 1) && (core_if->hwcfg3.b.i2c == 0) ? 0 : 1,
					    0);
#endif

	for (i=0; i<16; i++) {

		int changed = 1;
		int error = 0;

		if (dwc_otg_module_params.dev_perio_tx_fifo_size[i] == -1) {
			changed = 0;
			dwc_otg_module_params.dev_perio_tx_fifo_size[i] = dwc_param_dev_perio_tx_fifo_size_default;
		}
		if (!(dwc_otg_module_params.dev_perio_tx_fifo_size[i] <= (dwc_read_reg32(&core_if->core_global_regs->dptxfsiz[i])))) {
			if (changed) {
				DWC_ERROR("`%d' invalid for parameter `dev_perio_fifo_size_%d'.  Check HW configuration.\n", dwc_otg_module_params.dev_perio_tx_fifo_size[i],i);
				error = 1;
			}
			dwc_otg_module_params.dev_perio_tx_fifo_size[i] = dwc_read_reg32(&core_if->core_global_regs->dptxfsiz[i]);
		}
		retval += error;
	}

	return retval;
}


/**
 * This function is the top level interrupt handler for the Common
 * (Device and host modes) interrupts.
 */
static irqreturn_t dwc_otg_common_irq(int _irq, void *_dev, struct pt_regs *_r)
{
        dwc_otg_device_t *otg_dev = _dev;
        int32_t retval = IRQ_NONE;
        unsigned long flags;

        DWC_OTG_LOCK(flags);
        retval = dwc_otg_handle_common_intr( otg_dev->core_if );
        DWC_OTG_UNLOCK(flags);
	return IRQ_RETVAL(retval);
}

/**
 * This function is called when a device is unregistered with the
 * dwc_otg_driver. This happens, for example, when the rmmod command is
 * executed. The device may or may not be electrically present. If it is
 * present, the driver stops device processing. Any resources used on behalf
 * of this device are freed.
 *
 * @param[in] dev
 */
static int dwc_otg_driver_remove(struct device *dev)
{
        dwc_otg_device_t *otg_dev = dev_get_drvdata(dev);
        DWC_DEBUGPL(DBG_ANY, "%s(%p)\n", __func__, dev);

	if (otg_dev == NULL) {
		/* Memory allocation for the dwc_otg_device failed. */
		return -ENOMEM;
	}

        /*
         * Free the IRQ
         */
	if (otg_dev->common_irq_installed) {
		free_irq( DWC_OTG_IRQ, otg_dev );
	}

#ifndef DWC_DEVICE_ONLY
	if (otg_dev->hcd != NULL) {
		dwc_otg_hcd_remove( dev );
	}
#endif

#ifndef DWC_HOST_ONLY
	if (otg_dev->pcd != NULL) {
		dwc_otg_pcd_remove( dev );
	}
#endif
	if (otg_dev->core_if != NULL) {
		dwc_otg_cil_remove( otg_dev->core_if );
	}

	/*
         * Remove the device attributes
         */
        dwc_otg_attr_remove(dev);

        /*
         * Return the memory.
         */
	if (otg_dev->base != NULL) {
		iounmap(otg_dev->base);
	}
	kfree(otg_dev);

        /*
         * Clear the drvdata pointer.
         */
        dev_set_drvdata( dev, 0 );
        return 0;
}

/**
 * This function is called when an device is bound to a
 * dwc_otg_driver. It creates the driver components required to
 * control the device (CIL, HCD, and PCD) and it initializes the
 * device. The driver components are stored in a dwc_otg_device
 * structure. A reference to the dwc_otg_device is saved in the
 * device. This allows the driver to access the dwc_otg_device
 * structure on subsequent calls to driver methods for this device.
 *
 * @param[in] dev  device definition
 */
static int dwc_otg_driver_probe(struct device *dev)
{
	int retval = 0;
	dwc_otg_device_t *dwc_otg_device;
	int32_t	snpsid;
	unsigned long flags;

	dev_dbg(dev, "dwc_otg_driver_probe(%p)\n", dev);
	DWC_OTG_LOCK_NOFLAGS();

	dwc_otg_device = kmalloc(sizeof(dwc_otg_device_t), GFP_KERNEL);
	if (dwc_otg_device == 0) {
		dev_err(dev, "kmalloc of dwc_otg_device failed\n");
		retval = -ENOMEM;
		goto fail;
	}
	memset(dwc_otg_device, 0, sizeof(*dwc_otg_device));
	dwc_otg_device->reg_offset = 0xFFFFFFFF;

	/*
	 * Map the DWC_otg Core memory into virtual address space.
	 */
#ifdef CONFIG_CPU_CAVIUM_OCTEON
	dwc_otg_device->base = ioremap(0x00016F0010000000ull, SZ_256K);
#else
	#error: Currently we only support the Cavium Octeon implementation
#endif
	if (dwc_otg_device->base == NULL)
	{
		dev_err(dev, "ioremap() failed\n");
		retval = -ENOMEM;
		goto fail;
	}
	dev_dbg(dev, "base=%p\n", dwc_otg_device->base);

	/*
	 * Attempt to ensure this device is really a DWC_otg Controller.
	 * Read and verify the SNPSID register contents. The value should be
	 * 0x45F42XXX, which corresponds to "OT2", as in "OTG version 2.XX".
	 */
	snpsid = dwc_read_reg32((uint32_t *)((uint8_t *)dwc_otg_device->base + 0x40));
	if ((snpsid & 0xFFFFF000) != 0x4F542000) {
		dev_err(dev, "Bad value for SNPSID: 0x%08x\n", snpsid);
		retval = -EINVAL;
		goto fail;
	}

	/*
	 * Initialize driver data to point to the global DWC_otg
	 * Device structure.
	 */
	dev_set_drvdata( dev, dwc_otg_device );
	dev_dbg(dev, "dwc_otg_device=0x%p\n", dwc_otg_device);
	
	dwc_otg_device->core_if = dwc_otg_cil_init( dwc_otg_device->base,
						    &dwc_otg_module_params);
	if (dwc_otg_device->core_if == 0) {
		dev_err(dev, "CIL initialization failed!\n");
		retval = -ENOMEM;
		goto fail;
	}
	
	/*
	 * Validate parameter values.
	 */
	if (check_parameters(dwc_otg_device->core_if) != 0) {
		retval = -EINVAL;
		goto fail;
	}

	/*
	 * Create Device Attributes in sysfs
	 */
	dwc_otg_attr_create (dev);

	/*
	 * Disable the global interrupt until all the interrupt
	 * handlers are installed.
	 */
	dwc_otg_disable_global_interrupts( dwc_otg_device->core_if );
	/*
	 * Install the interrupt handler for the common interrupts before
	 * enabling common interrupts in core_init below.
	 */
	DWC_DEBUGPL( DBG_CIL, "registering (common) handler for irq%d\n",
		     DWC_OTG_IRQ);
	retval = request_irq(DWC_OTG_IRQ, dwc_otg_common_irq,
			     SA_SHIRQ, "dwc_otg", dwc_otg_device );
	if (retval != 0) {
		DWC_ERROR("request of irq%d failed\n", DWC_OTG_IRQ);
		retval = -EBUSY;
		goto fail;
	} else {
		dwc_otg_device->common_irq_installed = 1;
	}
#ifdef CONFIG_MACH_IPMATE
	set_irq_type(DWC_OTG_IRQ, IRQT_LOW);
#endif

	/*
	 * Initialize the DWC_otg core.
	 */
	dwc_otg_core_init( dwc_otg_device->core_if );

#ifndef DWC_HOST_ONLY
	/*
	 * Initialize the PCD
	 */
	retval = dwc_otg_pcd_init( dev );
	if (retval != 0) {
		DWC_ERROR("dwc_otg_pcd_init failed\n");
		dwc_otg_device->pcd = NULL;
		goto fail;
	}
#endif
#ifndef DWC_DEVICE_ONLY
	/*
	 * Initialize the HCD
	 */
	retval = dwc_otg_hcd_init(dev);
	if (retval != 0) {
		DWC_ERROR("dwc_otg_hcd_init failed\n");
		dwc_otg_device->hcd = NULL;
		goto fail;
	}
#endif

	/*
	 * Enable the global interrupt after all the interrupt
	 * handlers are installed.
	 */
	local_irq_save(flags);
	dwc_otg_enable_global_interrupts( dwc_otg_device->core_if );
	DWC_OTG_UNLOCK_NOFLAGS();
	local_irq_restore(flags);

	return 0;

 fail:
	dwc_otg_driver_remove(dev);
	DWC_OTG_UNLOCK_NOFLAGS();
	return retval;
}

/**
 * This structure defines the methods to be called by a bus driver
 * during the lifecycle of a device on that bus. Both drivers and
 * devices are registered with a bus driver. The bus driver matches
 * devices to drivers based on information in the device and driver
 * structures.
 *
 * The probe function is called when the bus driver matches a device
 * to this driver. The remove function is called when a device is
 * unregistered with the bus driver.
 */
static struct device_driver dwc_otg_driver = {
        .name   	= dwc_driver_name,
	.bus		= &platform_bus_type,
	.probe		= dwc_otg_driver_probe,
	.remove		= dwc_otg_driver_remove,
};

/**
 * This function is called when the dwc_otg_driver is installed with the
 * insmod command. It registers the dwc_otg_driver structure with the
 * appropriate bus driver. This will cause the dwc_otg_driver_probe function
 * to be called. In addition, the bus driver will automatically expose
 * attributes defined for the device and driver in the special sysfs file
 * system.
 *
 * @return
 */
static int __init dwc_otg_driver_init(void)
{
        int retval;

#ifdef CONFIG_CPU_CAVIUM_OCTEON

        union
        {
            uint64_t u64;
            struct
            {
#ifdef __BIG_ENDIAN_BITFIELD
                uint64_t reserved                : 47;
                uint64_t p_x_on                  : 1;
                uint64_t p_rclk                  : 1;
                uint64_t p_xenbn                 : 1;
                uint64_t p_com_on                : 1;
                uint64_t p_c_sel                 : 2;
                uint64_t cdiv_byp                : 1;
                uint64_t sd_mode                 : 2;
                uint64_t s_bist                  : 1;
                uint64_t por                     : 1;
                uint64_t enable                  : 1;
                uint64_t prst                    : 1;
                uint64_t hrst                    : 1;
                uint64_t divide                  : 3;
#else
                uint64_t divide                  : 3;
                uint64_t hrst                    : 1;
                uint64_t prst                    : 1;
                uint64_t enable                  : 1;
                uint64_t por                     : 1;
                uint64_t s_bist                  : 1;
                uint64_t sd_mode                 : 2;
                uint64_t cdiv_byp                : 1;
                uint64_t p_c_sel                 : 2;
                uint64_t p_com_on                : 1;
                uint64_t p_xenbn                 : 1;
                uint64_t p_rclk                  : 1;
                uint64_t p_x_on                  : 1;
                uint64_t reserved                : 47;
#endif
            } s;
        } usbn_clk_ctl;
        const uint64_t OCTEON_USBN_CLK_CTL = 0x8001180068000010ull;
        struct platform_device *pdev;

        // Only load driver for chips with USB
	struct cpuinfo_mips *c = &current_cpu_data;
	if ((c->cputype != CPU_CAVIUM_CN31XX) &&
            (c->cputype != CPU_CAVIUM_CN50XX) &&
            (c->cputype != CPU_CAVIUM_CN30XX))
            return 0;

        /*
         * Though core was configured for external dma override that
         * with slave mode only for CN31XX.
         */
        if (c->cputype == CPU_CAVIUM_CN31XX)
            dwc_otg_module_params.dma_enable = 0;

        printk(KERN_INFO "%s: version %s\n", dwc_driver_name, DWC_DRIVER_VERSION);

#ifdef CONFIG_SMP
        spin_lock_init(&dwc_otg_global_lock);
#endif

        // Fetch the value of the Register, and de-assert POR
        usbn_clk_ctl.u64 = octeon_read_csr(OCTEON_USBN_CLK_CTL);
        usbn_clk_ctl.s.por = 0;
        usbn_clk_ctl.s.p_rclk  = 1;
        usbn_clk_ctl.s.p_xenbn = 0;
        octeon_write_csr(OCTEON_USBN_CLK_CTL, usbn_clk_ctl.u64);

        // Wait for POR
        udelay(850);

        usbn_clk_ctl.u64 = octeon_read_csr(OCTEON_USBN_CLK_CTL);
        usbn_clk_ctl.s.por = 0;
        usbn_clk_ctl.s.p_rclk  = 1;
        usbn_clk_ctl.s.p_xenbn = 0;
        usbn_clk_ctl.s.prst = 1;
        octeon_write_csr(OCTEON_USBN_CLK_CTL, usbn_clk_ctl.u64);

        udelay(1);

        usbn_clk_ctl.s.hrst = 1;
        octeon_write_csr(OCTEON_USBN_CLK_CTL, usbn_clk_ctl.u64);

        udelay(1);
#endif
	retval = driver_register(&dwc_otg_driver);
	if (retval < 0) {
                printk(KERN_ERR "%s retval=%d\n", __func__, retval);
		return retval;
        }
        driver_create_file(&dwc_otg_driver, &driver_attr_version);
        driver_create_file(&dwc_otg_driver, &driver_attr_debuglevel);

        pdev = platform_device_register_simple((char*)dwc_driver_name, -1, NULL, 0);
        if (!pdev)
        {
            printk("%s: Failed to allocate platform device\n", dwc_driver_name);
            retval = -ENOMEM;
        }

        return retval;
}
module_init(dwc_otg_driver_init);

/**
 * This function is called when the driver is removed from the kernel
 * with the rmmod command. The driver unregisters itself with its bus
 * driver.
 *
 */
static void __exit dwc_otg_driver_cleanup(void)
{
	printk(KERN_DEBUG "dwc_otg_driver_cleanup()\n");

	driver_remove_file(&dwc_otg_driver, &driver_attr_debuglevel);
	driver_remove_file(&dwc_otg_driver, &driver_attr_version);

	driver_unregister(&dwc_otg_driver);

	printk(KERN_INFO "%s module removed\n", dwc_driver_name);
}
module_exit(dwc_otg_driver_cleanup);

MODULE_DESCRIPTION(DWC_DRIVER_DESC);
MODULE_AUTHOR("Synopsys Inc.");
MODULE_LICENSE("GPL");

module_param_named(otg_cap, dwc_otg_module_params.otg_cap, int, 0444);
MODULE_PARM_DESC(otg_cap, "OTG Capabilities 0=HNP&SRP 1=SRP Only 2=None");
module_param_named(opt, dwc_otg_module_params.opt, int, 0444);
MODULE_PARM_DESC(opt, "OPT Mode");
module_param_named(dma_enable, dwc_otg_module_params.dma_enable, int, 0444);
MODULE_PARM_DESC(dma_enable, "DMA Mode 0=Slave 1=DMA enabled");
module_param_named(dma_burst_size, dwc_otg_module_params.dma_burst_size, int, 0444);
MODULE_PARM_DESC(dma_burst_size, "DMA Burst Size 1, 4, 8, 16, 32, 64, 128, 256");
module_param_named(speed, dwc_otg_module_params.speed, int, 0444);
MODULE_PARM_DESC(speed, "Speed 0=High Speed 1=Full Speed");
module_param_named(host_support_fs_ls_low_power, dwc_otg_module_params.host_support_fs_ls_low_power, int, 0444);
MODULE_PARM_DESC(host_support_fs_ls_low_power, "Support Low Power w/FS or LS 0=Support 1=Don't Support");
module_param_named(host_ls_low_power_phy_clk, dwc_otg_module_params.host_ls_low_power_phy_clk, int, 0444);
MODULE_PARM_DESC(host_ls_low_power_phy_clk, "Low Speed Low Power Clock 0=48Mhz 1=6Mhz");
module_param_named(enable_dynamic_fifo, dwc_otg_module_params.enable_dynamic_fifo, int, 0444);
MODULE_PARM_DESC(enable_dynamic_fifo, "0=cC Setting 1=Allow Dynamic Sizing");
module_param_named(data_fifo_size, dwc_otg_module_params.data_fifo_size, int, 0444);
MODULE_PARM_DESC(data_fifo_size, "Total number of words in the data FIFO memory 32-32768");
module_param_named(dev_rx_fifo_size, dwc_otg_module_params.dev_rx_fifo_size, int, 0444);
MODULE_PARM_DESC(dev_rx_fifo_size, "Number of words in the Rx FIFO 16-32768");
module_param_named(dev_nperio_tx_fifo_size, dwc_otg_module_params.dev_nperio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(dev_nperio_tx_fifo_size, "Number of words in the non-periodic Tx FIFO 16-32768");
module_param_named(dev_perio_tx_fifo_size_1, dwc_otg_module_params.dev_perio_tx_fifo_size[0], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_1, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_2, dwc_otg_module_params.dev_perio_tx_fifo_size[1], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_2, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_3, dwc_otg_module_params.dev_perio_tx_fifo_size[2], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_3, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_4, dwc_otg_module_params.dev_perio_tx_fifo_size[3], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_4, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_5, dwc_otg_module_params.dev_perio_tx_fifo_size[4], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_5, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_6, dwc_otg_module_params.dev_perio_tx_fifo_size[5], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_6, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_7, dwc_otg_module_params.dev_perio_tx_fifo_size[6], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_7, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_8, dwc_otg_module_params.dev_perio_tx_fifo_size[7], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_8, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_9, dwc_otg_module_params.dev_perio_tx_fifo_size[8], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_9, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_10, dwc_otg_module_params.dev_perio_tx_fifo_size[9], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_10, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_11, dwc_otg_module_params.dev_perio_tx_fifo_size[10], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_11, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_12, dwc_otg_module_params.dev_perio_tx_fifo_size[11], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_12, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_13, dwc_otg_module_params.dev_perio_tx_fifo_size[12], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_13, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_14, dwc_otg_module_params.dev_perio_tx_fifo_size[13], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_14, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(dev_perio_tx_fifo_size_15, dwc_otg_module_params.dev_perio_tx_fifo_size[14], int, 0444);
MODULE_PARM_DESC(dev_perio_tx_fifo_size_15, "Number of words in the periodic Tx FIFO 4-768");
module_param_named(host_rx_fifo_size, dwc_otg_module_params.host_rx_fifo_size, int, 0444);
MODULE_PARM_DESC(host_rx_fifo_size, "Number of words in the Rx FIFO 16-32768");
module_param_named(host_nperio_tx_fifo_size, dwc_otg_module_params.host_nperio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(host_nperio_tx_fifo_size, "Number of words in the non-periodic Tx FIFO 16-32768");
module_param_named(host_perio_tx_fifo_size, dwc_otg_module_params.host_perio_tx_fifo_size, int, 0444);
MODULE_PARM_DESC(host_perio_tx_fifo_size, "Number of words in the host periodic Tx FIFO 16-32768");
module_param_named(max_transfer_size, dwc_otg_module_params.max_transfer_size, int, 0444);
/** @todo Set the max to 512K, modify checks */
MODULE_PARM_DESC(max_transfer_size, "The maximum transfer size supported in bytes 2047-65535");
module_param_named(max_packet_count, dwc_otg_module_params.max_packet_count, int, 0444);
MODULE_PARM_DESC(max_packet_count, "The maximum number of packets in a transfer 15-511");
module_param_named(host_channels, dwc_otg_module_params.host_channels, int, 0444);
MODULE_PARM_DESC(host_channels, "The number of host channel registers to use 1-16");
module_param_named(dev_endpoints, dwc_otg_module_params.dev_endpoints, int, 0444);
MODULE_PARM_DESC(dev_endpoints, "The number of endpoints in addition to EP0 available for device mode 1-15");
module_param_named(phy_type, dwc_otg_module_params.phy_type, int, 0444);
MODULE_PARM_DESC(phy_type, "0=Reserved 1=UTMI+ 2=ULPI");
module_param_named(phy_utmi_width, dwc_otg_module_params.phy_utmi_width, int, 0444);
MODULE_PARM_DESC(phy_utmi_width, "Specifies the UTMI+ Data Width 8 or 16 bits");
module_param_named(phy_ulpi_ddr, dwc_otg_module_params.phy_ulpi_ddr, int, 0444);
MODULE_PARM_DESC(phy_ulpi_ddr, "ULPI at double or single data rate 0=Single 1=Double");
module_param_named(phy_ulpi_ext_vbus, dwc_otg_module_params.phy_ulpi_ext_vbus, int, 0444);
MODULE_PARM_DESC(phy_ulpi_ext_vbus, "ULPI PHY using internal or external vbus 0=Internal");
module_param_named(i2c_enable, dwc_otg_module_params.i2c_enable, int, 0444);
MODULE_PARM_DESC(i2c_enable, "FS PHY Interface");
module_param_named(ulpi_fs_ls, dwc_otg_module_params.ulpi_fs_ls, int, 0444);
MODULE_PARM_DESC(ulpi_fs_ls, "ULPI PHY FS/LS mode only");
module_param_named(ts_dline, dwc_otg_module_params.ts_dline, int, 0444);
MODULE_PARM_DESC(ts_dline, "Term select Dline pulsing for all PHYs");
module_param_named(debug, g_dbg_lvl, int, 0444);
MODULE_PARM_DESC(debug, "");


/** @page "Module Parameters"
 *
 * The following parameters may be specified when starting the module.
 * These parameters define how the DWC_otg controller should be
 * configured.  Parameter values are passed to the CIL initialization
 * function dwc_otg_cil_init
 *
 * Example: <code>modprobe dwc_otg speed=1 otg_cap=1</code>
 *

 <table>
 <tr><td>Parameter Name</td><td>Meaning</td></tr>

 <tr>
 <td>otg_cap</td>
 <td>Specifies the OTG capabilities. The driver will automatically detect the
 value for this parameter if none is specified.
 - 0: HNP and SRP capable (default, if available)
 - 1: SRP Only capable
 - 2: No HNP/SRP capable
 </td></tr>

 <tr>
 <td>dma_enable</td>
 <td>Specifies whether to use slave or DMA mode for accessing the data FIFOs.
 The driver will automatically detect the value for this parameter if none is
 specified.
 - 0: Slave
 - 1: DMA (default, if available)
 </td></tr>

 <tr>
 <td>dma_burst_size</td>
 <td>The DMA Burst size (applicable only for External DMA Mode).
 - Values: 1, 4, 8 16, 32, 64, 128, 256 (default 32)
 </td></tr>

 <tr>
 <td>speed</td>
 <td>Specifies the maximum speed of operation in host and device mode. The
 actual speed depends on the speed of the attached device and the value of
 phy_type.
 - 0: High Speed (default)
 - 1: Full Speed
 </td></tr>

 <tr>
 <td>host_support_fs_ls_low_power</td>
 <td>Specifies whether low power mode is supported when attached to a Full
 Speed or Low Speed device in host mode.
 - 0: Don't support low power mode (default)
 - 1: Support low power mode
 </td></tr>

 <tr>
 <td>host_ls_low_power_phy_clk</td>
 <td>Specifies the PHY clock rate in low power mode when connected to a Low
 Speed device in host mode. This parameter is applicable only if
 HOST_SUPPORT_FS_LS_LOW_POWER is enabled.
 - 0: 48 MHz (default)
 - 1: 6 MHz
 </td></tr>

 <tr>
 <td>enable_dynamic_fifo</td>
 <td> Specifies whether FIFOs may be resized by the driver software.
 - 0: Use cC FIFO size parameters
 - 1: Allow dynamic FIFO sizing (default)
 </td></tr>

 <tr>
 <td>data_fifo_size</td>
 <td>Total number of 4-byte words in the data FIFO memory. This memory
 includes the Rx FIFO, non-periodic Tx FIFO, and periodic Tx FIFOs.
 - Values: 32 to 32768 (default 8192)

 Note: The total FIFO memory depth in the FPGA configuration is 8192.
 </td></tr>

 <tr>
 <td>dev_rx_fifo_size</td>
 <td>Number of 4-byte words in the Rx FIFO in device mode when dynamic
 FIFO sizing is enabled.
 - Values: 16 to 32768 (default 1064)
 </td></tr>

 <tr>
 <td>dev_nperio_tx_fifo_size</td>
 <td>Number of 4-byte words in the non-periodic Tx FIFO in device mode when
 dynamic FIFO sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>dev_perio_tx_fifo_size_n (n = 1 to 15)</td>
 <td>Number of 4-byte words in each of the periodic Tx FIFOs in device mode
 when dynamic FIFO sizing is enabled.
 - Values: 4 to 768 (default 256)
 </td></tr>

 <tr>
 <td>host_rx_fifo_size</td>
 <td>Number of 4-byte words in the Rx FIFO in host mode when dynamic FIFO
 sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>host_nperio_tx_fifo_size</td>
 <td>Number of 4-byte words in the non-periodic Tx FIFO in host mode when
 dynamic FIFO sizing is enabled in the core.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>host_perio_tx_fifo_size</td>
 <td>Number of 4-byte words in the host periodic Tx FIFO when dynamic FIFO
 sizing is enabled.
 - Values: 16 to 32768 (default 1024)
 </td></tr>

 <tr>
 <td>max_transfer_size</td>
 <td>The maximum transfer size supported in bytes.
 - Values: 2047 to 65,535 (default 65,535)
 </td></tr>

 <tr>
 <td>max_packet_count</td>
 <td>The maximum number of packets in a transfer.
 - Values: 15 to 511 (default 511)
 </td></tr>

 <tr>
 <td>host_channels</td>
 <td>The number of host channel registers to use.
 - Values: 1 to 16 (default 12)

 Note: The FPGA configuration supports a maximum of 12 host channels.
 </td></tr>

 <tr>
 <td>dev_endpoints</td>
 <td>The number of endpoints in addition to EP0 available for device mode
 operations.
 - Values: 1 to 15 (default 6 IN and OUT)

 Note: The FPGA configuration supports a maximum of 6 IN and OUT endpoints in
 addition to EP0.
 </td></tr>

 <tr>
 <td>phy_type</td>
 <td>Specifies the type of PHY interface to use. By default, the driver will
 automatically detect the phy_type.
 - 0: Full Speed
 - 1: UTMI+ (default, if available)
 - 2: ULPI
 </td></tr>

 <tr>
 <td>phy_utmi_width</td>
 <td>Specifies the UTMI+ Data Width. This parameter is applicable for a
 phy_type of UTMI+. Also, this parameter is applicable only if the
 OTG_HSPHY_WIDTH cC parameter was set to "8 and 16 bits", meaning that the
 core has been configured to work at either data path width.
 - Values: 8 or 16 bits (default 16)
 </td></tr>

 <tr>
 <td>phy_ulpi_ddr</td>
 <td>Specifies whether the ULPI operates at double or single data rate. This
 parameter is only applicable if phy_type is ULPI.
 - 0: single data rate ULPI interface with 8 bit wide data bus (default)
 - 1: double data rate ULPI interface with 4 bit wide data bus
 </td></tr>

 <tr>
 <td>i2c_enable</td>
 <td>Specifies whether to use the I2C interface for full speed PHY. This
 parameter is only applicable if PHY_TYPE is FS.
 - 0: Disabled (default)
 - 1: Enabled
 </td></tr>

*/
