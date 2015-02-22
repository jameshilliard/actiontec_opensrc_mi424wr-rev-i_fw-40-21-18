# File: drivers/at91/spi/Makefile
#
# Makefile for the Atmel AT91RM9200 SPI device drivers
#

O_TARGET := at91spi.o

export-objs := at91_spi.o

obj-y	:= at91_spi.o
obj-m	:=
obj-n	:=
obj-	:=

obj-$(CONFIG_AT91_SPIDEV) += at91_spidev.o

include $(TOPDIR)/Rules.make
