#include <ftdi.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#ifdef SPI_STATS
#include <math.h>
#endif
#include <sys/time.h>

#include "spi.h"
#include "compat.h"
#include "logging.h"

/* Default SPI clock rate, in kHz */
#define SPIMAXCLOCK     1000

static char *ftdi_type_str = NULL;

static uint8_t *ftdi_out_buf = NULL, *ftdi_in_buf = NULL;
static size_t ftdi_buf_size = 0;
static unsigned int ftdi_out_buf_offset;

static int spi_dev_open = 0;
static int spi_nrefs = 0;

static struct ftdi_context ftdic;
static enum ftdi_interface ftdi_intf = INTERFACE_A;
static uint8_t ftdi_pin_state = 0;

#define SPI_LED_FREQ  10   /* Hz */
static int spi_led_state = 0;

#ifdef SPI_STATS
static struct spi_stats {
    long reads, writes;
    long read_bytes, write_bytes;
    long ftdi_xfers, ftdi_bytes, ftdi_short_reads;
    struct timeval tv_xfer_begin, tv_xfer;
    struct timeval tv_open_begin, tv_open;
    unsigned long spi_clock_max, spi_clock_min;
    unsigned long slowdowns;
} spi_stats;
#endif

struct ftdi_device_ids {
    uint16_t vid, pid;
    char name[10];
};

#define SPI_MAX_PORTS   16
static struct spi_port spi_ports[SPI_MAX_PORTS];
static int spi_nports = 0;

unsigned long spi_clock = 0, spi_max_clock = SPIMAXCLOCK;

static struct ftdi_device_ids ftdi_device_ids[] = {
    { 0x0403, 0x6001, "FT232R" }, /* FT232R */
    { 0x0403, 0x0000, "FT232R" }, /* Counterfeit FT232RL bricked by FTDI driver */
    /* Chips below are not tested. */
    { 0x0403, 0x6010, "FT2232" }, /* FT2232H/C/D */
    { 0x0403, 0x6011, "FT4232" }, /* FT4232H */
    { 0x0403, 0x6014, "FT232H" }, /* FT232H */
    /*{ 0x0403, 0x6015, "FT230X" },*/ /* FT230X, only since libftdi1-1.2 */
};

static char *spi_err_buf = NULL;
static size_t spi_err_buf_sz = 0;

static struct spi_pins *spi_pins;
static struct spi_pins spi_pin_presets[] = SPI_PIN_PRESETS;
static enum spi_pinouts spi_pinout = SPI_PINOUT_DEFAULT;

void spi_set_err_buf(char *buf, size_t sz)
{
    if (buf && sz) {
        spi_err_buf = buf;
        spi_err_buf_sz = sz;
    } else {
        spi_err_buf = NULL;
        spi_err_buf_sz = 0;
    }
}

#define SPI_ERR(...)   do { \
        LOG(ERR, __VA_ARGS__); \
        spi_err(__VA_ARGS__); \
    } while (0)

static void spi_err(const char *fmt, ...) {
    static char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    if (spi_err_buf) {
        strncpy(spi_err_buf, buf, spi_err_buf_sz);
        spi_err_buf[spi_err_buf_sz - 1] = '\0';
    }
    va_end(args);
}

/*
 * FTDI transfer data forth and back in syncronous bitbang mode
 */

static int spi_ftdi_xfer(uint8_t *out_buf, uint8_t *in_buf, int size)
{
    int rc;
    uint8_t *bufp;
    int len;
    int lost_buffer_counter = 0;

    rc = ftdi_write_data(&ftdic, out_buf, size);
    if (rc < 0) {
        SPI_ERR("FTDI: write data failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
        return -1;
    }
    if (rc != size) {
        SPI_ERR("FTDI: short write: need %d, got %d", size, rc);
        return -1;
    }
#ifdef SPI_STATS
    spi_stats.ftdi_xfers++;
    spi_stats.ftdi_bytes += size;
#endif

    /* In FTDI sync bitbang mode every write is preceded by a read to internal
     * buffer. We need to issue read for every write.
     *
     * The data in the read buffer may not be immediately available. Wait for
     * it if needed. */

    bufp = in_buf;
    len = size;

    while (len > 0) {
        rc = ftdi_read_data(&ftdic, bufp, len);

        if (rc < 0) {
            SPI_ERR("FTDI: read data failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
            return -1;
        }

        /*
         * I've encountered a bug with Gen2 counterfeit FT232RL (S/N A50285BI)
         * in SyncBB mode connected to ASM1042 USB 3.0 controller (on Asus
         * M5A78L-M/USB3 motherboard), Linux 3.13.0: sometimes, when flashing
         * or erasing a flash on HC-05 module, after ftdi_write_data(), the
         * next ftdi_read_data() returns less amount of data than it should.
         * The maximum lost data size seen is 62 bytes, that size combined with
         * 2 byte FTDI overhead gives 64 bytes which is FT232R max packet size.
         * Dumping USB traffic shows missing packet while in ftdi_read_data().
         * The bug is clearly in counterfeit FT232RL. The strange thing is that
         * it only occurs while flashing or erasing, not when dumping.
         *
         * The workaround is to connect FTDI adapter to USB 2.0 socket or hub.
         */
        if (rc == 0)
            lost_buffer_counter++;
        else
            lost_buffer_counter = 0;
        if (lost_buffer_counter > 50) {
            LOG(ERR, "***************************************************");
            LOG(ERR, "Lost %d of %d bytes of data in transit", len, size);
            LOG(ERR, "Probably a counterfeit FT232RL in USB3.0 socket.");
            LOG(ERR, "Try to plug programmer into USB 2.0 socket.");
            LOG(ERR, "***************************************************");
            ftdi_out_buf_offset = 0;
            return -1;
        }

        len -= rc;
        bufp += rc;
#ifdef SPI_STATS
        spi_stats.ftdi_xfers++;
        spi_stats.ftdi_bytes += rc;
        if (len > 0)
            spi_stats.ftdi_short_reads++;
#endif
    }

    return 0;
}

static void spi_led_tick(void)
{
    struct timeval tv;

    if (spi_led_state == SPI_LED_OFF) {
        /* Asked to turn LEDs off */
        if (spi_pins->nledr)
            ftdi_pin_state |= spi_pins->nledr;
        if (spi_pins->nledw)
            ftdi_pin_state |= spi_pins->nledw;
        return;
    }

    if (gettimeofday(&tv, NULL) < 0)
        LOG(WARN, "gettimeofday failed: %s", strerror(errno));

    if (((tv.tv_sec * 1000 + tv.tv_usec / 1000) /
                (1000 / SPI_LED_FREQ / 2)) % 2 == 0)
    {
        if (spi_led_state & SPI_LED_READ && spi_pins->nledr)
            ftdi_pin_state &= ~spi_pins->nledr;
        if (spi_led_state & SPI_LED_WRITE && spi_pins->nledw)
            ftdi_pin_state &= ~spi_pins->nledw;
    } else {
        if (spi_pins->nledr)
            ftdi_pin_state |= spi_pins->nledr;
        if (spi_pins->nledw)
            ftdi_pin_state |= spi_pins->nledw;
    }
}

void spi_led(int led) 
{
    spi_led_state = led;
    spi_led_tick();
}

/*
 * spi_xfer_*() use global output and input buffers. Output buffer is flushed
 * on the following conditions:
 *  * when buffer becomes full;
 *  * on the clock change;
 *  * before read operation in spi_xfer();
 *  * if the running status of the CPU is requested from spi_xfer_begin();
 *  * at the closure of FTDI device.
 * Read operations are only done in spi_xfer() and spi_xfer_begin(), in other
 * situations we may safely discard what was read into the input buffer buffer
 * by spi_ftdi_xfer().
 */

int spi_xfer_begin(int get_status)
{
    unsigned int status_offset = 0;
    int status;

    LOG(DEBUG, "");

    if (spi_clock == 0) {
        SPI_ERR("SPI clock not initialized");
        return -1;
    }

    spi_led_tick();

#ifdef SPI_STATS
    if (gettimeofday(&spi_stats.tv_xfer_begin, NULL) < 0)
        LOG(WARN, "gettimeofday failed: %s", strerror(errno));
#endif

    /* Check if there is enough space in the buffer */
    if (ftdi_buf_size - ftdi_out_buf_offset < 7) {
        /* There is no room in the buffer for the following operations, flush
         * the buffer */
        if (spi_ftdi_xfer(ftdi_out_buf, ftdi_in_buf, ftdi_out_buf_offset) < 0)
            return -1;
        /* The data in the buffer is useless, discard it */
        ftdi_out_buf_offset = 0;
    }

    /* BlueCore chip SPI port reset sequence: deassert CS, wait at least two
     * clock cycles */

    ftdi_pin_state |= spi_pins->ncs;
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    ftdi_pin_state |= spi_pins->clk;
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    ftdi_pin_state &= ~spi_pins->clk;
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    ftdi_pin_state |= spi_pins->clk;
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    ftdi_pin_state &= ~spi_pins->clk;
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    /* Start transfer */

    ftdi_pin_state &= ~spi_pins->ncs;
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    if (get_status) {
        /*
         * Read the stopped status of the CPU. From CSR8645 datasheet: "When
         * CSR8645 BGA is deselected (SPI_CS# = 1), the SPI_MISO line does not
         * float. Instead, CSR8645 BGA outputs 0 if the processor is running or
         * 1 if it is stopped". However in practice this is not entirely true.
         * Reading MISO while the CPU is deselected gives wrong result. But
         * reading it just after selecting gives the actual status. Also both
         * sources I consulted (CsrSpiDrivers and CsrUsbSpiDeviceRE) are
         * reading the status after setting CS# to 0.
         */

        status_offset = ftdi_out_buf_offset;
        ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

        if (spi_ftdi_xfer(ftdi_out_buf, ftdi_in_buf, ftdi_out_buf_offset) < 0)
            return -1;

        if (ftdi_in_buf[status_offset] & spi_pins->miso)
            status = SPI_CPU_STOPPED;
        else
            status = SPI_CPU_RUNNING;

        /* Other data in the buffer is useless, discard it */
        ftdi_out_buf_offset = 0;

        return status;
    }

    return 0;
}

int spi_xfer_end(void)
{
    LOG(DEBUG, "");

    /* Check if there is enough space in the buffer */
    if (ftdi_buf_size - ftdi_out_buf_offset < 2) {
        /* There is no room in the buffer for the following operations, flush
         * the buffer */
        if (spi_ftdi_xfer(ftdi_out_buf, ftdi_in_buf, ftdi_out_buf_offset) < 0)
            return -1;
        /* The data in the buffer is useless, discard it */
        ftdi_out_buf_offset = 0;
    }

    /* Commit the last ftdi_pin_state after spi_xfer() */
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    ftdi_pin_state |= spi_pins->ncs;
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    /* Buffer flush is done on close */

#ifdef SPI_STATS
    {
        struct timeval tv;

        if (gettimeofday(&tv, NULL) < 0)
            LOG(WARN, "gettimeofday failed: %s", strerror(errno));
        timersub(&tv, &spi_stats.tv_xfer_begin, &tv);
        timeradd(&spi_stats.tv_xfer, &tv, &spi_stats.tv_xfer);
    }
#endif

    spi_led(SPI_LED_OFF);

    return 0;
}

int spi_xfer(int cmd, int iosize, void *buf, int size)
{
    unsigned int write_offset, read_offset, ftdi_in_buf_offset;
    uint16_t bit, word;

    LOG(DEBUG, "(%d, %d, %p, %d)", cmd, iosize, buf, size);

    write_offset = 0;
    read_offset = 0;

    do {
        spi_led_tick();

        /* In FTDI sync bitbang mode we need to write something to device to
         * toggle a read. */

        /* The read, if any, will start at current buffer offset */
        ftdi_in_buf_offset = ftdi_out_buf_offset;

        while (write_offset < size) {
            /* 2 bytes per bit */
            if (ftdi_buf_size - ftdi_out_buf_offset < iosize * 2) {
                /* There is no room in the buffer for following word write,
                 * flush the buffer */
                if (spi_ftdi_xfer(ftdi_out_buf, ftdi_in_buf, ftdi_out_buf_offset) < 0)
                    return -1;
                ftdi_out_buf_offset = 0;

                /* Let following part to parse from ftdi_in_buf if needed */
                break;
            }

            if (iosize == 8)
                word = ((uint8_t *)buf)[write_offset];
            else
                word = ((uint16_t *)buf)[write_offset];

            /* MOSI is sensed by BlueCore on the rising edge of CLK, MISO is
             * changed on the falling edge of CLK. */
            for (bit = (1 << (iosize - 1)); bit != 0; bit >>= 1) {
                if (cmd & SPI_XFER_WRITE) {
                    /* Set output bit */
                    if (word & bit)
                        ftdi_pin_state |= spi_pins->mosi;
                    else
                        ftdi_pin_state &= ~spi_pins->mosi;
                } else {
                    /* Write 0 during a read */
                    ftdi_pin_state &= ~spi_pins->mosi;
                }

                ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

                /* Clock high */
                ftdi_pin_state |= spi_pins->clk;
                ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

                /* Clock low */
                ftdi_pin_state &= ~spi_pins->clk;
            }
            write_offset++;
        }

        if (cmd & SPI_XFER_READ) {
            if (ftdi_out_buf_offset) {
                if (spi_ftdi_xfer(ftdi_out_buf, ftdi_in_buf, ftdi_out_buf_offset) < 0)
                    return -1;
                ftdi_out_buf_offset = 0;
            }
            while (read_offset < write_offset) {
                word = 0;
                for (bit = (1 << (iosize - 1)); bit != 0; bit >>= 1) {
                    /* Input bit */
                    ftdi_in_buf_offset++;
                    if (ftdi_in_buf[ftdi_in_buf_offset] & spi_pins->miso)
                        word |= bit;
                    ftdi_in_buf_offset++;
                }

                if (iosize == 8)
                    ((uint8_t *)buf)[read_offset] = (uint8_t)word;
                else
                    ((uint16_t *)buf)[read_offset] = word;

                read_offset++;
            }
            /* Reading done, reset buffer */
            ftdi_out_buf_offset = 0;
            ftdi_in_buf_offset = 0;
        }

    } while (write_offset < size);

#ifdef SPI_STATS
    if (cmd & SPI_XFER_WRITE) {
        spi_stats.writes++;
        spi_stats.write_bytes += size * iosize / 8;
    } else {
        spi_stats.reads++;
        spi_stats.read_bytes += size * iosize / 8;
    }
#endif

    return size;
}

/* Fills spi_ports array with discovered devices, sets spi_nports */
static int spi_enumerate_ports(void)
{
    int id, rc;
    struct ftdi_device_list *ftdevlist, *ftdev;

    spi_nports = 0;

    for (id = 0; id < sizeof(ftdi_device_ids) / sizeof(ftdi_device_ids[0]) && spi_nports < SPI_MAX_PORTS; id++) {
        LOG(DEBUG, "find all: 0x%04x:0x%04x", ftdi_device_ids[id].vid, ftdi_device_ids[id].pid);
        rc = ftdi_usb_find_all(&ftdic, &ftdevlist, ftdi_device_ids[id].vid, ftdi_device_ids[id].pid);
        if (rc < 0) {
            SPI_ERR("FTDI: ftdi_usb_find_all() failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
            return -1;
        }
        if (rc == 0)
            continue;

        for (ftdev = ftdevlist; ftdev && spi_nports < SPI_MAX_PORTS; ftdev = ftdev->next) {
            spi_ports[spi_nports].vid = ftdi_device_ids[id].vid;
            spi_ports[spi_nports].pid = ftdi_device_ids[id].pid;
            memset(spi_ports[spi_nports].manuf, 0, sizeof(spi_ports[spi_nports].manuf));
            memset(spi_ports[spi_nports].desc, 0, sizeof(spi_ports[spi_nports].desc));
            memset(spi_ports[spi_nports].serial, 0, sizeof(spi_ports[spi_nports].serial));

            rc = ftdi_usb_get_strings(&ftdic, ftdev->dev,
                    spi_ports[spi_nports].manuf, sizeof(spi_ports[spi_nports].manuf),
                    spi_ports[spi_nports].desc, sizeof(spi_ports[spi_nports].desc),
                    spi_ports[spi_nports].serial, sizeof(spi_ports[spi_nports].serial));
            if (rc < 0) {
                /* It's not critical to have these strings, make it a warning. */
                LOG(WARN, "FTDI: ftdi_usb_get_strings() failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
                if (rc == -9) { /* "\retval  -9: get serial number failed" */
                    /* Serial number can be unavailable due to SerNumEnable*
                     * settings in FTDI EEPROM. */
                    LOG(WARN, "FTDI: getting serial number failed, probably SerNumEnable* is off in EEPROM");
                    spi_ports[spi_nports].serial[0] = '\0';
                }
            }
            snprintf(spi_ports[spi_nports].name, sizeof(spi_ports[spi_nports].name),
                    "%s %s", ftdi_device_ids[id].name, spi_ports[spi_nports].serial);
            LOG(INFO, "Found device: name=\"%s\", manuf=\"%s\", desc=\"%s\", serial=\"%s\", vid=0x%04x, pid=0x%04x",
                    spi_ports[spi_nports].name, spi_ports[spi_nports].manuf,
                    spi_ports[spi_nports].desc, spi_ports[spi_nports].serial,
                    ftdi_device_ids[id].vid, ftdi_device_ids[id].pid);

            spi_nports++;
        }
        ftdi_list_free(&ftdevlist);
    }

    return 0;
}

void spi_set_pinout(enum spi_pinouts pinout)
{
    spi_pinout = pinout;
}

int spi_set_interface(const char *intf)
{
    enum ftdi_interface eintf;

    if (intf[1] != '\0')
        return -1;
    if (intf[0] == 'a' || intf[0] == 'A' || intf[0] == '0') {
        eintf = INTERFACE_A;
    } else if (intf[0] == 'b' || intf[0] == 'B' || intf[0] == '1') {
        eintf = INTERFACE_B;
    } else if (intf[0] == 'c' || intf[0] == 'C' || intf[0] == '2') {
        eintf = INTERFACE_C;
    } else if (intf[0] == 'd' || intf[0] == 'D' || intf[0] == '3') {
        eintf = INTERFACE_D;
    } else {
        return -1;
    }

    ftdi_intf = eintf;
    return 0;
}

int spi_init(void)
{
    LOG(DEBUG, "spi_nrefs=%d, spi_dev_open=%d", spi_nrefs, spi_dev_open);

    spi_nrefs++;

    if (spi_nrefs > 1) {
        LOG(WARN, "Superfluos call to spi_init()");
        return 0;
    }

    LOG(ALL, "csr-spi-ftdi " VERSION ", git rev " GIT_REVISION);

    if (ftdi_init(&ftdic) < 0) {
        SPI_ERR("FTDI: init failed");
        spi_nrefs = 0;
        return -1;
    }

    if (spi_enumerate_ports() < 0) {
        spi_deinit();
        return -1;
    }

    spi_pins = &spi_pin_presets[spi_pinout];

    return 0;
}

int spi_get_port_list(struct spi_port **pportlist, int *pnports)
{
    if (spi_nrefs < 1) {
        SPI_ERR("FTDI: spi not initialized");
        return -1;
    }

    if (pportlist)
        *pportlist = spi_ports;
    if (pnports)
        *pnports = spi_nports;

    return 0;
}

int spi_deinit(void)
{
    LOG(DEBUG, "spi_nrefs=%d, spi_dev_open=%d", spi_nrefs, spi_dev_open);

    if (spi_nrefs) {
        if (spi_dev_open)
            if (spi_close() < 0)
                return -1;
        spi_nrefs = 0;
    }
    return 0;
}

int spi_set_clock(unsigned long spi_clk) {
    unsigned long ftdi_clk;
    int rc;

    LOG(DEBUG, "(%lu)", spi_clk);

    if (!spi_isopen()) {
        SPI_ERR("FTDI: setting SPI clock failed: SPI device is not open");
        return -1;
    }

    if (spi_clk > spi_max_clock)
        spi_clk = spi_max_clock;

    spi_clock = spi_clk;
    /* FTDI clock in Hz is 2 * SPI clock, FTDI clock in Hz */
    ftdi_clk = spi_clock * 2000;

    /* Flush the write buffer before setting clock */
    if (ftdi_out_buf_offset) {
        if (spi_ftdi_xfer(ftdi_out_buf, ftdi_in_buf, ftdi_out_buf_offset) < 0)
            return -1;
        /* The data in the buffer is useless, discard it */
        ftdi_out_buf_offset = 0;
    }

    /*
     * See FT232R datasheet, section "Baud Rate Generator" and AppNote
     * AN_232R-01, section "Synchronous Bit Bang Mode". Also see this thread on
     * bitbang baud rate hardware bug in FTDI chips (XXX is this related to
     * syncbb mode?):
     * http://developer.intra2net.com/mailarchive/html/libftdi/2010/msg00240.html
     */
    LOG(INFO, "FTDI: setting SPI clock to %lu (FTDI baudrate %lu)", spi_clk, ftdi_clk / 16);
    rc = ftdi_set_baudrate(&ftdic, ftdi_clk / 16);
    if (rc < 0) {
        SPI_ERR("FTDI: set baudrate %lu failed: [%d] %s",
                ftdi_clk / 16, rc, ftdi_get_error_string(&ftdic));
        return -1;
    }

#ifdef SPI_STATS
    if (spi_stats.spi_clock_max == 0)
        spi_stats.spi_clock_max = spi_max_clock;
    if (spi_stats.spi_clock_min == 0)
        spi_stats.spi_clock_min = spi_max_clock;
    /* Don't account for slow cmds, that are executing at 20 kHz,
     * they are short and not representative */
    if (spi_clock > 20 && spi_clock < spi_stats.spi_clock_min)
            spi_stats.spi_clock_min = spi_clock;
#endif
    return 0;
}

void spi_set_max_clock(unsigned long clk) {
    LOG(INFO, "FTDI: setting SPI max clock: %lu", clk);
    spi_max_clock = clk;
}

int spi_clock_slowdown(void) {
    unsigned long clk = spi_clock;

    /* Slow SPI clock down by 1.5 */
    clk = (clk * 2) / 3;
    if (clk < 25)
        clk = 25;

#ifdef SPI_STATS
    spi_stats.slowdowns++;
#endif

    LOG(INFO, "FTDI: SPI clock slowdown, set SPI clock to %lu", clk);
    return spi_set_clock(clk);
}

unsigned long spi_get_max_clock(void) {
    return spi_max_clock;
}

unsigned long spi_get_clock(void) {
    return spi_clock;
}

int spi_open(int nport)
{
    int rc;
    char *serial;
    uint8_t output_pins;

    LOG(DEBUG, "(%d) spi_dev_open=%d", nport, spi_dev_open);

    if (spi_dev_open > 0) {
        LOG(WARN, "Superfluos call to spi_open()");
        return 0;
    }

    if (spi_nports == 0 || nport >= spi_nports) {
        SPI_ERR("No FTDI device found");
        goto open_err;
    }

#ifdef SPI_STATS
    memset(&spi_stats, 0, sizeof(spi_stats));
    if (gettimeofday(&spi_stats.tv_open_begin, NULL) < 0)
        LOG(WARN, "gettimeofday failed: %s", strerror(errno));
#endif

    rc = ftdi_set_interface(&ftdic, ftdi_intf);
    if (rc < 0)
    {
        SPI_ERR("FTDI: ftdi_set_interface() failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    /* Serial number can be unavailable due to SerNumEnable* settings in FTDI
     * EEPROM. Don't try to pass a serial number pointer for such a device to
     * ftdi_usb_open*(), or it will fail. See issue #7. */
    serial = spi_ports[nport].serial;
    if (serial[0] == '\0')
        serial = NULL;
    rc = ftdi_usb_open_desc(&ftdic, spi_ports[nport].vid, spi_ports[nport].pid,
            NULL, serial);
    if (rc < 0)
    {
        SPI_ERR("FTDI: ftdi_usb_open_desc() failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    spi_dev_open++;

    LOG(INFO, "FTDI: using FTDI device: \"%s\"", spi_ports[nport].name);

    rc = ftdi_usb_reset(&ftdic);
    if (rc < 0) {
        SPI_ERR("FTDI: reset failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    rc = ftdi_usb_purge_buffers(&ftdic);
    if (rc < 0) {
        SPI_ERR("FTDI: purge buffers failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    /* Set 1 ms latency timer, see FTDI AN232B-04 */
    rc = ftdi_set_latency_timer(&ftdic, 1);
    if (rc < 0) {
        SPI_ERR("FTDI: setting latency timer failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    rc = ftdi_set_bitmode(&ftdic, 0, BITMODE_RESET);
    if (rc < 0) {
        SPI_ERR("FTDI: reset bitmode failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    /* Set pins direction */
    output_pins = spi_pins->mosi | spi_pins->clk | spi_pins->ncs;
    if (spi_pins->nledr)
        output_pins |= spi_pins->nledr;
    if (spi_pins->nledw)
        output_pins |= spi_pins->nledw;
    rc = ftdi_set_bitmode(&ftdic, output_pins, BITMODE_SYNCBB);
    if (rc < 0) {
        SPI_ERR("FTDI: set synchronous bitbang mode failed: [%d] %s", rc, ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    /*
     * Note on buffer sizes:
     *
     * FT232R has 256 byte receive buffer and 128 byte transmit buffer. It works
     * like 384 byte buffer. See:
     * http://developer.intra2net.com/mailarchive/html/libftdi/2011/msg00410.html
     * http://developer.intra2net.com/mailarchive/html/libftdi/2011/msg00413.html
     * http://jdelfes.blogspot.ru/2014/03/ft232r-bitbang-spi-part-2.html
     *
     * FT2232C has 128 byte TX and 384 byte RX buffers per channel.
     * FT2232H has 4kB RX and TX buffers per channel.
     * FT4232H has 2kB RX and TX buffers per channel.
     * FT232H has 1 kB RX and TX buffers.
     * FT230X has 512 byte TX and RX buffers.
     */
    switch (ftdic.type) {
        case TYPE_AM:
            ftdi_type_str = "FT232AM";
            SPI_ERR("This chip type is not supported: %s", ftdi_type_str);
            goto open_err;
            break;
        case TYPE_BM:
            ftdi_type_str = "FT232BM";
            SPI_ERR("This chip type is not supported: %s", ftdi_type_str);
            goto open_err;
            break;
        case TYPE_2232C:
            ftdi_type_str = "FT2232C/D";
            ftdi_buf_size = 512;
            break;
        case TYPE_R:
            ftdi_type_str = "FT232R";
            ftdi_buf_size = 384;
            break;
        case TYPE_2232H:
            ftdi_type_str = "FT2232H";
            ftdi_buf_size = 8192;
            break;
        case TYPE_4232H:
            ftdi_type_str = "FT4232H";
            ftdi_buf_size = 4096;
            break;
        case TYPE_232H:
            ftdi_type_str = "FT232H";
            ftdi_buf_size = 2048;
            break;
        /* TYPE_230X is supported since libftdi1-1.2 */
        /*case TYPE_230X:
            ftdi_type_str = "FT230X";
            ftdi_buf_size = 1024;
            break;
        */
        default:
            LOG(WARN, "Unknown FTDI chip type, assuming FT232R");
            ftdi_type_str = "Unknown";
            ftdi_buf_size = 384;
            break;
    }

    LOG(INFO, "Detected %s type programmer chip, buffer size: %u",
            ftdi_type_str, ftdi_buf_size);

    /* Initialize xfer buffers */
    ftdi_out_buf = malloc(ftdi_buf_size);
    ftdi_in_buf = malloc(ftdi_buf_size);
    if (ftdi_out_buf == NULL || ftdi_in_buf == NULL) {
        SPI_ERR("Not enough memory");
        goto open_err;
    }
    ftdi_out_buf_offset = 0;

    /* Set initial pin state: CS high, MISO high as pullup, MOSI and CLK low, LEDs off */
    ftdi_pin_state = spi_pins->ncs | spi_pins->miso;
    if (spi_pins->nledr)
        ftdi_pin_state |= spi_pins->nledr;
    if (spi_pins->nledw)
        ftdi_pin_state |= spi_pins->nledw;
    ftdi_out_buf[ftdi_out_buf_offset++] = ftdi_pin_state;

    return 0;

open_err:
    if (spi_dev_open > 0)
        ftdi_usb_close(&ftdic);
    spi_dev_open = 0;

    return -1;
}

int spi_isopen(void)
{
    return spi_dev_open ? 1 : 0;
}

#ifdef SPI_STATS
void spi_output_stats(void)
{
    double xfer_pct = NAN, avg_read = NAN, avg_write = NAN, rate = NAN, iops = NAN;
    double ftdi_rate = NAN, ftdi_xfers_per_io = NAN, avg_ftdi_xfer = NAN, ftdi_short_rate = NAN;
    struct timeval tv;
    long inxfer_ms;
    FILE *fp;

    fp = log_get_dest();
    if (!fp)
        return;

    /* Calculate timeranges until now */
    if (gettimeofday(&tv, NULL) < 0)
        LOG(WARN, "gettimeofday failed: %s", strerror(errno));
    timersub(&tv, &spi_stats.tv_open_begin, &tv);
    timeradd(&spi_stats.tv_open, &tv, &spi_stats.tv_open);

    xfer_pct = avg_read = avg_write = rate = iops = NAN;
    ftdi_rate = ftdi_xfers_per_io = avg_ftdi_xfer = NAN;

    if (spi_stats.tv_open.tv_sec || spi_stats.tv_open.tv_usec) {
        xfer_pct = (spi_stats.tv_xfer.tv_sec * 1000 + spi_stats.tv_xfer.tv_usec / 1000);
        xfer_pct *= 100;
        xfer_pct /= (spi_stats.tv_open.tv_sec * 1000 + spi_stats.tv_open.tv_usec / 1000);
    }

    if (spi_stats.reads) {
        avg_read = spi_stats.read_bytes;
        avg_read /= spi_stats.reads;
    }

    if (spi_stats.writes) {
        avg_write = spi_stats.write_bytes;
        avg_write /= spi_stats.writes;
    }

    inxfer_ms = spi_stats.tv_xfer.tv_sec * 1000 + spi_stats.tv_xfer.tv_usec / 1000;
    if (inxfer_ms > 0) {
        rate = ((spi_stats.read_bytes + spi_stats.write_bytes) * 1000) /
            inxfer_ms;
        rate /= 1024;   /* In KB/s */

        iops = ((spi_stats.reads + spi_stats.writes) * 1000) / inxfer_ms;

        ftdi_rate = (spi_stats.ftdi_xfers * 1000) / inxfer_ms;

        ftdi_short_rate = (spi_stats.ftdi_short_reads * 1000) / inxfer_ms;
    }

    if (spi_stats.reads || spi_stats.writes)
        ftdi_xfers_per_io = spi_stats.ftdi_xfers / (spi_stats.reads + spi_stats.writes);

    if (spi_stats.ftdi_xfers)
        avg_ftdi_xfer = spi_stats.ftdi_bytes / spi_stats.ftdi_xfers;

    fprintf(fp,
            "*** FTDI Statistics ********************************************************\n"
            "csr-spi-ftdi version: " VERSION " (git rev " GIT_REVISION ")\n"
            "Time open: %ld.%02ld s\n"
            "Time in xfer: %ld.%02ld s (%.2f%% of open time)\n"
            "Reads: %ld (%ld bytes, %.2f bytes avg read size)\n"
            "Writes: %ld (%ld bytes, %.2f bytes avg write size)\n"
            "Xfer data rate: %.2f KB/s (%ld bytes in %ld.%02ld s)\n"
            "IOPS: %.2f IO/s (%ld IOs in %ld.%02ld s)\n"
            "FTDI chip: %s (%d), buffer size: %u bytes\n"
            "FTDI stats: %.2f xfers/s (%.2f short reads/s,\n"
            "            %ld xfers/%ld short reads in %ld.%02ld s,\n"
            "            %.2f xfers/IO, %.2f bytes/xfer)\n"
            "SPI max clock: %lu kHz, min clock: %lu kHz, slowdowns: %lu\n"
            "****************************************************************************\n",
            spi_stats.tv_open.tv_sec, spi_stats.tv_open.tv_usec / 10000,
            spi_stats.tv_xfer.tv_sec, spi_stats.tv_xfer.tv_usec / 10000, xfer_pct,
            spi_stats.reads, spi_stats.read_bytes, avg_read,
            spi_stats.writes, spi_stats.write_bytes, avg_write,
            rate, spi_stats.read_bytes + spi_stats.write_bytes,
                spi_stats.tv_xfer.tv_sec, spi_stats.tv_xfer.tv_usec / 10000,
            iops, spi_stats.reads + spi_stats.writes,
                spi_stats.tv_xfer.tv_sec, spi_stats.tv_xfer.tv_usec / 10000,
            ftdi_type_str, ftdic.type, ftdi_buf_size,
            ftdi_rate, ftdi_short_rate, spi_stats.ftdi_xfers,
                spi_stats.ftdi_short_reads, spi_stats.tv_xfer.tv_sec,
                spi_stats.tv_xfer.tv_usec / 10000, ftdi_xfers_per_io, avg_ftdi_xfer,
            spi_stats.spi_clock_max, spi_stats.spi_clock_min, spi_stats.slowdowns
    );
}
#endif

int spi_close(void)
{
    int rc;

    LOG(DEBUG, "spi_nrefs=%d, spi_dev_open=%d", spi_nrefs, spi_dev_open);

    if (spi_dev_open) {
        spi_led(SPI_LED_OFF);

        /* Flush and reset the buffers */
        if (ftdi_out_buf_offset) {
            if (spi_ftdi_xfer(ftdi_out_buf, ftdi_in_buf, ftdi_out_buf_offset) < 0)
                return -1;
            ftdi_out_buf_offset = 0;
        }

        rc = ftdi_set_bitmode(&ftdic, 0, BITMODE_RESET);
        if (rc < 0) {
            SPI_ERR("FTDI: reset bitmode failed: [%d] %s", rc,
                    ftdi_get_error_string(&ftdic));
            return -1;
        }

        rc = ftdi_usb_close(&ftdic);
        if (rc < 0) {
            SPI_ERR("FTDI: close failed: [%d] %s", rc,
                    ftdi_get_error_string(&ftdic));
            return -1;
        }
#ifdef SPI_STATS
        spi_output_stats();
#endif

        free(ftdi_out_buf);
        free(ftdi_in_buf);
        ftdi_out_buf = ftdi_in_buf = NULL;
        ftdi_buf_size = 0;

        spi_dev_open = 0;
    }

    return 0;
}
