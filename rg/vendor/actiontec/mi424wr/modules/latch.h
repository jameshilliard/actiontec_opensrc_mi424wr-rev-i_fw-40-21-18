/****************************************************************************
 *
 * rg/vendor/actiontec/mi424wr/modules/latch.h
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

/* default latch value
 * 0  - power alarm led (red)		0 (off)
 * 1  - power led (green)		0 (off)
 * 2  - wireless led	(green)		1 (off)
 * 3  - no internet led (red)		0 (off)
 * 4  - internet ok led (green)		0 (off)
 * 5  - moca LAN			0 (off)
 * 6  - N/A
 * 7  - PCI reset			1 (not reset)
 * 8  - IP phone 1 led (green)		1 (off)
 * 9  - IP phone 2 led (green)		1 (off)
 * 10 - VOIP ready led (green)		1 (off)
 * 11 - PSTN relay 1 control		0 (PSTN)
 * 12 - PSTN relay 1 control		0 (PSTN)
 * 13 - N/A
 * 14 - N/A
 * 15 - N/A
 */
#define MI424WR_LATCH_DEFAULT		0x1f84

#define MI424WR_LATCH_ALARM_LED		0x00
#define MI424WR_LATCH_POWER_LED		0x01
#define MI424WR_LATCH_WIRELESS_LED	0x02
#define MI424WR_LATCH_INTET_DOWN_LED	0x03
#define MI424WR_LATCH_INTET_OK_LED	0x04
#define MI424WR_LATCH_MOCA_LAN_LED	0x05
#define MI424WR_LATCH_PCI_RESET		0x07
#define MI424WR_LATCH_PHONE1_LED	0x08
#define MI424WR_LATCH_PHONE2_LED	0x09
#define MI424WR_LATCH_VOIP_LED		0x10
#define MI424WR_LATCH_PSTN_RELAY1	0x11
#define MI424WR_LATCH_PSTN_RELAY2	0x12

/* initialize CS1 to default timings, Intel style, 16bit bus */
#define MI424WR_CS1_CONFIG 0x80003c42

void mi424wr_latch_set(u8 line, u8 value);

