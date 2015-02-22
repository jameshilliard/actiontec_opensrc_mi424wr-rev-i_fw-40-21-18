#ident @(#)libschily_p.mk	1.2 00/02/10 
###########################################################################
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

SUBARCHDIR=	/profiled
.SEARCHLIST:	. $(ARCHDIR) stdio $(ARCHDIR)
VPATH=		.:stdio:$(ARCHDIR)
INSDIR=		lib
TARGETLIB=	schily_p
CPPOPTS +=	-Istdio
COPTS +=	$(COPTGPROF)
include		Targets
LIBS=		

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.lib
###########################################################################
