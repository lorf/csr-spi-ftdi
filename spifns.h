#ifndef SPIFNS_H
#define SPIFNS_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define DLLEXPORT /* Empty */
#else
#define DLLEXPORT __declspec(dllexport)
#endif

#define SPIFNS_API_1_3  0x103
#define SPIFNS_API_1_4  0x104

/* From BlueSuiteSource_V2_5.zip/CSRSource/result/include/spi/spi_common.h */

// Pre BC7 chips have version at this address
#define GBL_CHIP_VERSION_GEN1_ADDR  0xFF9A
// Unifi chips and post BC7 chips have version at this address
#define GBL_CHIP_VERSION_GEN2_ADDR  0xFE81
// PreBC7 chips have a analogue version
#define GBL_ANA_VERSION_ID          0xFF7D

/* From BlueSuiteSource_V2_5.zip/CSRSource/result/include/chiputil/XapHelper.h */
/* Macros to extract version information bits */
#define DIGITAL_CHIP_VERSION(reg_ff9a) (((reg_ff9a) >> 8) & 0x1F)
#define CHIP_VERSION_FE81(reg_fe81) ((reg_fe81) & 0xFF)
#define CHIP_VARIANT_FE81(reg_fe81) ((reg_fe81) & 0x0F00)

/**
 * Retrieves the ana version id from the register content
 * @param anaVersion the content of the register
 */
#define CHIP_ANA_VERSION_ID(anaVersion) ((anaVersion) & 0x00FF)


/* From BlueSuiteSource_V2_5.zip/CSRSource/interface/host/io/chipversions.h */
#if 0
typedef enum
{
    chip_bc01a = 0x01,
    chip_bc01b = 0x02,
    chip_bc02 = 0x03,
    chip_kato = 0x04,
    chip_kalimba = 0x05,
    chip_nicknack = 0x06,
    chip_paddywack = 0x07,
    chip_coyote = 0x08,
    chip_oddjob = 0x09,
    chip_pugwash = 0x0C,
    chip_stanley = 0x0D,
    chip_zebedee = 0x0E,
    chip_elvis = 0x10,
    chip_johnpeel = 0x11,
    chip_jumpinjack = 0x0F,
    chip_priscilla = 0x12,
    chip_herbie = 0x13,
    chip_fraggle = 0x14,
    chip_sugarlump = 0x15,
    chip_lisamarie = 0x16,
    chip_dash = 0x20,
    chip_jemima = 0x21,
    chip_cinderella = 0x22,
    chip_anastasia = 0x23
}
chipversion;
#endif
/* Continuation from BlueSuiteSource_V2_5.zip/CSRSource/result/include/chiputil/XapHelper.h */
#if 0
    RAW_CHIP_VERSION_LPC                = 0x24,
    RAW_CHIP_VERSION_OXY                = 0x27,
    RAW_CHIP_VERSION_GDN                = 0x28,
    RAW_CHIP_VERSION_GEM                = 0x29,
    RAW_CHIP_VERSION_NUT                = 0x2A,
    RAW_CHIP_VERSION_ROB                = 0x2B,
    RAW_CHIP_VERSION_LEO                = 0x2D,
    RAW_CHIP_VERSION_PUR                = 0x30,
    RAW_CHIP_VERSION_VGA                = 0x31,
    RAW_CHIP_VERSION_DAL                = 0x32,
    RAW_CHIP_VERSION_VUL                = 0x33,
    RAW_CHIP_VERSION_SS5                = 0x41,
    RAW_CHIP_VERSION_WAL                = 0x43,
    RAW_CHIP_VERSION_AMB                = 0x44,
#endif

/* From BlueSuiteSource_V2_5.zip/CSRSource/result/include/spi/spifns.h */

#define SPIFNS_SUCCESS (0)
#define SPIFNS_FAILURE (1)

/* Return type of spifns_bluecore_xap_stopped() and spifns_stream_bluecore_xap_stopped() */
enum spifns_xap_state
{
    SPIFNS_XAP_STOPPED = 1,
    SPIFNS_XAP_RUNNING = 0,
    SPIFNS_XAP_NO_REPLY = -1,
    SPIFNS_XAP_SPI_LOCKED = -2,
    SPIFNS_XAP_NOT_IMPL = -3,
};

enum spifns_spi_errors
{
    SPIFNS_ERROR_NO_ERROR  = 0x000, /* tests equal to SPIFNS_SUCCESS and PTTRANS_SUCCESS */
    SPIFNS_ERROR_GENERAL_FAILURE = SPIFNS_FAILURE, /* tests equal to SPIFNS_FAILURE and PTTRANS_FAILURE */
    /* the spifns error range is 0x100-0x1FF */
    SPIFNS_ERROR_MEMORY    = 0x101,
    SPIFNS_ERROR_CONNECT   = 0x102,
    SPIFNS_ERROR_READ_FAIL = 0x103, /* The checksum read from MISO after a read command was incorrect. */
    SPIFNS_ERROR_OS_ERROR  = 0x104,
    SPIFNS_ERROR_TOO_MANY_STREAMS = 0x105,
    SPIFNS_ERROR_INVALID_STREAM = 0x106,
    SPIFNS_ERROR_ALREADY_OPEN = 0x107,
    SPIFNS_ERROR_WRITE_FAIL = 0x108,
    SPIFNS_ERROR_INVALID_ADDRESS = 0x109,
    SPIFNS_ERROR_INVALID_PARAMETER = 0x10A,
    SPIFNS_ERROR_TIMEOUT   = 0x10B,
};


#define SPIERR_NO_ERROR 0x100
#define SPIERR_MALLOC_FAILED 0x101
#define SPIERR_NO_LPT_PORT_SELECTED 0x102
#define SPIERR_READ_FAILED 0x103
#define SPIERR_IOCTL_FAILED 0x104

struct SPIVARDEF {
    const char *szName;
    const char *szDefault;
    int nUnknown;
};

struct SPISEQ {
    enum {
        TYPE_READ=0,
        TYPE_WRITE,
        TYPE_SETVAR,
    } nType;
    union {
        struct {
            unsigned short nAddress;
            unsigned short nLength;
            unsigned short *pnData;
        } rw;
        struct {
            const char *szName;
            const char *szValue;
        } setvar;
    };
};

typedef void (__cdecl *spifns_enumerate_ports_callback)(unsigned int nPortNumber, const char *szPortName, void *pData);
typedef void (__cdecl *spifns_debug_callback)(const char *szDebug);

DLLEXPORT int spifns_init(); //Return 0 on no error, negative on error
DLLEXPORT void spifns_close();
DLLEXPORT void spifns_getvarlist(const SPIVARDEF **ppList, unsigned int *pnCount);
DLLEXPORT const char* spifns_getvar(const char *szName);
DLLEXPORT uint32_t spifns_get_version();
DLLEXPORT void spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData);
#define CHIP_SELECT_XILINX      (0)
#define CHIP_SELECT_SPARTAN     (1)
#define CHIP_SELECT_NONE        (-1)
DLLEXPORT void spifns_chip_select(int nUnknown);
DLLEXPORT const char* spifns_command(const char *szCmd); //Return 0 on no error, or string on error.
/* returns the last error code, and if a pointer is passed in, the problematic address.*/
/* get_last_error and clear_last_error both deal with the error that occurred in the current thread */
DLLEXPORT unsigned int spifns_get_last_error(unsigned short *pnErrorAddress, const char **szErrorString); //Returns where the error occured, or 0x100 for none
DLLEXPORT int spifns_bluecore_xap_stopped(); //Returns -1 on error, 0 on XAP running, 1 on stopped
DLLEXPORT int spifns_sequence(SPISEQ *pSequence, unsigned int nCount); //Return 0 on no error
DLLEXPORT int spifns_bccmd_init(unsigned int _spi_user_cmd_addr, unsigned int _bccmd_spi_interface_addr);
DLLEXPORT int spifns_bccmd_cmd(uint16_t *pdu, unsigned int _pdulen, unsigned int wait_for_result);
DLLEXPORT void spifns_set_debug_callback(spifns_debug_callback pCallback);
DLLEXPORT void spifns_clear_last_error(void);


/* SPI API 1.4 structure */
/* From BlueSuiteSource_V2_5.zip/CSRSource/result/include/spi/spifns.h */

struct SPISEQ_1_4 {
    enum {
        TYPE_READ=0,
        TYPE_WRITE,
        TYPE_SETVAR,
        TYPE_READ_VERIFY,
        TYPE_WRITE_VERIFY,
        TYPE_READ_BYTE,
        TYPE_WRITE_BYTE,
        TYPE_READ_BYTE_VERIFY,
        TYPE_WRITE_BYTE_VERIFY,
    } nType;
    union {
        struct {
            uint32_t nAddress;
            uint32_t nLength;
            unsigned short *pnData;
        } rw;
        struct {
            const char *szName;
            const char *szValue;
        } setvar;
    };
};

typedef unsigned int spifns_stream_t;
#define SPIFNS_STREAM_INVALID 0x7FFFFFFF
#define SPIFNS_STREAMS_EQUAL(stream1, stream2) ((stream1)==(stream2))


DLLEXPORT unsigned int spifns_count_streams(void);
DLLEXPORT int spifns_stream_init(spifns_stream_t *p_stream);
DLLEXPORT void spifns_stream_close(spifns_stream_t stream);
DLLEXPORT unsigned int spifns_count_streams(void);
DLLEXPORT int spifns_stream_sequence(spifns_stream_t stream, SPISEQ_1_4 *pSequence, int nCount);
DLLEXPORT const char* spifns_stream_command(spifns_stream_t stream, const char *command);
DLLEXPORT const char* spifns_stream_getvar(spifns_stream_t stream, const char *var);
DLLEXPORT void spifns_stream_chip_select(spifns_stream_t stream, int which);
DLLEXPORT int spifns_stream_bluecore_xap_stopped(spifns_stream_t stream);
DLLEXPORT int spifns_stream_bccmd_init(spifns_stream_t stream, unsigned int _spi_user_cmd_addr, unsigned int _bccmd_spi_interface_addr);
DLLEXPORT int spifns_stream_bccmd_cmd(spifns_stream_t stream, uint16_t *pdu, unsigned int _pdulen, unsigned int wait_for_result);
/* returns the last error code, and if a pointer is passed in, the problematic
 * address.*/
/* get_last_error and clear_last_error both deal with the error that occurred
 * in the current thread */
DLLEXPORT int spifns_get_last_error32(uint32_t *addr, const char ** buf);
DLLEXPORT void spifns_stream_set_debug_callback(spifns_stream_t stream, spifns_debug_callback fn, void *pvcontext);
DLLEXPORT int spifns_stream_get_device_id(spifns_stream_t stream, char *buf, size_t length);
DLLEXPORT int spifns_stream_lock(spifns_stream_t stream, uint32_t timeout);
DLLEXPORT void spifns_stream_unlock(spifns_stream_t stream);

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif  /* SPIFNS_H */
