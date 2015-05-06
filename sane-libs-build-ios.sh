#!/bin/bash -e -x

LIB_DIR="`pwd`/sane-libs"

cd sane-backends

OPT_FLAGS="-Os -g3"
MAKE_JOBS=16

dobuild() {
    export CC="$(xcrun -find -sdk ${SDK} cc)"
    export CXX="$(xcrun -find -sdk ${SDK} cxx)"
    export CPP="$(xcrun -find -sdk ${SDK} cpp)"
    export CFLAGS="${HOST_FLAGS} ${OPT_FLAGS}"
    export CXXFLAGS="${HOST_FLAGS} ${OPT_FLAGS}"
    export LDFLAGS="${HOST_FLAGS}"

    ./configure --host=${CHOST} --prefix=${PREFIX} --enable-static --disable-shared BACKENDS="net"

    make clean
    make -j${MAKE_JOBS}
    make install
}

SDK="iphoneos"
ARCH_FLAGS="-arch armv7"
HOST_FLAGS="${ARCH_FLAGS} -miphoneos-version-min=7.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
CHOST="arm-apple-darwin"
PREFIX="${LIB_DIR}/armv7"
dobuild

SDK="iphoneos"
ARCH_FLAGS="-arch arm64"
HOST_FLAGS="${ARCH_FLAGS} -miphoneos-version-min=8.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
CHOST="armv64-apple-darwin"
PREFIX="${LIB_DIR}/arm64"
dobuild

#SDK="iphonesimulator"
#ARCH_FLAGS="-arch i386"
#HOST_FLAGS="${ARCH_FLAGS} -mios-simulator-version-min=7.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
#CHOST="i386-apple-darwin"
#PREFIX="${LIB_DIR}/sim86"
#dobuild

#SDK="iphonesimulator"
#ARCH_FLAGS="-arch x86_64"
#HOST_FLAGS="${ARCH_FLAGS} -mios-simulator-version-min=7.0 -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"
#CHOST="x86_64-apple-darwin"
#PREFIX="${LIB_DIR}/sim64"
#dobuild
