#!/bin/bash -e -x

LIB_DIR="`pwd`/sane-libs"

cd sane-backends

OPT_FLAGS="-O0 -g3"
MAKE_JOBS=1

dobuild() {
    export CC="$(xcrun -find -sdk ${SDK} cc)"
    export CXX="$(xcrun -find -sdk ${SDK} cxx)"
    export CPP="$(xcrun -find -sdk ${SDK} cpp)"
    export CFLAGS="${HOST_FLAGS} ${OPT_FLAGS}"
    export CXXFLAGS="${HOST_FLAGS} ${OPT_FLAGS}"
    export LDFLAGS="${HOST_FLAGS}"

    ./configure --host=${CHOST} --prefix=${PREFIX} --enable-static --disable-shared --disable-libusb --disable-warnings --enable-pthread --disable-local-backends --enable-silent-rules

    make clean
    make -j${MAKE_JOBS}
    make install
}

SDK="iphoneos"
ARCH_FLAGS="-arch armv7"
HOST_FLAGS="${ARCH_FLAGS} -miphoneos-version-min=6.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
CHOST="arm-apple-darwin"
PREFIX="${LIB_DIR}/armv7"
dobuild

SDK="iphoneos"
ARCH_FLAGS="-arch armv7s"
HOST_FLAGS="${ARCH_FLAGS} -miphoneos-version-min=6.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
CHOST="arm-apple-darwin"
PREFIX="${LIB_DIR}/armv7s"
dobuild

SDK="iphoneos"
ARCH_FLAGS="-arch arm64"
HOST_FLAGS="${ARCH_FLAGS} -miphoneos-version-min=8.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
CHOST="armv64-apple-darwin"
PREFIX="${LIB_DIR}/arm64"
dobuild

#SDK="iphonesimulator"
#ARCH_FLAGS="-arch i386"
#HOST_FLAGS="${ARCH_FLAGS} -mios-simulator-version-min=8.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
#CHOST="i386-apple-darwin"
#PREFIX="${LIB_DIR}/sim86"
#dobuild

#SDK="iphonesimulator"
#ARCH_FLAGS="-arch x86_64"
#HOST_FLAGS="${ARCH_FLAGS} -mios-simulator-version-min=7.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
#CHOST="x86_64-apple-darwin"
#PREFIX="${LIB_DIR}/sim64"
#dobuild

##merge libs
[ -d "${LIB_DIR}/all" ] || mkdir "${LIB_DIR}/all"

merge() {
    lipo -create "${LIB_DIR}/armv7/lib/$1/$2" "${LIB_DIR}/armv7s/lib/$1/$2" "${LIB_DIR}/arm64/lib/$1/$2" -output "${LIB_DIR}/all/$2"
}

merge '.'    'libsane.a'
merge 'sane' 'libsane-dll.a'
merge 'sane' 'libsane-net.a'

cp "${LIB_DIR}/armv7/include/sane/sane.h"     "${LIB_DIR}/all"
cp "${LIB_DIR}/armv7/include/sane/saneopts.h" "${LIB_DIR}/all"


