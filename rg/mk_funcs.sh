#!/bin/bash

panic()
{
	echo "Panic: $1" 1>&2
	exit 1
}

rg_vpath_cp()
{
    src=$1
    dst=$2
    dst_dir=${dst%/*}
    
    if [ ! -d $dst_dir ] ; then
        echo "mkdir -p $dst_dir"
        mkdir -p $dst_dir || return 1
    fi
    if [ "${src:0:1}" == / ] ; then
        src_path=$src
    elif [ -e $PWD_SRC/$src ] ; then
        src_path=$PWD_SRC/$src
    else
        src_path=$PWD_BUILD/$src
    fi

    if [ ! -z "$DO_LINK" ] && [ "$src_path" == "$dst" ] ; then
	#echo $0: $dst already linked
	return 0
    fi

    if [ -e $dst ] ; then
        rm $dst || return 1
    fi

    if [ -n "$DO_LINK" ]; then 
        rg_lnf $src_path $dst || return 1
    else
        $BUILDDIR/pkg/build/export_src $src_path $dst
    fi
}

rg_lnf()
{
    if [ -e $1 ] ; then
        ln -sfn $1 $2
    fi
}

