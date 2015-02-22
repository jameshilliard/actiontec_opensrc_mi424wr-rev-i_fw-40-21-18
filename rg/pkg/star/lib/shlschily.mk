#ident @(#)shlschily.mk	1.2 00/02/10 
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
CCOM=		cc
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

SUBARCHDIR=	/pic
.SEARCHLIST:	. $(ARCHDIR) stdio $(ARCHDIR)
VPATH=		.:stdio:$(ARCHDIR)
INSDIR=		lib
TARGETLIB=	schily
CPPOPTS +=	-Istdio
include		Targets
LIBS=		

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.shl
###########################################################################
