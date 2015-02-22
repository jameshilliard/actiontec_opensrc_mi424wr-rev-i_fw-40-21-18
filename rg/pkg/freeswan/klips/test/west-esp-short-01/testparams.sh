#!/bin/sh

TEST_TYPE=klipstest
TESTNAME=west-esp-short-01
TESTHOST=west
EXITONEMPTY=--exitonempty
ARPREPLY=--arpreply 

PUBINPUT=../inputs/01-sunrise-sunset-esp-short.pcap
REFPRIVOUTPUT=spi1-cleartext.txt
REFCONSOLEOUTPUT=spi1-console.txt
REFCONSOLEFIXUPS="kern-list-fixups.sed nocr.sed"
REFCONSOLEFIXUPS="$REF_CONSOLE_FIXUPS klips-spi-sanitize.sed"
REFCONSOLEFIXUPS="$REF_CONSOLE_FIXUPS ipsec-look-sanitize.sed"
REFCONSOLEFIXUPS="$REF_CONSOLE_FIXUPS east-prompt-splitline.pl"
REFCONSOLEFIXUPS="$REF_CONSOLE_FIXUPS klips-debug-sanitize.sed"
TCPDUMPFLAGS="-n -E 3des-cbc-hmac96:0x4043434545464649494a4a4c4c4f4f515152525454575758"
SCRIPT=spi1-in.sh

#NETJIGDEBUG=true


