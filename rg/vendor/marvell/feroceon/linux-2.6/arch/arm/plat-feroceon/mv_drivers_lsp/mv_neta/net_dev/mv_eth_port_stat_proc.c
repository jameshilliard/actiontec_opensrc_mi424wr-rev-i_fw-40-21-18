/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell 
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.


********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or 
modify this File in accordance with the terms and conditions of the General 
Public License Version 2, June 1991 (the "GPL License"), a copy of which is 
available along with the File in the license.txt file or by writing to the Free 
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or 
on the worldwide web at http://www.gnu.org/licenses/gpl.txt. 

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED 
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY 
DISCLAIMED.  The GPL License provides additional details about this warranty 
disclaimer.
*******************************************************************************/
#include <linux/version.h>
#include <linux/proc_fs.h>

#include "gbe/mvNeta.h"
#include "mv_eth_proc.h"
#include "mv_netdev.h"


/* global variables from 'regdump' */
static struct proc_dir_entry *mv_eth_port_tool;


int mv_eth_port_tool_write (struct file *file, const char *buffer,
                      unsigned long count, void *data) {
	return 0;
}

static int proc_calc_metrics(char *page, char **start, off_t off,
                                 int count, int *eof, int len)
{
        if (len <= off+count) *eof = 1;
        *start = page + off;
        len -= off;
        if (len>count) len = count;
        if (len<0) len = 0;
        return len;
}


/* Read link duplex & speed status for switch ports 0..4 */
int mv_eth_port_tool_read (char *page, char **start, off_t off,
                            int count, int *eof, void *data) 
{
	unsigned  int port = 0, link = 0;
	MV_ETH_PORT_DUPLEX duplex = 0;
	MV_ETH_PORT_SPEED speed = 0;
	char *out = page;
	int len;
	int num_of_switch_ports = mvBoardSwitchNumPortsGet();

    	out += sprintf(out, "%d ", num_of_switch_ports);

	for (port = 0 ; port < num_of_switch_ports ; port++)
	{
	    mv_eth_switch_port_link_status_get(port, &link, &duplex, &speed);
	    out += sprintf(out, "%d %d %d ", link, duplex, speed);
	}

	len = out - page - off;

	if (len < count) 
	{
	    *eof = 1;
	    if (len <= 0) 
		return 0;
	} 
	else 
	    len = count;

	*start = page + off;

	return len;
}



int __init start_mv_eth_port_tool(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
     mv_eth_port_tool = proc_net_create(PORT_STAT_FILE_NAME , 0666 , NULL);
#else
     mv_eth_port_tool = create_proc_entry(PORT_STAT_FILE_NAME , 0666 , init_net.proc_net);
#endif
  mv_eth_port_tool->read_proc = mv_eth_port_tool_read;
  mv_eth_port_tool->write_proc = mv_eth_port_tool_write;
  mv_eth_port_tool->nlink = 1;
  return 0;
}

module_init(start_mv_eth_port_tool);
