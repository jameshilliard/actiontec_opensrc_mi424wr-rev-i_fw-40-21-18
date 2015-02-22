/****************************************************************************
 *
 * rg/vendor/jstream/jiwis8xx/modules/jiwis8xx.h
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

#ifndef _JIWIS8XX_H_

#define JIWIS8XX_LATCH_D0 0
#define JIWIS8XX_LATCH_D1 1
#define JIWIS8XX_LATCH_D2 2
#define JIWIS8XX_LATCH_D3 3
#define JIWIS8XX_LATCH_D4 4
#define JIWIS8XX_LATCH_D5 5
#define JIWIS8XX_LATCH_D6 6
#define JIWIS8XX_LATCH_D7 7
#define JIWIS8XX_LATCH_D8 8
#define JIWIS8XX_LATCH_D9 9
#define JIWIS8XX_LATCH_D10 10
#define JIWIS8XX_LATCH_D11 11
#define JIWIS8XX_LATCH_D12 12
#define JIWIS8XX_LATCH_D13 13
#define JIWIS8XX_LATCH_D14 14
#define JIWIS8XX_LATCH_D15 15

int jiwis8xx_latch_set(u8 latch, u8 line, u8 value);
int jiwis8xx_led_set(int led, int state);

#endif
