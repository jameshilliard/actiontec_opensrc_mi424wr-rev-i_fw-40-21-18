/****************************************************************************
 *  Copyright (c) 2002 Jungo LTD. All Rights Reserved.
 * 
 *  rg/pkg/include/dyn_field.h
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

#ifndef _DYN_FIELD_H_
#define _DYN_FIELD_H_

/* Base interface for a dynamic opaque field.
 * Inheritors may augment it. The derived user structure MUST begin
 * with a 'dyn_field_t' member; 'copy' and 'destruct' MUST be implemented.
 */
typedef struct dyn_field_t {
    struct dyn_field_t *(*copy)(void *old);
    void (*destruct)(void *self);
} dyn_field_t;
    
#endif
