#ident %W% %E% %Q%
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

INSDIR=		bin
TARGET=		mt
HARDLINKS=	smt
CPPOPTS +=	-I../star/ -DUSE_REMOTE
CFILES=		mt.c remote.c
#HFILES=
LIBS=		-lschily $(LIB_SOCKET)
#XMK_FILE=	Makefile.man

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.cmd
###########################################################################
count:	$(CFILES) $(HFILES)
	count $r1
