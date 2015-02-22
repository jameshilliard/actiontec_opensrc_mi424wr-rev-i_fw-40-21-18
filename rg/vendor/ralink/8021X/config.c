/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.

    Module Name:
    config.c

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    Jan, Lee    Dec --2003    modified

*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rt2500apd.h"
#include "md5.h"

static int
Config_read_radius_addr(struct hostapd_radius_server **server,
                int *num_server, const char *val, int def_port,
                struct hostapd_radius_server **curr_serv)
{
    struct hostapd_radius_server *nserv;
    int ret;

    nserv = realloc(*server, (*num_server + 1) * sizeof(*nserv));
    if (nserv == NULL)
        return -1;

    *server = nserv;
    nserv = &nserv[*num_server];
    (*num_server)++;
    (*curr_serv) = nserv;

    memset(nserv, 0, sizeof(*nserv));
    nserv->port = def_port;
    ret = !inet_aton(val, &nserv->addr);

    return ret;
}

struct rtapd_config * Config_read(const char *fname, int pid)
{
    struct rtapd_config *conf;
    FILE *f;
    char buf[1024], *pos;
    int line = 0,  savecur = 0,tmp = 0;
    int errors = 0,i;
    int flag = 0;
    long    filesize,cur = 0;
    char    *ini_buffer,save,prev,*buff;             /* storage area for .INI file */
    int now = 100000;

    f = fopen(fname, "r+t");
    //DBGPRINT(RT_DEBUG_TRACE, "r + b     %x \n",'\n');
    if (f == NULL)
    {
        DBGPRINT(RT_DEBUG_ERROR,"Could not open configuration file '%s' for reading.\n", fname);
        return NULL;
    }
    if ((fseek(f, 0, SEEK_END))!=0)
        return (0);

    filesize=ftell(f);
    if ((ini_buffer=(char *)malloc(filesize + 1 + 10))==NULL)
        return (0);   //out of memory

    fseek(f,0,SEEK_SET);
    fread(ini_buffer, filesize, 1, f);
    fseek(f,0,SEEK_SET);
    ini_buffer[filesize]='\0';
    conf = malloc(sizeof(*conf));
    if (conf == NULL)
    {
        DBGPRINT(RT_DEBUG_TRACE, "Failed to allocate memory for configuration data.\n");
        fclose(f);
        return NULL;
    }
    memset(conf, 0, sizeof(*conf));

    conf->SsidNum = 1;
    conf->individual_wep_key_len = WEP8021X_KEY_LEN;
    conf->session_timeout_set = 0xffff;
    hostapd_get_rand(conf->IEEE8021X_ikey, conf->individual_wep_key_len);
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
            if (pos == NULL) 
                errors++;
            continue;
        }
        *pos = '\0';
        pos++;

        if (strcmp(buf, "debug") == 0)
        {
            conf->debug = atoi(pos);
        }
        else if (strcmp(buf, "own_ip_addr") == 0)
        {
            if (!inet_aton(pos, &conf->own_ip_addr))
            {
                DBGPRINT(RT_DEBUG_ERROR,"Line %d: invalid IP address '%s'\n", line, pos);
                errors++;
            }
            flag |= 0x01;
        }
        else if (strcmp(buf, "RADIUS_Server") == 0)
        {
            if (Config_read_radius_addr(
                    &conf->auth_servers,
                    &conf->num_auth_servers, pos, 1812,
                    &conf->auth_server))
            {
                DBGPRINT(RT_DEBUG_ERROR,"Line %d: invalid IP address '%s'\n", line, pos);
                errors++;
            }
            flag |= 0x02;
            DBGPRINT(RT_DEBUG_TRACE,"IP address: '%s'\n", pos);
        }
        else if (conf->auth_server && strcmp(buf, "RADIUS_Port") == 0)
        {
            conf->auth_server->port = atoi(pos);
            flag |= 0x04;
            DBGPRINT(RT_DEBUG_TRACE,"RADIUS_Port: '%s'\n", pos);
        }
       else if (conf->auth_server && strcmp(buf, "RADIUS_Key") == 0)
        {
            int len = strlen(pos);
            if (pos[len-1] == 0xd)
                len--;
            if ( len == 0 || len == 1)
            {
                /* RFC 2865, Ch. 3 */
                DBGPRINT(RT_DEBUG_ERROR,"Line %d: empty shared secret is not allowed.\n", line);
                errors++;
            }
            conf->auth_server->shared_secret = strdup(pos);            
            conf->auth_server->shared_secret_len = len;
            DBGPRINT(RT_DEBUG_TRACE,"RADIUS_Key: '%s', Key_len: %d\n", conf->auth_server->shared_secret, conf->auth_server->shared_secret_len);
            flag |= 0x08;
        }
        else if (strcmp(buf, "radius_retry_primary_interval") == 0)
        {
            conf->radius_retry_primary_interval = atoi(pos);
        }
        else if (strcmp(buf, "SSIDNum") == 0)
        {
            conf->SsidNum = atoi(pos);
            if(conf->SsidNum > MAX_MSSID_NUM)
                conf->SsidNum = 1;
            DBGPRINT(RT_DEBUG_TRACE,"conf->SsidNum=%d\n", conf->SsidNum);
        }
        else if ((strcmp(buf, "pid") == 0) && (pid != 0))
        {
            //fseek(f,0,SEEK_CUR);
            //fprintf(f, "pid=%4x", pid);
            flag |= 0x10;
            cur = 0;
            tmp = pid;
            while(cur <= (int)filesize)
            {  
                if ((ini_buffer[cur]=='p') && (ini_buffer[cur+1]=='i') && (ini_buffer[cur+2]=='d') )
                {
                    cur += 4;
                    for( i=4; i>=0; i--)
                    {
                        now = now/10;
                        if(ini_buffer[cur]!='\n')
                        {
                            ini_buffer[cur] = tmp/(now)+0x30;
                        }
                        else
                        {
                            prev = ini_buffer[cur];
                            ini_buffer[cur] = tmp/(now)+0x30;
                            savecur = cur+1;
                            do
                            {
                                save = ini_buffer[savecur];
                                ini_buffer[savecur] = prev;
                                prev = save;
                                savecur ++;
                            }while(savecur <= (filesize+5));
                        }  
                        cur++;
                        tmp -= ((tmp/(now))*(now));                        
                    }   
                    break;
                }
                cur++;
            }
        } 
        else if (strcmp(buf, "session_timeout_interval") == 0)
        {

            flag |= 0x20;
            conf->session_timeout_interval = atoi(pos);
            if (conf->session_timeout_interval == 0)
                conf->session_timeout_set= 0;
            else
                conf->session_timeout_set= 1;
		
            DBGPRINT(RT_DEBUG_TRACE,"session_timeout policy = %s \n", conf->session_timeout_set?"set":"not use");
            DBGPRINT(RT_DEBUG_TRACE,"Read Session Timeout Interval  %d seconds. \n", conf->session_timeout_interval);
            conf->session_timeout_interval = (atoi(pos)<60) ? REAUTH_TIMER_DEFAULT_reAuthPeriod : atoi(pos);
            DBGPRINT(RT_DEBUG_TRACE,"Set Session Timeout Interval  %d seconds. \n", conf->session_timeout_interval);
    
        }
        
    }
    fseek(f,0,SEEK_SET);
    fprintf(f, ini_buffer);    
    fclose(f);
    if ((!(flag&0x10)) && (pid != 0))
    {
        tmp = pid;
        f = fopen(fname, "r+");
        if ((buff=(char *)malloc(10))==NULL)
            return (0);   //out of memory
        buff[9]='\0';
        fseek(f,0,SEEK_END);
        strcpy(buff,"pid=");
        cur = 4;
        for( i=4; i>=0; i--)
        {
            now = now/10;
            buff[cur] = tmp/(now)+0x30;
            cur++;
            tmp -= ((tmp/(now))*(now));
        }        
        fprintf(f, buff);    
        fclose(f);
    }

    conf->auth_server = conf->auth_servers;
    if (errors )
    {
        DBGPRINT(RT_DEBUG_ERROR,"%d errors found in configuration file '%s'\n", errors, fname);
        Config_free(conf);
        conf = NULL;
    }
    if ((flag&0x0f)!=0x0f )
    {
        DBGPRINT(RT_DEBUG_ERROR,"Not enough parameters found in configuration file '%s'\n", fname);
        Config_free(conf);
        conf = NULL;
    }
    return conf;
}

static void Config_free_radius(struct hostapd_radius_server *servers, int num_servers)
{
    int i;

    for (i = 0; i < num_servers; i++)
    {
        free(servers[i].shared_secret);
    }
    free(servers);
}

void Config_free(struct rtapd_config *conf)
{
    if (conf == NULL)
        return;

    Config_free_radius(conf->auth_servers, conf->num_auth_servers);
    free(conf);
}

