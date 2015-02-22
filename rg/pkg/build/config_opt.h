/****************************************************************************
 *
 * rg/pkg/build/config_opt.h
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

#ifndef _CONFIG_OPT_H_
#define _CONFIG_OPT_H_

#include <stdio.h>
#ifdef CONFIG_RG_DO_DEVICES
#include <enums.h>
#endif

typedef enum {
    OPT_NONE = 0,
    OPT_MODULE_EXPAND = 0x1, /* can be expanded to module, i.e. option_MODULE */
    /* Link statically when CONFIG_RG_DEV is on */
    OPT_STATIC_ON_DEVELOP = 0x2,
    OPT_NOSTRIP = 0x4, /* don't strip according to this flag */
    OPT_MAJOR_FEATURE = 0x8,
    OPT_STRIP_ON_Y = 0x10,
    OPT_THEME = 0x20,
    OPT_MODULE = 0x40,
    OPT_HARDWARE = 0x80,
    /* If none of these flags (OPT_H | OPT_MK) is defined then
     * create_config assumes all of them are defined. 
     */
    OPT_H = 0x100, /* Print the CONFIG_xxx to rg_config.h */
    OPT_HC = 0x200, /* Print the cCONFIG_xxx to rg_config.h */
    OPT_MK = 0x400, /* Print the CONFIG_xxx to rg_config.mk */
    OPT_INT = 0x800, /* Internal use, cannot be configure from command line */
    OPT_STR = 0x1000, /* The value is a string */
    OPT_C_STR = 0x2000, /* The value is a string when exported to C */
    OPT_NUMBER = 0x4000, /* The value is a number when exported to C */
    OPT_EXPORT = 0x8000, /* The CONFIG is exported (i.e: export CONFIG_XXX) */
} opt_type_t;

/* Set priority. The largest number is the highest priority (e.g:
 * SET_PRIO_CMD_LINE is stronger than SET_PRIO_TOKEN_SET) */
typedef enum {
    /* The option was set in initialization (i.e:
     * ...
     * { "CONFIG_RG_XXX", "y", OPT_HC },
     * ...
     */
    SET_PRIO_INIT = 1,
    /* The option was set by calling token_set_default() */
    SET_PRIO_TOKEN_SET_DEFAULT = 2,
    /* The option was set by calling token_set() */
    SET_PRIO_TOKEN_SET = 3,
    /* The option was set from the command line */
    SET_PRIO_CMD_LINE = 4,
} set_prio_t;

/* TODO: Do we want to add a type (STRING, BOOL...) to this? */
typedef struct {
    char *token;
    char *value;
    opt_type_t type;
    char *description;
    char *file;
    int line;
    set_prio_t set_prio;
} option_t;

extern option_t openrg_distribution_options[];
extern option_t openrg_hw_options[];
extern option_t openrg_os_options[];
extern option_t *openrg_config_options;
extern option_t config_options_base[];
extern char *hw, *os, *dist, *features;

/* handle both decimal and hex numbers */
int str_is_number_value(char *val);
int str_to_number(char *val);

/* Retrun a pointer to the token 'token' in the openrg_config_options array */
option_t *option_token_get_nofail(option_t *array, char *token);
option_t *option_token_get(option_t *array, char *token);
/* Return the value of token in the openrg_config_options array or NULL if
 * token does not exist.
 */
char *token_get_str(char *token);
/* return 1 if the token is set to "y" or "m", 0 otherwise */
int token_get(char *token);
/* return 1 if the token is set to "y", 0 otherwise */
int token_is_y(char *token);
/* Return the value of token in the openrg_config_options array or "" if
 * token does not exist.
 */
char *token_getz(char *token);
/* Set value of token 'token' */
void _token_set(char *file, int line, set_prio_t set_prio, char *token,
    const char *value_, ...)
  __attribute__((__format__(__printf__, 5, 6)));

#define token_set(token, value, args...) \
    _token_set(__FILE__, __LINE__, SET_PRIO_TOKEN_SET, token, value, ##args)

/* Same as token_set, but with lower priority (if there are calls to
 * token_set() with the same token as in the call to token_set_default() - they
 * will override the call to token_set_default()) */
#define token_set_default(token, value, args...) \
    _token_set(__FILE__, __LINE__, SET_PRIO_TOKEN_SET_DEFAULT, token, \
	value, ##args)
  
/* Set value of token to "y" */
void _token_set_y(char *file, int line, char *token);
#define token_set_y(token) _token_set_y(__FILE__, __LINE__, token)
/* Set value of token to "m" */
void _token_set_m(char *file, int line, char *token);
#define token_set_m(token) _token_set_m(__FILE__, __LINE__, token)

/* Define the device type as a config, so we'll know it is in use. */
#ifdef CONFIG_RG_DO_DEVICES
void _token_set_dev_type(char *file, int line, dev_if_type_t type);
#define token_set_dev_type(type) _token_set_dev_type(__FILE__, __LINE__, type)
#else
#define token_set_dev_type(...)
#endif

/* Convert a number to a (static) string */
char *itoa(int num);

/* The local host can't fail malloc function */
#define malloc_e(n) malloc(n)

#define IS_HW(x) (hw && !strcmp(hw, x))
#define IS_DIST(x) (!strcmp(dist, x))

#endif
