#!/bin/bash

# detect the OS
detect_os()
{
    hostname=`uname -n`
    os=`uname`

    case "$os" in
	CYGWIN_*)
	    echo "#define CONFIG_RG_HOST_CYGWIN 1" >> $oc
	    HOSTARCH=WIN32
	    HOSTARCH_WIN=1
	    ;;
	Linux)
	    echo "#define CONFIG_RG_HOST_LINUX 1" >> $oc
	    HOSTARCH=Linux
	    HOSTARCH_UNIX=1
	    ;;
	SunOS)
	    echo "#define CONFIG_RG_HOST_SUNOS 1" >> $oc
	    HOSTARCH=SunOS
	    HOSTARCH_UNIX=1
	    ;;
	*)
	    echo "failed detecting OS $os"
	    exit 1
	    ;; 
    esac
}

set_feature()
{
    local config=$1 value="$2" _type="$3"
    if [ "$_type" == "" -o "$_type" == "m" ] ; then
	if [ -n "$value" ] ; then
	    echo $config=$value >> $om
        fi
    fi
    if [ "$_type" == "" -o "$_type" == "c" ] ; then
	if [ -n "$value" ] ; then
	    echo "#define $config $value" >> $oc
        else
	    echo "#undef $config" >> $oc
        fi
    fi
    eval $config=\"$value\"
}

detect_cc_feature()
{
    local config=$1 code_h="$2" code_c="$3" _type="$4"
    local value=1 tc=$TMPF.detect.c to=$TMPF.detect
    cat > $tc <<EOF
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdarg.h>
EOF
    echo $code_h >> $tc
    cat >> $tc <<EOF
    int main(int argc, char *argv[])
    {
EOF
    echo $code_c >> $tc
    cat >> $tc <<EOF
	return 0;
    }
EOF
    if $CC -Wall -Werror $tc -o $to >/dev/null 2>&1 ; then
        value=1
    else
        value=
    fi
    set_feature $config $value $_type
    rm -f $tc $to
}

detect_app()
{
    local config=$1 paths="$2" must_succeed=$3
    local a path=""
    for a in $paths ; do
	path="`which $a 2>/dev/null`"
	if [ -n "$path" ] ; then
	    break
        fi
    done
    if [ -n "$path" ] ; then
	set_feature $config $path m
	return 0
    fi
    if ((must_succeed)) ; then
	echo "failed locating $config in $2"
	exit 1
    fi
    return 1
}

detect_cc_version()
{
    ver=`$CC --version`
    case "$ver" in
	*cc\ \(GCC\)\ *)
	    CCVER=`echo $ver | sed 's/^.*cc (GCC) \([0-9.]*\) .*$/\1/'`
	;;
	*gcc-[0-9.]*)
	    CCVER=`echo $ver | sed 's/^gcc-\([0-9.]*\) .*$/\1/'`
	;;
	[0-9.]*)
	    CCVER=$ver
	;;
	*)
	    echo "failed detecting version of CC: '$ver'"
	    exit 1
    esac
    set_feature CCVER $CCVER m
}

detect_gcc_features()
{
    detect_app CC "/usr/local/openrg/i386-jungo-linux-gnu/bin/i386-jungo-linux-gnu-gcc cc gcc" 1
    detect_cc_version
    detect_cc_feature HAVE_ATOLL '' 'long long x = atoll("1");'
    detect_cc_feature HAVE_ALLOCA_H '#include <alloca.h>' ''
    detect_cc_feature HAVE_SEARCH_H '#include <search.h>' ''
    detect_cc_feature CONFIG_CC_FMTCHECK_ESC_SQL '' 'printf("%.s", "test");'
    detect_cc_feature HAVE_VA_COPY '' 'va_list args; va_copy(args, args_in);'
    detect_cc_feature HAVE_ATTRIBUTE_NONULL 'void f(char *) __attribute__((__nonull__(1)))' ''
    if [ -z "$HAVE_ATTRIBUTE_NONULL" ] ; then
        echo "#define __nonull__" >> $oc
    fi
    detect_cc_feature HAVE_ICONV '#include <iconv.h>' ''
    if [ "$HAVE_ICONV" == 1 ] ; then
        code="iconv_t cd; size_t avail, left; char *in, *out;"
        code="$code iconv(cd, (const char**) &in, &left, &out, &avail);"
        detect_cc_feature CONFIG_ICONV_CONST_IN '#include <iconv.h>' "$code"
        detect_cc_feature CONFIG_ICONV_NOLIB '#include <iconv.h>' \
          'iconv_open("", "");'
    fi
}

linux_package_install_instructions()
{
    local comp=$1
    local pkg=$2
    
    echo "To install $comp, open a command prompt as root and install the general linux package $pkg:"
    echo "Debian/Knoppix:"
    echo "    Type:"
    echo "    apt-get install $pkg"
    echo "RedHat/Mandrake/Fedora/Suse:"
    echo "    Download/locate $pkg-x.y.z-arch.rpm package."
    echo "    To find this package you can google '$pkg rpm'."
    echo "    After downloading, type:"
    echo "    rpm -Uvh $pkg-x.y.z-arch.rpm"
}

output_fake_configs()
{
    # define arbitrary target endianess - it doesn't matter since we don't
    # compile target
    echo "#define CONFIG_CPU_LITTLE_ENDIAN 1" >> $oc
}

output_results_init()
{
    oc=$OUTPUT_C
    om=$OUTPUT_M
    osh=$OUTPUT_SH
    rm -f $oc $om $osh
    echo "/* HOST C configuration */" >> $oc
    echo >> $oc
    echo "# HOST Makefile configuration" >> $om
    echo >> $om
}

output_results()
{
    if [ "$CONFIG_ICONV_CONST_IN" != 1 ] ; then
	echo '#define CONFIG_ICONV_CONST_CAST' >> $oc
    else
	echo '#define CONFIG_ICONV_CONST_CAST (const char **)' >> $oc
    fi
    echo "HOSTARCH=$HOSTARCH" >> $om
    if [ -n "$HOSTARCH_WIN" ] ; then
        echo "HOSTARCH_WIN=$HOSTARCH_WIN" >> $om
    fi
    if [ -n "$HOSTARCH_UNIX" ] ; then
        echo "HOSTARCH_UNIX=$HOSTARCH_UNIX" >> $om
    fi
    if [ $HOSTARCH = SunOS ] ; then
	echo 'LDLIBS:=-lnsl -lresolv -lsocket $(LDLIBS)' >> $om
    fi
    echo "#define HAVE_CTYPE_H 1" >> $oc
    echo "#define HAVE_STRING_H 1" >> $oc
    echo "#define HAVE_STRINGS_H 1" >> $oc
    echo "#define HAVE_STDLIB_H 1" >> $oc
    echo "#define __HOST__ 1" >> $oc
    sed 's/^\([A-Za-z][^=]*\)=\(.*\)$/\1="\2"/' $om > $osh
    if [ "$HAVE_ICONV" == 1 -a "$CONFIG_ICONV_NOLIB" != 1 ] ; then
	echo 'LDLIBS:=-liconv $(LDLIBS)' >> $om
    fi
}

usage()
{
    echo "Usage: detect_host output_c output_m output_sh"
    echo "envirnoment used:"
    echo '  $TMP: temporary directory. uses /tmp if not defined'
    exit 1
}

OUTPUT_C=$1
if [ "$1" == "" ] ; then
    usage
fi
shift
OUTPUT_M=$1
if [ "$1" == "" ] ; then
    usage
fi
shift
OUTPUT_SH=$1
if [ "$1" == "" ] ; then
    usage
fi
shift
if [ "$1" != "" ] ; then
    usage
fi

if [ -z "$TMP" ] ; then
    TMP=$TEMP
fi
if [ -z "$TMP" ] ; then
    TMP=/tmp
fi
TMPF=$TMP/$$.
output_results_init
detect_os
detect_gcc_features
output_fake_configs
output_results

exit 0

