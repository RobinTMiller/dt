#if !defined(SCSI_CDBS_INCLUDE)
#define SCSI_CDBS_INCLUDE 1

/****************************************************************************
 *      								    *
 *      		  COPYRIGHT (c) 1991 - 2021     		    *
 *      		   This Software Provided       		    *
 *      			     By 				    *
 *      		  Robin's Nest Software Inc.    		    *
 *      								    *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,     *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permission notice appear in the      *
 * supporting documentation, and that the name of the author not be used    *
 * in advertising or publicity pertaining to distribution of the software   *
 * without specific, written prior permission.  			    *
 *      								    *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,        *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN      *
 * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
 * THIS SOFTWARE.       						    *
 *      								    *
 ****************************************************************************/
/************************************************************************
 *									*
 * File:	scsi_cdbs.h						*
 * Date:	April 9, 1991						*
 * Author:	Robin T. Miller						*
 *									*
 * Description:								*
 *	SCSI Command Block Descriptor definitions.			*
 *									*
 ************************************************************************/
 /*
  * Modification History:
  *
  * October 6th, 2015 by Robin T. Miller
  * 	Adding CDB's from libscsi.c to here.
  * 	Updating bitfields for native AIX compiler.
  * 	AIX does NOT like uint8_t or unnamed bit fields!
  * 
  * July 31st, 2015 by Robin T. Miller
  * 	Modify/add new SCSI CDB's (16-byte, etc)
  */

#if defined(__IBMC__)
/* IBM aligns bit fields to 32-bits by default! */
#  pragma options align=bit_packed
#endif /* defined(__IBMC__) */

/*
 * MACRO for converting a longword to a byte
 *	args - the longword
 *	     - which byte in the longword you want
 */
//#define	LTOB(a,b)	(uint8_t)((a>>(b*8))&0xff)

/************************************************************************
 *									*
 *			  Generic SCSI Commands				*
 *									*
 ************************************************************************/

/* 
 * Test Unit Ready Command Descriptor Block:
 */
struct TestUnitReady_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Inquiry Command Descriptor Block:
 */
struct Inquiry_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    evpd		: 1,	/* Enable Vital Product Data.	[1] */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    evpd		: 1;	/* Enable Vital Product Data.	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	pgcode;			/* EVPD Page Code.		[2] */
	uint8_t	reserved_byte3;		/* Reserved.			[3] */
	uint8_t	alclen;			/* Allocation Length.		[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Mode Sense Command Descriptor Block:
 */
struct ModeSense_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_3	: 3,	/* Reserved.			[1] */
	    dbd			: 1,	/* Disable Block Descriptors.	    */
	    res_byte1_b4_1	: 1,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b4_1	: 1,	/* Reserved.			    */
	    dbd			: 1,	/* Disable Block Descriptors.	    */
	    res_byte1_b0_3	: 3;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    	pgcode	: 6,		/* Page code.			[2] */
		pcf	: 2;		/* Page Control Field.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		pcf	: 2,		/* Page Control Field.		    */
		pgcode	: 6;		/* Page code.			[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	alclen;			/* Allocation Length.		[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Mode Select Command Descriptor Block:
 */
struct ModeSelect_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    sp			: 1,	/* Save Parameters.		[1] */
	    res_byte1_b1_3	: 3,	/* Reserved.			    */
	    pf			: 1,	/* Page Format.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    pf			: 1,	/* Page Format.			    */
	    res_byte1_b1_3	: 3,	/* Reserved.			    */
	    sp			: 1;	/* Save Parameters.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	pll;			/* Parameter List Length.	[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Send Diagnostic Command Descriptor Block:
 */
struct SendDiagnostic_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    control	: 5,		/* Diagnostic control bits.	[1] */
	    lun		: 3;		/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun		: 3,		/* Logical Unit Number.		    */
	    control	: 5;		/* Diagnostic control bits.	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	param_len1;		/* Allocation length (MSB).	[3] */
	uint8_t	param_len0;		/* Allocation length (LSB).	[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Receive Diagnostic Result Command Descriptor Block:
 */
struct ReceiveDiagnostic_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	alloc_len1;		/* Allocation length (MSB).	[3] */
	uint8_t	alloc_len0;		/* Allocation length (LSB).	[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Sense Key Codes:
 */
#define SKV_NOSENSE		0x0	/* No error or no sense info.	*/
#define SKV_RECOVERED		0x1	/* Recovered error (success).	*/
#define SKV_NOT_READY		0x2	/* Unit is not ready.		*/
#define SKV_MEDIUM_ERROR	0x3	/* Nonrecoverable error.	*/
#define SKV_HARDWARE_ERROR	0x4	/* Nonrecoverable hardware err.	*/
#define SKV_ILLEGAL_REQUEST	0x5	/* Illegal CDB parameter.	*/
#define SKV_UNIT_ATTENTION	0x6	/* Target has been reset.	*/
#define SKV_DATA_PROTECT	0x7	/* Unit is write protected.	*/
#define SKV_BLANK_CHECK		0x8	/* A no-data condition occured.	*/
#define SKV_COPY_ABORTED	0xA	/* Copy command aborted.	*/
#define SKV_ABORTED_CMD		0xB	/* Target aborted cmd, retry.	*/
#define SKV_EQUAL		0xC	/* Vendor unique, not used.	*/
#define SKV_VOLUME_OVERFLOW	0xD	/* Physical end of media detect	*/
#define SKV_MISCOMPARE		0xE	/* Source & medium data differ.	*/
#define SKV_RESERVED		0xF	/* This sense key is reserved.	*/

/* Note: Use definitions in libscsi.h now! */
#if 0
/*
 * Request Sense Data Format.
 */
struct RequestSenseData {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    error_code		: 4,	/* Error code.			[0] */
	    error_class		: 3,	/* Error class.			    */
	    valid		: 1;	/* Information fields valid.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    valid		: 1,	/* Information fields valid.	    */
	    error_class		: 3,	/* Error class.			    */
	    error_code		: 4;	/* Error code.			[0] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	segment_number;		/* Segment number.		[1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    sense_key		: 4,	/* Sense key.			[2] */
	    res_byte2_b4	: 1,	/* Reserved.			    */
	    illegal_length	: 1,	/* Illegal length indicator.	    */
	    end_of_medium	: 1,	/* End of medium.		    */
	    file_mark		: 1;	/* Tape filemark detected.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    file_mark		: 1,	/* Tape filemark detected.	    */
	    end_of_medium	: 1,	/* End of medium.		    */
	    illegal_length	: 1,	/* Illegal length indicator.	    */
	    res_byte2_b4	: 1,	/* Reserved.			    */
	    sense_key		: 4;	/* Sense key.			[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	info_byte3;		/* Information byte (MSB).	[3] */
	uint8_t	info_byte2;		/*      "       "		[4] */
	uint8_t	info_byte1;		/*      "       "		[5] */
	uint8_t	info_byte0;		/* Information byte (LSB).	[6] */
	uint8_t	addl_sense_length;	/* Additional sense length.	[7] */
	uint8_t	cmd_spec_info3;		/* Command specific info (MSB).	[8] */
	uint8_t	cmd_spec_info2;		/*    "       "      "		[9] */
	uint8_t	cmd_spec_info1;		/*    "       "      "		[10]*/
	uint8_t	cmd_spec_info0;		/* Command specific info (LSB).	[11]*/
	uint8_t	addl_sense_code;	/* Additional sense code.	[12]*/
	uint8_t	addl_sense_qual;	/* Additional sense qualifier.	[13]*/
	uint8_t	fru_code;		/* Field replaceable unit.	[14]*/
	uint8_t	addl_sense_bytes[10];	/* Additional sense bytes.	[15]*/
};

/*
 * Additional Sense Bytes Format for "ILLEGAL REQUEST" Sense Key:
 */
struct sense_field_pointer {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	uint8_t	bit_ptr		: 3,	/* Bit pointer.			[15]*/
		bpv		: 1,	/* Bit pointer valid.		    */
				: 2,	/* Reserved.			    */
		cmd_data	: 1,	/* Command/data (1=CDB, 0=Data)	    */
		sksv		: 1;	/* Sense key specific valid.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	uint8_t	sksv		: 1,	/* Sense key specific valid.	    */
		cmd_data	: 1,	/* Command/data (1=CDB, 0=Data)	    */
				: 2,	/* Reserved.			    */
		bpv		: 1,	/* Bit pointer valid.		    */
		bit_ptr		: 3;	/* Bit pointer.			[15]*/
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	field_ptr1;		/* Field pointer (MSB byte).	[16]*/
	uint8_t	field_ptr0;		/* Field pointer (LSB byte).	[17]*/
};

#endif /* 0 */

/*
 * Additional Sense Bytes Format for "RECOVERED ERROR", "HARDWARE ERROR",
 * or "MEDIUM ERROR" Sense Keys:
 */
struct sense_retry_count {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte15_b0_7	: 7,	/* Reserved.			[15]*/
	    sksv		: 1;	/* Sense key specific valid.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    sksv		: 1,	/* Sense key specific valid.	    */
	    res_byte15_b0_7	: 7;	/* Reserved.			[15]*/
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	retry_count1;		/* Retry count (MSB byte).	[16]*/
	uint8_t	retry_count0;		/* Retry count (LSB byte).	[17]*/
};

/*
 * Additional Sense Bytes Format for "NOT READY" Sense Key:
 */
struct sense_format_progress {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte15_b0_7	: 7,	/* Reserved.			[15]*/
	    sksv		: 1;	/* Sense key specific valid.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    sksv		: 1,	/* Sense key specific valid.	    */
	    res_byte15_b0_7	: 7;	/* Reserved.			[15]*/
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	progress_ind1;		/* Progress indicator (MSB byte)[16]*/
	uint8_t	progress_ind0;		/* Progress indicator (LSB byte)[17]*/
};

/************************************************************************
 *									*
 *			     Direct I/O Commands			*
 *									*
 ************************************************************************/

/*
 * Format Unit Command Descriptor Block:
 */
struct FormatUnit_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    dlf		: 3,		/* Defect List Format.		[1] */
	    cmplst	: 1,		/* Complete List.		    */
	    fmtdat	: 1,		/* Format Data.			    */
	    lun		: 3;		/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun		: 3,		/* Logical Unit Number.		    */
	    fmtdat	: 1,		/* Format Data.			    */
	    cmplst	: 1,		/* Complete List.		    */
	    dlf		: 3;		/* Defect List Format.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	pattern;		/* Format Data Pattern.		[2] */
	uint8_t	interleave1;		/* Interleave Factor.		[3] */
	uint8_t	interleave0;		/* Interleave Factor.		[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Prevent/Allow Medium Removal Command Descriptor Block:
 */
struct PreventAllow_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    prevent		: 1,	/* Prevent = 1, Allow = 0.	[4] */
	    res_byte4_b1_7	: 7;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte4_b1_7	: 7,	/* Reserved.			    */
	    prevent		: 1;	/* Prevent = 1, Allow = 0.	[4] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Read Capacity(10) Command Descriptor Block:
 */
typedef struct ReadCapacity10_CDB {
    uint8_t opcode;
    uint8_t reserved_byte1;
    uint8_t lba[4];
    uint8_t reserved_byte7;
    uint8_t reserved1_byte8;
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
	pmi		: 1,		/* Partial medium indicator             */
        res_byte9_b1_7	: 7;		/* 7 bits reserved                      */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
	res_byte9_b1_7	: 7,		/* 7 bits reserved                      */
	pmi		: 1;		/* Partial medium indicator             */
#else
#       error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t control;
} ReadCapacity10_CDB_t;

typedef struct ReadCapacity10_data {
    uint8_t last_block[4];
    uint8_t block_length[4];
} ReadCapacity10_data_t;

typedef struct ReadCapacity16_CDB {
    uint8_t opcode;
    uint8_t service_action;
    uint8_t lba[8];
    uint8_t allocation_length[4];
    uint8_t flags;
    uint8_t control;
} ReadCapacity16_CDB_t;

/*
 * Read Capacity(16) Command Descriptor Block:
 */
typedef struct ReadCapacity16_data {
    uint8_t last_block[8];
    uint8_t block_length[4];
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
	prot_en		: 1,		/* Protection enabled.			[12][b0]   */
	p_type		: 3,		/* Protection type			    (b1:3) */
	reserved_b4_7	: 4;		/* Reserved.				    (b4:7) */
    bitfield_t
	lbppbe		: 4,		/* Logical blocks per physical exponent.[13](b0:4) */
	p_i_exponent	: 4;		/* Protection information exponent.	    (b4:4) */
    bitfield_t
	lowest_aligned_msb : 6,	/* Lowest aligned logical block (MSB).	[14](b0:6) */
	lbprz		: 1,		/* Thin provisioning read zeroes.	     (b6)  */
	lbpme		: 1;		/* Thin provisioning enabled (1 = True).     (b7)  */
    uint8_t lowest_aligned_lsb;		/* Lowest aligned logical block (LSB).	[15]       */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
	reserved_b4_7	: 4,		/* Reserved.				    (b4:7) */
	p_type		: 3,		/* Protection type			    (b1:3) */
	prot_en		: 1;		/* Protection enabled.			[12][b0]   */
    bitfield_t
	p_i_exponent	: 4,		/* Protection information exponent.	    (b4:4) */
	lbppbe		: 4;		/* Logical blocks per physical exponent.[13](b0:4) */
    bitfield_t
	lbpme		: 1,		/* Thin provisioning enabled (1 = True).     (b7)  */
	lbprz		: 1,		/* Thin provisioning read zeroes.	     (b6)  */
	lowest_aligned_msb : 6;		/* Lowest aligned logical block (MSB)	[14](b0:6) */
    uint8_t lowest_aligned_lsb;		/* Lowest aligned logical block (LSB).	[15]       */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved_byte16_31[16];	/* Reserved bytes.			[16-31]    */
} ReadCapacity16_data_t;

/*
 * Reassign Blocks Command Descriptor Block:
 */
struct ReassignBlocks_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Disk Read / Write / Seek CDB's.
 */
typedef struct DirectRW6_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
	uint8_t	lba[3];			/* Logical Block Address.     [1-3] */
	uint8_t	length;			/* Transfer Length.		[4] */
	uint8_t	control;		/* Various control flags.	[5] */
} DirectRW6_CDB_t;

#define SCSI_DIR_RDWR_10_DPO		0x10
#define SCSI_DIR_RDWR_10_FUA            0x08
#define SCSI_DIR_RDWR_10_RELADR         0x01

typedef struct DirectRW10_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
	uint8_t	flags;			/* Various flags.		[1] */
	uint8_t	lba[4];			/* Logical Block Address.     [2-5] */
	uint8_t	reserved_byte6;		/* Reserved.			[6] */
	uint8_t	length[2];		/* Transfer Length.    	      [7-8] */
	uint8_t	control;		/* Various control flags.	[9] */
} DirectRW10_CDB_t;

#define SCSI_DIR_RDWR_16_DPO            0x10
#define SCSI_DIR_RDWR_16_FUA            0x08
#define SCSI_DIR_RDWR_16_RELADR         0x01

typedef struct DirectRW16_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
	uint8_t	flags;			/* Various flags.		[1] */
	uint8_t	lba[8];			/* Logical block address.     [2-9] */
	uint8_t	length[4];		/* Transfer Length.    	    [10-13] */
	uint8_t	reserved_byte14;	/* Reserved.		       [14] */
	uint8_t	control;		/* Various control flags.      [15] */
} DirectRW16_CDB_t;

/*
 * Read Defect Data Command Descriptor Block:
 */
struct ReadDefectData_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    dlf			: 3,	/* Defect List Format.		[2] */
	    grown		: 1,	/* Grown Defect List.		    */
	    manuf		: 1,	/* Manufacturers Defect List.	    */
	    res_byte2_5_3	: 3;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte2_5_3	: 3,	/* Reserved.			    */
	    manuf		: 1,	/* Manufacturers Defect List.	    */
	    grown		: 1,	/* Grown Defect List.		    */
	    dlf			: 3;	/* Defect List Format.		[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	alclen1;		/* Allocation Length.		[7] */
	uint8_t	alclen0;		/* Allocation Length.		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Seek(10) LBA Command Descriptor Block:
 */
typedef struct Seek10_CDB {
        uint8_t opcode;		/* Operation Code.			[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
        bitfield_t
	    res_byte1_b0_5	: 5, /* 5 bits reserved                 [1] */
	    lun  	   	: 3; /* Logical unit number                 */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
        bitfield_t
	    lun			: 3, /* Logical unit number             [1] */
	    res_byte1_b0_5	: 5; /* 5 bits reserved                     */
#else
#       error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lba[4];		/* The logical block address.	      [2-5] */
	uint8_t	reserved[3];	/* Reserved.			      [6-8] */
        uint8_t control;        /* The control byte                     [9] */
} Seek10_CDB_t;

/*
 * Start/Stop Unit Command Descriptor Block:
 */
struct StartStopUnit_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    immed		: 1,	/* Immediate.			[1] */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    immed		: 1;	/* Immediate.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    start		: 1,	/* Start = 1, Stop = 0.		[4] */
	    loej		: 1,	/* Load/Eject = 1, 0 = No Affect.   */
	    res_byte4_b2_6    	: 6;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    ires_byte4_b2_6	: 6,	/* Reserved.			    */
	    loej		: 1,	/* Load/Eject = 1, 0 = No Affect.   */
	    start		: 1;	/* Start = 1, Stop = 0.		[4] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/* 
 * Verify Data Command Descriptor Block:
 */
struct VerifyDirect_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    reladr 		: 1,	/* Relative Address.		[1] */
	    bytchk		: 1,	/* Byte Check.			    */
	    res_byte1_b2_3    	: 3,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    bytchk		: 1,	/* Byte Check.			    */
	    reladr 		: 1;	/* Relative Address.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lbaddr3;		/* Logical Block Address 	[2] */
	uint8_t	lbaddr2;		/*    "      "      "		[3] */
	uint8_t	lbaddr1;		/*    "      "      "		[4] */
	uint8_t	lbaddr0;		/*    "      "      "		[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	verflen1;		/* Verification Length.		[7] */
	uint8_t	verflen0;		/* Verification Length.		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/************************************************************************
 *									*
 *			 Sequential I/O Commands			*
 *									*
 ************************************************************************/

/*
 * Erase Tape Command Descriptor Block:
 */
struct EraseTape_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    longe		: 1,	/* Long Erase (1 = Entire Tape)	[1] */
	    res_byte1_b1_4 	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    longe		: 1;	/* Long Erase (1 = Entire Tape)	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Load / Unload / Retention Tape Command Descriptor Block:
 */
struct LoadUnload_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    immed		: 1,	/* Immediate.			[1] */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    immed		: 1;	/* Immediate.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    load		: 1,	/* Load.			[4] */
	    reten		: 1,	/* Retention.			    */
	    eot			: 1,	/* End Of Tape.			    */
	    res_byte4_5		: 5;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte4_5		: 5,	/* Reserved.			    */
	    eot			: 1,	/* End Of Tape.			    */
	    reten		: 1,	/* Retention.			    */
	    load		: 1;	/* Load.			[4] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Rewind Tape Command Descriptor Block:
 */
struct RewindTape_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    immed		: 1,	/* Immediate.			[1] */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    immed		: 1;	/* Immediate.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Space Operation Codes.
 */
#define SPACE_BLOCKS		0	/* Space blocks (or records).	*/
#define SPACE_FILE_MARKS	1	/* Space file marks.		*/
#define SPACE_SEQ_FILE_MARKS	2	/* Space sequential file marks.	*/
#define SPACE_END_OF_DATA	3	/* Space to end of media.	*/
#define SPACE_SETMARKS		4	/* Space setmarks.		*/
#define SPACE_SEQ_SET_MARKS	5	/* Space sequential setmarks.	*/

/*
 * Space Tape Command Descriptor Block:
 */
struct SpaceTape_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    code		: 3,	/* Space Blocks/Filemarks/EOM.	[1] */
	    res_byte1_b3_2    	: 2,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b3_2	: 2,	/* Reserved.			    */
	    code		: 3;	/* Space Blocks/Filemarks/EOM.	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	count2;			/* Count (MSB).			[2] */
	uint8_t	count1;			/* Count.			[3] */
	uint8_t	count0;			/* Count (LSB).			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 1,	/* Vendor Unique.		    */
	    fast		: 1;	/* Fast Space Algorithm.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    fast		: 1,	/* Fast Space Algorithm.	    */
	    vendor		: 1,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Write Filemarks Command Descriptor Block:
 */
struct WriteFileMark_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	fmcount2;		/* Number of Filemarks (MSB).	[2] */
	uint8_t	fmcount1;		/* Number of Filemarks.		[3] */
	uint8_t	fmcount0;		/* Number of Filemarks (LSB).	[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 1,	/* Vendor Unique.		    */
	    fast		: 1;	/* Fast Space Algorithm.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    fast		: 1,	/* Fast Space Algorithm.	    */
	    vendor		: 1,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/************************************************************************
 *									*
 *			  CD-ROM Audio Commands				*
 *									*
 ************************************************************************/

/*
 * CD-ROM Pause/Resume Command Descriptor Block:
 */
struct CdPauseResume_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	res_byte7;		/* Reserved.			[7] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    resume		: 1,	/* Resume = 1, Pause = 0.	[8] */
	    res_byte8_b1_7	: 7;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte8_b1_7	: 7,	/* Reserved.			    */
	    resume		: 1;	/* Resume = 1, Pause = 0.	[8] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Audio LBA Command Descriptor Block:
 */
struct CdPlayAudioLBA_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    reladr		: 1,	/* Relative Address bit		[1] */
	    res_byte1_b1_4	: 4,	/* Reserved			    */
	    lun			: 3;	/* Logical Unit Number		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number		    */
	    res_byte1_b1_4	: 4,	/* Reserved			    */
	    reladr		: 1;	/* Relative Address bit		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lbaddr3;		/* Logical Block Address	[2] */
	uint8_t	lbaddr2;		/* Logical Block Address	[3] */
	uint8_t	lbaddr1;		/* Logical Block Address	[4] */
	uint8_t	lbaddr0;		/* Logical Block Address	[5] */
	uint8_t	res_byte6;		/* Reserved			[6] */
	uint8_t	xferlen1;		/* Transfer Length    		[7] */
	uint8_t	xferlen0;		/* Transfer Length    		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Audio MSF Command Descriptor Block:
 */
struct CdPlayAudioMSF_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	starting_M_unit;	/* Starting M-unit.		[3] */
	uint8_t	starting_S_unit;	/* Starting S-unit.		[4] */
	uint8_t	starting_F_unit;	/* Starting F-unit.		[5] */
	uint8_t	ending_M_unit;		/* Ending M-unit.		[6] */
	uint8_t	ending_S_unit;		/* Ending S-unit.		[7] */
	uint8_t	ending_F_unit;		/* Ending F-unit.		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Audio Track/Index Command Descriptor Block:
 */
struct CdPlayAudioTI_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	starting_track;		/* Starting Track.		[4] */
	uint8_t	starting_index;		/* Starting Index.		[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	ending_track;		/* Ending Track.		[7] */
	uint8_t	ending_index;		/* Ending Index			[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Audio Track Relative Command Descriptor Block:
 */
struct CdPlayAudioTR_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lbaddr3;		/* Logical Block Address	[2] */
	uint8_t	lbaddr2;		/* Logical Block Address.	[3] */
	uint8_t	lbaddr1;		/* Logical Block Address.	[4] */
	uint8_t	lbaddr0;		/* Logical Block Address.	[5] */
	uint8_t	starting_track;		/* Starting Track.		[6] */
	uint8_t	xfer_len1;		/* Transfer Length    		[7] */
	uint8_t	xfer_len0;		/* Transfer Length    		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Read TOC Command Descriptor Block:
 */
struct CdReadTOC_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0	: 1,	/* Reserved.			[1] */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b0	: 1;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	starting_track;		/* Reserved.			[6] */
	uint8_t	alloc_len1;		/* Allocation length (MSB).	[7] */
	uint8_t	alloc_len0;		/* Allocation length (LSB).	[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Read Sub-Channel Command Descriptor Block:
 */
struct CdReadSubChannel_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0	: 1,	/* Reserved.			[1] */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b2_4	: 3,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b2_4	: 3,	/* Reserved.			    */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b0	: 1;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte2_b0_5	: 6,	/* Reserved.			[2] */
	    subQ		: 1,	/* Sub-Q Channel Data.		    */
	    res_byte2_b7	: 1;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte2_b7	: 1,	/* Reserved.			    */
	    subQ		: 1,	/* Sub-Q Channel Data.		    */
	    res_byte2_b0_5	: 6;	/* Reserved.			[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	data_format;		/* Data Format Code.		[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	track_number;		/* Reserved.			[6] */
	uint8_t	alloc_len1;		/* Allocation length (MSB).	[7] */
	uint8_t	alloc_len0;		/* Allocation length (LSB).	[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_6	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_6	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Read Header Command Descriptor Block:
 */
struct CdReadHeader_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0	: 1,	/* Reserved.			[1] */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b0	: 1;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lbaddr3;		/* Logical Block Address	[2] */
	uint8_t	lbaddr2;		/* Logical Block Address.	[3] */
	uint8_t	lbaddr1;		/* Logical Block Address.	[4] */
	uint8_t	lbaddr0;		/* Logical Block Address.	[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	alloc_len1;		/* Allocation Length MSB.	[7] */
	uint8_t	alloc_len0;		/* Allocation Length LSB.	[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Track Command Descriptor Block:
 */
struct CdPlayTrack_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	starting_track;		/* Starting track.		[4] */
	uint8_t	starting_index;		/* Starting index.		[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	res_byte7;		/* Reserved.			[7] */
	uint8_t	number_indexes;		/* Number of indexes.		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Playback Control/Status Command Descriptor Block:
 */
struct CdPlayback_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	alloc_len1;		/* Allocation length (MSB).	[7] */
	uint8_t	alloc_len0;		/* Allocation length (LSB).	[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Set Address Format Command Descriptor Block:
 */
struct CdSetAddressFormat_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	res_byte7;		/* Reserved.			[7] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    lbamsf		: 1,	/* Address Format 0/1 = LBA/MSF	[8] */
	    res_byte8_b1_7	: 7;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte8_b1_7	: 7,	/* Reserved.			    */
	    lbamsf		: 1;	/* Address Format 0/1 = LBA/MSF	[8] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

#if defined(__IBMC__)
#  pragma options align=reset
#endif /* defined(__IBMC__) */

#endif /* !defined(SCSI_CDBS_INCLUDE) */
