#ifndef _CLIP_H_
#define _CLIP_H_

#ifdef __KERNEL__
#include <linux/types.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)
#define tx_inuse sk->wmem_alloc
#define recvq sk->receive_queue
#endif
#endif
#include <linux/atmapi.h>
#include <linux/atmioc.h>

#define	SIOCCLIP_CREATE	_IOW('a',ATMIOC_CLIP, int) /* create IP interface */
#define	SIOCCLIP_DEL	_IOW('a',ATMIOC_CLIP+1, int) /* destroy IP interface */
#define SIOCCLIP_MKIP	_IOW('a',ATMIOC_CLIP+2, int) /* attach socket to IP */
#define SIOCCLIP_ENCAP	_IOW('a',ATMIOC_CLIP+3, int) /* change encapsulation */
#define SIOCCLIP_SETDEV	_IOW('a',ATMIOC_CLIP+4, int) /* Attach vcc to CLIP
						      *	device
					              */

#endif
