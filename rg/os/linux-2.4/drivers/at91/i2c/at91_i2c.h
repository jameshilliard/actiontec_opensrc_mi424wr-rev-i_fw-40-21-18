/*
    i2c Support for Atmel's AT91RM9200 Two-Wire Interface

    (c) Rick Bronson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef AT91_I2C_H
#define AT91_I2C_H

#define AT91C_TWI_CLOCK		100000
#define AT91C_TWI_SCLOCK	(10 * AT91C_MASTER_CLOCK / AT91C_TWI_CLOCK)
#define AT91C_TWI_CKDIV1	(1 << 16)	/* TWI clock divider */

#if (AT91C_TWI_SCLOCK % 10) >= 5
#define AT91C_TWI_CLDIV2 ((AT91C_TWI_SCLOCK / 10) - 5)
#else
#define AT91C_TWI_CLDIV2 ((AT91C_TWI_SCLOCK / 10) - 6)
#endif
#define AT91C_TWI_CLDIV3 ((AT91C_TWI_CLDIV2 + (4 - AT91C_TWI_CLDIV2 % 4)) >> 2)

#define AT91C_EEPROM_I2C_ADDRESS        (0x50 << 16)

/* Physical interface */
struct at91_i2c_local {
	struct i2c_adapter adapter;
	unsigned long base_addr;
};

#endif
