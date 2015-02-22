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
#include <linux/init.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysdev.h>
#include <asm/mach/time.h>
//#include <linux/clocksource.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>
#include <asm/arch/system.h>

#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <asm/serial.h>
#include <linux/proc_fs.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>

#include <asm/arch/serial.h>

#include "ctrlEnv/mvCtrlEnvLib.h"
#include "ctrlEnv/mvCtrlEthCompLib.h"
#include "ctrlEnv/sys/mvCpuIf.h"
#include "cpu/mvCpu.h"
#include "boardEnv/mvBoardEnvLib.h"
#include "mvDebug.h"
#include "mvSysHwConfig.h"
#include "pex/mvPexRegs.h"
#include "cntmr/mvCntmr.h"
#include "gpp/mvGpp.h"

#if defined(CONFIG_MV_INCLUDE_SDIO)
#include "ctrlEnv/sys/mvSysSdmmc.h"
#include <plat/mvsdio.h>
#endif
#if defined(CONFIG_MV_INCLUDE_CESA)
#include "cesa/mvCesa.h"
#endif
#ifdef CONFIG_MV_WATCHDOG
#include <feroceon_wdt.h>
#endif

/* I2C */
#include <linux/i2c.h>	
#include <linux/mv643xx_i2c.h>
#include "ctrlEnv/mvCtrlEnvSpec.h"

/* SPI */
#include "mvSysSpiApi.h"


/* Eth Phy */
#include "mvSysEthPhyApi.h"

extern unsigned int irq_int_type[];

/* for debug putstr */
#include <asm/arch/uncompress.h> 
static char arr[256];

#ifdef MV_INCLUDE_EARLY_PRINTK
extern void putstr(const char *ptr);
void mv_early_printk(char *fmt,...)
{
	va_list args;
	va_start(args, fmt);
	vsprintf(arr,fmt,args);
	va_end(args);
	putstr(arr);
}
#endif


extern void __init mv_map_io(void);
extern void __init mv_init_irq(void);
extern struct sys_timer mv_timer;
extern MV_CPU_DEC_WIN* mv_sys_map(void);
#if defined(CONFIG_MV_INCLUDE_CESA)
extern u32 mv_crypto_base_get(void);
#endif
unsigned int support_wait_for_interrupt = 0x1;

#ifdef CONFIG_MV_PMU_PROC
struct proc_dir_entry *mv_pm_proc_entry;
#endif /* CONFIG_MV_PMU_PROC */

u32 mvTclk = 166666667;
u32 mvSysclk = 200000000;
u32 mvIsUsbHost = 1;
u32 mod_config = 0;

#ifdef CONFIG_MV_INCLUDE_GIG_ETH
u8	mvMacAddr[CONFIG_MV_ETH_PORTS_NUM][6];
u16	mvMtu[CONFIG_MV_ETH_PORTS_NUM] = {0};
#endif
extern MV_U32 gBoardId; 
extern unsigned int elf_hwcap;

#ifdef CONFIG_MV_NAND
extern unsigned int mv_nand_ecc;
#endif

static int __init parse_tag_mv_uboot(const struct tag *tag)
{
    	unsigned int mvUbootVer = 0;
	int i = 0;
 
	mvUbootVer = tag->u.mv_uboot.uboot_version;
	mvIsUsbHost = tag->u.mv_uboot.isUsbHost;
	if (mvIsUsbHost != 0)
		mvIsUsbHost = 1;
        printk("Using UBoot passing parameters structure\n");
  
	gBoardId =  (mvUbootVer & 0xff);
#ifdef CONFIG_MV_INCLUDE_GIG_ETH
	for (i = 0; i < CONFIG_MV_ETH_PORTS_NUM; i++) {
#if defined (CONFIG_OVERRIDE_ETH_CMDLINE)
		memset(mvMacAddr[i], 0, 6);
		mvMtu[i] = 0;
#else
		memcpy(mvMacAddr[i], tag->u.mv_uboot.macAddr[i], 6);
		mvMtu[i] = tag->u.mv_uboot.mtu[i];
#endif
	}
#endif
#ifdef CONFIG_MV_NAND
               /* get NAND ECC type(1-bit or 4-bit) */
               if((mvUbootVer >> 8) >= 0x3040c)
                       mv_nand_ecc = tag->u.mv_uboot.nand_ecc;
               else
                       mv_nand_ecc = 1; /* fallback to 1-bit ECC */
#endif

	mod_config = tag->u.mv_uboot.mod_bitmask;

	return 0;
}

__tagtable(ATAG_MV_UBOOT, parse_tag_mv_uboot);

#ifdef CONFIG_MV_INCLUDE_CESA
unsigned char*  mv_sram_usage_get(int* sram_size_ptr)
{
	int used_size = 0;

#if defined(CONFIG_MV_CESA)
	used_size = sizeof(MV_CESA_SRAM_MAP);
#endif

	if(sram_size_ptr != NULL)
		*sram_size_ptr = _8K - used_size;

	return (char *)(mv_crypto_base_get() + used_size);
}
#endif


void print_board_info(void)
{
	char name_buff[50];
	printk("\n  Marvell Development Board (LSP Version %s)",LSP_VERSION);

	mvBoardNameGet(name_buff);
	printk("-- %s ",name_buff);

	mvCtrlModelRevNameGet(name_buff);
	printk(" Soc: %s",  name_buff);
#if defined(MV_CPU_LE)
	printk(" LE");
#else
	printk(" BE");
#endif
	printk("\n\n");
	printk(" Detected Tclk %d and SysClk %d \n",mvTclk, mvSysclk);
}

/*****************************************************************************
 * I2C(TWSI)
 ****************************************************************************/

/*Platform devices list*/

static struct mv64xxx_i2c_pdata kw_i2c_pdata = {
       .freq_m         = 8, /* assumes 166 MHz TCLK */
       .freq_n         = 3,
       .timeout        = 1000, /* Default timeout of 1 second */
};

static struct resource kw_i2c_resources[] = {
       {
               .name   = "i2c base",
               .start  = INTER_REGS_BASE + MV_TWSI_SLAVE_REGS_OFFSET(0),
               .end    = INTER_REGS_BASE + MV_TWSI_SLAVE_REGS_OFFSET(0) + 0x20 -1,
               .flags  = IORESOURCE_MEM,
       },
       {
               .name   = "i2c irq",
               .start  = TWSI_IRQ_NUM(0),
               .end    = TWSI_IRQ_NUM(0),
               .flags  = IORESOURCE_IRQ,
       },
};

static struct platform_device kw_i2c = {
       .name           = MV64XXX_I2C_CTLR_NAME,
       .id             = 0,
       .num_resources  = ARRAY_SIZE(kw_i2c_resources),
       .resource       = kw_i2c_resources,
       .dev            = {
               .platform_data = &kw_i2c_pdata,
       },
};

/*****************************************************************************
 * NAND controller
 ****************************************************************************/

struct mtd_partition nand_parts_info[] = {
	{ .name = "u-boot",
	  .offset = 0,
	  .size = 2 * 1024 * 1024 },
	{ .name = "uImage",
	  .offset = MTDPART_OFS_NXTBLK,
	  .size = 4 * 1024 * 1024 },
	{ .name = "root",
	  .offset = MTDPART_OFS_NXTBLK,
	  .size = MTDPART_SIZ_FULL },
};
int nand_parts_num = 3;



/*****************************************************************************
 * UART
 ****************************************************************************/
static struct resource mv_uart0_resources[] = {
	{
		.start		= PORT0_BASE,
		.end		= PORT0_BASE + 0xff,
		.flags		= IORESOURCE_MEM,
	},
	{
#ifdef CONFIG_MV_UART_POLLING_MODE
		.start          = 0,
		.end            = 0,
#else
		.start          = UART_IRQ_NUM(0),
		.end            = UART_IRQ_NUM(0),
#endif
		.flags          = IORESOURCE_IRQ,
	},
};

#if 0
static struct resource mv_uart1_resources[] = {
	{
		.start		= PORT1_BASE,
		.end		= PORT1_BASE + 0xff,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start          = UART_IRQ_NUM(1),
		.end            = UART_IRQ_NUM(1),
		.flags          = IORESOURCE_IRQ,
	},
};
#endif

static struct plat_serial8250_port mv_uart0_data[] = {
	{
		.mapbase	= PORT0_BASE,
		.membase	= (char *)PORT0_BASE,
#ifdef CONFIG_MV_UART_POLLING_MODE
		.irq		= 0, //UART_IRQ_NUM(0),
#else
		.irq		= UART_IRQ_NUM(0),
#endif
		.flags		= UPF_SKIP_TEST | UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_DWAPB,
		.private_data	= (void *)0xf101207c,
		.regshift	= 2,
	},
	{ },
};

static struct plat_serial8250_port mv_uart1_data[] = {
	{
		.mapbase	= PORT1_BASE,
		.membase	= (char *)PORT1_BASE,
		.irq		= UART_IRQ_NUM(1),
		.flags		= UPF_SKIP_TEST | UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_DWAPB,
		.private_data	= (void *)0xf101217c,
		.regshift	= 2,
	},
	{ },
};

static struct platform_device mv_uart = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev			= {
		.platform_data	= mv_uart0_data,
	},
	.num_resources		= 2, /*ARRAY_SIZE(mv_uart_resources),*/
	.resource		= mv_uart0_resources,
};


static void serial_initialize(void)
{
	mv_uart0_data[0].uartclk = mv_uart1_data[0].uartclk = mvTclk;
	platform_device_register(&mv_uart);
}

#if defined(CONFIG_MV_INCLUDE_SDIO)

static struct resource mvsdio_resources[] = {
	[0] = {
		.start	= INTER_REGS_BASE + MV_SDMMC_REGS_OFFSET,
		.end	= INTER_REGS_BASE + MV_SDMMC_REGS_OFFSET + SZ_1K -1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SDIO_IRQ_NUM,
		.end	= SDIO_IRQ_NUM,
		.flags	= IORESOURCE_IRQ,
	},

};

static u64 mvsdio_dmamask = 0xffffffffUL;

static struct mvsdio_platform_data mvsdio_data = {
	.gpio_write_protect	= 0,
	.gpio_card_detect	= 0,
	.dram			= NULL,
};

static struct platform_device mv_sdio_plat = {
	.name		= "mvsdio",
	.id		= -1,
	.dev		= {
		.dma_mask = &mvsdio_dmamask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data	= &mvsdio_data,
	},
	.num_resources	= ARRAY_SIZE(mvsdio_resources),
	.resource	= mvsdio_resources,
};


#endif /* #if defined(CONFIG_MV_INCLUDE_SDIO) */

#ifdef CONFIG_MV_ETHERNET
/*****************************************************************************
 * Ethernet
 ****************************************************************************/
static struct platform_device mv88fx_eth = {
	.name		= "mv88fx_eth",
	.id		= 0,
	.num_resources	= 0,
};
#endif

#ifdef CONFIG_MV_WATCHDOG
static struct feroceon_wdt_platform_data feroceon_wdt_data = {
        .tclk           = 0,
};

static struct platform_device feroceon_wdt = {
	.name		= "feroceon_wdt",
	.id		= -1,
        .dev            = {
                .platform_data  = &feroceon_wdt_data,
        },
	.num_resources	= 0,
};
#endif

static void __init mv_init(void)
{

	mvEthCompSkipInitSet(MV_TRUE);

	/* Do not rely on U-Boot for module detection - as this causes a dependency between Linux and U-Boot */
	/* mvBoardModuleConfigSet(mod_config); */

        /* init the Board environment */
       	mvBoardEnvInit();

        /* init the controller environment */
        if( mvCtrlEnvInit() ) {
            printk( "Controller env initialization failed.\n" );
            return;
        }

	/* Init the CPU windows setting and the access protection windows. */
	if( mvCpuIfInit(mv_sys_map()) ) {

		printk( "Cpu Interface initialization failed.\n" );
		return;
	}

    	/* Init Tclk & SysClk */
    	mvTclk = mvBoardTclkGet();
   	mvSysclk = mvBoardSysClkGet();

        support_wait_for_interrupt = 1;

#ifdef CONFIG_JTAG_DEBUG
	support_wait_for_interrupt = 0; /*  for Lauterbach */
#endif

	elf_hwcap &= ~HWCAP_JAVA;

	serial_initialize();

	/* At this point, the CPU windows are configured according to default definitions in mvSysHwConfig.h */
	/* and cpuAddrWinMap table in mvCpuIf.c. Now it's time to change defaults for each platform.         */
	mvCpuIfAddDecShow();

	print_board_info();

	//mv_gpio_init();

#ifdef CONFIG_MV_INCLUDE_SPI
	/* SPI */
	mvSysSpiInit(0, _16M);
#endif

	/* ETH-PHY */
	mvSysEthPhyInit();

	/* I2C */
	platform_device_register(&kw_i2c);

#ifdef CONFIG_MV_WATCHDOG
	feroceon_wdt_data.tclk = mvTclk;
	platform_device_register(&feroceon_wdt);
#endif

#ifdef CONFIG_MV_PMU_PROC
	mv_pm_proc_entry = proc_mkdir("mv_pm", NULL);
#endif /* CONFIG_MV_PMU_PROC */

#if defined(CONFIG_MV_INCLUDE_SDIO)
	if (MV_TRUE == mvCtrlPwrClckGet(SDIO_UNIT_ID, 0)) {
		int irq_detect = mvBoardSDIOGpioPinGet(BOARD_GPP_SDIO_DETECT);
		MV_UNIT_WIN_INFO addrWinMap[MAX_TARGETS + 1];

		if (irq_detect != MV_ERROR) {
			mvsdio_data.gpio_card_detect = mvBoardSDIOGpioPinGet(BOARD_GPP_SDIO_DETECT);
			irq_int_type[mvBoardSDIOGpioPinGet(BOARD_GPP_SDIO_DETECT)+IRQ_GPP_START] = GPP_IRQ_TYPE_CHANGE_LEVEL;
		}

		if(mvBoardSDIOGpioPinGet(BOARD_GPP_SDIO_WP) != MV_ERROR)
			mvsdio_data.gpio_write_protect = mvBoardSDIOGpioPinGet(BOARD_GPP_SDIO_WP);

		if(MV_OK == mvCtrlAddrWinMapBuild(addrWinMap, MAX_TARGETS + 1))
			if (MV_OK == mvSdmmcWinInit(addrWinMap))
				mvsdio_data.clock = 200000000; //mvBoardTclkGet();
		platform_device_register(&mv_sdio_plat);
       }
#endif

#ifdef CONFIG_MV_ETHERNET
	/* ethernet */
	platform_device_register(&mv88fx_eth);
#endif

	return;
}


MACHINE_START(FEROCEON_KW2 ,"Feroceon-KW2")
	/* MAINTAINER("MARVELL") */
	.phys_io = 0xf1000000,
	.io_pg_offst = ((0xf1000000) >> 18) & 0xfffc,
	.boot_params = 0x00000100,
	.map_io = mv_map_io,
	.init_irq = mv_init_irq,
	.timer = &mv_timer,
	.init_machine = mv_init,
MACHINE_END

