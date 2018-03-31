**Table of Contents**

* [CSR BlueCore USB SPI programmer/debugger](#csr-bluecore-usb-spi-programmerdebugger)
   * [CSR chips supported by programmer](#csr-chips-supported-by-programmer)
      * [Chip notes](#chip-notes)
   * [Programmer hardware](#programmer-hardware)
      * [Using FT232RL breakout board as a programmer](#using-ft232rl-breakout-board-as-a-programmer)
      * [Dedicated programmer](#dedicated-programmer)
      * [Supported FTDI chips](#supported-ftdi-chips)
      * [Counterfeit FT232RL chips](#counterfeit-ft232rl-chips)
   * [Software](#software)
      * [CSR software](#csr-software)
      * [Installing prebuilt drivers](#installing-prebuilt-drivers)
         * [Installing on Ubuntu/Debian Linux](#installing-on-ubuntudebian-linux)
         * [Installing on Windows](#installing-on-windows)
      * [Using the driver](#using-the-driver)
         * [Choosing USB transport](#choosing-usb-transport)
         * [Options](#options)
         * [Communication speed](#communication-speed)
         * [Useful commands](#useful-commands)
         * [Troubleshooting](#troubleshooting)
      * [Building for Wine](#building-for-wine)
         * [Building Wine DLL on 32-bit Debian/Ubuntu Linux](#building-wine-dll-on-32-bit-debianubuntu-linux)
         * [Building Wine DLL on 64-bit Debian/Ubuntu Linux](#building-wine-dll-on-64-bit-debianubuntu-linux)
         * [Installing](#installing)
      * [Building DLL for Windows](#building-dll-for-windows)
         * [Cross-compiling DLL for Windows on Debian/Ubuntu using MinGW](#cross-compiling-dll-for-windows-on-debianubuntu-using-mingw)
      * [Bugs](#bugs)
   * [Thanks](#thanks)
   * [Related projects](#related-projects)
   * [Other sources of information](#other-sources-of-information)

# CSR BlueCore USB SPI programmer/debugger

This is USB SPI programmer and debugger for CSR BlueCore bluetooth chips, based
on FTDI FT232R USB to UART converter chip. Programmer hardware can be made
using simple FT232RL breakout board, or built as a dedicated programmer using
included schematic. Programmer driver works by replacing USB SPI programmer
driver, usbspi.dll, in CSR applications and is currently ported to Linux/Wine
and Windows.

Project home page: <https://github.com/lorf/csr-spi-ftdi>.

## CSR chips supported by programmer

Generally, all CSR BlueCore chips starting with BlueCore 2 should be supported.
Programmer was tested with the following chips:

* BC417143 (on HC-05 module)
* BC57F687A
* CSR8645
* BC212015 (reported by Alex Nuc, see [Chip notes](#chip-notes))

### Chip notes

* BlueCore chips require either 3.3V or 1.8V I/O voltage level. Check the
  datasheet.
* Some chips (like CSR8645) share SPI pins with PCM function. For such chips to
  be accessible via SPI, `SPI_PCM#` pin should be pulled up to I/O voltage
  supply through a 10K resistor.
* On some chips `SPI_DEBUG_EN` (on BC6140) or `SPI_PIO#` (on CSR1010) pin
  should be pulled up directly to I/O voltage supply to enable SPI port.
* BlueCore chips which integrate battery charger has charger configuration
  enabled by default. Such chip may shutdown shortly after power on if You
  don't connect charged battery. Battery charger configuration is defined in
  `PSKEY_USR0` and can be changed using appropriate Configuration Tool or
  PSTool. See sample PSR files for disabling charger in [misc/](misc/).
* BlueCore 2 chips (such as BC212015) are not supported in BlueSuite 2.4 and
  above. It's also reported that to flash/dump these chips it's required to
  lower SPI speed. So for BC2 chips it's recommended to use BlueSuite 2.3 and
  set `SPIMAXCLOCK=200` [option](#options).

## Programmer hardware

Programmer hardware is based on FT232R chip. It is possible to use later
generation FTDI chips, see [Supported FTDI chips](#supported-ftdi-chips).

### Using FT232RL breakout board as a programmer

You can build a simple programmer using popular FT232RL breakout boards (search
Ebay for "FT232RL adapter" for example). Pinout specified in spi.c file.
Change it at will. Note that FTDI boards usually provide 5V or 3.3V I/O levels
while CSR chips require 3.3V or 1.8V I/O level. You may supply appropriate
VCCIO to FTDI chip or use logic level converter if levels don't match. See
description of VCCIO pin in FTDI chip datasheet for details.

This is a default pinout:

| Signal | FT232RL pin | FTDI pin name | FTDI GPIO bit | CSR pin  |
|--------|-------------|---------------|---------------|----------|
| CS#    | 2           | DTR#          | D4            | SPI_CS#  |
| CLK    | 3           | RTS#          | D2            | SPI_CLK  |
| MOSI   | 6           | RI#           | D7            | SPI_MOSI |
| MISO   | 9           | DSR#          | D5            | SPI_MISO |
| TX     | 1           | TXD           | Not used      | UART_RX  |
| RX     | 5           | RXD           | Not used      | UART_TX  |
| LED_RD | 10          | DCD#          | D6            | --       |
| LED_WR | 11          | CTS#          | D3            | --       |
| GND    | 7, 18, 21   | GND           | --            | GND      |

SPI and UART BlueCore pins could be connected directly to FTDI pins, but I'd
recommend to wire them through the 220 Ohm (or so) resistors.

TX and RX connections are optional and provide connectivity to BlueCore UART.

LED connections are optional. Wire LED cathodes through the current limiting
resistors (330 Ohm works fine) to the appropriate FTDI
pins. Wire LED anodes to FTDI 3V3 pin.

Other pinouts can be specified with `FTDI_PINOUT` [Option](#options).

Don't power BlueCore chip from FT232R internal 3.3V regulator! It's current
draw may exceed FT232R 50mA limit, which may cause communication errors.

Also see notes on [Counterfeit FT232RL chips](#counterfeit-ft232rl-chips).

### Dedicated programmer

KiCad schematic for a dedicated programmer can be found in
[hardware/](hardware/) subdirectory.

### Supported FTDI chips

The following FTDI chips are reported working with programmer driver: FT232R
(including counterfeits, even "bricked" ones), FT2232H, FT4232H. There is
experimental support for the following chips: FT2232C, FT2232D, FT232H, FT220X,
FT221X, FT230X, FT231X, FT234XD, FT240X. Please report the working status via
[GitHub issues](https://github.com/lorf/csr-spi-ftdi/issues).

Some FT-X family chips (FT220X, FT230X, FT234XD) have only four primary GPIO
pins and don't support default pinout. To use such chip You'll need to set
`hwspi` pinout using `FTDI_PINOUT` [option](#options).

### Counterfeit FT232RL chips

Cheap (< $5) FT232RL modules usually contain counterfeit FT232RL chips
([1](http://zeptobars.ru/en/read/FTDI-FT232RL-real-vs-fake-supereal),
[2](http://www.eevblog.com/forum/reviews/ftdi-driver-kills-fake-ftdi-ft232/msg536891/#msg536891),
[3](http://dangerousprototypes.com/forum/viewtopic.php?p=56343#p56343)).  I've
got such chip (this one has read-only EEPROM, S/N A50285BI, probably it's a
[generation 2 counterfeit](https://blog.cesanta.com/FTDI-adventures)) and
verified csr-spi-ftdi to work with it.

There is a known data loss issue with counterfeit FT232RL chip and ASM1042 USB
3.0 controller. If You encounter it (You'll get an error on stderr, see
[Troubleshooting](#troubleshooting)), try to replug the programmer to other
controller. Note that genuine FT232RL works fine with this controller.

Counterfeit IC also may produce some communication errors, but they are
automatically retried, so in general it works well enough.

Some versions of Windows FTDI driver tend to intentionally
[brick](https://hackaday.com/2014/10/22/watch-that-windows-update-ftdi-drivers-are-killing-fake-chips/)
1st generation counterfeit chips by setting their PID to 0x0000. There
are
[unbrick](https://www.reddit.com/r/arduino/comments/2k0i7x/watch_that_windows_update_ftdi_drivers_are/clgviyl)
([2](http://www.youtube.com/watch?v=RZH_qGautqM)) and
[protect](http://www.eevblog.com/forum/reviews/ftdi-driver-kills-fake-ftdi-ft232/msg537012/#msg537012)
procedures, but csr-spi-ftdi will work on the bricked chips too.


## Software

### CSR software

This driver is tested with CSR BlueSuite 2.1 - 2.6.0 and with CSR BlueLab 4.1,
but should work with other CSR software, such as SDK, ADK, Configuration Tool.
The driver is known to not work with CSR Parameter Manager, see [Bugs](#bugs).
Newer versions of BlueSuite can be found at `https://www.csrsupport.com/PCSW`.
Older versions of BlueSuite can be found at
`https://www.csrsupport.com/PCSWArchive`. Access to these pages requires
registration.

### Installing prebuilt drivers

Prebuilt drivers for Linux and Windows can be downloaded from
<https://github.com/lorf/csr-spi-ftdi/releases>.

#### Installing on Ubuntu/Debian Linux

Install Wine:

    sudo apt-get install wine

Install CSR BlueSuite in Wine. Find all instances of usbspi.dll installed and
move them out of the way:

    find ~/.wine -iname usbspi.dll -exec mv {} {}.orig \;

Copy usbspi.dll.so to Wine system directory:

    sudo cp -p lib-wine-linux/usbspi.dll.so /usr/lib/i386-linux-gnu/wine/

Alternately You can specify location of the .dll.so file in WINEDLLPATH
environment variable, see wine(1) man page for details.

Allow yourself access to FTDI device

    cat <<_EOT_ | sudo tee -a /etc/udev/rules.d/99-ftdi.rules
    # All FTDI chips
    SUBSYSTEM=="usb", ATTRS{idVendor}=="0403", MODE="0660", GROUP="plugdev"
    _EOT_

After that You'll need to add yourself to `plugdev` group and relogin.


#### Installing on Windows

1. Install CSR package such as BlueSuite;
2. Make a backup of usbspi.dll in your application directory (e.g. in
   `C:\Program Files (x86)\CSR\BlueSuite 2.6.0\`);
3. Copy lib-win32/usbspi.dll to your application directory;
4. Connect Your FTDI device to computer;
5. Download and run Zadig from <http://zadig.akeo.ie/>. In Options menu choose
   "List all devices", choose Your FTDI device ("FT232R USB UART" or similar),
   choose libusbK driver, press "Replace driver" or "Install driver".  This
   will install generic libusb-compatible driver for your FTDI chip. There is a
   tutorial on running libftdi programs on Windows here:
   <http://embedded-funk.net/running-libftdi-under-windows/>;
6. Run your CSR apps.

### Using the driver

#### Choosing USB transport

Older BlueSuite defaults to use CSR SPI LPT programmer, to use csr-spi-ftdi You
need to select USB SPI transport. Use `-TRANS "SPITRANS=USB SPIPORT=1"` option
for command line tools. Import
[misc/spi-set-usb-transport.reg](misc/spi-set-usb-transport.reg) to change the
default.

#### Options

Csr-spi-ftdi driver supports several options that can be set as environment
variables or using the -TRANS option to most CSR commandline apps.

* `SPIMAXCLOCK` - Maximum SPI clock frequency in kHz, default is 1000.
  Practical values range from 20 to 2000. CSR app may slow SPI clock down when
  read or write verification fails. Some commands are always executed at 20
  kHz.
* `FTDI_LOG_LEVEL` - sets csr-spi-ftdi log level, available log levels:
  `quiet`, `err`, `warn`, `info`, `debug`. Adding a `,dump` option provides hex
  dumps of transferred data. Example: `FTDI_LOG_LEVEL=info,dump`. Default:
  `warn`.
* `FTDI_LOG_FILE` - specify log file name. Can be set to `stdout` to log to
  standard output, or to `stderr` to log to standard error stream. Default:
  `stderr`.
* `FTDI_INTERFACE` (since version 0.5.1) - specify interface on multi-interface
  chips (FT2232, FT4232). Default: `A`.
* `FTDI_PINOUT` (since version 0.5.2) - specify a pinout. Available pinouts:
  * `default` - default pinout as described in [Using FT232RL breakout board as
    a programmer](#using-ft232rl-breakout-board-as-a-programmer).
  * `noleds` - this is the same as `default` but without LEDs.
  * `hwspi` - pinout for use with MPSSE (FT2232, FT4232, FT232H) and FT-X
    chips, uses the same pins as hardware SPI. Note that hardware SPI
    capability is not used, just the same pinout is used for convenience. This
    pinout can be used with adapters like
    [TIAO TUMPA](http://www.tiaowiki.com/w/TIAO_USB_Multi_Protocol_Adapter_User's_Manual).
    The pinout is as follows: `CS` - `CTS#` (`D3`), `CLK` - `TXD` (`D0`),
    `MOSI` - `RXD` (`D1`), `MISO` - `RTS#` (`D2`).
  * `hwspi+leds` - this is the same as `hwspi` but adds read and write LEDs on
    `DTR#` (`D4`) and `DSR#` (`D5`) pins respectively.
  * `sparkfun` - pinout for use with adapters with TX, RX, DTR and CTS
    signals brougth out, such as
    [Sparkfun FTDI Basic breakout board](https://cdn.sparkfun.com/datasheets/BreakoutBoards/FTDI%20Basic-v22-3.3V.pdf):
	`CS` - `CTS#` (`D3`), `CLK` - `TXD` (`D0`), `MOSI` - `RXD` (`D1`), `MISO` -
    `DTR#` (`D4`).

For other options see [misc/transport-options.md](misc/transport-options.md).

#### Communication speed

Reading or writing a 1 MB flash on HC-05 module takes about 60 seconds. Dumping
HC-05 PS keys takes about 40 seconds.

Running csr-spi-ftdi in a virtual machine slows things down presumably due to
latency added by USB virtualization. E.g. running csr-spi-ftdi under VirtualBox
slows transactions down about 4x times.

#### Useful commands

These commands should be executed from directory where BlueSuite is installed
or this directory should be in your PATH.

* Display chip ID, this is also a quick communication test:

        BlueFlashCmd.exe -chipver

* Display chip ID, firmware version and flash size:

        BlueFlashCmd.exe -identify

* Save firmware backup (only for chips with flash, backup will include PS
  keys):

        BlueFlashCmd.exe -dump csr-fw-backup

  This creates two files, `csr-fw-backup.xpv` and `csr-fw-backup.xdv`.

* Flash firmware from files `csr-fw.xpv` and `csr-fw.xdv`:

        BlueFlashCmd.exe csr-fw

* Collect debug logs:

        BlueFlashCmd.exe -trans "SPIDEBUG=ON \
            SPIDEBUG_FILE=csr-spi-debug.log FTDI_LOG_LEVEL=debug,dump \
            FTDI_LOG_FILE=ftdi-debug.log" -identify

* Lower SPI speed 5 times:

        BlueFlashCmd.exe -trans SPIMAXCLOCK=200 -dump csr-fw-backup

* Save chip settings (PS Keys) backup into `csr-pskeys.psr`:

        pscli.exe dump csr-pskeys.psr

* Merge some settings from `pskeys.psr` to the chip:

        pscli.exe merge pskeys.psr

* Backup whole EEPROM image to a file:

        e2cmd.exe dump eeprom-backup.hex

* Restore EEPROM contents from backup file:

        e2cmd.exe download eeprom-backup.hex

#### Troubleshooting

* Decreasing SPI speed using `SPIMAXCLOCK` [option](#options) may help in case
  of communication failures.
* `Unable to start read (invalid control data)` errors are usually harmless,
  since read attempts are retried. If You've got a pile of theese errors and
  programmer doesn't work - check connections, voltage levels, try to lower SPI
  connection resistor values or connecting ~ 1nF capacitors in parallel with
  resistors. Decreasing SPI speed using `SPIMAXCLOCK` [option](#options) may
  also help.
* `WARNING: Attempt %d to read sector %d` warnings are also harmless if they are
  not result in error.
* `Couldn't find LPT port` error means You use spilpt.dll driver instead of
  usbspi.dll. Try importing
  [misc/spi-set-usb-transport.reg](misc/spi-set-usb-transport.reg) or adding
  `-trans "SPITRANS=USB SPIPORT=1"` option on command line.
* `Lost %d of %d bytes of data in transit` or `ERROR: Download Failed, Sector
  %d` - probably an indication of counterfeit FT232RL plugged into ASM1042 USB
  3.0 controller. Try to replug into other controller. See [Counterfeit FT232RL
  chips](#counterfeit-ft232rl-chips).

### Building for Wine

#### Building Wine DLL on 32-bit Debian/Ubuntu Linux

Install build tools:

    sudo apt-get install -y build-essential

Install development libraries:

    sudo apt-get install -y wine-dev libc6-dev libstdc++-dev libftdi1-dev libudev-dev

Build with command:

    make -f Makefile.wine all


#### Building Wine DLL on 64-bit Debian/Ubuntu Linux

Install build tools:

    sudo apt-get install -y build-essential gcc-multilib g++-multilib

Install 32 bit stuff:

    sudo apt-get install -y wine-dev:i386 libc6-dev-i386 libstdc++-dev:i386 libftdi1-dev:i386 libudev-dev:i386

Build with command:

    make -f Makefile.wine all

#### Installing

Install CSR BlueSuite in Wine. Find all instances of usbspi.dll installed and
move them out of the way:

    find ~/.wine -iname usbspi.dll -exec mv {} {}.orig \;

Install Wine dll into the Wine libraries directory:

    sudo make -f Makefile.wine install

Alternately You can specify location of the .dll.so file in WINEDLLPATH
environment variable, see wine(1) man page for details.


### Building DLL for Windows

#### Cross-compiling DLL for Windows on Debian/Ubuntu using MinGW

Install MinGW cross-development environment:

    sudo apt-get install -y mingw-w64 cmake

Download [precompiled libusb for
windows](http://sourceforge.net/projects/libusb/files/) and extract it to the
libusb directory:

    wget https://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.22/libusb-1.0.22.7z
    7z x -olibusb libusb-1.0.22.7z

Build [libftdi](http://www.intra2net.com/en/developer/libftdi/) from source:

    wget http://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.4.tar.bz2
    tar xjvf libftdi1-1.4.tar.bz2
    cd libftdi1-1.4
    cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-w64-mingw32.cmake \
        -DLIBUSB_INCLUDE_DIR=../libusb/include/libusb-1.0 \
        -DLIBUSB_LIBRARIES="-L../../libusb/MinGW32/static -lusb-1.0" \
        -DCMAKE_INSTALL_PREFIX=../libftdi1 .
    make all install
    cd ..

Build with command:

    make -f Makefile.mingw all


### Bugs

* See [Issues on github](https://github.com/lorf/csr-spi-ftdi/issues) to list
  current bug reports or to report a bug.
* Some long standing bugs on a
  [TODO](https://github.com/lorf/csr-spi-ftdi/wiki/TODO) list:
  * Multiple programmers connected at the same time are not supported. For this
    to work, stream API (used by BlueSuite starting from 2.4) need to be
    properly implemented. Currently `spifns_stream_*()` functions are just
    wrappers around non-stream implementations.
  * CSR Parameter Manager is known to not work with this driver.
    `spifns_bccmd_*()` API needs to be implemented for it to work.


## Thanks
* This project is a derivative of Frans-Willem Hardijzer's [reverse-engineered
  spilpt.dll drivers](https://github.com/Frans-Willem/CsrSpiDrivers);
* Thanks to **unicorn** from <http://www.nebo-forum.kiev.ua/> for the idea of a
  DLL for Wine.
* Thanks to all the [Contributors](https://github.com/lorf/csr-spi-ftdi/wiki/Contributors)!

## Related projects

* [LPT programmer and general info](http://byron76.blogspot.com/) by Robin
  Gross;
* [Reverse-engineered SPILPT driver + Arduino SPILPT
  driver](https://github.com/Frans-Willem/CsrSpiDrivers) by Frans-Willem
  Hardijzer, for Windows;
* [USBSPI programmer](http://jernej87.blogspot.com/) based on CSR BC3 chip
  using original firmware by Jernej Škrabec;
* [USBSPI programmer software for Linux](https://gitorious.org/csrprogrammer)
  ([git mirror](https://github.com/lorf/csrprogrammer)) by Jernej Škrabec;
   * [USBSPI protocol
     analysis](http://jernej87.blogspot.com/2012/10/csrs-usb-programmer-protocol-analysis.html);
   * [Using USBSPI on
     Linux](http://jernej87.blogspot.com/2012/10/dumping-bluecore4-firmware-on-linux.html);
* [Remote debugging protocol
  implementaton](https://gitorious.org/csrprogrammer/csrremote) ([git
  mirror](https://github.com/lorf/csrremote))
* [USBSPI programmer based on Stellaris
  Launchpad](https://github.com/Frans-Willem/CsrUsbSpiDeviceRE) by Frans-Willem
  Hardijzer, for Windows;
   * [Port to Tiva C Launchpad](https://github.com/raplin/CsrUsbSpiDeviceRE)
* See a list of [related projects in the wiki](https://github.com/lorf/csr-spi-ftdi/wiki/RelatedProjects).

## Other sources of information
* ~~BlueSuite 2.5.0 "source code"
  `https://www.csrsupport.com/document.php?did=38692` - it doesn't contain
  source code for SPI drivers but at least development header files in
  CSRSource/result/include/ are of some help.~~ It seems CSR removed it from
  download.
