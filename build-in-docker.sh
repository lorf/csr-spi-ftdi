#!/bin/sh

set -xe

curl -sLO https://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.22/libusb-1.0.22.7z
7z x -olibusb-win32 libusb-1.0.22.7z

curl -sLO http://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2

LIBFTDI1_CMAKE_OPTS="-DBUILD_TESTS=OFF -DDOCUMENTATION=OFF -DEXAMPLES=OFF -DFTDI_EEPROM=OFF -DEXAMPLES=OFF -DPYTHON_BINDINGS=OFF -DLINK_PYTHON_LIBRARY=OFF"
LIBFTDI1_CMAKE_FLAGS_LINUX="-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32"

tar xjvf libftdi1-1.4.tar.bz2
cd libftdi1-1.4
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-w64-mingw32.cmake \
    -DLIBUSB_INCLUDE_DIR=../libusb-win32/include/libusb-1.0 \
    -DLIBUSB_LIBRARIES="-L../../libusb-win32/MinGW32/static -lusb-1.0" \
    -DCMAKE_INSTALL_PREFIX=../libftdi1-win32 $LIBFTDI1_CMAKE_OPTS .
make all install clean
cd ..
rm -rf libftdi-1.4

tar xjvf libftdi1-1.4.tar.bz2
cd libftdi1-1.4
cmake $LIBFTDI1_CMAKE_FLAGS_LINUX $LIBFTDI1_CMAKE_OPTS \
    -DCMAKE_INSTALL_PREFIX=../libftdi1-linux .
rm -f CMakeCache.txt
cmake $LIBFTDI1_CMAKE_FLAGS_LINUX $LIBFTDI1_CMAKE_OPTS \
    -DCMAKE_INSTALL_PREFIX=../libftdi1-linux .
make all install clean
cd ..
rm -rf libftdi-1.4

make zip

ls -l csr-spi-ftdi-*.zip
