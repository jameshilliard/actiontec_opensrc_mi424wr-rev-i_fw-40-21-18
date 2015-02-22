#! /bin/sh

TARGET_PLATFORM=$1

TARGET_PROVIDER=$2

TARGET_BCM=../../../../../targets/96368GW/fs.install

case $TARGET_PROVIDER in
telus)

TARGET_BCM=../../../../../targets/96368BGW/fs.install
;;

*)
;;
esac


case $TARGET_PLATFORM in
att|ATT)
cp dbus/libdbus.so ../../../../../targets/96328GW/fs.install/lib/
cp bus/dbus-daemon ../../../../../targets/96328GW/fs.install/bin/
cp tools/dbus-launch ../../../../../targets/96328GW/fs.install/bin/
cp tools/dbus-send ../../../../../targets/96328GW/fs.install/bin/
cp tools/dbus-monitor ../../../../../targets/96328GW/fs.install/bin/
cp tools/dbus-cleanup-sockets ../../../../../targets/96328GW/fs.install/bin/
cp tools/dbus-uuidgen ../../../../../targets/96328GW/fs.install/bin/
mkdir ../../../../../targets/96328GW/fs.install/etc/dbus
mkdir ../../../../../targets/96328GW/fs.install/etc/dbus/run
mkdir ../../../../../targets/96328GW/fs.install/etc/dbus/session.d
mkdir ../../../../../targets/96328GW/fs.install/etc/dbus/system.d
cp etc/system.conf ../../../../../targets/96328GW/fs.install/etc/dbus/
cp etc/session.conf ../../../../../targets/96328GW/fs.install/etc/dbus/
;;

bcm|BCM) 
cp dbus/libdbus.so $TARGET_BCM/lib/
cp bus/dbus-daemon $TARGET_BCM/bin/
cp tools/dbus-launch $TARGET_BCM/bin/
cp tools/dbus-send $TARGET_BCM/bin/
cp tools/dbus-monitor $TARGET_BCM/bin/
cp tools/dbus-cleanup-sockets $TARGET_BCM/bin/
cp tools/dbus-uuidgen $TARGET_BCM/bin/
mkdir $TARGET_BCM/etc/dbus
mkdir $TARGET_BCM/etc/dbus/run
mkdir $TARGET_BCM/etc/dbus/session.d
mkdir $TARGET_BCM/etc/dbus/system.d
cp etc/system.conf $TARGET_BCM/etc/dbus/
cp etc/session.conf $TARGET_BCM/etc/dbus/
;;

bhr2|BHR2) echo "bhr2"
;;

bhr2_refi|BHR2_REFI) echo "bhr2 refi"
;;

x86|X86) echo "x86"
;;

*)echo "Please choose a platform"
;;
esac

