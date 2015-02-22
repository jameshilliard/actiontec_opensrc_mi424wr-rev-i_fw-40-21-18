/*
 * drivers/usb/host/m82xx-hcd.h
 *
 * MPC82xx Host Controller Interface driver for USB.
 * (C) Copyright 2005 Compulab, Ltd
 * Mike Rapoport, mike@compulab.co.il
 *
 * Brad Parker, brad@heeltoe.com
 * (C) Copyright 2000-2004 Brad Parker <brad@heeltoe.com>
 *
 * Fixes and cleanup by:
 * George Panageas <gpana@intracom.gr>
 * Pantelis Antoniou <panto@intracom.gr>
 *
 * designed for the EmbeddedPlanet RPX lite board
 * (C) Copyright 2000 Embedded Planet
 * http://www.embeddedplanet.com
 *
 */

#ifndef M82XXHCI_H
#define M82XXHCI_H

#include <linux/config.h>
#include <linux/list.h>

#ifdef CONFIG_8xx
#include <asm/8xx_immap.h>
#include <asm/mpc8xx.h>
#endif

#ifdef CONFIG_CPM2
#include <asm/immap_cpm2.h>
#include <asm/cpm2.h>
#endif

#define M8XXHCI_HCI_VERS	0x0104

#ifdef CONFIG_8xx
#define USB_SCC_IDX	0	/* SCC1 */
#define CPMVEC_USB	(CPM_IRQ_OFFSET + CPMVEC_SCC1)
#endif

#ifdef CONFIG_CPM2
#define USB_SCC_IDX	2	/* SCC3 */
#define CPMVEC_USB	(CPM_IRQ_OFFSET + CPMVEC_SCC3)
#endif

#define BD_USB_TC       ((ushort)0x0400)        /* transmit crc after last */
#define BD_USB_CNF      ((ushort)0x0200)        /* wait for handshake */
#define BD_USB_LSP      ((ushort)0x0100)        /* low speed */
#define BD_USB_DATA0    ((ushort)0x0080)        /* send data0 pid */
#define BD_USB_DATA1    ((ushort)0x00c0)        /* send data1 pid */
#define BD_USB_RX_PID   ((ushort)0x00c0)        /* rx pid type bits */
#define BD_USB_RX_DATA0 ((ushort)0x0000)        /* rx data0 pid */
#define BD_USB_RX_DATA1 ((ushort)0x0040)        /* rx data1 pid */
#define BD_USB_RX_SETUP ((ushort)0x0080)        /* rx setup pid */

/* tx errors */
#define BD_USB_NAK      ((ushort)0x0010)        /* NAK received */
#define BD_USB_STAL     ((ushort)0x0008)        /* STALL received */
#define BD_USB_TO       ((ushort)0x0004)        /* timeout */
#define BD_USB_UN       ((ushort)0x0002)        /* usb underrun */

/* rx errors */
#define BD_USB_NONOCT   ((ushort)0x0010)        /* non-octet aligned pkt */
#define BD_USB_AB       ((ushort)0x0008)        /* frame aborted */
#define BD_USB_CRC      ((ushort)0x0004)        /* crc error */

/* FCR bits */
#define FCR_LE  0x08    /* little endian */
#define FCR_BE  0x18    /* big endian */

/* USEPx bits */
#define USEP_TM_CONTROL         0x0000
#define USEP_TM_INTERRUPT       0x0100
#define USEP_TM_BULK            0x0200
#define USEP_TM_ISOCHRONOUS     0x0300
#define USEP_TM_MASK   		0x0300
#define USEP_MF_ENABLED         0x0020
#define USEP_RTE_ENABLED        0x0010
#define USEP_THS_NORMAL         0x0000
#define USEP_THS_IGNORE         0x0004
#define USEP_RHS_NORMAL         0x0000
#define USEP_RHS_IGNORE         0x0001
                
/* USMOD bits */
#define USMOD_LSS       0x80
#define USMOD_RESUME    0x40
#define USMOD_SFTE      0x08
#define USMOD_TEST      0x04
#define USMOD_HOST      0x02
#define USMOD_EN        0x01

/* USBER bits */        
#define BER_SFT		0x0400
#define BER_RESET       0x0200
#define BER_IDLE        0x0100
#define BER_TXE3        0x0080
#define BER_TXE2        0x0040
#define BER_TXE1        0x0020
#define BER_TXE0        0x0010
#define BER_SOF         0x0008
#define BER_BSY         0x0004
#define BER_TXB         0x0002
#define BER_RXB         0x0001

/* USB tokens */
#define SOF     0xa5
#define OUT     0xe1
#define IN      0x69
#define SETUP   0x2d
#define DATA0   0xc3
#define DATA1   0x4b
#define ACK     0xd2

/* Rx & Tx ring sizes */

/* note: usb dictates that we need to be able to rx 64 byte frames;
 * the CPM wants to put 2 bytes of CRC at the end and requires that
 * the rx buffers be on a 4 byte boundary.  So, we add 4 bytes of
 * padding to the 64 byte min.
 */
#define CPM_USB_RX_PAGES        8
#define CPM_USB_RX_FRSIZE       (1024)
#define CPM_USB_RX_FRPPG        (PAGE_SIZE / CPM_USB_RX_FRSIZE)
#define RX_RING_SIZE            10 /* (CPM_USB_RX_FRPPG * CPM_USB_RX_PAGES) */
#define TX_RING_SIZE            10

/* CPM control and commands, need to be defined in 
   include/asm-ppc/cpm2.h  */
#define CPM_CR_USB_SBLOCK	(0x13)
#define CPM_CR_USB_PAGE		(0x0b)
#define CPM_CR_USB_RESTART_TX	((ushort)0x000b)


/* this is the max size we tell the CPM */
#define MAX_RBE (CPM_USB_RX_FRSIZE)     /* max receive buffer size (bytes) */

#ifdef CONFIG_8xx

#define CPMTIMER_TGCR_GM4       0x8000	/* gate mode     */
#define CPMTIMER_TGCR_FRZ4      0x4000	/* freeze timer  */
#define CPMTIMER_TGCR_STP4      0x2000	/* stop timer    */
#define CPMTIMER_TGCR_RST4      0x1000	/* restart timer */
#define CPMTIMER_TGCR_GM3       0x0800	/* gate mode     */
#define CPMTIMER_TGCR_FRZ3      0x0400	/* freeze timer  */
#define CPMTIMER_TGCR_STP3      0x0200	/* stop timer    */
#define CPMTIMER_TGCR_RST3      0x0100	/* restart timer */
#define CPMTIMER_TGCR_GM2       0x0080	/* gate mode     */
#define CPMTIMER_TGCR_FRZ2      0x0040	/* freeze timer  */
#define CPMTIMER_TGCR_STP2      0x0020	/* stop timer    */
#define CPMTIMER_TGCR_RST2      0x0010	/* restart timer */
#define CPMTIMER_TGCR_GM1       0x0008	/* gate mode     */
#define CPMTIMER_TGCR_FRZ1      0x0004	/* freeze timer  */
#define CPMTIMER_TGCR_STP1      0x0002	/* stop timer    */
#define CPMTIMER_TGCR_RST1      0x0001	/* restart timer */

#define CPMTIMER_TMR_ORI        0x0010	/* output reference interrupt enable */
#define CPMTIMER_TMR_FRR        0x0008	/* free run/restart */
#define CPMTIMER_TMR_ICLK_INT16 0x0004	/* source internal clock/16 */
#define CPMTIMER_TMR_ICLK_INT   0x0002	/* source internal clock */

#endif

/* MPC850 USB parameter RAM */
typedef struct usbpr {
        ushort  usb_epbptr[4];
        uint    usb_rstate;
        uint    usb_rptr;
        ushort  usb_frame_n;
        ushort  usb_rbcnt;
        uint    usb_rtemp;
	uint    usb_rxdata_temp;
	ushort  usb_micro_ra;
} usbpr_t;

/* USB endpoint parameter block */
typedef struct epb {
        ushort  epb_rbase;
        ushort  epb_tbase;
        u_char  epb_rfcr;
        u_char  epb_tfcr;
        ushort  epb_mrblr;
        ushort  epb_rbptr;
        ushort  epb_tbptr;
        uint    epb_tstate;
        uint    epb_tptr;
        ushort  epb_tcrc;
        ushort  epb_tbcnt;
	uint    epb_ttemp;
	ushort  epb_txmicro_ra;
	ushort  epb_himmr;	/* 8xx rsvd, 82xx himmr */
} epb_t;

/* MPC850 USB registers - mapped onto SCC1 address space */
typedef struct usbregs {
        u_char  usb_usmod;
        u_char  usb_usadr;
        u_char  usb_uscom;
        char    res0;
        ushort  usb_usep[4];
        char    res1[4];
        ushort  usb_usber;
        ushort  res2;
        ushort  usb_usbmr;
        u_char  res3;
        u_char  usb_usbs;
	ushort  usb_ussft;	/* 8xx rsvd, 82xx iusb_ussft */
        u_char  res4[6];
} usbregs_t;

#ifdef CONFIG_8xx

/* bits in parallel i/o port registers that have to be cleared to
 * configure the pins for SCC1 USB use.
 */
#define PA_DR4          ((ushort)0x0800)
#define PA_DR5          ((ushort)0x0400)
#define PA_DR6          ((ushort)0x0200)
#define PA_DR7          ((ushort)0x0100)

#define PA_USB_RXD      ((ushort)0x0001)
#define PA_USB_OE       ((ushort)0x0002)

#define PB_DR28         ((ushort)0x0008)

#define PC_DR5          ((ushort)0x0400)
#define PC_DR12         ((ushort)0x0008)
#define PC_DR13         ((ushort)0x0004)

#define PC_USB_RXP      ((ushort)0x0010)
#define PC_USB_RXN      ((ushort)0x0020)
#define PC_USB_TXP      ((ushort)0x0100)
#define PC_USB_TXN      ((ushort)0x0200)
#define PC_USB_SOF      ((ushort)0x0001) /* bit 15, dreq0* */

#endif

#ifdef CONFIG_CPM2

/* bits in parallel i/o port registers that have to be cleared to  */
/* configure the pins for SCC1 USB use.                            */
							/* sor dir */
							/* --- --- */
#define PD_USB_RXD      ((uint)(1 << (31 - 25)))	/*  0   0  */
#define PD_USB_TN       ((uint)(1 << (31 - 24)))	/*  0   1  */
#define PD_USB_TP       ((uint)(1 << (31 - 23)))	/*  0   1  */

#define PD_MSK		(PD_USB_RXD | PD_USB_TN | PD_USB_TP)
#define PD_DIR0		PD_USB_RXD		
#define PD_DIR1		(PD_USB_TN | PD_USB_TP)

#define PC_USB_OE	((uint)(1 << (31 - 20)))	/*  0   1  */
#define PC_USB_RP	((uint)(1 << (31 - 11))) 	/*  0   0  */
#define PC_USB_RN	((uint)(1 << (31 - 10)))	/*  0   0  */

#define PC_MSK		(PC_USB_OE | PC_USB_RP | PC_USB_RN)
#define PC_DIR0		(PC_USB_RP | PC_USB_RN)
#define PC_DIR1		PC_USB_OE

#define PC_CLK6_48MHz	((uint)(1 << (31 - 26)))	/*  0   0  */
#define PC_CLK8_48MHz	((uint)(1 << (31 - 24)))	/*  0   0  */

#endif

struct m8xxhci_ep {
	struct usb_host_endpoint *hep;
	struct urb *urb;
	struct usb_device *udev;

/* 	struct m8xxhci_qe *qe; */
	struct list_head qe_list;

	/* periodic schedule */
	u16			period;
	u16			branch;
	u16			load;
	struct m8xxhci_ep	*next;

	/* async schedule */
	struct list_head	schedule;
		
	int target_frame;
	int should_be_delayed;

	char busy;
	char busy_count;
};

#define MAX_EP_BUSYS    100/*10*/


/* forward decl. */
struct m8xxhci_private;

/* queue entry */
struct m8xxhci_qe {
        unsigned long inuse;		/* Inuse? */
        int retries;
#define MAX_QE_RETRIES  3
        int busys;			/* # times busy */
#define MAX_QE_STALLED  5
#define MAX_QE_BUSYS    10
        int qtype;
        int qstate;
#define QS_SETUP        1
#define QS_SETUP2       2       
#define QS_SETUP3       3
#define QS_INTR         4
#define QS_BULK         5
#define QS_ISO          6
        unsigned int pipe;              /* pipe info given */
        u_char devnum;
        u_char endpoint;
        void *cmd;
        void *data;
        int whichdata;                  /* data0/1 marker */
        int data_len;                   /* size of whole xfer */
        int recv_len;                   /* IN/size recv so far */
        int send_len;                   /* OUT/size sent so far */
        int status;
        int maxpacketsize;              /* max in/out size */
        int reschedule;                 /* flag - needs reschedule */
        int shortread;                  /* flag - short read */
        int iso_ptr;                    /* index into urb->iso_frame_desc */
        int frame_no;
        u_char *iso_data;               /* ptr to data for current iso frame */
        u_char ph[3];                   /* temp packet header */

        wait_queue_head_t wakeup;

	struct m8xxhci_private *hp;
        struct usb_device *dev;
	struct usb_host_endpoint *hep;
        struct urb *urb;

        struct m8xxhci_qe *next; /* for delay list */
	struct m8xxhci_frame *on_frame_list;
        struct list_head frame_list;
        struct list_head qe_list;

	struct list_head submitted_qe_list;

	struct m8xxhci_ep *ep;

        int delta;              /* delay (in ms) till this is due */
	void *priv;		/* points to hp data */

	int paced;
	int paced_frame_no;

		volatile cbd_t *current_token;
		int zero_length_data;

/* 		struct list_head qe_list; */
};

#define BYTES_PER_USB_FRAME     1280 /*1500*/
#define MAX_Q_TYPES     4

#define Q_ISO           0
#define Q_INTR          1
#define Q_CTRL          2
#define Q_BULK          3

struct m8xxhci_frame {
        int total_bytes;
        int bytes[MAX_Q_TYPES];
        struct list_head heads[MAX_Q_TYPES];
};

/* Virtual Root HUB */
struct virt_root_hub {
        int devnum; /* Address of Root Hub endpoint */ 
        void * urb;
        void * int_addr;
        int send;
        int interval;
        u32 feature;
        u32 hub_status;
        u32 port_status;
        struct timer_list rh_int_timer;
};

/* hub_status bits */
#define RH_HS_LPS            0x00000001         /* local power status */
#define RH_HS_LPSC           0x00010000         /* local power status change */

/* port_status bits */
#define RH_PS_CCS            0x00000001         /* current connect status */
#define RH_PS_PES            0x00000002         /* port enable status*/
#define RH_PS_PSS            0x00000004         /* port suspend status */
#define RH_PS_PRS            0x00000010         /* port reset status */
#define RH_PS_PPS            0x00000100         /* port power status */
#define RH_PS_LSDA           0x00000200         /* low speed device attached */

#define RH_PS_CSC            0x00010000         /* connect status change */
#define RH_PS_PESC           0x00020000         /* port enable status change */
#define RH_PS_PSSC           0x00040000         /* port suspend status change */
#define RH_PS_PRSC           0x00100000         /* port reset status change */

#define RH_PS_CHNG	     (RH_PS_CSC | RH_PS_PESC | RH_PS_PSSC | RH_PS_PRSC)


enum usb_clock {
	USB_CLOCK_PA5_CLK3,
	USB_CLOCK_PA7_CLK1,
	USB_CLOCK_PA6_CLK2,
	USB_CLOCK_BRG3,
	USB_CLOCK_BRG4,
	USB_CLOCK_PC26_CLK6,
	USB_CLOCK_PC24_CLK8,
	USB_CLOCK_CNT,
};

enum sof_timer {
	SOF_TIMER_1,
	SOF_TIMER_2,
	SOF_TIMER_3,
	SOF_TIMER_4,
	SOF_PIT_TIMER_1,
	SOF_PIT_TIMER_2,
	SOF_PIT_TIMER_3,
	SOF_PIT_TIMER_4,
#ifdef CONFIG_BSEIP
	SOF_FPGA_CLK,
#endif
	SOF_USB_CNTRLR,
	SOF_TIMER_CNT,
};

/* SOF Timer Flags */
#define SOFTF_XCVER_ECHOES_SOF	0x0001
#define SOFTF_FIX_SMC1_BRG1	0x0002
#define SOFTF_USE_BRG1_FOR_SOF	0x0004
#define SOFTF_UCODE_PATCH	0x0008

struct usb_m8xx_board_ops {
	int (*init_check)(void);
};

#define PERIODIC_SIZE	32

/*
 * this doesn't really need to be a structure, since we can only have
 * one mcp usb controller, but it makes things more tidy...
 */
struct m8xxhci_private {
	enum usb_clock usb_clock;
	enum sof_timer sof_timer;
	unsigned int sof_flags;

        volatile usbregs_t *usbregs;
        struct usb_bus *bus;
        struct virt_root_hub rh;/* virtual root hub */
        int disabled;

        epb_t *epbptr[4];       /* epb ptr */
        cbd_t *rbase;           /* rx ring bd ptr */
        cbd_t *tbase;           /* tx ring bd ptr */

        int rxnext;             /* index of next rx to be filled */
        int txlast;             /* index of last tx bd fill */
        int txnext;             /* index of next available tx bd */
        int txfree;             /* count of free tx bds */
        int frame_no;           /* frame # send in next SOF */
        u_char sof_pkt[3];      /* temp buffer for sof frames */
        int need_sof;           /* 1ms interrupt could not send flag */
        int ms_count;
        int need_query;
	int xin_clk;		/* needed for SOF_PIT_xxx */

#define M8XXHCI_MAXQE   (32 * 4)
        struct m8xxhci_qe queues[MAX_Q_TYPES][M8XXHCI_MAXQE];
        struct list_head qe_list[MAX_Q_TYPES];
	struct list_head submitted_qe_list;

        struct m8xxhci_qe *active_qe;

        struct m8xxhci_qe *tx_bd_qe[TX_RING_SIZE];

        int port_state;
#define PS_INIT         0
#define PS_DISCONNECTED 1
#define PS_CONNECTED    2
#define PS_READY        3
#define PS_MISSING      4

        int hw_features;
#define HF_LOWSPEED     1

        struct list_head urb_list; /* active urb list.. */

        struct m8xxhci_frame frames[2];
        struct m8xxhci_frame *current_frame;
        struct m8xxhci_frame *next_frame;

	void *rx_hostmem;
	void *rx_va[RX_RING_SIZE];

        /* stats */
        struct {
                ulong isrs;
                ulong cpm_interrupts;
                ulong tmr_interrupts;
		ulong overrun;

                ulong rxb;
                ulong txb;
                ulong bsy;
                ulong sof;
                ulong txe[5];
		ulong nak[5];
		ulong stall[5];
		ulong to[5];
                ulong idle;
                ulong reset;
                ulong tx_err;
                ulong tx_nak;
                ulong tx_stal;
                ulong tx_to;
                ulong tx_un;

                ulong rx_err;
                ulong rx_crc;
                ulong rx_abort;
                ulong rx_nonoct;

                ulong rx_mismatch;
                ulong retransmit;
                ulong tx_restart;

                ulong rh_send_irqs;

		ulong completes[MAX_Q_TYPES];
		ulong enqueues[MAX_Q_TYPES];
		ulong dequeues[MAX_Q_TYPES];
        } stats;

	/* proc entry */
	struct proc_dir_entry	*pde;

	/* various globals assembled here */
	int tmr_max_count;
	int tmr_bytes_per_count_frac;

#ifdef CONFIG_BSEIP
	volatile unsigned short *fpga_addr;
#endif
	spinlock_t lock;
	spinlock_t need_sof_lock;
	spinlock_t framelist_lock;

	spinlock_t queue_lock;
	int queues_busy;

	spinlock_t txbd_list_lock;
	int txbd_list_busy;

	int xcver_echoes_sof;

	unsigned int pending_isrs;

	spinlock_t wrap_lock;

	spinlock_t isr_lock;
	unsigned long isr_state;

	struct m8xxhci_qe *delay_qe_list;

	struct {
		char *dest;
		char *src;
		int len;
		int frame_no;
		struct urb *urb;
	}  pending_iso_rx;

	unsigned char force_disconnect;    
	struct timer_list reset_timer;

	int bus_history;

	/* async schedule */
	struct m8xxhci_ep	*active_ep;

	struct list_head	async;
	struct m8xxhci_ep	*next_async;

	/* periodic schedule: interrupt, iso */
	struct m8xxhci_ep	*next_periodic;
	u16			load[PERIODIC_SIZE];
	struct m8xxhci_ep	*periodic[PERIODIC_SIZE];
	unsigned		periodic_count;
};

static inline struct m8xxhci_private *hcd_to_m82xx(struct usb_hcd *hcd)
{
	return (struct m8xxhci_private *) (hcd->hcd_priv);
}

static inline struct usb_hcd *m82xx_to_hcd(struct m8xxhci_private *m82xx)
{
	return container_of((void *) m82xx, struct usb_hcd, hcd_priv);
}


#endif

