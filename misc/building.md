**Table of Contents**

* [Building](#building)
   * [Building Wine DLL on 32-bit Debian/Ubuntu Linux](#building-wine-dll-on-32-bit-debianubuntu-linux)
   * [Building Wine DLL on 64-bit Debian/Ubuntu Linux](#building-wine-dll-on-64-bit-debianubuntu-linux)
   * [Installing for Wine](#installing-for-wine)
   * [Cross-compiling DLL for Windows on Debian/Ubuntu using MinGW](#cross-compiling-dll-for-windows-on-debianubuntu-using-mingw)

# Building

These instructions may be outdated.

## Building Wine DLL on 32-bit Debian/Ubuntu Linux

Install build tools:

    sudo apt-get install -y build-essential pkg-config cmake wget

Install development libraries:

    sudo apt-get install -y wine-dev libc6-dev libstdc++-dev libusb-1.0-0-dev libudev-dev

Build fresh libftdi from source:

    wget http://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2
    tar xjvf libftdi1-1.4.tar.bz2
    cd libftdi1-1.4
    cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_INSTALL_PREFIX=../libftdi1-linux .
    make all install
    cd ..

Build with command:

    make -f Makefile.wine all


## Building Wine DLL on 64-bit Debian/Ubuntu Linux

Install build tools:

    sudo apt-get install -y build-essential pkg-config gcc-multilib g++-multilib cmake wget

Install 32 bit stuff:

    sudo dpkg --add-architecture i386
    sudo apt-get update
    sudo apt-get install -y wine-dev:i386 libc6-dev-i386 libstdc++-dev:i386 libusb-1.0-0-dev:i386 libudev-dev:i386

Build fresh libftdi from source:

    wget http://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2
    tar xjvf libftdi1-1.4.tar.bz2
    cd libftdi1-1.4
    cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 \
        -DCMAKE_INSTALL_PREFIX=../libftdi1-linux .
    make all install
    cd ..

Build with command:

    make -f Makefile.wine all

## Installing for Wine

Install CSR BlueSuite in Wine. Find all instances of usbspi.dll installed and
move them out of the way:

    find ~/.wine -iname usbspi.dll -exec mv {} {}.orig \;

Install Wine dll into the Wine libraries directory:

    sudo make -f Makefile.wine install

Alternately You can specify location of the .dll.so file in WINEDLLPATH
environment variable, see wine(1) man page for details.


## Cross-compiling DLL for Windows on Debian/Ubuntu using MinGW

Install MinGW cross-development environment:

    sudo apt-get install -y mingw-w64 cmake p7zip-full wget

Download [precompiled libusb for
windows](http://sourceforge.net/projects/libusb/files/) and extract it to the
libusb directory:

    wget https://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.22/libusb-1.0.22.7z
    7z x -olibusb-win32 libusb-1.0.22.7z

Build [libftdi](http://www.intra2net.com/en/developer/libftdi/) from source:

    wget http://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2
    tar xjvf libftdi1-1.4.tar.bz2
    cd libftdi1-1.4
    cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-w64-mingw32.cmake \
        -DLIBUSB_INCLUDE_DIR=../libusb-win32/include/libusb-1.0 \
        -DLIBUSB_LIBRARIES="-L../../libusb-win32/MinGW32/static -lusb-1.0" \
        -DCMAKE_INSTALL_PREFIX=../libftdi1-win32 .
    make all install
    cd ..

Build with command:

    make -f Makefile.mingw all
