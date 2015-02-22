#!/bin/sh
TZ=GMT export TZ

ipsec spi --clear
ipsec eroute --clear

ipsec klipsdebug --set pfkey
ipsec klipsdebug --set verbose

ipsec eroute --add --eraf inet --src 192.1.2.23/32 --dst 192.0.1.0/24 --said %hold

ipsec tncfg --attach --virtual ipsec0 --physical eth1
ifconfig ipsec0 inet 192.1.2.23 netmask 0xffffff00 broadcast 192.1.2.255 up

arp -s 192.1.2.45 10:00:00:64:64:45
arp -s 192.1.2.254 10:00:00:64:64:45

ipsec look

# magic route command
route add -host 192.0.1.1 gw 192.1.2.45 dev ipsec0

ipsec ikeping --verbose --wait 2 --inet 192.0.1.1 192.0.1.1 192.0.1.1 192.0.1.1
