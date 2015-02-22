#!/bin/sh

#
# $Id: runme.sh,v 1.2 2003/11/30 14:30:51 derry Exp $
#
# use this script to run a single test from within that test directory.
# note that this assumes a "klipstest" type test.
#

. ../../../umlsetup.sh
. ../setup.sh
. ../functions.sh
. testparams.sh

if [ -z "${TEST_TYPE}" ]
then
    echo runme.sh now requires that testparams.sh defines TEST_TYPE=
fi

( cd .. && $TEST_TYPE $TESTNAME good )

# $Log: runme.sh,v $
# Revision 1.2  2003/11/30 14:30:51  derry
# cvs_vendor_fixup: vendor branch fixup: 1.2 <-- 1.1, 1.1.1.1
# NOTIFY: cancel
#
# Revision 1.1.1.1  2003/02/19 11:46:31  sergey
# upgrading freeswan to ver. 1.99.
#
# Revision 1.3.2.1  2002/11/03 04:38:43  mcr
# 	port of runme.sh script
#
# Revision 1.2  2002/05/23 14:26:39  mcr
# 	verify that $TEST_TYPE is actually set.
#
# Revision 1.1  2002/05/05 23:12:05  mcr
# 	runme.sh script now common for all test types.
#
# 

