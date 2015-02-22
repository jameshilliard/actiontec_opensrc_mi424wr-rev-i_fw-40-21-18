#
# ptlib.mak
#
# mak rules to be included in a ptlib application Makefile.
#
# Portable Windows Library
#
# Copyright (c) 1993-1998 Equivalence Pty. Ltd.
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Portable Windows Library.
#
# The Initial Developer of the Original Code is Equivalence Pty. Ltd.
#
# Portions are Copyright (C) 1993 Free Software Foundation, Inc.
# All Rights Reserved.
# 
# Contributor(s): ______________________________________.
#
# $Log: ptlib.mak,v $
# Revision 1.4  2006/04/26 10:15:38  olegy
# B31918 - importing new 1.10.0 pwlib, 1.18.0 openh323 versions
#
# Revision 1.3  2005/08/14 09:56:51  dmitry
# B25117. Merged pwlib version 1_9_0 from vendor branch.
#
# Revision 1.2.2.1  2005/08/14 09:51:20  dmitry
# Added pwlib version 1.9.0 from pwlib-v1_9_0-src-tar.gz
#
# Revision 1.2.2.2  2006/04/16 11:40:20  olegy
# B31918 - update pwlib version to 1.10.0
#
# Revision 1.5  2005/08/14 13:12:42  csoutheren
# Fixed misleading error message from make when PWLIBDIR not defined
#
# Revision 1.4  2005/02/23 21:29:52  dominance
# have configure check for bison as we know we'll need it and stop implicit definition of PWLIBDIR. *geesh* that was about time, eh? ;)
#
# Revision 1.3  1998/12/02 02:37:31  robertj
# New directory structure.
#
# Revision 1.2  1998/09/24 04:20:52  robertj
# Added open software license.
#

ifndef PWLIBDIR

$(error No PWLIBDIR environment variable defined! \
You need to define PWLIBDIR! \
Try something like: \
PWLIBDIR = $(HOME)/pwlib)

endif

include $(PWLIBDIR)/make/unix.mak
include $(PWLIBDIR)/make/common.mak

# End of ptlib.mak
