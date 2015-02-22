TITLE: Asterisk quick proof-of-concept test
RELATED_DOCS:
WHY:
STATUS: WORK IN PROGRESS
AUTHOR: amirl
OWNER: amirl
NOTIFY: amirl

What is this
------------
Asterisk is a PBX software, that supports SIP client and server, MGCP server
(no MGCP client support... yet), and H323 (via OpenH323 C++ library).

This was a quick attempt to run Asterisk on a PCBOX configuration, with
an Internet Phone Jack (IXJ) as FXS.

It is not connected to maintask yet.

Setup
-----
- PC
- 2 RTL adapters
- 1 Internet PhoneJack adapter
- 1 analog telephone

Compilation
-----------
- set your board's WAN IP address manually in sip.conf:
  # g pkg/voip/asterisk/configs/sip.conf
  modify the line 'bindaddr=' to contain the correct IP address

- configure speed dials for the peers you're going to call:
  # g pkg/voip/asterisk/configs/extensions.conf
  add speed dials in the bottom of the file, use existing speed dials as an
  example.
  
- build the image:
  cvs up -A # it is commited on MAIN (OpenRG 4.1)
  make config CONFIG_IXJ=y CONFIG_RG_VOIP_ASTERISK=y DIST=ALLWELL_RTL_RTL
  make

- burn the image 
  OpenRG> load -u tftp://.../openrg.img

- reboot the image

- login to OpenRG:
  OpenRG> restore_defaults
  OpenRG> fw_stop
  OpenRG> shell
  # asterisk -dvvvvvvvvv

- now you should hear a dial tone, and can dial your speed dials or accept
  calls.

Things that were done to get it working
---------------------------------------
- removed 'n's from extensions.conf because asterisk thought they were invalid
- disabled call to verify_key() because it failed
- disabled calls to various DNS related functions that were missing from our
  version of ulibc
- manually configured WAN IP SIP should bind to because Asterisk couldn't
  figure it out by itself.

TODO
----
- Review changes made for the proof-of-conecpt against the original version
  that was brought to dev and try to minimize changes and fix things in a
  cleaner way.

- Review make files and clean them up.

Missing features in Asterisk
----------------------------
- no off hook warning
- Cannot act as an MG, only MGC
- SIP:
  - blind transfer works using #. 
- chan_zip.c
  - has 3-way calling, flash drops last person
  - has call waiting
  - has consultation call

Problems
--------
- linkage to dynamic libpthread.so caused strange things such as
  data loss/memory overruns on thread creation/removal.
  The problem was resolved by statically linking libpthread.a.

- running valgrind on a process that has threads: (diff on pkg/valgrind)
amirl@al valgrind$ cvs -q diff
Index: Makefile
===================================================================
RCS file: /arch/cvs/rg/pkg/valgrind/Makefile,v
retrieving revision 1.9
diff -r1.9 Makefile
6c6
< SO_TARGET=valgrind.so valgrinq.so
---
> SO_TARGET=valgrind.so valgrinq.so vg_libpthread.so
25a26,27
> O_OBJS_vg_libpthread.so=vg_libpthread.o
>
Index: vg_libpthread.c
===================================================================
RCS file: /arch/cvs/rg/pkg/valgrind/vg_libpthread.c,v
retrieving revision 1.3
diff -r1.3 vg_libpthread.c
58d57
< #define __USE_UNIX98
2368,2371d2366
< #ifndef HAVE_NFDS_T
< typedef unsigned long int nfds_t;
< #endif
<

