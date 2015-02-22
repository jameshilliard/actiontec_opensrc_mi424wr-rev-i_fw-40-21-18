#!/bin/sh
TOOLDIR=~/usr/kaffe-1.1.6/bin
TARGET=mini_rt
CLASSES=${TARGET}_classlist.txt
C_LIST=
for i in `cat $CLASSES` ; do $TOOLDIR/javac $i.java ; done
for i in `cat $CLASSES` ; do C_LIST="$i*.class $C_LIST"; done
zip -q -DX -r $TARGET.jar $C_LIST
