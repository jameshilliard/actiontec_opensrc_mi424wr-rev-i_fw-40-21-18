#
# Makefile for misc devices that really don't fit anywhere else.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

O_TARGET := misc.o

export-objs			:= mcp-core.o mcp-sa1100.o ucb1x00-core.o

obj-$(CONFIG_MCP)		+= mcp-core.o
obj-$(CONFIG_MCP_SA1100)	+= mcp-sa1100.o
obj-$(CONFIG_MCP_UCB1200)	+= ucb1x00-core.o
obj-$(CONFIG_MCP_UCB1200_AUDIO)	+= ucb1x00-audio.o
obj-$(CONFIG_MCP_UCB1200_TS)	+= ucb1x00-ts.o

include $(TOPDIR)/Rules.make

fastdep:
