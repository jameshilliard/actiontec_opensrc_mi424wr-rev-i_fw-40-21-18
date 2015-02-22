/*
 *  linux/include/asm-arm/arch-ixp425/ide.h
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 *
 * derived from:
 * linux/include/asm-arm/arch-shark/ide.h
 * Copyright (c) 2003 Barry Dallavalle
 */

#ifndef _ARCH_IXP425_IDE_H_
#define _ARCH_IXP425_IDE_H_

#if defined(__KERNEL__) && defined(CONFIG_ARCH_IXP425_COYOTE)

#include <asm/arch/ixp425.h>
#include <asm/arch/gpio.h>
#include <asm/arch/irqs.h>

#define CUSTOM_IDE_IO
#undef MAX_HWIFS
#define MAX_HWIFS 1 /* Only one HWIF */

#define IDE_REG(p) *((volatile u16 *)(IXP425_EXP_BUS_CS3_BASE_VIRT + (p)))

static __inline__ u8 custom_ide_inb(unsigned long port)
{
    return (u8)IDE_REG(port);
}

static __inline__ u16 custom_ide_inw(unsigned long port)
{
    return IDE_REG(port);
}

static __inline__ void custom_ide_insw(unsigned long port, void *addr,
    u32 count)
{
    u16 *dst = (u16 *)addr;

    while (count--)
	*dst++ = IDE_REG(port);
}

static __inline__ u32 custom_ide_inl(unsigned long port)
{
    printk("ide_inl not implemented");
    BUG();

    return 0;
}

static __inline__ void custom_ide_insl(unsigned long port, void *addr,
    u32 count)
{
    custom_ide_insw(port, addr, count << 1);
}

static __inline__ void custom_ide_outb(u8 val, unsigned long port)
{
    IDE_REG(port) = (u16)val;
}

#define custom_ide_outbsync(d,v,p) custom_ide_outb(v,p)

static __inline__ void custom_ide_outw(u16 val, unsigned long port)
{
    IDE_REG(port) = val;
}

static __inline__ void custom_ide_outsw(unsigned long port, void *addr,
    u32 count)
{
    u16 *dst = addr;

    while (count--)
	IDE_REG(port) = *dst++;
}

static __inline__ void custom_ide_outl(u32 val, unsigned long port)
{
    printk("ide_outl not implemented");
    BUG();
}

static __inline__ void custom_ide_outsl(unsigned long port, void *addr,
    u32 count)
{
    custom_ide_outsw(port, addr, count << 1);
}

static __inline__ void custom_ide_fix_driveid(struct hd_driveid *id)
{
    char temp;
    short s_temp;

    /* swap the two byte fields */
    temp = id->max_multsect;
    id->max_multsect = id->vendor3;
    id->vendor3 = temp;

    temp = id->capability;
    id->capability = id->vendor4;
    id->vendor4 = temp;

    temp = id->tPIO;
    id->tPIO = id->vendor5;
    id->vendor5 = temp;

    temp = id->tDMA;
    id->tDMA = id->vendor6;
    id->vendor6 = temp;

    temp = id->multsect;
    id->multsect = id->multsect_valid;
    id->multsect_valid = temp;

    /* swap the words of any 4 byte field */
    s_temp = id->cur_capacity0;
    id->cur_capacity0 = id->cur_capacity1;
    id->cur_capacity1 = s_temp;

    id->lba_capacity = ((id->lba_capacity & 0xffff) << 16) |
	(id->lba_capacity >> 17);
}

static __inline__ void ide_init_hwif_ports(hw_regs_t *hw,
    ide_ioreg_t data_port, ide_ioreg_t ctrl_port, int *irq)
{
    ide_ioreg_t reg = data_port;
    int i;

    /* Allow 16 bit RW access to expansion bus chipset 3 */
    *IXP425_EXP_CS3 = 0xbfff0002;

    gpio_line_config(IXP425_GPIO_PIN_5,
	IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_HIGH);
    gpio_line_isr_clear(IXP425_GPIO_PIN_5);

    for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++)
    {
	hw->io_ports[i] = reg;
	reg += 2;
    }
    hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
    if (irq != NULL)
	*irq = 0;
}

static __inline__ void ide_init_default_hwifs(void)
{
    hw_regs_t hw;

    ide_init_hwif_ports(&hw, 0xe0, 0xfc, NULL);
    hw.irq = IRQ_IXP425_GPIO5;
    ide_register_hw(&hw, NULL);
}

#endif /* KERNEL && CONFIG_ARCH_IXP425_COYOTE */
#endif /* _ARCH_IXP425_IDE_H_ */
