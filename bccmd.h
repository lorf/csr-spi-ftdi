#ifndef _BCCMD_H
#define _BCCMD_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push)

/* This is simple representation of BCCMD PDU, found in
 * BlueSuiteSource_V2_5.zip/CSRSource/interface/host/bccmd/bccmdpdu.h */

#pragma pack(2)
typedef struct {
    /* PDU type. */
    uint16_t          type;

/* Acceptable values for "type". */
#define BCCMDPDU_GETREQ         (0)
#define BCCMDPDU_GETRESP        (1)
#define BCCMDPDU_SETREQ         (2)
/* #define BCCMDPDU_GETNEXTREQ  (3)  may be added. */

    /* Total PDU length in uint16s. */
    uint16_t          pdulen;
    /* To get payload length, subtract BCCMDPDU_OVERHEAD_LEN (see below) */

/* Maximum total length of a PDU. */
#define BCCMDPDU_MAXLEN         (64)

    /* Sequence number. */
    uint16_t          seqno;

    /* Identifier of variable to be accessed. */
    uint16_t          varid;

    /* How the bccmd managed to perform the operation. */
    uint16_t          status;

/* Acceptable values for "status". */
#define BCCMDPDU_STAT_OK        (0)     /* Default.  No problem found. */
#define BCCMDPDU_STAT_NO_SUCH_VARID (1) /* varid not recognised. */
#define BCCMDPDU_STAT_TOO_BIG   (2)     /* Data exceeded pdu/mem capacity. */
#define BCCMDPDU_STAT_NO_VALUE  (3)     /* Variable has no value. */
#define BCCMDPDU_STAT_BAD_PDU   (4)     /* Bad value(s) in PDU. */
#define BCCMDPDU_STAT_NO_ACCESS (5)     /* Value of varid inaccessible. */
#define BCCMDPDU_STAT_READ_ONLY (6)     /* Value of varid unwritable. */
#define BCCMDPDU_STAT_WRITE_ONLY (7)    /* Value of varid unreadable. */
#define BCCMDPDU_STAT_ERROR     (8)     /* The useless catch-all error. */
#define BCCMDPDU_STAT_PERMISSION_DENIED (9) /* Request not allowed. */
#define BCCMDPDU_STAT_TIMEOUT   (10)    /* Deferred command timed-out. */

/***************************************************************************
That's the end of the common header of a bccmdpdu.  This define gives its
length in the units used for pdulen.
***************************************************************************/

#define BCCMDPDU_OVERHEAD_LEN 5

	union {
		uint8_t u8d[];
        uint16_t u16d[];
	} d;
} BCCMDPDU;


// ref: BlueSuiteSource_V2_5/CSRSource/interface/host/bccmd/bccmd_spi_common.h
/*
 * A block of data viewed by the host and chip code.  This provides the gateway
 * through which commands and responses are conveyed.
 *
 * This is used directly as a C definition by the code on the chip.  The host
 * code must work with this layout.  In practice this means it is unlikely that
 * the host will be able to use this typedef directly.
 */

#pragma pack(2)
typedef struct {
    uint16_t          cmd;          /* State variable and access cntl. */

/*
 * States of the "cmd" field control the feeding of commands to bccmd (over
 * SPI) via the buffer pointed by bccmd_spi_buffer.buffer, and the returning
 * the results to the caller in the same buffer.   Values for "cmd" are below.
 */

#define BCCMD_SPI_CMD_IDLE       (0)  /* British Rail porter. */
#define BCCMD_SPI_CMD_ALLOC_REQ  (1)  /* Host wants to write a command. */
#define BCCMD_SPI_CMD_ALLOC_OK   (2)  /* Chip has allocated a buffer. */
#define BCCMD_SPI_CMD_ALLOC_FAIL (3)  /* Chip cannot provide the buffer. */
#define BCCMD_SPI_CMD_CMD        (4)  /* Host has written a command. */
#define BCCMD_SPI_CMD_PENDING    (5)  /* Chip is executing command. */
#define BCCMD_SPI_CMD_RESP       (6)  /* Chip has written the result. */
#define BCCMD_SPI_CMD_DONE       (7)  /* Host has finished with the buffer. */

    uint16_t          size;         /* Requested size of *buffer. */
    uint16_t*         buffer;       /* bccmd command/response buffer. */
} BCCMD_SPI_INTERFACE;

/* The following rules surround use of the structure.

When "cmd" is IDLE:
    - the host may write to "size", (then)
    - the host may change "cmd" to ALLOC_REQ.

When "cmd" is ALLOC_REQ:
    - the chip may (allocate a buffer of size "size" uint16s and) write the
      buffer's address into "buffer", (then)
    - the chip may change "cmd" to ALLOC_OK or ALLOC_FAIL.

When "cmd" is ALLOC_OK:
    - the host may write to the buffer pointed to by "buffer", knowing it
      is at least "size" uint16s long, (then)
    - the host may change "cmd" to CMD.

When "cmd" is ALLOC_FAIL:
    - the host may write to "size", (then)
    - the host may change "cmd" to ALLOC_REQ.

When "cmd" is CMD:
    - the chip may change "cmd" to PENDING.

When "cmd" is PENDING:
    - the chip may write to the buffer pointed to by "buffer", knowing it
      is at least "size" uint16s long, (then)
    - the chip may change "cmd" to RESP.

When "cmd" is RESP:
    - the host may change "cmd" to DONE.

When "cmd" is DONE:
    - the chip may (free the memory pointed to by "buffer" and) set
      "buffer" to NULL.

When "cmd" is DONE:
    - the chip may change "cmd" to IDLE.
*/

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif
