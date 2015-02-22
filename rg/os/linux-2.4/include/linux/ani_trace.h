#ifndef _ANI_TRACE_H
#define _ANI_TRACE_H

#ifdef __KERNEL__
#include <linux/autoconf.h>
#endif

#ifdef ANI_NO_TRACE
#undef CONFIG_ANI_TRACE_ENABLED
#endif

#define ANI_TRACE_OFF 1
#define ANI_FREEZE_ON 2
#define ANI_STOP_ON 4
#define ANI_STOP_ON_PARAM 8
#define ANI_FREEZE_ON_PARAM 16

#define ANI_STOP_SLOW_TRACES 0x40000000 /* stop slow traces only */
#define ANI_STOP_FAST_TRACES 0x80000000 /* stop fast traces */

#define ANI_MAX_NUM_TRACE 256 /* power of 2 */

struct trace_state {
	unsigned int status;
	unsigned int param; /* apply to the first param */
};

struct ani_sym {
	unsigned int address;
	const char * name;
};

#define RTAI_SCHED_OUT              0x01
#define RTAI_SCHED_IN               0x02
#define RTAI_TO_LINUX               0x03
#define LINUX_TO_RTAI               0x04
#define RTAI_MBX_SEND               0x05
#define RTAI_MBX_RECV               0x06
#define RTAI_MBX_RECV_IF            0x07
#define RTAI_IRQ_TO_LINUX           0x08
#define RTAI_IRQ_IN                 0x09
#define RTAI_IRQ_OUT                0x0a
#define RTAI_GLOBAL_IRQ_IN          0x0b
#define RTAI_GLOBAL_IRQ_OUT         0x0c
#define RTAI_TIMER_IRQ_IN           0x0d
#define RTAI_TIMER_IRQ_OUT          0x0e
#define RTAI_DISPATCH_LINUX_IRQ_IN  0x0f

#define RTAI_DISPATCH_LINUX_IRQ_OUT  0x10
#define RTAI_SEM_SIGNAL              0x11
#define SYSISR                       0x12
#define LINUX_DO_IRQ_IN              0x13
#define LINUX_DO_IRQ_OUT             0x14
#define DPH_TX_IN                    0x15
#define DPH_TX_OUT                   0x16
#define DPH_RX_IN                    0x17
#define DPH_RX_OUT                   0x18
#define PLM_POLL_IN                  0x19
#define PLM_POLL_OUT                 0x1a
#define PLM_START_XMIT_IN            0x1b
#define PLM_START_XMIT_OUT           0x1c
#define PLM_START_XMIT_OUT1          0x1d
#define PLM_START_XMIT_OUT2          0x1e
#define PLM_START_XMIT_OUT3          0x1f


#define PLM_REFILL_IN            0x20
#define PLM_REFILL_OUT           0x21
#define PLM_POLL_NFILTER_IN      0x22
#define PLM_POLL_NFILTER_OUT     0x23
#define NF_HOOK_SLOW_IN          0x24
#define NF_HOOK_SLOW_OUT         0x25
#define NATSEMI_POLL_IN          0x26
#define NATSEMI_POLL_OUT         0x27
#define NATSEMI_POLL_OUT1        0x28
#define NATSEMI_POLL_OUT2        0x29
#define LINUX_SCHED_OUT          0x2a
#define LINUX_SCHED_IN           0x2b
#define LINUX_TASKLET_IN         0x2c
#define LINUX_TASKLET_OUT        0x2d
#define LINUX_NET_TX_IN          0x2e
#define LINUX_NET_TX_OUT         0x2f

#define LINUX_NET_RX_IN          0x30
#define LINUX_NET_RX_OUT         0x31
#define LINUX_TIMER_IRQ_IN       0x32
#define LINUX_TIMER_IRQ_OUT      0x33
#define NATSEMI_START_XMIT_IN    0x34
#define NATSEMI_START_XMIT_OUT   0x35
#define LINUX_DO_SOFTIRQ_IN      0x36
#define LINUX_DO_SOFTIRQ_OUT     0x37
#define LINUX_FORK               0x38
#define NATSEMI_POLL_END         0x39
#define PLM_CLEAN_UP_TX_RING     0x3a
#define TLB_MISS                 0x3b
#define NATSEMI_RECEIVE_SKB_IN   0x3c
#define NATSEMI_RECEIVE_SKB_OUT  0x3d
#define IP_RCV_FINISH_IN         0x3e
#define IP_RCV_RT_INPUT_END      0x3f
#define IP_RCV_FINISH_DELIVER    0x40
#define DEV_DELIVER              0x41
#define DEV_DELIVER1             0x42
#define DEV_DELIVER2             0x43
#define DEV_DELIVER3             0x44
#define DEV_QUEUE_XMIT_IN        0x45
#define DEV_QUEUE_START_XMIT     0x46
#define DEV_QUEUE_XMIT_OUT       0x47
#define DEV_QUEUE_XMIT_OUT1       0x48
#define  RX_DEMUX_GET_BUFF 0x49
#define RTAI_RX_DEMUX_PEND_SRQ 0x4a
#define PLM_PROC_MBX 0x4b
#define PLM_PROC_MBX_RX 0x4c
#define PLM_TX_TKIP_IN 0x4d
#define PLM_TX_TKIP_OUT 0x4e
#define PLM_RX_TKIP_IN 0x4f
#define PLM_RX_TKIP_OUT 0x50
#define TKIP_RC4_IN 0x51
#define TKIP_RC4_OUT 0x52
#define TKIP_MIC_IN 0x53
#define TKIP_MIC_OUT 0x54
#define TKIP_PHASE1_IN 0x55
#define TKIP_PHASE1_OUT 0x56
#define TKIP_PHASE2_IN 0x57
#define TKIP_PHASE2_OUT 0x58
#define DPH_TKIP_RX_REASS_IN 0x59
#define DPH_TKIP_RX_REASS_OUT 0x5a
#define DPH_TKIP_RX_REASS_STEP1 0x5b
#define DPH_TKIP_RX_REASS_STEP2 0x5c
#define DPH_TKIP_RX_REASS_STEP3 0x5d
#define DPH_LOG 0x5e
#define TKIP_ICV_IN 0x5f
#define TKIP_ICV_OUT 0x60
#define PLM_POL_GET 0x61
#define PLM_POL_SET 0x62
#define PLM_POL_READ 0x63
#define PLM_POL_WRITE 0x64
#define DRV_POLARIS_GET 0x65
#define DRV_POLARIS_SET 0x66
#define DRV_POLARIS_MEM_READ 0x67
#define DRV_POLARIS_MEM_WRITE 0x68


#define NATSEMI_COPY  0x6a
#define NATSEMI_INTR_IN 0x6b
#define NATSEMI_INTR_OUT 0x6c
#define ALLOC_SKB_IN 0x6d
#define ALLOC_SKB_OUT 0x6e
#define KFREE_SKB_IN 0x6f
#define KFREE_SKB_OUT 0x70
#define CLONE_SKB_IN 0x71
#define CLONE_SKB_OUT 0x72

#define OPENPIC_ENABLE 0x73
#define OPENPIC_DISABLE 0x74
#define OPENPIC_ACK 0x75
#define OPENPIC_END 0x76
#define HANDLE_IRQ_EVENT_IN 0x77
#define HANDLE_IRQ_EVENT_OUT 0x78
#define PLM_RX_RADIO 0x79

#define NETIF_RX_IN 			0x80
#define NETIF_RX_OUT 			0x81
#define NET_RX_ACTION_IN 		0x82
#define NET_RX_ACTION_OUT		0x83
#define NET_TX_ACTION_IN		0x84
#define NET_TX_ACTION_OUT		0x85
#define ANI_TCP_TRACE_ID		0x86
#define ANI_ICMP_TRACE_ID		0x87


#define ANI_LOAD_MEASURE  0x88

//------------------------------ User Defined -----------------------
#define PLM_HDD_START			0xA0
#define PLM_HDD_END 			0xA9

#define NUM_TAG         		0xAA

              

#ifdef __KERNEL__
#ifdef CONFIG_ANI_TRACE_ENABLED
#include <asm-ppc/ani_trace.h>
#else
//static __inline__ void ani_tracer(unsigned long flag, unsigned int p1, unsigned int p2) {};
static __inline__ void ani_stop_trace(void) {};
static __inline__ void ani_start_trace(void) {};
static __inline__ void ani_stop_slow_trace(void) {};
static __inline__ void ani_start_slow_trace(void) {};
static __inline__ void ani_stop_fast_trace(void) {};
static __inline__ void ani_start_fast_trace(void) {};

static __inline__ void ani_set_wrap_mode_bit(void) {};
static __inline__ void ani_clear_wrap_mode_bit(void) {};
static __inline__ void enable_trace(unsigned int index) {};
static __inline__ void disable_trace(unsigned int index) {};

#define ANI_TRACE(x,y,z)

#define ANI_FAST_TRACE(x)
#define ANI_FAST_TRACE1(x,y)
#define ANI_FAST_TRACE2(x,y,z)

#define ANI_BIG_TRACE(tag,p1,p2,p3,p4,p5,p6)

#define ANI_TRACE_SKB(skb,tag)
#define ANI_UNTRACE_SKB(skb,tag)

#endif
#endif /* ifdef __KERNEL__ */

#endif /* ifndef _ANI_TRACE_H */
