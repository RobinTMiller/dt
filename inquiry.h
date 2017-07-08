/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2017			    *
 *			   This Software Provided			    *
 *				     By					    *
 *			  Robin's Nest Software Inc.			    *
 *									    *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,	    *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permission notice appear in the	    *
 * supporting documentation, and that the name of the author not be used    *
 * in advertising or publicity pertaining to distribution of the software   *
 * without specific, written prior permission.				    *
 *									    *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 	    *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN	    *
 * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
 * THIS SOFTWARE.							    *
 *									    *
 ****************************************************************************/
#if !defined(INQUIRY_INCLUDE)
#define INQUIRY_INCLUDE 1

#if defined(__IBMC__)
/* IBM aligns bit fields to 32-bits by default! */
#  pragma options align=bit_packed
#endif /* defined(__IBMC__) */

#include "libscsi.h"

/* %Z%%M% %I% %E% 1990 by Robin Miller. */

/*
 * Defined Peripheral Quailifiers:
 */
#define PQUAL_CONNECTED		0x0	/* Device is connected.		*/
#define PQUAL_NOT_CONNECTED	0x1	/* Device is NOT connected.	*/
#define PQUAL_NO_PHYSICAL	0x3	/* No physical device support.	*/
#define PQUAL_VENDOR_SPECIFIC	0x4	/* Vendor specific peripheral.	*/

/*
 * Defined Device Types:
 */
#define	DTYPE_DIRECT		0x00	/* Direct access.		*/
#define	DTYPE_SEQUENTIAL	0x01	/* Sequential access.		*/
#define	DTYPE_PRINTER		0x02	/* Printer.			*/
#define	DTYPE_PROCESSOR		0x03	/* Processor.			*/
#define	DTYPE_WORM		0x04	/* Write-Once/Read Many.	*/
#define	DTYPE_RODIRECT		0x05	/* Read-Only direct access.	*/
#define DTYPE_MULTIMEDIA	0x05	/* SCSI-3 Multi-media device.	*/
#define	DTYPE_SCANNER		0x06	/* Scanner.			*/
#define	DTYPE_OPTICAL		0x07	/* Optical.			*/
#define	DTYPE_CHANGER		0x08	/* Changer.			*/
#define DTYPE_COMM		0x09	/* Communications device.	*/
#define DTYPE_PREPRESS_0	0x0A	/* Graphics pre-press device.	*/
#define DTYPE_PREPRESS_1	0x0B	/* Graphics pre-press device.	*/
#define DTYPE_RAID		0x0C	/* Array controller device.	*/
#define DTYPE_ENCLOSURE		0x0D	/* Storage enclosure services.	*/
#define DTYPE_UTILITY		0x0E	/* Utility device.		*/
					/* 0x0F-0x1E are reserved.	*/
#define	DTYPE_NOTPRESENT	0x1F	/* Unknown or no device type.	*/

/*
 * Device type bitmasks control access to commands and mode pages.
 */
#define ALL_DEVICE_TYPES	0xffffU	/* Supported for all devices.   */

/*
 * Random access devices support many of the commands and pages, so...
 */
#define ALL_RANDOM_DEVICES	(BITMASK(DTYPE_DIRECT) |		\
				 BITMASK(DTYPE_OPTICAL) |		\
				 BITMASK(DTYPE_RODIRECT) |		\
				 BITMASK(DTYPE_WORM))

/*
 * ANSI Approved Versions:
 *
 * NOTE: Many devices are implementing SCSI-3 extensions, but since
 *	 that spec isn't approved by ANSI yet, return SCSI-2 response.
 */
#define ANSI_LEVEL0		0x00	/* May or maynot comply to ANSI	*/
#define ANSI_SCSI1		0x01	/* Complies to ANSI X3.131-1986	*/
#define ANSI_SCSI2		0x02	/* Complies to ANSI X3.131-1994 */
#define ANSI_SCSI3		0x03	/* Complies to ANSI X3.351-1997 */
#define ANSI_SPC	ANSI_SCSI3	/* SCSI Primary Commands 1.	*/
#define ANSI_SPC2		0x04	/* SCSI Primary Commands 2.	*/
#define ANSI_SPC3		0x05	/* SCSI Primary Commands 3.	*/
#define ANSI_SPC4		0x06	/* SCSI Primary Commands 4.	*/

					/* ANSI codes > 4 are reserved.	*/
/*
 * Response Data Formats:
 */
#define	RDF_SCSI1		0x00	/* SCSI-1 inquiry data format.	*/
#define	RDF_CCS			0x01	/* CCS inquiry data format.	*/
#define	RDF_SCSI2		0x02	/* SCSI-2 inquiry data format.	*/
					/* RDF codes > 2 are reserved.	*/
/*
 * The first 36 bytes of inquiry is standard, vendor unique after that.
 */
#define STD_INQ_LEN		36	/* Length of standard inquiry.	*/
#define STD_ADDL_LEN		31	/* Standard additional length.	*/
#define MAX_INQ_LEN		255	/* Maxiumum Inquiry length.	*/

#define INQ_VID_LEN		8	/* The vendor ident length.	*/
#define INQ_PID_LEN		16	/* The Product ident length.	*/
#define INQ_REV_LEN		4	/* The revision level length.	*/

typedef struct inquiry_sflags {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		inqf_res_b0_b2	: 3,	/* Reserved.			(b0) */
		inqf_mchngr	: 1,	/* Medium changer support.	(b3) */
		inqf_multip	: 1,	/* Multiport device support.	(b4) */
		inqf_vendspec	: 1,	/* Vendor specific.		(b5) */
		inqf_encserv	: 1,	/* Enclosure services.		(b6) */
		inqf_res_b7	: 1;	/* Reserved.			(b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		inqf_res_b7	: 1,	/* Reserved.			(b7) */
		inqf_encserv	: 1,	/* Enclosure services.		(b6) */
		inqf_vendspec	: 1,	/* Vendor specific.		(b5) */
		inqf_multip	: 1,	/* Multiport device support.	(b4) */
		inqf_mchngr	: 1,	/* Medium changer support.	(b3) */
		inqf_res_b0_b2	: 3;	/* Reserved.			(b0) */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} inquiry_sflags_t;

typedef struct inquiry_flags {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		inqf_sftre	: 1,	/* Soft reset support.	 	(b0) */
		inqf_cmdque	: 1,	/* Command queuing support.	(b1) */
		inqf_transdis	: 1,	/* Target transfer disable.(S-3)(b2) */
		inqf_linked	: 1,	/* Linked command support.	(b3) */
		inqf_sync	: 1,	/* Synchronous data transfers.	(b4) */
		inqf_wbus16	: 1, 	/* Support for 16 bit transfers.(b5) */
		inqf_wbus32	: 1,	/* Support for 32 bit transfers.(b6) */
		inqf_reladdr	: 1;	/* Relative addressing support.	(b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		inqf_reladdr	: 1,	/* Relative addressing support.	(b7) */
		inqf_wbus32	: 1,	/* Support for 32 bit transfers.(b6) */
		inqf_wbus16	: 1, 	/* Support for 16 bit transfers.(b5) */
		inqf_sync	: 1,	/* Synchronous data transfers.	(b4) */
		inqf_linked	: 1,	/* Linked command support.	(b3) */
		inqf_transdis	: 1,	/* Target transfer disable.(S-3)(b2) */
		inqf_cmdque	: 1,	/* Command queuing support.	(b1) */
		inqf_sftre	: 1;	/* Soft reset support.	 	(b0) */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} inquiry_flags_t;

/*
 * SCSI Inquiry Data Structure:
 */
typedef struct {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		inq_dtype	: 5,	/* Peripheral device type.	[0] */
		inq_pqual	: 3;	/* Peripheral qualifier.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		inq_pqual	: 3,	/* Peripheral qualifier.	    */
		inq_dtype	: 5;	/* Peripheral device type.	[0] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		inq_dtypmod	: 7,	/* Device type modifier.	[1] */
		inq_rmb		: 1;	/* Removable media.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		inq_rmb		: 1,	/* Removable media.		    */
		inq_dtypmod	: 7;	/* Device type modifier.	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		inq_ansi	: 3,	/* ANSI version.		[2] */
		inq_ecma	: 3,	/* ECMA version.		    */
		inq_iso		: 2;	/* ISO version.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		inq_iso		: 2,	/* ISO version.			    */
		inq_ecma	: 3,	/* ECMA version.		    */
		inq_ansi	: 3;	/* ANSI version.		[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		inq_rdf		: 4,	/* Response data format.	[3] */
		inq_hisup	: 1,	/* Hierarchical support.            */
		inq_normaca	: 1,	/* Normal ACA supported.	    */
		inq_trmiop	: 1,	/* Terminate I/O process.	    */
		inq_aenc	: 1;	/* Async event notification.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		inq_aenc	: 1,	/* Async event notification.	    */
		inq_trmiop	: 1,	/* Terminate I/O process.	    */
		inq_normaca	: 1,	/* Normal ACA supported. (SCSI-3)   */
		inq_hisup	: 1,	/* Hierarchical support.            */
		inq_rdf		: 4;	/* Response data format.	[3] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	unsigned char inq_addlen;	/* Additional length.		[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		inq_protect	: 1,	/* Supports Protection Information. */
		inq_res_5_b1_b2	: 2,	/* Reserved bits (1:2).		    */
		inq_3pc		: 1,	/* 3rd Party Copy Support.	    */
		inq_tpgs	: 2,	/* Target Port Group Support.	    */
		inq_acc		: 1,	/* Access Controls Coordinator.	    */
		inq_sccs	: 1;	/* Storage Controller Components.   */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
        bitfield_t
		inq_sccs	: 1,	/* Storage Controller Components[5] */
	        inq_acc		: 1,	/* Access Controls Coordinator.	    */
		inq_tpgs	: 2,	/* Target Port Group Support.	    */
		inq_3pc		: 1,	/* 3rd Party Copy Support.	    */ 
		inq_res_5_b1_b2	: 2,	/* Reserved bits (1:2).		    */
		inq_protect	: 1;	/* Supports Protection Information. */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	union {
		unsigned char sflags;	/* SCSI-3 capability flags.	[6] */
		struct inquiry_sflags bits;
	} s3un;
#define inq_sflags	s3un.sflags
#define inq_reserved_6	inq_sflags	/* Ref: Reserved for non-SCSI-3.    */
#define inq_encserv	s3un.bits.inqf_encserv
#define inq_mchngr	s3un.bits.inqf_mchngr
#define inq_multip	s3un.bits.inqf_multip
#define inq_vendspec	s3un.bits.inqf_vendspec
	union {
		unsigned char flags;	/* Device capability flags.	[7] */
		struct inquiry_flags bits;
	} un;
#define inq_flags	un.flags
#define inq_sftre	un.bits.inqf_sftre
#define inq_cmdque	un.bits.inqf_cmdque
#define inq_transdis	un.bits.inqf_transdis
#define inq_linked	un.bits.inqf_linked
#define inq_sync	un.bits.inqf_sync
#define inq_wbus16	un.bits.inqf_wbus16
#define inq_wbus32	un.bits.inqf_wbus32
#define inq_reladdr	un.bits.inqf_reladdr
	unsigned char	inq_vid[INQ_VID_LEN];	/* Vendor ID.		     [8-15] */
	unsigned char	inq_pid[INQ_PID_LEN];	/* Product ID.		    [16-31] */
	unsigned char	inq_revlevel[INQ_REV_LEN];/* Revision level.	    [32-35] */
	unsigned char	inq_vendor_unique[MAX_INQ_LEN - STD_INQ_LEN];
} inquiry_t;

/*
 * Inquiry Flag Bits:
 */
#define INQ_EVPD	0x01		/* Enable vital product data.	*/
#define INQ_CMDDT	0x02		/* Command support data.	*/

/*
 * Inquiry Page Codes:
 */
#define INQ_ALL_PAGES		0x00	/* Supported vital product data	*/
#define INQ_SERIAL_PAGE		0x80	/* Unit serial number page.	*/
#define INQ_IMPOPR_PAGE		0x81	/* Implemented opr defs page.	*/
#define INQ_ASCOPR_PAGE		0x82	/* ASCII operating defs page.	*/
#define INQ_DEVICE_PAGE		0x83	/* Device Identification page.	*/

#define INQ_SOFT_INT_ID_PAGE	0x84	/* Software Interface Identification. */
#define INQ_MGMT_NET_ADDR_PAGE	0x85	/* Management Network Addresses.*/
#define INQ_EXTENDED_INQ_PAGE	0x86	/* Extended INQUIRY Data.	*/
#define INQ_MP_POLICY_PAGE	0x87	/* Mode Page Policy.		*/
#define INQ_SCSI_PORTS_PAGE	0x88	/* SCSI Ports.			*/
#define INQ_ATA_INFO_PAGE	0x89	/* ATA Information.		*/
#define INQ_BLOCK_LIMITS_PAGE	0xB0	/* Block limits.		*/

#define INQ_ASCIIINFO_START	0x01	/* ASCII Info starting page.	*/
#define INQ_ASCIIINFO_END	0x07	/* ASCII Info ending page value	*/
#define INQ_RESERVED_START	0x84	/* Reserved starting page value	*/
#define INQ_RESERVED_END	0xBF	/* Reserved ending page value.	*/
#define INQ_VENDOR_START	0xC0	/* Vendor-specific start value.	*/
#define INQ_VENDOR_END		0xFF	/* Vendor-specific ending value	*/
#define MAX_INQUIRY_PAGE	0xFF	/* Maximum inquiry page code.	*/

typedef struct inquiry_header {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		inq_dtype	: 5,	/* Peripheral device type.	[0] */
		inq_pqual	: 3;	/* Peripheral qualifier.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		inq_pqual	: 3,	/* Peripheral qualifier.	    */
		inq_dtype	: 5;	/* Peripheral device type.	[0] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	unsigned char inq_page_code;	/* The inquiry page code.	[1] */
	unsigned char inq_reserved;	/* Reserved.			[2] */
	unsigned char inq_page_length;	/* The page code length.	[3] */
					/* Variable length data follows.    */
} inquiry_header_t;

#define MAX_INQ_PAGE_LENGTH	(MAX_INQ_LEN - sizeof(inquiry_header_t))

typedef struct inquiry_page {
	inquiry_header_t inquiry_hdr;
	unsigned char inquiry_page_data[MAX_INQ_PAGE_LENGTH];
} inquiry_page_t;

/*
 * Operating Definition Parameter Values:
 */
#define OPDEF_CURRENT	0x00		/* Use current operating def.	*/
#define OPDEF_SCSI1	0x01		/* SCSI-1 operating definition.	*/
#define OPDEF_CCS	0x02		/* CCS operating definition.	*/
#define OPDEF_SCSI2	0x03		/* SCSI-2 operating definition.	*/
#define OPDEF_SCSI3	0x04		/* SCSI-3 operating definition.	*/
#define OPDEF_MAX	0x05		/* Number of operating defs.	*/

struct opdef_param {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		opdef	: 7,		/* Operating definition.	*/
		savimp	: 1;		/* Operating def can be saved.	*/
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		savimp	: 1,		/* Operating def can be saved.	*/
		opdef	: 7;		/* Operating definition.	*/
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Implemented Operating Definition Page.
 */
typedef struct inquiry_opdef_page {
	inquiry_header_t inquiry_header;
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		current_opdef	: 7,	/* Current operating definition [0] */
		opdef_res_b7	: 1;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		opdef_res_b7	: 1,	/* Reserved.			    */
		current_opdef	: 7;	/* Current operating definition [0] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		default_opdef	: 7,	/* Default operating definition	*/
		default_savimp	: 1;	/* Operating def can be saved.	*/
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		default_savimp	: 1,	/* Operating def can be saved.	*/
		default_opdef	: 7;	/* Default operating definition	*/
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	unsigned char	support_list[10];	/* Supported definition list.	*/
} inquiry_opdef_page_t;

/*
 * Device Indentification Page Definitions:
 */
#define IID_CODE_SET_RESERVED	0x00	/* Reserved.			*/
#define IID_CODE_SET_BINARY	0x01	/* Identifier field is binary.	*/
#define IID_CODE_SET_ASCII	0x02	/* Identifier field is ASCII.	*/
#define IID_CODE_SET_ISO_IEC    0x03    /* Contains ISO/IEC identifier. */
					/* 0x04-0x0F are reserved.	*/

#define IID_ID_TYPE_VS		0x00	/* ID type is vendor specific.	*/
#define IID_ID_TYPE_T10_VID	0x01	/* T10 vendor ID based. 	*/
#define IID_ID_TYPE_EUI64	0x02	/* EUI-64 based identifier.	*/
#define IID_ID_TYPE_NAA 	0x03	/* Name Address Authority (NAA) */
#define IID_ID_TYPE_RELTGTPORT  0x04    /* Relative target port ident.  */
#define IID_ID_TYPE_TGTPORTGRP  0x05    /* Target port group identifier */
#define IID_ID_TYPE_LOGUNITGRP  0x06    /* Logical unit group ident.    */
#define IID_ID_TYPE_MD5LOGUNIT  0x07    /* MD5 logical unit identifier. */
#define IID_ID_TYPE_SCSI_NAME   0x08    /* SCSI name string identifier. */
					/* 0x09-0x0F are reserved.	*/

/*
 * Association Definitions:
 */
#define IID_ASSOC_LOGICAL_UNIT  0x00    /* Associated w/logical unit.   */
#define IID_ASSOC_TARGET_PORT   0x01    /* Associated w/target port.    */
#define IID_ASSOC_TARGET_DEVICE 0x02    /* Associated w/target device.  */

/*
 * Name Address Authority (NAA) Definitions:
 */
#define NAA_IEEE_EXTENDED       0x02    /* IEEE Extended format.        */
#define NAA_IEEE_REGISTERED     0x05    /* IEEE Registered format.      */
#define NAA_IEEE_REG_EXTENDED   0x06    /* IEEE Registered Extended.    */
                                        /* All other values reserved.   */

typedef struct inquiry_ident_descriptor {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		iid_code_set	: 4,	/* The code set.		[0] */
		iid_proto_ident : 4;	/* Protocol identifier.             */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		iid_proto_ident : 4,	/* Protocol identifier.	    	    */
		iid_code_set	: 4;	/* The code set.		[0] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		iid_ident_type	: 4,	/* The identifier type.i	[1] */
		iid_association	: 2,	/* Association.			    */
                iid_reserved_b6	: 1,    /* Reserved.                        */
                iid_proto_valid : 1;    /* Protocol identifier valid.       */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
        bitfield_t
		iid_proto_valid : 1,	/* Protocol identifier valid.       */
                iid_reserved_b6	: 1,    /* Reserved.                        */
		iid_association	: 2,	/* Association.			    */
		iid_ident_type	: 4;	/* The identifier type.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	unsigned char iid_reserved;	/* Reserved.			[2] */
	unsigned char iid_ident_length;	/* The identifier length.	[3] */
					/* Variable length identifier.	[4] */
} inquiry_ident_descriptor_t;

typedef struct inquiry_deviceid_page {
	inquiry_header_t inquiry_header;
	inquiry_ident_descriptor_t ident_descriptor;
} inquiry_deviceid_page_t;

/* Note: Defined without the inquiry page header! */
typedef struct inquiry_network_service_page {
    uint8_t association_service_type;
    uint8_t reserved;
    uint8_t address_length[2];
    uint8_t address[1];
} inquiry_network_service_page_t;

#if defined(__IBMC__)
#  pragma options align=reset
#endif /* defined(__IBMC__) */

#endif /* !defined(INQUIRY_INCLUDE) */
