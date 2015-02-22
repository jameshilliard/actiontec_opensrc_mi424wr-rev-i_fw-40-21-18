/****************************************************************************
 *
 * rg/os/linux-2.4/include/asm-arm/arch-ixp425/bamboo-cs6.h
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

#ifndef _ASM_ARCH_BAMBOO_CS6_H_
#define _ASM_ARCH_BAMBOO_CS6_H_

#include <asm/types.h>

int cs6_bit_get(u8 bit);
void cs6_bit_set(u8 bit);
void cs6_bit_clear(u8 bit);
void cs6_bit_toggle(u8 bit);
    
#endif
