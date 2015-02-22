#!/bin/sh

TEST_PURPOSE=regress
TEST_PROB_REPORT=0

TESTNAME=east-icmp-01
TESTHOST=east
EXITONEMPTY=--exitonempty
PRIVINPUT=../inputs/01-sunrise-sunset-ping.pcap
REFPUBOUTPUT=spi1-output.txt
REFCONSOLEOUTPUT=spi1-console.txt
REFCONSOLEFIXUPS="kern-list-fixups.sed nocr.sed"
REFCONSOLEFIXUPS="$REFCONSOLEFIXUPS klips-spi-sanitize.sed"
REFCONSOLEFIXUPS="$REFCONSOLEFIXUPS klips-debug-sanitize.sed"
REFCONSOLEFIXUPS="$REFCONSOLEFIXUPS ipsec-look-sanitize.sed"
TCPDUMPFLAGS="-n -E 3des-cbc-hmac96:0x4043434545464649494a4a4c4c4f4f515152525454575758"
SCRIPT=spi1.sh



