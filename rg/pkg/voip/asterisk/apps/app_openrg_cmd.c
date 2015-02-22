/****************************************************************************
 *
 * rg/pkg/voip/asterisk/apps/app_openrg_cmd.c
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
 *
 * Developed by Jungo LTD.
 * Residential Gateway Software Division
 * www.jungo.com
 * info@jungo.com
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "asterisk.h"

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/manager.h"

static char *tdesc = "OpenRG Command";

static char *app = "openrg_cmd";

static char *synopsis = "Initiate manager event";

static char *descrip = 
"  openrg_cmd():  Use openrg_cmd to send a commmand to OpenRG.\n";

STANDARD_LOCAL_USER;

LOCAL_USER_DECL;

static int event_exec(struct ast_channel *chan, void *data)
{
	struct localuser *u;

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING,
		    "openrg_cmd requires an argument(text)\n");
		return -1;
	}

	LOCAL_USER_ADD(u);
	manager_event(EVENT_FLAG_SYSTEM, "openrg_cmd", "%s\n", (char *)data);
	LOCAL_USER_REMOVE(u);

	return 0;
}

int unload_module(void)
{
	int res;

	res = ast_unregister_application(app);

	return res;
}

int load_module(void)
{
	return ast_register_application(app, event_exec, synopsis, descrip);
}

char *description(void)
{
	return tdesc;
}

int usecount(void)
{
	int res;
	STANDARD_USECOUNT(res);
	return res;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
