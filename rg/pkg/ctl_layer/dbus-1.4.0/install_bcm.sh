#! /bin/sh

cp dbus/libdbus.so ../../../../../targets/96368GW/fs.install/lib/
cp bus/dbus-daemon ../../../../../targets/96368GW/fs.install/bin/
cp tools/dbus-launch ../../../../../targets/96368GW/fs.install/bin/
cp tools/dbus-send ../../../../../targets/96368GW/fs.install/bin/
cp tools/dbus-monitor ../../../../../targets/96368GW/fs.install/bin/
cp tools/dbus-cleanup-sockets ../../../../../targets/96368GW/fs.install/bin/
cp tools/dbus-uuidgen ../../../../../targets/96368GW/fs.install/bin/
mkdir ../../../../../targets/96368GW/fs.install/etc/dbus
mkdir ../../../../../targets/96368GW/fs.install/etc/dbus/run
mkdir ../../../../../targets/96368GW/fs.install/etc/dbus/session.d
mkdir ../../../../../targets/96368GW/fs.install/etc/dbus/system.d
cp etc/system.conf ../../../../../targets/96368GW/fs.install/etc/dbus/
cp etc/session.conf ../../../../../targets/96368GW/fs.install/etc/dbus/

