#ident @(#)Makefile	1.2 01/07/20 
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

INSDIR=		sbin
TARGET=		rmt
CPPOPTS +=	-DUSE_REMOTE
CPPOPTS +=	-DUSE_LARGEFILES
CFILES=		rmt.c
#HFILES=
LIBS=		-ldeflt -lschily $(LIB_SOCKET)
XMK_FILE=	Makefile.man

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.cmd
###########################################################################
count:	$(CFILES) $(HFILES)
	count $r1
