#ident @(#)libschily.mk	1.2 00/02/10 
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

.SEARCHLIST:	. $(ARCHDIR) stdio $(ARCHDIR)
VPATH=		.:stdio:$(ARCHDIR)
INSDIR=		lib
TARGETLIB=	schily
CPPOPTS +=	-Istdio
include		Targets
LIBS=		

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.lib
###########################################################################
