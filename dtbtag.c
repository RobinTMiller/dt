/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2021			    *
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
 * Module:	dtbtag.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	This file contains functions required for block tags (btag).
 *
 * Modification History:
 * 
 * March 31st, 2020 by Robin T. Miller
 *      Add function to display block tag verify flags that are set.
 * 
 * January 8th, 2020 by Robin T. Miller
 *      When verifying the received CRC, also verify against the expected CRC
 * when using read-after-write, since we may read stale (yet valid) btag data,
 * but the expected btag CRC will be incorrect! Yikes, how did I miss this?
 *      When reporting or verifying btags with read-after-write, do *not*
 * overwrite the expected btag CRC, which should be correct from write.
 * 
 * January 6th, 2020 by Robin T. Miller
 *      For file systems, if a serial number exists, verify the serial number.
 *      Previously this was cleared for files, so we did not verify serial #
 * and mark as incorrect if wrong. The side-by-side compare showed corruption.
 *      Note: We still detected the corruption by other fields or CRC-32.
 * 
 * December 24th, 2019 by Robin T. Miller
 *      Display file system physical LBA's when this is supported.
 * 
 * October 6th, 2019 by Robin T. Miller
 *      With variable I/O modes, when switching from random to sequential mode,
 * use the initial btag verify flags, to prevent setting flags not intended.
 * Previously the random disable btag flags were set, enabling several flags
 * which lead to a false corruption. We now honor user set verify flags.
 * Note: This prevents false miscompare on ENOSPC or short record writes.
 * 
 * May 28th, 2019 by Robin T. Miller
 *      With I/O lock and random percentages, disable btag flags that are not
 * safe to verify since they may get overwritten, which leads to false corruptions.
 * 
 * December 29th, 2017 by Robin T. Miller
 *      When the I/O lock flag is enabled, don't compare the thread number.
 *	The I/O lock indicates multiple threads are accessing the same file.
 *
 * May 13th, 2015 by Robin T Miller
 * 	Initial creation.
 * 
 * September 17th, 2015 by Robin T. Miller
 * 	Add support for btag extention for tracking previous writes.
 */
#include "dt.h"
#include <ctype.h>

/*
 * Forward References:
 */
void show_btag_verify_flags(dinfo_t *dip);
char *copy_string(char *in, char *out, size_t length);
char *decode_btag_flags(char *out, uint16_t btag_flags);
char *decode_btag_pattern_type(char *out, uint8_t btag_pattern_type);
char *decode_btag_opaque_type(char *out, uint8_t btag_opaque_type);

btag_t *
initialize_btag(dinfo_t *dip, uint8_t opaque_type)
{
    btag_t *btag;
    uint16_t btag_flags = 0;
    uint16_t opaque_size = 0;
    char *p, *hostname;

    /* Block tag extensions. */
   if (opaque_type == OPAQUE_WRITE_ORDER_TYPE) {
	btag_flags |= BTAG_OPAQUE;
	opaque_size = sizeof(btag_write_order_t);
    }
    btag = Malloc(dip, sizeof(btag_t) + opaque_size);
    if (btag == NULL) return(NULL);
    btag->btag_signature = HtoL32(BTAG_SIGNATURE);
    btag->btag_version = BTAG_VERSION_1;
    btag->btag_device_size = HtoL32(dip->di_lbdata_size);
    btag->btag_process_id = HtoL32( (uint32_t)os_getpid() );
    btag->btag_job_id = HtoL32(dip->di_job->ji_job_id);
    btag->btag_thread_number = HtoL32(dip->di_thread_number);
    btag->btag_opaque_data_type = opaque_type;
    btag->btag_opaque_data_size = HtoL16(opaque_size);

    if ( isDiskDevice(dip) ) {
	uint32_t devid = os_get_devID(dip->di_dname, dip->di_fd);
	btag->btag_devid = HtoL32(devid);
    } else {
	btag_flags |= BTAG_FILE;
	if (dip->di_serial_number == NULL) {
	    dip->di_btag_vflags &= ~BTAGV_SERIAL;
	}
    }
    hostname = os_gethostname();
    if (hostname) {
	if (p = strchr(hostname, '.')) {
	    *p = '\0';
	}
	(void)strncpy(btag->btag_hostname, hostname, sizeof(btag->btag_hostname)-1);
	free(hostname);
    }
#if defined(SCSI)
    if (dip->di_serial_number) {
	(void)strncpy(btag->btag_serial, dip->di_serial_number, sizeof(btag->btag_serial)-1);
    }
#endif /* defined(SCSI) */
    if (dip->di_iot_pattern) {
	btag->btag_pattern_type = PTYPE_IOT;
	btag->btag_pattern = HtoL32(dip->di_iot_seed_per_pass);
    } else if (dip->di_incr_pattern == True) {
	btag->btag_pattern_type = PTYPE_INCR;
    } else if (dip->di_pattern_file) {
	btag->btag_pattern_type = PTYPE_PFILE;
    } else {
	btag->btag_pattern_type = PTYPE_PATTERN;
	btag->btag_pattern = HtoL32(dip->di_pattern);
    }
    if (dip->di_lbdata_flag == True) {
	btag->btag_pattern_type |= PTYPE_LBDATA;
    }
    if (dip->di_timestamp_flag == True) {
	btag->btag_pattern_type |= PTYPE_TIMESTAMP;
    }
    if (dip->di_prefix_string) {
	btag_flags |= BTAG_PREFIX;
    }
    btag->btag_flags = HtoL16(btag_flags);
    btag->btag_step_offset = HtoL64(dip->di_step_offset);
    /* Set the initial verification flags. */
    if (dip->di_btag_vflags == 0) {
	; // dip->di_btag_vflags = BTAGV_QV;
    }
    if (dip->di_io_type == RANDOM_IO) {
	dip->di_btag_vflags &= ~BTAGV_RANDOM_DISABLE;
    }
    if ( (dip->di_ftype == INPUT_FILE) && (dip->di_io_mode != MIRROR_MODE) ) {
	dip->di_btag_vflags &= ~BTAGV_READONLY_DISABLE;
    }
    if (dip->di_iolock) {
	/* Multiple threads to the same file/device. */
	dip->di_btag_vflags &= ~BTAGV_THREAD_NUMBER;
        /* With random percentages, clear random flags. */
	if (dip->di_read_percentage || dip->di_random_percentage ||
	    dip->di_random_rpercentage || dip->di_random_wpercentage) {
	    dip->di_btag_vflags &= ~BTAGV_RANDOM_DISABLE;
	}
    }
    return (btag);
}

char *
decode_btag_flags(char *out, uint16_t btag_flags)
{
    char *bp = out;
    
    *bp = '\0';
    if (btag_flags & BTAG_FILE) {
	bp += sprintf(bp, "file");
    } else {
	bp += sprintf(bp, "disk");
    }
    if (btag_flags & BTAG_OPAQUE) {
	bp += sprintf(bp, ",opaque");
    }
    if (btag_flags & BTAG_PREFIX) {
	bp += sprintf(bp, ",prefix");
    }
    if (btag_flags & BTAG_RANDOM) {
	bp += sprintf(bp, ",random");
    } else {
	bp += sprintf(bp, ",sequential");
    }
    if (btag_flags & BTAG_REVERSE) {
	bp += sprintf(bp, ",reverse");
    } else {
	bp += sprintf(bp, ",forward");
    }
    return(out);
}

char *
decode_btag_pattern_type(char *out, uint8_t btag_pattern_type)
{
    char *bp = out;

    switch (btag_pattern_type & PTYPE_MASK) {
	case PTYPE_IOT:
	    bp += sprintf(bp, "IOT");
	    break;
	case PTYPE_INCR:
	    bp += sprintf(bp, "incrementing");
	    break;
	case PTYPE_PATTERN:
	    bp += sprintf(bp, "32-bit pattern");
	    break;
	case PTYPE_PFILE:
	    bp += sprintf(bp, "pattern file");
	    break;
	default:
	    bp += sprintf(bp, "UNKNOWN");
	    break;
    }
    if (btag_pattern_type & PTYPE_LBDATA) {
	bp += sprintf(bp, " w/lbdata");
    }
    if (btag_pattern_type & PTYPE_TIMESTAMP) {
	bp += sprintf(bp, ",timestamp");
    }
    return(out);
}

char *
decode_btag_opaque_type(char *out, uint8_t btag_opaque_type)
{
    char *bp = out;
    
    switch (btag_opaque_type) {
	case OPAQUE_NO_DATA_TYPE:
	    (void)strcpy(bp, "No Data Type");
	    break;
	case OPAQUE_WRITE_ORDER_TYPE:
	    (void)strcpy(bp, "Write Order Type");
	    break;
	default:
	    (void)strcpy(bp, "Unknown Type");
	    break;
    }
    return(out);
}

static char *empty_str = "";
static char *incorrect_str = "incorrect";
static char *expected_str = "Expected";
static char *received_str = "Received";
static char *physical_str = "Physical LBA";
static char *relative_str = "Relative LBA";
static char *notmapped_str = "<not mapped or not a valid offset>";

/*
 * Report (display) a Block Tag (btag).
 */
void
report_btag(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag, hbool_t raw_flag)
{
    char str[LARGE_BUFFER_SIZE];
    char *strp = str;
    int btag_errors = 0;
    uint32_t btag_index = 0;
    size_t btag_size = getBtagSize(rbtag);
    uint32_t rcrc32 = 0;
    
    Fprintf(dip, "\n");
    Fprintf(dip, "Block Tag (btag) @ "LLPX0FMT" ("SDF" bytes):\n", rbtag, btag_size);
    Fprintf(dip, "\n");
    
    if ( isDiskDevice(dip) ) {

	btag_index = offsetof(btag_t, btag_lba);
	if ( (ebtag && rbtag) &&
	     (dip->di_btag_vflags & BTAGV_LBA) &&
	     (ebtag->btag_lba != rbtag->btag_lba) ) {
	    Offset_t eoffset, roffset;
	    Fprintf(dip, DT_BTAG_FIELD "%s\n",
		    "LBA", btag_index, incorrect_str);
	    Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n",
		    expected_str, LtoH64(ebtag->btag_lba), LtoH64(ebtag->btag_lba) );
	    Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n",
		    received_str, LtoH64(rbtag->btag_lba), LtoH64(rbtag->btag_lba) );
	    eoffset = LtoH64(ebtag->btag_lba) * dip->di_device_size;
	    roffset = LtoH64(rbtag->btag_lba) * dip->di_device_size;
	    Fprintf(dip, DT_BTAG_FIELD "%s\n",
		    "Offset", btag_index, incorrect_str);
	    Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n",
		    expected_str, eoffset, eoffset );
	    Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n",
		    received_str, roffset, roffset );
	    btag_errors++;
	} else {
	    Offset_t roffset;
	    Fprintf(dip, DT_BTAG_FIELD FUF" ("LXF")\n",
		    "LBA", btag_index,
		    LtoH64(rbtag->btag_lba), LtoH64(rbtag->btag_lba));
	    roffset = LtoH64(rbtag->btag_lba) * dip->di_device_size;
	    Fprintf(dip, DT_BTAG_FIELD FUF" ("LXF")\n",
		    "Offset", btag_index, roffset, roffset);
	}

	btag_index = offsetof(btag_t, btag_devid);
	if ( (ebtag && rbtag) &&
	     (dip->di_btag_vflags & BTAGV_DEVID) &&
	     (ebtag->btag_devid != rbtag->btag_devid) ) {
	    Fprintf(dip, DT_BTAG_FIELD "%s\n",
		    "Device ID", btag_index, incorrect_str);
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		    expected_str, LtoH32(ebtag->btag_devid) );
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		    received_str, LtoH32(rbtag->btag_devid) );
	    btag_errors++;
	} else {
	    Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		    "Device ID", btag_index, LtoH32(rbtag->btag_devid) );
	}

    } else { /* Files */
	uint64_t lba;
	Offset_t offset;
	btag_index = offsetof(btag_t, btag_offset);
	if ( (ebtag && rbtag) &&
	     (dip->di_btag_vflags & BTAGV_OFFSET) &&
	     (ebtag->btag_offset != rbtag->btag_offset) ) {
	    Fprintf(dip, DT_BTAG_FIELD "%s\n", "File Offset", btag_index, incorrect_str);
	    /* Expected */
	    offset = LtoH64(ebtag->btag_offset);
	    Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n", expected_str, offset, offset);
	    lba = MapOffsetToLBA(dip, dip->di_fd, dip->di_dsize, offset, MismatchedData);
	    /* Note: Only display the physical LBA, if a file map was allocated. */
	    if (dip->di_fsmap) {
		if (lba == NO_LBA) {
		    Fprintf(dip, DT_FIELD_WIDTH "%s\n", physical_str, notmapped_str);
		} else {
		    Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n", physical_str, lba, lba);
		}
        	lba = (offset / dip->di_dsize);
		Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n", relative_str, lba, lba);
	    }
	    /* Received */
	    offset = LtoH64(rbtag->btag_offset);
	    Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n", received_str, offset, offset);
	    lba = MapOffsetToLBA(dip, dip->di_fd, dip->di_dsize, offset, MismatchedData);
	    if (dip->di_fsmap) {
		if (lba == NO_LBA) {
		    Fprintf(dip, DT_FIELD_WIDTH "%s\n", physical_str, notmapped_str);
		} else {
		    Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n", physical_str, lba, lba);
		}
        	lba = (offset / dip->di_dsize);
		Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n", relative_str, lba, lba);
	    }
	    btag_errors++;
	} else {
	    offset = LtoH64(rbtag->btag_offset);
	    Fprintf(dip, DT_BTAG_FIELD FUF" ("LXF")\n",
		    "File Offset", btag_index, offset, offset);
	    lba = MapOffsetToLBA(dip, dip->di_fd, dip->di_dsize, offset, MismatchedData);
	    if (dip->di_fsmap) {
		if (lba == NO_LBA) {
		    Fprintf(dip, DT_FIELD_WIDTH "%s\n", physical_str, notmapped_str);
		} else {
		    Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n", physical_str, lba, lba);
		}
        	lba = (offset / dip->di_dsize);
		Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n", relative_str, lba, lba);
	    }
	}

	btag_index = offsetof(btag_t, btag_inode);
	if ( (ebtag && rbtag) &&
	     (dip->di_btag_vflags & BTAGV_INODE) &&
	     (ebtag->btag_inode != rbtag->btag_inode) ) {
	    Fprintf(dip, DT_BTAG_FIELD "%s\n",
		    "File "OS_FILE_ID, btag_index, incorrect_str);
	    Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n",
		    expected_str, LtoH64(ebtag->btag_inode), LtoH64(ebtag->btag_inode) );
	    Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n",
		    received_str, LtoH64(rbtag->btag_inode), LtoH64(rbtag->btag_inode) );
	    btag_errors++;
	} else {
	    Fprintf(dip, DT_BTAG_FIELD FUF" ("LXF")\n",
		    "File "OS_FILE_ID, btag_index,
		    LtoH64(rbtag->btag_inode), LtoH64(rbtag->btag_inode) );
	}
    }

#if defined(SCSI)
    btag_index = offsetof(btag_t, btag_serial);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_SERIAL) &&
	 memcmp(ebtag->btag_serial, rbtag->btag_serial, sizeof(ebtag->btag_serial)) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Serial Number", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%s\n",
		expected_str,
		copy_string(ebtag->btag_serial, strp, sizeof(ebtag->btag_serial)) );
	Fprintf(dip, DT_FIELD_WIDTH "%s\n",
		received_str,
		copy_string(rbtag->btag_serial, strp, sizeof(rbtag->btag_serial)) );
	btag_errors++;
    } else if (rbtag->btag_serial[0]) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Serial Number", btag_index,
		copy_string(rbtag->btag_serial, strp, sizeof(rbtag->btag_serial)) );
    }
#endif /* defined(SCSI) */

    btag_index = offsetof(btag_t, btag_hostname);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_HOSTNAME) && 
	 memcmp(ebtag->btag_hostname, rbtag->btag_hostname, sizeof(ebtag->btag_hostname)) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Host Name", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%s\n",
		expected_str,
		copy_string(ebtag->btag_hostname, strp, sizeof(ebtag->btag_hostname)) );
	Fprintf(dip, DT_FIELD_WIDTH "%s\n",
		received_str,
		copy_string(rbtag->btag_hostname, strp, sizeof(rbtag->btag_hostname)) );
	btag_errors++;
    } else if (rbtag->btag_hostname[0]) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Host Name", btag_index,
		copy_string(rbtag->btag_hostname, strp, sizeof(rbtag->btag_hostname)) );
    }

    btag_index = offsetof(btag_t, btag_signature);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_SIGNATURE) &&
	 (ebtag->btag_signature != rbtag->btag_signature) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Signature", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		expected_str, LtoH32(ebtag->btag_signature) );
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		received_str, LtoH32(rbtag->btag_signature) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		"Signature", btag_index, LtoH32(rbtag->btag_signature) );
    }

    btag_index = offsetof(btag_t, btag_version);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_VERSION) &&
	 (ebtag->btag_version != rbtag->btag_version) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Version", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u\n",
		expected_str, ebtag->btag_version );
	Fprintf(dip, DT_FIELD_WIDTH "%u\n",
		received_str, rbtag->btag_version );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u\n",
		"Version", btag_index, rbtag->btag_version );
    }

    btag_index = offsetof(btag_t, btag_pattern_type);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_PATTERN_TYPE) &&
	 (ebtag->btag_pattern_type != rbtag->btag_pattern_type) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Pattern Type", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (%s)\n",
		expected_str, ebtag->btag_pattern_type,
		decode_btag_pattern_type(strp, ebtag->btag_pattern_type) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (%s)\n",
		received_str, rbtag->btag_pattern_type,
		decode_btag_pattern_type(strp, rbtag->btag_pattern_type) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (%s)\n",
		"Pattern Type", btag_index, rbtag->btag_pattern_type,
		decode_btag_pattern_type(strp, rbtag->btag_pattern_type) );
    }

    btag_index = offsetof(btag_t, btag_flags);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_FLAGS) &&
	 (ebtag->btag_flags != rbtag->btag_flags) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Flags", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "0x%x (%s)\n",
		expected_str, ebtag->btag_flags,
		decode_btag_flags(strp, ebtag->btag_flags) );
	Fprintf(dip, DT_FIELD_WIDTH "0x%x (%s)\n",
		received_str, rbtag->btag_flags,
		decode_btag_flags(strp, rbtag->btag_flags) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "0x%x (%s)\n",
		"Flags", btag_index, rbtag->btag_flags,
		decode_btag_flags(strp, rbtag->btag_flags) );
    }

    btag_index = offsetof(btag_t, btag_write_start);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_WRITE_START) &&
	 (ebtag->btag_write_start != rbtag->btag_write_start) ) {
	time_t ewrite_start = (time_t)LtoH32(ebtag->btag_write_start);
	time_t rwrite_start = (time_t)LtoH32(rbtag->btag_write_start);
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Write Pass Start (secs)", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x => %s\n",
		expected_str, ewrite_start,
		os_ctime(&ewrite_start, dip->di_time_buffer, sizeof(dip->di_time_buffer)) );
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x => %s\n",
		received_str, rwrite_start,
		(rwrite_start == (time_t)0) ? "<invalid time value>"
		: os_ctime(&rwrite_start, dip->di_time_buffer, sizeof(dip->di_time_buffer)) );
	btag_errors++;
    } else {
	time_t rwrite_start = (time_t)LtoH32(rbtag->btag_write_start);
	Fprintf(dip, DT_BTAG_FIELD "0x%08x => %s\n",
		"Write Pass Start (secs)", btag_index, rwrite_start,
		(rwrite_start == (time_t)0) ? "<invalid time value>"
		: os_ctime(&rwrite_start, dip->di_time_buffer, sizeof(dip->di_time_buffer)) );
    }

    /* 
     * We don't compare the write times, since times will be different, esp. usecs!
     * Note: The expected btag is calculated *after* the write completes!
     * The code is conditionalized out, in case we add optimization for expected.
     */ 
    btag_index = offsetof(btag_t, btag_write_secs);
    if ( raw_flag && (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_WRITE_SECS) &&
	 (ebtag->btag_write_secs != rbtag->btag_write_secs) ) {
	time_t ewrite_secs = (time_t)LtoH32(ebtag->btag_write_secs);
	time_t rwrite_secs = (time_t)LtoH32(rbtag->btag_write_secs);
	/* Note: For Windows, this is not seconds since the Epoch! */
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Write Timestamp (secs)", btag_index, incorrect_str);
#if defined(HighResolutionClock)
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		expected_str, ewrite_secs);
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		received_str, rwrite_secs);
#else /* defined(HighResolutionClock) */
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x => %s\n",
		expected_str, ewrite_secs,
		os_ctime(&ewrite_secs, dip->di_time_buffer, sizeof(dip->di_time_buffer)) );
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x => %s\n",
		received_str, rwrite_secs,
		(rwrite_secs == (time_t)0) ? "<invalid time value>"
		: os_ctime(&rwrite_secs, dip->di_time_buffer, sizeof(dip->di_time_buffer)) );
#endif /* defined(HighResolutionClock) */
	btag_errors++;
    } else {
	time_t rwrite_secs = (time_t)LtoH32(rbtag->btag_write_secs);
	/* Note: For Windows, this is not seconds since the Epoch! */
#if defined(HighResolutionClock)
	Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		    "Write Timestamp (secs)", btag_index, rwrite_secs);
#else /* defined(HighResolutionClock) */
	Fprintf(dip, DT_BTAG_FIELD "0x%08x => %s\n",
		"Write Timestamp (secs)", btag_index, rwrite_secs,
		(rwrite_secs == (time_t)0) ? "<invalid time value>"
		: os_ctime(&rwrite_secs, dip->di_time_buffer, sizeof(dip->di_time_buffer)) );
#endif /* defined(HighResolutionClock) */
    }
    btag_index = offsetof(btag_t, btag_write_usecs);
    if ( raw_flag && (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_WRITE_USECS) &&
	 (ebtag->btag_write_usecs != rbtag->btag_write_usecs) ) {
	time_t ewrite_usecs = (time_t)LtoH32(ebtag->btag_write_usecs);
	time_t rwrite_usecs = (time_t)LtoH32(rbtag->btag_write_usecs);
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Write Timestamp (usecs)", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		expected_str, ewrite_usecs);
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		received_str, rwrite_usecs);
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		    "Write Timestamp (usecs)", btag_index, LtoH32(rbtag->btag_write_usecs) );
    }

    btag_index = offsetof(btag_t, btag_pattern);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_PATTERN) &&
	 (ebtag->btag_pattern != rbtag->btag_pattern) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		(ebtag->btag_pattern_type == PTYPE_IOT) ? "IOT Seed" : "Pattern",
		btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		expected_str, LtoH32(ebtag->btag_pattern) );
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		received_str, LtoH32(rbtag->btag_pattern) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		(rbtag->btag_pattern_type == PTYPE_IOT) ? "IOT Seed" : "Pattern",
		btag_index, LtoH32(rbtag->btag_pattern) );
    }

    btag_index = offsetof(btag_t, btag_generation);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_GENERATION) &&
	 (ebtag->btag_generation != rbtag->btag_generation) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Generation", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		expected_str, LtoH32(ebtag->btag_generation), LtoH32(ebtag->btag_generation) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		received_str, LtoH32(rbtag->btag_generation), LtoH32(rbtag->btag_generation) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%08x)\n",
		"Generation", btag_index, LtoH32(rbtag->btag_generation), LtoH32(rbtag->btag_generation) );
    }

    btag_index = offsetof(btag_t, btag_process_id);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_PROCESS_ID) &&
	 (ebtag->btag_process_id != rbtag->btag_process_id) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Process ID", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		expected_str, LtoH32(ebtag->btag_process_id), LtoH32(ebtag->btag_process_id) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		received_str, LtoH32(rbtag->btag_process_id), LtoH32(rbtag->btag_process_id) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%08x)\n",
		"Process ID", btag_index, LtoH32(rbtag->btag_process_id), LtoH32(rbtag->btag_process_id) );
    }

    btag_index = offsetof(btag_t, btag_job_id);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_JOB_ID) &&
	 (ebtag->btag_job_id != rbtag->btag_job_id) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Job ID", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		expected_str, LtoH32(ebtag->btag_job_id), LtoH32(ebtag->btag_job_id) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		received_str, LtoH32(rbtag->btag_job_id), LtoH32(rbtag->btag_job_id) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%08x)\n",
		"Job ID", btag_index, LtoH32(rbtag->btag_job_id), LtoH32(rbtag->btag_job_id) );
    }

    btag_index = offsetof(btag_t, btag_thread_number);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_THREAD_NUMBER) &&
	 (ebtag->btag_thread_number != rbtag->btag_thread_number) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Thread Number", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		expected_str, LtoH32(ebtag->btag_thread_number), LtoH32(ebtag->btag_thread_number) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		received_str, LtoH32(rbtag->btag_thread_number), LtoH32(rbtag->btag_thread_number) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%08x)\n",
		"Thread Number", btag_index, LtoH32(rbtag->btag_thread_number), LtoH32(rbtag->btag_thread_number) );
    }

    btag_index = offsetof(btag_t, btag_device_size);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_DEVICE_SIZE) &&
	 (ebtag->btag_device_size != rbtag->btag_device_size) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Device Size", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		expected_str, LtoH32(ebtag->btag_device_size), LtoH32(ebtag->btag_device_size) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		received_str, LtoH32(rbtag->btag_device_size), LtoH32(rbtag->btag_device_size) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%08x)\n",
		"Device Size", btag_index, LtoH32(rbtag->btag_device_size), LtoH32(rbtag->btag_device_size) );
    }
    
    btag_index = offsetof(btag_t, btag_record_index);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_RECORD_INDEX) &&
	 (ebtag->btag_record_index != rbtag->btag_record_index) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Record Index", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		expected_str, LtoH32(ebtag->btag_record_index), LtoH32(ebtag->btag_record_index) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		received_str, LtoH32(rbtag->btag_record_index), LtoH32(rbtag->btag_record_index) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%08x)\n",
		"Record Index", btag_index, LtoH32(rbtag->btag_record_index), LtoH32(rbtag->btag_record_index) );
    }

    btag_index = offsetof(btag_t, btag_record_size);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_RECORD_SIZE) &&
	 (ebtag->btag_record_size != rbtag->btag_record_size) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Record Size", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		expected_str, LtoH32(ebtag->btag_record_size), LtoH32(ebtag->btag_record_size) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		received_str, LtoH32(rbtag->btag_record_size), LtoH32(rbtag->btag_record_size) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%08x)\n",
		"Record Size", btag_index, LtoH32(rbtag->btag_record_size), LtoH32(rbtag->btag_record_size) );
    }
    
    btag_index = offsetof(btag_t, btag_record_number);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_RECORD_NUMBER) &&
	 (ebtag->btag_record_number != rbtag->btag_record_number) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Record Number", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		expected_str, LtoH32(ebtag->btag_record_number), LtoH32(ebtag->btag_record_number) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
		received_str, LtoH32(rbtag->btag_record_number), LtoH32(rbtag->btag_record_number) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%08x)\n",
		"Record Number", btag_index, LtoH32(rbtag->btag_record_number), LtoH32(rbtag->btag_record_number) );
    }

    btag_index = offsetof(btag_t, btag_step_offset);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_STEP_OFFSET) &&
	 (ebtag->btag_step_offset != rbtag->btag_step_offset) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Step Offset", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n",
		expected_str, LtoH32(ebtag->btag_step_offset), LtoH32(ebtag->btag_step_offset) );
	Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n",
		received_str, LtoH32(rbtag->btag_step_offset), LtoH32(rbtag->btag_step_offset) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD LUF" ("LXF")\n",
		"Step Offset", btag_index, LtoH32(rbtag->btag_step_offset), LtoH32(rbtag->btag_step_offset) );
    }

    btag_index = offsetof(btag_t, btag_opaque_data_type);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_OPAQUE_DATA_TYPE) &&
	 (ebtag->btag_opaque_data_type != rbtag->btag_opaque_data_type) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Opaque Data Type", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (%s)\n",
		expected_str, ebtag->btag_opaque_data_type,
		decode_btag_opaque_type(strp, ebtag->btag_opaque_data_type));
	Fprintf(dip, DT_FIELD_WIDTH "%u (%s)\n",
		received_str, rbtag->btag_opaque_data_type,
		decode_btag_opaque_type(strp, rbtag->btag_opaque_data_type));
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (%s)\n",
		"Opaque Data Type", btag_index, rbtag->btag_opaque_data_type,
		decode_btag_opaque_type(strp, rbtag->btag_opaque_data_type));
    }

    btag_index = offsetof(btag_t, btag_opaque_data_size);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_OPAQUE_DATA_SIZE) &&
	 (ebtag->btag_opaque_data_size != rbtag->btag_opaque_data_size) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Opaque Data Size", btag_index, incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%04x)\n",
		expected_str, LtoH16(ebtag->btag_opaque_data_size), LtoH16(ebtag->btag_opaque_data_size) );
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%04x)\n",
		received_str, LtoH16(rbtag->btag_opaque_data_size), LtoH16(rbtag->btag_opaque_data_size) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u (0x%04x)\n",
		"Opaque Data Size", btag_index, LtoH16(rbtag->btag_opaque_data_size), LtoH16(rbtag->btag_opaque_data_size) );
    }
    
    btag_index = offsetof(btag_t, btag_crc32);
    rcrc32 = calculate_btag_crc(dip, rbtag);
    if ( (ebtag && rbtag) &&
	 (dip->di_btag_vflags & BTAGV_CRC32) ) {
	uint32_t ecrc32 = LtoH32(ebtag->btag_crc32);
	if (rcrc32 != rbtag->btag_crc32) {
	    Fprintf(dip, DT_BTAG_FIELD "%s\n",
		    "CRC-32", btag_index, incorrect_str);
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		    expected_str, rcrc32 );
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		    received_str, LtoH32(rbtag->btag_crc32) );
	    btag_errors++;
	} else if ( (raw_flag == True) && (ecrc32 != rcrc32) ) {
	    Fprintf(dip, DT_BTAG_FIELD "%s\n",
		    "CRC-32", btag_index, incorrect_str);
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		    expected_str, ecrc32 );
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		    received_str, LtoH32(rbtag->btag_crc32) );
	    btag_errors++;
	} else {
	    Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		    "CRC-32", btag_index, LtoH32(rbtag->btag_crc32) );
	}
	if (raw_flag == False) {
	    /* Copy received CRC to expected since we don't have a valid one! */
	    ebtag->btag_crc32 = HtoL32(rcrc32);
	}
    } else {
	Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		"CRC-32", btag_index, LtoH32(rbtag->btag_crc32) );
    }
    
    /*
     * Report or verify the btag extension, if any.
     */
    if ( (dip->di_funcs->tf_report_btag) && (dip->di_btag_vflags & BTAGV_OPAQUE_DATA) ) {
	btag_errors += (*dip->di_funcs->tf_report_btag)(dip, ebtag, rbtag, raw_flag);
    }

    if ( btag_errors ) {
	Fprintf(dip, DT_FIELD_WIDTH "%d\n", "Btag Errors", btag_errors);
    }

    Fprintf(dip, "\n");
    return;
}

void
update_btag(dinfo_t *dip, btag_t *btag, Offset_t offset, 
	    uint32_t record_index, size_t record_size, uint32_t record_number)
{
    struct timeval tv, *tvp = &tv;
    uint16_t btag_flags;

    /* Only update the btag flags if varying the I/O! */
    if (dip->di_vary_iodir || dip->di_vary_iotype) {
	btag_flags = LtoH16(btag->btag_flags);
	if (dip->di_vary_iodir) {
	    if (dip->di_io_dir == REVERSE) {
		btag_flags |= BTAG_REVERSE;
	    } else {
		btag_flags &= ~BTAG_REVERSE;
	    }
	}
	if (dip->di_vary_iotype) {
	    if (dip->di_io_type == RANDOM_IO) {
		btag_flags |= BTAG_RANDOM;
		if (dip->di_ftype == OUTPUT_FILE) {
		    dip->di_btag_vflags &= ~BTAGV_RANDOM_DISABLE;
		}
	    } else { /* Sequential I/O */
		btag_flags &= ~BTAG_RANDOM;
		if (dip->di_ftype == OUTPUT_FILE) {
		    /* Switching to sequential, set initial verify flags. */
		    dip->di_btag_vflags = dip->di_initial_vflags;
		}
	    }
	}
	btag->btag_flags = HtoL16(btag_flags);
    }
    btag->btag_write_start = HtoL32((uint32_t)dip->di_write_pass_start);
    if (gettimeofday(tvp, NULL) == SUCCESS) {
	btag->btag_write_secs = HtoL32(tvp->tv_sec);
	btag->btag_write_usecs = HtoL32(tvp->tv_usec);
    }
    if ( isDiskDevice(dip) ) {
	uint64_t lba = makeLBA(dip, offset);
	btag->btag_lba = HtoL64(lba);
    } else {
	btag->btag_offset = HtoL64(offset);
	btag->btag_inode = HtoL64(dip->di_inode);
    }
    if (dip->di_iot_pattern) {
	btag->btag_pattern = HtoL32(dip->di_iot_seed_per_pass);
    } else {
	btag->btag_pattern = HtoL32(dip->di_pattern);
    }
    btag->btag_generation = HtoL32(dip->di_pass_count + 1);
    btag->btag_record_index = HtoL32(record_index);
    btag->btag_record_number = HtoL32(record_number);
    btag->btag_record_size = HtoL32((uint32_t)record_size - record_index);
    btag->btag_crc32 = 0;
    if (dip->di_funcs->tf_update_btag) {
	(void)(*dip->di_funcs->tf_update_btag)(dip, btag, offset,
					       record_index, record_size, record_number);
    }
    return;
}

void
update_buffer_btags(dinfo_t *dip, btag_t *btag, Offset_t offset,
		    void *buffer, size_t record_size, uint32_t record_number)
{
    register uint8_t *bp = buffer;
    register uint32_t dsize = dip->di_lbdata_size;
    register uint32_t record_index;
    size_t btag_size = getBtagSize(btag);

    for (record_index = 0; record_index < record_size; record_index += dsize) {
	uint32_t crc = 0;
	update_btag(dip, btag, (offset + record_index),
		    record_index, record_size, record_number);
	/* Copy the btag template. */
	memcpy(bp, btag, btag_size);
	/* Calculate the CRC (btag + data). */
	crc = crc32(crc, bp, dip->di_lbdata_size);
	((btag_t *)bp)->btag_crc32 = HtoL32(crc);
	bp += dsize;
    }
    /* Return the 1st btag! */
    memcpy(btag, buffer, btag_size);
    return;
}

void
update_record_btag(dinfo_t *dip, btag_t *btag, Offset_t offset,
		   uint32_t record_index, size_t record_size, uint32_t record_number)
{
    struct timeval tv, *tvp = &tv;
    uint32_t dsize = dip->di_lbdata_size;
    
    /* Update things changing on each read record. */
    if ( isDiskDevice(dip) ) {
        uint64_t lba = makeLBA(dip, offset);
        btag->btag_lba = HtoL64(lba);
    } else {
        btag->btag_offset = HtoL64(offset);
        btag->btag_inode = HtoL64(dip->di_inode);
    }
    btag->btag_record_index = HtoL32(record_index);
    btag->btag_record_number = HtoL32(record_number);
    btag->btag_record_size = HtoL32((uint32_t)record_size - record_index);
    return;
}

char *
copy_string(char *in, char *out, size_t length)
{
    char *bp = out;
    uint8_t byte;
    int nonprintable = 0;
    size_t i;

    for (i = 0; (i < length); i++) {
	byte = in[i];
	if (byte == '\0') break;
	if ( isprint((int)byte) ) {
	    *bp++ = byte;
	} else {
	    nonprintable++;
	    break;
	}
    }
    if ( length && ( nonprintable || (i == 0) ) ) {
	bp = out;
	/* Non-printable becomes hex display! */
	for (i = 0; (i < length); i++) {
	    bp += sprintf(bp, "%02x ", (uint8_t)in[i]);
	}
	bp[-1] = '\0';
    } else {
	*bp = '\0';
    }
    return(out);
}

/*
 * Verify the expected btag vs. the received btag.
 */
int
verify_btags(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag, uint32_t *eindex, hbool_t raw_flag)
{
    char str[LARGE_BUFFER_SIZE];
    char *strp = str;
    int btag_errors = 0;
    uint32_t btag_index = 0;
    
    if (eindex) *eindex = 0xFFFF;

    /* REMEMBER: The btag is in little-endian format! */

    if ( isDiskDevice(dip) ) {
	if ( (dip->di_btag_vflags & BTAGV_LBA) &&
	     (ebtag->btag_lba != rbtag->btag_lba) ) {
	    if (dip->di_btag_debugFlag) {
		Fprintf(dip, "BTAG: LBA incorrect, expected "LUF", received "LUF"\n",
			LtoH64(ebtag->btag_lba), LtoH64(rbtag->btag_lba) );
	    }
	    btag_index = offsetof(btag_t, btag_lba);
	    if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	    btag_errors++;
	}
	if ( (dip->di_btag_vflags & BTAGV_DEVID) &&
	     (ebtag->btag_devid != rbtag->btag_devid) ) {
	    if (dip->di_btag_debugFlag) {
		Fprintf(dip, "BTAG: Device ID incorrect, expected 0x%08x, received 0x%08x\n",
			LtoH32(ebtag->btag_devid), LtoH32(rbtag->btag_devid) );
	    }
	    btag_index = offsetof(btag_t, btag_devid);
	    if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	    btag_errors++;
	}
    } else { /* Files */
	if ( (dip->di_btag_vflags & BTAGV_OFFSET) &&
	     (ebtag->btag_offset != rbtag->btag_offset) ) {
	    if (dip->di_btag_debugFlag) {
		Fprintf(dip, "BTAG: File offset incorrect, expected "FUF", received "FUF"\n",
			LtoH64(ebtag->btag_offset), LtoH64(rbtag->btag_offset) );
	    }
	    btag_index = offsetof(btag_t, btag_offset);
	    if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	    btag_errors++;
	}
	if ( (dip->di_btag_vflags & BTAGV_INODE) &&
	     (ebtag->btag_inode != rbtag->btag_inode) ) {
	    if (dip->di_btag_debugFlag) {
		Fprintf(dip, "BTAG: File %s incorrect, expected "FUF", received "FUF"\n",
			OS_FILE_ID, LtoH64(ebtag->btag_inode), LtoH64(rbtag->btag_inode) );
	    }
	    btag_index = offsetof(btag_t, btag_inode);
	    if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	    btag_errors++;
	}
    }

#if defined(SCSI)
    /* Note: Without a serial number, this should be all zeroes! */
    if ( (dip->di_btag_vflags & BTAGV_SERIAL) &&
	 (memcmp(ebtag->btag_serial, rbtag->btag_serial, sizeof(ebtag->btag_serial))) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Serial number incorrect, expected %s, received %s\n",
		    copy_string(ebtag->btag_serial, strp, sizeof(ebtag->btag_serial)),
		    copy_string(rbtag->btag_serial, strp, sizeof(rbtag->btag_serial))  );
	}
	btag_index = offsetof(btag_t, btag_serial);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }
#endif /* defined(SCSI) */

    if ( (dip->di_btag_vflags & BTAGV_HOSTNAME) &&
	 (memcmp(ebtag->btag_hostname, rbtag->btag_hostname, sizeof(ebtag->btag_hostname))) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Host name incorrect, expected %s, received %s\n",
		    copy_string(ebtag->btag_hostname, strp, sizeof(ebtag->btag_hostname)),
		    copy_string(rbtag->btag_hostname, strp, sizeof(rbtag->btag_hostname)) );
	}
	btag_index = offsetof(btag_t, btag_hostname);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_SIGNATURE) &&
	 (ebtag->btag_signature != rbtag->btag_signature) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Signature incorrect, expected 0x%08x, received 0x%08x\n",
		    LtoH32(ebtag->btag_signature), LtoH32(rbtag->btag_signature) );
	}
	btag_index = offsetof(btag_t, btag_signature);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_VERSION) &&
	 (ebtag->btag_version != rbtag->btag_version) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Version incorrect, expected %u, received %u\n",
		    ebtag->btag_version, rbtag->btag_version );
	}
	btag_index = offsetof(btag_t, btag_version);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_PATTERN_TYPE) &&
	 (ebtag->btag_pattern_type != rbtag->btag_pattern_type) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Pattern type incorrect, expected %u, received %u\n",
		    ebtag->btag_pattern_type, rbtag->btag_pattern_type );
	}
	btag_index = offsetof(btag_t, btag_pattern_type);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_FLAGS) &&
	 (ebtag->btag_flags != rbtag->btag_flags) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Flags incorrect, expected 0x%04x, received 0x%04x\n",
		    LtoH16(ebtag->btag_flags), LtoH16(rbtag->btag_flags) );
	}
	btag_index = offsetof(btag_t, btag_flags);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_WRITE_START) &&
	 (ebtag->btag_write_start != rbtag->btag_write_start) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Write start incorrect, expected 0x%08x, received 0x%08x\n",
		    LtoH32(ebtag->btag_write_start), LtoH32(rbtag->btag_write_start) );
	}
	btag_index = offsetof(btag_t, btag_write_start);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( raw_flag && (dip->di_btag_vflags & BTAGV_WRITE_SECS) &&
	 (ebtag->btag_write_secs != rbtag->btag_write_secs) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Write secs incorrect, expected 0x%08x, received 0x%08x\n",
		    LtoH32(ebtag->btag_write_secs), LtoH32(rbtag->btag_write_secs) );
	}
	btag_index = offsetof(btag_t, btag_write_secs);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( raw_flag && (dip->di_btag_vflags & BTAGV_WRITE_USECS) &&
	 (ebtag->btag_write_usecs != rbtag->btag_write_usecs) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Write usecs incorrect, expected 0x%08x, received 0x%08x\n",
		    LtoH32(ebtag->btag_write_usecs), LtoH32(rbtag->btag_write_usecs) );
	}
	btag_index = offsetof(btag_t, btag_write_usecs);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_PATTERN) &&
	 (ebtag->btag_pattern != rbtag->btag_pattern) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Pattern incorrect, expected 0x%08x, received 0x%08x\n",
		    LtoH32(ebtag->btag_pattern), LtoH32(rbtag->btag_pattern) );
	}
	btag_index = offsetof(btag_t, btag_pattern);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_GENERATION) &&
	 (ebtag->btag_generation != rbtag->btag_generation) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Generation incorrect, expected %u, received %u\n",
		    LtoH32(ebtag->btag_generation), LtoH32(rbtag->btag_generation) );
	}
	btag_index = offsetof(btag_t, btag_generation);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_PROCESS_ID) &&
	 (ebtag->btag_process_id != rbtag->btag_process_id) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Process ID incorrect, expected %u, received %u\n",
		    LtoH32(ebtag->btag_process_id), LtoH32(rbtag->btag_process_id) );
	}
	btag_index = offsetof(btag_t, btag_process_id);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_JOB_ID) &&
	 (ebtag->btag_job_id != rbtag->btag_job_id) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Job ID incorrect, expected %u, received %u\n",
		    LtoH32(ebtag->btag_job_id), LtoH32(rbtag->btag_job_id) );
	}
	btag_index = offsetof(btag_t, btag_job_id);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_THREAD_NUMBER) &&
	 (ebtag->btag_thread_number != rbtag->btag_thread_number) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Thread ID incorrect, expected %u, received %u\n",
		    LtoH32(ebtag->btag_thread_number), LtoH32(rbtag->btag_thread_number) );
	}
	btag_index = offsetof(btag_t, btag_thread_number);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_DEVICE_SIZE) &&
	 (ebtag->btag_device_size != rbtag->btag_device_size) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Device size incorrect, expected %u, received %u\n",
		    LtoH32(ebtag->btag_device_size), LtoH32(rbtag->btag_device_size) );
	}
	btag_index = offsetof(btag_t, btag_device_size);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    /*
     * Bypass verifying these for random I/O since we do overwrites.
     */
    if ( (dip->di_btag_vflags & BTAGV_RECORD_INDEX) &&
	 (ebtag->btag_record_index != rbtag->btag_record_index) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Record index incorrect, expected %u, received %u\n",
		    LtoH32(ebtag->btag_record_index), LtoH32(rbtag->btag_record_index) );
	}
	btag_index = offsetof(btag_t, btag_record_index);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_RECORD_SIZE) &&
	 (ebtag->btag_record_size != rbtag->btag_record_size) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Record size incorrect, expected %u, received %u\n",
		    LtoH32(ebtag->btag_record_size), LtoH32(rbtag->btag_record_size) );
	}
	btag_index = offsetof(btag_t, btag_record_size);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_RECORD_NUMBER) &&
	 (ebtag->btag_record_number != rbtag->btag_record_number) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Record number incorrect, expected %u, received %u\n",
		    LtoH32(ebtag->btag_record_number), LtoH32(rbtag->btag_record_number) );
	}
	btag_index = offsetof(btag_t, btag_record_number);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_STEP_OFFSET) &&
	 (ebtag->btag_step_offset != rbtag->btag_step_offset) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Step offset incorrect, expected "LUF", received "LUF"\n",
		    LtoH64(ebtag->btag_step_offset), LtoH64(rbtag->btag_step_offset) );
	}
	btag_index = offsetof(btag_t, btag_step_offset);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_OPAQUE_DATA_TYPE) &&
	 (ebtag->btag_opaque_data_type != rbtag->btag_opaque_data_type) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Opaque data type incorrect, expected %u, received %u\n",
		    ebtag->btag_opaque_data_type, rbtag->btag_opaque_data_type );
	}
	btag_index = offsetof(btag_t, btag_opaque_data_type);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if ( (dip->di_btag_vflags & BTAGV_OPAQUE_DATA_SIZE) &&
	 (ebtag->btag_opaque_data_size != rbtag->btag_opaque_data_size) ) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Opaque data size incorrect, expected %u, received %u\n",
		    LtoH16(ebtag->btag_opaque_data_size), LtoH16(rbtag->btag_opaque_data_size) );
	}
	btag_index = offsetof(btag_t, btag_opaque_data_size);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if (dip->di_btag_vflags & BTAGV_CRC32) {
	uint32_t ecrc32 = LtoH32(ebtag->btag_crc32);
	uint32_t rcrc32 = calculate_btag_crc(dip, rbtag);
	if ( rcrc32 != LtoH32(rbtag->btag_crc32) ) {
	    if (dip->di_btag_debugFlag) {
		Fprintf(dip, "BTAG: CRC-32 incorrect, expected 0x%08x, received 0x%08x\n",
			rcrc32, LtoH32(rbtag->btag_crc32) );
	    }
	    btag_index = offsetof(btag_t, btag_crc32);
	    if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	    btag_errors++;
	} else if ( (raw_flag == True) && (ecrc32 != rcrc32) ) {
	    if (dip->di_btag_debugFlag) {
		Fprintf(dip, "BTAG: CRC-32 incorrect, expected 0x%08x, received 0x%08x\n",
			ecrc32, rcrc32);
	    }
	    btag_index = offsetof(btag_t, btag_crc32);
	    if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	    btag_errors++;
	}
	if (raw_flag == False) {
	    /* During reads, we don't know the expected CRC, so copy to expected. */
	    ebtag->btag_crc32 = HtoL32(rcrc32);
	}
    }

    /*
     * Verify the btag extension, if any.
     */
    if ( dip->di_funcs->tf_verify_btag && (dip->di_btag_vflags & BTAGV_OPAQUE_DATA) ) {
	btag_errors += (*dip->di_funcs->tf_verify_btag)(dip, ebtag, rbtag, eindex, raw_flag);
    }

    if ( btag_errors && eindex && dip->di_btag_debugFlag ) {
	Fprintf(dip, "BTAG: Number of btag errors %d, first error index is %u\n",
		btag_errors, *eindex);
    }
    return( (btag_errors == 0) ? SUCCESS : FAILURE );
}

uint32_t
calculate_btag_crc(dinfo_t *dip, btag_t *btag)
{
    uint32_t expected_crc = 0;
    uint32_t saved_crc = btag->btag_crc32;

    btag->btag_crc32 = 0;
    expected_crc = crc32(0, btag, dip->di_lbdata_size);
    btag->btag_crc32 = saved_crc;
    return( expected_crc );
}

/*
 * Note: The received btag (rbtag) is followed by the data to CRC.
 */
int
verify_btag_crc(dinfo_t *dip, btag_t *rbtag, uint32_t *rcrc, hbool_t errors)
{
    uint32_t expected_crc;
    int status = SUCCESS;

    expected_crc = calculate_btag_crc(dip, rbtag);
    if (rcrc) *rcrc = expected_crc;
    if ( expected_crc != LtoH32(rbtag->btag_crc32) ) {
	if (errors == EnableErrors) {
	    Fprintf(dip, "Wrong btag CRC-32 detected, expected 0x%08x, received 0x%08x\n",
		    expected_crc, LtoH32(rbtag->btag_crc32) );
	}
	status = FAILURE;
    }
    return(status);
}

/*
 * Verify the CRC of all btags in the specified buffer.
 */
int
verify_buffer_btags(dinfo_t *dip, void *buffer, size_t record_size, btag_t **error_btag)
{
    register uint8_t *bp = buffer;
    register uint32_t dsize = dip->di_lbdata_size;
    register btag_t *btag = (btag_t *)buffer;
    size_t buffer_index;
    uint32_t crc, btag_crc;
    int status = SUCCESS;

    if (error_btag) *error_btag = NULL;
    for (buffer_index = 0; buffer_index < record_size; buffer_index += dsize) {
	btag = (btag_t *)bp;
	btag_crc = btag->btag_crc32;
	btag->btag_crc32 = 0;
	crc = crc32(0, btag, dsize);
	btag->btag_crc32 = btag_crc;
	btag_crc = LtoH32(btag_crc);
	if (crc != btag_crc) {
	    if (error_btag) {
		*error_btag = btag;
	    }
	    status = FAILURE;
	    break;
	}
	bp += dsize;
    }
    return(status);
}

int
parse_btag_verify_flags(dinfo_t *dip, char *string)
{
    int status = SUCCESS;
    
    while (*string != '\0') {
	if (match(&string, ",")) {
	    continue;
	} else if (match(&string, "all")) {
	    dip->di_btag_vflags = BTAGV_ALL;
	    continue;
	} else if (match(&string, "~all")) {
	    dip->di_btag_vflags &= ~BTAGV_ALL;
	    continue;
	} else if (match(&string, "qv")) {
	    dip->di_btag_vflags = BTAGV_QV;
	    continue;
	} else if (match(&string, "~qv")) {
	    dip->di_btag_vflags &= ~BTAGV_QV;
	    continue;
	} else if (match(&string, "lba")) {
	    dip->di_btag_vflags |= BTAGV_LBA;
	    continue;
	} else if (match(&string, "~lba")) {
	    dip->di_btag_vflags &= ~BTAGV_LBA;
	    continue;
	} else if (match(&string, "offset")) {
	    dip->di_btag_vflags |= BTAGV_OFFSET;
	    continue;
	} else if (match(&string, "~offset")) {
	    dip->di_btag_vflags &= ~BTAGV_OFFSET;
	    continue;
	} else if (match(&string, "devid")) {
	    dip->di_btag_vflags |= BTAGV_DEVID;
	    continue;
	} else if (match(&string, "~devid")) {
	    dip->di_btag_vflags &= ~BTAGV_DEVID;
	    continue;
	} else if (match(&string, "inode")) {
	    dip->di_btag_vflags |= BTAGV_INODE;
	    continue;
	} else if (match(&string, "~inode")) {
	    dip->di_btag_vflags &= ~BTAGV_INODE;
	    continue;
	} else if (match(&string, "serial")) {
	    dip->di_btag_vflags |= BTAGV_SERIAL;
	    continue;
	} else if (match(&string, "~serial")) {
	    dip->di_btag_vflags &= ~BTAGV_SERIAL;
	    continue;
	} else if (match(&string, "hostname")) {
	    dip->di_btag_vflags |= BTAGV_HOSTNAME;
	    continue;
	} else if (match(&string, "~hostname")) {
	    dip->di_btag_vflags &= ~BTAGV_HOSTNAME;
	    continue;
	} else if (match(&string, "signature")) {
	    dip->di_btag_vflags |= BTAGV_SIGNATURE;
	    continue;
	} else if (match(&string, "~signature")) {
	    dip->di_btag_vflags &= ~BTAGV_SIGNATURE;
	    continue;
	} else if (match(&string, "version")) {
	    dip->di_btag_vflags |= BTAGV_VERSION;
	    continue;
	} else if (match(&string, "~version")) {
	    dip->di_btag_vflags &= ~BTAGV_VERSION;
	    continue;
	} else if (match(&string, "pattern_type")) {
	    dip->di_btag_vflags |= BTAGV_PATTERN_TYPE;
	    continue;
	} else if (match(&string, "~pattern_type")) {
	    dip->di_btag_vflags &= ~BTAGV_PATTERN_TYPE;
	    continue;
	} else if (match(&string, "flags")) {
	    dip->di_btag_vflags |= BTAGV_FLAGS;
	    continue;
	} else if (match(&string, "~flags")) {
	    dip->di_btag_vflags &= ~BTAGV_FLAGS;
	    continue;
	} else if (match(&string, "write_start")) {
	    dip->di_btag_vflags |= BTAGV_WRITE_START;
	    continue;
	} else if (match(&string, "~write_start")) {
	    dip->di_btag_vflags &= ~BTAGV_WRITE_START;
	    continue;
	} else if (match(&string, "write_secs")) {
	    dip->di_btag_vflags |= BTAGV_WRITE_SECS;
	    continue;
	} else if (match(&string, "~write_secs")) {
	    dip->di_btag_vflags &= ~BTAGV_WRITE_SECS;
	    continue;
	} else if (match(&string, "write_usecs")) {
	    dip->di_btag_vflags |= BTAGV_WRITE_USECS;
	    continue;
	} else if (match(&string, "~write_usecs")) {
	    dip->di_btag_vflags &= ~BTAGV_WRITE_USECS;
	    continue;
	} else if (match(&string, "pattern")) {
	    dip->di_btag_vflags |= BTAGV_PATTERN;
	    continue;
	} else if (match(&string, "~pattern")) {
	    dip->di_btag_vflags &= ~BTAGV_PATTERN;
	    continue;
	} else if (match(&string, "generation")) {
	    dip->di_btag_vflags |= BTAGV_GENERATION;
	    continue;
	} else if (match(&string, "~generation")) {
	    dip->di_btag_vflags &= ~BTAGV_GENERATION;
	    continue;
	} else if (match(&string, "process_id")) {
	    dip->di_btag_vflags |= BTAGV_PROCESS_ID;
	    continue;
	} else if (match(&string, "~process_id")) {
	    dip->di_btag_vflags &= ~BTAGV_PROCESS_ID;
	    continue;
	} else if (match(&string, "job_id")) {
	    dip->di_btag_vflags |= BTAGV_JOB_ID;
	    continue;
	} else if (match(&string, "~job_id")) {
	    dip->di_btag_vflags &= ~BTAGV_JOB_ID;
	    continue;
	} else if (match(&string, "thread_number")) {
	    dip->di_btag_vflags |= BTAGV_THREAD_NUMBER;
	    continue;
	} else if (match(&string, "~thread_number")) {
	    dip->di_btag_vflags &= ~BTAGV_THREAD_NUMBER;
	    continue;
	} else if (match(&string, "device_size")) {
	    dip->di_btag_vflags |= BTAGV_DEVICE_SIZE;
	    continue;
	} else if (match(&string, "~device_size")) {
	    dip->di_btag_vflags &= ~BTAGV_DEVICE_SIZE;
	    continue;
	} else if (match(&string, "record_index")) {
	    dip->di_btag_vflags |= BTAGV_RECORD_INDEX;
	    continue;
	} else if (match(&string, "~record_index")) {
	    dip->di_btag_vflags &= ~BTAGV_RECORD_INDEX;
	    continue;
	} else if (match(&string, "record_size")) {
	    dip->di_btag_vflags |= BTAGV_RECORD_SIZE;
	    continue;
	} else if (match(&string, "~record_size")) {
	    dip->di_btag_vflags &= ~BTAGV_RECORD_SIZE;
	    continue;
	} else if (match(&string, "record_number")) {
	    dip->di_btag_vflags |= BTAGV_RECORD_NUMBER;
	    continue;
	} else if (match(&string, "~record_number")) {
	    dip->di_btag_vflags &= ~BTAGV_RECORD_NUMBER;
	    continue;
	} else if (match(&string, "step_offset")) {
	    dip->di_btag_vflags |= BTAGV_STEP_OFFSET;
	    continue;
	} else if (match(&string, "~step_offset")) {
	    dip->di_btag_vflags &= ~BTAGV_STEP_OFFSET;
	    continue;
	} else if (match(&string, "opaque_data_type")) {
	    dip->di_btag_vflags |= BTAGV_OPAQUE_DATA_TYPE;
	    continue;
	} else if (match(&string, "~opaque_data_type")) {
	    dip->di_btag_vflags &= ~BTAGV_OPAQUE_DATA_TYPE;
	    continue;
	} else if (match(&string, "opaque_data_size")) {
	    dip->di_btag_vflags |= BTAGV_OPAQUE_DATA_SIZE;
	    continue;
	} else if (match(&string, "~opaque_data_size")) {
	    dip->di_btag_vflags &= ~BTAGV_OPAQUE_DATA_SIZE;
	    continue;
	} else if (match(&string, "opaque_data")) {
	    dip->di_btag_vflags |= BTAGV_OPAQUE_DATA;
	    continue;
	} else if (match(&string, "~opaque_data")) {
	    dip->di_btag_vflags &= ~BTAGV_OPAQUE_DATA;
	    continue;
	} else if (match(&string, "crc32")) {
	    dip->di_btag_vflags |= BTAGV_CRC32;
	    continue;
	} else if (match(&string, "~crc32")) {
	    dip->di_btag_vflags &= ~BTAGV_CRC32;
	    continue;
	} else {
	    Eprintf(dip, "Unknown verify flag: %s\n", string);
	    show_btag_verify_flags(dip);
	    status = FAILURE;
	    break;
	}
    }
    if (status == SUCCESS) {
	dip->di_initial_vflags = dip->di_btag_vflags;
    }
    return(status);
}

#define P Printf

void
show_btag_verify_flags(dinfo_t *dip)
{
    P(dip, "\n");
    P(dip, "    Block Tag Verify Flags:\n");
    P(dip, "\n");
    P(dip, "\t             all = 0x%08x\n", BTAGV_ALL);
    P(dip, "\t              qv = 0x%08x\n", BTAGV_QV);
    P(dip, "\t             lba = 0x%08x\n", BTAGV_LBA);
    P(dip, "\t          offset = 0x%08x\n", BTAGV_OFFSET);
    P(dip, "\t           devid = 0x%08x\n", BTAGV_DEVID);
    P(dip, "\t           inode = 0x%08x\n", BTAGV_INODE);
    P(dip, "\t          serial = 0x%08x\n", BTAGV_SERIAL);
    P(dip, "\t        hostname = 0x%08x\n", BTAGV_HOSTNAME);
    P(dip, "\t       signature = 0x%08x\n", BTAGV_SIGNATURE);
    P(dip, "\t         version = 0x%08x\n", BTAGV_VERSION);
    P(dip, "\t    pattern_type = 0x%08x\n", BTAGV_PATTERN_TYPE);
    P(dip, "\t           flags = 0x%08x\n", BTAGV_FLAGS);
    P(dip, "\t     write_start = 0x%08x\n", BTAGV_WRITE_START);
    P(dip, "\t      write_secs = 0x%08x\n", BTAGV_WRITE_SECS);
    P(dip, "\t     write_usecs = 0x%08x\n", BTAGV_WRITE_USECS);
    P(dip, "\t         pattern = 0x%08x\n", BTAGV_PATTERN);
    P(dip, "\t      generation = 0x%08x\n", BTAGV_GENERATION);
    P(dip, "\t      process_id = 0x%08x\n", BTAGV_PROCESS_ID);
    P(dip, "\t          job_id = 0x%08x\n", BTAGV_JOB_ID);
    P(dip, "\t   thread_number = 0x%08x\n", BTAGV_THREAD_NUMBER);
    P(dip, "\t     device_size = 0x%08x\n", BTAGV_DEVICE_SIZE);
    P(dip, "\t    record_index = 0x%08x\n", BTAGV_RECORD_INDEX);
    P(dip, "\t     record_size = 0x%08x\n", BTAGV_RECORD_SIZE);
    P(dip, "\t   record_number = 0x%08x\n", BTAGV_RECORD_NUMBER);
    P(dip, "\t     step_offset = 0x%08x\n", BTAGV_STEP_OFFSET);
    P(dip, "\topaque_data_type = 0x%08x\n", BTAGV_OPAQUE_DATA_TYPE);
    P(dip, "\topaque_data_size = 0x%08x\n", BTAGV_OPAQUE_DATA_SIZE);
    P(dip, "\t     opaque_data = 0x%08x\n", BTAGV_OPAQUE_DATA);
    P(dip, "\t           crc32 = 0x%08x\n", BTAGV_CRC32);
    return;
}

void
show_btag_verify_flags_set(dinfo_t *dip, uint32_t verify_flags)
{
    P(dip, "\n");
    P(dip, "    Block Tag Verify Flags Set: 0x%08x\n", verify_flags);
    P(dip, "\n");
    if (verify_flags & BTAGV_LBA) {
	P(dip, "\t             lba = 0x%08x\n", BTAGV_LBA);
    }
    if (verify_flags & BTAGV_OFFSET) {
	P(dip, "\t          offset = 0x%08x\n", BTAGV_OFFSET);
    }
    if (verify_flags & BTAGV_DEVID) {
	P(dip, "\t           devid = 0x%08x\n", BTAGV_DEVID);
    }
    if (verify_flags & BTAGV_INODE) {
	P(dip, "\t           inode = 0x%08x\n", BTAGV_INODE);
    }
    if (verify_flags & BTAGV_SERIAL) {
	P(dip, "\t          serial = 0x%08x\n", BTAGV_SERIAL);
    }
    if (verify_flags & BTAGV_HOSTNAME) {
	P(dip, "\t        hostname = 0x%08x\n", BTAGV_HOSTNAME);
    }
    if (verify_flags & BTAGV_SIGNATURE) {
	P(dip, "\t       signature = 0x%08x\n", BTAGV_SIGNATURE);
    }
    if (verify_flags & BTAGV_VERSION) {
	P(dip, "\t         version = 0x%08x\n", BTAGV_VERSION);
    }
    if (verify_flags & BTAGV_PATTERN_TYPE) {
	P(dip, "\t    pattern_type = 0x%08x\n", BTAGV_PATTERN_TYPE);
    }
    if (verify_flags & BTAGV_FLAGS) {
	P(dip, "\t           flags = 0x%08x\n", BTAGV_FLAGS);
    }
    if (verify_flags & BTAGV_WRITE_START) {
	P(dip, "\t     write_start = 0x%08x\n", BTAGV_WRITE_START);
    }
    if (verify_flags & BTAGV_WRITE_SECS) {
	P(dip, "\t      write_secs = 0x%08x\n", BTAGV_WRITE_SECS);
    }
    if (verify_flags & BTAGV_WRITE_USECS) {
	P(dip, "\t     write_usecs = 0x%08x\n", BTAGV_WRITE_USECS);
    }
    if (verify_flags & BTAGV_PATTERN) {
	P(dip, "\t         pattern = 0x%08x\n", BTAGV_PATTERN);
    }
    if (verify_flags & BTAGV_GENERATION) {
	P(dip, "\t      generation = 0x%08x\n", BTAGV_GENERATION);
    }
    if (verify_flags & BTAGV_PROCESS_ID) {
	P(dip, "\t      process_id = 0x%08x\n", BTAGV_PROCESS_ID);
    }
    if (verify_flags & BTAGV_JOB_ID) {
	P(dip, "\t          job_id = 0x%08x\n", BTAGV_JOB_ID);
    }
    if (verify_flags & BTAGV_THREAD_NUMBER) {
	P(dip, "\t   thread_number = 0x%08x\n", BTAGV_THREAD_NUMBER);
    }
    if (verify_flags & BTAGV_DEVICE_SIZE) {
	P(dip, "\t     device_size = 0x%08x\n", BTAGV_DEVICE_SIZE);
    }
    if (verify_flags & BTAGV_RECORD_INDEX) {
	P(dip, "\t    record_index = 0x%08x\n", BTAGV_RECORD_INDEX);
    }
    if (verify_flags & BTAGV_RECORD_SIZE) {
	P(dip, "\t     record_size = 0x%08x\n", BTAGV_RECORD_SIZE);
    }
    if (verify_flags & BTAGV_RECORD_NUMBER) {
	P(dip, "\t   record_number = 0x%08x\n", BTAGV_RECORD_NUMBER);
    }
    if (verify_flags & BTAGV_STEP_OFFSET) {
	P(dip, "\t     step_offset = 0x%08x\n", BTAGV_STEP_OFFSET);
    }
    if (verify_flags & BTAGV_OPAQUE_DATA_TYPE) {
	P(dip, "\topaque_data_type = 0x%08x\n", BTAGV_OPAQUE_DATA_TYPE);
    }
    if (verify_flags & BTAGV_OPAQUE_DATA_SIZE) {
	P(dip, "\topaque_data_size = 0x%08x\n", BTAGV_OPAQUE_DATA_SIZE);
    }
    if (verify_flags & BTAGV_OPAQUE_DATA) {
	P(dip, "\t     opaque_data = 0x%08x\n", BTAGV_OPAQUE_DATA);
    }
    if (verify_flags & BTAGV_CRC32) {
	P(dip, "\t           crc32 = 0x%08x\n", BTAGV_CRC32);
    }
    return;
}

int
verify_btag_options(dinfo_t *dip)
{
    int status = SUCCESS;
    
    /* 
     * Block Tag (btag) Sanity Checks: 
     */
    if (dip->di_btag_flag == True) {
	if (dip->di_block_size < sizeof(btag_t)) {
	    Eprintf(dip, "Please specify a block size >= %d for block tags!\n",
		    sizeof(btag_t));
	    status = FAILURE;
	}
	if ( dip->di_align_offset || (dip->di_rotate_flag == True) ) {
	    Wprintf(dip, "Disabling misaligned buffers since block tags is enabled!\n");
	    dip->di_align_offset = 0;
	    dip->di_rotate_flag = False;
	}
    }
    return(status);
}

/* --------------------------------------------------------------------------- */

/* URL: http://www.stillhq.com/gpg/source-modified-1.0.3/zlib/crc32.html */
/* OR: http://www.opensource.apple.com/source/zlib/zlib-5/zlib/crc32.c */

/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2002 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.1.4, March 11th, 2002

  Copyright (C) 1995-2002 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu


  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files ftp://ds.internic.net/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

/* @(#) $Id: crc32.c,v 1.1.1.1 2000/10/19 05:38:50 root Exp $ */

//#include "zlib.h"

#define local static

#ifdef DYNAMIC_CRC_TABLE

//local int crc_table_empty = 1;
//local uLongf crc_table[256];
//local void make_crc_table OF((void));

/*
  Generate a table for a byte-wise 32-bit CRC calculation on the polynomial:
  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

  Polynomials over GF(2) are represented in binary, one bit per coefficient,
  with the lowest powers in the most significant bit.  Then adding polynomials
  is just exclusive-or, and multiplying a polynomial by x is a right shift by
  one.  If we call the above polynomial p, and represent a byte as the
  polynomial q, also with the lowest power in the most significant bit (so the
  byte 0xb1 is the polynomial x^7+x^3+x+1), then the CRC is (q*x^32) mod p,
  where a mod b means the remainder after dividing a by b.

  This calculation is done using the shift-register method of multiplying and
  taking the remainder.  The register is initialized to zero, and for each
  incoming bit, x^32 is added mod p to the register if the bit is a one (where
  x^32 mod p is p+x^32 = x^26+...+1), and the register is multiplied mod p by
  x (which is shifting right by one and adding x^32 mod p if the bit shifted
  out is a one).  We start with the highest power (least significant bit) of
  q and repeat for all eight bits of q.

  The table is simply the CRC of all possible eight bit values.  This is all
  the information needed to generate CRC's on data a byte at a time for all
  combinations of CRC register values and incoming bytes.
*/
local void make_crc_table()
{
  uLong c;
  int n, k;
  uLong poly;            /* polynomial exclusive-or pattern */
  /* terms of polynomial defining this crc (except x^32): */
  static const Byte p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  /* make exclusive-or pattern from polynomial (0xedb88320L) */
  poly = 0L;
  for (n = 0; n < sizeof(p)/sizeof(Byte); n++)
    poly |= 1L << (31 - p[n]);
 
  for (n = 0; n < 256; n++)
  {
    c = (uLong)n;
    for (k = 0; k < 8; k++)
      c = c & 1 ? poly ^ (c >> 1) : c >> 1;
    crc_table[n] = c;
  }
  crc_table_empty = 0;
}
#else /* !defined(DYNAMIC_CRC_TABLE) */
/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */
local const uint32_t crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};
#endif /* !defined(DYNAMIC_CRC_TABLE) */

#if 0
/* =========================================================================
 * This function can be used by asm versions of crc32()
 */
const uLongf * ZEXPORT get_crc_table()
{
#ifdef DYNAMIC_CRC_TABLE
  if (crc_table_empty) make_crc_table();
#endif
  return (const uLongf *)crc_table;
}
#endif /* 0 */

#define Do1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define Do2(buf)  Do1(buf); Do1(buf);
#define Do4(buf)  Do2(buf); Do2(buf);
#define Do8(buf)  Do4(buf); Do4(buf);

uint32_t
crc32(uint32_t crc, void *buffer, unsigned int length)
{
    register uint8_t *bp = buffer;
    register unsigned int len = length;

    if (bp == NULL) return(0);
#ifdef DYNAMIC_CRC_TABLE
    if (crc_table_empty)
      make_crc_table();
#endif
    crc = (crc ^ 0xffffffffU);
    while (len >= 8) {
      Do8(bp);
      len -= 8;
    }
    if (len) do {
	Do1(bp);
    } while (--len);
    return( crc ^ 0xffffffffU );
}
