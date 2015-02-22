#
# use this script to undo effects of sourcing a "testparams.sh" into
# your shell, when testing.
#
# $Id: cleanup.sh,v 1.2 2003/11/30 14:30:45 derry Exp $
#
unset SCRIPT
unset REFCONSOLEOUTPUT 
unset PRIVINPUT
unset PUBINPUT
unset REFPRIVOUTPUT
unset REFPUBOUTPUT
unset TCPDUMPARGS

#
# $Log: cleanup.sh,v $
# Revision 1.2  2003/11/30 14:30:45  derry
# cvs_vendor_fixup: vendor branch fixup: 1.2 <-- 1.1, 1.1.1.1
# NOTIFY: cancel
#
# Revision 1.1.1.1  2003/02/19 11:46:31  sergey
# upgrading freeswan to ver. 1.99.
#
# Revision 1.3  2002/02/20 07:26:24  rgb
# Corrected de-pluralized variable names.
#
# Revision 1.2  2001/11/23 01:08:12  mcr
# 	pullup of test bench from klips2 branch.
#
# Revision 1.1.2.1  2001/10/23 04:43:18  mcr
# 	shell/testing cleaning script.
#
# 
