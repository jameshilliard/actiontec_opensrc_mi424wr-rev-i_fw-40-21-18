/* Copyright(c) 2000, Compaq Computer Corporation 
 * Fibre Channel Host Bus Adapter 
 * 64-bit, 66MHz PCI 
 * Originally developed and tested on:
 * (front): [chip] Tachyon TS HPFC-5166A/1.2  L2C1090 ...
 *          SP# P225CXCBFIEL6T, Rev XC
 *          SP# 161290-001, Rev XD
 * (back): Board No. 010008-001 A/W Rev X5, FAB REV X5
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * Written by Don Zimmerman
*/
// These functions control the NVRAM I2C hardware on 
// non-intelligent Fibre Host Adapters.
// The primary purpose is to read the HBA's NVRAM to get adapter's 
// manufactured WWN to copy into Tachyon chip registers
// Orignal source author unknown

#include <linux/types.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/io.h>		// struct pt_regs for IRQ handler & Port I/O

#include "cpqfcTSchip.h"

static void tl_i2c_tx_byte(void *GPIOout, u8 data);

/*
 * Tachlite GPIO2, GPIO3 (I2C) DEFINES
 * The NVRAM chip NM24C03 defines SCL (serial clock) and SDA (serial data)
 * GPIO2 drives SDA, and GPIO3 drives SCL
 * 
 * Since Tachlite inverts the state of the GPIO 0-3 outputs, SET writes 0
 * and clear writes 1. The input lines (read in TL status) is NOT inverted
 * This really helps confuse the code and debugging.
 */
 
#define SET_DATA_HI		0x0
#define SET_DATA_LO		0x8
#define SET_CLOCK_HI		0x0
#define SET_CLOCK_LO		0x4

#define SENSE_DATA_HI		0x8
#define SENSE_DATA_LO		0x0
#define SENSE_CLOCK_HI		0x4
#define SENSE_CLOCK_LO		0x0

#define SLAVE_READ_ADDRESS	0xA1
#define SLAVE_WRITE_ADDRESS	0xA0


static void i2c_delay(u32 mstime);
static void tl_i2c_clock_pulse(u8, void *GPIOout);
static u8 tl_read_i2c_data(void *);


//-----------------------------------------------------------------------------
//
//      Name:   tl_i2c_rx_ack
//
//      This routine receives an acknowledge over the I2C bus.
//
//-----------------------------------------------------------------------------
static unsigned short tl_i2c_rx_ack(void *GPIOin, void *GPIOout)
{
	unsigned long value;

	// do clock pulse, let data line float high
	tl_i2c_clock_pulse(SET_DATA_HI, GPIOout);

	// slave must drive data low for acknowledge
	value = tl_read_i2c_data(GPIOin);
	if (value & SENSE_DATA_HI)
		return 0;

	return 1;
}

//-----------------------------------------------------------------------------
//
//      Name:   tl_read_i2c_reg
//
//      This routine reads the I2C control register using the global
//      IO address stored in gpioreg.
//
//-----------------------------------------------------------------------------
static u8 tl_read_i2c_data(void *gpioreg)
{
	return ((u8) (readl(gpioreg) & 0x08L));	// GPIO3
}

//-----------------------------------------------------------------------------
//
//      Name:   tl_write_i2c_reg
//
//      This routine writes the I2C control register using the global
//      IO address stored in gpioreg.
//      In Tachlite, we don't want to modify other bits in TL Control reg.
//
//-----------------------------------------------------------------------------
static void tl_write_i2c_reg(void *gpioregOUT, u8 value)
{
	u32 temp;

	// First read the register and clear out the old bits
	temp = readl(gpioregOUT) & 0xfffffff3L;

	// Now or in the new data and send it back out
	writel(temp | value, gpioregOUT);
	
	/* PCI posting ???? */
}

//-----------------------------------------------------------------------------
//
//      Name:   tl_i2c_tx_start
//
//      This routine transmits a start condition over the I2C bus.
//      1. Set SCL (clock, GPIO2) HIGH, set SDA (data, GPIO3) HIGH,
//      wait 5us to stabilize.
//      2. With SCL still HIGH, drive SDA low.  The low transition marks
//         the start condition to NM24Cxx (the chip)
//      NOTE! In TL control reg., output 1 means chip sees LOW
//
//-----------------------------------------------------------------------------
static unsigned short tl_i2c_tx_start(void *GPIOin, void *GPIOout)
{
	unsigned short i;
	u32 value;

	if (!(tl_read_i2c_data(GPIOin) & SENSE_DATA_HI)) {
		// start with clock high, let data float high
		tl_write_i2c_reg(GPIOout, SET_DATA_HI | SET_CLOCK_HI);

		// keep sending clock pulses if slave is driving data line
		for (i = 0; i < 10; i++) {
			tl_i2c_clock_pulse(SET_DATA_HI, GPIOout);

			if (tl_read_i2c_data(GPIOin) & SENSE_DATA_HI)
				break;
		}

		// if he's still driving data low after 10 clocks, abort
		value = tl_read_i2c_data(GPIOin);	// read status
		if (!(value & 0x08))
			return 0;
	}

	// To START, bring data low while clock high
	tl_write_i2c_reg(GPIOout, SET_CLOCK_HI | SET_DATA_LO);

	udelay(5);

	return 1;		// TX start successful
}

//-----------------------------------------------------------------------------
//
//      Name:   tl_i2c_tx_stop
//
//      This routine transmits a stop condition over the I2C bus.
//
//-----------------------------------------------------------------------------

static unsigned short tl_i2c_tx_stop(void *GPIOin, void *GPIOout)
{
	int i;

	for (i = 0; i < 10; i++) {
		// Send clock pulse, drive data line low
		tl_i2c_clock_pulse(SET_DATA_LO, GPIOout);

		// To STOP, bring data high while clock high
		tl_write_i2c_reg(GPIOout, SET_DATA_HI | SET_CLOCK_HI);

		// Give the data line time to float high
		udelay(5);

		// If slave is driving data line low, there's a problem; retry
		if (tl_read_i2c_data(GPIOin) & SENSE_DATA_HI)
			return 1;	// TX STOP successful!
	}

	return 0;		// error
}

//-----------------------------------------------------------------------------
//
//      Name:   tl_i2c_tx_byte
//
//      This routine transmits a byte across the I2C bus.
//
//-----------------------------------------------------------------------------
static void tl_i2c_tx_byte(void *GPIOout, u8 data)
{
	u8 bit;

	for (bit = 0x80; bit; bit >>= 1) {
		if (data & bit)
			tl_i2c_clock_pulse((u8) SET_DATA_HI, GPIOout);
		else
			tl_i2c_clock_pulse((u8) SET_DATA_LO, GPIOout);
	}
}

//-----------------------------------------------------------------------------
//
//      Name:   tl_i2c_rx_byte
//
//      This routine receives a byte across the I2C bus.
//
//-----------------------------------------------------------------------------
static u8 tl_i2c_rx_byte(void *GPIOin, void *GPIOout)
{
	u8 bit;
	u8 data = 0;


	for (bit = 0x80; bit; bit >>= 1) {
		// do clock pulse, let data line float high
		tl_i2c_clock_pulse(SET_DATA_HI, GPIOout);

		// read data line
		if (tl_read_i2c_data(GPIOin) & 0x08)
			data |= bit;
	}

	return (data);
}

//*****************************************************************************
//*****************************************************************************
// Function:   read_i2c_nvram
// Arguments:  u8 count     number of bytes to read
//             u8 *buf      area to store the bytes read
// Returns:    0 - failed
//             1 - success
//*****************************************************************************
//*****************************************************************************
unsigned long cpqfcTS_ReadNVRAM(void *GPIOin, void *GPIOout, u16 count, u8 * buf)
{
	unsigned short i;

	if (!(tl_i2c_tx_start(GPIOin, GPIOout)))
		return 0;

	// Select the NVRAM for "dummy" write, to set the address
	tl_i2c_tx_byte(GPIOout, SLAVE_WRITE_ADDRESS);
	if (!tl_i2c_rx_ack(GPIOin, GPIOout))
		return 0;

	// Now send the address where we want to start reading  
	tl_i2c_tx_byte(GPIOout, 0);
	if (!tl_i2c_rx_ack(GPIOin, GPIOout))
		return 0;

	// Send a repeated start condition and select the
	//  slave for reading now.
	if (tl_i2c_tx_start(GPIOin, GPIOout))
		tl_i2c_tx_byte(GPIOout, SLAVE_READ_ADDRESS);

	if (!tl_i2c_rx_ack(GPIOin, GPIOout))
		return 0;

	// this loop will now read out the data and store it
	//  in the buffer pointed to by buf
	for (i = 0; i < count; i++) {
		*buf++ = tl_i2c_rx_byte(GPIOin, GPIOout);

		// Send ACK by holding data line low for 1 clock
		if (i < (count - 1))
			tl_i2c_clock_pulse(0x08, GPIOout);
		else {
			// Don't send ack for final byte
			tl_i2c_clock_pulse(SET_DATA_HI, GPIOout);
		}
	}

	tl_i2c_tx_stop(GPIOin, GPIOout);

	return 1;
}

//****************************************************************
//
//
//
// routines to set and clear the data and clock bits
//
//
//
//****************************************************************

static void tl_set_clock(void *gpioreg)
{
	u32 ret_val;

	ret_val = readl(gpioreg);
	ret_val &= 0xffffffFBL;	// clear GPIO2 (SCL)
	writel(ret_val, gpioreg);
}

static void tl_clr_clock(void *gpioreg)
{
	u32 ret_val;

	ret_val = readl(gpioreg);
	ret_val |= SET_CLOCK_LO;
	writel(ret_val, gpioreg);
}

//*****************************************************************
//
//
// This routine will advance the clock by one period
//
//
//*****************************************************************
static void tl_i2c_clock_pulse(u8 value, void *GPIOout)
{
	u32 ret_val;

	// clear the clock bit
	tl_clr_clock(GPIOout);

	udelay(5);


	// read the port to preserve non-I2C bits
	ret_val = readl(GPIOout);

	// clear the data & clock bits
	ret_val &= 0xFFFFFFf3;

	// write the value passed in...
	// data can only change while clock is LOW!
	ret_val |= value;	// the data
	ret_val |= SET_CLOCK_LO;	// the clock
	writel(ret_val, GPIOout);

	udelay(5);


	//set clock bit
	tl_set_clock(GPIOout);
}




//*****************************************************************
//
//
// This routine returns the 64-bit WWN
//
//
//*****************************************************************
int cpqfcTS_GetNVRAM_data(u8 * wwnbuf, u8 * buf)
{
	u32 len;
	u32 sub_len;
	u32 ptr_inc;
	u32 i;
	u32 j;
	u8 *data_ptr;
	u8 z;
	u8 name;
	u8 sub_name;
	u8 done;
	int ret = 0;	// def. 0 offset is failure to find WWN field



	data_ptr = (u8 *) buf;

	done = 0;
	i = 0;

	while (i < 128 && !done) {
		z = data_ptr[i];
		if (!(z & 0x80)) {
			len = 1 + (z & 0x07);

			name = (z & 0x78) >> 3;
			if (name == 0x0F)
				done = 1;
		} else {
			name = z & 0x7F;
			len = 3 + data_ptr[i + 1] + (data_ptr[i + 2] << 8);

			switch (name) {
			case 0x0D:
				//
				j = i + 3;
				//
				if (data_ptr[j] == 0x3b) {
					len = 6;
					break;
				}

				while (j < (i + len)) {
					sub_name = (data_ptr[j] & 0x3f);
					sub_len = data_ptr[j + 1] + (data_ptr[j + 2] << 8);
					ptr_inc = sub_len + 3;
					switch (sub_name) {
					case 0x3C:
						memcpy(wwnbuf, &data_ptr[j + 3], 8);
						ret = j + 3;
						break;
					default:
						break;
					}
					j += ptr_inc;
				}
				break;
			default:
				break;
			}
		}
		//
		i += len;
	}			// end while 
	return ret;
}

