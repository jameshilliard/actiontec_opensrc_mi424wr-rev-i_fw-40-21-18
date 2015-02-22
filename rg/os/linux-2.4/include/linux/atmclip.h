/* atmclip.h - Classical IP over ATM */
 
/* Written 1995-1998 by Werner Almesberger, EPFL LRC/ICA */
 

#ifndef LINUX_ATMCLIP_H
#define LINUX_ATMCLIP_H

#include <linux/sockios.h>
#include <linux/atmioc.h>

#define RFC1483LLC_LEN	8		/* LLC+OUI+PID = 8 */
#define RFC1626_MTU	9180		/* RFC1626 default MTU */

#ifdef __KERNEL__
extern const unsigned char llc_oui[6];
#endif

#endif
