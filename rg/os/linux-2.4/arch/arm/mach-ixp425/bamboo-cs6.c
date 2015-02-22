/****************************************************************************
 *
 * rg/os/linux-2.4/arch/arm/mach-ixp425/bamboo-cs6.c
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

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/sizes.h>
#include <asm/arch/ixp425.h>
#include <asm/arch/gpio.h>
#include <asm/arch/bamboo-cs6.h>

#define EXP_CS6_INIT   	0xbfff2c03

#define RST_MODULE  BIT(0)
#define RST_CARDBUS BIT(1)
#define RST_USB     BIT(2)
#define RST_SWITCH  BIT(3)

#ifdef DEBUG
#  define DBG(x...) printk(__FILE__": "x)
#else
#  define DBG(x...)
#endif

static u8 *cs6_addr;
static struct resource *cs6_resource;
static u8 cs6_state;

int cs6_bit_get(u8 bit)
{
    return (cs6_state & BIT(bit)) >> bit;
}

void cs6_bit_set(u8 bit)
{ 
    cs6_state |= BIT(bit);
    *cs6_addr = cs6_state;
}

void cs6_bit_clear(u8 bit)
{
    cs6_state &= ~BIT(bit); 
    *cs6_addr = cs6_state;
}

void cs6_bit_toggle(u8 bit)
{
    cs6_state ^= BIT(bit); 
    *cs6_addr = cs6_state;
}

static void cs6_cleanup(void)
{
    if (cs6_addr)
	iounmap((void *)cs6_addr);
    
    if (cs6_resource)
	release_mem_region(IXP425_EXP_BUS_CS6_BASE_PHYS, SZ_1M);
}

int __init cs6_init(void)
{    
    *IXP425_EXP_CS6 = EXP_CS6_INIT;

    cs6_resource = request_mem_region(IXP425_EXP_BUS_CS6_BASE_PHYS, 
	SZ_1M, "cs6");
    if (!cs6_resource)
    {
	printk(__FILE__":  request_mem_region() failed\n");
	goto Error;
    }

    cs6_addr = (u8 *)ioremap(IXP425_EXP_BUS_CS6_BASE_PHYS, SZ_1M);
    if (!cs6_addr)
    {
	printk(__FILE__": ioremap() failed\n");
	goto Error;
    }
    
    /* reset cardbus controller, usb, switch */
    cs6_state = RST_MODULE | RST_CARDBUS | RST_USB | RST_SWITCH;
    *cs6_addr = cs6_state;
    gpio_line_config(IXP425_GPIO_PIN_3, IXP425_GPIO_OUT);
    gpio_line_set(IXP425_GPIO_PIN_3, IXP425_GPIO_LOW);
    cs6_state &= ~(RST_MODULE | RST_CARDBUS | RST_USB);
#ifdef CONFIG_RG_RGLOADER
    /* XXX: fix switch reset in a way it could read configuration 
     * from EEPROM  
     */
    cs6_state &= ~RST_SWITCH;
#endif
    *cs6_addr = cs6_state;
    udelay(1000);
    cs6_state |= (RST_MODULE | RST_CARDBUS | RST_USB | RST_SWITCH);
    *cs6_addr = cs6_state;
    return 0;
    
Error:
    cs6_cleanup();
    return -1;
}

EXPORT_SYMBOL(cs6_bit_get);
EXPORT_SYMBOL(cs6_bit_set);
EXPORT_SYMBOL(cs6_bit_clear);
EXPORT_SYMBOL(cs6_bit_toggle);
