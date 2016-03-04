#!/bin/bash -e -x

# to show all actions use /bin/bash -e -x

# vim ./sane-backends/sanei/sanei_ir.c
# replace #include <values.h> with #include <limits.h> and #include <float.h>

# grep -rnw . -e "CROSS_COMPILING"
#   for each file : remove the test on CROSS_COMPILING to use the case where we are
#   if not the x86 simulator build won't be working

# find . -name Makefile.in -delete
# automake


LIB_DIR="`pwd`/sane-libs"

cd sane-backends

OPT_FLAGS="-O3 -g3"
MAKE_JOBS=4
VERBOSE=0


# Arguments for dobuild
#    IS_SIM :   0 to build for real device, 1 to build for simulator
#    ARCH :    architecture to build for
#    MIN_SDK : minimum iOS version to build for
#    CHOST :   target host full name

dobuild ()
{
    IS_SIM=$1
    ARCH=$2
    MIN_SDK=$3
    CHOST=$4

    if [ $IS_SIM -eq 1 ]; then
        SDK="iphonesimulator"
        MIN_SDK="ios-simulator-version-min=${MIN_SDK}"
    else
        SDK="iphoneos"
        MIN_SDK="iphoneos-version-min=${MIN_SDK}"
    fi

    HOST_FLAGS="-arch ${ARCH} -m${MIN_SDK} -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"

    export CC="$(xcrun -find -sdk ${SDK} cc)"
    export CXX="$(xcrun -find -sdk ${SDK} cxx)"
    export CPP="$(xcrun -find -sdk ${SDK} cpp)"
    export CFLAGS="${HOST_FLAGS} ${OPT_FLAGS}"
    export CXXFLAGS="${HOST_FLAGS} ${OPT_FLAGS}"
    export LDFLAGS="${HOST_FLAGS}"

    ./configure --host=${CHOST} --prefix="${LIB_DIR}/${ARCH}" --enable-static --disable-shared --disable-libusb --disable-warnings --enable-pthread --disable-local-backends --enable-silent-rules

    make clean
    make V=${VERBOSE} -j${MAKE_JOBS}
    make install
}


dobuild 0 "armv7"  6.0 "arm-apple-darwin"

dobuild 0 "armv7s" 6.0 "arm-apple-darwin"

dobuild 0 "arm64"  7.0 "armv64-apple-darwin"

dobuild 1 "i386"   6.0 "i386-apple-darwin"

dobuild 1 "x86_64" 7.0 "x86_64-apple-darwin"

##merge libs
[ -d "${LIB_DIR}/all" ] || mkdir "${LIB_DIR}/all"

merge() {
    lipo -create "${LIB_DIR}/armv7/lib/$1/$2" "${LIB_DIR}/armv7s/lib/$1/$2" "${LIB_DIR}/arm64/lib/$1/$2" "${LIB_DIR}/i386/lib/$1/$2" "${LIB_DIR}/x86_64/lib/$1/$2" -output "${LIB_DIR}/all/$2"
}

merge '.'    'libsane.a'
merge 'sane' 'libsane-dll.a'
merge 'sane' 'libsane-net.a'

cp "${LIB_DIR}/armv7/include/sane/sane.h"     "${LIB_DIR}/all"
cp "${LIB_DIR}/armv7/include/sane/saneopts.h" "${LIB_DIR}/all"


