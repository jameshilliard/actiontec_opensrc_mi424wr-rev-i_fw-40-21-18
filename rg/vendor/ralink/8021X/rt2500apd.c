/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.

	Module Name:
	rt2500apd.c

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
	Jan, Lee    Dec --2003    modified

*/

#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/wireless.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "rt2500apd.h"
#include "eloop.h"
#include "ieee802_1x.h"
#include "eapol_sm.h"
#include "ap.h"
#include "sta_info.h"
#include "radius_client.h"
#include "config.h"

#define RT2500AP_SYSTEM_PATH   "/etc/Wireless/RT2500AP/RT2500AP.dat"

struct hapd_interfaces {
	int count;
	rtapd **rtapd;
};

/*
	========================================================================
	
	Routine Description:
		Compare two memory block

	Arguments:
		Adapter						Pointer to our adapter

	Return Value:
		0:			memory is equal
		1:			pSrc1 memory is larger
		2:			pSrc2 memory is larger

	Note:
		
	========================================================================
*/
u16	RTMPCompareMemory(void *pSrc1,void *pSrc2, u16 Length)
{
	char *pMem1;
	char *pMem2;
	u16	Index = 0;

	pMem1 = (char*) pSrc1;
	pMem2 = (char*) pSrc2;

	for (Index = 0; Index < Length; Index++)
	{
		if (pMem1[Index] > pMem2[Index])
			return (1);
		else if (pMem1[Index] < pMem2[Index])
			return (2);
	}

	// Equal
	return (0);
}

int OidQueryInformation(u16 OidQueryCode, int socket_id, char *DeviceName, void *ptr, u32 PtrLength)
{
    struct iwreq wrq;

    strcpy(wrq.ifr_name, DeviceName);
    wrq.u.data.length = PtrLength;
    wrq.u.data.pointer = (caddr_t) ptr;
    wrq.u.data.flags = OidQueryCode;

    return (ioctl(socket_id, RT_PRIV_IOCTL, &wrq));
}

int OidSetInformation(u16 OidQueryCode, int socket_id, char *DeviceName, void *ptr, u32 PtrLength)
{
    struct iwreq wrq;
	DBGPRINT(RT_DEBUG_INFO,"OidSetInformation:socket_id=%d, DeviceName=%s)\n",socket_id, DeviceName);

    strcpy(wrq.ifr_name, DeviceName);
    wrq.u.data.length = PtrLength;
    wrq.u.data.pointer = (caddr_t) ptr;
    wrq.u.data.flags = OidQueryCode | OID_GET_SET_TOGGLE;

    return (ioctl(socket_id, RT_PRIV_IOCTL, &wrq));
}

int RT_ioctl(rtapd *rtapd, int code, int param, char  *data, int data_len, unsigned char apidx)
{
    char name[12];
    int ret = 1;

    sprintf(name, "ra%d", apidx);

    name[3] = '\0';
    switch(code)
    {
        case CODE_SET:
            ret = OidSetInformation(param, rtapd->ioctl_sock, name, data, data_len);
            break;

        case CODE_QUERY:
            ret = OidQueryInformation(param, rtapd->ioctl_sock, name, data, data_len);
            break;
    }
    return ret;
}

static void Handle_read(int sock, void *eloop_ctx, void *sock_ctx)
{                              
    //rtapd *rtapd = (rtapd*) eloop_ctx;
	rtapd *rtapd = eloop_ctx;
	int len;
	unsigned char buf[3000];
	u8 *sa, *pos, apidx=0;
    u16 ethertype,i;
    priv_rec *rec;
    size_t left;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0)
    {
		perror("recv");
        Handle_term(15,eloop_ctx,sock_ctx);
        return;
	}

    rec = (priv_rec*)buf;
    left = len -sizeof(*rec)+1;
	if (left <= 0)
    {
		DBGPRINT(RT_DEBUG_ERROR," too short recv\n");
		return;
	}

    sa = rec->saddr;
	ethertype = rec->ethtype[0] << 8;
	ethertype |= rec->ethtype[1];
	if(ethertype != ETH_P_PAE)
    {
        return;
    }

    // search this packet is coming from which interface
	for (i = 0; i < rtapd->conf->SsidNum; i++)
	{
	    if(!memcmp(rec->daddr, rtapd->own_addr[i], 6))
	    {
	        apidx = i;
	        break;
	    }
	}
	if(apidx >= rtapd->conf->SsidNum)
	{
        DBGPRINT(RT_DEBUG_ERROR,"sock not found!!!\n");
	    return;
	}

    pos = rec->xframe;
	if (len < 52 )
    {
		DBGPRINT(RT_DEBUG_INFO,"Handle_read :: handle_short_frame: (len=%d, left=%d \n", len,left);
        for(i = 0; i < left; i++)
			DBGPRINT(RT_DEBUG_INFO," %x", *(pos+i));
		DBGPRINT(RT_DEBUG_INFO,"\n");
	}
    
    ieee802_1x_receive(rtapd, sa, &apidx, pos, left);
}

int Apd_init_sockets(rtapd *rtapd)
{
    struct ifreq ifr;
	struct sockaddr_ll addr;
    int i;

    for(i = 0; i < rtapd->conf->SsidNum; i++)
    {
        sprintf(rtapd->conf->iface[i], "ra%d", i);

        rtapd->sock[i] = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (rtapd->sock[i] < 0)
        {
            perror("socket[PF_PACKET,SOCK_RAW]");
    		return -1;
        }

        if (eloop_register_read_sock(rtapd->sock[i], Handle_read, rtapd, NULL))
        {
            DBGPRINT(RT_DEBUG_ERROR,"Could not register read socket\n");
    		return -1;
        }
        memset(&ifr, 0, sizeof(ifr));

        sprintf(ifr.ifr_name, "ra%d", i);
        if (ioctl(rtapd->sock[i], SIOCGIFINDEX, &ifr) != 0)
        {
            perror("ioctl(SIOCGIFHWADDR)");
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
    	addr.sll_family = AF_PACKET;
    	addr.sll_ifindex = ifr.ifr_ifindex;
    	if (bind(rtapd->sock[i], (struct sockaddr *) &addr, sizeof(addr)) < 0)
        {
    		perror("bind");
    		return -1;
    	}
        DBGPRINT(RT_DEBUG_TRACE,"Opening raw packet socket for ifindex ra%d(sock=%d)\n", i, rtapd->sock[i]);
    }
    
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, 4, rtapd->conf->iface[0]);
    if (ioctl(rtapd->sock[0], SIOCGIFHWADDR, &ifr) != 0)
    {
        perror("ioctl(SIOCGIFHWADDR)");
        return -1;
    }

    DBGPRINT(RT_DEBUG_INFO," Device %s has ifr.ifr_hwaddr.sa_family %d\n",ifr.ifr_name, ifr.ifr_hwaddr.sa_family);
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Invalid HW-addr family 0x%04x\n", ifr.ifr_hwaddr.sa_family);
		return -1;
	}

	memcpy(rtapd->own_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    DBGPRINT(RT_DEBUG_TRACE,"MAC Address= %x:%x:%x:%x:%x:%x\n",rtapd->own_addr[0], rtapd->own_addr[1],rtapd->own_addr[2],rtapd->own_addr[3], rtapd->own_addr[4],rtapd->own_addr[5]);

	return 0;
}

static void Apd_cleanup(rtapd *rtapd)
{
    int i;

    for (i = 0; i < rtapd->conf->SsidNum; i++)
    {
    	if (rtapd->sock[i] >= 0)
    		close(rtapd->sock[i]);
    }
	if (rtapd->ioctl_sock >= 0)
		close(rtapd->ioctl_sock);
    
	Radius_client_deinit(rtapd);

	Config_free(rtapd->conf);
	rtapd->conf = NULL;

	free(rtapd->config_fname);
}

static int Apd_setup_interface(rtapd *rtapd)
{   
	if (rtapd->ioctl_sock < 0)
    {
	    rtapd->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	    if (rtapd->ioctl_sock < 0)
        {
		    perror("socket[PF_INET,SOCK_DGRAM]");
		    return -1;
	    }
    }

	if (Apd_init_sockets(rtapd))
		return -1;    
    
	if (Radius_client_init(rtapd))
    {
		DBGPRINT(RT_DEBUG_ERROR,"RADIUS client initialization failed.\n");
		return -1;
	}

	if (ieee802_1x_init(rtapd))
    {
		DBGPRINT(RT_DEBUG_ERROR,"IEEE 802.1X initialization failed.\n");
		return -1;
	}
    DBGPRINT(RT_DEBUG_TRACE,"rtapd->radius->auth_serv_sock = %d\n",rtapd->radius->auth_serv_sock);

	return 0;
}

static void usage(void)
{
    DBGPRINT(RT_DEBUG_TRACE, "USAGE :  rt2500apd 'config_filename' \n");
}

static rtapd * Apd_init(const char *config_file, int pid)
{
	rtapd *rtapd;
	int i;

	rtapd = malloc(sizeof(*rtapd));
	if (rtapd == NULL)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Could not allocate memory for hostapd data\n");
		exit(1);
	}
	memset(rtapd, 0, sizeof(*rtapd));

	rtapd->config_fname = strdup(config_file);
	if (rtapd->config_fname == NULL)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Could not allocate memory for config_fname\n");
		exit(1);
	}

	rtapd->conf = Config_read(rtapd->config_fname, pid);
	if (rtapd->conf == NULL)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Could not allocate memory for rtapd->conf \n");
		free(rtapd->config_fname);
		exit(1);
	}

	for (i = 0; i < rtapd->conf->SsidNum; i++)
	    rtapd->sock[i] = -1;
	rtapd->ioctl_sock = -1;

	return rtapd;
}

static void Handle_usr1(int sig, void *eloop_ctx, void *signal_ctx)
{
	struct hapd_interfaces *rtapds = (struct hapd_interfaces *) eloop_ctx;
	struct rtapd_config *newconf;
	int i;

	DBGPRINT(RT_DEBUG_TRACE,"Reloading configuration\n");
	for (i = 0; i < rtapds->count; i++)
    {
		rtapd *rtapd = rtapds->rtapd[i];
		newconf = Config_read(rtapd->config_fname,0);
		if (newconf == NULL)
        {
			DBGPRINT(RT_DEBUG_ERROR,"Failed to read new configuration file - continuing with old.\n");
			continue;
		}

		/* TODO: update dynamic data based on changed configuration
		 * items (e.g., open/close sockets, remove stations added to
		 * deny list, etc.) */
		Radius_client_flush(rtapd);
		Config_free(rtapd->conf);
		rtapd->conf = newconf;
        Apd_free_stas(rtapd);

/* when reStartAP, no need to reallocate sock
        for (i = 0; i < rtapd->conf->SsidNum; i++)
        {
            if (rtapd->sock[i] >= 0)
                close(rtapd->sock[i]);
                
    	    rtapd->sock[i] = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	    if (rtapd->sock[i] < 0)
            {
    		    perror("socket[PF_PACKET,SOCK_RAW]");
    		    return;
    	    }
        }*/

	    if (Radius_client_init(rtapd))
        {
		    DBGPRINT(RT_DEBUG_ERROR,"RADIUS client initialization failed.\n");
		    return;
	    }

        DBGPRINT(RT_DEBUG_TRACE,"rtapd->radius->auth_serv_sock = %d\n",rtapd->radius->auth_serv_sock);
	}
}

void Handle_term(int sig, void *eloop_ctx, void *signal_ctx)
{
	FILE    *f;
	char    buf[256], *pos;
	int     line = 0, i;
    int     filesize,cur = 0;
    char    *ini_buffer;             /* storage area for .INI file */

	DBGPRINT(RT_DEBUG_ERROR,"Signal %d received - terminating\n", sig);
	f = fopen(RT2500AP_SYSTEM_PATH, "r+");
	if (f == NULL)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Could not open configuration file '%s' for reading.\n", RT2500AP_SYSTEM_PATH);
		return;
	}

    if ((fseek(f, 0, SEEK_END))!=0)
        return;
    filesize=ftell(f);
	DBGPRINT(RT_DEBUG_ERROR,"filesize %d   - terminating\n", filesize);

    if ((ini_buffer=(char *)malloc(filesize + 1 ))==NULL)
        return;   //out of memory
    fseek(f,0,SEEK_SET);
    fread(ini_buffer, filesize, 1, f);
    fseek(f,0,SEEK_SET);
    ini_buffer[filesize]='\0';

	while ((fgets(buf, sizeof(buf), f)))
    {
		line++;
		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0')
        {
			if (*pos == '\n')
            {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		pos = strchr(buf, '=');
		if (pos == NULL)
        {
		    pos = strchr(buf, '[');                
			continue;
		}
		*pos = '\0';
		pos++;

        if ((strcmp(buf, "pid") == 0) )
        {
            cur = 0;
            while(cur < (int)filesize)
            {  
                if ((ini_buffer[cur]=='p') && (ini_buffer[cur+1]=='i') && (ini_buffer[cur+2]=='d'))
                {
                    cur += 4;
                    for( i=4; i>=0; i--)
                    {
                        if (ini_buffer[cur] !='\n' )
                        {
                            ini_buffer[cur] =0x30;
                        }
                        else
                        {
                            break;
                        }
                        cur++;
                    }   
                    break;
                }
                cur++;
            }
		} 
    }
    fseek(f,0,SEEK_SET);
    fprintf(f, ini_buffer);    
	fclose(f);

	eloop_terminate();
}

int main(int argc, char *argv[])
{
	struct hapd_interfaces interfaces;
    pid_t child_pid;
	int ret = 1, i;
	int c;
    
	for (;;)
    {
		c = getopt(argc, argv, "h");
		if (c < 0)
			break;

		switch (c)
        {
		    case 'h':
		        usage();
			    break;

		    default:
			    break;
		}
	} 

    child_pid = 0;
    if (child_pid == 0)
    {           
        openlog("rt2500apd",0,LOG_DAEMON);
        // set number of configuration file 1
        interfaces.count = 1;
        interfaces.rtapd = malloc(sizeof(rtapd *));
        if (interfaces.rtapd == NULL)
        {
            DBGPRINT(RT_DEBUG_ERROR,"malloc failed\n");
            exit(1);    
        }

        eloop_init(&interfaces);
        eloop_register_signal(SIGINT, Handle_term, NULL);
        eloop_register_signal(SIGTERM, Handle_term, NULL);
        eloop_register_signal(SIGUSR1, Handle_usr1, NULL);
        eloop_register_signal(SIGHUP, Handle_usr1, NULL);

        DBGPRINT(RT_DEBUG_ERROR,"\n Configuration file: %s\n", RT2500AP_SYSTEM_PATH);
        interfaces.rtapd[0] = Apd_init(strdup(RT2500AP_SYSTEM_PATH),(int)getpid());
        if (!interfaces.rtapd[0])
            goto out;
        if (Apd_setup_interface(interfaces.rtapd[0]))
            goto out;
        
        eloop_run();

        Apd_free_stas(interfaces.rtapd[0]);
	    ret = 0;

out:
	    for (i = 0; i < interfaces.count; i++)
        {
		    if (!interfaces.rtapd[i])
			    continue;

		    Apd_cleanup(interfaces.rtapd[i]);
		    free(interfaces.rtapd[i]);
	    }

	    free(interfaces.rtapd);
	    eloop_destroy();
        closelog();
	    return ret;
    }
    else        
        return 0;
}
