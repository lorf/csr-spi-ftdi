#ifndef _SPI_H
#define _SPI_H

#include <stdint.h>

#ifdef ENABLE_LEDS
/* Bit field */
#define SPI_LED_OFF     (0)
#define SPI_LED_READ    (1 << 0)
#define SPI_LED_WRITE   (1 << 1)
#endif

/* Bit field */
#define SPI_XFER_READ   (1 << 0)
#define SPI_XFER_WRITE  (1 << 1)

/* XAP CPU running status */
#define SPI_CPU_STOPPED 1
#define SPI_CPU_RUNNING 0

struct spi_port {
    uint16_t vid, pid;
    char manuf[128], desc[128], serial[128];
    char name[32];
};

struct spi_pins {
    uint8_t ncs, clk, mosi, miso;
    uint8_t nledr, nledw;
};

/*
 * Pinouts. Change at will. Beware that FTDI adapters provide 5V or 3V3 I/O
 * levels, but CSR chips require 3V3 or 1V8 I/O level.
 */

/*
 * Default pinout, this leaves TX and RX pins free for UART connection.
 *
 * CS - DBUS4 (DTR#), CLK - DBUS2 (RTS#), MOSI - DBUS7 (RI#),
 * MISO - DBUS5 (DSR#), R_LED - DBUS6 (DCD#), W_LED - DBUS3 (CTS#).
 */
#define SPI_PIN_PRESET_DEFAULT \
    { (1 << 4), (1 << 2), (1 << 7), (1 << 5), (1 << 6), (1 << 3) }

/*
 * Same as above, but without LEDs.
 */
#define SPI_PIN_PRESET_NOLEDS \
    { (1 << 4), (1 << 2), (1 << 7), (1 << 5), 0, 0 }

/*
 * Pinout for use with MPSSE chips, uses the same pins as hardware SPI.
 * Note that hardware SPI capability is not used, just the same pinout for
 * convenience. This pinout can be used with adapters like TIAO TUMPA.
 *
 * CS - DBUS3, CLK - DBUS0, MOSI - DBUS1, MISO - DBUS2, R_LED - DBUS4,
 * W_LED - DBUS5.
 */
#define SPI_PIN_PRESET_HWSPI_LEDS \
    { (1 << 3), (1 << 0), (1 << 1), (1 << 2), (1 << 4), (1 << 5) }

/*
 * Same as above, but without LEDs.
 */
#define SPI_PIN_PRESET_HWSPI \
    { (1 << 3), (1 << 0), (1 << 1), (1 << 2), 0, 0 }

/*
 * Pinout for Sparkfun FTDI-Basic breakout board.
 * Ref: https://cdn.sparkfun.com/datasheets/BreakoutBoards/FTDI%20Basic-v22-3.3V.pdf
 * CS   - CTS# (D3)
 * CLK  - TXD  (D0)
 * MOSI - RXD  (D1)
 * MISO - DTR#  (D4)
 */
#define SPI_PIN_PRESET_SPARKFUN \
	{ (1 << 3), (1 << 0), (1 << 1), (1 << 4), 0, 0 }

#define SPI_PIN_PRESETS { \
        SPI_PIN_PRESET_DEFAULT, \
        SPI_PIN_PRESET_NOLEDS, \
        SPI_PIN_PRESET_HWSPI_LEDS, \
        SPI_PIN_PRESET_HWSPI, \
        SPI_PIN_PRESET_SPARKFUN, \
    }

enum spi_pinouts {
    SPI_PINOUT_DEFAULT = 0,
    SPI_PINOUT_NOLEDS,
    SPI_PINOUT_HWSPI_LEDS,
    SPI_PINOUT_HWSPI,
    SPI_PINOUT_SPARKFUN,
};

#ifdef __cplusplus
extern "C" {
#endif

void spi_set_err_buf(char *buf, size_t sz);
void spi_set_pinout(enum spi_pinouts pinout);
int spi_set_interface(const char *intf);
int spi_init(void);
int spi_deinit(void);
int spi_get_port_list(struct spi_port **pportlist, int *pnports);

int spi_open(int nport);
int spi_isopen(void);
int spi_close(void);

int spi_set_clock(unsigned long spi_clk);
void spi_set_max_clock(unsigned long clk);
int spi_clock_slowdown(void);
unsigned long spi_get_max_clock(void);
unsigned long spi_get_clock(void);

int spi_xfer_begin(int get_status);
int spi_xfer(int cmd, int iosize, void *buf, int size);
int spi_xfer_end(void);

#ifdef ENABLE_LEDS
void spi_led(int led);
#endif

#ifdef SPI_STATS
void spi_output_stats(void);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
