#
# Makefile for the AT91RM9200-specific Linux kernel device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).

O_TARGET := at91drv.o

subdir-y :=	serial net watchdog rtc usb i2c spi mtd
subdir-m :=	$(subdir-y)

obj-$(CONFIG_SERIAL_AT91)		+= serial/at91serial.o
obj-$(CONFIG_AT91_ETHER)		+= net/at91net.o
obj-$(CONFIG_AT91_WATCHDOG)		+= watchdog/at91wdt.o
obj-$(CONFIG_AT91_RTC)			+= rtc/at91rtc.o
obj-$(CONFIG_USB)			+= usb/at91usb.o
obj-$(CONFIG_I2C_AT91)			+= i2c/at91i2c.o
obj-$(CONFIG_AT91_SPIDEV)		+= spi/at91spi.o
obj-$(CONFIG_MTD_AT91_DATAFLASH)	+= spi/at91spi.o mtd/at91mtd.o
obj-$(CONFIG_MTD_AT91_SMARTMEDIA)	+= mtd/at91mtd.o

include $(TOPDIR)/Rules.make
