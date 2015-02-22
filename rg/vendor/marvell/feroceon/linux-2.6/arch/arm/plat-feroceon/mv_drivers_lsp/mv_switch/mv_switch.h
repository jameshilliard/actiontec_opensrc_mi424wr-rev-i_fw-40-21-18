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
#ifndef __mv_switch_h__
#define __mv_switch_h__ 

#define MV_SWITCH_PHY_ACCESS			1
#define MV_SWITCH_PORT_ACCESS			2
#define MV_SWITCH_GLOBAL_ACCESS			3
#define MV_SWITCH_GLOBAL2_ACCESS		4
#define MV_SWITCH_SMI_ACCESS                	5

#define MV_SWITCH_PORT_VLAN_ID(grp,port)  ((grp)+(port)+1)
#define MV_SWITCH_GROUP_VLAN_ID(grp)      (((grp)+1)<<8)
#define MV_SWITCH_VLAN_TO_GROUP(vid)      ((((vid) & 0xf00) >> 8)-1)

int     mv_switch_load(unsigned int switch_ports_mask); 
int     mv_switch_unload(unsigned int switch_ports_mask);
int     mv_switch_init(int mtu, unsigned int switch_ports_mask);
void    mv_switch_link_update_event(MV_U32 port_mask, int force_link_check);
int     mv_switch_jumbo_mode_set(int max_size);
int     mv_switch_tos_get(unsigned char tos);
int     mv_switch_tos_set(unsigned char tos, int queue);
int     mv_switch_port_based_vlan_set(unsigned int ports_mask, int set_cpu_port);
int     mv_switch_vlan_in_vtu_set(unsigned short vlan_id, unsigned short db_num, 
                                  unsigned int ports_mask);
int     mv_switch_mac_addr_set(unsigned char *mac_addr, unsigned char db, 
			       unsigned int ports_mask, unsigned char op);
int     mv_switch_atu_db_flush(int db_num);
int     mv_eth_switch_vlan_set(u16 vlan_grp_id, u16 port_map, u16 cpu_port);
int     mv_switch_promisc_set(u16 vlan_grp_id, u16 port_map, u16 cpu_port, u8 promisc_on);
void    mv_switch_link_detection_init(void);

int     mv_switch_reg_read(int port, int reg, int type, MV_U16* value);
int     mv_switch_reg_write(int port, int reg, int type, MV_U16 value);

void    mv_switch_stats_print(void);
void    mv_switch_status_print(void);

int     mv_switch_all_multicasts_del(int db_num);

int     mv_switch_port_add(int switch_port, u16 vlan_grp_id, u16 port_map);
int     mv_switch_port_del(int switch_port, u16 vlan_grp_id, u16 port_map);

#endif /* __mv_switch_h__ */
