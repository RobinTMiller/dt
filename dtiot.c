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
 * Module:	dtiot.c
 * Author:	Robin T. Miller
 * Date:	August 17th, 2013
 *
 * Description:
 *	IOT related functions.
 *
 * Modification History:
 *
 * February 5th, 2021 by Robin T. Miller
 *      Enhance IOT analysis for partial block corruptions.
 * 
 * November 27th, 2020 by Robnin T. Miller
 *      Add more context for file systems, report physical & relative LBA.
 * 
 * June 1st, 2020 by Robin T. Miller
 *      Report the record buffer index to make it easier to display data in
 * saved corruption files for use with "showbtags offset=value" command.
 * 
 * January 9th, 2020 by Robin T. Miller
 *      When displaying block tags, do *not* overwrite the write time or CRC,
 * if we are doing immediate read-after-write, since the expected block tag is
 * the full block tag just written. This fudging of the btag data is to avoid
 * reporting incorrect btag reporting when we are in read only pass, since we
 * do not have the original write time stamps.
 * 
 * December 22nd, 2019 by Robin T. Miller
 *      For file systems, report the physical LBA, if we can map the offset.
 * 
 * March 8th, 2016 by Robin T. Miller
 * 	Fix bug in display_iot_data() where the wrong received word0 was copied,
 * which results in a wrong received block number and offset with timestamps!
 * 
 * June 8th, 2015 by Robin T. Miller
 * 	Added support for block tags (btags).
 *
 * May 11th, 2015 by Robin T. Miller
 * 	Omit reporting IOT pass when the pass count causes wrapping (256)
 * to avoid confusion, until the block tag (btag) support is implemented.
 * 	Update display_iot_block() to always display IOT seed information
 * when the LBA or IOT Seed does not match! Improper logic was omitting
 * this key information, when the pass count causes the IOT seed to wrap!
 *
 * February 14th, 2015 by Robin T. Miller
 *	Minor updates to decoding IOT data with read-after-write and
 * timestamps are enabled. Incorrect "good" data was reported, along
 * with the read start time, instead of the write pass time, which
 * was misleading (even to me, the author!).
 *
 * May 20th, 2014 by Robin T. Miller
 * 	Remove PRT_NOFLUSH when displaying IOT data, since this causes
 * garbled and/or lost stderr output on some OS's (Linux and Solaris).
 * 
 * May 15th, 2014 by Robin T. MIller
 * 	Fix minor bug when comparing bytes per line with timestamp,
 * a corruption *after* the timestamp was not properly flagged.
 * 
 * August 17th, 2013 by Robin T Miller
 * 	Moving IOT functions here.
 */
#include "dt.h"
#include <ctype.h>

/*
 * Forward Reference:
 */
int compare_iot_block(dinfo_t *dip, uint8_t *pptr, uint8_t *vptr, hbool_t raw_flag);
hbool_t	is_iot_data(dinfo_t *dip, uint8_t *rptr, size_t rsize, int rprefix_size,
		    int *iot_offset, iotlba_t *rlbn);

static char *notmapped_str = "<not mapped or not a valid offset>";

/*
 * init_iotdata() - Initialize Buffer with IOT test pattern.
 *
 * Description:
 *	This function takes the starting logical block address, and
 * inserts it every logical block size bytes.  The data pattern used
 * is the logical block with the constant 0x01010101 added every u_int.
 * Note: With multiple passes, the IOT constant is multiplied by the
 * pass count to generate uniqueness (unless disable=unique)
 *
 * Inputs:
 *	dip = The device information pointer.
 * 	buffer = The data buffer to initialize.
 * 	bcount = The data buffer size (in bytes).
 *	lba = The starting logical block address.
 *	lbsize = The logical block size (in bytes).
 *
 * Return Value:
 * 	Returns the next lba to use.
 * 
 * Note: If the count is smaller than sizeof(u_int32), then no lba is
 * encoded in the buffer.  Instead, we init odd bytes with ~0.
 */
u_int32	
init_iotdata (
	dinfo_t		*dip,
    	u_char		*buffer,
	size_t		bcount,
	u_int32		lba,
	u_int32		lbsize )
{
    register ssize_t count = (ssize_t)bcount;
    register u_int32 lba_pattern;
    /* IOT pattern initialization size. */
    register int iot_icnt = sizeof(lba_pattern);
    register int i;
    btag_t *btag = dip->di_btag;

    if (lbsize == 0) return (lba);
    dip->di_pattern_bufptr = buffer;

    /*
     * If the prefix string is a multiple of an unsigned int,
     * we can initialize the buffer using 32-bit words, otherwise
     * we must do so a byte at a time which is slower (of course).
     *
     * Note: This 32-bit fill is possible since the pattern buffer
     * is known to be page aligned!
     *
     * Format: <optional btag><prefix string><IOT pattern>...
     */
    if (count < iot_icnt) {
        init_buffer(dip, dip->di_pattern_bufptr, count, ~0);
    } else if (dip->di_fprefix_string && (dip->di_fprefix_size & (iot_icnt-1))) {
        register u_char *bptr = dip->di_pattern_bufptr;
        /*
         * Initialize the buffer with the IOT test pattern.
         */
        while ( (count > 0) && (count >= (ssize_t)sizeof(lba)) ) {
	    size_t header_size = 0;
	    int adjust;
            /*
             * Process one lbsize'd block at a time.
             *
             * Format: <optional btag><optional prefix><lba><lba data>...
             */
	    if (btag) {
		size_t btag_size = getBtagSize(btag);
		bptr += btag_size;
		count -= btag_size;
		header_size += btag_size;
	    }
	    if (dip->di_fprefix_string) {
		size_t pcount = copy_prefix(dip, (u_char *)bptr, count);
		bptr += pcount;
		count -= (ssize_t)pcount;
		header_size += pcount;
	    }
            lba_pattern = lba++;
            for (i = (lbsize - (uint32_t)header_size);
		 ( (i > 0) && (count >= iot_icnt) ); ) {
#if _BIG_ENDIAN_
                init_swapped(dip, bptr, iot_icnt, lba_pattern);
#else /* !_BIG_ENDIAN_ */
                init_buffer(dip, bptr, iot_icnt, lba_pattern);
#endif /* _BIG_ENDIAN_ */
		lba_pattern += dip->di_iot_seed_per_pass;
		/* Adjust counts, but not too many! */
		adjust = MIN(i, iot_icnt);
                i -= adjust;
                bptr += adjust;
                count -= adjust;
            }
        }
        /* Handle any residual count here! */
        if ( (count > 0) && (count < iot_icnt)) {
	    //Printf(dip, "#1: Residual count is %d, lba_pattern = %04x\n",  count, lba_pattern);
            //init_buffer(dip, bptr, count, ~0);
            init_buffer(dip, (u_char *)bptr, count, lba_pattern);
        }
    } else {
        register int wperb; /* words per lbsize'ed buffer */
        register u_int32 *bptr;

        wperb = ( (lbsize - dip->di_fprefix_size) / iot_icnt);
	if (btag) {
	    size_t btag_size = getBtagSize(btag);
	    wperb -= ((int)btag_size / iot_icnt);
	}
        bptr = (u_int32 *)dip->di_pattern_bufptr;

        /*
         * Initialize the buffer with the IOT test pattern.
         */
        while ( (count > 0) && (count >= iot_icnt) ) {
	    if (btag) {
		size_t btag_size = getBtagSize(btag);
		bptr += (btag_size / iot_icnt);
		count -= btag_size;
	    }
	    if (dip->di_fprefix_string) {
		size_t pcount = copy_prefix (dip, (u_char *)bptr, count);
		bptr += (pcount / iot_icnt);
		count -= (ssize_t)pcount;
	    }
            lba_pattern = lba++;
            for (i = 0; (i < wperb) && (count >= iot_icnt); i++) {
#if _BIG_ENDIAN_
                init_swapped(dip, (u_char *)bptr++, iot_icnt, lba_pattern);
#else /* !_BIG_ENDIAN_ */
                *bptr++ = lba_pattern;
#endif /* _BIG_ENDIAN_ */
		lba_pattern += dip->di_iot_seed_per_pass;
                count -= iot_icnt;
            }
        }
        /* Handle any residual count here! */
        if ( (count > 0) && (count < iot_icnt)) {
	    //Printf(dip, "#2: Residual count is %d, lba_pattern = %04x\n",  count, lba_pattern);
            //init_buffer(dip, (u_char *)bptr, count, ~0);
            init_buffer(dip, (u_char *)bptr, count, lba_pattern);
        }
    }
    return(lba);
}

void
process_iot_data(dinfo_t *dip, u_char *pbuffer, u_char *vbuffer, size_t bcount, hbool_t raw_flag)
{
    int status;

    status = AcquirePrintLock(dip);

    analyze_iot_data(dip, pbuffer, vbuffer, bcount, raw_flag);
    display_iot_data(dip, pbuffer, vbuffer, bcount, raw_flag);

    if (status == SUCCESS) {
        status = ReleasePrintLock(dip);
    }
    return;
}

void
report_bad_sequence(dinfo_t *dip, int start, int length, Offset_t offset)
{
    Offset_t pos = (offset + ((start-1) * dip->di_lbdata_size));
    uint64_t lba = MapOffsetToLBA(dip, dip->di_fd, dip->di_lbdata_size, pos, MismatchedData);

    Fprintf(dip, DT_FIELD_WIDTH "%d\n",
	  "Start of corrupted blocks", start);
    Fprintf(dip, DT_FIELD_WIDTH "%d (%d bytes)\n",
	  "Length of corrupted blocks", length, (length * dip->di_lbdata_size));
    if (lba == NO_LBA) {
	Fprintf(dip, DT_FIELD_WIDTH FUF " (<not mapped>)\n",
		"Corrupted blocks file offset", pos);
    } else if (dip->di_fsmap) {
	uint64_t rlba = makeLBA(dip, pos);
	Fprintf(dip, DT_FIELD_WIDTH FUF " (Relative LBA "FUF", Physical LBA "FUF")\n",
		"Corrupted blocks file offset", pos, rlba, lba);
    } else {
	Fprintf(dip, DT_FIELD_WIDTH FUF " (LBA "FUF")\n",
	       "Corrupted blocks file offset", pos, lba);
    }
    return;
}

void
report_good_sequence(dinfo_t *dip, int start, int length, Offset_t offset)
{
    Offset_t pos = (offset + ((start-1) * dip->di_lbdata_size));
    uint64_t lba = MapOffsetToLBA(dip, dip->di_fd, dip->di_lbdata_size, pos, MismatchedData);

    Fprintf(dip, DT_FIELD_WIDTH "%d\n",
	    "Start of good blocks", start);
    Fprintf(dip, DT_FIELD_WIDTH "%d (%d bytes)\n",
	    "Length of good blocks", length, (length * dip->di_lbdata_size));
    if (lba == NO_LBA) {
	Fprintf(dip, DT_FIELD_WIDTH FUF " (<not mapped>)\n",
		"Good blocks file offset", pos);
    } else if (dip->di_fsmap) {
	uint64_t rlba = makeLBA(dip, pos);
	Fprintf(dip, DT_FIELD_WIDTH FUF " (Relative LBA "FUF", Physical LBA "FUF")\n",
		"Good blocks file offset", pos, rlba, lba);
    } else {
	Fprintf(dip, DT_FIELD_WIDTH FUF " (LBA "FUF")\n",
		"Good blocks file offset", pos, lba);
    }
    return;
}

void
analyze_iot_data(dinfo_t *dip, uint8_t *pbuffer, uint8_t *vbuffer, size_t bcount, hbool_t raw_flag)
{
    uint8_t *pptr = pbuffer;
    uint8_t *vptr = vbuffer;
    size_t count = bcount;
    int bad_blocks = 0, good_blocks = 0;
    int bad_start = 0,  good_start = 0;
    int zero_blocks = 0;
    uint32_t block = 1;
    int blocks = (int)(count / dip->di_lbdata_size);
    Offset_t record_offset;
    
    /* Note: Use dt's offset rather than the OS fd offset (for now)! */
    record_offset = getFileOffset(dip);
    //record_offset = get_current_offset(dip, (ssize_t)bcount);
    
    Fprintf(dip, "\n");
    Fprintf(dip, "Analyzing IOT Record Data: (Note: Block #'s are relative to start of record!)\n");
    Fprintf(dip, "\n");
    Fprintf(dip, DT_FIELD_WIDTH "%d\n",
	    "IOT block size", dip->di_lbdata_size);
    Fprintf(dip, DT_FIELD_WIDTH "%d (%u bytes)\n",
	    "Total number of blocks", blocks, count);
    if (dip->di_pass_count < 256) {		/* Handle case where we wrap! */
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x (pass %u)\n",
		"Current IOT seed value",
		dip->di_iot_seed_per_pass, (dip->di_iot_seed_per_pass / IOT_SEED));
    } else {
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		"Current IOT seed value", dip->di_iot_seed_per_pass);
    }
    if (dip->di_iot_seed_per_pass != IOT_SEED) {
	if (dip->di_pass_count < 256) {		/* Handle case where we wrap! */
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x (pass %u)\n",
		    "Previous IOT seed value", (dip->di_iot_seed_per_pass - IOT_SEED),
		    ((dip->di_iot_seed_per_pass - IOT_SEED) / IOT_SEED));
	} else {
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		    "Previous IOT seed value", (dip->di_iot_seed_per_pass - IOT_SEED));
	}
    }

    /*
     * Compare one lbdata sized block at a time.
     */
    while (blocks) {
	int result = compare_iot_block(dip, pptr, vptr, raw_flag);
	if (result == 0) {
	    good_blocks++;
	    if (good_start == 0) {
		good_start = block;
	    }
	    if (bad_start) {
		report_bad_sequence(dip, bad_start, (block - bad_start), record_offset);
		bad_start = 0;
	    }
	} else {
	    u_int32 i;
	    bad_blocks++;
	    if (bad_start == 0) {
		bad_start = block;
	    }
	    for (i = 0; (i < dip->di_lbdata_size); i++) {
		if (vptr[i] != '\0') {
		    break;
		}
	    }
	    if (i == dip->di_lbdata_size) {
		zero_blocks++;
	    }
	    if (good_start) {
		report_good_sequence(dip, good_start, (block - good_start), record_offset);
		good_start = 0;
	    }
	}
	block++; blocks--;
	pptr += dip->di_lbdata_size;
	vptr += dip->di_lbdata_size;
    }
    if (bad_start) {
	report_bad_sequence(dip, bad_start, (block - bad_start), record_offset);
	bad_start = 0;
    }
    if (good_start) {
	report_good_sequence(dip, good_start, (block - good_start), record_offset);
	good_start = 0;
    }
    Fprintf(dip, DT_FIELD_WIDTH "%d\n",
	    "Number of corrupted blocks", bad_blocks);
    Fprintf(dip, DT_FIELD_WIDTH "%d\n",
	    "Number of good blocks found", good_blocks);
    Fprintf(dip, DT_FIELD_WIDTH "%d\n",
	    "Number of zero blocks found", zero_blocks);
    return;
}

/* Loop through received data to determine if it contains any IOT data pattern. */
hbool_t
is_iot_data(dinfo_t *dip, uint8_t *rptr, size_t rsize, int rprefix_size, int *iot_offset, iotlba_t *rlbn)
{
    uint32_t received_word0, received_word1, received_iot_seed;
    int doff = (rprefix_size + sizeof(iotlba_t));
    int seed_word = 1;

    /* Format: <optional prefix><lbn or timestamp><lbn + IOT_SEED>...*/
    /* Loop through received data looking for a valid IOT seed. */
    for (; ((doff + sizeof(iotlba_t)) < rsize); doff += sizeof(iotlba_t) ) {
	received_word0 = get_lbn( (rptr + doff) );
	received_word1 = get_lbn( (rptr + doff + sizeof(iotlba_t)) );
	received_iot_seed = (received_word1 - received_word0);
	if ( (received_iot_seed && received_word0 && received_word1) &&
	     (received_iot_seed % IOT_SEED) == 0) {
	    /* Assume matches IOT data. */
	    if (iot_offset) *iot_offset = doff;
	    if (rlbn) *rlbn = (received_word0 - (received_iot_seed * seed_word));
	    return(True);
	}
	seed_word++;
    }
    return(False);
}

void
display_iot_block(dinfo_t *dip, int block, Offset_t block_offset,
		  uint8_t *pptr, uint8_t *vptr, uint32_t vindex,
		  size_t bsize, hbool_t good_data, hbool_t raw_flag)
{
    char str[LARGE_BUFFER_SIZE];
    char astr[PATH_BUFFER_SIZE];	/* Prefix strings can be rather long! */
    register char *sbp = str;
    register char *abp = astr;
    uint8_t *tend = NULL, *tptr = NULL;
    int aprefix_size = 0;
    int rprefix_size = 0;
    int raprefix_size = 0;
    int boff = 0, rindex, i;
    int match, width;
    register int bytes_per_line;
    int expected_width;
    uint32_t expected_lbn = 0, received_lbn = 0;
    size_t limit = MIN(bsize,dip->di_dump_limit);
    uint32_t received_word0, received_word1, received_iot_seed;
    int btag_size = 0;
    uint64_t lba = 0;
    uint8_t *eiot = pptr, *riot = vptr;
    btag_t *ebtag = NULL, *rbtag = NULL;

    if (dip->di_btag_flag == True) {
	ebtag = (btag_t *)pptr;
	rbtag = (btag_t *)vptr;
	btag_size = getBtagSize(ebtag);
	eiot += btag_size;
	riot += btag_size;
	if (raw_flag == False) {
	    /* Avoid flagging write time as a miscompare! */
	    ebtag->btag_write_secs = rbtag->btag_write_secs;
	    ebtag->btag_write_usecs = rbtag->btag_write_usecs;
	    if (good_data == True) {
		ebtag->btag_crc32 = rbtag->btag_crc32;
	    }
	}
    }
    Fprintf(dip, "\n");
    Fprintf(dip, DT_FIELD_WIDTH "%u (%s)\n", "Record Block",
	    block, (good_data == True) ? "good data" : "bad data" );
    lba = MapOffsetToLBA(dip, dip->di_fd, dip->di_lbdata_size, block_offset, MismatchedData);
    if (lba == NO_LBA) {
	Fprintf(dip, DT_FIELD_WIDTH FUF " (<not mapped>)\n", "Record Block Offset",
		block_offset);
    } else if (dip->di_fsmap) {
	uint64_t rlba = makeLBA(dip, block_offset);
	Fprintf(dip, DT_FIELD_WIDTH FUF " (Relative LBA "FUF", Physical LBA "FUF")\n",
		"Record Block Offset", block_offset, rlba, lba);
    } else {
	Fprintf(dip, DT_FIELD_WIDTH FUF " (LBA "FUF")\n", "Record Block Offset",
		block_offset, lba);
    }
    Fprintf(dip, DT_FIELD_WIDTH "%u (0x%x)\n", "Record Buffer Index", vindex, vindex);

    /* 
     * Verify and display the prefix string (if any).
     */
    if (dip->di_fprefix_size) {
	int result = 0;
	unsigned char byte;

	aprefix_size = (int)strlen(dip->di_fprefix_string);
	rprefix_size = dip->di_fprefix_size;
	/* 
	 * Note: The formatted prefix size includes the terminating NULL.
	 * and is also rounded up to the sizeof(unsigned int). Therefore,
	 * we are comparing the ASCII prefix string + NULL bytes!
	 */
	result = memcmp(eiot, riot, (size_t)dip->di_fprefix_size);
        Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Prefix String Compare",
		(result == 0) ? "correct" : "incorrect");
	if (eiot != pptr) {
            uint32_t prefix_offset = (uint32_t)(eiot - pptr);
	    Fprintf(dip, DT_FIELD_WIDTH "%u (0x%x)\n", "Prefix String Offset",
		    prefix_offset, prefix_offset);
	}
        						
	/* If the prefix is incorrect, display prefix information. */
	if (result != 0) {
	    int printable = 0;
	    abp = astr;
	    rindex = 0;
	    /* Note: IOT data can look printable, so check start of block. */
	    if ( is_iot_data(dip, riot, (sizeof(iotlba_t) * 3), rindex, NULL, NULL) == False ) {
		/* Ensure the received prefix string is printable. */
		for (rindex = 0; (rindex < aprefix_size); rindex++) {
		    byte = riot[rindex];
		    if (byte == '\0') break; /* Short prefix string. */
		    *abp++ = isprint((int)byte) ? (char)byte : ' ';
		    if ( isprint((int)byte) ) {
			printable++;
		    }
		}
	    }
	    if (rindex == 0) {
		/* We did NOT find a prefix string! */
		raprefix_size = rprefix_size = rindex;
	    } else if (rindex < aprefix_size) {
		/* The prefix string is shorter than the expected! */
		raprefix_size = rprefix_size = rindex;
		rprefix_size++; /* Include the terminating NULL. */
		rprefix_size = roundup(rprefix_size, sizeof(uint32_t));
	    } else if ( (rindex != aprefix_size) || (vptr[rindex] != '\0') ) {
		/* The prefix string is longer than the expected! */
		for (; (rindex < (int)bsize); rindex++) {
		    byte = riot[rindex];
		    if (byte == '\0') break; /* End of the prefix. */
		    *abp++ = isprint((int)byte) ? (char)byte : ' ';
		    if ( isprint((int)byte) ) {
			printable++;
		    }
		}
		/* Note: If prefix takes up the entire block, we need more work! */
		if (rindex < (int)bsize) {
		    raprefix_size = rprefix_size = rindex;
		    rprefix_size++; /* Include the terminating NULL. */
		    rprefix_size = roundup(rprefix_size, sizeof(uint32_t));
		} else { /* Assume this is NOT a prefix. */
		    raprefix_size = rprefix_size = 0;
		    printable = 0;
		}
	    } else { /* Expected and received are the same length! */
		raprefix_size = rindex;
	    }
	    Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Expected Prefix String", (pptr + btag_size));
	    Fprintf(dip, DT_FIELD_WIDTH, "Received Prefix String");
	    if (printable) {
		*abp = '\0';
		Fprint(dip, "%s\n", astr);
	    } else {
		Fprint(dip, "<non-printable string>\n");
	    }
	    if (rprefix_size != dip->di_fprefix_size) {
		Fprintf(dip, DT_FIELD_WIDTH "%d\n", "Expected Prefix Length", dip->di_fprefix_size);
		Fprintf(dip, DT_FIELD_WIDTH "%d\n", "Received Prefix Length", rprefix_size);
	    } else if (raprefix_size != aprefix_size) {
		  Fprintf(dip, DT_FIELD_WIDTH "%d\n", "Expected ASCII Prefix Length", aprefix_size);
		  Fprintf(dip, DT_FIELD_WIDTH "%d\n", "Received ASCII Prefix Length", raprefix_size);
	    }
	}
    } /* end 'if (dip->di_fprefix_size)...' */

    /* FYI: With btags and/or prefix strings, show the IOT offset. */
    if ( (eiot + dip->di_fprefix_size) != pptr) {
        uint8_t *iot = (eiot + dip->di_fprefix_size);
	uint32_t iot_offset = (uint32_t)(iot - pptr);
	Fprintf(dip, DT_FIELD_WIDTH "%u (0x%x)\n", "Expected IOT Data Offset",
		iot_offset, iot_offset);
    }

    /* Note: The pattern buffer *always* has the correct expected block number. */
    /* Yikes! This is NOT true with read-after-write w/timestamps is enabled! */
    if ( (dip->di_raw_flag == True) && (dip->di_timestamp_flag == True) ) {
	expected_lbn = (u_int32)(block_offset / dip->di_lbdata_size);
    } else {
	expected_lbn = get_lbn((eiot + dip->di_fprefix_size));
    }
    received_word0 = get_lbn( (riot + rprefix_size + (sizeof(received_lbn) * 1)) );
    received_word1 = get_lbn( (riot + rprefix_size + (sizeof(received_lbn) * 2)) );

    /* 
     * Process Timestamps (if any). 
     * Note: This is legacy timestamp support, block tags are now preferred! 
     */
    if (dip->di_timestamp_flag == True) {
	time_t seconds;
	tptr = (riot + rprefix_size);
	tend = (tptr + sizeof(iotlba_t));
	seconds = (time_t)stoh((riot + rprefix_size), sizeof(iotlba_t));
	/* Note: Big-Endian timestamp shown to match words dumped below! */
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x (actual 0x%08x)\n", "Block timestamp value",
		get_lbn( (riot + dip->di_fprefix_size) ), (uint32_t)seconds);
	/* Check for invalid time values, with upper limit including fudge factor! */
	if ( (seconds == (time_t)0) || (seconds > (time((time_t)0) + 300)) ) {
	    Fprintf(dip, DT_FIELD_WIDTH "%s\n",
		    "Data Block Written on", "<invalid time value>");
	} else {
	    Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Data Block Written on",
		    os_ctime(&seconds, dip->di_time_buffer, sizeof(dip->di_time_buffer)));
	}
	if ( seconds ) {
	    /* Remember: When writing, we may have raw,reread options enabled! */
	    if ( (dip->di_mode == WRITE_MODE) && (seconds < dip->di_write_pass_start) ) {
		/* Note: This occurs when we read stale data from the past! */
		Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Write Pass Start Time",
			os_ctime(&dip->di_write_pass_start, dip->di_time_buffer, sizeof(dip->di_time_buffer)));
	    } else if ( (dip->di_mode == READ_MODE) && (seconds > dip->di_read_pass_start) ) {
		/* Note: This is possible with wrong block data from another thread! */
		Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Read Pass Start Time",
			os_ctime(&dip->di_read_pass_start, dip->di_time_buffer, sizeof(dip->di_time_buffer)));
	    }
	}
	/*
	 * Since timestamp overwrites the lba, calculate the seed and lba.
	 */
	received_iot_seed = (received_word1 - received_word0);
	received_lbn = (received_word0 - received_iot_seed);
    } else {
	received_lbn = get_lbn((riot + rprefix_size));
	received_iot_seed = (received_word0 - received_lbn);
    } /* end 'if (dip->di_timestamp_flag)...' */

    Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n", "Expected Block Number", expected_lbn, expected_lbn);
    Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n", "Received Block Number", received_lbn, received_lbn);
    /* Report a little more information about the incorrect received block! */
    if (expected_lbn != received_lbn) {
	Offset_t received_offset = makeOffset(received_lbn, bsize);
	/* See if the received offset is within the expected data range. */
	if ( isDiskDevice(dip) &&
	     ( (received_offset < dip->di_file_position) ||
	       (received_offset > (dip->di_file_position + (Offset_t)dip->di_data_limit)) ) ) {
	    Fprintf(dip, DT_FIELD_WIDTH FUF " (%s Range: "FUF" - "FUF")\n", "Received Block Offset",
		    received_offset, (dip->di_slices) ? "Slice" : "Data",
		    dip->di_file_position, (dip->di_file_position + (Offset_t)dip->di_data_limit));
	} else {
	    Fprintf(dip, DT_FIELD_WIDTH FUF "\n", "Received Block Offset", received_offset);
	}
    }

    /*
     * Analyze the IOT data:
     * Steps:
     *  - Detect stale IOT data (most common case, past or future)
     * 	- Detect wrong IOT data (valid IOT data, but wrong block)
     * 	- Detect IOT data/seed anywhere within the data block.
     */
    if ( (expected_lbn != received_lbn) ||
	 (dip->di_iot_seed_per_pass != received_iot_seed) ) {
	/* Does this look like a valid IOT seed? */
	if ( (received_iot_seed && received_word0 && received_word1) &&
	     (received_word1 == (received_word0 + received_iot_seed)) ) {
	    /* Ok, this looks like valid IOT data, based on the seed. */
	    if (dip->di_pass_count < 256) {		/* Handle case where we wrap! */
		Fprintf(dip, DT_FIELD_WIDTH "%u\n",
			"Data Written During Pass", (received_iot_seed / IOT_SEED));
		Fprintf(dip, DT_FIELD_WIDTH "0x%08x (pass %u)\n",
			"Expected Data is for Seed",
			dip->di_iot_seed_per_pass, (dip->di_iot_seed_per_pass / IOT_SEED));
	    } else {
		Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
			"Expected Data is for Seed", dip->di_iot_seed_per_pass);
	    }
	    Fprintf(dip, DT_FIELD_WIDTH "0x%08x (%s)\n",
		    "Received Data is from Seed", received_iot_seed,
		    (expected_lbn == received_lbn) ? "stale data" : "wrong data");
	} else { /* Let's search for the IOT seed! */
	    /* Format: <optional prefix><lbn or timestamp><lbn + IOT_SEED>...*/
            uint8_t *rptr = (riot + rprefix_size);
	    int doff = sizeof(expected_lbn); /* Offset to 1st IOT data word. */
	    int seed_word = 1;
	    /* Loop through data looking for a valid IOT seed. */
	    for (; ((doff + sizeof(expected_lbn)) < bsize); doff += sizeof(expected_lbn) ) {
        	/* Traverse loooking at word pairs looking for modulo IOT seed for validity. */
		uint32_t iot_word0 = get_lbn( (rptr + doff) );
		uint32_t iot_word1 = get_lbn( (rptr + doff + sizeof(expected_lbn)) );
		uint32_t iot_seed = (iot_word1 - iot_word0);
        	/* Note: The accuracy depends on how munged the received data is! */
		if ( (iot_seed && iot_word0 && iot_word1) &&
		     ( (iot_seed % IOT_SEED) == 0) ||
		       (iot_seed == (dip->di_iot_seed_per_pass - IOT_SEED)) ) {
        	    uint32_t boff = (uint32_t)( (rptr + doff) - vptr);
		    uint32_t calculated_lbn = (iot_word0 - (iot_seed * seed_word));
		    Fprintf(dip, DT_FIELD_WIDTH "%u (0x%x) (word index %u)\n",
			    "Seed Detected at Offset", boff, boff, seed_word);
        	    /* Note: This is inaccurate when we exceed upper threshold. */
        	    /* We multiply the IOT seed (0x01010101) by the pass count. */
		    if (dip->di_pass_count < 256) { /* Handle case where we wrap! */
			Fprintf(dip, DT_FIELD_WIDTH "%u\n",
				"Data Written During Pass", (iot_seed / IOT_SEED));
		    }
		    Fprintf(dip, DT_FIELD_WIDTH "%u (0x%08x)\n",
			    "Calculated Block Number",
			    calculated_lbn, calculated_lbn);
        	    /* If the initial and calculated seeds differ, warn user (and me)! */
		    if (iot_seed != received_iot_seed) {
			Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
				"Calculated Initial Seed", received_iot_seed);
		    }
		    if (dip->di_pass_count < 256) { /* Handle case where we wrap! */
			Fprintf(dip, DT_FIELD_WIDTH "0x%08x (pass %u)\n",
				"Expected Data is for Seed",
				dip->di_iot_seed_per_pass, (dip->di_iot_seed_per_pass / IOT_SEED));
		    } else {
			Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
				"Expected Data is for Seed", dip->di_iot_seed_per_pass);
		    }
		    /* Since part of the block is corrupt, always report wrong data. */
		    Fprintf(dip, DT_FIELD_WIDTH "0x%08x (%s)\n",
			    "Received Data is from Seed", iot_seed, "wrong data");
		    break; /* Stop upon 1st valid IOT data. */
		}
		seed_word++;
	    }
	}
    }
    
    if (dip->di_btag_flag == True) {
	report_btag(dip, ebtag, rbtag, raw_flag);
    } else {
	Fprintf(dip, "\n");
    }

    /* 
     * Format and display the IOT data. 
     */
    width = sprintf(sbp, "Byte Expected: address "LLPXFMT, (ptr_t)pptr);
    sbp += width;
    if (dip->di_data_format == BYTE_FMT) {
	expected_width = BYTE_EXPECTED_WIDTH;
    } else {
	expected_width = WORD_EXPECTED_WIDTH;
    }
    while (width++ < expected_width) {
	sbp += sprintf(sbp, " ");
    }
    sbp += sprintf(sbp, "Received: address "LLPXFMT"\n", (ptr_t)vptr); 
    Fprintf(dip, "%s", str);

    while (limit > 0) {
	sbp = str;
	bytes_per_line = (int)MIN(bsize, BYTES_PER_LINE);
	if (dip->di_boff_format == DEC_FMT) {
	    sbp += sprintf(sbp, "%04d ", boff);
	} else {
	    sbp += sprintf(sbp, "%04x ", boff);
	}
	abp = NULL;
	/* Prefix is displayed in hex and ASCII on next line. */
	if ( aprefix_size &&
	     (boff >= btag_size) && ((boff - btag_size) < aprefix_size) ) {
	    abp = astr;
	    abp += sprintf(abp, "     ");
	}
	/* Handle the timestamp within this byte range. */
	if (tptr && (vptr < tend)) {
	    uint8_t *pp = pptr;
	    uint8_t *vp = vptr;
	    match = 0;
	    for (i = 0; (i < bytes_per_line); i++, pp++, vp++) {
		/* Check for and skip the timestamp. */
		if ( (vp >= tptr) && (vp < tend) ) {
		    continue; /* Skip the timestamp. */
		}
		if ( *pp != *vp ) {
		    match = 1; /* mismatch! */
		    break;
		}
	    }
	} else {
	    match = memcmp(pptr, vptr, bytes_per_line);
	}
	if (dip->di_data_format == BYTE_FMT) {
	    unsigned char byte;
	    for (i = 0; (i < bytes_per_line); i++) {
		byte = pptr[i];
		sbp += sprintf(sbp, "%02x ", byte);
		if (abp) abp += sprintf(abp, " %c ", isprint((int)byte) ? byte : ' ');
	    }
	    sbp += sprintf(sbp, "%c ", (match == 0) ? ' ' : '*');
	    if (abp) abp += sprintf(abp, "  ");
	    for (i = 0; (i < bytes_per_line); i++) {
		byte = vptr[i];
		sbp += sprintf(sbp, "%02x ", byte);
		if (abp) abp += sprintf(abp, " %c ", isprint((int)byte) ? byte : ' ');
	    }
	} else {
	    u_int32 data;
	    for (i = 0; (i < bytes_per_line); i += sizeof(data)) {
		data = get_lbn((pptr + i));
		sbp += sprintf(sbp, "%08x ", data);
		if (abp) {
		    unsigned char byte;
		    int x = sizeof(data);
		    while (x--) {
			byte = (data >> (x * BITS_PER_BYTE));
			abp += sprintf(abp, " %c", isprint((int)byte) ? byte : ' ');
		    }
		    abp += sprintf(abp, " ");
		}
	    }
	    sbp += sprintf(sbp, "%c ", (match == 0) ? ' ' : '*');
	    if (abp) abp += sprintf(abp, "  ");
	    for (i = 0; (i < bytes_per_line); i += sizeof(data)) {
		data = get_lbn((vptr + i));
		sbp += sprintf(sbp, "%08x ", data);
		if (abp) {
		    unsigned char byte;
		    int x = sizeof(data);
		    while (x--) {
			byte = (data >> (x * BITS_PER_BYTE));
			abp += sprintf(abp, " %c", isprint((int)byte) ? byte : ' ');
		    }
		    abp += sprintf(abp, " ");
		}
	    }
	}
	sbp += sprintf(sbp, "\n");
        Fprintf(dip, str);
	if (abp) {
	    abp += sprintf(abp, "\n");
            Fprintf(dip, astr);
	}
	limit -= bytes_per_line;
	boff += bytes_per_line;
	pptr += bytes_per_line;
	vptr += bytes_per_line;
    }
    return;
}

int
compare_iot_block(dinfo_t *dip, uint8_t *pptr, uint8_t *vptr, hbool_t raw_flag)
{
    int result = 0;

    if (dip->di_btag_flag == True) {
	btag_t *ebtag = (btag_t *)pptr;
	btag_t *rbtag = (btag_t *)vptr;
	result = verify_btags(dip, ebtag, rbtag, NULL, raw_flag);
	if ( (result == SUCCESS) && dip->di_xcompare_flag && dip->di_fprefix_size ) {
	    result = verify_btag_prefix(dip, ebtag, rbtag, NULL);
	}
    } else {
	if (dip->di_timestamp_flag) {
	    if (dip->di_fprefix_size) {
		result = memcmp(pptr, vptr, dip->di_fprefix_size);
	    }
	    if (result == 0) {
		/* Note: The timestamp overwrites the LBA! */
		int doff = (dip->di_fprefix_size + sizeof(iotlba_t));
		result = memcmp( (pptr + doff), (vptr + doff),
				 (dip->di_lbdata_size - doff) );
	    }
	} else {
	    result = memcmp(pptr, vptr, dip->di_lbdata_size);
	}
    }
    return(result);
}

void
display_iot_data(dinfo_t *dip, uint8_t *pbuffer, uint8_t *vbuffer, size_t bcount, hbool_t raw_flag)
{
    uint8_t *pptr = pbuffer;
    uint8_t *vptr = vbuffer;
    size_t count = bcount;
    lbdata_t dsize = dip->di_lbdata_size;
    int block = 0, blocks = (int)(count / dsize);
    unsigned int bad_blocks = 0, good_blocks = 0;
    Offset_t block_offset, record_offset, ending_offset;
    large_t starting_lba, ending_lba;
    uint32_t vindex = 0;

    /* Note: Use dt's offset rather than the OS fd offset (for now)! */
    block_offset = record_offset = getFileOffset(dip);

    Fprintf(dip, "\n");
    Fprintf(dip, DT_FIELD_WIDTH "%lu\n", "Record #", (dip->di_records_read + 1));
    Fprintf(dip, DT_FIELD_WIDTH FUF "\n", "Starting Record Offset", record_offset);
    Fprintf(dip, DT_FIELD_WIDTH "%u (%#x)\n", "Transfer Count", bcount, bcount);

    ending_offset = (record_offset + bcount);
    Fprintf(dip, DT_FIELD_WIDTH FUF "\n", "Ending Record Offset", ending_offset);

    if (dip->di_fsmap) {
	starting_lba =  MapOffsetToLBA(dip, dip->di_fd, dsize, record_offset, MismatchedData);
	ending_lba =  MapOffsetToLBA(dip, dip->di_fd, dsize, (ending_offset - 1), MismatchedData);
        /* Starting LBA */
	if (starting_lba == NO_LBA) {
	    Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Starting Physical LBA", notmapped_str);
	} else {
	    Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n", "Starting Physical LBA", starting_lba, starting_lba);
	}
        /* Ending LBA */
	if (ending_lba == NO_LBA) {
	    Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Ending Physical LBA", notmapped_str);
	} else {
	    Fprintf(dip, DT_FIELD_WIDTH LUF" ("LXF")\n", "Ending Physical LBA", ending_lba, ending_lba);
	}
    }
    /*
     * Note: Report relative file system information.
     */
    /* Original non-FS map display. */
    starting_lba = (record_offset / dsize);
    ending_lba = ((starting_lba + howmany(bcount, dsize)) - 1);
    if ( isFileSystemFile(dip) ) {
	Fprintf(dip, DT_FIELD_WIDTH LUF" - "LUF"\n", "Relative Record Block Range", starting_lba, ending_lba);
    } else {
	Fprintf(dip, DT_FIELD_WIDTH LUF" - "LUF"\n", "Record Block Range", starting_lba, ending_lba);
    }
    Fprintf(dip, DT_FIELD_WIDTH LLPXFMT "\n", "Read Buffer Address", vptr);
    Fprintf(dip, DT_FIELD_WIDTH LLPXFMT "\n", "Pattern Base Address", pptr);
    if (dip->di_fprefix_size) {
	int aprefix_size = (int)strlen(dip->di_fprefix_string);
        Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Prefix String", dip->di_fprefix_string);
        Fprintf(dip, DT_FIELD_WIDTH "%d bytes (0x%x) plus %d zero bytes\n", "Prefix length",
		dip->di_fprefix_size, dip->di_fprefix_size, dip->di_fprefix_size - aprefix_size);
    }
    Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Note", "Incorrect data is marked with asterisk '*'");

    /*
     * Compare one lbdata sized block at a time.
     * 
     * TODO: This does NOT handle any partial IOT blocks! (assumes full IOT data blocks)
     * This is *not* generally a problem, but partial blocks can occur with file system full,
     * and the file offset is not modulo the block size (crossing file system blocks).
     */
    while (blocks) {
	int result = compare_iot_block(dip, pptr, vptr, raw_flag);
	if (result == 0) {
	    int nresult;
	    hbool_t context_flag = False;
	    if ( (dip->di_dump_context_flag == True) && ((blocks - 1) > 0) ) {
		/* Verify the next block for good/bad to set context. */
		nresult = compare_iot_block(dip, (pptr + dsize),
					    (vptr + dsize), raw_flag);
		if (nresult != 0) {
		    context_flag = True;	/* Next block is bad, display good! */
		}
	    }
	    if ( (dip->di_dumpall_flag == True) || (context_flag == True) ) {
		display_iot_block(dip, block, block_offset, pptr, vptr, vindex,
				  dsize, True, raw_flag);
	    }
	} else {
	    if ( (dip->di_dumpall_flag == True) ||
		 (dip->di_max_bad_blocks && (bad_blocks < dip->di_max_bad_blocks) )) {
		display_iot_block(dip, block, block_offset, pptr, vptr, vindex,
				  dsize, False, raw_flag);
	    }
	    bad_blocks++;
	}
	block++;
	blocks--;
	pptr += dsize;
	vptr += dsize;
        vindex += dsize;
	block_offset += dsize;
    }
    /*
     * Warn user (including me), that some of the IOT data was NOT displayed!
     */ 
    if ((count % dsize) != 0) {
	Fprint(dip, "\n");
	Wprintf(dip, "A partial IOT data block of %u bytes was NOT displayed!\n", (count % dsize) );
    }
    return;
}
