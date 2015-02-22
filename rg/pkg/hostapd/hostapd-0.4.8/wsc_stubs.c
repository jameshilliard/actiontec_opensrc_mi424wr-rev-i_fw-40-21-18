/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/hostapd/hostapd/wsc_stubs.c
 *
 *  Developed by Jungo LTD.
 *  Residential Gateway Software Division
 *  www.jungo.com
 *  info@jungo.com
 *
 *  This file is part of the OpenRG Software and may not be distributed,
 *  sold, reproduced or copied in any way.
 *
 *  This copyright notice should not be removed
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include "hostapd.h"

int wsc_event_notify(char * pDataSend)
{
    return 0;
}

int wsc_ie_init(struct hostapd_data * hapd)
{
    return 0;
}

int wsc_ie_deinit(struct hostapd_data * hapd)
{
    return 0;
}

