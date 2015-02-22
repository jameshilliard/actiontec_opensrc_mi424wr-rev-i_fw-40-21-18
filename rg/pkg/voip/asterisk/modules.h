/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/voip/asterisk/modules.h
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

#ifndef _MODULES_H_
#define _MODULES_H_

#define MODULE_FUNCTIONS_NO_RELOAD(name) \
    char *name##_description(void); \
    char *name##_key(void); \
    int name##_load_module(void); \
    int name##_unload_module(void); \
    int name##_usecount(void);

#define MODULE_FUNCTIONS_WITH_RELOAD(name) \
    char *name##_description(void); \
    char *name##_key(void); \
    int name##_reload(void); \
    int name##_load_module(void); \
    int name##_unload_module(void); \
    int name##_usecount(void);

#define M_WITH_RELOAD(name) {#name, name##_load_module, name##_unload_module, name##_reload, name##_description, name##_usecount, name##_key}

#define M_NO_RELOAD(name) {#name, name##_load_module, name##_unload_module, NULL, name##_description, name##_usecount, name##_key}

#include <prototypes.h>

typedef struct {
    char *name;
    int (*load_module)(void);
    int (*unload_module)(void);
    int (*reload)(void);
    char *(*description)(void);
    int (*usecount)(void);
    char *(*key)(void);
} module_functions_t;

module_functions_t modules[] = {
#include <ast_modules.h>
    {NULL}
};

#endif

