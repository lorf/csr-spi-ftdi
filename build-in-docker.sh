#!/bin/sh
#
# Intended to be run inside the build Docker container, see
# <https://github.com/lorf/csr-spi-ftdi-build-image>. Can also be used
# directly, provided all the build dependencies are installed.
#

set -xe

LIBUSB_VERSION="1.0.22"
LIBFTDI1_VERSION="1.4"
LIBFTDI1_CMAKE_OPTS="-DBUILD_TESTS=OFF -DDOCUMENTATION=OFF -DFTDI_EEPROM=OFF -DEXAMPLES=OFF -DPYTHON_BINDINGS=OFF -DLINK_PYTHON_LIBRARY=OFF"
LIBFTDI1_CMAKE_FLAGS_LINUX="-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32"

if ! [ -f "dlcache/libusb-$LIBUSB_VERSION.7z" ]; then
    (mkdir -p dlcache && \
        cd dlcache && \
        curl -sLO "https://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-$LIBUSB_VERSION/libusb-$LIBUSB_VERSION.7z")
fi

if ! [ -f "dlcache/libftdi1-$LIBFTDI1_VERSION.tar.bz2" ]; then
    (mkdir -p dlcache && \
        cd dlcache && \
        curl -sLO "http://www.intra2net.com/en/developer/libftdi/download/libftdi1-$LIBFTDI1_VERSION.tar.bz2")
fi

7z x -y -olibusb-win32 "dlcache/libusb-$LIBUSB_VERSION.7z"

tar xjvf "dlcache/libftdi1-$LIBFTDI1_VERSION.tar.bz2"
cd "libftdi1-$LIBFTDI1_VERSION"
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-w64-mingw32.cmake \
    -DLIBUSB_INCLUDE_DIR=../libusb-win32/include/libusb-1.0 \
    -DLIBUSB_LIBRARIES="-L../../libusb-win32/MinGW32/static -lusb-1.0" \
    -DCMAKE_INSTALL_PREFIX=../libftdi1-win32 $LIBFTDI1_CMAKE_OPTS .
make all install clean
cd ..
rm -rf "libftdi1-$LIBFTDI1_VERSION"

tar xjvf "dlcache/libftdi1-$LIBFTDI1_VERSION.tar.bz2"
cd "libftdi1-$LIBFTDI1_VERSION"
cmake $LIBFTDI1_CMAKE_FLAGS_LINUX $LIBFTDI1_CMAKE_OPTS \
    -DCMAKE_INSTALL_PREFIX=../libftdi1-linux .
# Restart the configuration, see
# https://gitlab.kitware.com/cmake/cmake/issues/17616
rm -f CMakeCache.txt
cmake $LIBFTDI1_CMAKE_FLAGS_LINUX $LIBFTDI1_CMAKE_OPTS \
    -DCMAKE_INSTALL_PREFIX=../libftdi1-linux .
make all install clean
cd ..
rm -rf "libftdi1-$LIBFTDI1_VERSION"

make zip

ls -l csr-spi-ftdi-*.zip
