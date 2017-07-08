/****************************************************************************
 *                                                                          *
 *                      COPYRIGHT (c) 1988 - 2017                           *
 *                       This Software Provided                             *
 *                               By                                         *
 *                      Robin's Nest Software Inc.                          *
 *                                                                          *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,     *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permissikn notice appear in the      *
 * supporting documentation, and that the name of the author not be used    *
 * in advertising or publicity pertaining to distribution of the software   *
 * without specific, written prior permission.                              *
 *                                                                          *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,        *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN      *
 * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
 * THIS SOFTWARE.                                                           *
 *                                                                          *
 ****************************************************************************/
/*
 * Modification History:
 *
 * September 17th, 2015 by Robin T Miller
 *  Add btag extension for tracking previous writes, for write dependencies.
 */

#if !defined(_DTBTAG_H_)
#define _DTBTAG_H_ 1

/* 
 * Our unique block tag signature!
 */
#define BTAG_SIGNATURE  0xbadcafee

/* 
 * The host and serial sizes include a NULL byte!
 * Note: Host names can be very long, but we don't wish to waste space. 
 */ 
#define HOST_SIZE       24

/* 
 * Note: NetApp serial numbers are only 12 characters, but we round up!
 */
#define SERIAL_SIZE     16              /* SAN LUN serial number size. */
/* 
 * FYI: The device identifier may be more appropriate. than the serial #.
 */
#define DEVICEID_SIZE   40              /* Device identifier size.    */

#define BTAG_VERSION_1   1

/* 
 * Opaque Data Types:
 */
#define OPAQUE_NO_DATA_TYPE      0
#define OPAQUE_WRITE_ORDER_TYPE  1

/* 
 * Block Tag Flags:
 */
#define BTAG_FILE           0x01        /* File = 1, Disk = 0 */
#define BTAG_OPAQUE         0x02        /* Opaque area follows the btag. */
#define BTAG_PREFIX         0x04        /* Prefix string prior to pattern. */
#define BTAG_RANDOM         0x08        /* Random I/O = 1, Sequential = 0. */
#define BTAG_REVERSE        0x10        /* Reverse I/O = 1, Forward = 0. */

/* 
 * Block Tag Verify Flags:
 */
#define BTAGV_LBA           0x1
#define BTAGV_OFFSET        0x2
#define BTAGV_DEVID         0x4
#define BTAGV_INODE         0x8
#define BTAGV_SERIAL        0x10
#define BTAGV_HOSTNAME      0x20
#define BTAGV_SIGNATURE     0x40
#define BTAGV_VERSION       0x80
#define BTAGV_PATTERN_TYPE  0x100
#define BTAGV_FLAGS         0x200
#define BTAGV_WRITE_START   0x400
#define BTAGV_WRITE_SECS    0x1000
#define BTAGV_WRITE_USECS   0x2000
#define BTAGV_PATTERN       0x4000
#define BTAGV_GENERATION    0x8000
#define BTAGV_PROCESS_ID    0x10000
#define BTAGV_JOB_ID        0x20000
#define BTAGV_THREAD_NUMBER 0x40000
#define BTAGV_DEVICE_SIZE   0x80000
#define BTAGV_RECORD_INDEX  0x100000
#define BTAGV_RECORD_SIZE   0x200000
#define BTAGV_RECORD_NUMBER 0x400000
#define BTAGV_STEP_OFFSET   0x800000
#define BTAGV_OPAQUE_DATA_TYPE  0x1000000
#define BTAGV_OPAQUE_DATA_SIZE  0x2000000
#define BTAGV_CRC32         0x4000000
#define BTAGV_OPAQUE_DATA   0x8000000

/*
 * Verify Flags for ALL Data.
 *  
 * Note: These are the fields checked *after* the CRC is verified! 
 * These additonal checks are required since the CRC may be correct, 
 * but the btag may be stale or from a defferent device.
 */
#define BTAGV_ALL \
  ( BTAGV_LBA | \
    BTAGV_OFFSET | \
    BTAGV_DEVID | \
    BTAGV_INODE | \
    BTAGV_SERIAL | \
    BTAGV_HOSTNAME | \
    BTAGV_SIGNATURE | \
    BTAGV_VERSION | \
    BTAGV_PATTERN_TYPE | \
    BTAGV_FLAGS | \
    BTAGV_WRITE_START | \
    BTAGV_WRITE_SECS | \
    BTAGV_WRITE_USECS | \
    BTAGV_PATTERN | \
    BTAGV_GENERATION | \
    BTAGV_PROCESS_ID | \
    BTAGV_JOB_ID | \
    BTAGV_THREAD_NUMBER | \
    BTAGV_DEVICE_SIZE | \
    BTAGV_RECORD_INDEX | \
    BTAGV_RECORD_SIZE | \
    BTAGV_RECORD_NUMBER | \
    BTAGV_STEP_OFFSET | \
    BTAGV_OPAQUE_DATA_TYPE | \
    BTAGV_OPAQUE_DATA_SIZE | \
    BTAGV_OPAQUE_DATA | \
    BTAGV_CRC32 )

/* 
 * Flags for a Quick Verify. 
 *  
 * Note: QV provides faster verification, but may miss some corruptions. 
 */
#define BTAGV_QV \
  ( BTAGV_LBA | \
    BTAGV_OFFSET | \
    BTAGV_INODE | \
    BTAGV_SERIAL | \
    BTAGV_HOSTNAME | \
    BTAGV_SIGNATURE | \
    BTAGV_PATTERN_TYPE | \
    BTAGV_FLAGS | \
    BTAGV_WRITE_START | \
    BTAGV_WRITE_SECS | \
    BTAGV_WRITE_USECS | \
    BTAGV_PATTERN | \
    BTAGV_GENERATION | \
    BTAGV_PROCESS_ID | \
    BTAGV_JOB_ID | \
    BTAGV_THREAD_NUMBER | \
    BTAGV_CRC32 | \
    BTAGV_OPAQUE_DATA )

/* 
 * Flags disabled for random I/O due to overwrites.
 */
#define BTAGV_RANDOM_DISABLE \
  ( BTAGV_WRITE_SECS | BTAGV_WRITE_USECS | BTAGV_RECORD_INDEX | BTAGV_RECORD_SIZE | BTAGV_RECORD_NUMBER )

/* 
 * Flags disabled for read-only.
 *
 * Note: Most folks use a runtime, so disable generation check.
 * Disable checking the flags, since I/O dir or type may vary.
 * Disable checking record information, may be variable sizes.
 */
#define BTAGV_READONLY_DISABLE \
  ( BTAGV_GENERATION | BTAGV_FLAGS | \
    BTAGV_WRITE_START | BTAGV_WRITE_SECS | BTAGV_WRITE_USECS | \
    BTAGV_PROCESS_ID | BTAGV_JOB_ID | BTAGV_THREAD_NUMBER |  \
    BTAGV_RECORD_INDEX | BTAGV_RECORD_SIZE | BTAGV_RECORD_NUMBER )

/* 
 * Note: enum's not used, to save space! (enum's allocate an int)
 */
#define PTYPE_IOT       1
#define PTYPE_INCR      2
#define PTYPE_PATTERN   3
#define PTYPE_PFILE     4
#define PTYPE_MASK      0x3f
#define PTYPE_LBDATA    0x40
#define PTYPE_TIMESTAMP 0x80

/*
 * Block Tag (btag) Definition:
 * Note: Pack carefully to avoid wasted space due to alignment!
 */
typedef struct btag {                                             /* Offset */
    union {                         /*                                  0 */
        uint64_t lba;               /* LBA of this block (raw IO).        */
        int64_t offset;             /* File offset (file systems).        */
    } btag_u0;
    union {                         /*                                  8 */
        uint32_t devid;             /* OS device major/minor.             */
        int64_t inode;              /* FS file i-node number.             */
    } btag_u1;
    /* Note: This belongs in SAN btag extension, but want here for traces! */
    char     btag_serial[SERIAL_SIZE]; /* The SAN LUN serial number.   16 */
    char     btag_hostname[HOST_SIZE]; /* The host name (ASCII).       32 */
    uint32_t btag_signature;        /* Our unique binary signature.    56 */
    uint8_t  btag_version;          /* The block type version.         60 */
    uint8_t  btag_pattern_type;     /* The pattern type.               61 */
    uint16_t btag_flags;            /* Various information flags.      62 */
    int32_t  btag_write_start;      /* The write start time (secs).    64 */
    int32_t  btag_write_secs;       /* This write time stamp (secs).   68 */
    int32_t  btag_write_usecs;      /* This write time stamp (usecs)   72 */
    uint32_t btag_pattern;          /* The current pattern.            76 */
    uint32_t btag_generation;       /* Generation number.              80 */
    uint32_t btag_process_id;       /* Process ID.                     84 */
    uint32_t btag_job_id;           /* The job identifier.             88 */
    uint32_t btag_thread_number;    /* The thread number.              92 */
    uint32_t btag_device_size;      /* The device/block size.          96 */
    uint32_t btag_record_index;     /* The record index.              100 */
    uint32_t btag_record_size;      /* The record size.               104 */
    uint32_t btag_record_number;    /* The record number.             108 */
    uint64_t btag_step_offset;      /* The record number.             112 */
    uint8_t  btag_opaque_data_type; /* The type of opaque data.       120 */
    uint16_t btag_opaque_data_size; /* The size of the opaque data.   122 */
    uint32_t btag_crc32;            /* The 32-bit CRC value.          124 */
    /* Note: Opaque and block data are packed in this order!          128 */
  //uint8_t  btag_opaque_data[0];   /* Opaque data for extensions.        */
  //uint8_t  btag_block_data[0];    /* The actual block data pattern.     */
} btag_t;

#define btag_lba    btag_u0.lba
#define btag_offset btag_u0.offset

#define btag_devid  btag_u1.devid
#define btag_inode  btag_u1.inode

#define getBtagSize(btag) sizeof(*btag) + LtoH16(btag->btag_opaque_data_size);

/*
 * Write Order Block Tag Definitions:
 */
typedef struct btag_write_order {
    uint8_t  wro_device_index;      /* The device index.                128 */
    uint32_t wro_write_size;        /* The write size.                  132 */
    int64_t  wro_write_offset;      /* The write offset.                136 */
    int32_t  wro_write_secs;        /* The write time stamp (secs).     144 */
    int32_t  wro_write_usecs;       /* The write time stamp (usecs).    148 */
    uint32_t wro_crc32;             /* The CRC of previous btag.        152 */
                                    /*          Total block tag size -> 156 */
                                    /*    Total size of ordered btag ->  28 */
} btag_write_order_t;

#endif /* !defined(_DTBTAG_H_) */
