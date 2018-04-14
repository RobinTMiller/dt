#if !defined(SCSI_OPCODES_H)
#define SCSI_OPCODES_H 1

/************************************************************************
 *									*
 * File:	scsi_opcodes.h						*
 * Date:	May 7, 1991						*
 * Author:	Robin T. Miller						*
 *									*
 * Description:								*
 *	SCSI Operation Codes.  The operation codes defined in this file	*
 * were taken directly from the ANSI SCSI-2 Specification.		*
 *									*
 * Modification History:						*
 *									*
 * August 18th, 2007 by Robin T. Miller					*
 *	Rather dated file, but adding a few new opcodes.		*
 *									*
 ************************************************************************/

/*
 * SCSI Operation Code Information:
 */ 
typedef struct scsi_opcodes {
    unsigned char opcode;		/* The SCSI operation code.	*/
    unsigned char subcode;		/* The operation code (if any). */
    unsigned short device_mask;		/* The device types(s). (mask)	*/
    char	  *opname;		/* The ASCII opcode name.	*/
    scsi_data_dir_t data_dir;		/* The data direction. (host)	*/
    int (*encode)(void *arg);		/* Function to encode CDB data.	*/
    int (*decode)(void *arg);		/* Function to decode CDB data.	*/
    unsigned int default_blocks;	/* The default CDB block len.	*/
} scsi_opcode_t;

#if !defined(SCSI_GROUP_0)
/*
 * Define Masks for SCSI Group Codes.
 */
#define	SCSI_GROUP_0		0x00	/* SCSI Group Code 0.		*/
#define SCSI_GROUP_1		0x20	/* SCSI Group Code 1.		*/
#define SCSI_GROUP_2		0x40	/* SCSI Group Code 2.		*/
#define SCSI_GROUP_3		0x60	/* SCSI Group Code 3.		*/
#define SCSI_GROUP_4		0x80	/* SCSI Group Code 4.		*/
#define SCSI_GROUP_5		0xA0	/* SCSI Group Code 5.		*/
#define SCSI_GROUP_6		0xC0	/* SCSI Group Code 6.		*/
#define SCSI_GROUP_7		0xE0	/* SCSI Group Code 7.		*/
#define SCSI_GROUP_MASK		0xE0	/* SCSI Group Code mask.	*/

#endif /* !defined(SCSI_GROUP_0) */

/*
 * Define max Logical Block Address (LBA) and lengths for all I/O CDB's.
 */
#define SCSI_MAX_LBA		0x1FFFFF	/* Max 6-byte LBA.	*/
#define SCSI_MAX_BLOCKS		0xFF		/* Max 6-byte blocks.	*/
#define SCSI_MAX_LBA10		0xFFFFFFFF	/* Max 10-byte LBA.	*/
#define SCSI_MAX_BLOCKS10	0xFFFF		/* Max 10-byte blocks.	*/
#define SCSI_MAX_LBA16		0xFFFFFFFFFFFFFFFFLL /* Max 16-byte LBA	*/
#define SCSI_MAX_BLOCKS16	0xFFFFFFFF	/* Max 16-byte blocks.	*/

#define XCOPY_MAX_BLOCKS_PER_SEGMENT 0xFFFF	/* Max blocks per segment. */
#define XCOPY_MAX_SEGMENT_LENGTH     65535	/* That's 32M-b of blocks. */

#define XCOPY_PT_MAX_BLOCKS	     16384	/* Max blocks all desc.    */
						/* That's 0x4000 or 8MB!   */
#define XCOPY_PT_MAX_DESCRIPTORS     8		/* The max descriptors.    */
#define XCOPY_PT_MAX_BLOCKS_PER_SEGMENT (XCOPY_PT_MAX_BLOCKS / XCOPY_PT_MAX_DESCRIPTORS)
						/* Max blocks per segment. */

/* Get LBA Status Definitions: */
#define GLS_MAX_LBA	0xFFFFFFFFFFFFFFFFLL	/* Max Get LBA Status LBA.  */
#define GLS_MAX_BLOCKS		8192		/* Max blocks per request.  */

/*
 * Compare and Write (CAW) Definitions:
 */
#define CAW_DEFAULT_BLOCKS	1		/* The default CDB blocks.  */

/* 
 * Unmap Definitions:
 */
#define UNMAP_MAX_LBA	0xFFFFFFFFFFFFFFFFLL	/* Max Unmap LBA.	    */
#define UNMAP_MAX_BLOCKS	0x80000	/* 256MB - Max blocks per request.  */
#define UNMAP_MAX_PER_RANGE	0x80000		/* Max blocks per range.    */
#define UNMAP_MAX_RANGES	128		/* Max number of ranges.    */

/*
 * SCSI Operation Codes for All Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_GET_CONFIGURATION			0x46
#define SOPC_INQUIRY				0x12
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_READ_BUFFER			0x3C
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_WRITE_BUFFER			0x3B
#define SOPC_PERSISTENT_RESERVE_IN		0x5E
#define SOPC_PERSISTENT_RESERVE_OUT		0x5F
#define SOPC_REPORT_LUNS			0xA0
#define SOPC_MAINTENANCE_IN			0xA3

/*
 * SCSI Operation Codes for Direct-Access Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_FORMAT_UNIT			0x04
#define SOPC_INQUIRY				0x12
#define SOPC_LOCK_UNLOCK_CACHE			0x36
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_PREFETCH				0x34
#define SOPC_PREVENT_ALLOW_REMOVAL		0x1E
#define SOPC_READ_6				0x08
#define SOPC_READ_10				0x28
#define SOPC_READ_BUFFER			0x3C
#define SOPC_READ_CAPACITY			0x25
#define SOPC_READ_DEFECT_DATA			0x37
#define SOPC_READ_LONG				0x3E
#define SOPC_REASSIGN_BLOCKS			0x07
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_RELEASE				0x17
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_RESERVE				0x16
#define SOPC_REZERO_UNIT			0x01
#define SOPC_SEARCH_DATA_EQUAL			0x31
#define SOPC_SEARCH_DATA_HIGH			0x30
#define SOPC_SEARCH_DATA_LOW			0x32
#define SOPC_SEEK_6				0x0B
#define SOPC_SEEK_10				0x2B
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_SET_LIMITS				0x33
#define SOPC_START_STOP_UNIT			0x1B
#define SOPC_SYNCHRONIZE_CACHE			0x35
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_UNMAP				0x42
#define SOPC_VERIFY				0x2F
#define SOPC_WRITE_6				0x0A
#define SOPC_WRITE_10				0x2A
#define SOPC_WRITE_VERIFY			0x2E
#define SOPC_WRITE_BUFFER			0x3B
#define SOPC_WRITE_LONG				0x3F
#define SOPC_WRITE_SAME				0x41

/*
 * Extended Copy (XCOPY) Service Actions:
 */ 
typedef enum {
    SCSI_XCOPY_EXTENDED_COPY_LID1	= 0x00,
    SCSI_XCOPY_POPULATE_TOKEN 		= 0x10,
    SCSI_XCOPY_WRITE_USING_TOKEN 	= 0x11,
} scsi_xcopy_service_action_t;

/*
 * 16-byte Opcodes:
 */
#define SOPC_EXTENDED_COPY			0x83
#define SOPC_RECEIVE_COPY_RESULTS		0x84
#define SOPC_RECEIVE_ROD_TOKEN_INFO		0x84
#  define ROD_TOKEN_SIZE			512
#define SOPC_READ_16				0x88
#define SOPC_WRITE_16				0x8A
#define SOPC_WRITE_AND_VERIFY_16		0x8E
#define SOPC_VERIFY_16				0x8F
#define SOPC_SYNCHRONIZE_CACHE_16		0x91
#define SOPC_WRITE_SAME_16			0x93
#define SOPC_SERVICE_ACTION_IN_16		0x9E
#define SOPC_COMPARE_AND_WRITE			0x89

typedef enum {
    SCSI_SERVICE_ACTION_READ_CAPACITY_16        = 0x10,
    SCSI_SERVICE_ACTION_GET_LBA_STATUS          = 0x12,
} scsi_service_action_t;

/*
 * SCSI Operation Codes for Sequential-Access Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_ERASE				0x19
#define SOPC_ERASE_16				0x93
#define SOPC_INQUIRY				0x12
#define SOPC_LOAD_UNLOAD			0x1B
#define SOPC_LOCATE				0x2B
#define SOPC_LOCATE_16				0x92
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_PREVENT_ALLOW_REMOVAL		0x1E
#define SOPC_READ				0x08
#define SOPC_READ_BLOCK_LIMITS			0x05
#define SOPC_READ_BUFFER			0x3C
#define SOPC_READ_POSITION			0x34
#define SOPC_READ_REVERSE			0x0F
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_RECOVER_BUFFERED_DATA		0x14
#define SOPC_RELEASE_UNIT			0x17
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_RESERVE_UNIT			0x16
#define SOPC_REWIND				0x01
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_SPACE				0x11
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_VERIFY_TAPE			0x13
#define SOPC_WRITE				0x0A
#define SOPC_WRITE_BUFFER			0x3B
#define SOPC_WRITE_FILEMARKS			0x10

/*
 * SCSI Operation Codes for Printer Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_FORMAT				0x04
#define SOPC_INQUIRY				0x12
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_PRINT				0x0A
#define SOPC_READ_BUFFER			0x3C
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_RECOVER_BUFFERED_DATA		0x14
#define SOPC_RELEASE_UNIT			0x17
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_RESERVE_UNIT			0x16
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_SLEW_PRINT				0x0B
#define SOPC_STOP_PRINT				0x1B
#define SOPC_SYNCHRONIZE_BUFFER			0x10
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_WRITE_BUFFER			0x3B

/*
 * SCSI Operation Codes for Processor Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_INQUIRY				0x12
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_READ_BUFFER			0x3C
#define SOPC_RECEIVE				0x08
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_SEND				0x0A
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_WRITE_BUFFER			0x3B

/*
 * SCSI Operation Codes for Write-Once Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_INQUIRY				0x12
#define SOPC_LOCK_UNLOCK_CACHE			0x36
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MEDIUM_SCAN			0x38
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_PREFETCH				0x34
#define SOPC_PREVENT_ALLOW_REMOVAL		0x1E
#define SOPC_READ_6				0x08
#define SOPC_READ_10				0x28
#define SOPC_READ_12				0xA8
#define SOPC_READ_BUFFER			0x3C
#define SOPC_READ_CAPACITY			0x25
#define SOPC_READ_LONG				0x3E
#define SOPC_REASSIGN_BLOCKS			0x07
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_RELEASE				0x17
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_RESERVE				0x16
#define SOPC_REZERO_UNIT			0x01

/*
 * SCSI Operation Codes for CD-ROM Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_INQUIRY				0x12
#define SOPC_LOCK_UNLOCK_CACHE			0x36
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_PAUSE_RESUME			0x4B
#define SOPC_PLAY_AUDIO_10			0x45
#define SOPC_PLAY_AUDIO_12			0xA5
#define SOPC_PLAY_AUDIO_MSF			0x47
#define SOPC_PLAY_AUDIO_TRACK_INDEX		0x48
#define SOPC_PLAY_TRACK_RELATIVE_10		0x49
#define SOPC_PLAY_TRACK_RELATIVE_12		0xA9
#define SOPC_PREFETCH				0x34
#define SOPC_PREVENT_ALLOW_REMOVAL		0x1E
#define SOPC_READ_6				0x08
#define SOPC_READ_10				0x28
#define SOPC_READ_12				0xA8
#define SOPC_READ_BUFFER			0x3C
#define SOPC_READ_FORMAT_CAPACITIES		0x23
#define SOPC_READ_CAPACITY			0x25
#define SOPC_READ_HEADER			0x44
#define SOPC_READ_LONG				0x3E
#define SOPC_READ_SUBCHANNEL			0x42
#define SOPC_READ_TOC				0x43
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_RELEASE				0x17
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_RESERVE				0x16
#define SOPC_REZERO_UNIT			0x01
#define SOPC_SEARCH_DATA_EQUAL_10		0x31
#define SOPC_SEARCH_DATA_EQUAL_12		0xB1
#define SOPC_SEARCH_DATA_HIGH_10		0x30
#define SOPC_SEARCH_DATA_HIGH_12		0xB0
#define SOPC_SEARCH_DATA_LOW_10			0x32
#define SOPC_SEARCH_DATA_LOW_12			0xB2
#define SOPC_SEEK_6				0x0B
#define SOPC_SEEK_10				0x2B
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_SET_LIMITS_10			0x33
#define SOPC_SET_LIMITS_12			0xB3
#define SOPC_START_STOP_UNIT			0x1B
#define SOPC_SYNCHRONIZE_CACHE			0x35
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_VERIFY_10				0x2F
#define SOPC_VERIFY_12				0xAF
#define SOPC_WRITE_BUFFER			0x3B

/*
 * SCSI Operation Codes for Scanner Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_GET_DATA_BUFFER_STATUS		0x34
#define SOPC_GET_WINDOW				0x25
#define SOPC_INQUIRY				0x12
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_OBJECT_POSITION			0x31
#define SOPC_READ_SCANNER			0x28
#define SOPC_READ_BUFFER			0x3C
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_RELEASE_UNIT			0x17
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_RESERVE_UNIT			0x16
#define SOPC_SCAN				0x1B
#define SOPC_SET_WINDOW				0x24
#define SOPC_SEND_SCANNER			0x2A
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_WRITE_BUFFER			0x3B

/*
 * SCSI Operation Codes for Optical Memory Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_COMPARE				0x39
#define SOPC_COPY				0x18
#define SOPC_COPY_VERIFY			0x3A
#define SOPC_ERASE_10				0x2C
#define SOPC_ERASE_12				0xAC
#define SOPC_FORMAT_UNIT			0x04
#define SOPC_INQUIRY				0x12
#define SOPC_LOCK_UNLOCK_CACHE			0x36
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MEDIUM_SCAN			0x38
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_PREFETCH				0x34
#define SOPC_PREVENT_ALLOW_REMOVAL		0x1E
#define SOPC_READ_6				0x08
#define SOPC_READ_10				0x28
#define SOPC_READ_12				0xA8
#define SOPC_READ_BUFFER			0x3C
#define SOPC_READ_CAPACITY			0x25
#define SOPC_READ_DEFECT_DATA_10		0x37
#define SOPC_READ_DEFECT_DATA_12		0xB7
#define SOPC_READ_GENERATION			0x29
#define SOPC_READ_LONG				0x3E
#define SOPC_READ_UPDATED_BLOCK			0x2D
#define SOPC_REASSIGN_BLOCKS			0x07
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_RELEASE				0x17
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_RESERVE				0x16
#define SOPC_REZERO_UNIT			0x01
#define SOPC_SEARCH_DATA_EQUAL_10		0x31
#define SOPC_SEARCH_DATA_EQUAL_12		0xB1
#define SOPC_SEARCH_DATA_HIGH_10		0x30
#define SOPC_SEARCH_DATA_HIGH_12		0xB0
#define SOPC_SEARCH_DATA_LOW_10			0x32
#define SOPC_SEARCH_DATA_LOW_12			0xB2
#define SOPC_SEEK_6				0x0B
#define SOPC_SEEK_10				0x2B
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_SET_LIMITS_10			0x33
#define SOPC_SET_LIMITS_12			0xB3
#define SOPC_START_STOP_UNIT			0x1B
#define SOPC_SYNCHRONIZE_CACHE			0x35
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_UPDATE_BLOCK			0x3D
#define SOPC_VERIFY_10				0x2F
#define SOPC_VERIFY_12				0xAF
#define SOPC_WRITE_6				0x0A
#define SOPC_WRITE_10				0x2A
#define SOPC_WRITE_12				0xAA
#define SOPC_WRITE_VERIFY_10			0x2E
#define SOPC_WRITE_VERIFY_12			0xAE
#define SOPC_WRITE_BUFFER			0x3B
#define SOPC_WRITE_LONG				0x3F

/*
 * SCSI Operation Codes for Medium-Changer Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_EXCHANGE_MEDIUM			0xA6
#define SOPC_INITIALIZE_ELEMENT_STATUS		0x07
#define SOPC_INQUIRY				0x12
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_MOVE_MEDIUM			0xA5
#define SOPC_POSITION_TO_ELEMENT		0x2B
#define SOPC_PREVENT_ALLOW_REMOVAL		0x1E
#define SOPC_READ_BUFFER			0x3C
#define SOPC_READ_ELEMENT_STATUS		0xB8
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_RELEASE				0x17
#define SOPC_REQUEST_VOLUME_ELEMENT_ADDRESS	0xB5
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_RESERVE				0x16
#define SOPC_REZERO_UNIT			0x01
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_SEND_VOLUME_TAG			0xB6
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_WRITE_BUFFER			0x3B

/*
 * SCSI Operation Codes for Communication Devices.
 */
#define SOPC_CHANGE_DEFINITION			0x40
#define SOPC_GET_MESSAGE_6			0x08
#define SOPC_GET_MESSAGE_10			0x28
#define SOPC_GET_MESSAGE_12			0xA8
#define SOPC_INQUIRY				0x12
#define SOPC_LOG_SELECT				0x4C
#define SOPC_LOG_SENSE				0x4D
#define SOPC_MODE_SELECT_6			0x15
#define SOPC_MODE_SELECT_10			0x55
#define SOPC_MODE_SENSE_6			0x1A
#define SOPC_MODE_SENSE_10			0x5A
#define SOPC_READ_BUFFER			0x3C
#define SOPC_RECEIVE_DIAGNOSTIC			0x1C
#define SOPC_REQUEST_SENSE			0x03
#define SOPC_SEND_DIAGNOSTIC			0x1D
#define SOPC_SEND_MESSAGE_6			0x0A
#define SOPC_SEND_MESSAGE_10			0x2A
#define SOPC_SEND_MESSAGE_12			0xAA
#define SOPC_TEST_UNIT_READY			0x00
#define SOPC_WRITE_BUFFER			0x3B

/*
 * External Declarations:
 */ 
extern int inquiry_encode(void *arg);
extern int inquiry_decode(void *arg);
extern scsi_opcode_t *ScsiOpcodeEntry(unsigned char *cdb, unsigned short device_type);

#endif /* !defined(SCSI_OPCODES_H) */
