#
# Makefile for the SSI drivers
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now inherited from the
# parent makefile.
#

O_TARGET	:= ssi.o

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

export-objs	:=
list-multi	:=

obj-$(CONFIG_SSI)		+= ssi_core.o
obj-$(CONFIG_SSI_CLPS711X)	+= clps711x_ssi1.o
obj-y				+= juno.o

# Extract lists of the multi-part drivers.
# The 'int-*' lists are intermediate files used to build the multi's.

multi-y		:= $(filter $(list-multi), $(obj-y))
multi-m		:= $(filter $(list-multi), $(obj-m))
int-y		:= $(sort $(foreach m, $(multi-y), $($(basename $(m))-objs)))
int-m		:= $(sort $(foreach m, $(multi-m), $($(basename $(m))-objs)))

# Files that are both resident and modular; remove from modular.

obj-m		:= $(filter-out $(obj-y), $(obj-m))
int-m		:= $(filter-out $(int-y), $(int-m))

# Take multi-part drivers out of obj-y and put components in.

obj-y		:= $(filter-out $(list-multi), $(obj-y)) $(int-y)

# Translate to Rules.make lists.

O_OBJS		:= $(filter-out $(export-objs), $(obj-y))
OX_OBJS		:= $(filter     $(export-objs), $(obj-y))
M_OBJS		:= $(sort $(filter-out $(export-objs), $(obj-m)))
MX_OBJS		:= $(sort $(filter     $(export-objs), $(obj-m)))

include $(TOPDIR)/Rules.make
