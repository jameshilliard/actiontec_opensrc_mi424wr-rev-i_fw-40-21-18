#! /bin/sh

CROSS=/usr/local/openrg/arm-none-linux-gnueabi/

export PATH=$PATH:$CROSS/bin/

./configure --prefix=/usr/local/openrg/arm-none-linux-gnueabi/ --host=arm-none-linux-gnueabi --without-python --without-iconv --with-minimum --with-tree --with-writer --with-debug #--without-threads --without-thread-alloc
cp config.h.bhr2 config.h
make

cp -f .libs/libxml2.a ../../../target/bhr2/lib/


