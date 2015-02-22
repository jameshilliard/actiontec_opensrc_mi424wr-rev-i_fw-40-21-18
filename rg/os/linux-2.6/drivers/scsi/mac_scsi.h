/*
 * Cumana Generic NCR5380 driver defines
 *
 * Copyright 1993, Drew Eckhardt
 *	Visionary Computing
 *	(Unix and Linux consulting and custom programming)
 *	drew@colorado.edu
 *      +1 (303) 440-4894
 *
 * ALPHA RELEASE 1.
 *
 * For more information, please consult
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
 *
 * NCR Microelectronics
 * 1635 Aeroplaza Drive
 * Colorado Springs, CO 80916
 * 1+ (719) 578-3400
 * 1+ (800) 334-5454
 */

/*
 * $Log: mac_scsi.h,v $
 * Revision 1.1.1.1  2007/05/07 23:30:46  jungo
 * Import of Jungo RG SDK 4.6.2
 *
 * Revision 1.3  2006/06/15 21:32:32  itay
 * B33147 Upgrade kernel to linux-2.6.16.14
 * OPTIONS: novalidate
 *
 * Revision 1.2.34.1  2006/06/14 11:12:07  itay
 * B33147 Upgrade kernel to linux-2.6.16.14 and add support for broadcom 6358 (merge from branch-4_2_3_2)
 * OPTIONS: novalidate
 *
 * Revision 1.2.28.1  2006/05/17 15:29:31  itay
 * B33147 merge changes between linux-2_6_12 and linux-2_6_16_14
 * NOTIFY: cancel
 * OPTIONS: novalidate
 *
 * Revision 1.2  2005/08/08 06:53:24  noams
 * B24862 Initial import of linux-2.6.12
 *
 * Revision 1.1.2.1  2005/08/07 13:20:02  noams
 * B24862 Initial import of linux-2.6.12
 * Revision 1.1.2.2  2006/02/03 08:02:55  igor
 * Import linux kernel 2.6.15 to vendor branch
 *
 */

#ifndef MAC_NCR5380_H
#define MAC_NCR5380_H

#define MACSCSI_PUBLIC_RELEASE 2

#ifndef ASM

#ifndef CMD_PER_LUN
#define CMD_PER_LUN 2
#endif

#ifndef CAN_QUEUE
#define CAN_QUEUE 16
#endif

#ifndef SG_TABLESIZE
#define SG_TABLESIZE SG_NONE
#endif

#ifndef USE_TAGGED_QUEUING
#define	USE_TAGGED_QUEUING 0
#endif

#include <scsi/scsicam.h>

#ifndef HOSTS_C

#define NCR5380_implementation_fields \
    int port, ctrl

#define NCR5380_local_declare() \
        struct Scsi_Host *_instance

#define NCR5380_setup(instance) \
        _instance = instance

#define NCR5380_read(reg) macscsi_read(_instance, reg)
#define NCR5380_write(reg, value) macscsi_write(_instance, reg, value)

#define NCR5380_pread 	macscsi_pread
#define NCR5380_pwrite 	macscsi_pwrite
	
#define NCR5380_intr macscsi_intr
#define NCR5380_queue_command macscsi_queue_command
#define NCR5380_abort macscsi_abort
#define NCR5380_bus_reset macscsi_bus_reset
#define NCR5380_proc_info macscsi_proc_info

#define BOARD_NORMAL	0
#define BOARD_NCR53C400	1

#endif /* ndef HOSTS_C */
#endif /* ndef ASM */
#endif /* MAC_NCR5380_H */

