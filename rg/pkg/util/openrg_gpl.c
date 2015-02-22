/****************************************************************************
 *
 * rg/pkg/util/openrg_gpl.c
 * 
 * Copyright (C) Jungo LTD 2004
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General 
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02111-1307, USA.
 */

#include <rg_config.h>
#ifdef CONFIG_GLIBC
#include <syslog/glibc/ulibc_syslog.h>
#endif
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "openrg_gpl.h"

void sys_sleep(unsigned int seconds)
{
    sleep(seconds);
}

void socket_close(int fd)
{
    close(fd);
}

/* normally should be called with src_ip==0 and src_port==0 */
int sock_socket(int type, u32 src_ip, u16 src_port)
{
    int sock = -1;
    struct sockaddr_in sa;

    if ((sock = socket(AF_INET, type, 0))<0)
	return rg_error(LERR, "sock_socket: failed socket()");
    MZERO(sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = src_ip;
    sa.sin_port = src_port; 
    if (src_port)
    {
	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
	    (void *)&opt, sizeof(opt))<0)
	{
	    rg_error(LERR, "sock_socket: failed setsockopt(SO_REUSEADDR)");
	    goto Error;
	}
	if ((bind(sock, (struct sockaddr *)&sa, sizeof(sa)))<0)
	{
	    rg_error(LERR, "sock_socket: failed bind(): %s", strerror(errno));
	    goto Error;
	}
    }
    else
    {
	if ((bind(sock, (struct sockaddr *)&sa, sizeof(sa)))<0)
	{
	    rg_error(LERR, "sock_socket: failed bind()");
	    goto Error;
	}
    }
    
    rg_error(LDEBUG, "sock_socket: created fd %d for ip %s port %d", sock,
	inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
    return sock;
    
Error:
    if (sock>=0)
	close(sock);
    return -1;
}

static void rg_reboot(void)
{
    rg_error(LINFO, "rebooting!");

#ifdef HAVE_KILL
   /* signal the init process (openrg or init) to reboot. */
    kill(1, SIGINT);
#else
    /* This code generates three types of exception: 
     * division by 0 - handled by FPU/Emulator, 
     * unaligned access (float at wrong alignment) - for non-MMU out there
     * segmentation  - 1 is not legal at most MMU platforms.
     */
    *(float *)1 = 1 / 0.0;
#endif
}

int rg_error_full(log_entity_id_t entity_id, rg_error_level_t severity,
    const char *format, ...)
{
    va_list ap;
    int priority, need_rg_closelog = 0;
    u32 severity_level = severity & LLEVEL_MASK;    
    static int priorities[] = {
	LOG_EMERG,
	LOG_ALERT,
	LOG_CRIT,
	LOG_ERR,
	LOG_WARNING,
	LOG_NOTICE,
	LOG_INFO,
	LOG_DEBUG,
    };
    
    priority = LOG_DAEMON | priorities[severity_level];
   
    if (!rg_openlog_initialized)
    {
	/* There are some external processes that might call rg_error()
	 * indirectly (for example, by calling ipc_connect()), before
	 * rg_openlog() has been called. Since we need the entity ID in
	 * rg_error() calls, we call rg_openlog ourselves, and then close it,
	 * so the process will be able to call rg_openlog later */
	rg_openlog_full(entity_id, NULL, LOG_CONS | LOG_NDELAY | LOG_PID,
	    LOG_USER);
	need_rg_closelog = 1;
    }
    va_start(ap, format);
    vsyslog(priority, format, ap);
    va_end(ap);

    if (severity_level == LEXIT || severity_level == LPANIC)
	rg_reboot();
    
    if (need_rg_closelog)
	rg_closelog();

    return -1;
}

#define RG_IOCTL_PREFIX_BASE 0xD0
#define RG_IOCTL_PREFIX_KOS (RG_IOCTL_PREFIX_BASE + 8)
#define RG_IOCTL_SIOCSETRGCHRDEVTYPE \
    _IOW(RG_IOCTL_PREFIX_KOS, 3, struct ifreq)

int gpl_sys_rg_chrdev_open(int type, int mode)
{
    int fd = open("/dev/rg_chrdev", mode, 0);

    if (fd < 0)
    {
	rg_error(LERR, "Cannot open /dev/rg_chrdev %d %m", type);
	return fd;
    }

    if (ioctl(fd, RG_IOCTL_SIOCSETRGCHRDEVTYPE, type))
	return rg_error(LERR, "Cannot open /dev/rg_chrdev %d %m", type);

    return fd;
}

