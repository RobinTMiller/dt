/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2021			    *
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
/*
 * File:	scsi_data.c
 * Author:	Robin T. Miller
 * Date:	June 5th, 2007
 *
 * Descriptions:
 *	Functions and tables to decode SCSI data.
 *
 * Modification History:
 *
 * April 11th, 2012 by Robin T. Miller
 * 	Added token operation asc/asq (0x23, 0xNN) codes and error messages.
 *	Also added asc/ascq 0x55/0x0C and 0x55/0x0D for ROD Token errors.
 *
 * February 6th, 2012 by Robin T. Miller
 * 	When displaying additional sense data, dynamically allocate the memory
 * required, since the previously allocated 32 bytes could be exceeded!
 * 
 * May 12th, 2010 by Robin T. Miller
 * 	Added function to return SCSI operation code name.
 * 
 * April 23rd, 2010 by Robin T. Miller
 *	Update with new asc/ascq pairs from SPC4.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dt.h"
//#include "libscsi.h"
#include "inquiry.h"

void
print_scsi_status(scsi_generic_t *sgp, unsigned char scsi_status, unsigned char sense_key, unsigned char asc, unsigned char ascq)
{
    Fprintf(sgp->opaque, "    SCSI Status: %02Xh = %s\n", scsi_status, ScsiStatus(scsi_status));
    Fprintf(sgp->opaque, "      Sense Key: %02Xh = %s\n", sense_key, SenseKeyMsg(sense_key));
    Fprintf(sgp->opaque, "       asc/ascq: %02Xh/%02Xh = %s\n", asc, ascq, ScsiAscqMsg(asc, ascq));
    fflush(stderr);
}

/*
 * SCSI Status Code Table.
 */
static struct SCSI_StatusTable {
    unsigned char scsi_status;
    char          *status_msg;
    char	  *status_name;
} scsi_StatusTable[] = {
    { SCSI_GOOD,                 "SCSI_GOOD",		"good"	},	/* 0x00 */
    { SCSI_CHECK_CONDITION,      "SCSI_CHECK_CONDITION","cc"	},	/* 0x02 */
    { SCSI_CONDITION_MET,        "SCSI_CONDITION_MET",	"cmet"	},	/* 0x04 */
    { SCSI_BUSY,                 "SCSI_BUSY",		"busy"	},	/* 0x08 */
    { SCSI_INTERMEDIATE,         "SCSI_INTERMEDIATE",	"inter"	},	/* 0x10 */
    { SCSI_INTER_COND_MET,       "SCSI_INTER_COND_MET",	"icmet"	}, 	/* 0x14 */
    { SCSI_RESERVATION_CONFLICT, "SCSI_RESERVATION_CONFLICT", "rescon"},/* 0x18 */
    { SCSI_COMMAND_TERMINATED,   "SCSI_COMMAND_TERMINATED", "term" },	/* 0x22 */ /* obsolete */
    { SCSI_QUEUE_FULL,           "SCSI_QUEUE_FULL",	"qfull"	},	/* 0x28 */
    { SCSI_ACA_ACTIVE,           "SCSI_ACA_ACTIVE",	"aca_active"},	/* 0x30 */
    { SCSI_TASK_ABORTED,         "SCSI_TASK_ABORTED",	"aborted"}	/* 0x40 */
};
static int scsi_StatusEntrys = sizeof(scsi_StatusTable) / sizeof(scsi_StatusTable[0]);

/*
 * ScsiStatusMsg() - Translate SCSI Status to Message Text.
 *
 * Inputs:
 *  scsi_status = The SCSI status value.
 *
 * Return Value:
 *  Returns the SCSI status text string.
 */
char *
ScsiStatus(unsigned char scsi_status)
{
    struct SCSI_StatusTable *cst = scsi_StatusTable;
    int entrys;

    for (entrys = 0; entrys < scsi_StatusEntrys; cst++, entrys++) {
	if (cst->scsi_status == scsi_status) {
	    return (cst->status_msg);
	}
    }
    return ("???");
}

int
LookupScsiStatus(char *status_name)
{
    struct SCSI_StatusTable *cst = scsi_StatusTable;
    int entrys;

    for (entrys = 0; entrys < scsi_StatusEntrys; cst++, entrys++) {
	if (strcmp(cst->status_name, status_name) == 0) {
	    return (cst->scsi_status);
	}
    }
    return (-1);
}

/* ======================================================================== */

static struct SCSI_SenseKeyTable {
    unsigned char sense_key;
    char          *sense_msg;
    char	  *sense_name;
} scsi_SenseKeyTable[] = {
    { SKV_NOSENSE,		"NO SENSE",		"none"		},	/* 0x00 */
    { SKV_RECOVERED,		"RECOVERED ERROR",	"recovered"	},	/* 0x01 */
    { SKV_NOT_READY,		"NOT READY",		"notready"	},	/* 0x02 */
    { SKV_MEDIUM_ERROR,		"MEDIUM ERROR",		"medium"	},	/* 0x03 */
    { SKV_HARDWARE_ERROR,	"HARDWARE ERROR",	"hardware"	},	/* 0x04 */
    { SKV_ILLEGAL_REQUEST,	"ILLEGAL REQUEST",	"illegal"	}, 	/* 0x05 */
    { SKV_UNIT_ATTENTION,	"UNIT ATTENTION",	"ua"		},	/* 0x06 */
    { SKV_DATA_PROTECT,		"DATA PROTECT",		"dataprot"	},	/* 0x07 */
    { SKV_BLANK_CHECK,		"BLANK CHECK",		"blank"		},	/* 0x08 */
    { SKV_VENDOR_SPECIFIC,	"VENDOR SPECIFIC",	"vendor"	},	/* 0x09 */
    { SKV_COPY_ABORTED,		"COPY ABORTED",		"copyaborted"	},	/* 0x0a */
    { SKV_ABORTED_CMD,		"ABORTED COMMAND",	"aborted"	},	/* 0x0b*/
    { SKV_VOLUME_OVERFLOW,	"VOLUME OVERFLOW",	"overflow"	},	/* 0x0d */
    { SKV_MISCOMPARE,		"MISCOMPARE",		"miscompare"	}	/* 0x0e */
};
static int scsi_SenseKeyEntrys = sizeof(scsi_SenseKeyTable) / sizeof(scsi_SenseKeyTable[0]);

/*
 * ScsiStatusMsg() - Translate SCSI Status to Message Text.
 *
 * Inputs:
 *  scsi_status = The SCSI status value.
 *
 * Return Value:
 *  Returns the SCSI status text string.
 */
char *
SenseKeyMsg(unsigned char sense_key)
{
    struct SCSI_SenseKeyTable *skp = scsi_SenseKeyTable;
    int entrys;

    for (entrys = 0; entrys < scsi_SenseKeyEntrys; skp++, entrys++) {
	if (skp->sense_key == sense_key) {
	    return (skp->sense_msg);
	}
    }
    return ("???");
}

int
LookupSenseKey(char *sense_key_name)
{
    struct SCSI_SenseKeyTable *skp = scsi_SenseKeyTable;
    int entrys;

    for (entrys = 0; entrys < scsi_SenseKeyEntrys; skp++, entrys++) {
	if (strcmp(skp->sense_name, sense_key_name) == 0) {
	    return(skp->sense_key);
	}
    }
    return (-1);
}

/* ======================================================================== */

/*
 * Sense Code/Qualifier Table:
 *
 *                       D - DIRECT ACCESS BLOCK DEVICE (SBC-2)  Device Column key:
 *                       . T - SEQUENTIAL ACCESS DEVICE (SSC-2)  blank = code not used
 *                       .   L - PRINTER DEVICE (SSC)            not blank = code used
 *                       .     P - PROCESSOR DEVICE (SPC-2) 
 *                       .     . W - WRITE ONCE BLOCK DEVICE (SBC) 
 *                       .     .   R - CD/DVD DEVICE (MMC-4) 
 *                       .     .     O - OPTICAL MEMORY BLOCK DEVICE (SBC) 
 *                       .     .     . M - MEDIA CHANGER DEVICE (SMC-2) 
 *                       .     .     .   A - STORAGE ARRAY DEVICE (SCC-2) 
 *                       .     .     .     E - ENCLOSURE SERVICES DEVICE (SES) 
 *                       .     .     .     . B - SIMPLIFIED DIRECT-ACCESS DEVICE (RBC) 
 *                       .     .     .     .   K - OPTICAL CARD READER/WRITER DEVICE (OCRW) 
 *                       .     .     .     .     V - AUTOMATION/DRIVE INTERFACE (ADC) 
 *                       .     .     .     .     . F - OBJECT-BASED STORAGE (OSD) 
 *                       .     .     .     .     . 
 *         ASC  ASCQ     D T L P W R O M A E B K V F    Description 
 */
struct sense_entry SenseCodeTable[] = {
	{ 0x00, 0x00, /* D T L P W R O M A E B K V F */ "No additional sense information"			},
	{ 0x00, 0x01, /* T                           */ "Filemark detected"					},
	{ 0x00, 0x02, /* T                           */ "End-of-partition/medium detected"			},
	{ 0x00, 0x03, /* T                           */ "Setmark detected"					},
	{ 0x00, 0x04, /* T                           */ "Beginning-of-partition/medium detected"		},
	{ 0x00, 0x05, /* T   L                       */ "End-of-data detected"					},
	{ 0x00, 0x06, /* D T L P W R O M A E B K V F */ "I/O process terminated"				},
	{ 0x00, 0x07, /*   T                         */ "Programmable early warning detected"			},
	{ 0x00, 0x11, /*           R                 */ "Audio play operation in progress"			},
	{ 0x00, 0x12, /*           R                 */ "Audio play operation paused"				},
	{ 0x00, 0x13, /*           R                 */ "Audio play operation successfully completed"		},
	{ 0x00, 0x14, /*           R                 */ "Audio play operation stopped due to error"		},
	{ 0x00, 0x15, /*           R                 */ "No current audio status to return"			},
	{ 0x00, 0x16, /* D T L P W R O M A E B K V F */ "Operation in progress"					},
	{ 0x00, 0x17, /* D T L   W R O M A E B K V F */ "Cleaning requested"					},
	{ 0x00, 0x18, /* T                           */ "Erase operation in progress"				},
	{ 0x00, 0x19, /* T                           */ "Locate operation in progress"				},
	{ 0x00, 0x1A, /* T                           */ "Rewind operation in progress"				},
	{ 0x00, 0x1B, /* T                           */ "Set capacity operation in progress"			},
	{ 0x00, 0x1C, /* T                           */ "Verify operation in progress"				},
	{ 0x00, 0x1D, /* D T                 B       */ "ATA pass through information available"		},
	{ 0x00, 0x1E, /* D T       R M A E B   K V   */ "Conflicting SA creation request"			},
	{ 0x01, 0x00, /* D       W   O       B K     */ "No index/sector signal"				},
	{ 0x02, 0x00, /* D       W R O M     B K     */ "No seek complete"					},
	{ 0x03, 0x00, /* D T L   W   O       B K     */ "Peripheral device write fault"				},
	{ 0x03, 0x01, /* T                           */ "No write current"					},
	{ 0x03, 0x02, /* T                           */ "Excessive write errors"				},
	{ 0x04, 0x00, /* D T L P W R O M A E B K V F */ "Logical unit not ready, cause not reportable"		},
	{ 0x04, 0x01, /* D T L P W R O M A E B K V F */ "Logical unit is in process of becoming ready"		},
	{ 0x04, 0x02, /* D T L P W R O M A E B K V F */ "Logical unit not ready, initializing command required" },
	{ 0x04, 0x03, /* D T L P W R O M A E B K V F */ "Logical unit not ready, manual intervention required"  },
	{ 0x04, 0x04, /* D T L     R O       B       */ "Logical unit not ready, format in progress"		},
	{ 0x04, 0x05, /* D T     W   O M A   B K     */ "Logical unit not ready, rebuild in progress"		},
	{ 0x04, 0x06, /* D T     W   O M A   B K     */ "Logical unit not ready, recalculation in progress"	},
	{ 0x04, 0x07, /* D T L P W R O M A E B K V F */ "Logical unit not ready, operation in progress"		},
	{ 0x04, 0x08, /*           R                 */ "Logical unit not ready, long write in progress"	},
	{ 0x04, 0x09, /* D T L P W R O M A E B K V F */ "Logical unit not ready, self-test in progress"		},
	{ 0x04, 0x0A, /* D T L P W R O M A E B K V F */ "Logical unit not accessible, asymmetric access state transition" },
	{ 0x04, 0x0B, /* D T L P W R O M A E B K V F */ "Logical unit not accessible, target port in standby state" },
	{ 0x04, 0x0C, /* D T L P W R O M A E B K V F */ "Logical unit not accessible, target port in unavailable state" },
	{ 0x04, 0x0D, /*                           F */ "Logical unit not ready, structure check required"	},
	{ 0x04, 0x10, /* D T     W R O M     B       */ "Logical unit not ready, auxiliary memory not accessible" },
	{ 0x04, 0x11, /* D T     W R O M A E B   V F */ "Logical unit not ready, notify (enable spinup) required" },
	{ 0x04, 0x12, /*                         V   */ "Logical unit not ready, offline"			},
	{ 0x04, 0x13, /* D T       R   M A E B K V   */ "Logical unit not ready, sa creation in progress"	},
	{ 0x04, 0x14, /* D                   B       */ "Logical unit not ready, space allocation in progress"	},
	{ 0x04, 0x15, /*               M             */ "Logical unit not ready, robotics disabled"		},
	{ 0x04, 0x16, /*               M             */ "Logical unit not ready, configuration required"	},
	{ 0x04, 0x17, /*               M             */ "Logical unit not ready, calibration required"		},
	{ 0x04, 0x18, /*               M             */ "Logical unit not ready, a door is open"		},
	{ 0x04, 0x19, /*               M             */ "Logical unit not ready, operating in sequential mode"	},
	{ 0x05, 0x00, /* D T L   W R O M A E B K V F */ "Logical unit does not respond to selection"		},
	{ 0x06, 0x00, /* D       W R O M     B K     */ "No reference position found"				},
	{ 0x07, 0x00, /* D T L   W R O M     B K     */ "Multiple peripheral devices selected"			},
	{ 0x08, 0x00, /* D T L   W R O M A E B K V F */ "Logical unit communication failure"			},
	{ 0x08, 0x01, /* D T L   W R O M A E B K V F */ "Logical unit communication time-out"			},
	{ 0x08, 0x02, /* D T L   W R O M A E B K V F */ "Logical unit communication parity error"		},
	{ 0x08, 0x03, /* D T       R O M     B K     */ "Logical unit communication CRC error (ULTRA-DMA/32)"	},
	{ 0x08, 0x04, /* D T L P W R O         K     */ "Unreachable copy target"				},
	{ 0x09, 0x00, /* D T     W R O       B       */ "Track following error"					},
	{ 0x09, 0x01, /*         W R O         K     */ "Tracking servo failure"				},
	{ 0x09, 0x02, /*         W R O         K     */ "Focus servo failure"					},
	{ 0x09, 0x03, /*         W R O               */ "Spindle servo failure"					},
	{ 0x09, 0x04, /* D T     W R O       B       */ "Head select fault"					},
	{ 0x0A, 0x00, /* D T L P W R O M A E B K V F */ "Error log overflow"					},
	{ 0x0B, 0x00, /* D T L P W R O M A E B K V F */ "Warning"						},
	{ 0x0B, 0x01, /* D T L P W R O M A E B K V F */ "Warning - specified temperature exceeded"		},
	{ 0x0B, 0x02, /* D T L P W R O M A E B K V F */ "Warning - enclosure degraded"				},
	{ 0x0B, 0x03, /* D T L P W R O M A E B K V F */ "Warning - background self-test failed"			},
	{ 0x0B, 0x04, /* D T L P W R O   A E B K V F */ "Warning - background pre-scan detected medium error"	},
	{ 0x0B, 0x05, /* D T L P W R O   A E B K V F */ "Warning - background medium scan detected medium error" },
	{ 0x0B, 0x06, /* D T L P W R O M A E B K V F */ "Warning - non-volatile cache now volatile"		},
	{ 0x0B, 0x07, /* D T L P W R O M A E B K V F */ "Warning - degraded power to non-volatile cache"	},
	{ 0x0B, 0x08, /* D T L P W R O M A E B K V F */ "Warning - power loss expected"				},
	{ 0x0C, 0x00, /*   T       R                 */ "Write error"						},
	{ 0x0C, 0x01, /*                       K     */ "Write error - recovered with auto reallocation"	},
	{ 0x0C, 0x02, /* D       W   O       B K     */ "Write error - auto reallocation failed"		},
	{ 0x0C, 0x03, /* D       W   O       B K     */ "Write error - recommend reassignment"			},
	{ 0x0C, 0x04, /* D T     W   O       B       */ "Compression check miscompare error"			},
	{ 0x0C, 0x05, /* D T     W   O       B       */ "Data expansion occurred during compression"		},
	{ 0x0C, 0x06, /* D T     W   O       B       */ "Block not compressible"				},
	{ 0x0C, 0x07, /*           R                 */ "Write error - recovery needed"				},
	{ 0x0C, 0x08, /*           R                 */ "Write error - recovery failed"				},
	{ 0x0C, 0x09, /*           R                 */ "Write error - loss of streaming"			},
	{ 0x0C, 0x0A, /*           R                 */ "Write error - padding blocks added"			},
	{ 0x0C, 0x0B, /* D T     W R O M     B       */ "Auxiliary memory write error"				},
	{ 0x0C, 0x0C, /* D T L P W R O M A E B K V F */ "Write error - unexpected unsolicited data"		},
	{ 0x0C, 0x0D, /* D T L P W R O M A E B K V F */ "Write error - not enough unsolicited data"		},
	{ 0x0C, 0x0F, /*           R                 */ "Defects in error window"				},
	{ 0x0D, 0x00, /* D T L P W R O   A     K     */ "Error detected by third party temporary initiator"	},
	{ 0x0D, 0x01, /* D T L P W R O   A     K     */ "Third party device failure"				},
	{ 0x0D, 0x02, /* D T L P W R O   A     K     */ "Copy target device not reachable"			},
	{ 0x0D, 0x03, /* D T L P W R O   A     K     */ "Incorrect copy target device type"			},
	{ 0x0D, 0x04, /* D T L P W R O   A     K     */ "Copy target device data underrun"			},
	{ 0x0D, 0x05, /* D T L P W R O   A     K     */ "Copy target device data overrun"			},
	{ 0x0E, 0x00, /* D T   P W R O M A E B K   F */ "Invalid information unit"				},
	{ 0x0E, 0x01, /* D T   P W R O M A E B K   F */ "Information unit too short"				},
	{ 0x0E, 0x02, /* D T   P W R O M A E B K   F */ "Information unit too long"				},
	{ 0x0E, 0x03, /* D T   P   R   M A E B K   F */ "Invalid field in command information unit"		},
	{ 0x10, 0x00, /* D       W   O       B K     */ "ID CRC or ECC error"					},
	{ 0x10, 0x01, /* D T     W   O               */ "Data block guard check failed"				},
	{ 0x10, 0x02, /* D T     W   O               */ "Data block application tag check failed"		},
	{ 0x10, 0x03, /* D T     W   O               */ "Data block reference tag check failed"			},
	{ 0x11, 0x00, /* D T     W R O       B K     */ "Unrecovered read error"				},
	{ 0x11, 0x01, /* D T     W R O       B K     */ "Read retries exhausted"				},
	{ 0x11, 0x02, /* D T     W R O       B K     */ "Error too long to correct"				},
	{ 0x11, 0x03, /* D T     W   O       B K     */ "Multiple read errors"					},
	{ 0x11, 0x04, /* D       W   O       B K     */ "Unrecovered read error - auto reallocate failed"	},
	{ 0x11, 0x05, /*         W R O       B       */ "L-EC uncorrectable error"				},
	{ 0x11, 0x06, /*         W R O       B       */ "CIRC unrecovered error"				},
	{ 0x11, 0x07, /*         W O         B       */ "Data re-synchronization error"				},
	{ 0x11, 0x08, /*   T                         */ "Incomplete block read"					},
	{ 0x11, 0x09, /*   T                         */ "No gap found"						},
	{ 0x11, 0x0A, /* D T         O       B K     */ "Miscorrected"						},
	{ 0x11, 0x0B, /* D       W   O       B K     */ "Unrecovered read error - recommend reassignment"	},
	{ 0x11, 0x0C, /* D       W   O       B K     */ "Unrecovered read error - recommend rewrite the data"	},
	{ 0x11, 0x0D, /* D T     W R O       B       */ "De-compression crc error"				},
	{ 0x11, 0x0E, /* D T     W R O       B       */ "Cannot decompress using declared algorithm"		},
	{ 0x11, 0x0F, /*           R                 */ "Error reading UPC/EAN number"				},
	{ 0x11, 0x10, /*           R                 */ "Error reading ISRC number"				},
	{ 0x11, 0x11, /*           R                 */ "Read error - loss of streaming"			},
	{ 0x11, 0x12, /* D T     W R O M     B       */ "Auxiliary memory read error"				},
	{ 0x11, 0x13, /* D T L P W R O M A E B K V F */ "Read error - failed retransmission request"		},
	{ 0x11, 0x14, /* D                           */ "Read error - LBA marked bad by application client"	},
	{ 0x12, 0x00, /* D       W   O       B K     */ "Address mark not found for id field"			},
	{ 0x13, 0x00, /* D       W   O       B K     */ "Address mark not found for data field"			},
	{ 0x14, 0x00, /* D T L   W R O       B K     */ "Recorded entity not found"				},
	{ 0x14, 0x01, /* D T     W R O       B K     */ "Record not found"					},
	{ 0x14, 0x02, /*   T                         */ "Filemark or setmark not found"				},
	{ 0x14, 0x03, /*   T                         */ "End-of-data not found"					},
	{ 0x14, 0x04, /*   T                         */ "Block sequence error"					},
	{ 0x14, 0x05, /* D T     W   O       B K     */ "Record not found - recommend reassignment"		},
	{ 0x14, 0x06, /* D T     W   O       B K     */ "Record not found - data auto-reallocated"		},
	{ 0x14, 0x07, /*   T                         */ "Locate operation failure"				},
	{ 0x15, 0x00, /* D T L   W R O M     B K     */ "Random positioning error"				},
	{ 0x15, 0x01, /* D T L   W R O M     B K     */ "Mechanical positioning error"				},
	{ 0x15, 0x02, /* D T     W R O       B K     */ "Positioning error detected by read of medium"		},
	{ 0x16, 0x00, /* D       W   O       B K     */ "Data synchronization mark error"			},
	{ 0x16, 0x01, /* D       W   O       B K     */ "Data sync error - data rewritten"			},
	{ 0x16, 0x02, /* D       W   O       B K     */ "Data sync error - recommend rewrite"			},
	{ 0x16, 0x03, /* D       W   O       B K     */ "Data sync error - data auto-reallocated"		},
	{ 0x16, 0x04, /* D       W   O       B K     */ "Data sync error - recommend reassignment"		},
	{ 0x17, 0x00, /* D T     W R O       B K     */ "Recovered data with no error correction applied"	},
	{ 0x17, 0x01, /* D T     W R O       B K     */ "Recovered data with retries"				},
	{ 0x17, 0x02, /* D T     W R O       B K     */ "Recovered data with positive head offset"		},
	{ 0x17, 0x03, /* D T     W R O       B K     */ "Recovered data with negative head offset"		},
	{ 0x17, 0x04, /*         W R O       B       */ "Recovered data with retries and/or circ applied"	},
	{ 0x17, 0x05, /* D       W R O       B K     */ "Recovered data using previous sector id"		},
	{ 0x17, 0x06, /* D       W   O       B K     */ "Recovered data without ECC - data auto-reallocated"   	},
	{ 0x17, 0x07, /* D       W R O       B K     */ "Recovered data without ECC - recommend reassignment"	},
	{ 0x17, 0x08, /* D       W R O       B K     */ "Recovered data without ECC - recommend rewrite"	},
	{ 0x17, 0x09, /* D       W R O       B K     */ "Recovered data without ECC - data rewritten"		},
	{ 0x18, 0x00, /* D T     W R O       B K     */ "Recovered data with error correction applied"		},
	{ 0x18, 0x01, /* D       W R O       B K     */ "Recovered data with error corr. & retries applied"	},
	{ 0x18, 0x02, /* D       W R O       B K     */ "Recovered data - data auto-reallocated"		},
	{ 0x18, 0x03, /*           R                 */ "Recovered data with CIRC"				},
	{ 0x18, 0x04, /*           R                 */ "Recovered data with L-EC"				},
	{ 0x18, 0x05, /* D       W R O       B K     */ "Recovered data - recommend reassignment"		},
	{ 0x18, 0x06, /* D       W R O       B K     */ "Recovered data - recommend rewrite"			},
	{ 0x18, 0x07, /* D       W   O       B K     */ "Recovered data with ecc - data rewritten"		},
	{ 0x18, 0x08, /*           R                 */ "Recovered data with linking"				},
	{ 0x19, 0x00, /* D           O         K     */ "Defect list error"					},
	{ 0x19, 0x01, /* D           O         K     */ "Defect list not available"				},
	{ 0x19, 0x02, /* D           O         K     */ "Defect list error in primary list"			},
	{ 0x19, 0x03, /* D           O         K     */ "Defect list error in grown list"			},
	{ 0x1A, 0x00, /* D T L P W R O M A E B K V F */ "Parameter list length error"				},
	{ 0x1B, 0x00, /* D T L P W R O M A E B K V F */ "Synchronous data transfer error"			},
	{ 0x1C, 0x00, /* D           O       B K     */ "Defect list not found"					},
	{ 0x1C, 0x01, /* D           O       B K     */ "Primary defect list not found"				},
	{ 0x1C, 0x02, /* D           O       B K     */ "Grown defect list not found"				},
	{ 0x1D, 0x00, /* D T     W R O       B K     */ "Miscompare during verify operation"			},
	{ 0x1D, 0x01, /* D                   B       */ "Miscompare verify of unmapped lba"			},
	{ 0x1E, 0x00, /* D       W   O       B K     */ "Recovered id with ECC correction"			},
	{ 0x1F, 0x00, /* D           O         K     */ "Partial defect list transfer"				},
	{ 0x20, 0x00, /* D T L P W R O M A E B K V F */ "Invalid command operation code"			},
	{ 0x20, 0x01, /* D T   P W R O M A E B K     */ "Access denied - initiator pending-enrolled"		},
	{ 0x20, 0x02, /* D T   P W R O M A E B K     */ "Access denied - no access rights"			},
	{ 0x20, 0x03, /* D T   P W R O M A E B K     */ "Access denied - invalid mgmt id key"			},
	{ 0x20, 0x04, /*   T                         */ "Illegal command while in write capable state"		},
	{ 0x20, 0x05, /*   T                         */ "Obsolete"						},
	{ 0x20, 0x06, /*   T                         */ "Illegal command while in explicit address mode"	},
	{ 0x20, 0x07, /*   T                         */ "Illegal command while in implicit address mode"	},
	{ 0x20, 0x08, /* D T P   W R O M A E B K     */ "Access denied - enrollment conflict"			},
	{ 0x20, 0x09, /* D T P   W R O M A E B K     */ "Access denied - invalid lu identifier"			},
	{ 0x20, 0x0A, /* D T P   W R O M A E B K     */ "Access denied - invalid proxy token"			},
	{ 0x20, 0x0B, /* D T P   W R O M A E B K     */ "Access denied - ACL LUN conflict"			},
	{ 0x20, 0x0C, /*   T                         */ "Illegal command when not in append-only mode"		},
	{ 0x21, 0x00, /* D T     W R O M     B K     */ "Logical block address out of range"			},
	{ 0x21, 0x01, /* D T     W R O M     B K     */ "Invalid element address"				},
	{ 0x21, 0x02, /*           R                 */ "Invalid address for write"				},
	{ 0x21, 0x03, /*           R                 */ "Invalid write crossing layer jump"			},
	{ 0x22, 0x00, /* D                           */ "Illegal function (use 20 00, 24 00, or 26 00)"		},
	{ 0x23, 0x00, /* D T   P             B       */ "Invalid token operation, cause not reportable"		},
	{ 0x23, 0x01, /* D T   P             B       */ "Invalid token operation, unsupported token type"	},
	{ 0x23, 0x02, /* D T   P             B       */ "Invalid token operation, remote token usage not supported" },
	{ 0x23, 0x03, /* D T   P             B       */ "Invalid token operation, remote rod token creation not supported" },
	{ 0x23, 0x04, /* D T   P             B       */ "Invalid token operation, token unknown"		},
	{ 0x23, 0x05, /* D T   P             B       */ "Invalid token operation, token corrupt"		},
	{ 0x23, 0x06, /* D T   P             B       */ "Invalid token operation, token revoked"		},
	{ 0x23, 0x07, /* D T   P             B       */ "Invalid token operation, token expired"		},
	{ 0x23, 0x08, /* D T   P             B       */ "Invalid token operation, token cancelled"		},
	{ 0x23, 0x09, /* D T   P             B       */ "Invalid token operation, token deleted"		},
	{ 0x23, 0x0A, /* D T   P             B       */ "Invalid token operation, invalid token length"		},
	{ 0x24, 0x00, /* D T L P W R O M A E B K V F */ "Invalid field in CDB"					},
	{ 0x24, 0x01, /* D T L P W R O M A E B K V F */ "CDB decryption error"					},
	{ 0x24, 0x02, /*   T                         */ "Obsolete"						},
	{ 0x24, 0x03, /*   T                         */ "Obsolete"						},
	{ 0x24, 0x04, /*                           F */ "Security audit value frozen"				},
	{ 0x24, 0x05, /*                           F */ "Security working key frozen"				},
	{ 0x24, 0x06, /*                           F */ "Nonce not unique"					},
	{ 0x24, 0x07, /*                           F */ "Nonce timestamp out of range"				},
	{ 0x24, 0x08, /* D T       R   M A E B K V   */ "Invalid XCDB"						},
	{ 0x25, 0x00, /* D T L P W R O M A E B K V F */ "Logical unit not supported"				},
	{ 0x26, 0x00, /* D T L P W R O M A E B K V F */ "Invalid field in parameter list"			},
	{ 0x26, 0x01, /* D T L P W R O M A E B K V F */ "Parameter not supported"				},
	{ 0x26, 0x02, /* D T L P W R O M A E B K V F */ "Parameter value invalid"				},
	{ 0x26, 0x03, /* D T L P W R O M A E   K     */ "Threshold parameters not supported"			},
	{ 0x26, 0x04, /* D T L P W R O M A E B K V F */ "Invalid release of persistent reservation"		},
	{ 0x26, 0x05, /* D T L P W R O M A   B K     */ "Data decryption error"					},
	{ 0x26, 0x06, /* D T L P W R O         K     */ "Too many target descriptors"				},
	{ 0x26, 0x07, /* D T L P W R O         K     */ "Unsupported target descriptor type code"		},
	{ 0x26, 0x08, /* D T L P W R O         K     */ "Too many segment descriptors"				},
	{ 0x26, 0x09, /* D T L P W R O         K     */ "Unsupported segment descriptor type code"		},
	{ 0x26, 0x0A, /* D T L P W R O         K     */ "Unexpected inexact segment"				},
	{ 0x26, 0x0B, /* D T L P W R O         K     */ "Inline data length exceeded"				},
	{ 0x26, 0x0C, /* D T L P W R O         K     */ "Invalid operation for copy source or destination"	},
	{ 0x26, 0x0D, /* D T L P W R O         K     */ "Copy segment granularity violation"			},
	{ 0x26, 0x0E, /* D T   P W R O M A E B K     */ "Invalid parameter while port is enabled"		},
	{ 0x26, 0x0F, /*                           F */ "Invalid data-out buffer integrity check value"		},
	{ 0x26, 0x10, /*   T                         */ "Data decryption key fail limit reached"		},
	{ 0x26, 0x11, /*   T                         */ "Incomplete key-associated data set"			},
	{ 0x26, 0x12, /*   T                         */ "Vendor specific key reference not found"		},
	{ 0x27, 0x00, /* D T     W R O       B K     */ "Write protected"					},
	{ 0x27, 0x01, /* D T     W R O       B K     */ "Hardware write protected"				},
	{ 0x27, 0x02, /* D T     W R O       B K     */ "Logical unit software write protected"			},
	{ 0x27, 0x03, /*   T       R                 */ "Associated write protect"		       		},
	{ 0x27, 0x04, /*   T       R                 */ "Persistent write protect"				},
	{ 0x27, 0x05, /*   T       R                 */ "Permanent write protect"				},
	{ 0x27, 0x06, /*           R                 */ "Conditional write protect"				},
	{ 0x27, 0x07, /* D                   B       */ "Space allocation failed write protect"			},
	{ 0x27, 0x08, /* D                   B       */ "Zone is read only"					},
	{ 0x28, 0x00, /* D T L P W R O M A E B K V F */ "Not ready to ready change, medium may have changed"	},
	{ 0x28, 0x01, /* D T     W R O M     B       */ "Import or export element accessed"			},
	{ 0x28, 0x02, /*           R                 */ "Format-layer may have changed"				},
	{ 0x28, 0x03, /*               M             */ "Import/export element accessed, medium changed"	},
	{ 0x29, 0x00, /* D T L P W R O M A E B K V F */ "Power on, reset, or bus device reset occurred"		},
	{ 0x29, 0x01, /* D T L P W R O M A E B K V F */ "Power on occurred"					},
	{ 0x29, 0x02, /* D T L P W R O M A E B K V F */ "SCSI bus reset occurred"				},
	{ 0x29, 0x03, /* D T L P W R O M A E B K V F */ "Bus device reset function occurred"			},
	{ 0x29, 0x04, /* D T L P W R O M A E B K V F */ "Device internal reset"					},
	{ 0x29, 0x05, /* D T L P W R O M A E B K V F */ "Transceiver mode changed to single-ended"		},
	{ 0x29, 0x06, /* D T L P W R O M A E B K V F */ "Transceiver mode changed to LVD"			},
	{ 0x29, 0x07, /* D T L P W R O M A E B K V F */ "I_T nexus loss occurred"				},
	{ 0x2A, 0x00, /* D T L W R O M   A E B K V F */ "Parameters changed"					},
	{ 0x2A, 0x01, /* D T L W R O M   A E B K V F */ "Mode parameters changed"				},
	{ 0x2A, 0x02, /* D T L W R O M   A E   K     */ "Log parameters changed"				},
	{ 0x2A, 0x03, /* D T L P W R O M A E   K     */ "Reservations preempted"				},
	{ 0x2A, 0x04, /* D T L P W R O M A E         */ "Reservations released"					},
	{ 0x2A, 0x05, /* D T L P W R O M A E         */ "Registrations preempted"				},
	{ 0x2A, 0x06, /* D T L P W R O M A E B K V F */ "Asymmetric access state changed"			},
	{ 0x2A, 0x07, /* D T L P W R O M A E B K V F */ "Implicit asymmetric access state transition failed"	},
	{ 0x2A, 0x08, /* D T     W R O M A E B K V F */ "Priority changed"					},
	{ 0x2A, 0x09, /* D                           */ "Capacity data has changed"				},
	{ 0x2A, 0x0A, /* D T                         */ "Error history I_T nexus cleared"			},
	{ 0x2A, 0x0B, /* D T                         */ "Error history snapshot released"			},
	{ 0x2A, 0x0C, /*                           F */ "Error recovery attributes have changed"		},
	{ 0x2A, 0x0D, /*   T                         */ "Data encryption capabilities changed"			},
	{ 0x2A, 0x10, /* D T           M   E     V   */ "Timestamp changed"					},
	{ 0x2A, 0x11, /*   T                         */ "Data encryption parameters changed by another I_T nexus" },
	{ 0x2A, 0x12, /*   T                         */ "Data encryption parameters changed by vendor specific event" },
	{ 0x2A, 0x13, /*   T                         */ "Data encryption key instance counter has changed"	},
	{ 0x2A, 0x14, /* D T       R   M A E B K V   */ "SA creation capabilities data has changed"		},
	{ 0x2B, 0x00, /* D T L P W R O         K     */ "Copy cannot execute since host cannot disconnect"	},
	{ 0x2C, 0x00, /* D T L P W R O M A E B K V F */ "Command sequence error"				},
	{ 0x2C, 0x01, /*                             */ "Too many windows specified"				},
	{ 0x2C, 0x02, /*                             */ "Invalid combination of windows specified"		},
	{ 0x2C, 0x03, /*           R                 */ "Current program area is not empty"			},
	{ 0x2C, 0x04, /*           R                 */ "Current program area is empty"				},
	{ 0x2C, 0x05, /*                    B        */ "Illegal power condition request"			},
	{ 0x2C, 0x06, /*           R                 */ "Persistent prevent conflict"				},
	{ 0x2C, 0x07, /* D T L P W R O M A E B K V F */ "Previous busy status"					},
	{ 0x2C, 0x08, /* D T L P W R O M A E B K V F */ "Previous task set full status"				},
	{ 0x2C, 0x09, /* D T L P W R O M   E B K V F */ "Previous reservation conflict status"			},
	{ 0x2C, 0x0A, /*                           F */ "Partition or collection contains user objects"		},
	{ 0x2C, 0x0B, /*   T                         */ "Not reserved"						},
	{ 0x2D, 0x00, /*   T                         */ "Overwrite error on update in place"			},
	{ 0x2E, 0x00, /*           R                 */ "Insufficient time for operation"			},
	{ 0x2F, 0x00, /* D T L P W R O M A E B K V F */ "Commands cleared by another initiator"			},
	{ 0x2F, 0x01, /* D                           */ "Commands cleared by power loss notification"		},
	{ 0x2F, 0x02, /* D T L P W R O M A E B K V F */ "Commands cleared by device server"			},
	{ 0x30, 0x00, /* D T     W R O M     B K     */ "Incompatible medium installed"				},
	{ 0x30, 0x01, /* D T     W R O       B K     */ "Cannot read medium - unknown format"			},
	{ 0x30, 0x02, /* D T     W R O       B K     */ "Cannot read medium - incompatible format"		},
	{ 0x30, 0x03, /* D T       R           K     */ "Cleaning cartridge installed"				},
	{ 0x30, 0x04, /* D T     W R O       B K     */ "Cannot write medium - unknown format"			},
	{ 0x30, 0x05, /* D T     W R O       B K     */ "Cannot write medium - incompatible format"		},
	{ 0x30, 0x06, /* D T     W R O       B       */ "Cannot format medium - incompatible medium"		},
	{ 0x30, 0x07, /* D T L   W R O M A E B K V F */ "Cleaning failure"					},
	{ 0x30, 0x08, /*           R                 */ "Cannot write - application code mismatch"		},
	{ 0x30, 0x09, /*           R                 */ "Current session not fixated for append"		},
	{ 0x30, 0x0A, /* D T     W R O M A E B K     */ "Cleaning request rejected"				},
	{ 0x30, 0x0C, /*   T                         */ "Worm medium - overwrite attempted"			},
	{ 0x30, 0x0D, /*   T                         */ "Worm medium - integrity check"				},
	{ 0x30, 0x10, /*           R                 */ "Medium not formatted"					},
	{ 0x30, 0x11, /*               M             */ "Incompatible volume type"				},
	{ 0x30, 0x12, /*               M             */ "Incompatible volume qualifier"				},
	{ 0x30, 0x13, /*               M             */ "Cleaning volume expired"				},
	{ 0x31, 0x00, /* D T     W R O       B K     */ "Medium format corrupted"				},
	{ 0x31, 0x01, /* D L       R O       B       */ "Format command failed"					},
	{ 0x31, 0x02, /*           R                 */ "Zoned formatting failed due to spare linking"		},
	{ 0x32, 0x00, /* D       W   O       B K     */ "No defect spare location available"			},
	{ 0x32, 0x01, /* D       W   O       B K     */ "Defect list update failure"				},
	{ 0x33, 0x00, /*   T                         */ "Tape length error"					},
	{ 0x34, 0x00, /* D T L P W R O M A E B K V F */ "Enclosure failure"					},
	{ 0x35, 0x00, /* D T L P W R O M A E B K V F */ "Enclosure services failure"				},
	{ 0x35, 0x01, /* D T L P W R O M A E B K V F */ "Unsupported enclosure function"			},
	{ 0x35, 0x02, /* D T L P W R O M A E B K V F */ "Enclosure services unavailable"			},
	{ 0x35, 0x03, /* D T L P W R O M A E B K V F */ "Enclosure services transfer failure"			},
	{ 0x35, 0x04, /* D T L P W R O M A E B K V F */ "Enclosure services transfer refused"			},
	{ 0x35, 0x05, /* D T L   W R O M A E B K V F */ "Enclosure services checksum error"			},
	{ 0x36, 0x00, /*     L                       */ "Ribbon, ink, or toner failure"				},
	{ 0x37, 0x00, /* D T L   W R O M A E B K V F */ "Rounded parameter"					},
	{ 0x38, 0x00, /*                     B       */ "Event status notification"				},
	{ 0x38, 0x02, /*                     B       */ "ESN - power management class event"			},
	{ 0x38, 0x04, /*                     B       */ "ESN - media class event"				},
	{ 0x38, 0x06, /*                     B       */ "ESN - device busy class event"				},
	{ 0x38, 0x07, /* D                           */ "Thin provisioning soft threshold reached"		},
	{ 0x39, 0x00, /* D T L   W R O M A E   K     */ "Saving parameters not supported"			},
	{ 0x3A, 0x00, /* D T L   W R O M     B K     */ "Medium not present"					},
	{ 0x3A, 0x01, /* D T     W R O M     B K     */ "Medium not present - tray closed"			},
	{ 0x3A, 0x02, /* D T     W R O M     B K     */ "Medium not present - tray open"			},
	{ 0x3A, 0x03, /* D T     W R O M     B       */ "Medium not present - loadable"				},
	{ 0x3A, 0x04, /* D T     W R O M     B       */ "Medium not present - medium auxiliary memory accessible" },
	{ 0x3B, 0x00, /*   T L                       */ "Sequential positioning error"				},
	{ 0x3B, 0x01, /*   T                         */ "Tape position error at beginning-of-medium"		},
	{ 0x3B, 0x02, /*   T                         */ "Tape position error at end-of-medium"			},
	{ 0x3B, 0x03, /*     L                       */ "Tape or electronic vertical forms unit not ready"	},
	{ 0x3B, 0x04, /*     L                       */ "Slew failure"						},
	{ 0x3B, 0x05, /*     L                       */ "Paper jam"						},
	{ 0x3B, 0x06, /*     L                       */ "Failed to sense top-of-form"				},
	{ 0x3B, 0x07, /*     L                       */ "Failed to sense bottom-of-form"			},
	{ 0x3B, 0x08, /*   T                         */ "Reposition error"					},
	{ 0x3B, 0x09, /*                             */ "Read past end of medium"				},
	{ 0x3B, 0x0A, /*                             */ "Read past beginning of medium"				},
	{ 0x3B, 0x0B, /*                             */ "Position past end of medium"				},
	{ 0x3B, 0x0C, /*   T                         */ "Position past beginning of medium"			},
	{ 0x3B, 0x0D, /* D T     W R O M     B K     */ "Medium destination element full"			},
	{ 0x3B, 0x0E, /* D T     W R O M     B K     */ "Medium source element empty"				},
	{ 0x3B, 0x0F, /*       R                     */ "End of medium reached"					},
	{ 0x3B, 0x11, /* D T     W R O M     B K     */ "Medium magazine not accessible"			},
	{ 0x3B, 0x12, /* D T     W R O M     B K     */ "Medium magazine removed"				},
	{ 0x3B, 0x13, /* D T     W R O M     B K     */ "Medium magazine inserted"				},
	{ 0x3B, 0x14, /* D T     W R O M     B K     */ "Medium magazine locked"				},
	{ 0x3B, 0x15, /* D T     W R O M     B K     */ "Medium magazine unlocked"				},
	{ 0x3B, 0x16, /*       R                     */ "Mechanical positioning or changer error"		},
	{ 0x3B, 0x17, /*                           F */ "Read past end of user object"				},
	{ 0x3B, 0x18, /*               M             */ "Element disabled"					},
	{ 0x3B, 0x19, /*               M             */ "Element enabled"					},
	{ 0x3B, 0x1A, /*               M             */ "Data transfer device removed"				},
	{ 0x3B, 0x1B, /*               M             */ "Data transfer device inserted"				},
	{ 0x3D, 0x00, /* D T L P W R O M A E   K     */ "Invalid bits in identify message"			},
	{ 0x3E, 0x00, /* D T L P W R O M A E B K V F */ "Logical unit has not self-configured yet"		},
	{ 0x3E, 0x01, /* D T L P W R O M A E B K V F */ "Logical unit failure"					},
	{ 0x3E, 0x02, /* D T L P W R O M A E B K V F */ "Timeout on logical unit"				},
	{ 0x3E, 0x03, /* D T L P W R O M A E B K V F */ "Logical unit failed self-test"				},
	{ 0x3E, 0x04, /* D T L P W R O M A E B K V F */ "Logical unit unable to update self-test log"		},
	{ 0x3F, 0x00, /* D T L P W R O M A E B K V F */ "Target operating conditions have changed"		},
	{ 0x3F, 0x01, /* D T L P W R O M A E B K V F */ "Microcode has been changed"				},
	{ 0x3F, 0x02, /* D T L P W R O M     B K     */ "Changed operating definition"				},
	{ 0x3F, 0x03, /* D T L P W R O M A E B K V F */ "Inquiry data has changed"				},
	{ 0x3F, 0x04, /* D T     W R O M A E B K     */ "Component device attached"				},
	{ 0x3F, 0x05, /* D T     W R O M A E B K     */ "Device identifier changed"				},
	{ 0x3F, 0x06, /* D T     W R O M A E B       */ "Redundancy group created or modified"			},
	{ 0x3F, 0x07, /* D T     W R O M A E B       */ "Redundancy group deleted"				},
	{ 0x3F, 0x08, /* D T     W R O M A E B       */ "Spare created or modified"				},
	{ 0x3F, 0x09, /* D T     W R O M A E B       */ "Spare deleted"						},
	{ 0x3F, 0x0A, /* D T     W R O M A E B K     */ "Volume set created or modified"			},
	{ 0x3F, 0x0B, /* D T     W R O M A E B K     */ "Volume set deleted"					},
	{ 0x3F, 0x0C, /* D T     W R O M A E B K     */ "Volume set deassigned"					},
	{ 0x3F, 0x0D, /* D T     W R O M A E B K     */ "Volume set reassigned"					},
	{ 0x3F, 0x0E, /* D T L P W R O M A E         */ "Reported LUNs data has changed"			},
	{ 0x3F, 0x0F, /* D T L P W R O M A E B K V F */ "Echo buffer overwritten"				},
	{ 0x3F, 0x10, /* D T     W R O M     B       */ "Medium loadable"					},
	{ 0x3F, 0x11, /* D T     W R O M     B       */ "Medium auxiliary memory accessible"			},
	{ 0x3F, 0x12, /* D T L P W R   M A E B K   F */ "iSCSI IP address added"				},
	{ 0x3F, 0x13, /* D T L P W R   M A E B K   F */ "iSCSI IP address removed"				},
	{ 0x3F, 0x14, /* D T L P W R   M A E B K   F */ "iSCSI IP address changed"				},
	{ 0x40, 0x00, /* D                           */ "RAM failure (should use 40 NN)"			},
	{ 0x40,  '*', /* D T L P W R O M A E B K V F */ "Diagnostic failure on component NN (80H-FFH)"		},
	{ 0x41, 0x00, /* D                           */ "Data path failure (should use 40 NN)"			},
	{ 0x42, 0x00, /* D                           */ "Power-on or self-test failure (should use 40 NN)"	},
	{ 0x43, 0x00, /* D T L P W R O M A E B K V F */ "Message error"						},
	{ 0x44, 0x00, /* D T L P W R O M A E B K V F */ "Internal target failure"				}, 
	{ 0x44, 0x71, /* D T                 B       */ "ATA device failed set features"			},
	{ 0x45, 0x00, /* D T L P W R O M A E B K V F */ "Select or reselect failure"				}, 
	{ 0x46, 0x00, /* D T L P W R O M     B K     */ "Unsuccessful soft reset"				},
	{ 0x47, 0x00, /* D T L P W R O M A E B K V F */ "SCSI parity error"					},
	{ 0x47, 0x01, /* D T L P W R O M A E B K V F */ "Data phase CRC error detected"				},
	{ 0x47, 0x02, /* D T L P W R O M A E B K V F */ "SCSI parity error detected during ST data phase"	},
	{ 0x47, 0x03, /* D T L P W R O M A E B K V F */ "Information unit iuCRC error detected"			},
	{ 0x47, 0x04, /* D T L P W R O M A E B K V F */ "Asynchronous information protection error detected"	},
	{ 0x47, 0x05, /* D T L P W R O M A E B K V F */ "Protocol service CRC error"				},
	{ 0x47, 0x06, /* D T           M A E B K V F */ "Phy test function in progress"				},
	{ 0x47, 0x7F, /* D T   P W R O M A E B K     */ "Some commands cleared by ISCSI protocol event"		},
	{ 0x48, 0x00, /* D T L P W R O M A E B K V F */ "Initiator detected error message received"		},
	{ 0x49, 0x00, /* D T L P W R O M A E B K V F */ "Invalid message error"					},
	{ 0x4A, 0x00, /* D T L P W R O M A E B K V F */ "Command phase error"					},
	{ 0x4B, 0x00, /* D T L P W R O M A E B K V F */ "Data phase error"					},
	{ 0x4B, 0x01, /* D T   P W R O M A E B K     */ "Invalid target port transfer tag received"		},
	{ 0x4B, 0x02, /* D T   P W R O M A E B K     */ "Too much write data"					},
	{ 0x4B, 0x03, /* D T   P W R O M A E B K     */ "ACK/NAK timeout"					},
	{ 0x4B, 0x04, /* D T   P W R O M A E B K     */ "NAK received"						},
	{ 0x4B, 0x05, /* D T   P W R O M A E B K     */ "Data offset error"					},
	{ 0x4B, 0x06, /* D T   P W R O M A E B K     */ "Initiator response timeout"				},
	{ 0x4B, 0x07, /* D T   P W R O M A E B K   F */ "Connection lost"					},
	{ 0x4C, 0x00, /* D T L P W R O M A E B K V F */ "Logical unit failed self-configuration"		},
	{ 0x4D,  '*', /* D T L P W R O M A E B K V F */ "Tagged overlapped commands (NN = task tag)"		},
	{ 0x4E, 0x00, /* D T L P W R O M A E B K V F */ "Overlapped commands attempted"				},
	{ 0x50, 0x00, /*   T                         */ "Write append error"					},
	{ 0x50, 0x01, /*   T                         */ "Write append position error"				},
	{ 0x50, 0x02, /*   T                         */ "Position error related to timing"			},
	{ 0x51, 0x00, /*   T       R O               */ "Erase failure"						},
	{ 0x51, 0x01, /*           R                 */ "Erase failure - incomplete erase operation detected"	},
	{ 0x52, 0x00, /*   T                         */ "Cartridge fault"					},
	{ 0x53, 0x00, /* D T L   W R O M     B K     */ "Media load or eject failed"				},
	{ 0x53, 0x01, /*   T                         */ "Unload tape failure"					},
	{ 0x53, 0x02, /* D T     W R O M     B K     */ "Medium removal prevented"				},
	{ 0x53, 0x03, /*               M             */ "Medium removal prevented by data transfer element"	},
	{ 0x53, 0x04, /*   T                         */ "Medium thread or unthread failure"			},
	{ 0x54, 0x00, /*       P                     */ "SCSI to host system interface failure"			},
	{ 0x55, 0x00, /*       P                     */ "System resource failure"				},
	{ 0x55, 0x01, /* D           O       B K     */ "System buffer full"					},
	{ 0x55, 0x02, /* D T L P W R O M A E   K     */ "Insufficient reservation resources"			},
	{ 0x55, 0x03, /* D T L P W R O M A E   K     */ "Insufficient resources"				},
	{ 0x55, 0x04, /* D T L P W R O M A E   K     */ "Insufficient registration resources"			},
	{ 0x55, 0x05, /* D T   P W R O M A E B K     */ "Insufficient access control resources"			},
	{ 0x55, 0x06, /* D T     W R O M     B       */ "Auxiliary memory out of space"				},
	{ 0x55, 0x07, /*                           F */ "Quota error"						},
	{ 0x55, 0x08, /*   T                         */ "Maximum number of supplemental decryption keys exceeded" },
	{ 0x55, 0x09, /*               M             */ "Medium auxiliary memory not accessible"		},
	{ 0x55, 0x0A, /*               M             */ "Data currently unavailable"				},
	{ 0x55, 0x0B, /* D T L P W R O M A E B K V F */ "Insufficient power for operation"			},
	{ 0x55, 0x0C, /* D T   P             B       */ "Insufficient resources to create rod"			},
	{ 0x55, 0x0D, /* D T   P             B       */ "Insufficient resources to create rod token"		},
	{ 0x57, 0x00, /*           R                 */ "Unable to recover table-of-contents"			},
	{ 0x58, 0x00, /*             O               */ "Generation does not exist"				},
	{ 0x59, 0x00, /*             O               */ "Updated block read"					},
	{ 0x5A, 0x00, /* D T L P W R O M     B K     */ "Operator request or state change input"		},
	{ 0x5A, 0x01, /* D T     W R O M     B K     */ "Operator medium removal request"			},
	{ 0x5A, 0x02, /* D T     W R O   A   B K     */ "Operator selected write protect"			},
	{ 0x5A, 0x03, /* D T     W R O   A   B K     */ "Operator selected write permit"			},
	{ 0x5B, 0x00, /* D T L P W R O M       K     */ "Log exception"						},
	{ 0x5B, 0x01, /* D T L P W R O M       K     */ "Threshold condition met"				},
	{ 0x5B, 0x02, /* D T L P W R O M       K     */ "Log counter at maximum"				},
	{ 0x5B, 0x03, /* D T L P W R O M       K     */ "Log list codes exhausted"				},
	{ 0x5C, 0x00, /* D           O               */ "Rpl status change"					},
	{ 0x5C, 0x01, /* D           O               */ "Spindles synchronized"					},
	{ 0x5C, 0x02, /* D           O               */ "Spindles not synchronized"				},
	{ 0x5D, 0x00, /* D T L P W R O M A E B K V F */ "Failure prediction threshold exceeded"			},
	{ 0x5D, 0x01, /*           R         B       */ "Media failure prediction threshold exceeded"		},
	{ 0x5D, 0x02, /*           R                 */ "Logical unit failure prediction threshold exceeded"	},
	{ 0x5D, 0x03, /*           R                 */ "Spare area exhaustion prediction threshold exceeded"	},
	{ 0x5D, 0x10, /* D                   B       */ "Hardware impending failure general hard drive failure"	},
	{ 0x5D, 0x11, /* D                   B       */ "Hardware impending failure drive error rate too high"	},
	{ 0x5D, 0x12, /* D                   B       */ "Hardware impending failure data error rate too high"	},
	{ 0x5D, 0x13, /* D                   B       */ "Hardware impending failure seek error rate too high"	},
	{ 0x5D, 0x14, /* D                   B       */ "Hardware impending failure too many block reassigns"	},
	{ 0x5D, 0x15, /* D                   B       */ "Hardware impending failure access times too high"	},
	{ 0x5D, 0x16, /* D                   B       */ "Hardware impending failure start unit times too high"	},
	{ 0x5D, 0x17, /* D                   B       */ "Hardware impending failure channel parametrics"	},
	{ 0x5D, 0x18, /* D                   B       */ "Hardware impending failure controller detected"	},
	{ 0x5D, 0x19, /* D                   B       */ "Hardware impending failure throughput performance"	},
	{ 0x5D, 0x1A, /* D                   B       */ "Hardware impending failure seek time performance"	},
	{ 0x5D, 0x1B, /* D                   B       */ "Hardware impending failure spin-up retry count"	},
	{ 0x5D, 0x1C, /* D                   B       */ "Hardware impending failure drive calibration retry count" },
	{ 0x5D, 0x20, /* D                   B       */ "Controller impending failure general hard drive failure"  },
	{ 0x5D, 0x21, /* D                   B       */ "Controller impending failure drive error rate too high"},
	{ 0x5D, 0x22, /* D                   B       */ "Controller impending failure data error rate too high"	},
	{ 0x5D, 0x23, /* D                   B       */ "Controller impending failure seek error rate too high"	},
	{ 0x5D, 0x24, /* D                   B       */ "Controller impending failure too many block reassigns"	},
	{ 0x5D, 0x25, /* D                   B       */ "Controller impending failure access times too high"	},
	{ 0x5D, 0x26, /* D                   B       */ "Controller impending failure start unit times too high"},
	{ 0x5D, 0x27, /* D                   B       */ "Controller impending failure channel parametrics"	},
	{ 0x5D, 0x28, /* D                   B       */ "Controller impending failure controller detected"	},
	{ 0x5D, 0x29, /* D                   B       */ "Controller impending failure throughput performance"	},
	{ 0x5D, 0x2A, /* D                   B       */ "Controller impending failure seek time performance"	},
	{ 0x5D, 0x2B, /* D                   B       */ "Controller impending failure spin-up retry count"	},
	{ 0x5D, 0x2C, /* D                   B       */ "Controller impending failure drive calibration retry count" },
	{ 0x5D, 0x30, /* D                   B       */ "Data channel impending failure general hard drive failure"  },
	{ 0x5D, 0x31, /* D                   B       */ "Data channel impending failure drive error rate too high"   },
	{ 0x5D, 0x32, /* D                   B       */ "Data channel impending failure data error rate too high"    },
	{ 0x5D, 0x33, /* D                   B       */ "Data channel impending failure seek error rate too high"    },
	{ 0x5D, 0x34, /* D                   B       */ "Data channel impending failure too many block reassigns"    },
	{ 0x5D, 0x35, /* D                   B       */ "Data channel impending failure access times too high"	     },
	{ 0x5D, 0x36, /* D                   B       */ "Data channel impending failure start unit times too high"   },
	{ 0x5D, 0x37, /* D                   B       */ "Data channel impending failure channel parametrics"	},
	{ 0x5D, 0x38, /* D                   B       */ "Data channel impending failure controller detected"	},
	{ 0x5D, 0x39, /* D                   B       */ "Data channel impending failure throughput performance"	},
	{ 0x5D, 0x3A, /* D                   B       */ "Data channel impending failure seek time performance"	},
	{ 0x5D, 0x3B, /* D                   B       */ "Data channel impending failure spin-up retry count"	},
	{ 0x5D, 0x3C, /* D                   B       */ "Data channel impending failure drive calibration retry count" },
	{ 0x5D, 0x40, /* D                   B       */ "Servo impending failure general hard drive failure"	},
	{ 0x5D, 0x41, /* D                   B       */ "Servo impending failure drive error rate too high"	},
	{ 0x5D, 0x42, /* D                   B       */ "Servo impending failure data error rate too high"	},
	{ 0x5D, 0x43, /* D                   B       */ "Servo impending failure seek error rate too high"	},
	{ 0x5D, 0x44, /* D                   B       */ "Servo impending failure too many block reassigns"	},
	{ 0x5D, 0x45, /* D                   B       */ "Servo impending failure access times too high"		},
	{ 0x5D, 0x46, /* D                   B       */ "Servo impending failure start unit times too high"	},
	{ 0x5D, 0x47, /* D                   B       */ "Servo impending failure channel parametrics"		},
	{ 0x5D, 0x48, /* D                   B       */ "Servo impending failure controller detected"		},
	{ 0x5D, 0x49, /* D                   B       */ "Servo impending failure throughput performance"	},
	{ 0x5D, 0x4A, /* D                   B       */ "Servo impending failure seek time performance"		},
	{ 0x5D, 0x4B, /* D                   B       */ "Servo impending failure spin-up retry count"		},
	{ 0x5D, 0x4C, /* D                   B       */ "Servo impending failure drive calibration retry count"	},
	{ 0x5D, 0x50, /* D                   B       */ "Spindle impending failure general hard drive failure"	},
	{ 0x5D, 0x51, /* D                   B       */ "Spindle impending failure drive error rate too high"	},
	{ 0x5D, 0x52, /* D                   B       */ "Spindle impending failure data error rate too high"	},
	{ 0x5D, 0x53, /* D                   B       */ "Spindle impending failure seek error rate too high"	},
	{ 0x5D, 0x54, /* D                   B       */ "Spindle impending failure too many block reassigns"	},
	{ 0x5D, 0x55, /* D                   B       */ "Spindle impending failure access times too high"	},
	{ 0x5D, 0x56, /* D                   B       */ "Spindle impending failure start unit times too high"	},
	{ 0x5D, 0x57, /* D                   B       */ "Spindle impending failure channel parametrics"		},
	{ 0x5D, 0x58, /* D                   B       */ "Spindle impending failure controller detected"		},
	{ 0x5D, 0x59, /* D                   B       */ "Spindle impending failure throughput performance"	},
	{ 0x5D, 0x5A, /* D                   B       */ "Spindle impending failure seek time performance"	},
	{ 0x5D, 0x5B, /* D                   B       */ "Spindle impending failure spin-up retry count"		},
	{ 0x5D, 0x5C, /* D                   B       */ "Spindle impending failure drive calibration retry count" },
	{ 0x5D, 0x60, /* D                   B       */ "Firmware impending failure general hard drive failure"	},
	{ 0x5D, 0x61, /* D                   B       */ "Firmware impending failure drive error rate too high"	},
	{ 0x5D, 0x62, /* D                   B       */ "Firmware impending failure data error rate too high"	},
	{ 0x5D, 0x63, /* D                   B       */ "Firmware impending failure seek error rate too high"	},
	{ 0x5D, 0x64, /* D                   B       */ "Firmware impending failure too many block reassigns"	},
	{ 0x5D, 0x65, /* D                   B       */ "Firmware impending failure access times too high"	},
	{ 0x5D, 0x66, /* D                   B       */ "Firmware impending failure start unit times too high"	},
	{ 0x5D, 0x67, /* D                   B       */ "Firmware impending failure channel parametrics"	},
	{ 0x5D, 0x68, /* D                   B       */ "Firmware impending failure controller detected"	},
	{ 0x5D, 0x69, /* D                   B       */ "Firmware impending failure throughput performance"	},
	{ 0x5D, 0x6A, /* D                   B       */ "Firmware impending failure seek time performance"	},
	{ 0x5D, 0x6B, /* D                   B       */ "Firmware impending failure spin-up retry count"	},
	{ 0x5D, 0x6C, /* D                   B       */ "Firmware impending failure drive calibration retry count" },
	{ 0x5D, 0xFF, /* D T L P W R O M A E B K V F */ "Failure prediction threshold exceeded (false)"		},
	{ 0x5E, 0x00, /* D T L P W R O   A     K     */ "Low power condition on"				},
	{ 0x5E, 0x01, /* D T L P W R O   A     K     */ "Idle condition activated by timer"			},
	{ 0x5E, 0x02, /* D T L P W R O   A     K     */ "Standby condition activated by timer"			},
	{ 0x5E, 0x03, /* D T L P W R O   A     K     */ "Idle condition activated by command"			},
	{ 0x5E, 0x04, /* D T L P W R O   A     K     */ "Standby condition activated by command"		},
	{ 0x5E, 0x05, /* D T L P W R O   A     K     */ "Idle_B condition activated by timer"			},
	{ 0x5E, 0x06, /* D T L P W R O   A     K     */ "Idle_B condition activated by command"			},
	{ 0x5E, 0x07, /* D T L P W R O   A     K     */ "Idle_C condition activated by timer"			},
	{ 0x5E, 0x08, /* D T L P W R O   A     K     */ "Idle_C condition activated by command"			},
	{ 0x5E, 0x09, /* D T L P W R O   A     K     */ "Standby_Y condition activated by timer"		},
	{ 0x5E, 0x0A, /* D T L P W R O   A     K     */ "Standby_Y condition activated by command"		},
	{ 0x5E, 0x41, /*                     B       */ "Power state change to active"				},
	{ 0x5E, 0x42, /*                     B       */ "Power state change to idle"				},
	{ 0x5E, 0x43, /*                     B       */ "Power state change to standby"				},
	{ 0x5E, 0x45, /*                     B       */ "Power state change to sleep"				},
	{ 0x5E, 0x47, /*                     B K     */ "Power state change to device control"			},
	{ 0x60, 0x00, /*                             */ "Lamp failure"						},
	{ 0x61, 0x00, /*                             */ "Video acquisition error"				},
	{ 0x61, 0x01, /*                             */ "Unable to acquire video"				},
	{ 0x61, 0x02, /*                             */ "Out of focus"						},
	{ 0x62, 0x00, /*                             */ "Scan head positioning error"				},
	{ 0x63, 0x00, /*           R                 */ "End of user area encountered on this track"		},
	{ 0x63, 0x01, /*           R                 */ "Packet does not fit in available space"		},
	{ 0x64, 0x00, /*           R                 */ "Illegal mode for this track"				},
	{ 0x64, 0x01, /*           R                 */ "Invalid packet size"					},
	{ 0x65, 0x00, /* D T L P W R O M A E B K V F */ "Voltage fault"						},
	{ 0x66, 0x00, /*                             */ "Automatic document feeder cover up"			},
	{ 0x66, 0x01, /*                             */ "Automatic document feeder lift up"			},
	{ 0x66, 0x02, /*                             */ "Document jam in automatic document feeder"		},
	{ 0x66, 0x03, /*                             */ "Document miss feed automatic in document feeder"	},
	{ 0x67, 0x00, /*                 A           */ "Configuration failure"					},
	{ 0x67, 0x01, /*                 A           */ "Configuration of incapable logical units failed"	},
	{ 0x67, 0x02, /*                 A           */ "Add logical unit failed"				},
	{ 0x67, 0x03, /*                 A           */ "Modification of logical unit failed"			},
	{ 0x67, 0x04, /*                 A           */ "Exchange of logical unit failed"			},
	{ 0x67, 0x05, /*                 A           */ "Remove of logical unit failed"				},
	{ 0x67, 0x06, /*                 A           */ "Attachment of logical unit failed"			},
	{ 0x67, 0x07, /*                 A           */ "Creation of logical unit failed"			},
	{ 0x67, 0x08, /*                 A           */ "Assign failure occurred"				},
	{ 0x67, 0x09, /*                 A           */ "Multiply assigned logical unit"			},
	{ 0x67, 0x0A, /* D T L P W R O M A E B K V F */ "Set target port groups command failed"			},
	{ 0x67, 0x0B, /* D T                 B       */ "ATA device feature not enabled"			},
	{ 0x68, 0x00, /*                 A           */ "Logical unit not configured"				},
	{ 0x69, 0x00, /*                 A           */ "Data loss on logical unit"				},
	{ 0x69, 0x01, /*                 A           */ "Multiple logical unit failures"			},
	{ 0x69, 0x02, /*                 A           */ "Parity/data mismatch"					},
	{ 0x6A, 0x00, /*                 A           */ "Informational, refer to log"				},
	{ 0x6B, 0x00, /*                 A           */ "State change has occurred"				},
	{ 0x6B, 0x01, /*                 A           */ "Redundancy level got better"				},
	{ 0x6B, 0x02, /*                 A           */ "Redundancy level got worse"				},
	{ 0x6C, 0x00, /*                 A           */ "Rebuild failure occurred"				},
	{ 0x6D, 0x00, /*                 A           */ "Recalculate failure occurred"				},
	{ 0x6E, 0x00, /*                 A           */ "Command to logical unit failed"			},
	{ 0x6F, 0x00, /*           R                 */ "Copy protection key exchange failure - authentication failure" },
	{ 0x6F, 0x01, /*           R                 */ "Copy protection key exchange failure - key not present" },
	{ 0x6F, 0x02, /*           R                 */ "Copy protection key exchange failure - key not established" },
	{ 0x6F, 0x03, /*           R                 */ "Read of scrambled sector without authentication"	},
	{ 0x6F, 0x04, /*           R                 */ "Media region code is mismatched to logical unit region" },
	{ 0x6F, 0x05, /*           R                 */ "Drive region must be permanent/region reset count error" },
	{ 0x6F, 0x06, /*           R                 */ "Insufficient block count for binding nonce recording"	},
	{ 0x6F, 0x07, /*           R                 */ "Conflict in binding nonce recording"			},
	{ 0x70,  '*', /*   T                         */ "Decompression exception short algorithm id of NN"	},
	{ 0x71, 0x00, /*   T                         */ "Decompression exception long algorithm id"		},
	{ 0x72, 0x00, /*           R                 */ "Session fixation error"				},
	{ 0x72, 0x01, /*           R                 */ "Session fixation error writing lead-in"		},
	{ 0x72, 0x02, /*           R                 */ "Session fixation error writing lead-out"		},
	{ 0x72, 0x03, /*           R                 */ "Session fixation error - incomplete track in session"	},
	{ 0x72, 0x04, /*           R                 */ "Empty or partially written reserved track"		},
	{ 0x72, 0x05, /*           R                 */ "No more track reservations allowed"			},
	{ 0x72, 0x06, /*           R                 */ "RMZ extension is not allowed"				},
	{ 0x72, 0x07, /*           R                 */ "No more test zone extensions are allowed"		},
	{ 0x73, 0x00, /*           R                 */ "CD control error"					},
	{ 0x73, 0x01, /*           R                 */ "Power calibration area almost full"			},
	{ 0x73, 0x02, /*           R                 */ "Power calibration area is full"			},
	{ 0x73, 0x03, /*           R                 */ "Power calibration area error"				},
	{ 0x73, 0x04, /*           R                 */ "Program memory area update failure"			},
	{ 0x73, 0x05, /*           R                 */ "Program memory area is full"				},
	{ 0x73, 0x06, /*           R                 */ "RMA/PMA is almost full"				},
	{ 0x73, 0x10, /*           R                 */ "Current power calibration area almost full"		},
	{ 0x73, 0x11, /*           R                 */ "Current power calibration area is full"		},
	{ 0x73, 0x17, /*           R                 */ "RDZ is full"						},
	{ 0x74, 0x00, /*   T                         */ "Security error"					},
	{ 0x74, 0x01, /*   T                         */ "Unable to decrypt data"				},
	{ 0x74, 0x02, /*   T                         */ "Unencrypted data encountered while decrypting"		},
	{ 0x74, 0x03, /*   T                         */ "Incorrect data encryption key"				},
	{ 0x74, 0x04, /*   T                         */ "Cryptographic integrity validation failed"		},
	{ 0x74, 0x05, /*   T                         */ "Error decrypting data"					},
	{ 0x74, 0x06, /*   T                         */ "Unknown signature verification key"			},
	{ 0x74, 0x07, /*   T                         */ "Encryption parameters not useable"			},
	{ 0x74, 0x08, /* D T       R   M    E    V F */ "Digital signature validation failure"			},
	{ 0x74, 0x09, /*   T                         */ "Encryption mode mismatch on read"			},
	{ 0x74, 0x0A, /*   T                         */ "Encrypted block not raw read enabled"			},
	{ 0x74, 0x0B, /*   T                         */ "Incorrect encryption parameters"			},
	{ 0x74, 0x0C, /* D T       R   M A E B K V   */ "Unable to decrypt parameter list"			},
	{ 0x74, 0x0D, /*   T                         */ "Encryption algorithm disabled"				},
	{ 0x74, 0x10, /* D T       R   M A E B K V   */ "SA creation parameter value invalid"			},
	{ 0x74, 0x11, /* D T       R   M A E B K V   */ "SA creation parameter value rejected"			},
	{ 0x74, 0x12, /* D T       R   M A E B K V   */ "Invalid SA usage"					},
	{ 0x74, 0x21, /*   T                         */ "Data encryption configuration prevented"		},
	{ 0x74, 0x30, /* D T       R   M A E B K V   */ "SA creation parameter not supported"			},
	{ 0x74, 0x40, /* D T       R   M A E B K V   */ "Authentication failed"					},
	{ 0x74, 0x61, /*                         V   */ "External data encryption key manager access error"	},
	{ 0x74, 0x62, /*                         V   */ "External data encryption key manager error"		},
	{ 0x74, 0x63, /*                         V   */ "External data encryption key not found"		},
	{ 0x74, 0x64, /*                         V   */ "External data encryption request not authorized"	},
	{ 0x74, 0x6E, /*   T                         */ "External data encryption control timeout"		},
	{ 0x74, 0x6F, /*   T                         */ "External data encryption control error"		},
	{ 0x74, 0x71, /* D T       R   M   E     V   */ "Logical unit access not authorized"			},
	{ 0x74, 0x79, /* D                           */ "Security conflict in translated device"		}
};
int SenseCodeEntrys = sizeof(SenseCodeTable) / sizeof(struct sense_entry);

/*
 * Function to Find Additional Sense Code/Qualifier Message.
 */
char *
ScsiAscqMsg(unsigned char asc, unsigned char asq)
{
    struct sense_entry *se;
    int entrys = 0;

    for (se = SenseCodeTable; entrys < SenseCodeEntrys; se++, entrys++) {
	/*
	 * A sense qualifier of '*' (0x2A) is used for wildcarding.
	 */
	if ( (se->sense_code == asc) &&
	     ((se->sense_qualifier == asq) ||
	      (se->sense_qualifier == '*')) ) {
	    return(se->sense_message);
	}
    }
    return((char *) 0);
}

/* ======================================================================== */

char *
SenseCodeMsg(unsigned char error_code)
{
    char *msgp;
    if ( (error_code == ECV_CURRENT_FIXED) ||
	 (error_code == ECV_CURRENT_DESCRIPTOR) ) {
	msgp = "Current Error";
    } else if ( (error_code == ECV_DEFERRED_FIXED) ||
		(error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	msgp = "Deferred Error";
    } else if (error_code == ECV_VENDOR_SPECIFIC) {
	msgp = "Vendor Specific";
    } else {
	msgp = "NO CODE";
    }
    return (msgp);
}

void
GetSenseErrors(scsi_sense_t *ssp, unsigned char *sense_key,
	       unsigned char *asc, unsigned char *asq)
{
    if ( (ssp->error_code == ECV_CURRENT_FIXED) ||
	 (ssp->error_code == ECV_DEFERRED_FIXED) ) {
	*sense_key = ssp->sense_key;
	*asc = ssp->asc;
	*asq = ssp->asq;
    } else if ( (ssp->error_code == ECV_CURRENT_DESCRIPTOR) ||
		(ssp->error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)ssp;
	*sense_key = ssdp->sense_key;
	*asc = ssdp->asc;
	*asq = ssdp->asq;
    } else {
	*sense_key = 0;
	*asc = 0;
	*asq = 0;
    }
    return;
}

#if 0
/*
 * Note: Short term until sense descriptors are added. 
 * Also Note: This mapping mangles the original sense data! 
 */
void
MapSenseDescriptorToFixed(scsi_sense_t *ssp)
{
    if ( (ssp->error_code == ECV_CURRENT_DESCRIPTOR) ||
	 (ssp->error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	int sense_length = (int)ssp->addl_sense_len + 8;
	scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)ssp;
	unsigned char *bp = (unsigned char *)ssdp;
	memcpy(ssdp, ssp, sizeof(*ssdp));
	ssp->sense_key = ssdp->sense_key;	/* byte 1 to byte 2 */
	ssp->obsolete = 0;			/* clear byte 1 */
	ssp->asc = ssdp->asc;			/* byte 2 to byte 12 */
	ssp->asq = ssdp->asq;			/* byte 3 to byte 14 */
	//ssp->addl_sense_len = ssdp->addl_sense_len; /* same byte 7 */
	bp += 8;				/* skip to descriptors */
	sense_length -= 8;
	while (sense_length > 0) {
	    sense_data_desc_header_t *sdhp = (sense_data_desc_header_t *)bp;
	    int descriptor_length = sdhp->additional_length + sizeof(*sdhp);
	    switch (sdhp->descriptor_type) {
	        case INFORMATION_DESC_TYPE: {
		    information_desc_type_t *idtp = (information_desc_type_t *)bp;
		    if (idtp->info_valid) {
			ssp->info_valid = idtp->info_valid;
			memcpy(ssp->info_bytes, &idtp->information[4], sizeof(ssp->info_bytes));
		    }
		    break;
		}
		case COMMAND_SPECIFIC_DESC_TYPE: {
		    command_specific_desc_type_t *csp = (command_specific_desc_type_t *)bp;
		    memcpy(ssp->cmd_spec_info, &csp->information[4], sizeof(ssp->cmd_spec_info));
		    break;
		}
	    }
	    /* Adjust the sense length and point to next descriptor. */
	    sense_length -= descriptor_length;
	    bp += descriptor_length;
	}
    }
    return;
}
#endif /* 0 */

/*
 * Function to Dump Sense Data (fixed format).
 */
void
DumpSenseData(scsi_generic_t *sgp, scsi_sense_t *ssp)
{
    int sense_length = (int) ssp->addl_sense_len + 8;
    unsigned int cmd_spec_value, info_value;

    if ( (ssp->error_code == ECV_CURRENT_DESCRIPTOR) ||
	 (ssp->error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	DumpSenseDataDescriptor(sgp, (scsi_sense_desc_t *)ssp);
	return;
    }
    Lprintf(sgp->opaque, "\nRequest Sense Data: (sense length %d bytes)\n\n", sense_length);
    PrintHex(sgp->opaque, "Error Code", ssp->error_code, DNL);
    Lprintf(sgp->opaque, " = %s\n", SenseCodeMsg(ssp->error_code));
    PrintAscii(sgp->opaque, "Information Field Valid", (ssp->info_valid) ? "Yes" : "No", PNL);
    if (ssp->obsolete) {
	PrintHex(sgp->opaque, "Obsolete", ssp->obsolete, PNL);
    }
    PrintHex(sgp->opaque, "Sense Key", ssp->sense_key, DNL);
    Lprintf(sgp->opaque, " = %s\n", SenseKeyMsg(ssp->sense_key));
    info_value = (unsigned int)StoH(ssp->info_bytes);
    PrintDecHex(sgp->opaque, "Information Field", info_value, PNL);
    PrintDecHex(sgp->opaque, "Additional Sense Length", ssp->addl_sense_len, PNL);
    if ( (sense_length -= 8) > 0) {
	cmd_spec_value = (unsigned int)StoH(ssp->cmd_spec_info);
	PrintDecHex(sgp->opaque, "Command Specific Information", cmd_spec_value, PNL);
	sense_length -= 4;
    }
    if (sense_length > 0) {
	char *ascq_msg = ScsiAscqMsg(ssp->asc, ssp->asq);
	PrintAscii(sgp->opaque, "Additional Sense Code/Qualifier", "", DNL);
	Lprintf(sgp->opaque, "(%#x, %#x)", ssp->asc, ssp->asq);
	if (ascq_msg) {
	    Lprintf(sgp->opaque, " - %s\n", ascq_msg);
	} else {
	    Lprintf(sgp->opaque, "\n");
	}
	sense_length -=2;
    }
    if (sense_length > 0) {
	PrintHex(sgp->opaque, "Field Replaceable Unit Code", ssp->fru_code, PNL);
	sense_length--;
    }
    if (sense_length > 0) {
	int i;
	PrintAscii(sgp->opaque, "Sense Key Specific Bytes", "", DNL);
	for (i = 0; i < (int)sizeof(ssp->sense_key_specific); i++) {
	    Lprintf(sgp->opaque, "%02x ", ssp->sense_key_specific[i]);
	    if (--sense_length == 0) break;
	}
	Lprintf(sgp->opaque, "\n");
	if (ssp->sense_key == SKV_COPY_ABORTED) {
	    unsigned short field_ptr;
	    scsi_sense_copy_aborted_t *sksp;

	    sksp = (scsi_sense_copy_aborted_t *)ssp->sense_key_specific;
	    field_ptr = ((sksp->field_ptr1 << 8) + sksp->field_ptr0);
	    PrintHex(sgp->opaque, "Bit Pointer to Field in Error", sksp->bit_pointer,
				    (sksp->bit_pointer) ? DNL : PNL);
	    if (sksp->bpv) {
		Lprintf(sgp->opaque, " (valid, bit %u)\n", (sksp->bit_pointer + 1));
	    }
	    PrintAscii(sgp->opaque, "Bit Pointer Valid", (sksp->bpv) ? "Yes" : "No", PNL);
	    PrintDec(sgp->opaque, "Segment Descriptor", sksp->sd, DNL);
	    Lprintf(sgp->opaque, " (%s)\n", (sksp->sd) ? "error is in segment descriptor"
					   : "error is in parameter list");
	    PrintHex(sgp->opaque, "Byte Pointer to Field in Error", field_ptr,
					    (field_ptr) ? DNL : PNL);
	    if (field_ptr) {
		Lprintf(sgp->opaque, " (byte %u)\n", (field_ptr + 1)); /* zero-based */
	    }
	}

	if (ssp->sense_key == SKV_ILLEGAL_REQUEST) {
	    unsigned short field_ptr;
	    scsi_sense_illegal_request_t *sksp;
	    
	    sksp = (scsi_sense_illegal_request_t *)ssp->sense_key_specific;
	    field_ptr = ((sksp->field_ptr1 << 8) + sksp->field_ptr0);
	    PrintHex(sgp->opaque, "Bit Pointer to Field in Error", sksp->bit_pointer,
				    (sksp->bit_pointer) ? DNL : PNL);
	    if (sksp->bpv) {
		Lprintf(sgp->opaque, " (valid, bit %u)\n", (sksp->bit_pointer + 1));
	    }
	    PrintAscii(sgp->opaque, "Bit Pointer Valid", (sksp->bpv) ? "Yes" : "No", PNL);
	    PrintHex(sgp->opaque, "Error Field Command/Data (C/D)", sksp->c_or_d, DNL);
	    Lprintf(sgp->opaque, " (%s)\n", (sksp->c_or_d) ? "Illegal parameter in CDB bytes"
					      : "Illegal parameter in Data sent");
	    PrintHex(sgp->opaque, "Byte Pointer to Field in Error", field_ptr,
					    (field_ptr) ? DNL : PNL);
	    if (field_ptr) {
		Lprintf(sgp->opaque, " (byte %u)\n", (field_ptr + 1)); /* zero-based */
	    }
	}
    }
    /*
     * Additional sense bytes (if any);
     */
    if (sense_length > 0) {
	unsigned char *asbp = ssp->addl_sense;
	char *buf = Malloc(sgp->opaque, ((sense_length * 3) + 1) );
	char *bp = buf;
	if (buf == NULL) return;
	do {
	    bp += sprintf(bp, "%02x ", *asbp++);
	} while (--sense_length);
	PrintAscii(sgp->opaque, "Additional Sense Bytes", buf, PNL);
	free(buf);
    }
    DumpCdbData(sgp);
    Lprintf(sgp->opaque, "\n");
    Lflush(sgp->opaque);
    return;
}

/*
 * Function to Dump Sense Data (descriptor format).
 */
void
DumpSenseDataDescriptor(scsi_generic_t *sgp, scsi_sense_desc_t *ssdp)
{
    int sense_length = (int) ssdp->addl_sense_len + 8;
    char *ascq_msg = ScsiAscqMsg(ssdp->asc, ssdp->asq);

    Lprintf(sgp->opaque, "\nRequest Sense Data: (sense length %d bytes)\n\n", sense_length);
    PrintHex(sgp->opaque, "Error Code", ssdp->error_code, DNL);
    Lprintf(sgp->opaque, " = %s\n", SenseCodeMsg(ssdp->error_code));
    PrintHex(sgp->opaque, "Sense Key", ssdp->sense_key, DNL);
    Lprintf(sgp->opaque, " = %s\n", SenseKeyMsg(ssdp->sense_key));
    PrintAscii(sgp->opaque, "Additional Sense Code/Qualifier", "", DNL);
    Lprintf(sgp->opaque, "(%#x, %#x)", ssdp->asc, ssdp->asq);
    if (ascq_msg) {
	Lprintf(sgp->opaque, " - %s\n", ascq_msg);
    } else {
	Lprintf(sgp->opaque, "\n");
    }
    PrintDecHex(sgp->opaque, "Additional Sense Length", ssdp->addl_sense_len, PNL);
    if ( (sense_length -= 8) > 0) {
	DumpSenseDescriptors(sgp, ssdp, sense_length);
    }
    DumpCdbData(sgp);
    Lprintf(sgp->opaque, "\n");
    Lflush(sgp->opaque);
    return;
}

void
DumpSenseDescriptors(scsi_generic_t *sgp, scsi_sense_desc_t *ssdp, int sense_length)
{
    dinfo_t *dip = sgp->opaque;
    unsigned char *bp = (unsigned char *)ssdp + 8;

    while (sense_length > 0) {
	sense_data_desc_header_t *sdhp = (sense_data_desc_header_t *)bp;
	int descriptor_length = sdhp->additional_length + sizeof(*sdhp);

	switch (sdhp->descriptor_type) {

	    case INFORMATION_DESC_TYPE:
		DumpInformationSense(sgp, (information_desc_type_t *)bp);
		break;

	    case COMMAND_SPECIFIC_DESC_TYPE:
		DumpCommandSpecificSense(sgp, (command_specific_desc_type_t *)bp);
		break;

	    case SENSE_KEY_SPECIFIC_DESC_TYPE:
		DumpSenseKeySpecificSense(sgp, (sense_key_specific_desc_type_t *)bp);
		break;

	    case FIELD_REPLACEABLE_UNIT_DESC_TYPE:
		DumpFieldReplaceableUnitSense(sgp, (fru_desc_type_t *)bp);
		break;

	    case BLOCK_COMMAND_DESC_TYPE:
		DumpBlockCommandSense(sgp, (block_command_desc_type_t *)bp);
		break;

#if defined(HGST)
	    case HGST_UNIT_ERROR_CODE_DESC_TYPE:
		if (dip->di_vendor_id && EQ(dip->di_vendor_id, "HGST") ) {
		    DumpUnitErrorSense(sgp, (hgst_unit_error_desc_type_t *)bp);
		}
		break;

	    case HGST_PHYSICAL_ERROR_RECORD_DESC_TYPE:
		if (dip->di_vendor_id && EQ(dip->di_vendor_id, "HGST") ) {
		    DumpPhysicalRecordErrorSense(sgp, (hgst_physical_error_record_desc_type_t *)bp);
		}
		break;

#endif /* defined(HGST) */
	    default:
		Wprintf(sgp->opaque, "Unknown descriptor type %#x\n", sdhp->descriptor_type);
		break;
	}
	/* Adjust the sense length and point to next descriptor. */
	sense_length -= descriptor_length;
	bp += descriptor_length;
    }
    return;
}

void
DumpInformationSense(scsi_generic_t *sgp, information_desc_type_t *idtp)
{
    uint64_t info_value;
    if (idtp->info_valid) {
	info_value = (large_t)StoH(idtp->information);
	PrintLongDecHex(sgp->opaque, "Information Field", info_value, PNL);
    }
    return;
}

void
DumpCommandSpecificSense(scsi_generic_t *sgp, command_specific_desc_type_t *csp)
{
    uint64_t cmd_spec_value;
    cmd_spec_value = (large_t)StoH(csp->information);
    PrintLongDecHex(sgp->opaque, "Command Specific Information", cmd_spec_value, PNL);
    return;
}

void
DumpSenseKeySpecificSense(scsi_generic_t *sgp, sense_key_specific_desc_type_t *sksp)
{
    scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)sgp->sense_data;
    /* Avoid issue of taking address of a bit field! (ugly) */
    unsigned char *bp = (unsigned char *)&sksp->reserved_byte3;
    int i;

    bp++; /* Point to sense specific data. */
    PrintHex(sgp->opaque, "Sense Key Valid", sksp->sksv, PNL);
    PrintDecHex(sgp->opaque, "Sense Key Specific Bits", sksp->sense_key_bits, PNL); 
    PrintAscii(sgp->opaque, "Sense Key Bytes", "", DNL); 
    for (i = 0; i < (int)sizeof(sksp->sense_key_bytes); i++) {
	Lprintf(sgp->opaque, "%02x ", sksp->sense_key_bytes[i]);
    }
    Lprintf(sgp->opaque, "\n");
    if (ssdp->sense_key == SKV_ILLEGAL_REQUEST) {
	DumpIllegalRequestSense(sgp, (scsi_sense_illegal_request_t *)bp);
    } else if ( (ssdp->sense_key == SKV_RECOVERED) ||
		(ssdp->sense_key == SKV_MEDIUM_ERROR) ||
		(ssdp->sense_key == SKV_HARDWARE_ERROR) ) {
	DumpMediaErrorSense(sgp, (scsi_media_error_sense_t *)bp);
    }
    return;
}

void
DumpIllegalRequestSense(scsi_generic_t *sgp, scsi_sense_illegal_request_t *sirp)
{
    unsigned short field_ptr;

    field_ptr = ((sirp->field_ptr1 << 8) + sirp->field_ptr0);
    PrintHex(sgp->opaque, "Bit Pointer to Field in Error", sirp->bit_pointer,
				    (sirp->bit_pointer) ? DNL : PNL);
    if (sirp->bpv) {
	Lprintf(sgp->opaque, " (valid, bit %u)\n", (sirp->bit_pointer + 1));
    }
    PrintAscii(sgp->opaque, "Bit Pointer Valid", (sirp->bpv) ? "Yes" : "No", PNL);
    PrintHex(sgp->opaque, "Error Field Command/Data (C/D)", sirp->c_or_d, DNL);
    Lprintf(sgp->opaque, " (%s)\n", (sirp->c_or_d) ? "Illegal parameter in CDB bytes"
	                                           : "Illegal parameter in Data sent");
    PrintHex(sgp->opaque, "Byte Pointer to Field in Error", field_ptr,
					    (field_ptr) ? DNL : PNL);
    if (field_ptr) {
	Lprintf(sgp->opaque, " (byte %u)\n", (field_ptr + 1)); /* zero-based */
    }
    return;
}

char *error_recovery_types[] = {
    "Read",				/* 0x00 */
    "Verify",				/* 0x01 */
    "Write",				/* 0x02 */
    "Seek",				/* 0x03 */
    "Read Sync Byte branch",		/* 0x04 */
    "Read, Thermal Asperity branch",	/* 0x05 */
    "Read, Minus Mod branch",		/* 0x06 */
    "Verify, Sync Byte branch",		/* 0x07 */
    "Verify, Thermal Asperity branch",	/* 0x08 */
    "Verify, Minus Mod branch"		/* 0x09 */
};
static int num_error_recovery_types = sizeof(error_recovery_types) / sizeof(char *);

void
DumpMediaErrorSense(scsi_generic_t *sgp, scsi_media_error_sense_t *mep)
{
    PrintHex(sgp->opaque, "Error Recovery Type", mep->erp_type, DNL);
    if (mep->erp_type < num_error_recovery_types) {
	Lprintf(sgp->opaque, " = %s\n", error_recovery_types[mep->erp_type]);
    } else {
	Lprintf(sgp->opaque, "\n");
    }
    PrintDecimal(sgp->opaque, "Secondary Recovery Step", mep->secondary_step, PNL);
    PrintDecimal(sgp->opaque, "Actual Retry Count", mep->actual_retry_count, PNL);
    return;
}

void
DumpFieldReplaceableUnitSense(scsi_generic_t *sgp, fru_desc_type_t *frup)
{
    PrintHex(sgp->opaque, "Field Replaceable Unit Code", frup->fru_code, PNL);
    return;
}

void
DumpBlockCommandSense(scsi_generic_t *sgp, block_command_desc_type_t *bcp)
{
    PrintHex(sgp->opaque, "ili bit", bcp->ili, PNL);
    return;
}

#if defined(HGST)

void
DumpUnitErrorSense(scsi_generic_t *sgp, hgst_unit_error_desc_type_t *uep)
{
    uint16_t unit_error_code = (uint16_t)StoH(uep->unit_error_code);
    /*
     * This field gives detailed information about the error. It contains a
     * unique code which describes where the error was detected and which piece
     * of hardware or microcode detected the error depending on current operation. 
     *  
     * Note: I don't know where these error codes are documented! (Robin)
     */
    PrintHexDec(sgp->opaque, "Unit Error Code", unit_error_code, PNL); 
    return;
}

void
DumpPhysicalRecordErrorSense(scsi_generic_t *sgp, hgst_physical_error_record_desc_type_t *pep)
{
    scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)sgp->sense_data;
    int i;

    PrintAscii(sgp->opaque, "Physical Record Error", "", DNL);
    for (i = 0; i < (int)sizeof(pep->physical_error_record); i++) {
	Lprintf(sgp->opaque, "%02x ", pep->physical_error_record[i]);
    }
    Lprintf(sgp->opaque, "\n");
    if ( (ssdp->sense_key == SKV_RECOVERED) ||
	 (ssdp->sense_key == SKV_MEDIUM_ERROR) ||
	 (ssdp->sense_key == SKV_HARDWARE_ERROR) ) {
	hgst_physical_error_record_t *perp = (hgst_physical_error_record_t *)&pep->physical_error_record;
	uint32_t cylinder = (uint32_t)StoH(perp->cylinder_number);
	uint16_t sector = (uint16_t)StoH(perp->sector_number);
	PrintDecimal(sgp->opaque, "Cylinder Number", cylinder, PNL);
	PrintDecimal(sgp->opaque, "Head Number", perp->head_number, PNL);
	PrintDecimal(sgp->opaque, "Sector Number", sector, PNL);
    }
    return;
}

#endif /* defined(HGST) */

void
DumpCdbData(scsi_generic_t *sgp)
{
    unsigned int dump_length;
    
    if (!sgp || !sgp->data_buffer || !sgp->data_length || !sgp->data_dump_limit) {
	return;
    }
    dump_length = min(sgp->data_length, sgp->data_dump_limit);
    Lprintf(sgp->opaque, "\nCDB Data %s: (%u bytes)\n\n", 
	    (sgp->data_dir == scsi_data_read) ? "Received" : "Sent",
	    dump_length);
    DumpFieldsOffset(sgp->opaque, (uint8_t *)sgp->data_buffer, dump_length);
    return;
}
