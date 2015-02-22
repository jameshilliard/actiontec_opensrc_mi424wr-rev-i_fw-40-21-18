/****************************************************************************
 *
 * rg/pkg/build/create_config.h
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

#ifndef _CREATE_CONFIG_H_
#define _CREATE_CONFIG_H_

#ifdef CONFIG_RG_DO_DEVICES
#include <enums.h>
#include <dev_type.h>
#endif

#define UNUSED_VAR(var) ({__typeof__(var) __attribute__ ((unused))x = var;})

extern char *major_features_file, *major_features_cfile;
extern int is_evaluation;

#ifdef CONFIG_RG_DO_DEVICES
void _dev_add(char *file, int line, char *name, dev_if_type_t type,
    logical_network_t net);
#define dev_add(name, type, net) _dev_add(__FILE__, __LINE__, name, type, net)
void _dev_add_switch_port(char *file, int line, char *switch_name, char *name, int port, logical_network_t net);
#define  dev_add_switch_port(switch_name, name, port, net)  _dev_add_switch_port(__FILE__, __LINE__, switch_name, name, port, net)

/* Add bridge. Enslaved devices are specified as a NULL terminated list of
 * device names. If the device list is empty, the enslaved devices are 
 * determined automatically, by enslaving all ethernet devices with the same 
 * logical network as bridge.
 */
void dev_add_bridge(char *name, logical_network_t net, ...);

/* Enslave a device to an existing bridge */
void dev_add_to_bridge_if_opt(char *name, char *enslaved, char *opt_verify);
void dev_add_to_bridge(char *bridge, char *enslaved);

void dev_set_dependency(char *dev_name, char *depend_on);
void dev_can_be_missing(char *dev_name);

void dev_open_conf_file(char *filename);
void dev_close_conf_file(void);
#else
#define _dev_add(...)
#define  dev_add(name, type, net)  UNUSED_VAR(name)
#define _dev_add_switch_port(...)
#define  dev_add_switch_port(switch_name, name, port, net)  UNUSED_VAR(name)
#define dev_add_bridge(...)
#define dev_add_to_bridge_if_opt(...)
#define dev_add_to_bridge(...)
#define dev_set_dependency(...)
#define dev_can_be_missing(...)
#define dev_open_conf_file(...)
#define  dev_close_conf_file()
#endif

/* If 'is_big' is set to 1 turn on the flags for a big endian target,
 * otherwise turn on the flags for a little endian target.
 */
void _set_big_endian(char *file, int line, int is_big);
#define set_big_endian(is_big) _set_big_endian(__FILE__, __LINE__, is_big)

void hardware_features(void);
void distribution_features(void);
void create_device_list(void);
void openrg_features(void);
void package_features(void);
void general_features(void);
void bridge_config(void);
void print_major_features(void);
void target_os_features(char *os);
void target_primary_os(void);
void config_host(void);

/* set value of LIC token by distribution or default
 * return the value set to LIC or NULL if none */
char *set_dist_license(void);

void conf_err(const char *format, ...)
    __attribute__((__format__(__printf__, 1, 2)));
char *sys_get(int *ret, char *command, ...)
    __attribute__((__format__(__printf__, 2, 3)));

/* Allow module to be compiled into distribution, if a license exist */
void _enable_module(char *file, int line, char *name);
#define enable_module(m) _enable_module(__FILE__, __LINE__, m)

typedef struct jpkg_dist_t {
    struct jpkg_dist_t *next;
    char *dist;
    option_t *options;
} jpkg_dist_t;

extern jpkg_dist_t *jpkg_dists;

void jpkg_dist_add(char *dist);

#endif
