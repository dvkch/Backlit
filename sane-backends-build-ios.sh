#!/bin/bash -e
# to show all actions use /bin/bash -e -x

# Created this way
# - download sane-backends archive
# - modified configure.in to always set CROSS_COMPILING to 1
# - modified sanei/sanei_ir.c to replace #include <values.h> with #include <limits.h> and #inlude <float.h>
# - created patch with git format-patch HEAD^ --stdout > ios_crosscompiling.patch

CUR_DIR="`pwd`"
LIB_DIR="`pwd`/sane-libs"
SRC_DIR="`pwd`/sane-backends"
LOG_PATH="`pwd`/sane-log.txt"
BACKENDS_VERSION="sane-backends-1.0.25"

OPT_FLAGS="-O3 -g3"
MAKE_JOBS=4
VERBOSE=0

cleanup() {
    if [ $? -eq 0 ]; then
        # No error
        echo "All done!";
    else
        # Other errors
        echo_bold "An error has been encountered, please take a look at the log file "
        echo_bold "=> ${LOG_PATH} "
    fi
}
trap cleanup EXIT

# Prints string with bold black color
echo_bold ()
{
    echo "${1}"
#    echo "$(tput setaf 15)$(tput setab 0) ${1} $(tput sgr 0)"
}

# Will download the chosen sane-backends version, then patch
# it to prevent errors when cross-compiling for iOS
download_and_patch_sources ()
{
    echo_bold "Downloading ${BACKENDS_VERSION}.tar.gz"
    rm -rf "${SRC_DIR}"
    curl -O "https://alioth.debian.org/frs/download.php/file/4146/${BACKENDS_VERSION}.tar.gz" >> "${LOG_PATH}" 2>&1

    echo_bold "Extracting ${BACKENDS_VERSION}.tar.gz"
    tar -xzf "${BACKENDS_VERSION}.tar.gz"
    rm "${BACKENDS_VERSION}.tar.gz"

    mv "${BACKENDS_VERSION}" "sane-backends"

    echo_bold "Applying patch to ${BACKENDS_VERSION}"
    cp sane-backends-ios-crosscompiling.patch sane-backends
    cd sane-backends
    patch -p1 < sane-backends-ios-crosscompiling.patch >> "${LOG_PATH}" 2>&1
    rm sane-backends-ios-crosscompiling.patch
    autoconf
    echo
}

# Arguments for dobuild
#    IS_SIM    0 to build for real device, 1 to build for simulator
#    ARCH      architecture to build for
#    MIN_SDK   minimum iOS version to build for
#    CHOST     target host full name
dobuild ()
{
    IS_SIM=$1
    ARCH=$2
    MIN_SDK=$3
    CHOST=$4

    echo_bold "Using architecture ${ARCH}, minimum iOS version: ${MIN_SDK}"
    echo_bold "   => Configuring"

    if [ $IS_SIM -eq 1 ]; then
        SDK="iphonesimulator"
        MIN_SDK="ios-simulator-version-min=${MIN_SDK}"
    else
        SDK="iphoneos"
        MIN_SDK="iphoneos-version-min=${MIN_SDK}"
    fi

    HOST_FLAGS="-arch ${ARCH} -m${MIN_SDK} -isysroot $(xcrun -sdk ${SDK} --show-sdk-path)"

    #export CXX="$(xcrun -find -sdk ${SDK} cxx)"
    export CC="$(xcrun -find -sdk ${SDK} cc)"
    export CPP="$(xcrun -find -sdk ${SDK} cpp)"
    export CFLAGS="${HOST_FLAGS} ${OPT_FLAGS}"
    export CXXFLAGS="${HOST_FLAGS} ${OPT_FLAGS}"
    export LDFLAGS="${HOST_FLAGS}"

    ./configure --host=${CHOST} --prefix="${LIB_DIR}/${ARCH}" --enable-static --disable-shared --disable-libusb --disable-warnings --enable-pthread --disable-local-backends --enable-silent-rules >> "${LOG_PATH}" 2>&1

    echo_bold "   => Building"
    make clean                          >> "${LOG_PATH}" 2>&1
    make V=${VERBOSE} -j${MAKE_JOBS}    >> "${LOG_PATH}" 2>&1
    make install                        >> "${LOG_PATH}" 2>&1
    echo
}

# Arguments for merge_libs
#   DIR     directory where the library file can be found
#   LIB     file name of the library
merge_libs()
{
    DIR=$1
    LIB=$2

    echo_bold "Merging libraries of different arch to single file at ${LIB_DIR}/all/${LIB}"

    lipo -create "${LIB_DIR}/armv7/lib/${DIR}/${LIB}" "${LIB_DIR}/armv7s/lib/${DIR}/${LIB}" "${LIB_DIR}/arm64/lib/${DIR}/${LIB}" "${LIB_DIR}/i386/lib/${DIR}/${LIB}" "${LIB_DIR}/x86_64/lib/${DIR}/${LIB}" -output "${LIB_DIR}/all/${LIB}"
    echo
}


## Main script
echo
echo_bold "Welcome to sane-backends build script to generate iOS libs, hope all goes well for you!"
echo_bold "This tools needs the developer command line tools to be installed, as well as autoconf"
echo

if [ -f "${LOG_PATH}" ] ; then
    rm "${LOG_PATH}"
fi

# Dowloads backend if not present
[ -d "${SRC_DIR}" ] || download_and_patch_sources

# Build for real devices then simulators
cd "${SRC_DIR}"
dobuild 0 "armv7"  6.0 "arm-apple-darwin"
dobuild 0 "armv7s" 6.0 "arm-apple-darwin"
dobuild 0 "arm64"  7.0 "armv64-apple-darwin"
dobuild 1 "i386"   6.0 "i386-apple-darwin"
dobuild 1 "x86_64" 7.0 "x86_64-apple-darwin"

## Merge libs
[ -d "${LIB_DIR}/all" ] || mkdir "${LIB_DIR}/all"
merge_libs '.'    'libsane.a'
merge_libs 'sane' 'libsane-dll.a'
merge_libs 'sane' 'libsane-net.a'

# Copy header files
cp "${LIB_DIR}/armv7/include/sane/sane.h"     "${LIB_DIR}/all"
cp "${LIB_DIR}/armv7/include/sane/saneopts.h" "${LIB_DIR}/all"

