/* atmipv6.h - RFC2492 IPv6 over ATM */
 
/*
 * Copyright (C) 2001 USAGI/WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* based on atmclip.h - Classical IP over ATM */
/* Written 1995-1998 by Werner Almesberger, EPFL LRC/ICA */
 

#ifndef LINUX_ATMIPV6_H
#define LINUX_ATMIPV6_H

#include <linux/sockios.h>
#include <linux/atmioc.h>


#define RFC1483LLC_LEN	8		/* LLC+OUI+PID = 8 */
#define RFC1626_MTU	9180		/* RFC1626 default MTU */

#define CLIP_DEFAULT_IDLETIMER 1200	/* 20 minutes, see RFC1755 */
#define CLIP_CHECK_INTERVAL	 10	/* check every ten seconds */

#define	SIOCMKP2PPVC		_IO('a',ATMIOC_IPV6)	/* create Point to Point PVC */

#define ATMIPV6_MKIPV6	_IO('a',ATMIOC_IPV6+1)	/* attach socket to IP */
#define ATMIPV6_SETP2P	_IO('a',ATMIOC_IPV6+2)	/* attach socket to P2P PVC */
#define ATMIPV6_ENCAP	_IO('a',ATMIOC_IPV6+3)	/* change encapsulation */

#ifdef __KERNEL__
extern const unsigned char llc_oui[6];
//TODO: extern const unsigned char llc_oui[6];
#endif

#endif
