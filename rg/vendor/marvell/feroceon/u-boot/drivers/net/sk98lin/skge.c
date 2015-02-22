/******************************************************************************
 *
 * Name:        skge.c
 * Project:     GEnesis, PCI Gigabit Ethernet Adapter
 * Version:     $Revision: 1.60.2.68 $
 * Date:        $Date: 2005/11/14 15:22:08 $
 * Purpose:     The main driver source module
 *
 ******************************************************************************/

/******************************************************************************
 *
 *	(C)Copyright 1998-2002 SysKonnect GmbH.
 *	(C)Copyright 2002-2005 Marvell.
 *
 *	Driver for Marvell Yukon chipset and SysKonnect Gigabit Ethernet 
 *      Server Adapters.
 *
 *	Author: Mirko Lindner (mlindner@syskonnect.de)
 *	        Ralph Roesler (rroesler@syskonnect.de)
 *
 *	Address all question to: linux@syskonnect.de
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Description:
 *
 *	All source files in this sk98lin directory except of the sk98lin 
 *	Linux specific files
 *
 *		- skdim.c
 *		- skethtool.c
 *		- skge.c
 *		- skproc.c
 *		- sky2.c
 *		- Makefile
 *		- h/skdrv1st.h
 *		- h/skdrv2nd.h
 *		- h/sktypes.h
 *		- h/skversion.h
 *
 *	are part of SysKonnect's common modules for the SK-9xxx adapters.
 *
 *	Those common module files which are not Linux specific are used to 
 *	build drivers on different OS' (e.g. Windows, MAC OS) so that those
 *	drivers are based on the same set of files
 *
 *	At a first glance, this seems to complicate things unnescessarily on 
 *	Linux, but please do not try to 'clean up' them without VERY good 
 *	reasons, because this will make it more difficult to keep the sk98lin
 *	driver for Linux in synchronisation with the other drivers running on
 *	other operating systems.
 *
 ******************************************************************************/
/*#define DEBUG*/
#include <config.h>
#ifdef CONFIG_SK98

#include	"h/skversion.h"

#if 0 /* uboot */
#include	<linux/module.h>
#include	<linux/init.h>
#include	<linux/ethtool.h>
#endif

#ifdef CONFIG_PROC_FS
#include 	<linux/proc_fs.h>
#endif

#include	"h/skdrv1st.h"
#include	"h/skdrv2nd.h"


/*******************************************************************************
 *
 * Defines
 *
 ******************************************************************************/

/* for debuging on x86 only */
/* #define BREAKPOINT() asm(" int $3"); */


/* Set blink mode*/
#define OEM_CONFIG_VALUE (	SK_ACT_LED_BLINK | \
				SK_DUP_LED_NORMAL | \
				SK_LED_LINK100_ON)

#define CLEAR_AND_START_RX(Port) SK_OUT8(pAC->IoBase, RxQueueAddr[(Port)]+Q_CSR, CSR_START | CSR_IRQ_CL_F)
#define CLEAR_TX_IRQ(Port,Prio) SK_OUT8(pAC->IoBase, TxQueueAddr[(Port)][(Prio)]+Q_CSR, CSR_IRQ_CL_F)


/*******************************************************************************
 *
 * Local Function Prototypes
 *
 ******************************************************************************/

#if 0 /* uboot */
static int 	__devinit sk98lin_init_device(struct pci_dev *pdev, const struct pci_device_id *ent);
static void 	sk98lin_remove_device(struct pci_dev *pdev);
#endif
#ifdef CONFIG_PM
static int	sk98lin_suspend(struct pci_dev *pdev, u32 state);
static int	sk98lin_resume(struct pci_dev *pdev);
static void	SkEnableWOMagicPacket(SK_AC *pAC, SK_IOC IoC, SK_MAC_ADDR MacAddr);
#endif
#ifdef Y2_RECOVERY
static void	SkGeHandleKernelTimer(unsigned long ptr);
void		SkGeCheckTimer(DEV_NET *pNet);
static SK_BOOL  CheckRXCounters(DEV_NET *pNet);
static void	CheckRxPath(DEV_NET *pNet);
#endif
static void	FreeResources(struct SK_NET_DEVICE *dev);
static int	SkGeBoardInit(struct SK_NET_DEVICE *dev, SK_AC *pAC);
static SK_BOOL	BoardAllocMem(SK_AC *pAC);
static void	BoardFreeMem(SK_AC *pAC);
static void	BoardInitMem(SK_AC *pAC);
static void	SetupRing(SK_AC*, void*, uintptr_t, RXD**, RXD**, RXD**, int*, int*, SK_BOOL);
#if 0 /* uboot */
static SkIsrRetVar	SkGeIsr(int irq, void *dev_id, struct pt_regs *ptregs);
static SkIsrRetVar	SkGeIsrOnePort(int irq, void *dev_id, struct pt_regs *ptregs);
static int	SkGeOpen(struct SK_NET_DEVICE *dev);
static int	SkGeClose(struct SK_NET_DEVICE *dev);
static int	SkGeXmit(struct sk_buff *skb, struct SK_NET_DEVICE *dev);
static int	SkGeSetMacAddr(struct SK_NET_DEVICE *dev, void *p);
static void	SkGeSetRxMode(struct SK_NET_DEVICE *dev);
static struct	net_device_stats *SkGeStats(struct SK_NET_DEVICE *dev);
static int	SkGeIoctl(struct SK_NET_DEVICE *dev, struct ifreq *rq, int cmd);
#else
SkIsrRetVar	SkGeIsr(int irq, void *dev_id, struct pt_regs *ptregs);
SkIsrRetVar	SkGeIsrOnePort(int irq, void *dev_id, struct pt_regs *ptregs);
int	SkGeOpen(struct SK_NET_DEVICE *dev);
int	SkGeClose(struct SK_NET_DEVICE *dev);
int	SkGeXmit(struct sk_buff *skb, struct SK_NET_DEVICE *dev);
#endif
static void	GetConfiguration(SK_AC*);
#if 0 /* uboot */
static void	ProductStr(SK_AC*);
#endif
static int	XmitFrame(SK_AC*, TX_PORT*, struct sk_buff*);
static void	FreeTxDescriptors(SK_AC*pAC, TX_PORT*);
static void	FillRxRing(SK_AC*, RX_PORT*);
static SK_BOOL	FillRxDescriptor(SK_AC*, RX_PORT*);
#ifdef CONFIG_SK98LIN_NAPI
static int	SkGePoll(struct net_device *dev, int *budget);
static void	ReceiveIrq(SK_AC*, RX_PORT*, SK_BOOL, int*, int);
#else
#if 0 /* uboot */
static void	ReceiveIrq(SK_AC*, RX_PORT*, SK_BOOL);
#else
void	ReceiveIrq(SK_AC*, RX_PORT*, SK_BOOL);
#endif
#endif
#ifdef SK_POLL_CONTROLLER
static void	SkGeNetPoll(struct SK_NET_DEVICE *dev);
#endif
static void	ClearRxRing(SK_AC*, RX_PORT*);
static void	ClearTxRing(SK_AC*, TX_PORT*);
#if 0 /* uboot */
static int	SkGeChangeMtu(struct SK_NET_DEVICE *dev, int new_mtu);
#endif
static void	PortReInitBmu(SK_AC*, int);
#if 0 /* uboot */
static int	SkGeIocMib(DEV_NET*, unsigned int, int);
static int	SkGeInitPCI(SK_AC *pAC);
static SK_U32   ParseDeviceNbrFromSlotName(const char *SlotName);
#endif
static int      SkDrvInitAdapter(SK_AC *pAC, int devNbr);
static int      SkDrvDeInitAdapter(SK_AC *pAC, int devNbr);
extern void	SkLocalEventQueue(	SK_AC *pAC,
					SK_U32 Class,
					SK_U32 Event,
					SK_U32 Param1,
					SK_U32 Param2,
					SK_BOOL Flag);
extern void	SkLocalEventQueue64(	SK_AC *pAC,
					SK_U32 Class,
					SK_U32 Event,
					SK_U64 Param,
					SK_BOOL Flag);
#if 0 /* uboot */
static int	XmitFrameSG(SK_AC*, TX_PORT*, struct sk_buff*);
#endif

/*******************************************************************************
 *
 * Extern Function Prototypes
 *
 ******************************************************************************/

extern SK_BOOL SkY2AllocateResources(SK_AC *pAC);
extern void SkY2FreeResources(SK_AC *pAC);
extern void SkY2AllocateRxBuffers(SK_AC *pAC,SK_IOC IoC,int Port);
extern void SkY2FreeRxBuffers(SK_AC *pAC,SK_IOC IoC,int Port);
extern void SkY2FreeTxBuffers(SK_AC *pAC,SK_IOC IoC,int Port);
extern SkIsrRetVar SkY2Isr(int irq,void *dev_id,struct pt_regs *ptregs);
extern int SkY2Xmit(struct sk_buff *skb,struct SK_NET_DEVICE *dev);
extern void SkY2PortStop(SK_AC *pAC,SK_IOC IoC,int Port,int Dir,int RstMode);
extern void SkY2PortStart(SK_AC *pAC,SK_IOC IoC,int Port);
extern int SkY2RlmtSend(SK_AC *pAC,int PortNr,struct sk_buff *pMessage);
extern void SkY2RestartStatusUnit(SK_AC *pAC);
extern void FillReceiveTableYukon2(SK_AC *pAC,SK_IOC IoC,int Port);
#ifdef CONFIG_SK98LIN_NAPI
extern int SkY2Poll(struct net_device *dev, int *budget);
#endif

extern void SkDimEnableModerationIfNeeded(SK_AC *pAC);	
extern void SkDimStartModerationTimer(SK_AC *pAC);
extern void SkDimModerate(SK_AC *pAC);


#if 0/* uboot */
extern int SkEthIoctl(struct net_device *netdev, struct ifreq *ifr);
#endif

#ifdef CONFIG_PROC_FS
static const char 	SK_Root_Dir_entry[] = "sk98lin";
static struct		proc_dir_entry *pSkRootDir;
extern struct	file_operations sk_proc_fops;
#endif

#ifdef DEBUG
static void	DumpMsg(struct sk_buff*, char*);
static void	DumpData(char*, int);
static void	DumpLong(char*, int);
#endif

/* global variables *********************************************************/
#if 0 /* uboot */
static const char *BootString = BOOT_STRING;
#endif
struct SK_NET_DEVICE *SkGeRootDev = NULL;
static SK_BOOL DoPrintInterfaceChange = SK_TRUE;

/* local variables **********************************************************/
static uintptr_t TxQueueAddr[SK_MAX_MACS][2] = {{0x680, 0x600},{0x780, 0x700}};
static uintptr_t RxQueueAddr[SK_MAX_MACS] = {0x400, 0x480};
#if 0 /* uboot */
static int sk98lin_max_boards_found = 0;
#endif

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry	*pSkRootDir;
#endif




#if 0 /* uboot */
static struct pci_device_id sk98lin_pci_tbl[] __devinitdata = {
/*	{ pci_vendor_id, pci_device_id, * SAMPLE ENTRY! *
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL }, */
	{ 0x10b7, 0x1700, /* 3Com (10b7), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x10b7, 0x80eb, /* 3Com (10b7), 3Com 3C940B Gigabit LOM Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1148, 0x4300, /* SysKonnect (1148), SK-98xx Gigabit Ethernet Server Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1148, 0x4320, /* SysKonnect (1148), SK-98xx V2.0 Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1148, 0x9000, /* SysKonnect (1148), SK-9Sxx 10/100/1000Base-T Server Adapter  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1148, 0x9E00, /* SysKonnect (1148), SK-9Exx 10/100/1000Base-T Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1186, 0x4b00, /* D-Link (1186), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1186, 0x4b01, /* D-Link (1186), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1186, 0x4b02, /* D-Link (1186), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1186, 0x4c00, /* D-Link (1186), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4320, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4340, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4341, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4342, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4343, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4344, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4345, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4346, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4347, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4350, /* Marvell (11ab), Fast Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4351, /* Marvell (11ab), Fast Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4352, /* Marvell (11ab), Fast Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4356, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4360, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4361, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4362, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4363, /* Marvell (11ab), Marvell */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4364, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4366, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4367, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4368, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x5005, /* Marvell (11ab), Belkin */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1371, 0x434e, /* CNet (1371), GigaCard Network Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1737, 0x1032, /* Linksys (1737), Gigabit Network Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1737, 0x1064, /* Linksys (1737), Gigabit Network Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, sk98lin_pci_tbl);

static struct pci_driver sk98lin_driver = {
	.name		= DRIVER_FILE_NAME,
	.id_table	= sk98lin_pci_tbl,
	.probe		= sk98lin_init_device,
	.remove		= __devexit_p(sk98lin_remove_device),
#ifdef CONFIG_PM
	.suspend	= sk98lin_suspend,
	.resume		= sk98lin_resume
#endif
};


#else

static struct pci_device_id supported[] = {
/*	{ pci_vendor_id, pci_device_id, * SAMPLE ENTRY! *
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL }, */
	{ 0x10b7, 0x1700, /* 3Com (10b7), Gigabit Ethernet Adapter */},
	{ 0x1148, 0x4300, /* SysKonnect (1148), SK-98xx Gigabit Ethernet Server Adapter */},
	{ 0x1148, 0x4320, /* SysKonnect (1148), SK-98xx V2.0 Gigabit Ethernet Adapter */},
	{ 0x1148, 0x4340, /* SysKonnect (1148), SK-9Sxx 10/100/1000Base-T Server Adapter  */},
	{ 0x1148, 0x9000, /* SysKonnect (1148), SK-9Sxx 10/100/1000Base-T Server Adapter  */},
	{ 0x1148, 0x9E00, /* SysKonnect (1148), SK-9Exx 10/100/1000Base-T Adapter */},
	{ 0x1186, 0x4c00, /* D-Link (1186), Gigabit Ethernet Adapter */},
	{ 0x11ab, 0x4320, /* Marvell (11ab), Gigabit Ethernet Controller */},
	{ 0x11ab, 0x4350, /* Marvell (11ab), Fast Ethernet Controller */},
	{ 0x11ab, 0x4351, /* Marvell (11ab), Fast Ethernet Controller */},
	{ 0x11ab, 0x4360, /* Marvell (11ab), Gigabit Ethernet Controller */},
	{ 0x11ab, 0x4361, /* Marvell (11ab), Gigabit Ethernet Controller */},
	{ 0x11ab, 0x4362, /* Marvell (11ab), Gigabit Ethernet Controller */},
	{ 0x11ab, 0x4363, /* Marvell (11ab), Gigabit Ethernet Controller */},
	{ 0x11ab, 0x5005, /* Marvell (11ab), Belkin */},
	{ 0x1371, 0x434e, /* CNet (1371), GigaCard Network Adapter */},
	{ 0x1737, 0x1032, /* Linksys (1737), Gigabit Network Adapter */},
	{ 0x1737, 0x1064, /* Linksys (1737), Gigabit Network Adapter */},
	{ }
};

#endif

#if 0
/*****************************************************************************
 *
 * 	sk98lin_init_device - initialize the adapter
 *
 * Description:
 *	This function initializes the adapter. Resources for
 *	the adapter are allocated and the adapter is brought into Init 1
 *	state.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static int __devinit sk98lin_init_device(struct pci_dev *pdev,
				  const struct pci_device_id *ent)

{
	static SK_BOOL 		sk98lin_boot_string = SK_FALSE;
	static SK_BOOL 		sk98lin_proc_entry = SK_FALSE;
	static int		sk98lin_boards_found = 0;
	SK_AC			*pAC;
	DEV_NET			*pNet = NULL;
	struct SK_NET_DEVICE *dev = NULL;
	int			retval;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry	*pProcFile;
#endif
	int			pci_using_dac;

#if 0 /* uboot */
	retval = pci_enable_device(pdev);
	if (retval) {
		printk(KERN_ERR "Cannot enable PCI device, "
			"aborting.\n");
		return retval;
	}
#endif

	dev = NULL;
	pNet = NULL;


	/* INSERT * We have to find the power-management capabilities */
	/* Find power-management capability. */

	pci_using_dac = 0;		/* Set 32 bit DMA per default */
	/* Configure DMA attributes. */
	retval = pci_set_dma_mask(pdev, (u64) 0xffffffffffffffffULL);
	if (!retval) {
		pci_using_dac = 1;
	} else {
		retval = pci_set_dma_mask(pdev, (u64) 0xffffffff);
		if (retval) {
			printk(KERN_ERR "No usable DMA configuration, "
			       "aborting.\n");
			return retval;
		}
	}


	if ((dev = alloc_etherdev(sizeof(DEV_NET))) == NULL) {
		printk(KERN_ERR "Unable to allocate etherdev "
			"structure!\n");
		return -ENODEV;
	}

	pNet = dev->priv;
	pNet->pAC = kmalloc(sizeof(SK_AC), GFP_KERNEL);
	if (pNet->pAC == NULL){
		free_netdev(dev);
		printk(KERN_ERR "Unable to allocate adapter "
			"structure!\n");
		return -ENODEV;
	}


	/* Print message */
	if (!sk98lin_boot_string) {
		/* set display flag to TRUE so that */
		/* we only display this string ONCE */
		sk98lin_boot_string = SK_TRUE;
		printk("%s\n", BootString);
	}

	memset(pNet->pAC, 0, sizeof(SK_AC));
	pAC = pNet->pAC;
	pAC->PciDev = pdev;
	pAC->PciDevId = pdev->device;
	pAC->dev[0] = dev;
	pAC->dev[1] = dev;
	sprintf(pAC->Name, "SysKonnect SK-98xx");
	pAC->CheckQueue = SK_FALSE;

	dev->irq = pdev->irq;
	retval = SkGeInitPCI(pAC);
	if (retval) {
		printk("SKGE: PCI setup failed: %i\n", retval);
		free_netdev(dev);
		return -ENODEV;
	}

	SET_MODULE_OWNER(dev);

	dev->open		=  &SkGeOpen;
	dev->stop		=  &SkGeClose;
	dev->get_stats		=  &SkGeStats;
	dev->set_multicast_list	=  &SkGeSetRxMode;
	dev->set_mac_address	=  &SkGeSetMacAddr;
	dev->do_ioctl		=  &SkGeIoctl;
	dev->change_mtu		=  &SkGeChangeMtu;
	dev->flags		&= ~IFF_RUNNING;
#ifdef SK_POLL_CONTROLLER
	dev->poll_controller	=  SkGeNetPoll;
#endif
	SET_NETDEV_DEV(dev, &pdev->dev);

	pAC->Index = sk98lin_boards_found;

	if (SkGeBoardInit(dev, pAC)) {
		free_netdev(dev);
		return -ENODEV;
	} else {
		ProductStr(pAC);
	}

	if (pci_using_dac)
		dev->features |= NETIF_F_HIGHDMA;

	/* shifter to later moment in time... */
	if (CHIP_ID_YUKON_2(pAC)) {
		dev->hard_start_xmit =	&SkY2Xmit;
#ifdef CONFIG_SK98LIN_NAPI
		dev->poll =  &SkY2Poll;
		dev->weight = 64;
#endif
	} else {
		dev->hard_start_xmit =	&SkGeXmit;
#ifdef CONFIG_SK98LIN_NAPI
		dev->poll =  &SkGePoll;
		dev->weight = 64;
#endif
	}

#ifdef NETIF_F_TSO
#ifdef USE_SK_TSO_FEATURE	
	if ((CHIP_ID_YUKON_2(pAC)) && 
		(pAC->GIni.GIChipId != CHIP_ID_YUKON_EC_U)) {
		dev->features |= NETIF_F_TSO;
	}
#endif
#endif
#ifdef CONFIG_SK98LIN_ZEROCOPY
	if (pAC->GIni.GIChipId != CHIP_ID_GENESIS)
		dev->features |= NETIF_F_SG;
#endif
#ifdef USE_SK_TX_CHECKSUM
	if (pAC->GIni.GIChipId != CHIP_ID_GENESIS)
		dev->features |= NETIF_F_IP_CSUM;
#endif
#ifdef USE_SK_RX_CHECKSUM
	pAC->RxPort[0].UseRxCsum = SK_TRUE;
	if (pAC->GIni.GIMacsFound == 2 ) {
		pAC->RxPort[1].UseRxCsum = SK_TRUE;
	}
#endif

	/* Save the hardware revision */
	pAC->HWRevision = (((pAC->GIni.GIPciHwRev >> 4) & 0x0F)*10) +
		(pAC->GIni.GIPciHwRev & 0x0F);

	/* Set driver globals */
	pAC->Pnmi.pDriverFileName    = DRIVER_FILE_NAME;
	pAC->Pnmi.pDriverReleaseDate = DRIVER_REL_DATE;

	SK_MEMSET(&(pAC->PnmiBackup), 0, sizeof(SK_PNMI_STRUCT_DATA));
	SK_MEMCPY(&(pAC->PnmiBackup), &(pAC->PnmiStruct), 
			sizeof(SK_PNMI_STRUCT_DATA));

	/* Register net device */
	retval = register_netdev(dev);
	if (retval) {
		printk(KERN_ERR "SKGE: Could not register device.\n");
		FreeResources(dev);
		free_netdev(dev);
		return retval;
	}

	/* Save initial device name */
	strcpy(pNet->InitialDevName, dev->name);

	/* Set network to off */
	netif_stop_queue(dev);
	netif_carrier_off(dev);

	/* Print adapter specific string from vpd and config settings */
	printk("%s: %s\n", pNet->InitialDevName, pAC->DeviceStr);
	printk("      PrefPort:%c  RlmtMode:%s\n",
		'A' + pAC->Rlmt.Net[0].Port[pAC->Rlmt.Net[0].PrefPort]->PortNumber,
		(pAC->RlmtMode==0)  ? "Check Link State" :
		((pAC->RlmtMode==1) ? "Check Link State" :
		((pAC->RlmtMode==3) ? "Check Local Port" :
		((pAC->RlmtMode==7) ? "Check Segmentation" :
		((pAC->RlmtMode==17) ? "Dual Check Link State" :"Error")))));

	SkGeYellowLED(pAC, pAC->IoBase, 1);

	memcpy((caddr_t) &dev->dev_addr,
		(caddr_t) &pAC->Addr.Net[0].CurrentMacAddress, 6);

	/* First adapter... Create proc and print message */
#ifdef CONFIG_PROC_FS
	if (!sk98lin_proc_entry) {
		sk98lin_proc_entry = SK_TRUE;
		SK_MEMCPY(&SK_Root_Dir_entry, BootString,
			sizeof(SK_Root_Dir_entry) - 1);

		/*Create proc (directory)*/
		if(!pSkRootDir) {
			pSkRootDir = proc_mkdir(SK_Root_Dir_entry, proc_net);
			if (!pSkRootDir) {
				printk(KERN_WARNING "%s: Unable to create /proc/net/%s",
					dev->name, SK_Root_Dir_entry);
			} else {
				pSkRootDir->owner = THIS_MODULE;
			}
		}
	}

	/* Create proc file */
	if (pSkRootDir && 
		(pProcFile = create_proc_entry(pNet->InitialDevName, S_IRUGO,
			pSkRootDir))) {
		pProcFile->proc_fops = &sk_proc_fops;
		pProcFile->data      = dev;
	}

#endif

	pNet->PortNr = 0;
	pNet->NetNr  = 0;

	sk98lin_boards_found++;
	pci_set_drvdata(pdev, dev);

	/* More then one port found */
	if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 2)) {
		if ((dev = alloc_etherdev(sizeof(DEV_NET))) == 0) {
			printk(KERN_ERR "Unable to allocate etherdev "
				"structure!\n");
			return -ENODEV;
		}

		pAC->dev[1]   = dev;
		pNet          = dev->priv;
		pNet->PortNr  = 1;
		pNet->NetNr   = 1;
		pNet->pAC     = pAC;

		if (CHIP_ID_YUKON_2(pAC)) {
			dev->hard_start_xmit = &SkY2Xmit;
#ifdef CONFIG_SK98LIN_NAPI
			dev->poll =  &SkY2Poll;
			dev->weight = 64;
#endif
		} else {
			dev->hard_start_xmit = &SkGeXmit;
#ifdef CONFIG_SK98LIN_NAPI
			dev->poll =  &SkGePoll;
			dev->weight = 64;
#endif
		}
		dev->open               = &SkGeOpen;
		dev->stop               = &SkGeClose;
		dev->get_stats          = &SkGeStats;
		dev->set_multicast_list = &SkGeSetRxMode;
		dev->set_mac_address    = &SkGeSetMacAddr;
		dev->do_ioctl           = &SkGeIoctl;
		dev->change_mtu         = &SkGeChangeMtu;
		dev->flags             &= ~IFF_RUNNING;
#ifdef SK_POLL_CONTROLLER
		dev->poll_controller	= SkGeNetPoll;
#endif

#ifdef NETIF_F_TSO
#ifdef USE_SK_TSO_FEATURE	
		if ((CHIP_ID_YUKON_2(pAC)) && 
			(pAC->GIni.GIChipId != CHIP_ID_YUKON_EC_U)) {
			dev->features |= NETIF_F_TSO;
		}
#endif
#endif
#ifdef CONFIG_SK98LIN_ZEROCOPY
		/* Don't handle if Genesis chipset */
		if (pAC->GIni.GIChipId != CHIP_ID_GENESIS)
			dev->features |= NETIF_F_SG;
#endif
#ifdef USE_SK_TX_CHECKSUM
		/* Don't handle if Genesis chipset */
		if (pAC->GIni.GIChipId != CHIP_ID_GENESIS)
			dev->features |= NETIF_F_IP_CSUM;
#endif

		if (register_netdev(dev)) {
			printk(KERN_ERR "SKGE: Could not register device.\n");
			free_netdev(dev);
			pAC->dev[1] = pAC->dev[0];
		} else {

		/* Save initial device name */
		strcpy(pNet->InitialDevName, dev->name);

		/* Set network to off */
		netif_stop_queue(dev);
		netif_carrier_off(dev);


#ifdef CONFIG_PROC_FS
		if (pSkRootDir 
		    && (pProcFile = create_proc_entry(pNet->InitialDevName, 
						S_IRUGO, pSkRootDir))) {
			pProcFile->proc_fops = &sk_proc_fops;
			pProcFile->data      = dev;
		}
#endif

		memcpy((caddr_t) &dev->dev_addr,
		(caddr_t) &pAC->Addr.Net[1].CurrentMacAddress, 6);
	
		printk("%s: %s\n", pNet->InitialDevName, pAC->DeviceStr);
		printk("      PrefPort:B  RlmtMode:Dual Check Link State\n");
		}
	}

	pAC->Index = sk98lin_boards_found;
	sk98lin_max_boards_found = sk98lin_boards_found;
	return 0;
}



/*****************************************************************************
 *
 * 	SkGeInitPCI - Init the PCI resources
 *
 * Description:
 *	This function initialize the PCI resources and IO
 *
 * Returns: N/A
 *	
 */
int SkGeInitPCI(SK_AC *pAC)
{
	struct SK_NET_DEVICE *dev = pAC->dev[0];
	struct pci_dev *pdev = pAC->PciDev;
	int retval;

	if (pci_enable_device(pdev) != 0) {
		return 1;
	}

	dev->mem_start = pci_resource_start (pdev, 0);
	pci_set_master(pdev);

	if (pci_request_regions(pdev, DRIVER_FILE_NAME) != 0) {
		retval = 2;
		goto out_disable;
	}

#if defined (SK_BIG_ENDIAN)

                /*
                 * On big endian machines, we use the adapter's aibility of
                 * reading the descriptors as big endian.
                 */
                if (CHIP_ID_YUKON_2(pAC))
				{
				SK_U32          our2;
						SkPciReadCfgDWord(pAC, PCI_OUR_REG_2, &our2);
						our2 |= PCI_REV_DESC;
						SkPciWriteCfgDWord(pAC, PCI_OUR_REG_2, our2);
				}
				else
				{
                SK_U32          our2;
                        SkPciReadCfgDWord(pAC, PCI_OUR_REG_2, &our2);
                        our2 |= PCI_REV_DESC;
                        SkPciWriteCfgDWord(pAC, PCI_OUR_REG_2, our2);
                }
#else

				{
                SK_U32          our2;
                        SkPciReadCfgDWord(pAC, PCI_OUR_REG_2, &our2);
                        our2 &= ~PCI_REV_DESC;
                        SkPciWriteCfgDWord(pAC, PCI_OUR_REG_2, our2);
                }


#endif

	/*
	 * Remap the regs into kernel space.
	 */
	pAC->IoBase = (char*)ioremap_nocache(dev->mem_start, 0x4000);

	if (!pAC->IoBase){
		retval = 3;
		goto out_release;
	}

	return 0;

 out_release:
	pci_release_regions(pdev);
 out_disable:
	pci_disable_device(pdev);
	return retval;
}

#else
/*****************************************************************************
 *
 *      skge_probe - find all SK-98xx adapters
 *
 * Description:
 *      This function scans the PCI bus for SK-98xx adapters. Resources for
 *      each adapter are allocated and the adapter is brought into Init 1
 *      state.
 *
 * Returns:
 *      0, if everything is ok
 *      !=0, on error
 */
static int probed __initdata = 0; 

int skge_probe (struct eth_device ** ret_dev)
{
        int                     boards_found = 0;
        SK_AC                   *pAC;
        DEV_NET                 *pNet = NULL;
        u32                     base_address;
        struct SK_NET_DEVICE *dev = NULL;
        SK_BOOL BootStringCount = SK_FALSE;
        pci_dev_t devno;
 
        if (probed)
                return -ENODEV;
        probed++;
 
        if (!pci_present())             /* is PCI support present? */
                return -ENODEV;
 
                while(1)
                {
		if((devno = pci_find_devices (supported, boards_found)) < 0) {
                        break;
                }

 
                dev = NULL;
                pNet = NULL;
 
 
                dev = malloc (sizeof (struct SK_NET_DEVICE));
                memset(dev, 0, sizeof(struct SK_NET_DEVICE));
                dev->priv = malloc(sizeof(DEV_NET));
 
                if (dev->priv == NULL) {
                        printk(KERN_ERR "Unable to allocate adapter "
                               "structure!\n");
                        break;
                }
 
                pNet = dev->priv;
                pNet->pAC = kmalloc(sizeof(SK_AC), GFP_KERNEL);
                if (pNet->pAC == NULL){
                        kfree(dev->priv);
						printf("kfree: dev->priv1=0x%x\n",dev->priv);
                        printk(KERN_ERR "Unable to allocate adapter "
                               "structure!\n");

			#if 1 /* Marvell - uboot */
			return -ENODEV;
			#endif
                        /*break;*/
                }
 
                /* Print message */
                if (!BootStringCount) {
                        /* set display flag to TRUE so that */
                        /* we only display this string ONCE */
                        BootStringCount = SK_TRUE;
#ifdef SK98_INFO
                        printk("%s\n", BootString);
#endif
                }
 
                memset(pNet->pAC, 0, sizeof(SK_AC));
                pAC = pNet->pAC;
                pAC->PciDev = devno;
                ret_dev[boards_found] = pAC->dev[0] = dev;
                sprintf(pAC->Name, "SysKonnect SK-98xx");
                pAC->CheckQueue = SK_FALSE;
 
                pNet->Mtu = 1500;
                pNet->Up = 0;
 
#ifdef SK_ZEROCOPY
                if (pAC->GIni.GIChipId == CHIP_ID_YUKON) {
                        /* Use only if yukon hardware */
                        /* SK and ZEROCOPY - fly baby... */
                        dev->features |= NETIF_F_SG | NETIF_F_IP_CSUM;
                }
#endif
 
                pci_write_config_dword(devno,
                                       PCI_COMMAND,
                                       PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
                pci_read_config_dword (devno, PCI_BASE_ADDRESS_0,
                                       &base_address);


                /*
                 * Remap the regs into kernel space.
                 */
                pAC->IoBase =((char*)pci_mem_to_phys(devno, base_address));

		#if 1/* Marvell - uboot : make it aligned */
		pAC->IoBase = (SK_IOC)((unsigned int)pAC->IoBase & 0xFFFFFFF0);
		#endif


 
                if (!pAC->IoBase){
                        printk(KERN_ERR "%s:  Unable to map I/O register, "
                               "SK 98xx No. %i will be disabled.\n",
                               dev->name, boards_found);
                        kfree(dev);
						printf("kfree: dev3=0x%x\n",dev);
                        break;
                }



                pAC->Index = boards_found;
                if (SkGeBoardInit(dev, pAC)) {
                        FreeResources(dev);
                        kfree(dev);
						printf("kfree: dev4=0x%x\n",dev);
                        continue;
                }
 

                memcpy((caddr_t) &dev->enetaddr,
                        (caddr_t) &pAC->Addr.Net[0].CurrentMacAddress, 6);



 
                pNet->PortNr = 0;
                pNet->NetNr = 0;
 
                boards_found++;
 
                /* More then one port found */
                if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 2)) {
			printk(" NEVER TESTED \n");
                        dev = malloc (sizeof(struct SK_NET_DEVICE));
                        memcpy(dev, pAC->dev[0], sizeof(struct SK_NET_DEVICE));
                        dev->priv = malloc(sizeof(DEV_NET));
 			memcpy(dev->priv, pAC->dev[0]->priv, sizeof(DEV_NET));

			ret_dev[1] = pAC->dev[1] = dev;
			boards_found++;

                        pNet = dev->priv;
                        pNet->PortNr = 1;
                        pNet->NetNr = 1;
                        pNet->pAC = pAC;
                        pNet->Mtu = 1500;
                        pNet->Up = 0;
 
                         memcpy((caddr_t) &dev->enetaddr,
                        (caddr_t) &pAC->Addr.Net[1].CurrentMacAddress, 6);
 
                        printk("%s: %s\n", dev->name, pAC->DeviceStr);
                        printk("      PrefPort:B  RlmtMode:Dual Check Link State\n");
 
                }



 

                /* Save the hardware revision */
                pAC->HWRevision = (((pAC->GIni.GIPciHwRev >> 4) & 0x0F)*10) +
                        (pAC->GIni.GIPciHwRev & 0x0F);
#if 0

	printk("      PrefPort:%c  RlmtMode:%s\n",
		'A' + pAC->Rlmt.Net[0].Port[pAC->Rlmt.Net[0].PrefPort]->PortNumber,
		(pAC->RlmtMode==0)  ? "Check Link State" :
		((pAC->RlmtMode==1) ? "Check Link State" :
		((pAC->RlmtMode==3) ? "Check Local Port" :
		((pAC->RlmtMode==7) ? "Check Segmentation" :
		((pAC->RlmtMode==17) ? "Dual Check Link State" :"Error")))));

	SkGeYellowLED(pAC, pAC->IoBase, 1);
#endif

#if defined (SK_BIG_ENDIAN)

                /*
                 * On big endian machines, we use the adapter's aibility of
                 * reading the descriptors as big endian.
                 */
                if (CHIP_ID_YUKON_2(pAC))
				{
				SK_U32          our2;
						SkPciReadCfgDWord(pAC, PCI_OUR_REG_2, &our2);
						our2 |= PCI_REV_DESC;
						SkPciWriteCfgDWord(pAC, PCI_OUR_REG_2, our2);
				}
				else
				{
                SK_U32          our2;
                        SkPciReadCfgDWord(pAC, PCI_OUR_REG_2, &our2);
                        our2 |= PCI_REV_DESC;
                        SkPciWriteCfgDWord(pAC, PCI_OUR_REG_2, our2);
                }
#else

				{
                SK_U32          our2;
                        SkPciReadCfgDWord(pAC, PCI_OUR_REG_2, &our2);
                        our2 &= ~PCI_REV_DESC;
                        SkPciWriteCfgDWord(pAC, PCI_OUR_REG_2, our2);
                }


#endif

 
        }


 
        /*
         * If we're at this point we're going through skge_probe() for
         * the first time.  Return success (0) if we've initialized 1
         * or more boards. Otherwise, return failure (-ENODEV).
         */
 
        return boards_found;
} /* skge_probe */

#endif

#ifdef Y2_RECOVERY
/*****************************************************************************
 *
 * 	SkGeHandleKernelTimer - Handle the kernel timer requests
 *
 * Description:
 *	If the requested time interval for the timer has elapsed, 
 *	this function checks the link state.
 *
 * Returns:	N/A
 *
 */
static void SkGeHandleKernelTimer(
unsigned long ptr)  /* holds the pointer to adapter control context */
{
	DEV_NET         *pNet = (DEV_NET*) ptr;
	SkGeCheckTimer(pNet);	
}

/*****************************************************************************
 *
 * 	sk98lin_check_timer - Resume the the card
 *
 * Description:
 *	This function checks the kernel timer
 *
 * Returns: N/A
 *	
 */
void SkGeCheckTimer(
DEV_NET *pNet)  /* holds the pointer to adapter control context */
{
	SK_AC           *pAC = pNet->pAC;
	SK_BOOL		StartTimer = SK_TRUE;

	if (pNet->InRecover)
		return;
	if (pNet->TimerExpired)
		return;
	pNet->TimerExpired = SK_TRUE;

#define TXPORT pAC->TxPort[pNet->PortNr][TX_PRIO_LOW]
#define RXPORT pAC->RxPort[pNet->PortNr]

	if (	(CHIP_ID_YUKON_2(pAC)) &&
		(netif_running(pAC->dev[pNet->PortNr]))) {
		
#ifdef Y2_RX_CHECK
		if (HW_FEATURE(pAC, HWF_WA_DEV_4167)) {
		/* Checks the RX path */
			CheckRxPath(pNet);
		}
#endif

		/* Checkthe transmitter */
		if (!(IS_Q_EMPTY(&TXPORT.TxAQ_working))) {
			if (TXPORT.LastDone != TXPORT.TxALET.Done) {
				TXPORT.LastDone = TXPORT.TxALET.Done;
				pNet->TransmitTimeoutTimer = 0;
			} else {
				pNet->TransmitTimeoutTimer++;
				if (pNet->TransmitTimeoutTimer >= 10) {
					pNet->TransmitTimeoutTimer = 0;
#ifdef CHECK_TRANSMIT_TIMEOUT
					StartTimer =  SK_FALSE;
					SkLocalEventQueue(pAC, SKGE_DRV, 
						SK_DRV_RECOVER,pNet->PortNr,-1,SK_FALSE);
#endif
				}
			} 
		} 

#ifdef CHECK_TRANSMIT_TIMEOUT
//		if (!timer_pending(&pNet->KernelTimer)) {
#if 0 //u-boot
			pNet->KernelTimer.expires = jiffies + (HZ/4); /* 100ms */
			add_timer(&pNet->KernelTimer);
#endif
			pNet->TimerExpired = SK_FALSE;

//		}
#endif
	}
}


/*****************************************************************************
*
* CheckRXCounters - Checks the the statistics for RX path hang
*
* Description:
*	This function is called periodical by a timer. 
*
* Notes:
*
* Function Parameters:
*
* Returns:
*	Traffic status
*
*/
static SK_BOOL CheckRXCounters(
DEV_NET *pNet)  /* holds the pointer to adapter control context */
{
#if 0 // u-boot
	SK_AC           	*pAC = pNet->pAC;
#endif
	SK_BOOL bStatus 	= SK_FALSE;

	/* Variable used to store the MAC RX FIFO RP, RPLev*/
	SK_U32			MACFifoRP = 0;
	SK_U32			MACFifoRLev = 0;

	/* Variable used to store the PCI RX FIFO RP, RPLev*/
	SK_U32			RXFifoRP = 0;
	SK_U8			RXFifoRLev = 0;

	SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MSG,
		("==> CheckRXCounters()\n"));

	/*Check if statistic counters hangs*/
#if 0 // u-boot
	if (pNet->LastJiffies == pAC->dev[pNet->PortNr]->last_rx) {
		/* Now read the values of read pointer/level from MAC RX FIFO again */
		SK_IN32(pAC->IoBase, MR_ADDR(pNet->PortNr, RX_GMF_RP), &MACFifoRP);
		SK_IN32(pAC->IoBase, MR_ADDR(pNet->PortNr, RX_GMF_RLEV), &MACFifoRLev);

		/* Now read the values of read pointer/level from RX FIFO again */
		SK_IN8(pAC->IoBase, Q_ADDR(pAC->GIni.GP[pNet->PortNr].PRxQOff, Q_RX_RP), &RXFifoRP);
		SK_IN8(pAC->IoBase, Q_ADDR(pAC->GIni.GP[pNet->PortNr].PRxQOff, Q_RX_RL), &RXFifoRLev);

		/* Check if the MAC RX hang */
		if ((MACFifoRP == pNet->PreviousMACFifoRP) &&
			(pNet->PreviousMACFifoRP != 0) &&
			(MACFifoRLev >= pNet->PreviousMACFifoRLev)){
			bStatus = SK_TRUE;
		}

		/* Check if the PCI RX hang */
		if ((RXFifoRP == pNet->PreviousRXFifoRP) &&
			(pNet->PreviousRXFifoRP != 0) &&
			(RXFifoRLev >= pNet->PreviousRXFifoRLev)){
			/*Set the flag to indicate that the RX FIFO hangs*/
			bStatus = SK_TRUE;
		}
	}

	/* Store now the values of counters for next check */
	pNet->LastJiffies = pAC->dev[pNet->PortNr]->last_rx;
#endif


	/* Store the values of  read pointer/level from MAC RX FIFO for next test */
	pNet->PreviousMACFifoRP = MACFifoRP;
	pNet->PreviousMACFifoRLev = MACFifoRLev;

	/* Store the values of  read pointer/level from RX FIFO for next test */
	pNet->PreviousRXFifoRP = RXFifoRP;
	pNet->PreviousRXFifoRLev = RXFifoRLev;

	SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MSG,
		("<== CheckRXCounters()\n"));

	return bStatus;
}

/*****************************************************************************
*
* CheckRxPath - Checks if the RX path
*
* Description:
*	This function is called periodical by a timer. 
*
* Notes:
*
* Function Parameters:
*
* Returns:
*	None.
*
*/
static void  CheckRxPath(
DEV_NET *pNet)  /* holds the pointer to adapter control context */
{
	unsigned long		Flags;    /* for the spin locks    */
	/* Initialize the pAC structure.*/
	SK_AC           	*pAC = pNet->pAC;

	SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MSG,
		("==> CheckRxPath()\n"));

	/*If the statistics are not changed then could be an RX problem */
	if (CheckRXCounters(pNet)){
		/* 
		 * First we try the simple solution by resetting the Level Timer
		 */

		/* Stop Level Timer of Status BMU */
		SK_OUT8(pAC->IoBase, STAT_LEV_TIMER_CTRL, TIM_STOP);

		/* Start Level Timer of Status BMU */
		SK_OUT8(pAC->IoBase, STAT_LEV_TIMER_CTRL, TIM_START);

		if (!CheckRXCounters(pNet)) {
			return;
		}

		spin_lock_irqsave(&pAC->SlowPathLock, Flags);
		SkLocalEventQueue(pAC, SKGE_DRV,
			SK_DRV_RECOVER,pNet->PortNr,-1,SK_TRUE);

		/* Reset the fifo counters */
		pNet->PreviousMACFifoRP = 0;
		pNet->PreviousMACFifoRLev = 0;
		pNet->PreviousRXFifoRP = 0;
		pNet->PreviousRXFifoRLev = 0;

		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
	}

	SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MSG,
		("<== CheckRxPath()\n"));
}



#endif


#ifdef CONFIG_PM
/*****************************************************************************
 *
 * 	sk98lin_resume - Resume the the card
 *
 * Description:
 *	This function resumes the card into the D0 state
 *
 * Returns: N/A
 *	
 */
static int sk98lin_resume(
struct pci_dev *pdev)   /* the device that is to resume */
{
	struct net_device   *dev  = pci_get_drvdata(pdev);
	DEV_NET		    *pNet = (DEV_NET*) dev->priv;
	SK_AC		    *pAC  = pNet->pAC;
	SK_U16		     PmCtlSts;

	/* Set the power state to D0 */
	pci_set_power_state(pdev, 0);
	pci_restore_state(pdev, pAC->PciState);

	pci_enable_device(pdev);
	pci_set_master(pdev);

	pci_enable_wake(pdev, 3, 0);
	pci_enable_wake(pdev, 4, 0);

	SK_OUT8(pAC->IoBase, RX_GMF_CTRL_T, (SK_U8)GMF_RST_CLR);

	/* Set the adapter power state to D0 */
	SkPciReadCfgWord(pAC, PCI_PM_CTL_STS, &PmCtlSts);
	PmCtlSts &= ~(PCI_PM_STATE_D3);	/* reset all DState bits */
	PmCtlSts |= PCI_PM_STATE_D0;
	SkPciWriteCfgWord(pAC, PCI_PM_CTL_STS, PmCtlSts);

	/* Reinit the adapter and start the port again */
	pAC->BoardLevel = SK_INIT_DATA;
	SkDrvLeaveDiagMode(pAC);

	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_EC) ||
		(CHIP_ID_YUKON_2(pAC)) ) {
		pAC->StatusLETable.Done  = 0;
		pAC->StatusLETable.Put   = 0;
		pAC->StatusLETable.HwPut = 0;
		SkGeY2InitStatBmu(pAC, pAC->IoBase, &pAC->StatusLETable);
	}

	return 0;
}
 
/*****************************************************************************
 *
 * 	sk98lin_suspend - Suspend the card
 *
 * Description:
 *	This function suspends the card into a defined state
 *
 * Returns: N/A
 *	
 */
static int sk98lin_suspend(
struct pci_dev	*pdev,   /* pointer to the device that is to suspend */
u32		state)  /* what power state is desired by Linux?    */
{
	struct net_device   *dev  = pci_get_drvdata(pdev);
	DEV_NET		    *pNet = (DEV_NET*) dev->priv;
	SK_AC		    *pAC  = pNet->pAC;
	SK_U16		     PciPMControlStatus;
	SK_U16		     PciPMCapabilities;
	SK_MAC_ADDR	     MacAddr;
	int		     i;

	/* GEnesis and first yukon revs do not support power management */
	if (pAC->GIni.GIChipId == CHIP_ID_YUKON) {
		if (pAC->GIni.GIChipRev == 0) {
			return 0; /* power management not supported */
		}
	} 

	if (pAC->GIni.GIChipId == CHIP_ID_GENESIS) {
		return 0; /* not supported for this chipset */
	}

	if (pAC->WolInfo.ConfiguredWolOptions == 0) {
		return 0; /* WOL possible, but disabled via ethtool */
	}

	if(netif_running(dev)) {
		netif_stop_queue(dev); /* stop device if running */
	}
	
	/* read the PM control/status register from the PCI config space */
	SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_CTL_STS), &PciPMControlStatus);

	/* read the power management capabilities from the config space */
	SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_CAP_REG), &PciPMCapabilities);

	/* Enable WakeUp with Magic Packet - get MAC address from adapter */
	for (i = 0; i < SK_MAC_ADDR_LEN; i++) {
		/* virtual address: will be used for data */
		SK_IN8(pAC->IoBase, (B2_MAC_1 + i), &MacAddr.a[i]);
	}

	SkDrvEnterDiagMode(pAC);
	SkEnableWOMagicPacket(pAC, pAC->IoBase, MacAddr);

	pci_enable_wake(pdev, 3, 1);
	pci_enable_wake(pdev, 4, 1);	/* 4 == D3 cold */
	pci_save_state(pdev, pAC->PciState);
	pci_disable_device(pdev); // NEW
	pci_set_power_state(pdev, state); /* set the state */

	return 0;
}


/******************************************************************************
 *
 *	SkEnableWOMagicPacket - Enable Wake on Magic Packet on the adapter
 *
 * Context:
 *	init, pageable
 *	the adapter should be de-initialized before calling this function
 *
 * Returns:
 *	nothing
 */

static void SkEnableWOMagicPacket(
SK_AC         *pAC,      /* Adapter Control Context          */
SK_IOC         IoC,      /* I/O control context              */
SK_MAC_ADDR    MacAddr)  /* MacAddr expected in magic packet */
{
	SK_U16	Word;
	SK_U32	DWord;
	int 	i;
	int	HwPortIndex;
	int	Port = 0;

	/* use Port 0 as long as we do not have any dual port cards which support WOL */
	HwPortIndex = 0;
	DWord = 0;

	SK_OUT16(IoC, 0x0004, 0x0002);	/* clear S/W Reset */
	SK_OUT16(IoC, 0x0f10, 0x0002);	/* clear Link Reset */

	/*
	 * PHY Configuration:
	 * Autonegotioation is enalbed, advertise 10 HD, 10 FD,
	 * 100 HD, and 100 FD.
	 */
	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_EC) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON_LITE) || 
		(CHIP_ID_YUKON_2(pAC)) ) {

		SK_OUT8(IoC, 0x0007, 0xa9);			/* enable VAUX */

		/* WA code for COMA mode */
		/* Only for yukon plus based chipsets rev A3 */
		if (pAC->GIni.GIChipRev >= CHIP_REV_YU_LITE_A3) {
			SK_IN32(IoC, B2_GP_IO, &DWord);
			DWord |= GP_DIR_9;			/* set to output */
			DWord &= ~GP_IO_9;			/* clear PHY reset (active high) */
			SK_OUT32(IoC, B2_GP_IO, DWord);		/* clear PHY reset */
		}

		if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_LITE) ||
			(pAC->GIni.GIChipId == CHIP_ID_YUKON)) {
			SK_OUT32(IoC, 0x0f04, 0x01f04001);	/* set PHY reset */
			SK_OUT32(IoC, 0x0f04, 0x01f04002);	/* clear PHY reset */
		} else {
			SK_OUT8(IoC, 0x0f04, 0x02);		/* clear PHY reset */
		}

		SK_OUT8(IoC, 0x0f00, 0x02);			/* clear MAC reset */
		SkGmPhyWrite(pAC, IoC, Port, 4, 0x01e1);	/* advertise 10/100 HD/FD */
		SkGmPhyWrite(pAC, IoC, Port, 9, 0x0000);	/* do not advertise 1000 HD/FD */
		SkGmPhyWrite(pAC, IoC, Port, 00, 0xB300);	/* 100 MBit, disable Autoneg */
	} else if (pAC->GIni.GIChipId == CHIP_ID_YUKON_FE) {
		SK_OUT8(IoC, 0x0007, 0xa9);			/* enable VAUX */
		SK_OUT8(IoC, 0x0f04, 0x02);			/* clear PHY reset */
		SK_OUT8(IoC, 0x0f00, 0x02);			/* clear MAC reset */
		SkGmPhyWrite(pAC, IoC, Port, 16, 0x0130);	/* Enable Automatic Crossover */
		SkGmPhyWrite(pAC, IoC, Port, 00, 0xB300);	/* 100 MBit, disable Autoneg */
	}


	/*
	 * MAC Configuration:
	 * Set the MAC to 100 HD and enable the auto update features
	 * for Speed, Flow Control and Duplex Mode.
	 * If autonegotiation completes successfully the
	 * MAC takes the link parameters from the PHY.
	 * If the link partner doesn't support autonegotiation
	 * the MAC can receive magic packets if the link partner
	 * uses 100 HD.
	 */
	SK_OUT16(IoC, 0x2804, 0x3832);
   

	/*
	 * Set Up Magic Packet parameters
	 */
	for (i = 0; i < 6; i+=2) {		/* set up magic packet MAC address */
		SK_IN16(IoC, 0x100 + i, &Word);
		SK_OUT16(IoC, 0xf24 + i, Word);
	}

	SK_OUT16(IoC, 0x0f20, 0x0208);		/* enable PME on magic packet */
						/* and on wake up frame */

	/*
	 * Set up PME generation
	 */
	/* set PME legacy mode */
	/* Only for PCI express based chipsets */
	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_EC) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON_FE) || 
		(CHIP_ID_YUKON_2(pAC))) {
		SkPciReadCfgDWord(pAC, 0x40, &DWord);
		DWord |= 0x8000;
		SkPciWriteCfgDWord(pAC, 0x40, DWord);
	}

	SK_OUT8(IoC, RX_GMF_CTRL_T, (SK_U8)GMF_RST_SET);

	/* clear PME status and switch adapter to DState */
	SkPciReadCfgWord(pAC, 0x4c, &Word);
	Word |= 0x103;
	SkPciWriteCfgWord(pAC, 0x4c, Word);
}	/* SkEnableWOMagicPacket */
#endif


/*****************************************************************************
 *
 * 	FreeResources - release resources allocated for adapter
 *
 * Description:
 *	This function releases the IRQ, unmaps the IO and
 *	frees the desriptor ring.
 *
 * Returns: N/A
 *	
 */
static void FreeResources(struct SK_NET_DEVICE *dev)
{
SK_U32 AllocFlag;
DEV_NET		*pNet;
SK_AC		*pAC;

	if (dev->priv) {
		pNet = (DEV_NET*) dev->priv;
		pAC = pNet->pAC;
		AllocFlag = pAC->AllocFlag;
#if 0 /* uboot */
		if (pAC->PciDev) {
			pci_release_regions(pAC->PciDev);
		}

		if (AllocFlag & SK_ALLOC_IRQ) {
			free_irq(dev->irq, dev);
		}
		if (pAC->IoBase) {
			iounmap(pAC->IoBase);
		}
#endif

#if 0 /* Marvell- uboot */

		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2FreeResources(pAC);
		} else {
			BoardFreeMem(pAC);
		}
	
#else
	if (CHIP_ID_YUKON_2(pAC)) {
		SkY2FreeResources(pAC);
	} else {
		BoardFreeMem(pAC);
	}
#endif
	}
	
} /* FreeResources */

#if 0 /* uboot */
MODULE_AUTHOR("Mirko Lindner <mlindner@syskonnect.de>");
MODULE_DESCRIPTION("SysKonnect SK-NET Gigabit Ethernet SK-98xx driver");
MODULE_LICENSE("GPL");
#endif

#ifdef LINK_SPEED_A
static char *Speed_A[SK_MAX_CARD_PARAM] = LINK_SPEED;
#else
static char *Speed_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef LINK_SPEED_B
static char *Speed_B[SK_MAX_CARD_PARAM] = LINK_SPEED;
#else
static char *Speed_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef AUTO_NEG_A
static char *AutoNeg_A[SK_MAX_CARD_PARAM] = AUTO_NEG_A;
#else
static char *AutoNeg_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef DUP_CAP_A
static char *DupCap_A[SK_MAX_CARD_PARAM] = DUP_CAP_A;
#else
static char *DupCap_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef FLOW_CTRL_A
static char *FlowCtrl_A[SK_MAX_CARD_PARAM] = FLOW_CTRL_A;
#else
static char *FlowCtrl_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef ROLE_A
static char *Role_A[SK_MAX_CARD_PARAM] = ROLE_A;
#else
static char *Role_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef AUTO_NEG_B
static char *AutoNeg_B[SK_MAX_CARD_PARAM] = AUTO_NEG_B;
#else
static char *AutoNeg_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef DUP_CAP_B
static char *DupCap_B[SK_MAX_CARD_PARAM] = DUP_CAP_B;
#else
static char *DupCap_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef FLOW_CTRL_B
static char *FlowCtrl_B[SK_MAX_CARD_PARAM] = FLOW_CTRL_B;
#else
static char *FlowCtrl_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef ROLE_B
static char *Role_B[SK_MAX_CARD_PARAM] = ROLE_B;
#else
static char *Role_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef CON_TYPE
static char *ConType[SK_MAX_CARD_PARAM] = CON_TYPE;
#else
static char *ConType[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef PREF_PORT
static char *PrefPort[SK_MAX_CARD_PARAM] = PREF_PORT;
#else
static char *PrefPort[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef RLMT_MODE
static char *RlmtMode[SK_MAX_CARD_PARAM] = RLMT_MODE;
#else
static char *RlmtMode[SK_MAX_CARD_PARAM] = {"", };
#endif

static int   IntsPerSec[SK_MAX_CARD_PARAM];
static char *Moderation[SK_MAX_CARD_PARAM];
static char *ModerationMask[SK_MAX_CARD_PARAM];

static char *LowLatency[SK_MAX_CARD_PARAM];

#if 0
/*****************************************************************************
 *
 * 	sk98lin_remove_device - device deinit function
 *
 * Description:
 *	Disable adapter if it is still running, free resources,
 *	free device struct.
 *
 * Returns: N/A
 */

static void sk98lin_remove_device(struct pci_dev *pdev)
{
DEV_NET		*pNet;
SK_AC		*pAC;
struct SK_NET_DEVICE *next;
unsigned long Flags;
struct net_device *dev = pci_get_drvdata(pdev);


	/* Device not available. Return. */
	if (!dev)
		return;

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	next = pAC->Next;

	netif_stop_queue(dev);
	SkGeYellowLED(pAC, pAC->IoBase, 0);

	if(pAC->BoardLevel == SK_INIT_RUN) {
		/* board is still alive */
		spin_lock_irqsave(&pAC->SlowPathLock, Flags);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
					0, -1, SK_FALSE);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
					1, -1, SK_TRUE);

		/* disable interrupts */
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);
		SkGeDeInit(pAC, pAC->IoBase);
		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
		pAC->BoardLevel = SK_INIT_DATA;
		/* We do NOT check here, if IRQ was pending, of course*/
	}

	if(pAC->BoardLevel == SK_INIT_IO) {
		/* board is still alive */
		SkGeDeInit(pAC, pAC->IoBase);
		pAC->BoardLevel = SK_INIT_DATA;
	}

	if ((pAC->GIni.GIMacsFound == 2) && pAC->RlmtNets == 2){
		unregister_netdev(pAC->dev[1]);
		free_netdev(pAC->dev[1]);
	}

	FreeResources(dev);

#ifdef CONFIG_PROC_FS
	/* Remove the sk98lin procfs device entries */
	if ((pAC->GIni.GIMacsFound == 2) && pAC->RlmtNets == 2){
		remove_proc_entry(pAC->dev[1]->name, pSkRootDir);
	}
	remove_proc_entry(pNet->InitialDevName, pSkRootDir);
#endif

	dev->get_stats = NULL;
	/*
	 * otherwise unregister_netdev calls get_stats with
	 * invalid IO ...  :-(
	 */
	unregister_netdev(dev);
	free_netdev(dev);
	kfree(pAC);
	sk98lin_max_boards_found--;

#ifdef CONFIG_PROC_FS
	/* Remove all Proc entries if last device */
	if (sk98lin_max_boards_found == 0) {
		/* clear proc-dir */
		remove_proc_entry(pSkRootDir->name, proc_net);
	}
#endif

}
#endif

/*****************************************************************************
 *
 * 	SkGeBoardInit - do level 0 and 1 initialization
 *
 * Description:
 *	This function prepares the board hardware for running. The desriptor
 *	ring is set up, the IRQ is allocated and the configuration settings
 *	are examined.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static int __init SkGeBoardInit(struct SK_NET_DEVICE *dev, SK_AC *pAC)
{
short	i;
unsigned long Flags;
char	*DescrString = "sk98lin: Driver for Linux"; /* this is given to PNMI */
char	*VerStr	= VER_STRING;
#if 0 /* uboot */
int	Ret;			/* return code of request_irq */
#endif
SK_BOOL	DualNet;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("IoBase: %08lX\n", (unsigned long)pAC->IoBase));
	for (i=0; i<SK_MAX_MACS; i++) {
		pAC->TxPort[i][0].HwAddr = pAC->IoBase + TxQueueAddr[i][0];
		pAC->TxPort[i][0].PortIndex = i;
		pAC->RxPort[i].HwAddr = pAC->IoBase + RxQueueAddr[i];
		pAC->RxPort[i].PortIndex = i;
	}

	/* Initialize the mutexes */
	for (i=0; i<SK_MAX_MACS; i++) {
		spin_lock_init(&pAC->TxPort[i][0].TxDesRingLock);
		spin_lock_init(&pAC->RxPort[i].RxDesRingLock);
	}

	spin_lock_init(&pAC->InitLock);		/* Init lock */
	spin_lock_init(&pAC->SlowPathLock);
	spin_lock_init(&pAC->TxQueueLock);	/* for Yukon2 chipsets */
	spin_lock_init(&pAC->SetPutIndexLock);	/* for Yukon2 chipsets */

	/* level 0 init common modules here */
	
	spin_lock_irqsave(&pAC->SlowPathLock, Flags);
	/* Does a RESET on board ...*/
	if (SkGeInit(pAC, pAC->IoBase, SK_INIT_DATA) != 0) {
		printk("HWInit (0) failed.\n");
		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
		return(-EAGAIN);
	}
	SkI2cInit(  pAC, pAC->IoBase, SK_INIT_DATA);
	SkEventInit(pAC, pAC->IoBase, SK_INIT_DATA);
#ifdef SK_PNMI_SUPPORT
	SkPnmiInit( pAC, pAC->IoBase, SK_INIT_DATA);
#endif
	SkAddrInit( pAC, pAC->IoBase, SK_INIT_DATA);
	SkRlmtInit( pAC, pAC->IoBase, SK_INIT_DATA);
	SkTimerInit(pAC, pAC->IoBase, SK_INIT_DATA);

	pAC->BoardLevel = SK_INIT_DATA;
	pAC->RxPort[0].RxBufSize = ETH_BUF_SIZE;
	pAC->RxPort[1].RxBufSize = ETH_BUF_SIZE;

	SK_PNMI_SET_DRIVER_DESCR(pAC, DescrString);
	SK_PNMI_SET_DRIVER_VER(pAC, VerStr);

	/* level 1 init common modules here (HW init) */
	if (SkGeInit(pAC, pAC->IoBase, SK_INIT_IO) != 0) {
		printk("sk98lin: HWInit (1) failed.\n");
		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
		return(-EAGAIN);
	}
	SkI2cInit(  pAC, pAC->IoBase, SK_INIT_IO);
	SkEventInit(pAC, pAC->IoBase, SK_INIT_IO);
#ifdef SK_PNMI_SUPPORT
	SkPnmiInit( pAC, pAC->IoBase, SK_INIT_IO);
#endif
	SkAddrInit( pAC, pAC->IoBase, SK_INIT_IO);
	SkRlmtInit( pAC, pAC->IoBase, SK_INIT_IO);
	SkTimerInit(pAC, pAC->IoBase, SK_INIT_IO);
#ifdef Y2_RECOVERY
	/* mark entries invalid */
	pAC->LastPort = 3;
	pAC->LastOpc = 0xFF;
#endif

	/* Set chipset type support */
	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON_LITE) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON_LP)) {
		pAC->ChipsetType = 1;	/* Yukon chipset (descriptor logic) */
	} else if (CHIP_ID_YUKON_2(pAC)) {
		pAC->ChipsetType = 2;	/* Yukon2 chipset (list logic) */
	} else {
		pAC->ChipsetType = 0;	/* Genesis chipset (descriptor logic) */
	}

	/* wake on lan support */
	pAC->WolInfo.SupportedWolOptions = 0;
#if defined (ETHTOOL_GWOL) && defined (ETHTOOL_SWOL)
	if (pAC->GIni.GIChipId != CHIP_ID_GENESIS) {
		pAC->WolInfo.SupportedWolOptions  = WAKE_MAGIC;
		if (pAC->GIni.GIChipId == CHIP_ID_YUKON) {
			if (pAC->GIni.GIChipRev == 0) {
				pAC->WolInfo.SupportedWolOptions = 0;
			}
		} 
	}
#endif
	pAC->WolInfo.ConfiguredWolOptions = pAC->WolInfo.SupportedWolOptions;

	GetConfiguration(pAC);
	if (pAC->RlmtNets == 2) {
		pAC->GIni.GP[0].PPortUsage = SK_MUL_LINK;
		pAC->GIni.GP[1].PPortUsage = SK_MUL_LINK;
	}

	pAC->BoardLevel = SK_INIT_IO;
	spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);

	if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
		dev->poll =  &SkGePoll;
		dev->weight = 64;
#endif
#if 0 /* uboot */
		if (pAC->GIni.GIMacsFound == 2) {
			Ret = request_irq(dev->irq, SkGeIsr, SA_SHIRQ, dev->name, dev);
		} else if (pAC->GIni.GIMacsFound == 1) {
			Ret = request_irq(dev->irq, SkGeIsrOnePort, SA_SHIRQ, dev->name, dev);
		} else {
			printk(KERN_WARNING "sk98lin: Illegal number of ports: %d\n",
				pAC->GIni.GIMacsFound);
			return -EAGAIN;
		}
	}
	else {
		Ret = request_irq(dev->irq, SkY2Isr, SA_SHIRQ, dev->name, dev);
	}

	if (Ret) {
		printk(KERN_WARNING "sk98lin: Requested IRQ %d is busy.\n",
			dev->irq);
		return -EAGAIN;
	}
#endif
	}


	pAC->AllocFlag |= SK_ALLOC_IRQ;

	/* 
	** Alloc descriptor/LETable memory for this board (both RxD/TxD)
	*/
	if (CHIP_ID_YUKON_2(pAC)) {
		if (!SkY2AllocateResources(pAC)) {
			printk("No memory for Yukon2 settings\n");
			return(-EAGAIN);
		}
	} else {
		if(!BoardAllocMem(pAC)) {
			printk("No memory for descriptor rings.\n");
			return(-EAGAIN);
		}
	}

#ifdef SK_USE_CSUM
	SkCsSetReceiveFlags(pAC,
		SKCS_PROTO_IP | SKCS_PROTO_TCP | SKCS_PROTO_UDP,
		&pAC->CsOfs1, &pAC->CsOfs2, 0);
	pAC->CsOfs = (pAC->CsOfs2 << 16) | pAC->CsOfs1;
#endif

	/*
	** Function BoardInitMem() for Yukon dependent settings...
	*/
	BoardInitMem(pAC);
	/* tschilling: New common function with minimum size check. */
	DualNet = SK_FALSE;
	if (pAC->RlmtNets == 2) {
		DualNet = SK_TRUE;
	}
	
	if (SkGeInitAssignRamToQueues(
		pAC,
		pAC->ActivePort,
		DualNet)) {
#if 0 /* Marvell - uboot */
		BoardFreeMem(pAC);
#else
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2FreeResources(pAC);
		} else {
			BoardFreeMem(pAC);
		}
#endif



		printk("sk98lin: SkGeInitAssignRamToQueues failed.\n");
		return(-EAGAIN);
	}

	/*
	 * Register the device here
	 */
	pAC->Next = SkGeRootDev;
	SkGeRootDev = dev;

	return (0);
} /* SkGeBoardInit */


/*****************************************************************************
 *
 * 	BoardAllocMem - allocate the memory for the descriptor rings
 *
 * Description:
 *	This function allocates the memory for all descriptor rings.
 *	Each ring is aligned for the desriptor alignment and no ring
 *	has a 4 GByte boundary in it (because the upper 32 bit must
 *	be constant for all descriptiors in one rings).
 *
 * Returns:
 *	SK_TRUE, if all memory could be allocated
 *	SK_FALSE, if not
 */
static SK_BOOL BoardAllocMem(
SK_AC	*pAC)
{
caddr_t		pDescrMem;	/* pointer to descriptor memory area */
size_t		AllocLength;	/* length of complete descriptor area */
int		i;		/* loop counter */
unsigned long	BusAddr;

	
	/* rings plus one for alignment (do not cross 4 GB boundary) */
	/* RX_RING_SIZE is assumed bigger than TX_RING_SIZE */
#if (BITS_PER_LONG == 32)
	AllocLength = (RX_RING_SIZE + TX_RING_SIZE) * pAC->GIni.GIMacsFound + 8;
#else
	AllocLength = (RX_RING_SIZE + TX_RING_SIZE) * pAC->GIni.GIMacsFound
		+ RX_RING_SIZE + 8;
#endif

	pDescrMem = pci_alloc_consistent(pAC->PciDev, AllocLength,
					 &pAC->pDescrMemDMA);

	if (pDescrMem == NULL) {
		return (SK_FALSE);
	}
	pAC->pDescrMem = pDescrMem;
	BusAddr = (unsigned long) pAC->pDescrMemDMA;

	/* Descriptors need 8 byte alignment, and this is ensured
	 * by pci_alloc_consistent.
	 */
	for (i=0; i<pAC->GIni.GIMacsFound; i++) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_TX_PROGRESS,
			("TX%d/A: pDescrMem: %lX,   PhysDescrMem: %lX\n",
			i, (unsigned long) pDescrMem,
			BusAddr));
		pAC->TxPort[i][0].pTxDescrRing = pDescrMem;
		pAC->TxPort[i][0].VTxDescrRing = BusAddr;
		pDescrMem += TX_RING_SIZE;
		BusAddr += TX_RING_SIZE;
	
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_TX_PROGRESS,
			("RX%d: pDescrMem: %lX,   PhysDescrMem: %lX\n",
			i, (unsigned long) pDescrMem,
			(unsigned long)BusAddr));
		pAC->RxPort[i].pRxDescrRing = pDescrMem;
		pAC->RxPort[i].VRxDescrRing = BusAddr;
		pDescrMem += RX_RING_SIZE;
		BusAddr += RX_RING_SIZE;
	} /* for */
	
	return (SK_TRUE);
} /* BoardAllocMem */


/****************************************************************************
 *
 *	BoardFreeMem - reverse of BoardAllocMem
 *
 * Description:
 *	Free all memory allocated in BoardAllocMem: adapter context,
 *	descriptor rings, locks.
 *
 * Returns:	N/A
 */
static void BoardFreeMem(
SK_AC		*pAC)
{
size_t		AllocLength;	/* length of complete descriptor area */

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("BoardFreeMem\n"));

	if (pAC->pDescrMem) {

#if (BITS_PER_LONG == 32)
		AllocLength = (RX_RING_SIZE + TX_RING_SIZE) * pAC->GIni.GIMacsFound + 8;
#else
		AllocLength = (RX_RING_SIZE + TX_RING_SIZE) * pAC->GIni.GIMacsFound
			+ RX_RING_SIZE + 8;
#endif

		pci_free_consistent(pAC->PciDev, AllocLength,
			    pAC->pDescrMem, pAC->pDescrMemDMA);
		pAC->pDescrMem = NULL;
	}
} /* BoardFreeMem */


/*****************************************************************************
 *
 * 	BoardInitMem - initiate the descriptor rings
 *
 * Description:
 *	This function sets the descriptor rings or LETables up in memory.
 *	The adapter is initialized with the descriptor start addresses.
 *
 * Returns:	N/A
 */
static void BoardInitMem(
SK_AC	*pAC)	/* pointer to adapter context */
{
int	i;		/* loop counter */
int	RxDescrSize;	/* the size of a rx descriptor rounded up to alignment*/
int	TxDescrSize;	/* the size of a tx descriptor rounded up to alignment*/

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("BoardInitMem\n"));

	if (!pAC->GIni.GIYukon2) {
		RxDescrSize = (((sizeof(RXD) - 1) / DESCR_ALIGN) + 1) * DESCR_ALIGN;
		pAC->RxDescrPerRing = RX_RING_SIZE / RxDescrSize;
		TxDescrSize = (((sizeof(TXD) - 1) / DESCR_ALIGN) + 1) * DESCR_ALIGN;
		pAC->TxDescrPerRing = TX_RING_SIZE / RxDescrSize;
	
		for (i=0; i<pAC->GIni.GIMacsFound; i++) {
			SetupRing(
				pAC,
				pAC->TxPort[i][0].pTxDescrRing,
				pAC->TxPort[i][0].VTxDescrRing,
				(RXD**)&pAC->TxPort[i][0].pTxdRingHead,
				(RXD**)&pAC->TxPort[i][0].pTxdRingTail,
				(RXD**)&pAC->TxPort[i][0].pTxdRingPrev,
				&pAC->TxPort[i][0].TxdRingFree,
				&pAC->TxPort[i][0].TxdRingPrevFree,
				SK_TRUE);
			SetupRing(
				pAC,
				pAC->RxPort[i].pRxDescrRing,
				pAC->RxPort[i].VRxDescrRing,
				&pAC->RxPort[i].pRxdRingHead,
				&pAC->RxPort[i].pRxdRingTail,
				&pAC->RxPort[i].pRxdRingPrev,
				&pAC->RxPort[i].RxdRingFree,
				&pAC->RxPort[i].RxdRingFree,
				SK_FALSE);
		}
	}
} /* BoardInitMem */

/*****************************************************************************
 *
 * 	SetupRing - create one descriptor ring
 *
 * Description:
 *	This function creates one descriptor ring in the given memory area.
 *	The head, tail and number of free descriptors in the ring are set.
 *
 * Returns:
 *	none
 */
static void SetupRing(
SK_AC		*pAC,
void		*pMemArea,	/* a pointer to the memory area for the ring */
uintptr_t	VMemArea,	/* the virtual bus address of the memory area */
RXD		**ppRingHead,	/* address where the head should be written */
RXD		**ppRingTail,	/* address where the tail should be written */
RXD		**ppRingPrev,	/* address where the tail should be written */
int		*pRingFree,	/* address where the # of free descr. goes */
int		*pRingPrevFree,	/* address where the # of free descr. goes */
SK_BOOL		IsTx)		/* flag: is this a tx ring */
{
int	i;		/* loop counter */
int	DescrSize;	/* the size of a descriptor rounded up to alignment*/
int	DescrNum;	/* number of descriptors per ring */
RXD	*pDescr;	/* pointer to a descriptor (receive or transmit) */
RXD	*pNextDescr;	/* pointer to the next descriptor */
RXD	*pPrevDescr;	/* pointer to the previous descriptor */
uintptr_t VNextDescr;	/* the virtual bus address of the next descriptor */

	if (IsTx == SK_TRUE) {
		DescrSize = (((sizeof(TXD) - 1) / DESCR_ALIGN) + 1) *
			DESCR_ALIGN;
		DescrNum = TX_RING_SIZE / DescrSize;
	} else {
		DescrSize = (((sizeof(RXD) - 1) / DESCR_ALIGN) + 1) *
			DESCR_ALIGN;
		DescrNum = RX_RING_SIZE / DescrSize;
	}
	
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_TX_PROGRESS,
		("Descriptor size: %d   Descriptor Number: %d\n",
		DescrSize,DescrNum));
	
	pDescr = (RXD*) pMemArea;
	pPrevDescr = NULL;
	pNextDescr = (RXD*) (((char*)pDescr) + DescrSize);
	VNextDescr = VMemArea + DescrSize;
	for(i=0; i<DescrNum; i++) {
		/* set the pointers right */
		pDescr->VNextRxd = VNextDescr & 0xffffffffULL;
		pDescr->pNextRxd = pNextDescr;
		pDescr->TcpSumStarts = pAC->CsOfs;

		/* advance one step */
		pPrevDescr = pDescr;
		pDescr = pNextDescr;
		pNextDescr = (RXD*) (((char*)pDescr) + DescrSize);
		VNextDescr += DescrSize;
	}
	pPrevDescr->pNextRxd = (RXD*) pMemArea;
	pPrevDescr->VNextRxd = VMemArea;
	pDescr               = (RXD*) pMemArea;
	*ppRingHead          = (RXD*) pMemArea;
	*ppRingTail          = *ppRingHead;
	*ppRingPrev          = pPrevDescr;
	*pRingFree           = DescrNum;
	*pRingPrevFree       = DescrNum;
} /* SetupRing */


/*****************************************************************************
 *
 * 	PortReInitBmu - re-initiate the descriptor rings for one port
 *
 * Description:
 *	This function reinitializes the descriptor rings of one port
 *	in memory. The port must be stopped before.
 *	The HW is initialized with the descriptor start addresses.
 *
 * Returns:
 *	none
 */
static void PortReInitBmu(
SK_AC	*pAC,		/* pointer to adapter context */
int	PortIndex)	/* index of the port for which to re-init */
{
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("PortReInitBmu "));

	/* set address of first descriptor of ring in BMU */
	SK_OUT32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_L,
		(uint32_t)(((caddr_t)
		(pAC->TxPort[PortIndex][TX_PRIO_LOW].pTxdRingHead) -
		pAC->TxPort[PortIndex][TX_PRIO_LOW].pTxDescrRing +
		pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing) &
		0xFFFFFFFF));
	SK_OUT32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_H,
		(uint32_t)(((caddr_t)
		(pAC->TxPort[PortIndex][TX_PRIO_LOW].pTxdRingHead) -
		pAC->TxPort[PortIndex][TX_PRIO_LOW].pTxDescrRing +
		pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing) >> 32));
	SK_OUT32(pAC->IoBase, RxQueueAddr[PortIndex]+Q_DA_L,
		(uint32_t)(((caddr_t)(pAC->RxPort[PortIndex].pRxdRingHead) -
		pAC->RxPort[PortIndex].pRxDescrRing +
		pAC->RxPort[PortIndex].VRxDescrRing) & 0xFFFFFFFF));
	SK_OUT32(pAC->IoBase, RxQueueAddr[PortIndex]+Q_DA_H,
		(uint32_t)(((caddr_t)(pAC->RxPort[PortIndex].pRxdRingHead) -
		pAC->RxPort[PortIndex].pRxDescrRing +
		pAC->RxPort[PortIndex].VRxDescrRing) >> 32));
} /* PortReInitBmu */


/****************************************************************************
 *
 *	SkGeIsr - handle adapter interrupts
 *
 * Description:
 *	The interrupt routine is called when the network adapter
 *	generates an interrupt. It may also be called if another device
 *	shares this interrupt vector with the driver.
 *
 * Returns: N/A
 *
 */
#if 0 /* uboot */
static SkIsrRetVar SkGeIsr(int irq, void *dev_id, struct pt_regs *ptregs)
#else
void SkGeIsr(int irq, void *dev_id, struct pt_regs *ptregs)
#endif
{
struct SK_NET_DEVICE *dev = (struct SK_NET_DEVICE *)dev_id;
DEV_NET		*pNet;
SK_AC		*pAC;
SK_U32		IntSrc;		/* interrupts source register contents */	

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	
	/*
	 * Check and process if its our interrupt
	 */
	SK_IN32(pAC->IoBase, B0_SP_ISRC, &IntSrc);
	if ((IntSrc == 0) && (!pNet->NetConsoleMode)) {
		return;
	}

#ifdef CONFIG_SK98LIN_NAPI
	if (netif_rx_schedule_prep(dev)) {
		pAC->GIni.GIValIrqMask &= ~(NAPI_DRV_IRQS);
		SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
		__netif_rx_schedule(dev);
	}

#ifdef USE_TX_COMPLETE /* only if tx complete interrupt used */
	if (IntSrc & IS_XA1_F) {
		CLEAR_TX_IRQ(0, TX_PRIO_LOW);
	}
	if (IntSrc & IS_XA2_F) {
		CLEAR_TX_IRQ(1, TX_PRIO_LOW);
	}
#endif


#else
	while (((IntSrc & IRQ_MASK) & ~SPECIAL_IRQS) != 0) {
#if 0 /* software irq currently not used */
		if (IntSrc & IS_IRQ_SW) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("Software IRQ\n"));
		}
#endif
		if (IntSrc & IS_R1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF RX1 IRQ\n"));
			ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
			CLEAR_AND_START_RX(0);
			SK_PNMI_CNT_RX_INTR(pAC, 0);
		}
		if (IntSrc & IS_R2_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF RX2 IRQ\n"));
			ReceiveIrq(pAC, &pAC->RxPort[1], SK_TRUE);
			CLEAR_AND_START_RX(1);
			SK_PNMI_CNT_RX_INTR(pAC, 1);
		}
#ifdef USE_TX_COMPLETE /* only if tx complete interrupt used */
		if (IntSrc & IS_XA1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF AS TX1 IRQ\n"));
			CLEAR_TX_IRQ(0, TX_PRIO_LOW);
			SK_PNMI_CNT_TX_INTR(pAC, 0);
			spin_lock(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
			FreeTxDescriptors(pAC, &pAC->TxPort[0][TX_PRIO_LOW]);
			spin_unlock(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
		}
		if (IntSrc & IS_XA2_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF AS TX2 IRQ\n"));
			CLEAR_TX_IRQ(1, TX_PRIO_LOW);
			SK_PNMI_CNT_TX_INTR(pAC, 1);
			spin_lock(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
			FreeTxDescriptors(pAC, &pAC->TxPort[1][TX_PRIO_LOW]);
			spin_unlock(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
		}
#if 0 /* only if sync. queues used */
		if (IntSrc & IS_XS1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF SY TX1 IRQ\n"));
			CLEAR_TX_IRQ(0, TX_PRIO_HIGH);
			SK_PNMI_CNT_TX_INTR(pAC, 1);
			spin_lock(&pAC->TxPort[0][TX_PRIO_HIGH].TxDesRingLock);
			FreeTxDescriptors(pAC, 0, TX_PRIO_HIGH);
			spin_unlock(&pAC->TxPort[0][TX_PRIO_HIGH].TxDesRingLock);
		}
		if (IntSrc & IS_XS2_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF SY TX2 IRQ\n"));
			CLEAR_TX_IRQ(1, TX_PRIO_HIGH);
			SK_PNMI_CNT_TX_INTR(pAC, 1);
			spin_lock(&pAC->TxPort[1][TX_PRIO_HIGH].TxDesRingLock);
			FreeTxDescriptors(pAC, 1, TX_PRIO_HIGH);
			spin_unlock(&pAC->TxPort[1][TX_PRIO_HIGH].TxDesRingLock);
		}
#endif
#endif

		SK_IN32(pAC->IoBase, B0_ISRC, &IntSrc);
	} /* while (IntSrc & IRQ_MASK != 0) */
#endif

	IntSrc &= pAC->GIni.GIValIrqMask;
	if ((IntSrc & SPECIAL_IRQS) || pAC->CheckQueue) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_INT_SRC,
			("SPECIAL IRQ DP-Cards => %x\n", IntSrc));
		pAC->CheckQueue = SK_FALSE;
		spin_lock(&pAC->SlowPathLock);
		if (IntSrc & SPECIAL_IRQS)
			SkGeSirqIsr(pAC, pAC->IoBase, IntSrc);

		SkEventDispatcher(pAC, pAC->IoBase);
		spin_unlock(&pAC->SlowPathLock);
	}

#ifndef CONFIG_SK98LIN_NAPI
	/* Handle interrupts */
	ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
	ReceiveIrq(pAC, &pAC->RxPort[1], SK_TRUE);
#endif

	if (pAC->CheckQueue) {
		pAC->CheckQueue = SK_FALSE;
		spin_lock(&pAC->SlowPathLock);
		SkEventDispatcher(pAC, pAC->IoBase);
		spin_unlock(&pAC->SlowPathLock);
	}

	/* IRQ is processed - Enable IRQs again*/
	SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);

		return;
} /* SkGeIsr */


/****************************************************************************
 *
 *	SkGeIsrOnePort - handle adapter interrupts for single port adapter
 *
 * Description:
 *	The interrupt routine is called when the network adapter
 *	generates an interrupt. It may also be called if another device
 *	shares this interrupt vector with the driver.
 *	This is the same as above, but handles only one port.
 *
 * Returns: N/A
 *
 */
#if 0 /* uboot */
static SkIsrRetVar SkGeIsrOnePort(int irq, void *dev_id, struct pt_regs *ptregs)
#else
SkIsrRetVar SkGeIsrOnePort(int irq, void *dev_id, struct pt_regs *ptregs)
#endif
{
struct SK_NET_DEVICE *dev = (struct SK_NET_DEVICE *)dev_id;
DEV_NET		*pNet;
SK_AC		*pAC;
SK_U32		IntSrc;		/* interrupts source register contents */	

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	
	/*
	 * Check and process if its our interrupt
	 */
	SK_IN32(pAC->IoBase, B0_SP_ISRC, &IntSrc);
	if ((IntSrc == 0) && (!pNet->NetConsoleMode)) {
		return;
	}
	
#ifdef CONFIG_SK98LIN_NAPI
	if (netif_rx_schedule_prep(dev)) {
		CLEAR_AND_START_RX(0);
		CLEAR_TX_IRQ(0, TX_PRIO_LOW);
		pAC->GIni.GIValIrqMask &= ~(NAPI_DRV_IRQS);
		SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
		__netif_rx_schedule(dev);
	} 

#ifdef USE_TX_COMPLETE /* only if tx complete interrupt used */
	if (IntSrc & IS_XA1_F) {
		CLEAR_TX_IRQ(0, TX_PRIO_LOW);
	}
#endif
#else
	while (((IntSrc & IRQ_MASK) & ~SPECIAL_IRQS) != 0) {
#if 0 /* software irq currently not used */
		if (IntSrc & IS_IRQ_SW) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("Software IRQ\n"));
		}
#endif
		if (IntSrc & IS_R1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF RX1 IRQ\n"));
			ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
			CLEAR_AND_START_RX(0);
			SK_PNMI_CNT_RX_INTR(pAC, 0);
		}
#ifdef USE_TX_COMPLETE /* only if tx complete interrupt used */
		if (IntSrc & IS_XA1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF AS TX1 IRQ\n"));
			CLEAR_TX_IRQ(0, TX_PRIO_LOW);
			SK_PNMI_CNT_TX_INTR(pAC, 0);
			spin_lock(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
			FreeTxDescriptors(pAC, &pAC->TxPort[0][TX_PRIO_LOW]);
			spin_unlock(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
		}
#if 0 /* only if sync. queues used */
		if (IntSrc & IS_XS1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF SY TX1 IRQ\n"));
			CLEAR_TX_IRQ(0, TX_PRIO_HIGH);
			SK_PNMI_CNT_TX_INTR(pAC, 0);
			spin_lock(&pAC->TxPort[0][TX_PRIO_HIGH].TxDesRingLock);
			FreeTxDescriptors(pAC, 0, TX_PRIO_HIGH);
			spin_unlock(&pAC->TxPort[0][TX_PRIO_HIGH].TxDesRingLock);
		}
#endif
#endif

		SK_IN32(pAC->IoBase, B0_ISRC, &IntSrc);
	} /* while (IntSrc & IRQ_MASK != 0) */
#endif
	
	IntSrc &= pAC->GIni.GIValIrqMask;
	if ((IntSrc & SPECIAL_IRQS) || pAC->CheckQueue) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_INT_SRC,
			("SPECIAL IRQ SP-Cards => %x\n", IntSrc));
		pAC->CheckQueue = SK_FALSE;
		spin_lock(&pAC->SlowPathLock);
		if (IntSrc & SPECIAL_IRQS)

			SkGeSirqIsr(pAC, pAC->IoBase, IntSrc);

		SkEventDispatcher(pAC, pAC->IoBase);
		spin_unlock(&pAC->SlowPathLock);
	}

#ifndef CONFIG_SK98LIN_NAPI
	ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
#endif

	/* IRQ is processed - Enable IRQs again*/
	SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);

		return;
} /* SkGeIsrOnePort */

/****************************************************************************
 *
 *	SkGeOpen - handle start of initialized adapter
 *
 * Description:
 *	This function starts the initialized adapter.
 *	The board level variable is set and the adapter is
 *	brought to full functionality.
 *	The device flags are set for operation.
 *	Do all necessary level 2 initialization, enable interrupts and
 *	give start command to RLMT.
 *
 * Returns:
 *	0 on success
 *	!= 0 on error
 */
#if 0 /* uboot */
static int SkGeOpen(
#else
int SkGeOpen(
#endif
struct SK_NET_DEVICE *dev)  /* the device that is to be opened */
{
	DEV_NET        *pNet = (DEV_NET*) dev->priv;
	SK_AC          *pAC  = pNet->pAC;
	unsigned long   Flags;    	/* for the spin locks	*/
	unsigned long   InitFlags;	/* for the spin locks	*/
	int             CurrMac;	/* loop ctr for ports	*/

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeOpen: pAC=0x%lX:\n", (unsigned long)pAC));
	spin_lock_irqsave(&pAC->InitLock, InitFlags);
#if 0 /* uboot */

	if (pAC->DiagModeActive == DIAG_ACTIVE) {
		if (pAC->Pnmi.DiagAttached == SK_DIAG_RUNNING) {
			return (-1);   /* still in use by diag; deny actions */
		} 
	}

	if (!try_module_get(THIS_MODULE)) {
		return (-1);	/* increase of usage count not possible */
	}

	/* Set blink mode */
	if ((pAC->PciDev->vendor == 0x1186) || (pAC->PciDev->vendor == 0x11ab ))
		pAC->GIni.GILedBlinkCtrl = OEM_CONFIG_VALUE;
#endif



	if (pAC->BoardLevel == SK_INIT_DATA) {
		/* level 1 init common modules here */
		if (SkGeInit(pAC, pAC->IoBase, SK_INIT_IO) != 0) {
			printk("%s: HWInit (1) failed.\n", pAC->dev[pNet->PortNr]->name);
			return (-1);
		}
		SkI2cInit	(pAC, pAC->IoBase, SK_INIT_IO);
		SkEventInit	(pAC, pAC->IoBase, SK_INIT_IO);

#ifdef SK_PNMI_SUPPORT
		SkPnmiInit	(pAC, pAC->IoBase, SK_INIT_IO);
#endif

		SkAddrInit	(pAC, pAC->IoBase, SK_INIT_IO);
		SkRlmtInit	(pAC, pAC->IoBase, SK_INIT_IO);
		SkTimerInit	(pAC, pAC->IoBase, SK_INIT_IO);
		pAC->BoardLevel = SK_INIT_IO;
#ifdef Y2_RECOVERY
		/* mark entries invalid */
		pAC->LastPort = 3;
		pAC->LastOpc = 0xFF;
#endif
	}

	if (pAC->BoardLevel != SK_INIT_RUN) {
		/* tschilling: Level 2 init modules here, check return value. */
		if (SkGeInit(pAC, pAC->IoBase, SK_INIT_RUN) != 0) {
			printk("%s: HWInit (2) failed.\n", pAC->dev[pNet->PortNr]->name);
			return (-1);
		}
		SkI2cInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		SkEventInit	(pAC, pAC->IoBase, SK_INIT_RUN);

#ifdef SK_PNMI_SUPPORT
		SkPnmiInit	(pAC, pAC->IoBase, SK_INIT_RUN);
#endif

		SkAddrInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		SkRlmtInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		SkTimerInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		pAC->BoardLevel = SK_INIT_RUN;
	}

	for (CurrMac=0; CurrMac<pAC->GIni.GIMacsFound; CurrMac++) {
		if (!CHIP_ID_YUKON_2(pAC)) {
			/* Enable transmit descriptor polling. */
			SkGePollTxD(pAC, pAC->IoBase, CurrMac, SK_TRUE);
			FillRxRing(pAC, &pAC->RxPort[CurrMac]);
			SkMacRxTxEnable(pAC, pAC->IoBase, pNet->PortNr);
		}
	}

	SkGeYellowLED(pAC, pAC->IoBase, 1);
	SkDimEnableModerationIfNeeded(pAC);	

	if (!CHIP_ID_YUKON_2(pAC)) {
		/*
		** Has been setup already at SkGeInit(SK_INIT_IO),
		** but additional masking added for Genesis & Yukon
		** chipsets -> modify it...
		*/
		pAC->GIni.GIValIrqMask &= IRQ_MASK;
#ifndef USE_TX_COMPLETE
		pAC->GIni.GIValIrqMask &= ~(TX_COMPL_IRQS);
#endif
	}

	spin_lock_irqsave(&pAC->SlowPathLock, Flags);

	if ((pAC->RlmtMode != 0) && (pAC->MaxPorts == 0)) {
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_SET_NETS,
					pAC->RlmtNets, -1, SK_FALSE);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_MODE_CHANGE,
					pAC->RlmtMode, 0, SK_FALSE);
	}

	SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_START,
				pNet->NetNr, -1, SK_TRUE);
	spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);

#ifdef Y2_RECOVERY
	pNet->TimerExpired = SK_FALSE;
	pNet->InRecover = SK_FALSE;
	pNet->NetConsoleMode = SK_FALSE;

	/* Initialize the kernel timer */
#if 0 // u-boot
	init_timer(&pNet->KernelTimer);
	pNet->KernelTimer.function	= SkGeHandleKernelTimer;
	pNet->KernelTimer.data		= (unsigned long) pNet;
	pNet->KernelTimer.expires	= jiffies + (HZ/4); /* initially 250ms */
	add_timer(&pNet->KernelTimer);
#endif
#endif

	/* enable Interrupts */
	SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
	SK_OUT32(pAC->IoBase, B0_HWE_IMSK, IRQ_HWE_MASK);

	pAC->MaxPorts++;
	spin_unlock_irqrestore(&pAC->InitLock, InitFlags);

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeOpen suceeded\n"));

	return (0);
} /* SkGeOpen */


/****************************************************************************
 *
 *	SkGeClose - Stop initialized adapter
 *
 * Description:
 *	Close initialized adapter.
 *
 * Returns:
 *	0 - on success
 *	error code - on error
 */
#if 0 /* uboot */
static int SkGeClose(
#else
int SkGeClose(
#endif
struct SK_NET_DEVICE *dev)  /* the device that is to be closed */
{
	DEV_NET         *pNet = (DEV_NET*) dev->priv;
	SK_AC           *pAC  = pNet->pAC;
	DEV_NET         *newPtrNet;
	unsigned long    Flags;		/* for the spin locks		*/
	unsigned long    InitFlags;	/* for the spin locks		*/
	int              CurrMac;	/* loop ctr for the current MAC	*/
	int              PortIdx;
#ifdef CONFIG_SK98LIN_NAPI
	int              WorkToDo = 1; /* min(*budget, dev->quota);    */
	int              WorkDone = 0;
#endif
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeClose: pAC=0x%lX ", (unsigned long)pAC));
	spin_lock_irqsave(&pAC->InitLock, InitFlags);

#ifdef Y2_RECOVERY
	pNet->InRecover = SK_TRUE;
#if 0 /* uboot */
	del_timer(&pNet->KernelTimer);
#endif
#endif

	if (pAC->DiagModeActive == DIAG_ACTIVE) {
		if (pAC->DiagFlowCtrl == SK_FALSE) {
			MOD_DEC_USE_COUNT;
			/* 
			** notify that the interface which has been closed
			** by operator interaction must not be started up 
			** again when the DIAG has finished. 
			*/
			newPtrNet = (DEV_NET *) pAC->dev[0]->priv;
			if (newPtrNet == pNet) {
				pAC->WasIfUp[0] = SK_FALSE;
			} else {
				pAC->WasIfUp[1] = SK_FALSE;
			}
			return 0; /* return to system everything is fine... */
		} else {
			pAC->DiagFlowCtrl = SK_FALSE;
		}
	}

	netif_stop_queue(dev);

	if (pAC->RlmtNets == 1)
		PortIdx = pAC->ActivePort;
	else
		PortIdx = pNet->NetNr;

	/*
	 * Clear multicast table, promiscuous mode ....
	 */
	SkAddrMcClear(pAC, pAC->IoBase, PortIdx, 0);
	SkAddrPromiscuousChange(pAC, pAC->IoBase, PortIdx,
		SK_PROM_MODE_NONE);

	if (pAC->MaxPorts == 1) {
		spin_lock_irqsave(&pAC->SlowPathLock, Flags);
		/* disable interrupts */
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
					pNet->NetNr, -1, SK_TRUE);
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);
		/* stop the hardware */


		if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 1)) {
		/* RLMT check link state mode */
			for (CurrMac=0; CurrMac<pAC->GIni.GIMacsFound; CurrMac++) {
				if (CHIP_ID_YUKON_2(pAC))
					SkY2PortStop(	pAC, 
							pAC->IoBase,
							CurrMac,
							SK_STOP_ALL,
							SK_HARD_RST);
				else
					SkGeStopPort(	pAC,
							pAC->IoBase,
							CurrMac,
							SK_STOP_ALL,
							SK_HARD_RST);
			} /* for */
		} else {
		/* Single link or single port */
			if (CHIP_ID_YUKON_2(pAC))
				SkY2PortStop(	pAC, 
						pAC->IoBase,
						PortIdx,
						SK_STOP_ALL,
						SK_HARD_RST);
			else
				SkGeStopPort(	pAC,
						pAC->IoBase,
						PortIdx,
						SK_STOP_ALL,
						SK_HARD_RST);
		}
		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
	} else {
		spin_lock_irqsave(&pAC->SlowPathLock, Flags);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
					pNet->NetNr, -1, SK_FALSE);
		SkLocalEventQueue(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					pNet->NetNr, -1, SK_TRUE);
		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
		
		/* Stop port */
		spin_lock_irqsave(&pAC->TxPort[pNet->PortNr]
			[TX_PRIO_LOW].TxDesRingLock, Flags);
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStop(pAC, pAC->IoBase, pNet->PortNr,
				SK_STOP_ALL, SK_HARD_RST);
		}
		else {
			SkGeStopPort(pAC, pAC->IoBase, pNet->PortNr,
				SK_STOP_ALL, SK_HARD_RST);
		}
		spin_unlock_irqrestore(&pAC->TxPort[pNet->PortNr]
			[TX_PRIO_LOW].TxDesRingLock, Flags);
	}

	if (pAC->RlmtNets == 1) {
		/* clear all descriptor rings */
		for (CurrMac=0; CurrMac<pAC->GIni.GIMacsFound; CurrMac++) {
			if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
				WorkToDo = 1;
				ReceiveIrq(pAC,&pAC->RxPort[CurrMac],
						SK_TRUE,&WorkDone,WorkToDo);
#else
				ReceiveIrq(pAC,&pAC->RxPort[CurrMac],SK_TRUE);
#endif
				ClearRxRing(pAC, &pAC->RxPort[CurrMac]);
				ClearTxRing(pAC, &pAC->TxPort[CurrMac][TX_PRIO_LOW]);
			} else {
				SkY2FreeRxBuffers(pAC, pAC->IoBase, CurrMac);
				SkY2FreeTxBuffers(pAC, pAC->IoBase, CurrMac);
			}
		}
	} else {
		/* clear port descriptor rings */
		if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
			WorkToDo = 1;
			ReceiveIrq(pAC, &pAC->RxPort[pNet->PortNr], SK_TRUE, &WorkDone, WorkToDo);
#else
			ReceiveIrq(pAC, &pAC->RxPort[pNet->PortNr], SK_TRUE);
#endif
			ClearRxRing(pAC, &pAC->RxPort[pNet->PortNr]);
			ClearTxRing(pAC, &pAC->TxPort[pNet->PortNr][TX_PRIO_LOW]);
		}
		else {
			SkY2FreeRxBuffers(pAC, pAC->IoBase, pNet->PortNr);
			SkY2FreeTxBuffers(pAC, pAC->IoBase, pNet->PortNr);
		}
	}

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeClose: done "));

	SK_MEMSET(&(pAC->PnmiBackup), 0, sizeof(SK_PNMI_STRUCT_DATA));
	SK_MEMCPY(&(pAC->PnmiBackup), &(pAC->PnmiStruct), 
			sizeof(SK_PNMI_STRUCT_DATA));

	pAC->MaxPorts--;

#ifdef Y2_RECOVERY
	pNet->InRecover = SK_FALSE;
#endif
	spin_unlock_irqrestore(&pAC->InitLock, InitFlags);

	return (0);
} /* SkGeClose */


/*****************************************************************************
 *
 * 	SkGeXmit - Linux frame transmit function
 *
 * Description:
 *	The system calls this function to send frames onto the wire.
 *	It puts the frame in the tx descriptor ring. If the ring is
 *	full then, the 'tbusy' flag is set.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 * WARNING: returning 1 in 'tbusy' case caused system crashes (double
 *	allocated skb's) !!!
 */
#if 0
static int SkGeXmit(struct sk_buff *skb, struct SK_NET_DEVICE *dev)
#else
int SkGeXmit(struct sk_buff *skb, struct SK_NET_DEVICE *dev)
#endif
{
DEV_NET		*pNet;
SK_AC		*pAC;
int			Rc;	/* return code of XmitFrame */

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;

#if 0 /* uboot */
	if ((!skb_shinfo(skb)->nr_frags) ||
#else
	if(1 ||
#endif
		(pAC->GIni.GIChipId == CHIP_ID_GENESIS)) {
		/* Don't activate scatter-gather and hardware checksum */
		if (pAC->RlmtNets == 2)
			Rc = XmitFrame(
				pAC,
				&pAC->TxPort[pNet->PortNr][TX_PRIO_LOW],
				skb);
		else
			Rc = XmitFrame(
				pAC,
				&pAC->TxPort[pAC->ActivePort][TX_PRIO_LOW],
				skb);
	} else {
#if 0 /* uboot */
		/* scatter-gather and hardware TCP checksumming anabled*/
		if (pAC->RlmtNets == 2)
			Rc = XmitFrameSG(
				pAC,
				&pAC->TxPort[pNet->PortNr][TX_PRIO_LOW],
				skb);
		else
			Rc = XmitFrameSG(
				pAC,
				&pAC->TxPort[pAC->ActivePort][TX_PRIO_LOW],
				skb);
#endif
	}

	/* Transmitter out of resources? */
#ifdef USE_TX_COMPLETE
	if (Rc <= 0) {
		netif_stop_queue(dev);
	}
#endif

	/* If not taken, give buffer ownership back to the
	 * queueing layer.
	 */
	if (Rc < 0)
		return (1);

#if 0 /* uboot */
	dev->trans_start = jiffies;
#endif
	return (0);
} /* SkGeXmit */

#ifdef CONFIG_SK98LIN_NAPI
/*****************************************************************************
 *
 * 	SkGePoll - NAPI Rx polling callback for GEnesis and Yukon chipsets
 *
 * Description:
 *	Called by the Linux system in case NAPI polling is activated
 *
 * Returns:
 *	The number of work data still to be handled
 */
static int SkGePoll(struct net_device *dev, int *budget) 
{
	SK_AC		*pAC = ((DEV_NET*)(dev->priv))->pAC; /* pointer to adapter context */
	int		WorkToDo = min(*budget, dev->quota);
	int		WorkDone = 0;
	unsigned long	Flags;       


	if (pAC->dev[0] != pAC->dev[1]) {
		spin_lock(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
		FreeTxDescriptors(pAC, &pAC->TxPort[1][TX_PRIO_LOW]);
		spin_unlock(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);

		ReceiveIrq(pAC, &pAC->RxPort[1], SK_TRUE, &WorkDone, WorkToDo);
		CLEAR_AND_START_RX(1);
	}
	spin_lock(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
	FreeTxDescriptors(pAC, &pAC->TxPort[0][TX_PRIO_LOW]);
	spin_unlock(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);

	ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE, &WorkDone, WorkToDo);
	CLEAR_AND_START_RX(0);

	*budget -= WorkDone;
	dev->quota -= WorkDone;

	if(WorkDone < WorkToDo) {
		spin_lock_irqsave(&pAC->SlowPathLock, Flags);
		netif_rx_complete(dev);
		pAC->GIni.GIValIrqMask |= (NAPI_DRV_IRQS);
#ifndef USE_TX_COMPLETE
		pAC->GIni.GIValIrqMask &= ~(TX_COMPL_IRQS);
#endif
		/* enable interrupts again */
		SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
	}
	return (WorkDone >= WorkToDo);
} /* SkGePoll */
#endif

/* uboot */
unsigned long xchg(volatile int * m, unsigned long val) 
{
	unsigned int retval;
        retval = *m;
        *m = val;
        return retval;
}


#ifdef SK_POLL_CONTROLLER
/*****************************************************************************
 *
 * 	SkGeNetPoll - Polling "interrupt"
 *
 * Description:
 *	Polling 'interrupt' - used by things like netconsole and netdump
 *	to send skbs without having to re-enable interrupts.
 *	It's not called while the interrupt routine is executing.
 */
static void SkGeNetPoll(
struct SK_NET_DEVICE *dev) 
{
DEV_NET		*pNet;
SK_AC		*pAC;

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	pNet->NetConsoleMode = SK_TRUE;

		/*  Prevent any reconfiguration while handling
		    the 'interrupt' */
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);

		if (!CHIP_ID_YUKON_2(pAC)) {
		/* Handle the GENESIS Isr */
			if (pAC->GIni.GIMacsFound == 2)
				SkGeIsr(dev->irq, dev, NULL);
			else
				SkGeIsrOnePort(dev->irq, dev, NULL);
		} else {
		/* Handle the Yukon2 Isr */
			SkY2Isr(dev->irq, dev, NULL);
		}

}
#endif


/*****************************************************************************
 *
 * 	XmitFrame - fill one socket buffer into the transmit ring
 *
 * Description:
 *	This function puts a message into the transmit descriptor ring
 *	if there is a descriptors left.
 *	Linux skb's consist of only one continuous buffer.
 *	The first step locks the ring. It is held locked
 *	all time to avoid problems with SWITCH_../PORT_RESET.
 *	Then the descriptoris allocated.
 *	The second part is linking the buffer to the descriptor.
 *	At the very last, the Control field of the descriptor
 *	is made valid for the BMU and a start TX command is given
 *	if necessary.
 *
 * Returns:
 *	> 0 - on succes: the number of bytes in the message
 *	= 0 - on resource shortage: this frame sent or dropped, now
 *		the ring is full ( -> set tbusy)
 *	< 0 - on failure: other problems ( -> return failure to upper layers)
 */
static int XmitFrame(
SK_AC 		*pAC,		/* pointer to adapter context	        */
TX_PORT		*pTxPort,	/* pointer to struct of port to send to */
struct sk_buff	*pMessage)	/* pointer to send-message              */
{
	TXD		*pTxd;		/* the rxd to fill */
	TXD		*pOldTxd;
	unsigned long	 Flags;
	SK_U64		 PhysAddr;
#if 0 /* uboot */

	int	 	 Protocol;
	int		 IpHeaderLength;
#endif
	int		 BytesSend = pMessage->len;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_TX_PROGRESS, ("X"));

	spin_lock_irqsave(&pTxPort->TxDesRingLock, Flags);
#ifndef USE_TX_COMPLETE
	if ((pTxPort->TxdRingPrevFree - pTxPort->TxdRingFree) > 6)  {
		FreeTxDescriptors(pAC, pTxPort);
		pTxPort->TxdRingPrevFree = pTxPort->TxdRingFree;
	}
#endif
	if (pTxPort->TxdRingFree == 0) {
		/* 
		** not enough free descriptors in ring at the moment.
		** Maybe free'ing some old one help?
		*/
		FreeTxDescriptors(pAC, pTxPort);
		if (pTxPort->TxdRingFree == 0) {
			spin_unlock_irqrestore(&pTxPort->TxDesRingLock, Flags);
			SK_PNMI_CNT_NO_TX_BUF(pAC, pTxPort->PortIndex);
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_TX_PROGRESS,
				("XmitFrame failed\n"));
			/* 
			** the desired message can not be sent
			** Because tbusy seems to be set, the message 
			** should not be freed here. It will be used 
			** by the scheduler of the ethernet handler 
			*/
			return (-1);
		}
	}

#if 0 /* uboot*/
	/*
	** If the passed socket buffer is of smaller MTU-size than 60,
	** copy everything into new buffer and fill all bytes between
	** the original packet end and the new packet end of 60 with 0x00.
	** This is to resolve faulty padding by the HW with 0xaa bytes.
	*/
	if (BytesSend < C_LEN_ETHERNET_MINSIZE) {
		if ((pMessage = skb_padto(pMessage, C_LEN_ETHERNET_MINSIZE)) == NULL) {
			spin_unlock_irqrestore(&pTxPort->TxDesRingLock, Flags);
			return 0;
		}
		pMessage->len = C_LEN_ETHERNET_MINSIZE;
	}
#endif

	/* 
	** advance head counter behind descriptor needed for this frame, 
	** so that needed descriptor is reserved from that on. The next
	** action will be to add the passed buffer to the TX-descriptor
	*/
	pTxd = pTxPort->pTxdRingHead;
	pTxPort->pTxdRingHead = pTxd->pNextTxd;
	pTxPort->TxdRingFree--;

#ifdef SK_DUMP_TX
	DumpMsg(pMessage, "XmitFrame");
#endif

	/* 
	** First step is to map the data to be sent via the adapter onto
	** the DMA memory. Kernel 2.2 uses virt_to_bus(), but kernels 2.4
	** and 2.6 need to use pci_map_page() for that mapping.
	*/
#if 0 /* uboot */
	PhysAddr = (SK_U64) pci_map_page(pAC->PciDev,
					virt_to_page(pMessage->data),
					((unsigned long) pMessage->data & ~PAGE_MASK),
					pMessage->len,
					PCI_DMA_TODEVICE);
#else
	PhysAddr = (SK_U64)(SK_U32)pMessage->data;
#endif
	pTxd->VDataLow  = (SK_U32) (PhysAddr & 0xffffffff);
	pTxd->VDataHigh = (SK_U32) (PhysAddr >> 32);
	pTxd->pMBuf     = pMessage;

#if 0 /* uboot */
	if (pMessage->ip_summed == CHECKSUM_HW) {
		Protocol = ((SK_U8)pMessage->data[C_OFFSET_IPPROTO] & 0xff);
		if ((Protocol == C_PROTO_ID_UDP) && 
			(pAC->GIni.GIChipRev == 0) &&
			(pAC->GIni.GIChipId == CHIP_ID_YUKON)) {
			pTxd->TBControl = BMU_TCP_CHECK;
		} else {
			pTxd->TBControl = BMU_UDP_CHECK;
		}

		IpHeaderLength  = (SK_U8)pMessage->data[C_OFFSET_IPHEADER];
		IpHeaderLength  = (IpHeaderLength & 0xf) * 4;
		pTxd->TcpSumOfs = 0; /* PH-Checksum already calculated */
		pTxd->TcpSumSt  = C_LEN_ETHERMAC_HEADER + IpHeaderLength + 
							(Protocol == C_PROTO_ID_UDP ?
							C_OFFSET_UDPHEADER_UDPCS : 
							C_OFFSET_TCPHEADER_TCPCS);
		pTxd->TcpSumWr  = C_LEN_ETHERMAC_HEADER + IpHeaderLength;

		pTxd->TBControl |= BMU_OWN | BMU_STF | 
				   BMU_SW  | BMU_EOF |
#ifdef USE_TX_COMPLETE
				   BMU_IRQ_EOF |
#endif
				   pMessage->len;
	} else 
#endif
		{
		pTxd->TBControl = BMU_OWN | BMU_STF | BMU_CHECK | 
				  BMU_SW  | BMU_EOF |
#ifdef USE_TX_COMPLETE
				   BMU_IRQ_EOF |
#endif
			pMessage->len;
	}

	/* 
	** If previous descriptor already done, give TX start cmd 
	*/
	/* uboot */
	pOldTxd = (TXD *)xchg((int *)&pTxPort->pTxdRingPrev, (unsigned int)pTxd);
	if ((pOldTxd->TBControl & BMU_OWN) == 0) {
		SK_OUT8(pTxPort->HwAddr, Q_CSR, CSR_START);
	}	

	/* 
	** after releasing the lock, the skb may immediately be free'd 
	*/
	spin_unlock_irqrestore(&pTxPort->TxDesRingLock, Flags);
	if (pTxPort->TxdRingFree != 0) {
		return (BytesSend);
	} else {
		return (0);
	}

} /* XmitFrame */

/*****************************************************************************
 *
 * 	XmitFrameSG - fill one socket buffer into the transmit ring
 *                (use SG and TCP/UDP hardware checksumming)
 *
 * Description:
 *	This function puts a message into the transmit descriptor ring
 *	if there is a descriptors left.
 *
 * Returns:
 *	> 0 - on succes: the number of bytes in the message
 *	= 0 - on resource shortage: this frame sent or dropped, now
 *		the ring is full ( -> set tbusy)
 *	< 0 - on failure: other problems ( -> return failure to upper layers)
 */
#if 0 /* uboot */
static int XmitFrameSG(
SK_AC 		*pAC,		/* pointer to adapter context           */
TX_PORT		*pTxPort,	/* pointer to struct of port to send to */
struct sk_buff	*pMessage)	/* pointer to send-message              */
{

	TXD		*pTxd;
	TXD		*pTxdFst;
	TXD		*pTxdLst;
	int 	 	 CurrFrag;
	int		 BytesSend;
	int		 IpHeaderLength; 
	int		 Protocol;
	skb_frag_t	*sk_frag;
	SK_U64		 PhysAddr;
	unsigned long	 Flags;

	spin_lock_irqsave(&pTxPort->TxDesRingLock, Flags);
#ifndef USE_TX_COMPLETE
	FreeTxDescriptors(pAC, pTxPort);
#endif
	if ((skb_shinfo(pMessage)->nr_frags +1) > pTxPort->TxdRingFree) {
		FreeTxDescriptors(pAC, pTxPort);
		if ((skb_shinfo(pMessage)->nr_frags + 1) > pTxPort->TxdRingFree) {
			spin_unlock_irqrestore(&pTxPort->TxDesRingLock, Flags);
			SK_PNMI_CNT_NO_TX_BUF(pAC, pTxPort->PortIndex);
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_TX_PROGRESS,
				("XmitFrameSG failed - Ring full\n"));
				/* this message can not be sent now */
			return(-1);
		}
	}

	pTxd      = pTxPort->pTxdRingHead;
	pTxdFst   = pTxd;
	pTxdLst   = pTxd;
	BytesSend = 0;
	Protocol  = 0;

	/* 
	** Map the first fragment (header) into the DMA-space
	*/
	PhysAddr = (SK_U64) pci_map_page(pAC->PciDev,
			virt_to_page(pMessage->data),
			((unsigned long) pMessage->data & ~PAGE_MASK),
			skb_headlen(pMessage),
			PCI_DMA_TODEVICE);

	pTxd->VDataLow  = (SK_U32) (PhysAddr & 0xffffffff);
	pTxd->VDataHigh = (SK_U32) (PhysAddr >> 32);

	/* 
	** Does the HW need to evaluate checksum for TCP or UDP packets? 
	*/
	if (pMessage->ip_summed == CHECKSUM_HW) {
		pTxd->TBControl = BMU_STF | BMU_STFWD | skb_headlen(pMessage);
		/* 
		** We have to use the opcode for tcp here,  because the
		** opcode for udp is not working in the hardware yet 
		** (Revision 2.0)
		*/
		Protocol = ((SK_U8)pMessage->data[C_OFFSET_IPPROTO] & 0xff);
		if ((Protocol == C_PROTO_ID_UDP) && 
			(pAC->GIni.GIChipRev == 0) &&
			(pAC->GIni.GIChipId == CHIP_ID_YUKON)) {
			pTxd->TBControl |= BMU_TCP_CHECK;
		} else {
			pTxd->TBControl |= BMU_UDP_CHECK;
		}

		IpHeaderLength  = ((SK_U8)pMessage->data[C_OFFSET_IPHEADER] & 0xf)*4;
		pTxd->TcpSumOfs = 0; /* PH-Checksum already claculated */
		pTxd->TcpSumSt  = C_LEN_ETHERMAC_HEADER + IpHeaderLength +
						(Protocol == C_PROTO_ID_UDP ?
						C_OFFSET_UDPHEADER_UDPCS :
						C_OFFSET_TCPHEADER_TCPCS);
		pTxd->TcpSumWr  = C_LEN_ETHERMAC_HEADER + IpHeaderLength;
	} else {
		pTxd->TBControl = BMU_CHECK | BMU_SW | BMU_STF |
					skb_headlen(pMessage);
	}

	pTxd = pTxd->pNextTxd;
	pTxPort->TxdRingFree--;
	BytesSend += skb_headlen(pMessage);

	/* 
	** Browse over all SG fragments and map each of them into the DMA space
	*/
	for (CurrFrag = 0; CurrFrag < skb_shinfo(pMessage)->nr_frags; CurrFrag++) {
		sk_frag = &skb_shinfo(pMessage)->frags[CurrFrag];
		/* 
		** we already have the proper value in entry
		*/
		PhysAddr = (SK_U64) pci_map_page(pAC->PciDev,
						 sk_frag->page,
						 sk_frag->page_offset,
						 sk_frag->size,
						 PCI_DMA_TODEVICE);

		pTxd->VDataLow  = (SK_U32) (PhysAddr & 0xffffffff);
		pTxd->VDataHigh = (SK_U32) (PhysAddr >> 32);
		pTxd->pMBuf     = pMessage;
		
		/* 
		** Does the HW need to evaluate checksum for TCP or UDP packets? 
		*/
		if (pMessage->ip_summed == CHECKSUM_HW) {
			pTxd->TBControl = BMU_OWN | BMU_SW | BMU_STFWD;
			/* 
			** We have to use the opcode for tcp here because the 
			** opcode for udp is not working in the hardware yet 
			** (revision 2.0)
			*/
			if ((Protocol == C_PROTO_ID_UDP) && 
				(pAC->GIni.GIChipRev == 0) &&
				(pAC->GIni.GIChipId == CHIP_ID_YUKON)) {
				pTxd->TBControl |= BMU_TCP_CHECK;
			} else {
				pTxd->TBControl |= BMU_UDP_CHECK;
			}
		} else {
			pTxd->TBControl = BMU_CHECK | BMU_SW | BMU_OWN;
		}

		/* 
		** Do we have the last fragment? 
		*/
		if( (CurrFrag+1) == skb_shinfo(pMessage)->nr_frags )  {
#ifdef USE_TX_COMPLETE
			pTxd->TBControl |= BMU_EOF | BMU_IRQ_EOF | sk_frag->size;
#else
			pTxd->TBControl |= BMU_EOF | sk_frag->size;
#endif
			pTxdFst->TBControl |= BMU_OWN | BMU_SW;

		} else {
			pTxd->TBControl |= sk_frag->size;
		}
		pTxdLst = pTxd;
		pTxd    = pTxd->pNextTxd;
		pTxPort->TxdRingFree--;
		BytesSend += sk_frag->size;
	}

	/* 
	** If previous descriptor already done, give TX start cmd 
	*/
	if ((pTxPort->pTxdRingPrev->TBControl & BMU_OWN) == 0) {
		SK_OUT8(pTxPort->HwAddr, Q_CSR, CSR_START);
	}

	pTxPort->pTxdRingPrev = pTxdLst;
	pTxPort->pTxdRingHead = pTxd;

	spin_unlock_irqrestore(&pTxPort->TxDesRingLock, Flags);

	if (pTxPort->TxdRingFree > 0) {
		return (BytesSend);
	} else {
		return (0);
	}
}

#endif
/*****************************************************************************
 *
 * 	FreeTxDescriptors - release descriptors from the descriptor ring
 *
 * Description:
 *	This function releases descriptors from a transmit ring if they
 *	have been sent by the BMU.
 *	If a descriptors is sent, it can be freed and the message can
 *	be freed, too.
 *	The SOFTWARE controllable bit is used to prevent running around a
 *	completely free ring for ever. If this bit is no set in the
 *	frame (by XmitFrame), this frame has never been sent or is
 *	already freed.
 *	The Tx descriptor ring lock must be held while calling this function !!!
 *
 * Returns:
 *	none
 */
static void FreeTxDescriptors(
SK_AC	*pAC,		/* pointer to the adapter context */
TX_PORT	*pTxPort)	/* pointer to destination port structure */
{
TXD	*pTxd;		/* pointer to the checked descriptor */
TXD	*pNewTail;	/* pointer to 'end' of the ring */
SK_U32	Control;	/* TBControl field of descriptor */
SK_U64	PhysAddr;	/* address of DMA mapping */

	pNewTail = pTxPort->pTxdRingTail;
	pTxd     = pNewTail;
	/*
	** loop forever; exits if BMU_SW bit not set in start frame
	** or BMU_OWN bit set in any frame
	*/
	while (1) {
		Control = pTxd->TBControl;
		if ((Control & BMU_SW) == 0) {
			/*
			** software controllable bit is set in first
			** fragment when given to BMU. Not set means that
			** this fragment was never sent or is already
			** freed ( -> ring completely free now).
			*/
			pTxPort->pTxdRingTail = pTxd;
			netif_wake_queue(pAC->dev[pTxPort->PortIndex]);
			return;
		}
		if (Control & BMU_OWN) {
			pTxPort->pTxdRingTail = pTxd;
			if (pTxPort->TxdRingFree > 0) {
				netif_wake_queue(pAC->dev[pTxPort->PortIndex]);
			}
			return;
		}
		
		/* 
		** release the DMA mapping, because until not unmapped
		** this buffer is considered being under control of the
		** adapter card!
		*/
		PhysAddr = ((SK_U64) pTxd->VDataHigh) << (SK_U64) 32;
		PhysAddr |= (SK_U64) pTxd->VDataLow;
		pci_unmap_page(pAC->PciDev, PhysAddr,
				 pTxd->pMBuf->len,
				 PCI_DMA_TODEVICE);

		if (Control & BMU_EOF)
			DEV_KFREE_SKB_ANY(pTxd->pMBuf);	/* free message */

		pTxPort->TxdRingFree++;
		pTxd->TBControl &= ~BMU_SW;
		pTxd = pTxd->pNextTxd; /* point behind fragment with EOF */
	} /* while(forever) */
} /* FreeTxDescriptors */

/*****************************************************************************
 *
 * 	FillRxRing - fill the receive ring with valid descriptors
 *
 * Description:
 *	This function fills the receive ring descriptors with data
 *	segments and makes them valid for the BMU.
 *	The active ring is filled completely, if possible.
 *	The non-active ring is filled only partial to save memory.
 *
 * Description of rx ring structure:
 *	head - points to the descriptor which will be used next by the BMU
 *	tail - points to the next descriptor to give to the BMU
 *	
 * Returns:	N/A
 */
static void FillRxRing(
SK_AC		*pAC,		/* pointer to the adapter context */
RX_PORT		*pRxPort)	/* ptr to port struct for which the ring
				   should be filled */
{
unsigned long	Flags;

	spin_lock_irqsave(&pRxPort->RxDesRingLock, Flags);
	while (pRxPort->RxdRingFree > pRxPort->RxFillLimit) {
		if(!FillRxDescriptor(pAC, pRxPort))
			break;
	}
	spin_unlock_irqrestore(&pRxPort->RxDesRingLock, Flags);
} /* FillRxRing */


/*****************************************************************************
 *
 * 	FillRxDescriptor - fill one buffer into the receive ring
 *
 * Description:
 *	The function allocates a new receive buffer and
 *	puts it into the next descriptor.
 *
 * Returns:
 *	SK_TRUE - a buffer was added to the ring
 *	SK_FALSE - a buffer could not be added
 */
static SK_BOOL FillRxDescriptor(
SK_AC		*pAC,		/* pointer to the adapter context struct */
RX_PORT		*pRxPort)	/* ptr to port struct of ring to fill */
{
struct sk_buff	*pMsgBlock;	/* pointer to a new message block */
RXD		*pRxd;		/* the rxd to fill */
SK_U16		Length;		/* data fragment length */
SK_U64		PhysAddr;	/* physical address of a rx buffer */

	pMsgBlock = alloc_skb(pRxPort->RxBufSize, GFP_ATOMIC);
	if (pMsgBlock == NULL) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
			SK_DBGCAT_DRV_ENTRY,
			("%s: Allocation of rx buffer failed !\n",
			pAC->dev[pRxPort->PortIndex]->name));
		SK_PNMI_CNT_NO_RX_BUF(pAC, pRxPort->PortIndex);
		return(SK_FALSE);
	}
	skb_reserve(pMsgBlock, 2); /* to align IP frames */
	/* skb allocated ok, so add buffer */
	pRxd = pRxPort->pRxdRingTail;
	pRxPort->pRxdRingTail = pRxd->pNextRxd;
	pRxPort->RxdRingFree--;
	Length = pRxPort->RxBufSize;
#if 0 /* uboot */
	PhysAddr = (SK_U64) pci_map_page(pAC->PciDev,
		virt_to_page(pMsgBlock->data),
		((unsigned long) pMsgBlock->data &
		~PAGE_MASK),
		pRxPort->RxBufSize - 2,
		PCI_DMA_FROMDEVICE);

#else
	PhysAddr = (SK_U64)(SK_U32)pMsgBlock->data;
#endif
	pRxd->VDataLow  = (SK_U32) (PhysAddr & 0xffffffff);
	pRxd->VDataHigh = (SK_U32) (PhysAddr >> 32);
	pRxd->pMBuf     = pMsgBlock;
	pRxd->RBControl = BMU_OWN       | 
			  BMU_STF       | 
			  BMU_IRQ_EOF   | 
			  BMU_TCP_CHECK | 
			  Length;
	return (SK_TRUE);

} /* FillRxDescriptor */


/*****************************************************************************
 *
 * 	ReQueueRxBuffer - fill one buffer back into the receive ring
 *
 * Description:
 *	Fill a given buffer back into the rx ring. The buffer
 *	has been previously allocated and aligned, and its phys.
 *	address calculated, so this is no more necessary.
 *
 * Returns: N/A
 */
static void ReQueueRxBuffer(
SK_AC		*pAC,		/* pointer to the adapter context struct */
RX_PORT		*pRxPort,	/* ptr to port struct of ring to fill */
struct sk_buff	*pMsg,		/* pointer to the buffer */
SK_U32		PhysHigh,	/* phys address high dword */
SK_U32		PhysLow)	/* phys address low dword */
{
RXD		*pRxd;		/* the rxd to fill */
SK_U16		Length;		/* data fragment length */

	pRxd = pRxPort->pRxdRingTail;
	pRxPort->pRxdRingTail = pRxd->pNextRxd;
	pRxPort->RxdRingFree--;
	Length = pRxPort->RxBufSize;

	pRxd->VDataLow  = PhysLow;
	pRxd->VDataHigh = PhysHigh;
	pRxd->pMBuf     = pMsg;
	pRxd->RBControl = BMU_OWN       | 
			  BMU_STF       |
			  BMU_IRQ_EOF   | 
			  BMU_TCP_CHECK | 
			  Length;
	return;
} /* ReQueueRxBuffer */

/*****************************************************************************
 *
 * 	ReceiveIrq - handle a receive IRQ
 *
 * Description:
 *	This function is called when a receive IRQ is set.
 *	It walks the receive descriptor ring and sends up all
 *	frames that are complete.
 *
 * Returns:	N/A
 */
#if 0 /* uboot */
static void ReceiveIrq(
#else
void ReceiveIrq(
#endif
#ifdef CONFIG_SK98LIN_NAPI
SK_AC    *pAC,          /* pointer to adapter context          */
RX_PORT  *pRxPort,      /* pointer to receive port struct      */
SK_BOOL   SlowPathLock, /* indicates if SlowPathLock is needed */
int      *WorkDone,
int       WorkToDo)
#else
SK_AC    *pAC,          /* pointer to adapter context          */
RX_PORT  *pRxPort,      /* pointer to receive port struct      */
SK_BOOL   SlowPathLock) /* indicates if SlowPathLock is needed */
#endif
{
	RXD             *pRxd;          /* pointer to receive descriptors         */
	struct sk_buff  *pMsg;          /* pointer to message holding frame       */
	struct sk_buff  *pNewMsg;       /* pointer to new message for frame copy  */
	SK_MBUF         *pRlmtMbuf;     /* ptr to buffer for giving frame to RLMT */
	SK_EVPARA        EvPara;        /* an event parameter union        */	
	SK_U32           Control;       /* control field of descriptor     */
	unsigned long    Flags;         /* for spin lock handling          */
	int              PortIndex = pRxPort->PortIndex;
	int              FrameLength;   /* total length of received frame  */
	int              IpFrameLength; /* IP length of the received frame */
	unsigned int     Offset;
	unsigned int     NumBytes;
	unsigned int     RlmtNotifier;
	SK_BOOL          IsBc;          /* we received a broadcast packet  */
	SK_BOOL          IsMc;          /* we received a multicast packet  */
	SK_BOOL          IsBadFrame;    /* the frame received is bad!      */
	SK_U32           FrameStat;
	unsigned short   Csum1;
	unsigned short   Csum2;
	unsigned short   Type;
#if 0 /* uboot */
	int              Result;
#endif
	SK_U64           PhysAddr;

rx_start:	
	/* do forever; exit if BMU_OWN found */
	for ( pRxd = pRxPort->pRxdRingHead ;
		  pRxPort->RxdRingFree < pAC->RxDescrPerRing ;
		  pRxd = pRxd->pNextRxd,
		  pRxPort->pRxdRingHead = pRxd,
		  pRxPort->RxdRingFree ++) {

		/*
		 * For a better understanding of this loop
		 * Go through every descriptor beginning at the head
		 * Please note: the ring might be completely received so the OWN bit
		 * set is not a good crirteria to leave that loop.
		 * Therefore the RingFree counter is used.
		 * On entry of this loop pRxd is a pointer to the Rxd that needs
		 * to be checked next.
		 */

		Control = pRxd->RBControl;
	
#ifdef CONFIG_SK98LIN_NAPI
		if (*WorkDone >= WorkToDo) {
			break;
		}
		(*WorkDone)++;
#endif

		/* check if this descriptor is ready */
		if ((Control & BMU_OWN) != 0) {
			/* this descriptor is not yet ready */
			/* This is the usual end of the loop */
			/* We don't need to start the ring again */
			FillRxRing(pAC, pRxPort);
			return;
		}

		/* get length of frame and check it */
		FrameLength = Control & BMU_BBC;
		if (FrameLength > pRxPort->RxBufSize) {
			goto rx_failed;
		}

		/* check for STF and EOF */
		if ((Control & (BMU_STF | BMU_EOF)) != (BMU_STF | BMU_EOF)) {
			goto rx_failed;
		}

		/* here we have a complete frame in the ring */
		pMsg = pRxd->pMBuf;

		FrameStat = pRxd->FrameStat;

		/* check for frame length mismatch */
#define XMR_FS_LEN_SHIFT	18
#define GMR_FS_LEN_SHIFT	16
		if (pAC->GIni.GIChipId == CHIP_ID_GENESIS) {
			if (FrameLength != (SK_U32) (FrameStat >> XMR_FS_LEN_SHIFT)) {
				SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
					SK_DBGCAT_DRV_RX_PROGRESS,
					("skge: Frame length mismatch (%u/%u).\n",
					FrameLength,
					(SK_U32) (FrameStat >> XMR_FS_LEN_SHIFT)));
				goto rx_failed;
			}
		} else {
			if (FrameLength != (SK_U32) (FrameStat >> GMR_FS_LEN_SHIFT)) {
				SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
					SK_DBGCAT_DRV_RX_PROGRESS,
					("skge: Frame length mismatch (%u/%u).\n",
					FrameLength,
					(SK_U32) (FrameStat >> XMR_FS_LEN_SHIFT)));
				goto rx_failed;
			}
		}

		/* Set Rx Status */
		if (pAC->GIni.GIChipId == CHIP_ID_GENESIS) {
			IsBc = (FrameStat & XMR_FS_BC) != 0;
			IsMc = (FrameStat & XMR_FS_MC) != 0;
			IsBadFrame = (FrameStat &
				(XMR_FS_ANY_ERR | XMR_FS_2L_VLAN)) != 0;
		} else {
			IsBc = (FrameStat & GMR_FS_BC) != 0;
			IsMc = (FrameStat & GMR_FS_MC) != 0;
			IsBadFrame = (((FrameStat & GMR_FS_ANY_ERR) != 0) ||
							((FrameStat & GMR_FS_RX_OK) == 0));
		}

		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, 0,
			("Received frame of length %d on port %d\n",
			FrameLength, PortIndex));
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, 0,
			("Number of free rx descriptors: %d\n",
			pRxPort->RxdRingFree));
/* DumpMsg(pMsg, "Rx");	*/

		if ((Control & BMU_STAT_VAL) != BMU_STAT_VAL || (IsBadFrame)) {
#if 0
			(FrameStat & (XMR_FS_ANY_ERR | XMR_FS_2L_VLAN)) != 0) {
#endif
			/* there is a receive error in this frame */
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_RX_PROGRESS,
				("skge: Error in received frame, dropped!\n"
				"Control: %x\nRxStat: %x\n",
				Control, FrameStat));

			PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
			PhysAddr |= (SK_U64) pRxd->VDataLow;
			pci_dma_sync_single(pAC->PciDev,
						(dma_addr_t) PhysAddr,
						FrameLength,
						PCI_DMA_FROMDEVICE);
			ReQueueRxBuffer(pAC, pRxPort, pMsg,
				pRxd->VDataHigh, pRxd->VDataLow);

			continue;
		}

		/*
		 * if short frame then copy data to reduce memory waste
		 */
		if ((FrameLength < SK_COPY_THRESHOLD) &&
			((pNewMsg = alloc_skb(FrameLength+2, GFP_ATOMIC)) != NULL)) {
			/*
			 * Short frame detected and allocation successfull
			 */
			/* use new skb and copy data */
			skb_reserve(pNewMsg, 2);
			skb_put(pNewMsg, FrameLength);
			PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
			PhysAddr |= (SK_U64) pRxd->VDataLow;
			pci_dma_sync_single(pAC->PciDev,
						(dma_addr_t) PhysAddr,
						FrameLength,
						PCI_DMA_FROMDEVICE);
			eth_copy_and_sum(pNewMsg, pMsg->data,
				FrameLength, 0);
			ReQueueRxBuffer(pAC, pRxPort, pMsg,
				pRxd->VDataHigh, pRxd->VDataLow);

			pMsg = pNewMsg;

		} else {
			/*
			 * if large frame, or SKB allocation failed, pass
			 * the SKB directly to the networking
			 */
			PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
			PhysAddr |= (SK_U64) pRxd->VDataLow;

			/* release the DMA mapping */
			pci_unmap_single(pAC->PciDev,
					 PhysAddr,
					 pRxPort->RxBufSize - 2,
					 PCI_DMA_FROMDEVICE);
			skb_put(pMsg, FrameLength); /* set message len */
#if 0 /* uboot */
			pMsg->ip_summed = CHECKSUM_NONE; /* initial default */
#endif

			if (pRxPort->UseRxCsum) {
				Type = ntohs(*((short*)&pMsg->data[12]));
				if (Type == 0x800) {
					IpFrameLength = (int) ntohs((unsigned short)
							((unsigned short *) pMsg->data)[8]);
					if ((FrameLength - IpFrameLength) == 0xe) {
						Csum1=le16_to_cpu(pRxd->TcpSums & 0xffff);
						Csum2=le16_to_cpu((pRxd->TcpSums >> 16) & 0xffff);
#if 0 /* uboot */
						if ((((Csum1 & 0xfffe) && (Csum2 & 0xfffe)) &&
							(pAC->GIni.GIChipId == CHIP_ID_GENESIS)) ||
							(pAC->ChipsetType)) {
							Result = SkCsGetReceiveInfo(pAC, &pMsg->data[14],
								Csum1, Csum2, PortIndex);
							if ((Result == SKCS_STATUS_IP_FRAGMENT) ||
							    (Result == SKCS_STATUS_IP_CSUM_OK)  ||
							    (Result == SKCS_STATUS_TCP_CSUM_OK) ||
							    (Result == SKCS_STATUS_UDP_CSUM_OK)) {
								pMsg->ip_summed = CHECKSUM_UNNECESSARY;
							} else if ((Result == SKCS_STATUS_TCP_CSUM_ERROR)    ||
							           (Result == SKCS_STATUS_UDP_CSUM_ERROR)    ||
							           (Result == SKCS_STATUS_IP_CSUM_ERROR_UDP) ||
							           (Result == SKCS_STATUS_IP_CSUM_ERROR_TCP) ||
							           (Result == SKCS_STATUS_IP_CSUM_ERROR)) {
								/* HW Checksum error */
								SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
								SK_DBGCAT_DRV_RX_PROGRESS,
								("skge: CRC error. Frame dropped!\n"));
								goto rx_failed;
							} else {
								pMsg->ip_summed = CHECKSUM_NONE;
							}
						}/* checksumControl calculation valid */
#endif 
					} /* Frame length check */
				} /* IP frame */
			} /* pRxPort->UseRxCsum */
		} /* frame > SK_COPY_TRESHOLD */
		
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV,	1,("V"));
		RlmtNotifier = SK_RLMT_RX_PROTOCOL;
		SK_RLMT_PRE_LOOKAHEAD(pAC, PortIndex, FrameLength,
					IsBc, &Offset, &NumBytes);
		if (NumBytes != 0) {
			SK_RLMT_LOOKAHEAD(pAC,PortIndex,&pMsg->data[Offset],
						IsBc,IsMc,&RlmtNotifier);
		}
		if (RlmtNotifier == SK_RLMT_RX_PROTOCOL) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,	1,("W"));
			/* send up only frames from active port */
			if ((PortIndex == pAC->ActivePort)||(pAC->RlmtNets == 2)) {
				SK_DBG_MSG(NULL, SK_DBGMOD_DRV, 1,("U"));
#ifdef xDEBUG
				DumpMsg(pMsg, "Rx");
#endif
#if 0 /* uboot */
				SK_PNMI_CNT_RX_OCTETS_DELIVERED(pAC,FrameLength,PortIndex);
				pMsg->dev = pAC->dev[PortIndex];
				pMsg->protocol = eth_type_trans(pMsg,pAC->dev[PortIndex]);
				netif_rx(pMsg); /* frame for upper layer */
				pAC->dev[PortIndex]->last_rx = jiffies;
#else
                                NetReceive(pMsg->data, pMsg->len);
                                dev_kfree_skb_any(pMsg);
#endif
			} else {
				SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
					SK_DBGCAT_DRV_RX_PROGRESS,("D"));
				DEV_KFREE_SKB(pMsg); /* drop frame */
			}
		} else { /* packet for RLMT stack */
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_RX_PROGRESS,("R"));
			pRlmtMbuf = SkDrvAllocRlmtMbuf(pAC,
				pAC->IoBase, FrameLength);
			if (pRlmtMbuf != NULL) {
				pRlmtMbuf->pNext = NULL;
				pRlmtMbuf->Length = FrameLength;
				pRlmtMbuf->PortIdx = PortIndex;
				EvPara.pParaPtr = pRlmtMbuf;
				memcpy((char*)(pRlmtMbuf->pData),
					   (char*)(pMsg->data),
					   FrameLength);

				/* SlowPathLock needed? */
				if (SlowPathLock == SK_TRUE) {
					spin_lock_irqsave(&pAC->SlowPathLock, Flags);
					SkEventQueue(pAC, SKGE_RLMT,
						SK_RLMT_PACKET_RECEIVED,
						EvPara);
					pAC->CheckQueue = SK_TRUE;
					spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
				} else {
					SkEventQueue(pAC, SKGE_RLMT,
						SK_RLMT_PACKET_RECEIVED,
						EvPara);
					pAC->CheckQueue = SK_TRUE;
				}

				SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
					SK_DBGCAT_DRV_RX_PROGRESS,("Q"));
			}
#if 0 /* uboot */
			if ((pAC->dev[PortIndex]->flags & (IFF_PROMISC | IFF_ALLMULTI)) ||
			    (RlmtNotifier & SK_RLMT_RX_PROTOCOL)) {
				pMsg->dev = pAC->dev[PortIndex];
				pMsg->protocol = eth_type_trans(pMsg,pAC->dev[PortIndex]);
				netif_rx(pMsg);
				pAC->dev[PortIndex]->last_rx = jiffies;
#else
			if (0) {
#endif
			} else {
				DEV_KFREE_SKB(pMsg);
			}
		} /* if packet for RLMT stack */
	} /* for ... scanning the RXD ring */

	/* RXD ring is empty -> fill and restart */
	FillRxRing(pAC, pRxPort);
	return;

rx_failed:
	/* remove error frame */
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ERROR,
		("Schrottdescriptor, length: 0x%x\n", FrameLength));

	/* release the DMA mapping */

	PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
	PhysAddr |= (SK_U64) pRxd->VDataLow;
	pci_unmap_page(pAC->PciDev,
			 PhysAddr,
			 pRxPort->RxBufSize - 2,
			 PCI_DMA_FROMDEVICE);
	DEV_KFREE_SKB_IRQ(pRxd->pMBuf);
	pRxd->pMBuf = NULL;
	pRxPort->RxdRingFree++;
	pRxPort->pRxdRingHead = pRxd->pNextRxd;
	goto rx_start;

} /* ReceiveIrq */

/*****************************************************************************
 *
 * 	ClearRxRing - remove all buffers from the receive ring
 *
 * Description:
 *	This function removes all receive buffers from the ring.
 *	The receive BMU must be stopped before calling this function.
 *
 * Returns: N/A
 */
static void ClearRxRing(
SK_AC	*pAC,		/* pointer to adapter context */
RX_PORT	*pRxPort)	/* pointer to rx port struct */
{
RXD		*pRxd;	/* pointer to the current descriptor */
unsigned long	Flags;
SK_U64		PhysAddr;

	if (pRxPort->RxdRingFree == pAC->RxDescrPerRing) {
		return;
	}
	spin_lock_irqsave(&pRxPort->RxDesRingLock, Flags);
	pRxd = pRxPort->pRxdRingHead;
	do {
		if (pRxd->pMBuf != NULL) {

			PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
			PhysAddr |= (SK_U64) pRxd->VDataLow;
			pci_unmap_page(pAC->PciDev,
					 PhysAddr,
					 pRxPort->RxBufSize - 2,
					 PCI_DMA_FROMDEVICE);
			DEV_KFREE_SKB(pRxd->pMBuf);
			pRxd->pMBuf = NULL;
		}
		pRxd->RBControl &= BMU_OWN;
		pRxd = pRxd->pNextRxd;
		pRxPort->RxdRingFree++;
	} while (pRxd != pRxPort->pRxdRingTail);
	pRxPort->pRxdRingTail = pRxPort->pRxdRingHead;
	spin_unlock_irqrestore(&pRxPort->RxDesRingLock, Flags);
} /* ClearRxRing */

/*****************************************************************************
 *
 *	ClearTxRing - remove all buffers from the transmit ring
 *
 * Description:
 *	This function removes all transmit buffers from the ring.
 *	The transmit BMU must be stopped before calling this function
 *	and transmitting at the upper level must be disabled.
 *	The BMU own bit of all descriptors is cleared, the rest is
 *	done by calling FreeTxDescriptors.
 *
 * Returns: N/A
 */
static void ClearTxRing(
SK_AC	*pAC,		/* pointer to adapter context */
TX_PORT	*pTxPort)	/* pointer to tx prt struct */
{
TXD		*pTxd;		/* pointer to the current descriptor */
int		i;
unsigned long	Flags;

	spin_lock_irqsave(&pTxPort->TxDesRingLock, Flags);
	pTxd = pTxPort->pTxdRingHead;
	for (i=0; i<pAC->TxDescrPerRing; i++) {
		pTxd->TBControl &= ~BMU_OWN;
		pTxd = pTxd->pNextTxd;
	}
	FreeTxDescriptors(pAC, pTxPort);
	spin_unlock_irqrestore(&pTxPort->TxDesRingLock, Flags);
} /* ClearTxRing */

#if 0 /* uboot */
/*****************************************************************************
 *
 * 	SkGeSetMacAddr - Set the hardware MAC address
 *
 * Description:
 *	This function sets the MAC address used by the adapter.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static int SkGeSetMacAddr(struct SK_NET_DEVICE *dev, void *p)
{

DEV_NET *pNet = (DEV_NET*) dev->priv;
SK_AC	*pAC = pNet->pAC;
int	Ret;

struct sockaddr	*addr = p;
unsigned long	Flags;
	
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeSetMacAddr starts now...\n"));

	memcpy(dev->dev_addr, addr->sa_data,dev->addr_len);
	
	spin_lock_irqsave(&pAC->SlowPathLock, Flags);

	if (pAC->RlmtNets == 2)
		Ret = SkAddrOverride(pAC, pAC->IoBase, pNet->NetNr,
			(SK_MAC_ADDR*)dev->dev_addr, SK_ADDR_VIRTUAL_ADDRESS);
	else
		Ret = SkAddrOverride(pAC, pAC->IoBase, pAC->ActivePort,
			(SK_MAC_ADDR*)dev->dev_addr, SK_ADDR_VIRTUAL_ADDRESS);
	
	spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);

	if (Ret != SK_ADDR_OVERRIDE_SUCCESS)
		return -EBUSY;

	return 0;
} /* SkGeSetMacAddr */


/*****************************************************************************
 *
 * 	SkGeSetRxMode - set receive mode
 *
 * Description:
 *	This function sets the receive mode of an adapter. The adapter
 *	supports promiscuous mode, allmulticast mode and a number of
 *	multicast addresses. If more multicast addresses the available
 *	are selected, a hash function in the hardware is used.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static void SkGeSetRxMode(struct SK_NET_DEVICE *dev)
{

DEV_NET		*pNet;
SK_AC		*pAC;

struct dev_mc_list	*pMcList;
int			i;
int			PortIdx;
unsigned long		Flags;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeSetRxMode starts now... "));

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	if (pAC->RlmtNets == 1)
		PortIdx = pAC->ActivePort;
	else
		PortIdx = pNet->NetNr;

	spin_lock_irqsave(&pAC->SlowPathLock, Flags);
	if (dev->flags & IFF_PROMISC) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
			("PROMISCUOUS mode\n"));
		SkAddrPromiscuousChange(pAC, pAC->IoBase, PortIdx,
			SK_PROM_MODE_LLC);
	} else if (dev->flags & IFF_ALLMULTI) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
			("ALLMULTI mode\n"));
		SkAddrPromiscuousChange(pAC, pAC->IoBase, PortIdx,
			SK_PROM_MODE_ALL_MC);
	} else {
		SkAddrPromiscuousChange(pAC, pAC->IoBase, PortIdx,
			SK_PROM_MODE_NONE);
		SkAddrMcClear(pAC, pAC->IoBase, PortIdx, 0);

		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
			("Number of MC entries: %d ", dev->mc_count));
		
		pMcList = dev->mc_list;
		for (i=0; i<dev->mc_count; i++, pMcList = pMcList->next) {
			SkAddrMcAdd(pAC, pAC->IoBase, PortIdx,
				(SK_MAC_ADDR*)pMcList->dmi_addr, 0);
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MCA,
				("%02x:%02x:%02x:%02x:%02x:%02x\n",
				pMcList->dmi_addr[0],
				pMcList->dmi_addr[1],
				pMcList->dmi_addr[2],
				pMcList->dmi_addr[3],
				pMcList->dmi_addr[4],
				pMcList->dmi_addr[5]));
		}
		SkAddrMcUpdate(pAC, pAC->IoBase, PortIdx);
	}
	spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
	
	return;
} /* SkGeSetRxMode */


/*****************************************************************************
 *
 * 	SkSetMtuBufferSize - set the MTU buffer to another value
 *
 * Description:
 *	This function sets the new buffers and is called whenever the MTU 
 *      size is changed
 *
 * Returns:
 *	N/A
 */

static void SkSetMtuBufferSize(
SK_AC	*pAC,		/* pointer to adapter context */
int	PortNr,		/* Port number */
int	Mtu)		/* pointer to tx prt struct */
{
	pAC->RxPort[PortNr].RxBufSize = Mtu + 32;

	/* RxBufSize must be a multiple of 8 */
	while (pAC->RxPort[PortNr].RxBufSize % 8) {
		pAC->RxPort[PortNr].RxBufSize = 
			pAC->RxPort[PortNr].RxBufSize + 1;
	}

	if (Mtu > 1500) {
		pAC->GIni.GP[PortNr].PPortUsage = SK_JUMBO_LINK;
	} else {
		if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 2)) {
			pAC->GIni.GP[PortNr].PPortUsage = SK_MUL_LINK;
		} else {
			pAC->GIni.GP[PortNr].PPortUsage = SK_RED_LINK;
		}
	}

	return;
}


/*****************************************************************************
 *
 * 	SkGeChangeMtu - set the MTU to another value
 *
 * Description:
 *	This function sets is called whenever the MTU size is changed
 *	(ifconfig mtu xxx dev ethX). If the MTU is bigger than standard
 *	ethernet MTU size, long frame support is activated.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static int SkGeChangeMtu(struct SK_NET_DEVICE *dev, int NewMtu)
{
DEV_NET			*pNet;
SK_AC			*pAC;
unsigned long		Flags;
#ifdef CONFIG_SK98LIN_NAPI
int			WorkToDo = 1; // min(*budget, dev->quota);
int			WorkDone = 0;
#endif

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeChangeMtu starts now...\n"));

	pNet = (DEV_NET*) dev->priv;
	pAC  = pNet->pAC;

	/* MTU size outside the spec */
	if ((NewMtu < 68) || (NewMtu > SK_JUMBO_MTU)) {
		return -EINVAL;
	}

	/* MTU > 1500 on yukon ulra not allowed */
	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_EC_U) 
		&& (NewMtu > 1500)){
		return -EINVAL;
	}

	/* Diag access active */
	if (pAC->DiagModeActive == DIAG_ACTIVE) {
		if (pAC->DiagFlowCtrl == SK_FALSE) {
			return -1; /* still in use, deny any actions of MTU */
		} else {
			pAC->DiagFlowCtrl = SK_FALSE;
		}
	}

	dev->mtu = NewMtu;
	SkSetMtuBufferSize(pAC, pNet->PortNr, NewMtu);

	if(!netif_running(dev)) {
	/* Preset MTU size if device not ready/running */
		return 0;
	}

	/*  Prevent any reconfiguration while changing the MTU 
	    by disabling any interrupts */
	SK_OUT32(pAC->IoBase, B0_IMSK, 0);
	spin_lock_irqsave(&pAC->SlowPathLock, Flags);

	/* Notify RLMT that the port has to be stopped */
	netif_stop_queue(dev);
	SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
				pNet->PortNr, -1, SK_TRUE);
	spin_lock(&pAC->TxPort[pNet->PortNr][TX_PRIO_LOW].TxDesRingLock);


	/* Change RxFillLimit to 1 */
	if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 2)) {
		pAC->RxPort[pNet->PortNr].RxFillLimit = 1;
	} else {
		pAC->RxPort[1 - pNet->PortNr].RxFillLimit = 1;
		pAC->RxPort[pNet->PortNr].RxFillLimit = pAC->RxDescrPerRing -
					(pAC->RxDescrPerRing / 4);
	}

	/* clear and reinit the rx rings here, because of new MTU size */
	if (CHIP_ID_YUKON_2(pAC)) {
		SkY2PortStop(pAC, pAC->IoBase, pNet->PortNr, SK_STOP_ALL, SK_SOFT_RST);
		SkY2AllocateRxBuffers(pAC, pAC->IoBase, pNet->PortNr);
		SkY2PortStart(pAC, pAC->IoBase, pNet->PortNr);
	} else {
//		SkGeStopPort(pAC, pAC->IoBase, pNet->PortNr, SK_STOP_ALL, SK_SOFT_RST);
#ifdef CONFIG_SK98LIN_NAPI
		WorkToDo = 1;
		ReceiveIrq(pAC, &pAC->RxPort[pNet->PortNr], SK_TRUE, &WorkDone, WorkToDo);
#else
		ReceiveIrq(pAC, &pAC->RxPort[pNet->PortNr], SK_TRUE);
#endif
		ClearRxRing(pAC, &pAC->RxPort[pNet->PortNr]);
		FillRxRing(pAC, &pAC->RxPort[pNet->PortNr]);

		/* Enable transmit descriptor polling */
		SkGePollTxD(pAC, pAC->IoBase, pNet->PortNr, SK_TRUE);
		FillRxRing(pAC, &pAC->RxPort[pNet->PortNr]);
	}

	netif_start_queue(pAC->dev[pNet->PortNr]);

	spin_unlock(&pAC->TxPort[pNet->PortNr][TX_PRIO_LOW].TxDesRingLock);


	/* Notify RLMT about the changing and restarting one (or more) ports */
	SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_START,
					pNet->PortNr, -1, SK_TRUE);

	/* Enable Interrupts again */
	SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
	SK_OUT32(pAC->IoBase, B0_HWE_IMSK, IRQ_HWE_MASK);

	spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
	return 0;

}


/*****************************************************************************
 *
 * 	SkGeStats - return ethernet device statistics
 *
 * Description:
 *	This function return statistic data about the ethernet device
 *	to the operating system.
 *
 * Returns:
 *	pointer to the statistic structure.
 */
static struct net_device_stats *SkGeStats(struct SK_NET_DEVICE *dev)
{
	DEV_NET		*pNet = (DEV_NET*) dev->priv;
	SK_AC		*pAC = pNet->pAC;
	unsigned long	LateCollisions, ExcessiveCollisions, RxTooLong;
	unsigned long	Flags; /* for spin lock */
    	SK_U32		MaxNumOidEntries, Oid, Len;
	char		Buf[8];
	struct {
		SK_U32         Oid;
		unsigned long *pVar;
	} Vars[] = {
		{ OID_SKGE_STAT_TX_LATE_COL,   &LateCollisions               },
		{ OID_SKGE_STAT_TX_EXCESS_COL, &ExcessiveCollisions          },
		{ OID_SKGE_STAT_RX_TOO_LONG,   &RxTooLong                    },
		{ OID_SKGE_STAT_RX,            &pAC->stats.rx_packets        },
		{ OID_SKGE_STAT_TX,            &pAC->stats.tx_packets        },
		{ OID_SKGE_STAT_RX_OCTETS,     &pAC->stats.rx_bytes          },
		{ OID_SKGE_STAT_TX_OCTETS,     &pAC->stats.tx_bytes          },
		{ OID_SKGE_RX_NO_BUF_CTS,      &pAC->stats.rx_dropped        },
		{ OID_SKGE_TX_NO_BUF_CTS,      &pAC->stats.tx_dropped        },
		{ OID_SKGE_STAT_RX_MULTICAST,  &pAC->stats.multicast         },
		{ OID_SKGE_STAT_RX_RUNT,       &pAC->stats.rx_length_errors  },
		{ OID_SKGE_STAT_RX_FCS,        &pAC->stats.rx_crc_errors     },
		{ OID_SKGE_STAT_RX_FRAMING,    &pAC->stats.rx_frame_errors   },
		{ OID_SKGE_STAT_RX_OVERFLOW,   &pAC->stats.rx_over_errors    },
		{ OID_SKGE_STAT_RX_MISSED,     &pAC->stats.rx_missed_errors  },
		{ OID_SKGE_STAT_TX_CARRIER,    &pAC->stats.tx_carrier_errors },
		{ OID_SKGE_STAT_TX_UNDERRUN,   &pAC->stats.tx_fifo_errors    },
	};
	
	if ((pAC->DiagModeActive == DIAG_NOTACTIVE) &&
	    (pAC->BoardLevel     == SK_INIT_RUN)) {
		memset(&pAC->stats, 0x00, sizeof(pAC->stats)); /* clean first */
		spin_lock_irqsave(&pAC->SlowPathLock, Flags);

    		MaxNumOidEntries = sizeof(Vars) / sizeof(Vars[0]);
    		for (Oid = 0; Oid < MaxNumOidEntries; Oid++) {
			if (SkPnmiGetVar(pAC,pAC->IoBase, Vars[Oid].Oid,
				&Buf, &Len, 1, pNet->NetNr) != SK_PNMI_ERR_OK) {
				memset(Buf, 0x00, sizeof(Buf));
			}
			*Vars[Oid].pVar = (unsigned long) (*((SK_U64 *) Buf));
		}
		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);

		pAC->stats.collisions =	LateCollisions + ExcessiveCollisions;
		pAC->stats.tx_errors =	pAC->stats.tx_carrier_errors +
					pAC->stats.tx_fifo_errors;
		pAC->stats.rx_errors =	pAC->stats.rx_length_errors + 
					pAC->stats.rx_crc_errors +
					pAC->stats.rx_frame_errors + 
					pAC->stats.rx_over_errors +
					pAC->stats.rx_missed_errors;

		if (dev->mtu > 1500) {
			pAC->stats.rx_errors = pAC->stats.rx_errors - RxTooLong;
		}
	}

	return(&pAC->stats);
} /* SkGeStats */

/*****************************************************************************
 *
 * 	SkGeIoctl - IO-control function
 *
 * Description:
 *	This function is called if an ioctl is issued on the device.
 *	There are three subfunction for reading, writing and test-writing
 *	the private MIB data structure (usefull for SysKonnect-internal tools).
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static int SkGeIoctl(
struct SK_NET_DEVICE *dev,  /* the device the IOCTL is to be performed on   */
struct ifreq         *rq,   /* additional request structure containing data */
int                   cmd)  /* requested IOCTL command number               */
{
	DEV_NET          *pNet = (DEV_NET*) dev->priv;
	SK_AC            *pAC  = pNet->pAC;
	struct pci_dev   *pdev = NULL;
	void             *pMemBuf;
	SK_GE_IOCTL       Ioctl;
	unsigned long     Flags; /* for spin lock */
	unsigned int      Err = 0;
	unsigned int      Length = 0;
	int               HeaderLength = sizeof(SK_U32) + sizeof(SK_U32);
	int               Size = 0;
	int               Ret = 0;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeIoctl starts now...\n"));

	if(copy_from_user(&Ioctl, rq->ifr_data, sizeof(SK_GE_IOCTL))) {
		return -EFAULT;
	}

	switch(cmd) {
	case SIOCETHTOOL:
		return SkEthIoctl(dev, rq);
	case SK_IOCTL_SETMIB:     /* FALL THRU */
	case SK_IOCTL_PRESETMIB:  /* FALL THRU (if capable!) */
		if (!capable(CAP_NET_ADMIN)) return -EPERM;
 	case SK_IOCTL_GETMIB:
		if(copy_from_user(&pAC->PnmiStruct, Ioctl.pData,
			Ioctl.Len<sizeof(pAC->PnmiStruct)?
			Ioctl.Len : sizeof(pAC->PnmiStruct))) {
			return -EFAULT;
		}
		Size = SkGeIocMib(pNet, Ioctl.Len, cmd);
		if(copy_to_user(Ioctl.pData, &pAC->PnmiStruct,
			Ioctl.Len<Size? Ioctl.Len : Size)) {
			return -EFAULT;
		}
		Ioctl.Len = Size;
		if(copy_to_user(rq->ifr_data, &Ioctl, sizeof(SK_GE_IOCTL))) {
			return -EFAULT;
		}
		break;
	case SK_IOCTL_GEN:
		if (Ioctl.Len < (sizeof(pAC->PnmiStruct) + HeaderLength)) {
			Length = Ioctl.Len;
		} else {
			Length = sizeof(pAC->PnmiStruct) + HeaderLength;
		}
		if (NULL == (pMemBuf = kmalloc(Length, GFP_KERNEL))) {
			return -ENOMEM;
		}
		spin_lock_irqsave(&pAC->SlowPathLock, Flags);
		if(copy_from_user(pMemBuf, Ioctl.pData, Length)) {
			Err = -EFAULT;
			goto fault_gen;
		}
		if ((Ret = SkPnmiGenIoctl(pAC, pAC->IoBase, pMemBuf, &Length, 0)) < 0) {
			Err = -EFAULT;
			goto fault_gen;
		}
		if(copy_to_user(Ioctl.pData, pMemBuf, Length) ) {
			Err = -EFAULT;
			goto fault_gen;
		}
		Ioctl.Len = Length;
		if(copy_to_user(rq->ifr_data, &Ioctl, sizeof(SK_GE_IOCTL))) {
			Err = -EFAULT;
			goto fault_gen;
		}
fault_gen:
		spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
		kfree(pMemBuf); /* cleanup everything */
		break;
	case SK_IOCTL_DIAG:
		if (!capable(CAP_NET_ADMIN)) return -EPERM;
		if (Ioctl.Len < (sizeof(pAC->PnmiStruct) + HeaderLength)) {
			Length = Ioctl.Len;
		} else {
			Length = sizeof(pAC->PnmiStruct) + HeaderLength;
		}
		if (NULL == (pMemBuf = kmalloc(Length, GFP_KERNEL))) {
			return -ENOMEM;
		}
		if(copy_from_user(pMemBuf, Ioctl.pData, Length)) {
			Err = -EFAULT;
			goto fault_diag;
		}
		pdev = pAC->PciDev;
		Length = 3 * sizeof(SK_U32);  /* Error, Bus and Device */
		/* 
		** While coding this new IOCTL interface, only a few lines of code
		** are to to be added. Therefore no dedicated function has been 
		** added. If more functionality is added, a separate function 
		** should be used...
		*/
		* ((SK_U32 *)pMemBuf) = 0;
		* ((SK_U32 *)pMemBuf + 1) = pdev->bus->number;
		* ((SK_U32 *)pMemBuf + 2) = ParseDeviceNbrFromSlotName(pci_name(pdev));
		if(copy_to_user(Ioctl.pData, pMemBuf, Length) ) {
			Err = -EFAULT;
			goto fault_diag;
		}
		Ioctl.Len = Length;
		if(copy_to_user(rq->ifr_data, &Ioctl, sizeof(SK_GE_IOCTL))) {
			Err = -EFAULT;
			goto fault_diag;
		}
fault_diag:
		kfree(pMemBuf); /* cleanup everything */
		break;
	default:
		Err = -EOPNOTSUPP;
	}

	return(Err);

} /* SkGeIoctl */


/*****************************************************************************
 *
 * 	SkGeIocMib - handle a GetMib, SetMib- or PresetMib-ioctl message
 *
 * Description:
 *	This function reads/writes the MIB data using PNMI (Private Network
 *	Management Interface).
 *	The destination for the data must be provided with the
 *	ioctl call and is given to the driver in the form of
 *	a user space address.
 *	Copying from the user-provided data area into kernel messages
 *	and back is done by copy_from_user and copy_to_user calls in
 *	SkGeIoctl.
 *
 * Returns:
 *	returned size from PNMI call
 */
static int SkGeIocMib(
DEV_NET		*pNet,	/* pointer to the adapter context */
unsigned int	Size,	/* length of ioctl data */
int		mode)	/* flag for set/preset */
{
	SK_AC		*pAC = pNet->pAC;
	unsigned long	Flags;  /* for spin lock */

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeIocMib starts now...\n"));

	/* access MIB */
	spin_lock_irqsave(&pAC->SlowPathLock, Flags);
	switch(mode) {
	case SK_IOCTL_GETMIB:
		SkPnmiGetStruct(pAC, pAC->IoBase, &pAC->PnmiStruct, &Size,
			pNet->NetNr);
		break;
	case SK_IOCTL_PRESETMIB:
		SkPnmiPreSetStruct(pAC, pAC->IoBase, &pAC->PnmiStruct, &Size,
			pNet->NetNr);
		break;
	case SK_IOCTL_SETMIB:
		SkPnmiSetStruct(pAC, pAC->IoBase, &pAC->PnmiStruct, &Size,
			pNet->NetNr);
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("MIB data access succeeded\n"));
	return (Size);
} /* SkGeIocMib */
#endif

/*****************************************************************************
 *
 * 	GetConfiguration - read configuration information
 *
 * Description:
 *	This function reads per-adapter configuration information from
 *	the options provided on the command line.
 *
 * Returns:
 *	none
 */
static void GetConfiguration(
SK_AC	*pAC)	/* pointer to the adapter context structure */
{
SK_I32	Port;		/* preferred port */
SK_BOOL	AutoSet;
SK_BOOL DupSet;
int	LinkSpeed		= SK_LSPEED_AUTO;	/* Link speed */
int	AutoNeg			= 1;			/* autoneg off (0) or on (1) */
int	DuplexCap		= 0;			/* 0=both,1=full,2=half */
int	FlowCtrl		= SK_FLOW_MODE_SYM_OR_REM;	/* FlowControl  */
int	MSMode			= SK_MS_MODE_AUTO;	/* master/slave mode    */
int	IrqModMaskOffset	= 6;			/* all ints moderated=default */

SK_BOOL IsConTypeDefined	= SK_TRUE;
SK_BOOL IsLinkSpeedDefined	= SK_TRUE;
SK_BOOL IsFlowCtrlDefined	= SK_TRUE;
SK_BOOL IsRoleDefined		= SK_TRUE;
SK_BOOL IsModeDefined		= SK_TRUE;
/*
 *	The two parameters AutoNeg. and DuplexCap. map to one configuration
 *	parameter. The mapping is described by this table:
 *	DuplexCap ->	|	both	|	full	|	half	|
 *	AutoNeg		|		|		|		|
 *	-----------------------------------------------------------------
 *	Off		|    illegal	|	Full	|	Half	|
 *	-----------------------------------------------------------------
 *	On		|   AutoBoth	|   AutoFull	|   AutoHalf	|
 *	-----------------------------------------------------------------
 *	Sense		|   AutoSense	|   AutoSense	|   AutoSense	|
 */
int	Capabilities[3][3] =
		{ {                -1, SK_LMODE_FULL     , SK_LMODE_HALF     },
		  {SK_LMODE_AUTOBOTH , SK_LMODE_AUTOFULL , SK_LMODE_AUTOHALF },
		  {SK_LMODE_AUTOSENSE, SK_LMODE_AUTOSENSE, SK_LMODE_AUTOSENSE} };
SK_U32	IrqModMask[7][2] =
		{ { IRQ_MASK_RX_ONLY , Y2_DRIVER_IRQS  },
		  { IRQ_MASK_TX_ONLY , Y2_DRIVER_IRQS  },
		  { IRQ_MASK_SP_ONLY , Y2_SPECIAL_IRQS },
		  { IRQ_MASK_SP_RX   , Y2_IRQ_MASK     },
		  { IRQ_MASK_TX_RX   , Y2_DRIVER_IRQS  },
		  { IRQ_MASK_SP_TX   , Y2_IRQ_MASK     },
		  { IRQ_MASK_RX_TX_SP, Y2_IRQ_MASK     } };

#define DC_BOTH	0
#define DC_FULL 1
#define DC_HALF 2
#define AN_OFF	0
#define AN_ON	1
#define AN_SENS	2
#define M_CurrPort pAC->GIni.GP[Port]


	/*
	** Set the default values first for both ports!
	*/
	for (Port = 0; Port < SK_MAX_MACS; Port++) {
		M_CurrPort.PLinkModeConf = Capabilities[AN_ON][DC_BOTH];
		M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_SYM_OR_REM;
		M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
		M_CurrPort.PLinkSpeed    = SK_LSPEED_AUTO;
	}

	/*
	** Check merged parameter ConType. If it has not been used,
	** verify any other parameter (e.g. AutoNeg) and use default values. 
	**
	** Stating both ConType and other lowlevel link parameters is also
	** possible. If this is the case, the passed ConType-parameter is 
	** overwritten by the lowlevel link parameter.
	**
	** The following settings are used for a merged ConType-parameter:
	**
	** ConType   DupCap   AutoNeg   FlowCtrl      Role      Speed
	** -------   ------   -------   --------   ----------   -----
	**  Auto      Both      On      SymOrRem      Auto       Auto
	**  1000FD    Full      Off       None      <ignored>    1000
	**  100FD     Full      Off       None      <ignored>    100
	**  100HD     Half      Off       None      <ignored>    100
	**  10FD      Full      Off       None      <ignored>    10
	**  10HD      Half      Off       None      <ignored>    10
	** 
	** This ConType parameter is used for all ports of the adapter!
	*/
	if ( (ConType != NULL)                && 
	     (pAC->Index < SK_MAX_CARD_PARAM) &&
	     (ConType[pAC->Index] != NULL) ) {

		/* Check chipset family */
		if ((!pAC->ChipsetType) && 
			(strcmp(ConType[pAC->Index],"Auto")!=0) &&
			(strcmp(ConType[pAC->Index],"")!=0)) {
			/* Set the speed parameter back */
			printk("sk98lin: Illegal value \"%s\" " 
				"for ConType."
				" Using Auto.\n", 
				ConType[pAC->Index]);

			sprintf(ConType[pAC->Index], "Auto");	
		}

		if ((pAC->GIni.GICopperType != SK_TRUE) && 
			(strcmp(ConType[pAC->Index],"1000FD") != 0)) {
			/* Set the speed parameter back */
			printk("sk98lin: Illegal value \"%s\" " 
				"for ConType."
				" Using Auto.\n", 
				ConType[pAC->Index]);

			sprintf(ConType[pAC->Index], "Auto");
		}	

		if (strcmp(ConType[pAC->Index],"")==0) {
			IsConTypeDefined = SK_FALSE; /* No ConType defined */
		} else if (strcmp(ConType[pAC->Index],"Auto")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = Capabilities[AN_ON][DC_BOTH];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_SYM_OR_REM;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_AUTO;
		    }
		} else if (strcmp(ConType[pAC->Index],"1000FD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = Capabilities[AN_OFF][DC_FULL];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_1000MBPS;
		    }
		} else if (strcmp(ConType[pAC->Index],"100FD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = Capabilities[AN_OFF][DC_FULL];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_100MBPS;
		    }
		} else if (strcmp(ConType[pAC->Index],"100HD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = Capabilities[AN_OFF][DC_HALF];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_100MBPS;
		    }
		} else if (strcmp(ConType[pAC->Index],"10FD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = Capabilities[AN_OFF][DC_FULL];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_10MBPS;
		    }
		} else if (strcmp(ConType[pAC->Index],"10HD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = Capabilities[AN_OFF][DC_HALF];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_10MBPS;
		    }
		} else { 
		    printk("sk98lin: Illegal value \"%s\" for ConType\n", 
			ConType[pAC->Index]);
		    IsConTypeDefined = SK_FALSE; /* Wrong ConType defined */
		}
	} else {
	    IsConTypeDefined = SK_FALSE; /* No ConType defined */
	}

	/*
	** Parse any parameter settings for port A:
	** a) any LinkSpeed stated?
	*/
	if (Speed_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		Speed_A[pAC->Index] != NULL) {
		if (strcmp(Speed_A[pAC->Index],"")==0) {
		    IsLinkSpeedDefined = SK_FALSE;
		} else if (strcmp(Speed_A[pAC->Index],"Auto")==0) {
		    LinkSpeed = SK_LSPEED_AUTO;
		} else if (strcmp(Speed_A[pAC->Index],"10")==0) {
		    LinkSpeed = SK_LSPEED_10MBPS;
		} else if (strcmp(Speed_A[pAC->Index],"100")==0) {
		    LinkSpeed = SK_LSPEED_100MBPS;
		} else if (strcmp(Speed_A[pAC->Index],"1000")==0) {
#if 0 /* uboot */
		    if ((pAC->PciDev->vendor == 0x11ab ) &&
		    	(pAC->PciDev->device == 0x4350)) {
				LinkSpeed = SK_LSPEED_100MBPS;
				printk("sk98lin: Illegal value \"%s\" for Speed_A.\n"
					"Gigabit speed not possible with this chip revision!",
					Speed_A[pAC->Index]);
			} else
#endif
				{
				LinkSpeed = SK_LSPEED_1000MBPS;
		    }
		} else {
		    printk("sk98lin: Illegal value \"%s\" for Speed_A\n",
			Speed_A[pAC->Index]);
		    IsLinkSpeedDefined = SK_FALSE;
		}
	} else {
#if 0 /* uboot */
		if ((pAC->PciDev->vendor == 0x11ab ) && 
			(pAC->PciDev->device == 0x4350)) {
			/* Gigabit speed not supported
			 * Swith to speed 100
			 */
			LinkSpeed = SK_LSPEED_100MBPS;
		} else
#endif	
			{
			IsLinkSpeedDefined = SK_FALSE;
		}
	}

	/* 
	** Check speed parameter: 
	**    Only copper type adapter and GE V2 cards 
	*/
	if (((!pAC->ChipsetType) || (pAC->GIni.GICopperType != SK_TRUE)) &&
		((LinkSpeed != SK_LSPEED_AUTO) &&
		(LinkSpeed != SK_LSPEED_1000MBPS))) {
		printk("sk98lin: Illegal value for Speed_A. "
			"Not a copper card or GE V2 card\n    Using "
			"speed 1000\n");
		LinkSpeed = SK_LSPEED_1000MBPS;
	}
	
	/*	
	** Decide whether to set new config value if somethig valid has
	** been received.
	*/
	if (IsLinkSpeedDefined) {
		pAC->GIni.GP[0].PLinkSpeed = LinkSpeed;
	} 

	/* 
	** b) Any Autonegotiation and DuplexCapabilities set?
	**    Please note that both belong together...
	*/
	AutoNeg = AN_ON; /* tschilling: Default: Autonegotiation on! */
	AutoSet = SK_FALSE;
	if (AutoNeg_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		AutoNeg_A[pAC->Index] != NULL) {
		AutoSet = SK_TRUE;
		if (strcmp(AutoNeg_A[pAC->Index],"")==0) {
		    AutoSet = SK_FALSE;
		} else if (strcmp(AutoNeg_A[pAC->Index],"On")==0) {
		    AutoNeg = AN_ON;
		} else if (strcmp(AutoNeg_A[pAC->Index],"Off")==0) {
		    AutoNeg = AN_OFF;
		} else if (strcmp(AutoNeg_A[pAC->Index],"Sense")==0) {
		    AutoNeg = AN_SENS;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for AutoNeg_A\n",
			AutoNeg_A[pAC->Index]);
		}
	}

	DuplexCap = DC_BOTH;
	DupSet    = SK_FALSE;
	if (DupCap_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		DupCap_A[pAC->Index] != NULL) {
		DupSet = SK_TRUE;
		if (strcmp(DupCap_A[pAC->Index],"")==0) {
		    DupSet = SK_FALSE;
		} else if (strcmp(DupCap_A[pAC->Index],"Both")==0) {
		    DuplexCap = DC_BOTH;
		} else if (strcmp(DupCap_A[pAC->Index],"Full")==0) {
		    DuplexCap = DC_FULL;
		} else if (strcmp(DupCap_A[pAC->Index],"Half")==0) {
		    DuplexCap = DC_HALF;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for DupCap_A\n",
			DupCap_A[pAC->Index]);
		}
	}

	/* 
	** Check for illegal combinations 
	*/
	if ((LinkSpeed == SK_LSPEED_1000MBPS) &&
		((DuplexCap == SK_LMODE_STAT_AUTOHALF) ||
		(DuplexCap == SK_LMODE_STAT_HALF)) &&
		(pAC->ChipsetType)) {
		    printk("sk98lin: Half Duplex not possible with Gigabit speed!\n"
					"    Using Full Duplex.\n");
				DuplexCap = DC_FULL;
	}

	if ( AutoSet && AutoNeg==AN_SENS && DupSet) {
		printk("sk98lin, Port A: DuplexCapabilities"
			" ignored using Sense mode\n");
	}

	if (AutoSet && AutoNeg==AN_OFF && DupSet && DuplexCap==DC_BOTH){
		printk("sk98lin: Port A: Illegal combination"
			" of values AutoNeg. and DuplexCap.\n    Using "
			"Full Duplex\n");
		DuplexCap = DC_FULL;
	}

	if (AutoSet && AutoNeg==AN_OFF && !DupSet) {
		DuplexCap = DC_FULL;
	}
	
	if (!AutoSet && DupSet) {
		AutoNeg = AN_ON;
	}
	
	/* 
	** set the desired mode 
	*/
	if (AutoSet || DupSet) {
	    pAC->GIni.GP[0].PLinkModeConf = Capabilities[AutoNeg][DuplexCap];
	}
	
	/* 
	** c) Any Flowcontrol-parameter set?
	*/
	if (FlowCtrl_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		FlowCtrl_A[pAC->Index] != NULL) {
		if (strcmp(FlowCtrl_A[pAC->Index],"") == 0) {
		    IsFlowCtrlDefined = SK_FALSE;
		} else if (strcmp(FlowCtrl_A[pAC->Index],"SymOrRem") == 0) {
		    FlowCtrl = SK_FLOW_MODE_SYM_OR_REM;
		} else if (strcmp(FlowCtrl_A[pAC->Index],"Sym")==0) {
		    FlowCtrl = SK_FLOW_MODE_SYMMETRIC;
		} else if (strcmp(FlowCtrl_A[pAC->Index],"LocSend")==0) {
		    FlowCtrl = SK_FLOW_MODE_LOC_SEND;
		} else if (strcmp(FlowCtrl_A[pAC->Index],"None")==0) {
		    FlowCtrl = SK_FLOW_MODE_NONE;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for FlowCtrl_A\n",
			FlowCtrl_A[pAC->Index]);
		    IsFlowCtrlDefined = SK_FALSE;
		}
	} else {
	   IsFlowCtrlDefined = SK_FALSE;
	}

	if (IsFlowCtrlDefined) {
	    if ((AutoNeg == AN_OFF) && (FlowCtrl != SK_FLOW_MODE_NONE)) {
		printk("sk98lin: Port A: FlowControl"
			" impossible without AutoNegotiation,"
			" disabled\n");
		FlowCtrl = SK_FLOW_MODE_NONE;
	    }
	    pAC->GIni.GP[0].PFlowCtrlMode = FlowCtrl;
	}

	/*
	** d) What is with the RoleParameter?
	*/
	if (Role_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		Role_A[pAC->Index] != NULL) {
		if (strcmp(Role_A[pAC->Index],"")==0) {
		   IsRoleDefined = SK_FALSE;
		} else if (strcmp(Role_A[pAC->Index],"Auto")==0) {
		    MSMode = SK_MS_MODE_AUTO;
		} else if (strcmp(Role_A[pAC->Index],"Master")==0) {
		    MSMode = SK_MS_MODE_MASTER;
		} else if (strcmp(Role_A[pAC->Index],"Slave")==0) {
		    MSMode = SK_MS_MODE_SLAVE;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for Role_A\n",
			Role_A[pAC->Index]);
		    IsRoleDefined = SK_FALSE;
		}
	} else {
	   IsRoleDefined = SK_FALSE;
	}

	if (IsRoleDefined == SK_TRUE) {
	    pAC->GIni.GP[0].PMSMode = MSMode;
	}
	

	
	/* 
	** Parse any parameter settings for port B:
	** a) any LinkSpeed stated?
	*/
	IsConTypeDefined   = SK_TRUE;
	IsLinkSpeedDefined = SK_TRUE;
	IsFlowCtrlDefined  = SK_TRUE;
	IsModeDefined      = SK_TRUE;

	if (Speed_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		Speed_B[pAC->Index] != NULL) {
		if (strcmp(Speed_B[pAC->Index],"")==0) {
		    IsLinkSpeedDefined = SK_FALSE;
		} else if (strcmp(Speed_B[pAC->Index],"Auto")==0) {
		    LinkSpeed = SK_LSPEED_AUTO;
		} else if (strcmp(Speed_B[pAC->Index],"10")==0) {
		    LinkSpeed = SK_LSPEED_10MBPS;
		} else if (strcmp(Speed_B[pAC->Index],"100")==0) {
		    LinkSpeed = SK_LSPEED_100MBPS;
		} else if (strcmp(Speed_B[pAC->Index],"1000")==0) {
		    LinkSpeed = SK_LSPEED_1000MBPS;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for Speed_B\n",
			Speed_B[pAC->Index]);
		    IsLinkSpeedDefined = SK_FALSE;
		}
	} else {
	    IsLinkSpeedDefined = SK_FALSE;
	}

	/* 
	** Check speed parameter:
	**    Only copper type adapter and GE V2 cards 
	*/
	if (((!pAC->ChipsetType) || (pAC->GIni.GICopperType != SK_TRUE)) &&
		((LinkSpeed != SK_LSPEED_AUTO) &&
		(LinkSpeed != SK_LSPEED_1000MBPS))) {
		printk("sk98lin: Illegal value for Speed_B. "
			"Not a copper card or GE V2 card\n    Using "
			"speed 1000\n");
		LinkSpeed = SK_LSPEED_1000MBPS;
	}

	/*      
	** Decide whether to set new config value if somethig valid has
	** been received.
	*/
	if (IsLinkSpeedDefined) {
	    pAC->GIni.GP[1].PLinkSpeed = LinkSpeed;
	}

	/* 
	** b) Any Autonegotiation and DuplexCapabilities set?
	**    Please note that both belong together...
	*/
	AutoNeg = AN_SENS; /* default: do auto Sense */
	AutoSet = SK_FALSE;
	if (AutoNeg_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		AutoNeg_B[pAC->Index] != NULL) {
		AutoSet = SK_TRUE;
		if (strcmp(AutoNeg_B[pAC->Index],"")==0) {
		    AutoSet = SK_FALSE;
		} else if (strcmp(AutoNeg_B[pAC->Index],"On")==0) {
		    AutoNeg = AN_ON;
		} else if (strcmp(AutoNeg_B[pAC->Index],"Off")==0) {
		    AutoNeg = AN_OFF;
		} else if (strcmp(AutoNeg_B[pAC->Index],"Sense")==0) {
		    AutoNeg = AN_SENS;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for AutoNeg_B\n",
			AutoNeg_B[pAC->Index]);
		}
	}

	DuplexCap = DC_BOTH;
	DupSet    = SK_FALSE;
	if (DupCap_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		DupCap_B[pAC->Index] != NULL) {
		DupSet = SK_TRUE;
		if (strcmp(DupCap_B[pAC->Index],"")==0) {
		    DupSet = SK_FALSE;
		} else if (strcmp(DupCap_B[pAC->Index],"Both")==0) {
		    DuplexCap = DC_BOTH;
		} else if (strcmp(DupCap_B[pAC->Index],"Full")==0) {
		    DuplexCap = DC_FULL;
		} else if (strcmp(DupCap_B[pAC->Index],"Half")==0) {
		    DuplexCap = DC_HALF;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for DupCap_B\n",
			DupCap_B[pAC->Index]);
		}
	}

	
	/* 
	** Check for illegal combinations 
	*/
	if ((LinkSpeed == SK_LSPEED_1000MBPS) &&
		((DuplexCap == SK_LMODE_STAT_AUTOHALF) ||
		(DuplexCap == SK_LMODE_STAT_HALF)) &&
		(pAC->ChipsetType)) {
		    printk("sk98lin: Half Duplex not possible with Gigabit speed!\n"
					"    Using Full Duplex.\n");
				DuplexCap = DC_FULL;
	}

	if (AutoSet && AutoNeg==AN_SENS && DupSet) {
		printk("sk98lin, Port B: DuplexCapabilities"
			" ignored using Sense mode\n");
	}

	if (AutoSet && AutoNeg==AN_OFF && DupSet && DuplexCap==DC_BOTH){
		printk("sk98lin: Port B: Illegal combination"
			" of values AutoNeg. and DuplexCap.\n    Using "
			"Full Duplex\n");
		DuplexCap = DC_FULL;
	}

	if (AutoSet && AutoNeg==AN_OFF && !DupSet) {
		DuplexCap = DC_FULL;
	}
	
	if (!AutoSet && DupSet) {
		AutoNeg = AN_ON;
	}

	/* 
	** set the desired mode 
	*/
	if (AutoSet || DupSet) {
	    pAC->GIni.GP[1].PLinkModeConf = Capabilities[AutoNeg][DuplexCap];
	}

	/*
	** c) Any FlowCtrl parameter set?
	*/
	if (FlowCtrl_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		FlowCtrl_B[pAC->Index] != NULL) {
		if (strcmp(FlowCtrl_B[pAC->Index],"") == 0) {
		    IsFlowCtrlDefined = SK_FALSE;
		} else if (strcmp(FlowCtrl_B[pAC->Index],"SymOrRem") == 0) {
		    FlowCtrl = SK_FLOW_MODE_SYM_OR_REM;
		} else if (strcmp(FlowCtrl_B[pAC->Index],"Sym")==0) {
		    FlowCtrl = SK_FLOW_MODE_SYMMETRIC;
		} else if (strcmp(FlowCtrl_B[pAC->Index],"LocSend")==0) {
		    FlowCtrl = SK_FLOW_MODE_LOC_SEND;
		} else if (strcmp(FlowCtrl_B[pAC->Index],"None")==0) {
		    FlowCtrl = SK_FLOW_MODE_NONE;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for FlowCtrl_B\n",
			FlowCtrl_B[pAC->Index]);
		    IsFlowCtrlDefined = SK_FALSE;
		}
	} else {
		IsFlowCtrlDefined = SK_FALSE;
	}

	if (IsFlowCtrlDefined) {
	    if ((AutoNeg == AN_OFF) && (FlowCtrl != SK_FLOW_MODE_NONE)) {
		printk("sk98lin: Port B: FlowControl"
			" impossible without AutoNegotiation,"
			" disabled\n");
		FlowCtrl = SK_FLOW_MODE_NONE;
	    }
	    pAC->GIni.GP[1].PFlowCtrlMode = FlowCtrl;
	}

	/*
	** d) What is the RoleParameter?
	*/
	if (Role_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		Role_B[pAC->Index] != NULL) {
		if (strcmp(Role_B[pAC->Index],"")==0) {
		    IsRoleDefined = SK_FALSE;
		} else if (strcmp(Role_B[pAC->Index],"Auto")==0) {
		    MSMode = SK_MS_MODE_AUTO;
		} else if (strcmp(Role_B[pAC->Index],"Master")==0) {
		    MSMode = SK_MS_MODE_MASTER;
		} else if (strcmp(Role_B[pAC->Index],"Slave")==0) {
		    MSMode = SK_MS_MODE_SLAVE;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for Role_B\n",
			Role_B[pAC->Index]);
		    IsRoleDefined = SK_FALSE;
		}
	} else {
	    IsRoleDefined = SK_FALSE;
	}

	if (IsRoleDefined) {
	    pAC->GIni.GP[1].PMSMode = MSMode;
	}
	
	/*
	** Evaluate settings for both ports
	*/
	pAC->ActivePort = 0;
	if (PrefPort != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		PrefPort[pAC->Index] != NULL) {
		if (strcmp(PrefPort[pAC->Index],"") == 0) { /* Auto */
			pAC->ActivePort             =  0;
			pAC->Rlmt.Net[0].Preference = -1; /* auto */
			pAC->Rlmt.Net[0].PrefPort   =  0;
		} else if (strcmp(PrefPort[pAC->Index],"A") == 0) {
			/*
			** do not set ActivePort here, thus a port
			** switch is issued after net up.
			*/
			Port                        = 0;
			pAC->Rlmt.Net[0].Preference = Port;
			pAC->Rlmt.Net[0].PrefPort   = Port;
		} else if (strcmp(PrefPort[pAC->Index],"B") == 0) {
			/*
			** do not set ActivePort here, thus a port
			** switch is issued after net up.
			*/
			if (pAC->GIni.GIMacsFound == 1) {
				printk("sk98lin: Illegal value \"B\" for PrefPort.\n"
					"      Port B not available on single port adapters.\n");

				pAC->ActivePort             =  0;
				pAC->Rlmt.Net[0].Preference = -1; /* auto */
				pAC->Rlmt.Net[0].PrefPort   =  0;
			} else {
				Port                        = 1;
				pAC->Rlmt.Net[0].Preference = Port;
				pAC->Rlmt.Net[0].PrefPort   = Port;
			}
		} else {
		    printk("sk98lin: Illegal value \"%s\" for PrefPort\n",
			PrefPort[pAC->Index]);
		}
	}

	pAC->RlmtNets = 1;
	pAC->RlmtMode = 0;

	if (RlmtMode != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		RlmtMode[pAC->Index] != NULL) {
		if (strcmp(RlmtMode[pAC->Index], "") == 0) {
			if (pAC->GIni.GIMacsFound == 2) {
				pAC->RlmtMode = SK_RLMT_CHECK_LINK;
				pAC->RlmtNets = 2;
			}
		} else if (strcmp(RlmtMode[pAC->Index], "CheckLinkState") == 0) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK;
		} else if (strcmp(RlmtMode[pAC->Index], "CheckLocalPort") == 0) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK |
					SK_RLMT_CHECK_LOC_LINK;
		} else if (strcmp(RlmtMode[pAC->Index], "CheckSeg") == 0) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK     |
					SK_RLMT_CHECK_LOC_LINK |
					SK_RLMT_CHECK_SEG;
		} else if ((strcmp(RlmtMode[pAC->Index], "DualNet") == 0) &&
			(pAC->GIni.GIMacsFound == 2)) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK;
			pAC->RlmtNets = 2;
		} else {
		    printk("sk98lin: Illegal value \"%s\" for"
			" RlmtMode, using default\n", 
			RlmtMode[pAC->Index]);
			pAC->RlmtMode = 0;
		}
	} else {
		if (pAC->GIni.GIMacsFound == 2) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK;
			pAC->RlmtNets = 2;
		}
	}

#ifdef SK_YUKON2
	/*
	** use dualnet config per default
	*
	pAC->RlmtMode = SK_RLMT_CHECK_LINK;
	pAC->RlmtNets = 2;
	*/
#endif


	/*
	** Check the LowLatance parameters
	*/
	pAC->LowLatency = SK_FALSE;
	if (LowLatency[pAC->Index] != NULL) {
		if (strcmp(LowLatency[pAC->Index], "On") == 0) {
			pAC->LowLatency = SK_TRUE;
		}
	}


	/*
	** Check the interrupt moderation parameters
	*/
	pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_NONE;
	if (Moderation[pAC->Index] != NULL) {
		if (strcmp(Moderation[pAC->Index], "") == 0) {
			pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_NONE;
		} else if (strcmp(Moderation[pAC->Index], "Static") == 0) {
			pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_STATIC;
		} else if (strcmp(Moderation[pAC->Index], "Dynamic") == 0) {
			pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_DYNAMIC;
		} else if (strcmp(Moderation[pAC->Index], "None") == 0) {
			pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_NONE;
		} else {
	   		printk("sk98lin: Illegal value \"%s\" for Moderation.\n"
				"      Disable interrupt moderation.\n",
				Moderation[pAC->Index]);
		}
	} else {
/* Set interrupt moderation if wished */
#ifdef CONFIG_SK98LIN_STATINT
		pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_STATIC;
#endif
	}

	if (ModerationMask[pAC->Index] != NULL) {
		if (strcmp(ModerationMask[pAC->Index], "Rx") == 0) {
			IrqModMaskOffset = 0;
		} else if (strcmp(ModerationMask[pAC->Index], "Tx") == 0) {
			IrqModMaskOffset = 1;
		} else if (strcmp(ModerationMask[pAC->Index], "Sp") == 0) {
			IrqModMaskOffset = 2;
		} else if (strcmp(ModerationMask[pAC->Index], "RxSp") == 0) {
			IrqModMaskOffset = 3;
		} else if (strcmp(ModerationMask[pAC->Index], "SpRx") == 0) {
			IrqModMaskOffset = 3;
		} else if (strcmp(ModerationMask[pAC->Index], "RxTx") == 0) {
			IrqModMaskOffset = 4;
		} else if (strcmp(ModerationMask[pAC->Index], "TxRx") == 0) {
			IrqModMaskOffset = 4;
		} else if (strcmp(ModerationMask[pAC->Index], "TxSp") == 0) {
			IrqModMaskOffset = 5;
		} else if (strcmp(ModerationMask[pAC->Index], "SpTx") == 0) {
			IrqModMaskOffset = 5;
		} else { /* some rubbish stated */
			// IrqModMaskOffset = 6; ->has been initialized
			// already at the begin of this function...
		}
	}
	if (!CHIP_ID_YUKON_2(pAC)) {
		pAC->DynIrqModInfo.MaskIrqModeration = IrqModMask[IrqModMaskOffset][0];
	} else {
		pAC->DynIrqModInfo.MaskIrqModeration = IrqModMask[IrqModMaskOffset][1];
	}

	if (!CHIP_ID_YUKON_2(pAC)) {
		pAC->DynIrqModInfo.MaxModIntsPerSec = C_INTS_PER_SEC_DEFAULT;
	} else {
		pAC->DynIrqModInfo.MaxModIntsPerSec = C_Y2_INTS_PER_SEC_DEFAULT;
	}
	if (IntsPerSec[pAC->Index] != 0) {
		if ((IntsPerSec[pAC->Index]< C_INT_MOD_IPS_LOWER_RANGE) || 
			(IntsPerSec[pAC->Index] > C_INT_MOD_IPS_UPPER_RANGE)) {
	   		printk("sk98lin: Illegal value \"%d\" for IntsPerSec. (Range: %d - %d)\n"
				"      Using default value of %i.\n", 
				IntsPerSec[pAC->Index],
				C_INT_MOD_IPS_LOWER_RANGE,
				C_INT_MOD_IPS_UPPER_RANGE,
				pAC->DynIrqModInfo.MaxModIntsPerSec);
		} else {
			pAC->DynIrqModInfo.MaxModIntsPerSec = IntsPerSec[pAC->Index];
		}
	} 

	/*
	** Evaluate upper and lower moderation threshold
	*/
	pAC->DynIrqModInfo.MaxModIntsPerSecUpperLimit =
		pAC->DynIrqModInfo.MaxModIntsPerSec +
		(pAC->DynIrqModInfo.MaxModIntsPerSec / 5);

	pAC->DynIrqModInfo.MaxModIntsPerSecLowerLimit =
		pAC->DynIrqModInfo.MaxModIntsPerSec -
		(pAC->DynIrqModInfo.MaxModIntsPerSec / 5);

	pAC->DynIrqModInfo.DynIrqModSampleInterval = 
		SK_DRV_MODERATION_TIMER_LENGTH;

} /* GetConfiguration */

#if 0 /* uboot */
/*****************************************************************************
 *
 * 	ProductStr - return a adapter identification string from vpd
 *
 * Description:
 *	This function reads the product name string from the vpd area
 *	and puts it the field pAC->DeviceString.
 *
 * Returns: N/A
 */
static void ProductStr(SK_AC *pAC)
{
	char Default[] = "Generic Marvell Yukon chipset Ethernet device";
	char Key[] = VPD_NAME; /* VPD productname key */
	int StrLen = 80;       /* stringlen           */
	unsigned long Flags;

	spin_lock_irqsave(&pAC->SlowPathLock, Flags);
	if (VpdRead(pAC, pAC->IoBase, Key, pAC->DeviceStr, &StrLen)) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ERROR,
			("Error reading VPD data: %d\n", ReturnCode));
		strcpy(pAC->DeviceStr, Default);
	}
	spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
} /* ProductStr */

#endif
/****************************************************************************/
/* functions for common modules *********************************************/
/****************************************************************************/


/*****************************************************************************
 *
 *	SkDrvAllocRlmtMbuf - allocate an RLMT mbuf
 *
 * Description:
 *	This routine returns an RLMT mbuf or NULL. The RLMT Mbuf structure
 *	is embedded into a socket buff data area.
 *
 * Context:
 *	runtime
 *
 * Returns:
 *	NULL or pointer to Mbuf.
 */
SK_MBUF *SkDrvAllocRlmtMbuf(
SK_AC		*pAC,		/* pointer to adapter context */
SK_IOC		IoC,		/* the IO-context */
unsigned	BufferSize)	/* size of the requested buffer */
{
SK_MBUF		*pRlmtMbuf;	/* pointer to a new rlmt-mbuf structure */
struct sk_buff	*pMsgBlock;	/* pointer to a new message block */

	pMsgBlock = alloc_skb(BufferSize + sizeof(SK_MBUF), GFP_ATOMIC);
	if (pMsgBlock == NULL) {
		return (NULL);
	}
	pRlmtMbuf = (SK_MBUF*) pMsgBlock->data;
	skb_reserve(pMsgBlock, sizeof(SK_MBUF));
	pRlmtMbuf->pNext = NULL;
	pRlmtMbuf->pOs = pMsgBlock;
	pRlmtMbuf->pData = pMsgBlock->data;	/* Data buffer. */
	pRlmtMbuf->Size = BufferSize;		/* Data buffer size. */
	pRlmtMbuf->Length = 0;		/* Length of packet (<= Size). */
	return (pRlmtMbuf);

} /* SkDrvAllocRlmtMbuf */


/*****************************************************************************
 *
 *	SkDrvFreeRlmtMbuf - free an RLMT mbuf
 *
 * Description:
 *	This routine frees one or more RLMT mbuf(s).
 *
 * Context:
 *	runtime
 *
 * Returns:
 *	Nothing
 */
void  SkDrvFreeRlmtMbuf(
SK_AC		*pAC,		/* pointer to adapter context */
SK_IOC		IoC,		/* the IO-context */
SK_MBUF		*pMbuf)		/* size of the requested buffer */
{
SK_MBUF		*pFreeMbuf;
SK_MBUF		*pNextMbuf;

	pFreeMbuf = pMbuf;
	do {
		pNextMbuf = pFreeMbuf->pNext;
		DEV_KFREE_SKB_ANY(pFreeMbuf->pOs);
		pFreeMbuf = pNextMbuf;
	} while ( pFreeMbuf != NULL );
} /* SkDrvFreeRlmtMbuf */


/*****************************************************************************
 *
 *	SkOsGetTime - provide a time value
 *
 * Description:
 *	This routine provides a time value. The unit is 1/HZ (defined by Linux).
 *	It is not used for absolute time, but only for time differences.
 *
 *
 * Returns:
 *	Time value
 */
SK_U64 SkOsGetTime(SK_AC *pAC)
{
#if 0 /* uboot */
	SK_U64	PrivateJiffies;

	SkOsGetTimeCurrent(pAC, &PrivateJiffies);

	return PrivateJiffies;
#else
	return (get_timer(0)/(CONFIG_SYS_TCLK/1000));
#endif
} /* SkOsGetTime */


/*****************************************************************************
 *
 *	SkPciReadCfgDWord - read a 32 bit value from pci config space
 *
 * Description:
 *	This routine reads a 32 bit value from the pci configuration
 *	space.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciReadCfgDWord(
SK_AC *pAC,		/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U32 *pVal)		/* pointer to store the read value */
{
	pci_read_config_dword(pAC->PciDev, PciAddr, pVal);
	return(0);
} /* SkPciReadCfgDWord */


/*****************************************************************************
 *
 *	SkPciReadCfgWord - read a 16 bit value from pci config space
 *
 * Description:
 *	This routine reads a 16 bit value from the pci configuration
 *	space.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciReadCfgWord(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U16 *pVal)		/* pointer to store the read value */
{
	pci_read_config_word(pAC->PciDev, PciAddr, pVal);
	return(0);
} /* SkPciReadCfgWord */


/*****************************************************************************
 *
 *	SkPciReadCfgByte - read a 8 bit value from pci config space
 *
 * Description:
 *	This routine reads a 8 bit value from the pci configuration
 *	space.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciReadCfgByte(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U8 *pVal)		/* pointer to store the read value */
{
	pci_read_config_byte(pAC->PciDev, PciAddr, pVal);
	return(0);
} /* SkPciReadCfgByte */


/*****************************************************************************
 *
 *	SkPciWriteCfgDWord - write a 32 bit value to pci config space
 *
 * Description:
 *	This routine writes a 32 bit value to the pci configuration
 *	space.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciWriteCfgDWord(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U32 Val)		/* pointer to store the read value */
{
	pci_write_config_dword(pAC->PciDev, PciAddr, Val);
	return(0);
} /* SkPciWriteCfgDWord */


/*****************************************************************************
 *
 *	SkPciWriteCfgWord - write a 16 bit value to pci config space
 *
 * Description:
 *	This routine writes a 16 bit value to the pci configuration
 *	space. The flag PciConfigUp indicates whether the config space
 *	is accesible or must be set up first.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciWriteCfgWord(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U16 Val)		/* pointer to store the read value */
{
	pci_write_config_word(pAC->PciDev, PciAddr, Val);
	return(0);
} /* SkPciWriteCfgWord */


/*****************************************************************************
 *
 *	SkPciWriteCfgWord - write a 8 bit value to pci config space
 *
 * Description:
 *	This routine writes a 8 bit value to the pci configuration
 *	space. The flag PciConfigUp indicates whether the config space
 *	is accesible or must be set up first.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciWriteCfgByte(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U8 Val)		/* pointer to store the read value */
{
	pci_write_config_byte(pAC->PciDev, PciAddr, Val);
	return(0);
} /* SkPciWriteCfgByte */


/*****************************************************************************
 *
 *	SkDrvEvent - handle driver events
 *
 * Description:
 *	This function handles events from all modules directed to the driver
 *
 * Context:
 *	Is called under protection of slow path lock.
 *
 * Returns:
 *	0 if everything ok
 *	< 0  on error
 *	
 */
int SkDrvEvent(
SK_AC     *pAC,    /* pointer to adapter context */
SK_IOC     IoC,    /* IO control context         */
SK_U32     Event,  /* event-id                   */
SK_EVPARA  Param)  /* event-parameter            */
{
	SK_MBUF         *pRlmtMbuf;   /* pointer to a rlmt-mbuf structure   */
	struct sk_buff  *pMsg;        /* pointer to a message block         */
	SK_BOOL          DualNet;
	SK_U32           Reason;
	unsigned long    Flags;
	unsigned long    InitFlags;
	int              FromPort;    /* the port from which we switch away */
	int              ToPort;      /* the port we switch to              */
	int              Stat;
	DEV_NET 	*pNet = NULL;
#ifdef CONFIG_SK98LIN_NAPI
	int              WorkToDo = 1; /* min(*budget, dev->quota); */
	int              WorkDone = 0;
#endif

	switch (Event) {
	case SK_DRV_PORT_FAIL:
		FromPort = Param.Para32[0];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("PORT FAIL EVENT, Port: %d\n", FromPort));
		if (FromPort == 0) {
			printk("%s: Port A failed.\n", pAC->dev[0]->name);
		} else {
			printk("%s: Port B failed.\n", pAC->dev[1]->name);
		}
		break;
	case SK_DRV_PORT_RESET:
		FromPort = Param.Para32[0];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("PORT RESET EVENT, Port: %d ", FromPort));
		SkLocalEventQueue64(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					FromPort, SK_FALSE);
		spin_lock_irqsave(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStop(pAC, IoC, FromPort, SK_STOP_ALL, SK_HARD_RST);
		} else {
			SkGeStopPort(pAC, IoC, FromPort, SK_STOP_ALL, SK_HARD_RST);
		}
#if 0 /* uboot */
		pAC->dev[Param.Para32[0]]->flags &= ~IFF_RUNNING;
#endif
		spin_unlock_irqrestore(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);
		
		if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
			WorkToDo = 1;
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE, &WorkDone, WorkToDo);
#else
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE);
#endif
			ClearTxRing(pAC, &pAC->TxPort[FromPort][TX_PRIO_LOW]);
		}
		spin_lock_irqsave(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);

#ifdef USE_TIST_FOR_RESET
                if (pAC->GIni.GIYukon2) {
#ifdef Y2_RECOVERY
			/* for Yukon II we want to have tist enabled all the time */
			if (!SK_ADAPTER_WAITING_FOR_TIST(pAC)) {
				Y2_ENABLE_TIST(pAC->IoBase);
			}
#else
			/* make sure that we do not accept any status LEs from now on */
			if (SK_ADAPTER_WAITING_FOR_TIST(pAC)) {
#endif
				/* port already waiting for tist */
				SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DUMP,
					("Port %c is now waiting for specific Tist\n",
					'A' +  FromPort));
				SK_SET_WAIT_BIT_FOR_PORT(
					pAC,
					SK_PSTATE_WAITING_FOR_SPECIFIC_TIST,
					FromPort);
				/* get current timestamp */
				Y2_GET_TIST_LOW_VAL(pAC->IoBase, &pAC->MinTistLo);
				pAC->MinTistHi = pAC->GIni.GITimeStampCnt;
#ifndef Y2_RECOVERY
			} else {
				/* nobody is waiting yet */
				SK_SET_WAIT_BIT_FOR_PORT(
					pAC,
					SK_PSTATE_WAITING_FOR_ANY_TIST,
					FromPort);
				SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DUMP,
					("Port %c is now waiting for any Tist (0x%X)\n",
					'A' +  FromPort, pAC->AdapterResetState));
				/* start tist */
				Y2_ENABLE_TIST(pAC-IoBase);
			}
#endif
		}
#endif

#ifdef Y2_LE_CHECK
		/* mark entries invalid */
		pAC->LastPort = 3;
		pAC->LastOpc = 0xFF;
#endif
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStart(pAC, IoC, FromPort);
		} else {
			/* tschilling: Handling of return value inserted. */
			if (SkGeInitPort(pAC, IoC, FromPort)) {
				if (FromPort == 0) {
					printk("%s: SkGeInitPort A failed.\n", pAC->dev[0]->name);
				} else {
					printk("%s: SkGeInitPort B failed.\n", pAC->dev[1]->name);
				}
			}
			SkAddrMcUpdate(pAC,IoC, FromPort);
			PortReInitBmu(pAC, FromPort);
			SkGePollTxD(pAC, IoC, FromPort, SK_TRUE);
			CLEAR_AND_START_RX(FromPort);
		}
		spin_unlock_irqrestore(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);
		break;
	case SK_DRV_NET_UP:
		spin_lock_irqsave(&pAC->InitLock, InitFlags);
		FromPort = Param.Para32[0];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("NET UP EVENT, Port: %d ", FromPort));
		SkAddrMcUpdate(pAC,IoC, FromPort); /* Mac update */
		if (DoPrintInterfaceChange) {
			printk("%s: network connection up using port %c\n",
				pAC->dev[FromPort]->name, 'A'+FromPort);

			/* tschilling: Values changed according to LinkSpeedUsed. */
			Stat = pAC->GIni.GP[FromPort].PLinkSpeedUsed;
			if (Stat == SK_LSPEED_STAT_10MBPS) {
				printk("    speed:           10\n");
			} else if (Stat == SK_LSPEED_STAT_100MBPS) {
				printk("    speed:           100\n");
			} else if (Stat == SK_LSPEED_STAT_1000MBPS) {
				printk("    speed:           1000\n");
			} else {
				printk("    speed:           unknown\n");
			}

			Stat = pAC->GIni.GP[FromPort].PLinkModeStatus;
			if ((Stat == SK_LMODE_STAT_AUTOHALF) ||
			    (Stat == SK_LMODE_STAT_AUTOFULL)) {
				printk("    autonegotiation: yes\n");
			} else {
				printk("    autonegotiation: no\n");
			}

			if ((Stat == SK_LMODE_STAT_AUTOHALF) ||
			    (Stat == SK_LMODE_STAT_HALF)) {
				printk("    duplex mode:     half\n");
			} else {
				printk("    duplex mode:     full\n");
			}

			Stat = pAC->GIni.GP[FromPort].PFlowCtrlStatus;
			if (Stat == SK_FLOW_STAT_REM_SEND ) {
				printk("    flowctrl:        remote send\n");
			} else if (Stat == SK_FLOW_STAT_LOC_SEND ) {
				printk("    flowctrl:        local send\n");
			} else if (Stat == SK_FLOW_STAT_SYMMETRIC ) {
				printk("    flowctrl:        symmetric\n");
			} else {
				printk("    flowctrl:        none\n");
			}
		
			/* tschilling: Check against CopperType now. */
			if ((pAC->GIni.GICopperType == SK_TRUE) &&
				(pAC->GIni.GP[FromPort].PLinkSpeedUsed ==
				SK_LSPEED_STAT_1000MBPS)) {
				Stat = pAC->GIni.GP[FromPort].PMSStatus;
				if (Stat == SK_MS_STAT_MASTER ) {
					printk("    role:            master\n");
				} else if (Stat == SK_MS_STAT_SLAVE ) {
					printk("    role:            slave\n");
				} else {
					printk("    role:            ???\n");
				}
			}

			/* Display interrupt moderation informations */
			if (pAC->DynIrqModInfo.IntModTypeSelect == C_INT_MOD_STATIC) {
				printk("    irq moderation:  static (%d ints/sec)\n",
					pAC->DynIrqModInfo.MaxModIntsPerSec);
			} else if (pAC->DynIrqModInfo.IntModTypeSelect == C_INT_MOD_DYNAMIC) {
				printk("    irq moderation:  dynamic (%d ints/sec)\n",
					pAC->DynIrqModInfo.MaxModIntsPerSec);
			} else {
				printk("    irq moderation:  disabled\n");
			}
	
#ifdef NETIF_F_TSO
			if (CHIP_ID_YUKON_2(pAC)) {
				if (pAC->dev[FromPort]->features & NETIF_F_TSO) {
					printk("    tcp offload:     enabled\n");
				} else {
					printk("    tcp offload:     disabled\n");
				}
			}
#endif

#if 0 /* uboot */
			if (pAC->dev[FromPort]->features & NETIF_F_SG) {
				printk("    scatter-gather:  enabled\n");
			} else {
				printk("    scatter-gather:  disabled\n");
			}

			if (pAC->dev[FromPort]->features & NETIF_F_IP_CSUM) {
				printk("    tx-checksum:     enabled\n");
			} else {
				printk("    tx-checksum:     disabled\n");
			}

			if (pAC->RxPort[FromPort].UseRxCsum) {
				printk("    rx-checksum:     enabled\n");
			} else {
				printk("    rx-checksum:     disabled\n");
			}
#ifdef CONFIG_SK98LIN_NAPI
			printk("    rx-polling:      enabled\n");
#endif
#endif
			if (pAC->LowLatency) {
				printk("    low latency:     enabled\n");
			}
		} else {
			DoPrintInterfaceChange = SK_TRUE;
		}
	
		if ((FromPort != pAC->ActivePort)&&(pAC->RlmtNets == 1)) {
			SkLocalEventQueue(pAC, SKGE_DRV, SK_DRV_SWITCH_INTERN,
						pAC->ActivePort, FromPort, SK_FALSE);
		}

		/* Inform the world that link protocol is up. */
		netif_wake_queue(pAC->dev[FromPort]);
#if 0 //u-boot
		netif_carrier_on(pAC->dev[FromPort]);
		pAC->dev[FromPort]->flags |= IFF_RUNNING;
#endif
		spin_unlock_irqrestore(&pAC->InitLock, InitFlags);
		break;
	case SK_DRV_NET_DOWN:	
		Reason   = Param.Para32[0];
		FromPort = Param.Para32[1];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("NET DOWN EVENT "));

#if 0 //u-boot
		/* Stop queue and carrier */
		netif_stop_queue(pAC->dev[FromPort]);
		netif_carrier_off(pAC->dev[FromPort]);
#endif

		/* Print link change */
		if (DoPrintInterfaceChange) {
#if 0 /* uboot */
			if (pAC->dev[FromPort]->flags & IFF_RUNNING) {
				printk("%s: network connection down\n", 
					pAC->dev[FromPort]->name);
			}
#endif
		} else {
			DoPrintInterfaceChange = SK_TRUE;
		}
#if 0 /* uboot */
		pAC->dev[FromPort]->flags &= ~IFF_RUNNING;
#endif
		break;
	case SK_DRV_SWITCH_HARD:   /* FALL THRU */
	case SK_DRV_SWITCH_SOFT:   /* FALL THRU */
	case SK_DRV_SWITCH_INTERN: 
		FromPort = Param.Para32[0];
		ToPort   = Param.Para32[1];
		printk("%s: switching from port %c to port %c\n",
			pAC->dev[0]->name, 'A'+FromPort, 'A'+ToPort);
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("PORT SWITCH EVENT, From: %d  To: %d (Pref %d) ",
			FromPort, ToPort, pAC->Rlmt.Net[0].PrefPort));
		SkLocalEventQueue64(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					FromPort, SK_FALSE);
		SkLocalEventQueue64(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					ToPort, SK_FALSE);
		spin_lock_irqsave(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);
		spin_lock(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStop(pAC, IoC, FromPort, SK_STOP_ALL, SK_SOFT_RST);
			SkY2PortStop(pAC, IoC, ToPort, SK_STOP_ALL, SK_SOFT_RST);
		}
		else {
			SkGeStopPort(pAC, IoC, FromPort, SK_STOP_ALL, SK_SOFT_RST);
			SkGeStopPort(pAC, IoC, ToPort, SK_STOP_ALL, SK_SOFT_RST);
		}
		spin_unlock(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
		spin_unlock_irqrestore(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);

		
		if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
			WorkToDo = 1;
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE, &WorkDone, WorkToDo);
			ReceiveIrq(pAC, &pAC->RxPort[ToPort], SK_FALSE, &WorkDone, WorkToDo);
#else
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE); /* clears rx ring */
			ReceiveIrq(pAC, &pAC->RxPort[ToPort], SK_FALSE); /* clears rx ring */
#endif
			ClearTxRing(pAC, &pAC->TxPort[FromPort][TX_PRIO_LOW]);
			ClearTxRing(pAC, &pAC->TxPort[ToPort][TX_PRIO_LOW]);
		} 

		spin_lock_irqsave(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);
		spin_lock(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
		pAC->ActivePort = ToPort;

#if 0
		SetQueueSizes(pAC);
#else
		/* tschilling: New common function with minimum size check. */
		DualNet = SK_FALSE;
		if (pAC->RlmtNets == 2) {
			DualNet = SK_TRUE;
		}
		
		if (SkGeInitAssignRamToQueues(
			pAC,
			pAC->ActivePort,
			DualNet)) {
			spin_unlock(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
			spin_unlock_irqrestore(
				&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
				Flags);
			printk("SkGeInitAssignRamToQueues failed.\n");
			break;
		}
#endif
		if (!CHIP_ID_YUKON_2(pAC)) {
			/* tschilling: Handling of return values inserted. */
			if (SkGeInitPort(pAC, IoC, FromPort) ||
				SkGeInitPort(pAC, IoC, ToPort)) {
				printk("%s: SkGeInitPort failed.\n", pAC->dev[0]->name);
			}
		}
		if (!CHIP_ID_YUKON_2(pAC)) {
			if (Event == SK_DRV_SWITCH_SOFT) {
				SkMacRxTxEnable(pAC, IoC, FromPort);
			}
			SkMacRxTxEnable(pAC, IoC, ToPort);
		}

		SkAddrSwap(pAC, IoC, FromPort, ToPort);
		SkAddrMcUpdate(pAC, IoC, FromPort);
		SkAddrMcUpdate(pAC, IoC, ToPort);

#ifdef USE_TIST_FOR_RESET
                if (pAC->GIni.GIYukon2) {
			/* make sure that we do not accept any status LEs from now on */
			SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DUMP,
				("both Ports now waiting for specific Tist\n"));
			SK_SET_WAIT_BIT_FOR_PORT(
				pAC,
				SK_PSTATE_WAITING_FOR_ANY_TIST,
				0);
			SK_SET_WAIT_BIT_FOR_PORT(
				pAC,
				SK_PSTATE_WAITING_FOR_ANY_TIST,
				1);

			/* start tist */
			Y2_ENABLE_TIST(pAC->IoBase);
		}
#endif
		if (!CHIP_ID_YUKON_2(pAC)) {
			PortReInitBmu(pAC, FromPort);
			PortReInitBmu(pAC, ToPort);
			SkGePollTxD(pAC, IoC, FromPort, SK_TRUE);
			SkGePollTxD(pAC, IoC, ToPort, SK_TRUE);
			CLEAR_AND_START_RX(FromPort);
			CLEAR_AND_START_RX(ToPort);
		} else {
			SkY2PortStart(pAC, IoC, FromPort);
			SkY2PortStart(pAC, IoC, ToPort);
#ifdef SK_YUKON2
			/* in yukon-II always port 0 has to be started first */
			// SkY2PortStart(pAC, IoC, 0);
			// SkY2PortStart(pAC, IoC, 1);
#endif
		}
		spin_unlock(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
		spin_unlock_irqrestore(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);
		break;
	case SK_DRV_RLMT_SEND:	 /* SK_MBUF *pMb */
		SK_DBG_MSG(NULL,SK_DBGMOD_DRV,SK_DBGCAT_DRV_EVENT,("RLS "));
		pRlmtMbuf = (SK_MBUF*) Param.pParaPtr;
		pMsg = (struct sk_buff*) pRlmtMbuf->pOs;
		skb_put(pMsg, pRlmtMbuf->Length);
		if (!CHIP_ID_YUKON_2(pAC)) {
			if (XmitFrame(pAC, &pAC->TxPort[pRlmtMbuf->PortIdx][TX_PRIO_LOW],
				pMsg) < 0) {
				DEV_KFREE_SKB_ANY(pMsg);
			}
		} else {
			if (SkY2RlmtSend(pAC, pRlmtMbuf->PortIdx, pMsg) < 0) {
				DEV_KFREE_SKB_ANY(pMsg);
			}
		}
		break;
	case SK_DRV_TIMER:
		if (Param.Para32[0] == SK_DRV_MODERATION_TIMER) {
			/* check what IRQs are to be moderated */
			SkDimStartModerationTimer(pAC);
			SkDimModerate(pAC);
		} else {
			printk("Expiration of unknown timer\n");
		}
		break;
	case SK_DRV_ADAP_FAIL:
#if (!defined (Y2_RECOVERY) && !defined (Y2_LE_CHECK))
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("ADAPTER FAIL EVENT\n"));
		printk("%s: Adapter failed.\n", pAC->dev[0]->name);
		SK_OUT32(pAC->IoBase, B0_IMSK, 0); /* disable interrupts */
		break;
#endif

#if (defined (Y2_RECOVERY) || defined (Y2_LE_CHECK))
	case SK_DRV_RECOVER:
		spin_lock_irqsave(&pAC->InitLock, InitFlags);
		pNet = (DEV_NET *) pAC->dev[Param.Para32[0]]->priv;

		/* Recover already in progress */
		if (pNet->InRecover) {
			break;
		}

		netif_stop_queue(pAC->dev[Param.Para32[0]]); /* stop device if running */
		pNet->InRecover = SK_TRUE;

		FromPort = Param.Para32[0];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("PORT RESET EVENT, Port: %d ", FromPort));

		/* Disable interrupts */
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);
		SK_OUT32(pAC->IoBase, B0_HWE_IMSK, 0);

		SkLocalEventQueue64(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					FromPort, SK_FALSE);
		spin_lock_irqsave(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStop(pAC, IoC, FromPort, SK_STOP_ALL, SK_SOFT_RST);
		} else {
			SkGeStopPort(pAC, IoC, FromPort, SK_STOP_ALL, SK_SOFT_RST);
		}
#if 0 //u-boot
		pAC->dev[Param.Para32[0]]->flags &= ~IFF_RUNNING;
#endif
		spin_unlock_irqrestore(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);
		
		if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
			WorkToDo = 1;
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE, &WorkDone, WorkToDo);
#else
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE);
#endif
			ClearTxRing(pAC, &pAC->TxPort[FromPort][TX_PRIO_LOW]);
		}
		spin_lock_irqsave(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);

#ifdef USE_TIST_FOR_RESET
		if (pAC->GIni.GIYukon2) {
#if 0
			/* make sure that we do not accept any status LEs from now on */
			Y2_ENABLE_TIST(pAC->IoBase);

			/* get current timestamp */
			Y2_GET_TIST_LOW_VAL(pAC->IoBase, &pAC->MinTistLo);
			pAC->MinTistHi = pAC->GIni.GITimeStampCnt;

			SK_SET_WAIT_BIT_FOR_PORT(
				pAC,
				SK_PSTATE_WAITING_FOR_SPECIFIC_TIST,
				FromPort);
#endif
			SK_SET_WAIT_BIT_FOR_PORT(
				pAC,
				SK_PSTATE_WAITING_FOR_ANY_TIST,
				FromPort);

			/* start tist */
                        Y2_ENABLE_TIST(pAC->IoBase);
		}
#endif

		/* Restart Receive BMU on Yukon-2 */
		if (HW_FEATURE(pAC, HWF_WA_DEV_4167)) {
			SkYuk2RestartRxBmu(pAC, IoC, FromPort);
		}

#ifdef Y2_LE_CHECK
		/* mark entries invalid */
		pAC->LastPort = 3;
		pAC->LastOpc = 0xFF;
#endif

#endif
		/* Restart ports but do not initialize PHY. */
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStart(pAC, IoC, FromPort);
		} else {
			/* tschilling: Handling of return value inserted. */
			if (SkGeInitPort(pAC, IoC, FromPort)) {
				if (FromPort == 0) {
					printk("%s: SkGeInitPort A failed.\n", pAC->dev[0]->name);
				} else {
					printk("%s: SkGeInitPort B failed.\n", pAC->dev[1]->name);
				}
			}
			SkAddrMcUpdate(pAC,IoC, FromPort);
			PortReInitBmu(pAC, FromPort);
			SkGePollTxD(pAC, IoC, FromPort, SK_TRUE);
			CLEAR_AND_START_RX(FromPort);
		}
		spin_unlock_irqrestore(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Flags);

		/* Map any waiting RX buffers to HW */
		FillReceiveTableYukon2(pAC, pAC->IoBase, FromPort);

		pNet->InRecover = SK_FALSE;
		/* enable Interrupts */
		SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
		SK_OUT32(pAC->IoBase, B0_HWE_IMSK, IRQ_HWE_MASK);
		netif_wake_queue(pAC->dev[FromPort]);
		spin_unlock_irqrestore(&pAC->InitLock, InitFlags);
		break;
	default:
		break;
	}
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
		("END EVENT "));

	return (0);
} /* SkDrvEvent */


/******************************************************************************
 *
 *	SkLocalEventQueue()	-	add event to queue
 *
 * Description:
 *	This function adds an event to the event queue and run the
 *	SkEventDispatcher. At least Init Level 1 is required to queue events,
 *	but will be scheduled add Init Level 2.
 *
 * returns:
 *	nothing
 */
void SkLocalEventQueue(
SK_AC *pAC,		/* Adapters context */
SK_U32 Class,		/* Event Class */
SK_U32 Event,		/* Event to be queued */
SK_U32 Param1,		/* Event parameter 1 */
SK_U32 Param2,		/* Event parameter 2 */
SK_BOOL Dispatcher)	/* Dispatcher flag:
			 *	TRUE == Call SkEventDispatcher
			 *	FALSE == Don't execute SkEventDispatcher
			 */
{
	SK_EVPARA 	EvPara;
	EvPara.Para32[0] = Param1;
	EvPara.Para32[1] = Param2;
	

	if (Class == SKGE_PNMI) {
		#ifdef SK_PNMI_SUPPORT
		SkPnmiEvent(	pAC,
				pAC->IoBase,
				Event,
				EvPara);
		#endif
	} else {
		SkEventQueue(	pAC,
				Class,
				Event,
				EvPara);
	}

	/* Run the dispatcher */
	if (Dispatcher) {
		SkEventDispatcher(pAC, pAC->IoBase);
	}

}

/******************************************************************************
 *
 *	SkLocalEventQueue64()	-	add event to queue (64bit version)
 *
 * Description:
 *	This function adds an event to the event queue and run the
 *	SkEventDispatcher. At least Init Level 1 is required to queue events,
 *	but will be scheduled add Init Level 2.
 *
 * returns:
 *	nothing
 */
void SkLocalEventQueue64(
SK_AC *pAC,		/* Adapters context */
SK_U32 Class,		/* Event Class */
SK_U32 Event,		/* Event to be queued */
SK_U64 Param,		/* Event parameter */
SK_BOOL Dispatcher)	/* Dispatcher flag:
			 *	TRUE == Call SkEventDispatcher
			 *	FALSE == Don't execute SkEventDispatcher
			 */
{
	SK_EVPARA 	EvPara;
	EvPara.Para64 = Param;


	if (Class == SKGE_PNMI) {
		#ifdef SK_PNMI_SUPPORT
		SkPnmiEvent(	pAC,
				pAC->IoBase,
				Event,
				EvPara);
		#endif
	} else {
		SkEventQueue(	pAC,
				Class,
				Event,
				EvPara);
	}

	/* Run the dispatcher */
	if (Dispatcher) {
		SkEventDispatcher(pAC, pAC->IoBase);
	}

}


/*****************************************************************************
 *
 *	SkErrorLog - log errors
 *
 * Description:
 *	This function logs errors to the system buffer and to the console
 *
 * Returns:
 *	0 if everything ok
 *	< 0  on error
 *	
 */
void SkErrorLog(
SK_AC	*pAC,
int	ErrClass,
int	ErrNum,
char	*pErrorMsg)
{
char	ClassStr[80];

	switch (ErrClass) {
	case SK_ERRCL_OTHER:
		strcpy(ClassStr, "Other error");
		break;
	case SK_ERRCL_CONFIG:
		strcpy(ClassStr, "Configuration error");
		break;
	case SK_ERRCL_INIT:
		strcpy(ClassStr, "Initialization error");
		break;
	case SK_ERRCL_NORES:
		strcpy(ClassStr, "Out of resources error");
		break;
	case SK_ERRCL_SW:
		strcpy(ClassStr, "internal Software error");
		break;
	case SK_ERRCL_HW:
		strcpy(ClassStr, "Hardware failure");
		break;
	case SK_ERRCL_COMM:
		strcpy(ClassStr, "Communication error");
		break;
	}
	printk(KERN_INFO "%s: -- ERROR --\n        Class:  %s\n"
		"        Nr:  0x%x\n        Msg:  %s\n", pAC->dev[0]->name,
		ClassStr, ErrNum, pErrorMsg);

} /* SkErrorLog */

/*****************************************************************************
 *
 *	SkDrvEnterDiagMode - handles DIAG attach request
 *
 * Description:
 *	Notify the kernel to NOT access the card any longer due to DIAG
 *	Deinitialize the Card
 *
 * Returns:
 *	int
 */
int SkDrvEnterDiagMode(
SK_AC   *pAc)   /* pointer to adapter context */
{
	SK_AC   *pAC  = NULL;
	DEV_NET *pNet = NULL;

	pNet = (DEV_NET *) pAc->dev[0]->priv;
	pAC = pNet->pAC;

	SK_MEMCPY(&(pAc->PnmiBackup), &(pAc->PnmiStruct), 
			sizeof(SK_PNMI_STRUCT_DATA));

	pAC->DiagModeActive = DIAG_ACTIVE;
	if (pAC->BoardLevel > SK_INIT_DATA) {
		if (netif_running(pAC->dev[0])) {
			pAC->WasIfUp[0] = SK_TRUE;
			pAC->DiagFlowCtrl = SK_TRUE; /* for SkGeClose      */
			DoPrintInterfaceChange = SK_FALSE;
			SkDrvDeInitAdapter(pAC, 0);  /* performs SkGeClose */
		} else {
			pAC->WasIfUp[0] = SK_FALSE;
		}

		if (pNet != (DEV_NET *) pAc->dev[1]->priv) {
			pNet = (DEV_NET *) pAc->dev[1]->priv;
			if (netif_running(pAC->dev[1])) {
				pAC->WasIfUp[1] = SK_TRUE;
				pAC->DiagFlowCtrl = SK_TRUE; /* for SkGeClose */
				DoPrintInterfaceChange = SK_FALSE;
				SkDrvDeInitAdapter(pAC, 1);  /* do SkGeClose  */
			} else {
				pAC->WasIfUp[1] = SK_FALSE;
			}
		}
		pAC->BoardLevel = SK_INIT_DATA;
	}
	return(0);
}

#ifdef DSK_DIAG_SUPPORT
/*****************************************************************************
 *
 *	SkDrvLeaveDiagMode - handles DIAG detach request
 *
 * Description:
 *	Notify the kernel to may access the card again after use by DIAG
 *	Initialize the Card
 *
 * Returns:
 * 	int
 */
int SkDrvLeaveDiagMode(
SK_AC   *pAc)   /* pointer to adapter control context */
{ 
	SK_MEMCPY(&(pAc->PnmiStruct), &(pAc->PnmiBackup), 
			sizeof(SK_PNMI_STRUCT_DATA));
	pAc->DiagModeActive    = DIAG_NOTACTIVE;
	#ifdef SK_DIAG_SUPPORT

	pAc->Pnmi.DiagAttached = SK_DIAG_IDLE;
	#endif //SK_DIAG_SUPPORT

	if (pAc->WasIfUp[0] == SK_TRUE) {
		pAc->DiagFlowCtrl = SK_TRUE; /* for SkGeClose */
		DoPrintInterfaceChange = SK_FALSE;
		SkDrvInitAdapter(pAc, 0);    /* first device  */
	}
	if (pAc->WasIfUp[1] == SK_TRUE) {
		pAc->DiagFlowCtrl = SK_TRUE; /* for SkGeClose */
		DoPrintInterfaceChange = SK_FALSE;
		SkDrvInitAdapter(pAc, 1);    /* second device */
	}
	return(0);
}
#endif /* DSK_DIAG_SUPPORT */

#if 0 /* uboot */
/*****************************************************************************
 *
 *	ParseDeviceNbrFromSlotName - Evaluate PCI device number
 *
 * Description:
 * 	This function parses the PCI slot name information string and will
 *	retrieve the devcie number out of it. The slot_name maintianed by
 *	linux is in the form of '02:0a.0', whereas the first two characters 
 *	represent the bus number in hex (in the sample above this is 
 *	pci bus 0x02) and the next two characters the device number (0x0a).
 *
 * Returns:
 *	SK_U32: The device number from the PCI slot name
 */ 

static SK_U32 ParseDeviceNbrFromSlotName(
const char *SlotName)   /* pointer to pci slot name eg. '02:0a.0' */
{
	char	*CurrCharPos	= (char *) SlotName;
	int	FirstNibble	= -1;
	int	SecondNibble	= -1;
	SK_U32	Result		=  0;

	while (*CurrCharPos != '\0') {
		if (*CurrCharPos == ':') { 
			while (*CurrCharPos != '.') {
				CurrCharPos++;  
				if (	(*CurrCharPos >= '0') && 
					(*CurrCharPos <= '9')) {
					if (FirstNibble == -1) {
						/* dec. value for '0' */
						FirstNibble = *CurrCharPos - 48;
					} else {
						SecondNibble = *CurrCharPos - 48;
					}  
				} else if (	(*CurrCharPos >= 'a') && 
						(*CurrCharPos <= 'f')  ) {
					if (FirstNibble == -1) {
						FirstNibble = *CurrCharPos - 87; 
					} else {
						SecondNibble = *CurrCharPos - 87; 
					}
				} else {
					Result = 0;
				}
			}

			Result = FirstNibble;
			Result = Result << 4; /* first nibble is higher one */
			Result = Result | SecondNibble;
		}
		CurrCharPos++;   /* next character */
	}
	return (Result);
}
#endif
/****************************************************************************
 *
 *	SkDrvDeInitAdapter - deinitialize adapter (this function is only 
 *				called if Diag attaches to that card)
 *
 * Description:
 *	Close initialized adapter.
 *
 * Returns:
 *	0 - on success
 *	error code - on error
 */
static int SkDrvDeInitAdapter(
SK_AC   *pAC,		/* pointer to adapter context   */
int      devNbr)	/* what device is to be handled */
{
	struct SK_NET_DEVICE *dev;

	dev = pAC->dev[devNbr];

	/*
	** Function SkGeClose() uses MOD_DEC_USE_COUNT (2.2/2.4)
	** or module_put() (2.6) to decrease the number of users for
	** a device, but if a device is to be put under control of 
	** the DIAG, that count is OK already and does not need to 
	** be adapted! Hence the opposite MOD_INC_USE_COUNT or 
	** try_module_get() needs to be used again to correct that.
	*/
	MOD_INC_USE_COUNT;

	if (SkGeClose(dev) != 0) {
		MOD_DEC_USE_COUNT;
		return (-1);
	}
	return (0);

} /* SkDrvDeInitAdapter() */

#if 0 /* uboot*/
/****************************************************************************
 *
 *	SkDrvInitAdapter - Initialize adapter (this function is only 
 *				called if Diag deattaches from that card)
 *
 * Description:
 *	Close initialized adapter.
 *
 * Returns:
 *	0 - on success
 *	error code - on error
 */
static int SkDrvInitAdapter(
SK_AC   *pAC,		/* pointer to adapter context   */
int      devNbr)	/* what device is to be handled */
{
	struct SK_NET_DEVICE *dev;

	dev = pAC->dev[devNbr];

	if (SkGeOpen(dev) != 0) {
		return (-1);
	} else {
		/*
		** Function SkGeOpen() uses MOD_INC_USE_COUNT (2.2/2.4) 
		** or try_module_get() (2.6) to increase the number of 
		** users for a device, but if a device was just under 
		** control of the DIAG, that count is OK already and 
		** does not need to be adapted! Hence the opposite 
		** MOD_DEC_USE_COUNT or module_put() needs to be used 
		** again to correct that.
		*/
		module_put(THIS_MODULE);
	}

	/*
	** Use correct MTU size and indicate to kernel TX queue can be started
	*/ 
#if 0 /* uboot */
	if (SkGeChangeMtu(dev, dev->mtu) != 0) {
		return (-1);
	} 
#endif
	return (0);

} /* SkDrvInitAdapter */

#endif
#if 0 /* uboot */
static int __init sk98lin_init(void)
{
	return pci_module_init(&sk98lin_driver);
}

static void __exit sk98lin_cleanup(void)
{
	pci_unregister_driver(&sk98lin_driver);
}

module_init(sk98lin_init);
module_exit(sk98lin_cleanup);

#endif

#ifdef DEBUG
/****************************************************************************/
/* "debug only" section *****************************************************/
/****************************************************************************/

/*****************************************************************************
 *
 *	DumpMsg - print a frame
 *
 * Description:
 *	This function prints frames to the system logfile/to the console.
 *
 * Returns: N/A
 *	
 */
static void DumpMsg(
struct sk_buff *skb,  /* linux' socket buffer  */
char           *str)  /* additional msg string */
{
	int msglen = (skb->len > 64) ? 64 : skb->len;

	if (skb == NULL) {
		printk("DumpMsg(): NULL-Message\n");
		return;
	}

	if (skb->data == NULL) {
		printk("DumpMsg(): Message empty\n");
		return;
	}
#if 0 /* uboot */
	printk("DumpMsg: PhysPage: %p\n", 
		page_address(virt_to_page(skb->data)));
#endif
	printk("--- Begin of message from %s , len %d (from %d) ----\n", 
		str, msglen, skb->len);
	DumpData((char *)skb->data, msglen);
	printk("------- End of message ---------\n");
} /* DumpMsg */

/*****************************************************************************
 *
 *	DumpData - print a data area
 *
 * Description:
 *	This function prints a area of data to the system logfile/to the
 *	console.
 *
 * Returns: N/A
 *	
 */
static void DumpData(
char  *p,     /* pointer to area containing the data */
int    size)  /* the size of that data area in bytes */
{
	register int  i;
	int           haddr = 0, addr = 0;
	char          hex_buffer[180] = { '\0' };
	char          asc_buffer[180] = { '\0' };
	char          HEXCHAR[] = "0123456789ABCDEF";

	for (i=0; i < size; ) {
		if (*p >= '0' && *p <='z') {
			asc_buffer[addr] = *p;
		} else {
			asc_buffer[addr] = '.';
		}
		addr++;
		asc_buffer[addr] = 0;
		hex_buffer[haddr] = HEXCHAR[(*p & 0xf0) >> 4];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[*p & 0x0f];
		haddr++;
		hex_buffer[haddr] = ' ';
		haddr++;
		hex_buffer[haddr] = 0;
		p++;
		i++;
		if (i%16 == 0) {
			printk("%s  %s\n", hex_buffer, asc_buffer);
			addr = 0;
			haddr = 0;
		}
	}
} /* DumpData */


/*****************************************************************************
 *
 *	DumpLong - print a data area as long values
 *
 * Description:
 *	This function prints a long variable to the system logfile/to the
 *	console.
 *
 * Returns: N/A
 *	
 */
static void DumpLong(
char  *pc,    /* location of the variable to print */
int    size)  /* how large is the variable?        */
{
	register int   i;
	int            haddr = 0, addr = 0;
	char           hex_buffer[180] = { '\0' };
	char           asc_buffer[180] = { '\0' };
	char           HEXCHAR[] = "0123456789ABCDEF";
	long          *p = (long*) pc;
	int            l;

	for (i=0; i < size; ) {
		l = (long) *p;
		hex_buffer[haddr] = HEXCHAR[(l >> 28) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 24) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 20) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 16) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 12) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 8) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 4) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[l & 0x0f];
		haddr++;
		hex_buffer[haddr] = ' ';
		haddr++;
		hex_buffer[haddr] = 0;
		p++;
		i++;
		if (i%8 == 0) {
			printk("%4x %s\n", (i-8)*4, hex_buffer);
			haddr = 0;
		}
	}
	printk("------------------------\n");
} /* DumpLong */

#endif

/*******************************************************************************
 *
 * End of file
 *
 ******************************************************************************/

#endif

