/****************************************************************************
 *
 * rg/pkg/util/rg_error.h
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
/* SYNC: rg/pkg/util/rg_error.h <-> project/tools/util/rg_error.h */

#ifndef _RG_ERROR_H_
#define _RG_ERROR_H_

#define OS_INCLUDE_STDARG
#include <os_includes.h>
#include <cc_config.h>
#include "log_entity_id.h"

/* see 'man 2 syslog' and 'man 3 syslog' for LOG_xxx and KERN_xxx loglevel
 * definitions */
#define LLEVEL_MASK 0xf
#define LEMERG 0
#define LALERT 1
#define LCRIT 2
#define LERR 3
#define LWARNING 4
#define LNOTICE 5
#define LINFO 6
#define LDEBUG 7

/* From here on it acts as a bit field. */
#define LFLAGS_MASK 0xf00
#define LCONSOLE 0x100
#define LDOEXIT 0x200
#define LDOREBOOT 0x400
#define LDONT_FILTER 0x800 /* Don't filter the event (ignore its priority) */

/* 16 most significant bits are user defined */
#define LUSER_MASK 0xFFFF0000

#ifdef ACTION_TEC_PERSISTENT_LOG
#define SYSLOG_PERMISSION_CH_FMT	"PERMISSION_CH-"
#define SYSLOG_USER_ADD_FMT	"USER_ADD-"
#define SYSLOG_USER_DEL_FMT	"USER_REMOVE-"
#define SYSLOG_USER_CH_FMT	"USER_CH-"
#define SYSLOG_SECURITY_FMT "SECURITY-"
#endif /* ACTION_TEC_PERSISTENT_LOG */


typedef unsigned int rg_error_level_t;

#define LEXIT (LCRIT | LDOEXIT | LCONSOLE)
#define LPANIC (LEMERG | LDOEXIT | LCONSOLE)
#define LWARN LWARNING

#ifndef CONFIG_LSP_DIST
extern char *rg_error_level_str[];

typedef void (*rg_error_func_t)(void *data, char *msg,
    log_entity_id_t entity_id, rg_error_level_t level);

/* For backwards compatibility reasons */
typedef void (*rg_error_level_func_t)(void *data, char *msg,
    rg_error_level_t level);

/* The space before the ',' is important. Dont del it ! */
#define rg_error(SEVERITY, FMT, ARGS...) \
    rg_error_full(ENTITY_ID, SEVERITY, FMT , ## ARGS)
#define rg_error_f(SEVERITY, FMT, ARGS...) \
    rg_error_full(ENTITY_ID, SEVERITY, "%s:%d: " FMT, __FUNCTION__, \
	__LINE__ , ## ARGS)
#define rg_error_full_f(ENTITY_ID, SEVERITY, FMT, ARGS...) \
    rg_error_full(ENTITY_ID, SEVERITY, "%s:%d: " FMT, __FUNCTION__, \
	__LINE__ , ## ARGS)
#define rg_verror(SEVERITY, FMT, AP) \
    rg_verror_full(ENTITY_ID, SEVERITY, FMT, AP)
#define rg_perror(SEVERITY, FMT, ARGS...) \
    rg_perror_full(ENTITY_ID, SEVERITY, FMT , ## ARGS)
#define rg_vperror(SEVERITY, FMT, AP) \
    rg_vperror_full(ENTITY_ID, SEVERITY, FMT, AP)

/* Every registered function gets a reference to every log message from the
 * level specified and up.
 */
void rg_error_exit_reboot_set(void);

/* Set openrg thread_id for rg_error */
void rg_error_set_mt_id(void);

/* Every registered function gets a reference to every log message from the
 * level specified and up.
 */
void default_error_cb(void *data, char *msg, rg_error_level_t level);

void rg_error_register(int sequence, rg_error_func_t func, void *data);
void rg_error_unregister(rg_error_func_t func, void *data);

/* Similar to rg_error_register(), but the function it registers is
 * of type rg_error_level_func_t. This was added especially for backwards
 * compatibility reasons */
void rg_error_register_level(int sequence, rg_error_level_t level,
    rg_error_level_func_t func, void *data);
void rg_error_unregister_level(rg_error_level_func_t func, void *data);

#ifdef ACTION_TEC
extern int glb_aggr_log;
#endif
int rg_error_full(log_entity_id_t entity_id, rg_error_level_t severity,
    const char *format, ...)__attribute__((__format__(__printf__, 3, 4)));
int rg_verror_full(log_entity_id_t entity_id, rg_error_level_t severity,
    const char *format, va_list ap)__attribute__((nonnull(4)));
int rg_perror_full(log_entity_id_t entity_id, rg_error_level_t severity,
    const char *format, ...)__attribute__((__format__(__printf__, 3, 4)));
int rg_vperror_full(log_entity_id_t entity_id, rg_error_level_t severity,
    const char *format, va_list ap)__attribute__((nonnull(4)));

#ifdef RG_ERROR_INTERNALS
/* Deliver log message directly, must be used only within openrg */
void log_msg_deliver(log_entity_id_t entity_id, rg_error_level_t severity,
    char *msg);
#endif

void rg_error_init(int _reboot_on_exit, int _reboot_on_panic,
    int strace, char *process, int (*_main_addr)(int, char *[]),
    const char *_main_name);
void rg_error_init_default(rg_error_level_t level);
void rg_error_init_nodefault(void);

#ifdef ACTION_TEC_PERSISTENT_LOG

#define PLOG_WAN_ETH 8
#define PLOG_PPPOE_CREDENTIAL 15
#define PLOG_WIRELESS_CREDENTIAL 16
#define PLOG_SELF_DIAG_TEST 17

typedef void (*persistent_log_func_t)(void *data, char *msg, 
            log_entity_id_t entity_id, rg_error_level_t level, int flag);

int p_log(log_entity_id_t entity_id, int flag, const char *format, ...)
            __attribute__((__format__(__printf__, 3, 4)));
void persisent_log_register(persistent_log_func_t func, void *data);
void persisent_log_unregister(void);
#endif /* ACTION_TEC_PERSISTENT_LOG */

#else
/* LSP */
#include <stdio.h>
#define rg_error(lev, s...) ( { printf("<%d> ", lev); printf(s); -1; } )
#define rg_perror(lev, s...) ( { rg_error(lev, s); } )
#define rg_error_f(lev, s...) ( { rg_error(lev, s); } )
#endif
#ifdef __KERNEL__
extern int (*rg_error_logdev_hook)(char *msg, int exact_len);
#endif

#endif

