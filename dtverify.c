/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2018			    *
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
 * Module:	dtverify.c
 * Author:	Robin T. Miller
 * Date:	August 17th, 2013
 *
 * Description:
 *	Data Verification functions.
 *
 * Modification History:
 *
 * APril 30th, 2018 by Robin T. Miller
 *      Added extra compare flag, to control btag prefix verification.
 * Note: The btags is usually sufficient, extra compare is for my debug.
 * 
 * April 26th, 2018 by Robin T. Miller
 *      When reading only, if the btag is valid, also verify the prefix
 * string (if any), for higher validation. While this is not fullproof,
 * it does catch valid btags with an incorrect prefix strings.
 * 
 * June 7th, 2015 by Robin T. Miller
 * 	Adding support for verifying block tags (btags).
 * 	Updated dumping buffers display all data if less than dump limit.
 *
 * April 25th, 2015 by Robin T. Miller
 * 	During verify retries, alwasy display the dt reread command line,
 * but disable compare if pattern options do not permit data verification.
 * 
 * April 21st, 2015 by Robin T. Miller
 * 	Modified dump_buffer() to use log buffer to consolidate output.
 *
 * April 20th, 2015 by Robin T. Miller
 * 	For file systems, during errors report the file identifier (i-node).
 *
 * April 29th, 2014 by Robin T. Miller
 * 	WHen rereading after a corruption, for Linux NFS file systems, do
 * *not* disable Direct I/O (DIO) if the I/O size of offset is not modulo
 * the block size. For SAN on local file systems, these *must* be aligned,
 * or else EINVAL is returned on the read().
 * 
 * August 17th, 2013 by Robin T Miller
 * 	Moving verify functions here.
 */
#include "dt.h"
#include <ctype.h>

/*
 * Forward References:
 */
int verify_prefix(struct dinfo *dip, u_char *buffer, size_t bcount, int bindex, size_t *pcount);

static void report_reread_command(dinfo_t *dip, size_t request_size, Offset_t record_offset, uint32_t pattern);
static int verify_data_with_btags(	struct dinfo	*dip,
					uint8_t		*buffer,
					size_t		bytes,
					uint32_t	pattern,
					lbdata_t	*lba,
					hbool_t		raw_flag );
static int verify_data_normal(	struct dinfo	*dip,
				u_char		*buffer,
				size_t		bcount,
				u_int32		pattern,
				hbool_t		raw_flag );
static int verify_data_prefix(	struct dinfo	*dip,
				u_char		*buffer,
				size_t		bcount,
				u_int32		pattern,
				hbool_t		raw_flag );
static int verify_data_with_lba(struct dinfo	*dip,
				u_char		*buffer,
				size_t		bcount,
				u_int32		pattern,
				u_int32		*lba,
				hbool_t		raw_flag );

static size_t CalculateDumpSize(dinfo_t *dip, size_t size);
static int dopad_verify( struct dinfo	*dip,
			 u_char		*buffer,
			 size_t		offset,
			 u_int32	pattern,
			 size_t		pbytes,
			 size_t		pindex,
			 hbool_t	inverted );

#if 0
static char *pad_str =		"Pad";
#endif
static char *lba_str =          "Lba";
static char *data_str =		"Data";
static char *btag_str =		"Block Tag";
static char *pattern_str =	"Pattern";
static char *prefix_str =	"Prefix";
static char *verify_str =	"Verify";

static char *compare_error_str =	"Data compare error at byte";

/************************************************************************
 *									*
 * CalculateDumpSize() - Calculate the number of data bytes to dump.	*
 *									*
 * Description:								*
 *	For non-memory mapped files, we'll dump the pad bytes.  These	*
 * pad bytes do not exist for memory mapped files which are directly	*
 * mapped to memory addresses.						*
 *									*
 * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	size = The size of the buffer to dump.				*
 *									*
 * Outputs:	Size of buffer to dump.					*
 *									*
 ************************************************************************/
static size_t
CalculateDumpSize(dinfo_t *dip, size_t size)
{
    size_t dump_size = size;

    if (dip->di_mmap_flag == False) {
	dump_size += PADBUFR_SIZE;
    }
    if (dump_size > dip->di_data_size) {
	dump_size = dip->di_data_size;
    }
    return(dump_size);
}

/************************************************************************
 *									*
 * dump_buffer() Dump data buffer in hex bytes.				*
 *									*
 * Inputs:							        * 
 * 	dip = The device infromation pointer.				*
 *	name = The buffer name being dumped.				*
 *	base = Base pointer of buffer to dump.				*
 *	ptr  = Pointer into buffer being dumped.			*
 *	dump_size = The size of the buffer to dump.			*
 *	bufr_size = The maximum size of this buffer.			*
 *	expected = Boolean flag (True = Expected).			*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
dump_buffer (	dinfo_t		*dip,
		char		*name,
		uint8_t		*base,
		uint8_t		*ptr,
		size_t		dump_size,
		size_t		bufr_size,
		hbool_t		expected )
{
    size_t boff, coff, limit, offset;
    u_int field_width = 16;
    uint8_t *bend;
    uint8_t *bptr;
    uint8_t data;
    uint8_t *abufp, *abp;
    size_t dindex;

    if ( (base == NULL) || (ptr == NULL) ) {
	/* Avoid strange segmentation faults, making debug more difficult! */
	Eprintf(dip, "BUG: The base "LLPX0FMT" and/or dump buffer"LLPX0FMT", are NULL!\n",
		base, ptr);
	return;
    }
    bend = (base + bufr_size);
    bptr = base;
    dindex = (ptr - base);

    abufp = abp = (u_char *)Malloc(dip, (field_width + 1) );
    /*
     * Since many requests do large transfers, limit data dumped.
     */
    limit = (dump_size < dip->di_dump_limit) ? dump_size : dip->di_dump_limit;

    /*
     * Now to provide context, attempt to dump data on both sides of
     * the corrupted data, ensuring buffer limits are not exceeded.
     * Note: Only adjust the dumping, if index is > dump limit!
     */
    if (dindex > limit) {
	bptr = (ptr - (limit >> 1));
	if (bptr < base) bptr = base;
	if ( (bptr + limit) > bend) {
	    limit = (bend - bptr);		/* Dump to end of buffer. */
	}
    }
    offset = (ptr - base);		/* Offset to failing data. */
    coff = (ptr - bptr);		/* Offset from dump start. */

    /*
     * Note: Rotate parameters are not displayed since we don't have
     * the base data address and can't use global due to AIO design.
     * [ if I get ambitious, I'll correct this in a future release. ]
     */
    Lprintf(dip, "The %scorrect data starts at address "LLPXFMT" (marked by asterisk '*')\n",
							(expected) ? "" : "in", ptr);
    Lprintf(dip, "Dumping %s Buffer (base = "LLPXFMT", buffer offset = %u, limit = %u bytes):\n",
							name, base, offset, limit);
    /* Note: 64-bit look strange, but I'd rather not truncate the address below. */
    /*       Generally, I prefer to compile for 32 bits, with 64 bits offsets. */
#if defined(MACHINE_64BITS)
    Lprintf(dip, "          Address / Offset\n");
#else /* !defined(MACHINE_64BITS) */
    Lprintf(dip, "  Address / Offset\n");
#endif /* defined(MACHINE_64BITS) */

    /*
     * Note: This may be deprecated with new side by side comparision, but at 
     * present it's needed for non-IOT patterns, and also provides context. 
     */
    for (boff = 0; boff < limit; boff++, bptr++) {
	if ((boff % field_width) == (size_t) 0) {
	    if (boff) Lprintf(dip, " \"%s\"\n", abufp); abp = abufp;
	    Lprintf(dip, LLPX0FMT"/%6d |",
		   bptr, (boff + (offset - coff)));
	}
	data = *bptr;
	Lprintf(dip, "%c%02x", (bptr == ptr) ? '*' : ' ', data);
        abp += Sprintf((char *)abp, "%c", isprint((int)data) ? data : ' ');
    }
    if (abp != abufp) {
        while (boff++ % field_width) Lprintf(dip, "   ");
        Lprintf(dip, " \"%s\"\n", abufp);
    }
    Free(dip, abufp);
    if (expected) {
	Lprintf(dip, "\n");
    }
    eLflush(dip);
    return;
}

#if defined(TIMESTAMP)

void
display_timestamp(dinfo_t *dip, u_char *buffer)
{
    time_t seconds;

    seconds = (time_t)stoh(buffer, sizeof(iotlba_t));
    Fprintf(dip, "The data block was written on %s\n",
	    os_ctime(&seconds, dip->di_time_buffer, sizeof(dip->di_time_buffer)));
    return;
}
#endif /* defined(TIMESTAMP) */

/*
 * verify_prefix() - Verify Buffer with Prefix String.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	buffer = Address of buffer to verify.
 *	bcount = The full buffer count.
 *	bindex = The index into the buffer.
 *	pcount = Pointer to return prefix count verified.
 *
 * Outputs:
 *	pcount gets the prefix string count verified.
 *	Return value is Success or Failure.
 */
int
verify_prefix( struct dinfo *dip, u_char *buffer, size_t bcount, int bindex, size_t *pcount )
{
    register u_char *bptr = buffer;
    register u_char *pstr = (u_char *)dip->di_fprefix_string;
    register size_t count;
    register size_t i;
    int status = SUCCESS;

    count = MIN((size_t)dip->di_fprefix_size, (bcount - bindex));

    for (i = 0; (i < count); i++, bptr++, pstr++) {
	if (*bptr != *pstr) {
	    size_t dump_size;
	    ReportCompareError(dip, bcount, (u_int)(bindex + i), *pstr, *bptr);
	    Fprintf(dip, "Mismatch of data pattern prefix: '%s' (%d bytes w/pad)\n",
		    dip->di_fprefix_string, dip->di_fprefix_size);
	    /* expected */
	    dump_size = CalculateDumpSize(dip, dip->di_fprefix_size);
	    dump_buffer(dip, prefix_str, (u_char *)dip->di_fprefix_string,
			pstr, dump_size, dip->di_fprefix_size, True);
	    /* received */
#if defined(TIMESTAMP)
            if (dip->di_timestamp_flag) {
                display_timestamp(dip, buffer+count);
            }
#endif /* defined(TIMESTAMP) */
	    dump_size = CalculateDumpSize (dip, bcount);
	    dump_buffer (dip, data_str, buffer, bptr, dump_size, bcount, False);
	    status = FAILURE;
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
		(void)ExecuteTrigger(dip, miscompare_op);
	    }
	    break;
	}
    }
    *pcount = count;
    return (status);
}

/************************************************************************
 *									*
 * verify_buffers() - Verify Data Buffers.				*
 *									*
 * Description:								*
 *	Simple verification of two data buffers.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		dbuffer = Data buffer to verify with.			*
 *		vbuffer = Verification buffer to use.			*
 *		count = The number of bytes to compare.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Data Ok/Compare Error.	*
 *									*
 ************************************************************************/
int
verify_buffers(	struct dinfo	*dip,
		u_char		*dbuffer,
		u_char		*vbuffer,
		size_t		count )
{
    u_int32 i;
    u_char *dptr = dbuffer;
    u_char *vptr = vbuffer;

    for (i = 0; (i < count); i++, dptr++, vptr++) {
	if (*dptr != *vptr) {
	    size_t dump_size = CalculateDumpSize (dip, count);
	    ReportCompareError (dip, count, i, *dptr, *vptr);
	    /* expected */
	    dump_buffer (dip, data_str, dbuffer, dptr, dump_size, count, True);
	    /* received */
#if defined(TIMESTAMP)
            if (dip->di_timestamp_flag) {
                display_timestamp(dip, vbuffer);
            }
#endif /* defined(TIMESTAMP) */
	    dump_buffer (dip, verify_str, vbuffer, vptr, dump_size, count, False);
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
		(void)ExecuteTrigger(dip, miscompare_op);
	    }
	    return (FAILURE);
	}
    }
    return (SUCCESS);
}

/************************************************************************
 *									*
 * verify_lbdata() - Verify Logical Block Address in Buffer.            *
 *									*
 * Description:								*
 *	Note: This function is used during read-after-write tests.      *
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		dbuffer = Data buffer to verify with.			*
 *		vbuffer = Verification buffer to use.			*
 *		count = The number of bytes to compare.			*
 *              lba = Pointer to return last lba verified.              *
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = lba Ok/Compare Error. 	*
 *									*
 ************************************************************************/
int
verify_lbdata(	struct dinfo	*dip,
		u_char		*dbuffer,
		u_char		*vbuffer,
		size_t		count,
		u_int32		*lba )
{
    u_int32 i, dlbn = 0, vlbn;
    u_char *dptr = dbuffer;
    u_char *vptr = vbuffer;
    int status = SUCCESS;

    /*
     * Note: With timestamps enabled, we overwrite the lba.
     */
    if (dip->di_timestamp_flag) { return (status); }
    for (i = 0; (i+sizeof(dlbn) <= count); i += dip->di_lbdata_size,
		 dptr += dip->di_lbdata_size, vptr += dip->di_lbdata_size) {
	if (dip->di_iot_pattern) {
	    dlbn = get_lbn(dptr);
	    vlbn = get_lbn(vptr);
	} else {
	    dlbn = (u_int32)stoh(dptr, sizeof(dlbn));
	    vlbn = (u_int32)stoh(vptr, sizeof(vlbn));
	}
	if (dlbn != vlbn) {
	    size_t dump_size = CalculateDumpSize (dip, count);
	    ReportLbdataError(dip, *lba, (uint32_t)count, i, dlbn, vlbn);
	    /* expected */
	    dump_buffer (dip, data_str, dbuffer, dptr, dump_size, count, True);
	    /* received */
	    dump_buffer (dip, verify_str, vbuffer, vptr, dump_size, count, False);
	    status = FAILURE;
	    break;
	}
    }
    *lba = (dlbn + 1);
    return (status);
}

/************************************************************************
 *									*
 * verify_data() - Verify Data Pattern.					*
 *									*
 * Description:								*
 *	If a pattern_buffer exists, then this data is used to compare	*
 * the buffer instead of the pattern specified.				*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		buffer = Pointer to data to verify.			*
 *		count = The number of bytes to compare.			*
 *		pattern = Data pattern to compare against.		*
 * 		lba = Pointer to starting logical block address.        * 
 * 		raw_flag = Performing read-after-write flag.		* 
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Data Ok/Compare Error.	*
 *		lba gets updated with the next lba to verify with.	*
 *									*
 ************************************************************************/
int
verify_data (	struct dinfo	*dip,
		uint8_t		*buffer,
		size_t		count,
		uint32_t	pattern,
		uint32_t	*lba,
		hbool_t		raw_flag )
{
    hbool_t check_lba = (dip->di_iot_pattern || (dip->di_lbdata_flag && dip->di_lbdata_size));
    int status;

    if (dip->di_btag_flag == True) {
	status = verify_data_with_btags(dip, buffer, count, pattern, lba, raw_flag);
    } else if ( (check_lba == False) && (dip->di_fprefix_string == NULL) ) {
	status = verify_data_normal(dip, buffer, count, pattern, raw_flag);
    } else if ( (check_lba == False) && dip->di_fprefix_string ) {
	status = verify_data_prefix(dip, buffer, count, pattern, raw_flag);
    } else {
	status = verify_data_with_lba(dip, buffer, count, pattern, lba, raw_flag);
    }
    if ( (status != SUCCESS)   && dip->di_retryDC_flag &&
	 dip->di_random_access && (dip->di_retrying == False)) {
	(void)verify_reread(dip, buffer, count, pattern, lba);
    }
    return (status);
}

/*
 * verify_reread() - Verify Data after Rereading with DIO.
 *
 * Description:
 *	If a pattern_buffer exists, then this data is used to compare
 * the buffer instead of the pattern specified.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	buffer = Pointer to data to verify.
 *	count = The number of bytes to compare.
 *	pattern = Data pattern to compare against.
 *	lba = Pointer to starting logical block address.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE = Data Ok/Compare Error.
 *	lba gets updated with the next lba to verify with.
 */
int
verify_reread(
    struct dinfo	*cdip,
    u_char		*buffer,
    size_t		bcount,
    u_int32		pattern,
    u_int32		*lba )
{
    dinfo_t	*dip;
    uint8_t	*reread_buffer;
    Offset_t 	record_offset;
    ssize_t 	count;
    int		oflags;
    int		retries = 0;
    hbool_t	saved_aio_flag;
    hbool_t	saved_dio_flag;
    hbool_t	saved_rDebugFlag;
    int		status;

    /*
     * Please Note: For SAN disks, using a SCSI Read may be desirable?
     * Also Note: Dated code, overdue for a cleanup/rewrite (IMHO).
     */
    Fprintf(cdip, "\n");
    if ( (cdip->di_dtype->dt_dtype == DT_REGULAR) ||
	 (cdip->di_dtype->dt_dtype == DT_BLOCK) ) {
	Fprintf(cdip, "Rereading and verifying record data using Direct I/O...\n");
    } else {
	Fprintf(cdip, "Rereading and verifying record data...\n");
    }
    cdip->di_retrying = True;

    /*
     * We have a bit of work to reread the data.
     */
    dip = Malloc(cdip, sizeof(*dip));
    if (dip == NULL) goto cleanup_exit;
    reread_buffer = malloc_palign(cdip, bcount, 0);
    if (reread_buffer == NULL) goto cleanup_exit;
    *dip = *cdip;	 /* Copy current device information. */
    dip->di_fd = NoFd;
    record_offset = cdip->di_offset;
    /* Note: We've switched to pread()/pwrite() so... */
    //record_offset = get_current_offset(cdip, (ssize_t)bcount);
    oflags = OS_READONLY_MODE;

    /*
     * Enable direct I/O for our reread (bypass buffer cache).
     */
    saved_aio_flag = dip->di_aio_flag;
    saved_dio_flag = dip->di_dio_flag;
    dip->di_aio_flag = False;	/* Avoid overlapped attribute Windows. */
    /*
     * Only enable Direct I/O for regular files, not raw disks!
     */
    if ( (dip->di_dtype->dt_dtype == DT_REGULAR) ||
	 (dip->di_dtype->dt_dtype == DT_BLOCK) ) {
	/*
	 * For Linux and Windows DIO, the buffer, count, and offset must be
	 * block aligned/sized to avoid errors:
	 *  Linux:    dt: 'read', errno = 22 - Invalid argument
	 *  Windows: 'SetFilePointer', errno = 87 - The parameter is incorrect.
	 *  Others? Solaris, FreeBSD, etc?
	 * OS's like Tru64 and HP-UX have special fixup code so Ok.
	 */
#if defined(__linux__) || defined(WIN32)
	/* Mickey Mouse, trying to avoid SAN DIO read failures! */
	if ( (dip->di_bypass_flag == False) &&
	     (bcount % BLOCK_SIZE) || (record_offset % BLOCK_SIZE) ) {
	    if (dip->di_filesystem_type && EQS(dip->di_filesystem_type, "nfs")) {
		;
	    } else {
		dip->di_dio_flag = False;
		Wprintf(dip, "The I/O size or offset is NOT block aligned, so DIO is disabled!\n");
	    }
	} else 
#endif /* defined(__linux__) || defined(WIN32) */
	{
	    dip->di_dio_flag = True;    /* Valid for Solaris & native Windows. */
#if defined(O_DIRECT)
	    oflags |= O_DIRECT;		/* Direct disk access. */
#endif
	}
#if defined(__linux__)
    } else if (dip->di_dtype->dt_dtype == DT_DISK) {
	  /* Linux disks are block devices, grrrr! */
	  oflags |= O_DIRECT;		/* Bypass the buffer cache, please! */
#endif /* defined(__linux__) */
    }

    /*
     * Steps/Logic:
     * - open the device/file (again)
     * - seek to the failing offset
     * - reread the record data
     * - verify against previous read data (verify == write error)
     * - verify against expected data (verify == read error)
     */
    status = (*dip->di_funcs->tf_reopen_file)(dip, oflags);
    if (status != SUCCESS) goto cleanup_exit;
    saved_rDebugFlag = dip->di_rDebugFlag;
    dip->di_rDebugFlag = True;		/* Report our seek info. */

    do {
#if 0
	/* Note: Should NOT need since we switched to pread()/pwrite() API's! */
	if (dip->di_scsi_io_flag == False) {
	    if ( (dip->di_offset = set_position(dip, record_offset, False)) != record_offset) {
		Fprintf(dip, "Seek failed, exiting...\n");
		status = FAILURE;
		goto cleanup_exit;
	    }
	}
#endif /* 0 */
	report_record(dip, (dip->di_files_read + 1), (dip->di_records_read + 1),
			   (record_offset / dip->di_dsize), record_offset,
			   READ_MODE, reread_buffer, bcount);
	count = read_record(dip, reread_buffer, bcount, bcount, record_offset, &status);
	if (status == FAILURE) goto cleanup_exit;

	/*
	 * Now, compare reread data to previous read, and if that
	 * fails, verify against the expect pattern data, to help
	 * make a decision on where the failure occurred.
	 */
	if (dip->di_saved_pattern_ptr) {
	    dip->di_pattern_bufptr = dip->di_saved_pattern_ptr;	/* Messy, cleanup! */
	}
	if (memcmp(buffer, reread_buffer, count) == 0) {
	    Fprintf(dip, "Reread data matches previous data read, possible write failure!\n");
	} else {
	    status = (*dip->di_funcs->tf_verify_data)(dip, reread_buffer, count, pattern, lba, False);
	    if (status == SUCCESS) {
		Fprintf(dip, "Reread data matches the expected data, possible read failure!\n");
	    } else {
		Fprintf(dip, "Reread data does NOT match previous data or expected data!\n");
	    }
	}
	if (dip->di_loop_on_error) {

	    PAUSE_THREAD(dip);
	    if ( THREAD_TERMINATING(dip) ) break;
	    if (dip->di_terminating) break;

	    retries++;
	    Fprintf(dip, "Delaying %u seconds after retry %d...\n", (dip->di_retry_delay * retries), retries);
	    SleepSecs(dip, (dip->di_retry_delay * retries) );
	}
    } while ( dip->di_loop_on_error );

    /*
     * Ok, we're done... cleanup!
     */
    status = (*dip->di_funcs->tf_close)(dip);

cleanup_exit:
    cdip->di_retrying = False;
    report_reread_command(dip, bcount, record_offset, pattern);
#if defined(DATA_CORRUPTION_URL)
    Fprintf(dip, "Note: For more information regarding data corruptions, please visit this link:\n");
    Fprintf(dip, "    %s\n", DATA_CORRUPTION_URL);
    Fprintf(dip, "\n");
#endif /* defined(DATA_CORRUPTION_URL) */
    /* Due to copy, don't really need this restore! */
    dip->di_aio_flag = saved_aio_flag;
    dip->di_dio_flag = saved_dio_flag;
    dip->di_rDebugFlag = saved_rDebugFlag;
    if (reread_buffer) {
	free_palign(dip, reread_buffer);
    }
    if (dip) {
	Free(dip, dip); dip = NULL;
    }
    return(status);
}

static void
report_reread_command(dinfo_t *dip, size_t request_size, Offset_t record_offset, uint32_t pattern)
{
    char	str[STRING_BUFFER_SIZE];
    char	*sbp = str;

    /*
     * Display a re-read command line, required for "reference trace".
     */
    Fprintf(dip, "Command line to re-read the corrupted data:\n");
    sbp += Sprintf(str, "-> %s", dtpath);
    sbp += Sprintf(sbp, " if=%s bs=%u count=1 position=" FUF ,
		   dip->di_dname, (uint32_t)request_size, record_offset);
    if (dip->di_fprefix_string) {
	sbp += Sprintf(sbp, " prefix=\"%s\"", dip->di_fprefix_string);
    }
    if (dip->di_iot_pattern) {
	sbp += Sprintf(sbp, " pattern=iot");
	if (dip->di_iot_seed_per_pass != IOT_SEED) {
	    sbp += Sprintf(sbp, " iotseed=0x%08x", dip->di_iot_seed_per_pass);
	}
    } else if (dip->di_incr_pattern) {
	sbp += Sprintf(sbp, " pattern=incr");
    } else if (dip->di_pattern_file) {
	sbp += Sprintf(sbp, " pf=%s", dip->di_pattern_file);
    } else {
	sbp += Sprintf(sbp, " pattern=0x%08x", pattern);
    }
    if (dip->di_lbdata_flag && dip->di_timestamp_flag) {
	sbp += Sprintf(sbp, " enable=lbdata,timestamp");
    } else if (dip->di_lbdata_flag) {
	sbp += Sprintf(sbp, " enable=lbdata");
    } else if (dip->di_timestamp_flag) {
	sbp += Sprintf(sbp, " enable=timestamp");
    }
    if ( ( (dip->di_dtype->dt_dtype == DT_REGULAR) ||
	   (dip->di_dtype->dt_dtype == DT_BLOCK) ) && (dip->di_dio_flag == True) ) {
	sbp += Sprintf(sbp, " flags=direct");
    }
    if (dip->di_dump_limit != BLOCK_SIZE) {
	sbp += Sprintf(sbp, " dlimit=%u", dip->di_dump_limit);
    }
    if (dip->di_dsize != BLOCK_SIZE) {
	sbp += Sprintf(sbp, " dsize=%u", dip->di_dsize);
    }
    if (dip->di_scsi_io_flag) {
	sbp += Sprintf(sbp, " enable=scsi_io");
    }
    if (dip->di_btag) {
	sbp += Sprintf(sbp, " enable=btags");
    }
    /*
     * We cannot compare the data for non-IOT pattern:
     * - pattern files (need to save pattern data for this record)
     * - pattern strings (but handle special case IOT text here)
     * - request is non-modulo pattern buffer size (pattern wraps)
     * 
     * Note: This may not be perfect, but most folks use IOT pattern!
     */
    if ( (dip->di_iot_pattern == False) &&
	 ( dip->di_pattern_file || dip->di_pattern_string || 
	   (dip->di_pattern_bufsize && (request_size % dip->di_pattern_bufsize)) ) ) {
	sbp += Sprintf(sbp, " disable=compare");
    }
    Fprintf(dip, "%s\n", str);
    Fprintf(dip, "\n");
    return;
}

static int
verify_data_with_btags(
	struct dinfo	*dip,
	uint8_t		*buffer,
	size_t		bytes,
	uint32_t	pattern,
	lbdata_t	*lba,
	hbool_t		raw_flag )
{
    size_t bindex = 0;
    uint8_t *vptr = buffer;
    register uint8_t *pptr = dip->di_pattern_bufptr;
    uint8_t *pend = dip->di_pattern_bufend;
    uint32_t dsize = dip->di_lbdata_size;
    register btag_t *ebtag = dip->di_btag;
    register btag_t *rbtag = NULL;
    uint32_t error_index = 0;
    int btag_size = getBtagSize(ebtag);
    hbool_t error = False;
    int status = SUCCESS;
    
#if defined(DEBUG)
    /* Actually a sanity check! */
    if (bytes % dsize) {
	Eprintf(dip, "The I/O request of "SDF" bytes, is NOT modulo the expected %u btag block size!\n",
		bytes, dsize);
	return(FAILURE);
    }
#endif /* defined(DEBUG) */

    dip->di_saved_pattern_ptr = pptr;	/* Required for verify rereads! */
    /*
     * Note: We only use the pattern buffer for the IOT pattern, not for any
     * others, since we rely on the btag header and CRC for data correctness.
     */
    for (bindex = 0; bindex < bytes; bindex += dsize, vptr += dsize) {
	Offset_t offset = (dip->di_offset + bindex);
	uint32_t record_index = (uint32_t)bindex;

	rbtag = (btag_t *)vptr;
	/* For IOT or read-after-write (raw), btags are in the pattern buffer. */
	if (dip->di_iot_pattern || raw_flag) {
	    ebtag = (btag_t *)pptr;
	} else {
	    /* Update the btag for this record first! */
	    update_record_btag(dip, ebtag, offset,
			       record_index, bytes, (dip->di_records_read + 1));
	}
	if (dip->di_dump_btags == True) {
	    report_btag(dip, NULL, rbtag, raw_flag);
	} else if (dip->di_btag_vflags) {
	    /* Now verify the btag. */
	    status = verify_btags(dip, ebtag, rbtag, &error_index, raw_flag);
	    /* Compare the prefix string (if any) for higher data validation. */
	    /* Note: Selectively controlled since this may impact performance! */
	    if ( (status == SUCCESS) && dip->di_fprefix_string &&
		 dip->di_xcompare_flag && (dip->di_io_mode != MIRROR_MODE) ) {
		status = verify_btag_prefix(dip, ebtag, rbtag, &error_index);
	    }
	}
	if (status == FAILURE) {
	    uint8_t *eptr = (uint8_t *)ebtag + error_index;
	    uint8_t *rptr = (uint8_t *)rbtag + error_index;
	    error = True;
	    /* Report the btag now, before triggers execute! (if any) */
	    report_btag(dip, ebtag, rbtag, raw_flag);
	    ReportCompareError(dip, bytes, ((unsigned int)bindex + error_index), *eptr, *rptr);
	    break;
	}
	if (dip->di_iot_pattern || raw_flag) {
	    pptr += dsize;
	} else { /* Adjust the pattern buffer. */
	    int psize = (int)dsize - btag_size;
	    while (psize--) {
		if (++pptr == pend) pptr = dip->di_pattern_buffer;
	    }
	}
    }
    if (error == True) {
	if (dip->di_dump_flag) {
	    uint8_t *ebuffer = (uint8_t *)ebtag;
	    uint8_t *ebufptr = (ebuffer + error_index);
	    size_t ebuffer_size = btag_size;
	    size_t edump_size = ebuffer_size;
	    uint8_t *rbuffer = (uint8_t *)rbtag;
	    uint8_t *rbufptr = (rbuffer + error_index);
	    size_t rbuffer_size = (bytes - bindex);
	    size_t rdump_size = CalculateDumpSize(dip, rbuffer_size);
	    /* expected */
	    if (dip->di_iot_pattern || raw_flag) {
		/* We have a full block we can display! */
		ebuffer_size = edump_size = dsize;
		/* The btag and data are both in one buffer. */
		dump_buffer(dip, pattern_str, ebuffer, ebufptr, edump_size, ebuffer_size, True);
	    } else {
		uint8_t *pbuffer = dip->di_pattern_buffer;
		uint8_t *pbufptr = pptr;
		size_t pbuffer_size = dip->di_pattern_bufsize;
		size_t pdump_size = pbuffer_size;
		/* 1st, the btag! */
		dump_buffer(dip, btag_str, ebuffer, ebufptr, edump_size, ebuffer_size, True);
		/* Now, the pattern data. */
		dump_buffer(dip, pattern_str, pbuffer, pbufptr, pdump_size, pbuffer_size, True);
	    }
	    /* received */
	    dump_buffer(dip, data_str, rbuffer, rbufptr, rdump_size, rbuffer_size, False);
	}
	if (dip->di_retrying == False) {
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
		(void)ExecuteTrigger(dip, miscompare_op);
	    }
	}
	if (dip->di_iot_pattern == True) {
	    process_iot_data(dip, dip->di_pattern_bufptr, buffer, bytes, raw_flag);
	}
    }
    dip->di_pattern_bufptr = pptr;
    return(status);
}

int
verify_btag_prefix(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag, uint32_t *eindex)
{
    uint32_t btag_size = getBtagSize(ebtag);
    uint8_t *bptr = (uint8_t *)rbtag + btag_size;
    uint8_t *pstr = (uint8_t *)ebtag + btag_size;
    int pindex = 0;
    int status = SUCCESS;

    /* Do byte by byte comparison for accurate error index. */
    for ( ; (pindex < dip->di_fprefix_size); pindex++) {
	if (*bptr++ != *pstr++) {
	    if (eindex) {
		*eindex = (btag_size + pindex);
	    }
	    status = FAILURE;
	    break;
	}
    }
    return(status);
}

static int
verify_data_normal(
	struct dinfo	*dip,
	u_char		*buffer,
	size_t		bcount,
	u_int32		pattern,
	hbool_t		raw_flag )
{
#if defined(TIMESTAMP)
    u_char *tptr = NULL;
#endif /* defined(TIMESTAMP) */
    register size_t i = 0;
    register u_char *vptr = buffer;
    register u_char *pptr = dip->di_pattern_bufptr;
    register u_char *pend = dip->di_pattern_bufend;
    register uint32_t dsize = dip->di_lbdata_size;
    register size_t count = bcount;
    hbool_t error = False;
    int status = SUCCESS;

    dip->di_saved_pattern_ptr = pptr;

    while ( (i < count) ) {
#if defined(TIMESTAMP)
        /*
         * Skip the timestamp (if enabled).
         */
        if (dip->di_timestamp_flag && ((i % dip->di_lbdata_size) == 0)) {
            int p;
            i += sizeof(iotlba_t);
	    tptr = vptr;
            vptr += sizeof(iotlba_t);
            for (p = 0; (p < (int)sizeof(iotlba_t)); p++) {
                if (++pptr == pend) pptr = dip->di_pattern_buffer;
            }
        }
	if (i >= count) break;
#endif /* defined(TIMESTAMP) */
	if (*vptr != *pptr) {
	    error = True;
	    ReportCompareError(dip, count, (u_int)i, *pptr, *vptr);
	    break;
	} else {
	    i++, pptr++, vptr++;
	    if (pptr == pend) pptr = dip->di_pattern_buffer;
	}
    }
    if (error) {
	if (dip->di_dump_flag) {
	    size_t dump_size = CalculateDumpSize(dip, count);
	    if (dip->di_pattern_buffer) {
		size_t pdump_size = (dump_size < dip->di_pattern_bufsize)
					? dump_size : dip->di_pattern_bufsize;
		/* expected */
		dump_buffer(dip, pattern_str, dip->di_pattern_buffer, pptr,
			    pdump_size, dip->di_pattern_bufsize, True);
	    }
	    /* received */
#if defined(TIMESTAMP)
            if (dip->di_timestamp_flag) {
                display_timestamp(dip, tptr);
            }
#endif /* defined(TIMESTAMP) */
	    dump_buffer (dip, data_str, buffer, vptr, dump_size, count, False);
	}
	if (dip->di_retrying == False) {
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
		(void)ExecuteTrigger(dip, miscompare_op);
	    }
	}
	status = FAILURE;
    }
    dip->di_pattern_bufptr = pptr;
    return (status);
}

static int
verify_data_prefix(
	struct dinfo	*dip,
	u_char		*buffer,
	size_t		bcount,
	u_int32		pattern,
	hbool_t		raw_flag )
{
#if defined(TIMESTAMP)
    u_char *tptr = NULL;
#endif /* defined(TIMESTAMP) */
    register size_t i = 0;
    register u_char *vptr = buffer;
    register u_char *pptr = dip->di_pattern_bufptr;
    register u_char *pend = dip->di_pattern_bufend;
    register size_t count = bcount;
    hbool_t error = False;
    int status = SUCCESS;

    dip->di_saved_pattern_ptr = pptr;

    while ( (i < count) ) {
	/*
	 * Verify the prefix string (if any).
	 */
	if (dip->di_fprefix_string && ((i % dip->di_lbdata_size) == 0)) {
	    size_t pcount;
	    status = verify_prefix(dip, vptr, count, (int)i, &pcount);
	    if (status == FAILURE) return (status);
	    i += pcount;
	    vptr += pcount;
	    /* In mirror mode, the prefix *is* in the buffer as well. */
	    if (dip->di_pattern_in_buffer == True) {
		pptr += pcount;
	    }
#if defined(TIMESTAMP)
	    /*
	     * Skip the timestamp (if enabled).
	     */
	    if (dip->di_timestamp_flag) {
		int p;
		i += sizeof(iotlba_t);
		tptr = vptr;
		vptr += sizeof(iotlba_t);
		for (p = 0; (p < (int)sizeof(iotlba_t)); p++) {
		    if (++pptr == pend) pptr = dip->di_pattern_buffer;
		}
	    }
#endif /* defined(TIMESTAMP) */
	    continue;
	}
	if (*vptr != *pptr) {
	    error = True;
	    ReportCompareError(dip, count, (u_int)i, *pptr, *vptr);
	    break;
	} else {
	    i++, pptr++, vptr++;
	    if (pptr == pend) pptr = dip->di_pattern_buffer;
	}
    }
    if (error) {
	if (dip->di_dump_flag) {
	    size_t dump_size = CalculateDumpSize(dip, count);
	    if (dip->di_pattern_buffer) {
		size_t pdump_size = (dump_size < dip->di_pattern_bufsize)
					? dump_size : dip->di_pattern_bufsize;
		/* expected */
		dump_buffer(dip, pattern_str, dip->di_pattern_buffer, pptr,
					pdump_size, dip->di_pattern_bufsize, True);
	    }
	    /* received */
#if defined(TIMESTAMP)
            if (dip->di_timestamp_flag) {
                display_timestamp(dip, tptr);
            }
#endif /* defined(TIMESTAMP) */
	    dump_buffer(dip, data_str, buffer, vptr, dump_size, count, False);
	}
	if (dip->di_retrying == False) {
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
		(void)ExecuteTrigger(dip, miscompare_op);
	    }
	}
	status = FAILURE;
    }
    dip->di_pattern_bufptr = pptr;
    return (status);
}

static int
verify_data_with_lba(
	struct dinfo	*dip,
	u_char		*buffer,
	size_t		bcount,
	u_int32		pattern,
	u_int32		*lba,
	hbool_t		raw_flag )
{
    register size_t i = 0;
    register u_char *vptr = buffer;
    register u_char *pptr = dip->di_pattern_bufptr;
    register u_char *pend = dip->di_pattern_bufend;
    register size_t count = bcount;
    register lbdata_t lbn, vlbn = *lba;
    hbool_t error = False, lbn_error = False;
    int status = SUCCESS;

    dip->di_saved_pattern_ptr = pptr;
    /*
     * This optimization reduces IOT pattern compares by roughly 1/2
     * the CPU load. If it fails, we'll fall through to the previous
     * logic, which then determines if it's the prefix, LBA, or the
     * data which is incorrect. Timestamps always cause a mismatch!
     */
    if ( dip->di_iot_pattern && (dip->di_timestamp_flag == False) ) {
        if (memcmp(pptr, vptr, count) == 0) {
	    *lba += (lbdata_t)(count / dip->di_lbdata_size);
	    return (status);
	}
    }

    /* Note: This is overloaded, and needs cleaned up via rewrite! */
    while ( (i < count) ) {
	/*
	 * Handle IOT and Lbdata logical block checks first.
	 */
	if ( ((i % dip->di_lbdata_size) == 0) ) {
	    /*
	     * Verify the prefix string prior to encoded lba's.
	     */
	    if (dip->di_fprefix_string) {
		size_t pcount;
		/*
		 * For IOT data, the prefix is in the pattern buffer. 
		 * For lbdata, the prefix is NOT in the pattern buffer.
		 */
		status = verify_prefix(dip, vptr, count, (int)i, &pcount);
		if (status == FAILURE) {
		    if (dip->di_iot_pattern) {
                        process_iot_data(dip, dip->di_pattern_bufptr, buffer, bcount, raw_flag);
		    }
		    return (status);
		}
		/* In mirror mode, the prefix *is* in the buffer as well. */
		if (dip->di_iot_pattern || (dip->di_pattern_in_buffer == True) ) {
		    pptr += pcount;
		}
		vptr += pcount;
		if ( (i += pcount) == count) continue;
	    }
	    if ( (i+sizeof(lbn) <= count) ) {
		if (dip->di_iot_pattern) {
		    vlbn = get_lbn(pptr);
		    lbn = get_lbn(vptr);
		} else {
		    lbn = (u_int32)stoh(vptr, sizeof(lbn));
		}
	        if (!dip->di_timestamp_flag && (lbn != vlbn)) {
		    error = lbn_error = True;
		    ReportLbdataError(dip, *lba, (uint32_t)count, (uint32_t)i, vlbn, lbn);
		    break;
		} else {
		    int size;
		    vlbn++;
		    i += sizeof(lbn);
		    vptr += sizeof(lbn);
		    /* Skip past pattern bytes, handling wrapping. */
		    size = sizeof(lbn);
		    while (size--) {
			pptr++;
			if (pptr == pend) pptr = dip->di_pattern_buffer;
		    }
		}
		continue;
	    }
	}

	if (*vptr != *pptr) {
	    error = True;
	    ReportCompareError(dip, count, (u_int)i, *pptr, *vptr);
	    break;
	} else {
	    i++, pptr++, vptr++;
	    if (pptr == pend) pptr = dip->di_pattern_buffer;
	}
    }

    if (error) {
	if (dip->di_dump_flag) {
	    size_t dump_size = CalculateDumpSize(dip, count);
            if (lbn_error && (dip->di_iot_pattern == False) ) {
                u_int32 elbn = vlbn; /* Can't take address of register. */
                /* expected - yep, real ugly, but gotta be correct! */
                dump_buffer(dip, lba_str, (u_char *)&elbn, (u_char *)&elbn,
			    sizeof(elbn), sizeof(elbn), True); 
            } else if (dip->di_pattern_buffer) {
		size_t pdump_size = (dump_size < dip->di_pattern_bufsize)
				     ? dump_size : dip->di_pattern_bufsize;
		/* expected */
		dump_buffer(dip, pattern_str, dip->di_pattern_buffer, pptr,
			    pdump_size, dip->di_pattern_bufsize, True);
	    }
	    /* received */
#if defined(TIMESTAMP)
            if (dip->di_timestamp_flag) {
                display_timestamp(dip, buffer);
            }
#endif /* defined(TIMESTAMP) */
	    dump_buffer(dip, data_str, buffer, vptr, dump_size, count, False);
	}
	if (dip->di_retrying == False) {
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
		(void)ExecuteTrigger(dip, miscompare_op);
	    }
	}
	if (dip->di_iot_pattern) {
            process_iot_data(dip, dip->di_pattern_bufptr, buffer, bcount, raw_flag);
	}
	status = FAILURE;
    }
    dip->di_pattern_bufptr = pptr;
    *lba = vlbn;
    return (status);
}

/************************************************************************
 *									*
 * verify_padbytes() - Verify Pad Bytes Consistency.			*
 *									*
 * Description:								*
 *	This function simply checks the pad bytes to ensure they	*
 * haven't been overwritten after a read operation.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		buffer = Pointer to start of pad buffer.		*
 *		count = The last record read byte count.		*
 *		pattern = Data pattern to compare against.		*
 *		offset = Offset to where pad bytes start.		*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Data Ok/Compare Error.	*
 *									*
 ************************************************************************/
int
verify_padbytes (
	struct dinfo	*dip,
	u_char		*buffer,
	size_t		count,
	u_int32		pattern,
	size_t		offset )
{
    size_t pbytes, pindex;
    int status;

    /*
     * For short reads, checks inverted data bytes & pad bytes.
     */
    if ( (offset != count) && dip->di_spad_check) {
	size_t resid = (offset - count);
	pindex = (count & (sizeof(u_int32) - 1));
	pbytes = (resid < PADBUFR_SIZE) ? resid : PADBUFR_SIZE;
	status = dopad_verify(dip, buffer, count, pattern, pbytes, pindex, True);
	if (status == FAILURE) return(status);
    }
    pindex = 0;
    pbytes = PADBUFR_SIZE;
    return( dopad_verify (dip, buffer, offset, pattern, pbytes, pindex, False) );
}

static int
dopad_verify (
    struct dinfo	*dip,
    u_char		*buffer,
    size_t		offset,
    u_int32		pattern,
    size_t		pbytes,
    size_t		pindex,
    hbool_t		inverted )
{
    int status = SUCCESS;
    u_char *vptr;
    size_t i;
    union {
	u_char pat[sizeof(u_int32)];
	u_int32 pattern;
    } p;

    p.pattern = pattern;
    vptr = buffer + offset;

    /*
     * Note: We could be comparing inverted data on short reads.
     */
    for (i = pindex; i < (pbytes + pindex); i++, vptr++) {
	if (*vptr != p.pat[i & (sizeof(u_int32) - 1)]) {
	    if (dip->di_extended_errors == True) {
		INIT_ERROR_INFO(eip, dip->di_dname, miscompare_op, READ_OP, &dip->di_fd, 0, dip->di_offset,
				(size_t)offset, (os_error_t)0, logLevelError, PRT_SYSLOG, RPT_NOERRORMSG);
		ReportErrorNumber(dip);
		dip->di_buffer_index = (uint32_t)i;
		ReportExtendedErrorInfo(dip, eip, NULL);
	    } else {
		if (dip->di_history_size) dump_history_data(dip);
		RecordErrorTimes(dip, True);
	    }
	    Fprintf (dip, "Data compare error at %s byte %u in record number %lu\n",
			    (inverted) ? "inverted" : "pad",
			    (inverted) ? (offset + i) : i,
			    (dip->di_records_read + 1));
	    ReportDeviceInfo(dip, offset, (u_int)i, False);
	    Fprintf (dip, "Data expected = "LLPXFMT", data found = %#x, pattern = 0x%08x\n",
		    p.pat[i & (sizeof(u_int32) - 1)], *vptr, pattern);
	    if (dip->di_dump_flag) {
		/*
		 * Limit data dumped for short corrupted records.
		 */
		size_t dump_size = CalculateDumpSize(dip, offset);
		dump_buffer(dip, data_str, buffer, vptr, dump_size, dip->di_data_size, False);
	    } else {
		Fprintf(dip, "Data buffer pointer = "LLPXFMT", buffer offset = %ld\n",
			vptr, offset);
	    }
	    status = FAILURE;
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
		(void)ExecuteTrigger(dip, miscompare_op);
	    }
	    break;
	}
    }
    return (status);
}

hbool_t
is_retryable(dinfo_t *dip, int error_code)
{
    int entry;

    /* Note: These retry variable move to dip in threaded dt! */
    for (entry = 0; (entry < dip->di_retry_entries); entry++) {
	if ( (error_code == dip->di_retry_errors[entry]) ||
	     (dip->di_retry_errors[entry] == -1) ) {	/* Any error! */
	    return (True);
	}
    }
    return (False);
}

hbool_t
retry_operation(dinfo_t *dip, error_info_t *eip)
{
    FILE *fp;

    if ( (eip->ei_log_level == logLevelCrit) ||
	 (eip->ei_log_level == logLevelError) ) {
	fp = dip->di_efp;
    } else {
	fp = dip->di_ofp;
    }

    if (dip->di_retry_count++ >= dip->di_retry_limit) {
	LogMsg(dip, fp, eip->ei_log_level, eip->ei_prt_flags,
	       "Exceeded retry limit (%u) for this request!\n", dip->di_retry_limit);
	return(False);
    }
    /* Note: The caller *must* check for terminating! */
    if ( PROGRAM_TERMINATING || THREAD_TERMINATING(dip) || COMMAND_INTERRUPTED ) {
	return(True);
    }
    LogMsg(dip, fp, logLevelWarn, eip->ei_prt_flags,
	   "Retrying request after %u second delay, retry #%d\n",
	   dip->di_retry_delay, dip->di_retry_count);
    SleepSecs(dip, dip->di_retry_delay);
    
    return(True);
}

void
ReportCompareError (
	struct dinfo	*dip,
	size_t		byte_count,
	u_int		byte_position,
	u_int		expected_data,
	u_int		data_found)
{
    int prt_flags = (PRT_NOLEVEL|PRT_SYSLOG);

    if (dip->di_extended_errors == True) {
	INIT_ERROR_INFO(eip, dip->di_dname, miscompare_op, READ_OP, &dip->di_fd, 0, dip->di_offset,
			byte_count, (os_error_t)0, logLevelError, prt_flags, RPT_NOERRORMSG);
	ReportErrorNumber(dip);
	dip->di_buffer_index = (uint32_t)byte_position;
	ReportExtendedErrorInfo(dip, eip, NULL);
    } else {
	if (dip->di_history_size) dump_history_data(dip);
	RecordErrorTimes(dip, True);
    }
    if (dip->di_dtype->dt_dtype == DT_TAPE) {
	LogMsg(dip, dip->di_efp, logLevelError, prt_flags,
	       "File #%lu, %s %u in record number %lu\n", (dip->di_files_read + 1),
	       compare_error_str, byte_position, (dip->di_records_read + 1));
    } else {
	LogMsg(dip, dip->di_efp, logLevelError, prt_flags,
	       "%s %u in record number %lu\n",
	       compare_error_str, byte_position, (dip->di_records_read + 1));
    }

    ReportDeviceInfo(dip, byte_count, byte_position, False);

    LogMsg(dip, dip->di_efp, logLevelError, prt_flags,
	   "Data expected = %#x, data found = %#x, byte count = %lu\n",
	   expected_data, data_found, byte_count);
    return;
}

/************************************************************************
 *									*
 * ReportLbDataError() - Report Logical Block Data Compare Error.	*
 *									*
 * Inputs:	dip = The device info structure.			*
 *		lba = The starting logical block address.		*
 *		byte_count = The byte count of the last request.	*
 *		byte_position = Data buffer index where compare failed.	*
 *		expected_data = The expected data.			*
 *		data_found = The incorrect data found.			*
 *									*
 * Return Value: Void.							*
 *									*
 ************************************************************************/
void
ReportLbdataError (
	struct dinfo	*dip,
        u_int32		lba,
	u_int32		byte_count,
	u_int32		byte_position,
	u_int32		expected_data,
	u_int32		data_found )
{
    int prt_flags = (PRT_NOLEVEL|PRT_SYSLOG);

    if (dip->di_extended_errors == True) {
	INIT_ERROR_INFO(eip, dip->di_dname, miscompare_op, READ_OP, &dip->di_fd, 0, dip->di_offset,
			byte_count, (os_error_t)0, logLevelError, prt_flags, RPT_NOERRORMSG);
	ReportErrorNumber(dip);
	dip->di_buffer_index = (uint32_t)byte_position;
	ReportExtendedErrorInfo(dip, eip, NULL);
    } else {
	if (dip->di_history_size) dump_history_data(dip);
	RecordErrorTimes(dip, True);
    }
    if (dip->di_dtype->dt_dtype == DT_TAPE) {
	LogMsg(dip, dip->di_efp, logLevelError, prt_flags,
	       "File #%lu, %s %u in record number %u\n", (dip->di_files_read + 1),
	       compare_error_str, byte_position, (dip->di_records_read + 1));
    } else {
	LogMsg(dip, dip->di_efp, logLevelError, prt_flags,
	       "%s %u in record number %u\n",
	       compare_error_str, byte_position, (dip->di_records_read + 1));
    }

    ReportDeviceInfo(dip, byte_count, byte_position, False);

    LogMsg(dip, dip->di_efp, logLevelError, prt_flags,
	   "Block expected = %u (0x%08x), block found = %u (0x%08x), count = %u\n",
	   expected_data, expected_data, data_found, data_found, byte_count);
    return;
}

void
report_device_information(dinfo_t *dip)
{
    int flags = (PRT_NOLEVEL|PRT_SYSLOG);

    LogMsg(dip, dip->di_efp, logLevelInfo, flags,
           "Device name: %s\n", dip->di_dname);
#if defined(SCSI)
    if (dip->di_serial_number) {
        LogMsg (dip, dip->di_efp, logLevelInfo, flags,
                "Device serial number: %s\n", dip->di_serial_number);
    }
    if (dip->di_device_id) {
        LogMsg (dip, dip->di_efp, logLevelInfo, flags,
                "Device identifier: %s\n", dip->di_device_id);
    }
#endif /* defined(SCSI) */
    return;
}

/*
 * ReportDeviceInfo() - Report Device Information.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	byte_count = The request byte count.
 *	buffer_index = Position of failure (DC).
 *	eio_error = Indicates if an EIO error occurred.
 *
 * Return Value:
 *	void
 * 
 * TODO: This code is ancient, overloaded, and clearly needs rewritten.
 * 	 Perhaps one day I'll get around to it, but it's messey as heck!
 */
void
ReportDeviceInfo(
	struct dinfo	*dip,
	size_t		byte_count,
	u_int		buffer_index,
	hbool_t		eio_error )
{
    int flags = (PRT_NOLEVEL|PRT_SYSLOG);

    /* TODO: This needs updated for the new extended error reporting! */

    /* Report common device information. */
    if (dip->di_extended_errors == False) {
	report_device_information(dip);
    }
    if (dip->di_fd == NoFd) return;

    /*
     * For disk devices, also report the relative block address.
     */
    if (dip->di_random_access) {
        large_t lba;
	Offset_t current_offset;
	Offset_t starting_offset;
	u_int32 dsize = dip->di_dsize;
	/* Note: This is position in block, not a file offset! */
	u_int32 block_index = (buffer_index % dsize);

	/* Note: This old crap is kept as a sanity check for the new, but will go! */
#if defined(AIO)
	/*
	 * TODO: This AIO junk needs cleaned up!
	 */
	if (dip->di_aio_flag) {
	    starting_offset = dip->di_current_acb->aio_offset;
	    current_offset = starting_offset;
	} else if (dip->di_mmap_flag || dip->di_scsi_io_flag) {
	    starting_offset = dip->di_offset;
	    current_offset = starting_offset;
	} else {
	    /* Note: For non-EIO errors, file offset is updated! */
	    /* TODO: Cleanup *after* switching to pread/pwrite! */
	    if (dip->di_iobehavior == DT_IO) {
#if 1
		starting_offset = dip->di_offset;
		current_offset = starting_offset;
#else
		current_offset = get_position (dip);
		if (starting_offset = current_offset) {
		    if (eio_error == False) {
			starting_offset -= byte_count;
		    }
		}
#endif
	    } else {
		/* Note: sio uses pread/write, but maintains offset! */
		starting_offset = dip->di_offset;
		current_offset = starting_offset;
	    }
	}
#else /* !defined(AIO) */
	if (dip->di_mmap_flag || dip->di_scsi_io_flag) {
	    starting_offset = dip->di_offset;
	    current_offset = starting_offset;
	} else {
#if 1
	    starting_offset = dip->di_offset;
	    current_offset = starting_offset;
#else
	    current_offset = get_position(dip);
	    if (current_offset == (Offset_t)-1) {
		current_offset = (Offset_t)0;
	    }
	    if (starting_offset = current_offset) {
		if (eio_error == False) {
		    starting_offset -= byte_count;
		}
	    }
#endif
	}
#endif /* defined(AIO) */

	lba = WhichBlock((starting_offset + buffer_index), dsize);

        dip->di_error_lba = lba;	/* Only used by trigger script! */
        /*
         * Only save the offset for AIO, since our normal read/write
         * functions maintain the file offset themselves.  If we do
         * this here, our next offset will be incorrect breaking lba
         * values when using IOT pattern (for example).
	 *
	 * Note: di_offset is updated in success case, not on errors!
	 * If incorrectly updated, the new reread logic will fail!
         */
        if ( dip->di_aio_flag && (eio_error == True) ) {
            dip->di_offset = current_offset;
        }
        dip->di_block_index = block_index;
	dip->di_buffer_index = buffer_index;
	dip->di_error_offset = (starting_offset + buffer_index);

	/* The block index confuses some folks, so only display when non-zero! */
	if (block_index) {
	    LogMsg(dip, dip->di_efp, logLevelInfo, flags,
		   "Relative block number where the error occurred is " LUF ","
		   " offset " FUF " (index %u)\n",
		   lba, dip->di_error_offset, block_index);
	} else { /* No block index. */
	    LogMsg(dip, dip->di_efp, logLevelInfo, flags,
		   "Relative block number where the error occurred is " LUF ","
		   " offset " FUF "\n",	lba, dip->di_error_offset);
	}

	/* -> Note: This does NOT really belong here (IMO). <- */
	/* TODO: Revisit this, but removing since it screws up retrys! */
#if 0
	/*
	 * Seek past the erroring block, so we can continue our I/O.
	 * Note: This was added for disk copy to skip bad blocks!
	 */
	if (eio_error) {
	    current_offset += dsize;
	    if (dip->di_mode == READ_MODE) {
		dip->di_fbytes_read += dsize;
	    } else {
		dip->di_fbytes_written += dsize;
	    }
	    (void)set_position(dip, current_offset, False);

	    /*
	     * When copying data, properly position the output device too.
	     */
	    if ( (dip->di_io_mode != TEST_MODE)		&&
		 (dip != dip->di_output_dinfo)		&&
		 dip->di_output_dinfo->di_random_access ) {
		/*
		 * Note: Output device could be at a different offset.
		 */
		struct dinfo *odip = dip->di_output_dinfo;
		Offset_t output_offset = get_position(odip);
		output_offset += dsize;
		if (dip->di_mode == READ_MODE) {
		    dip->di_fbytes_read += dsize;
		} else {
		    dip->di_fbytes_written += dsize;
		}
		(void)set_position(odip, output_offset, False);
	    }
	}
#endif /* 0 */
    }
}

/* ------------------------------------------------------------------- */
/* These functions will replace the above functions during purge of old! */

void
report_device_informationX(dinfo_t *dip)
{
#if defined(SCSI)
    if (dip->di_serial_number) {
        PrintAscii(dip, "Device Serial Number", dip->di_serial_number, PNL);
    }
    if (dip->di_device_id) {
        PrintAscii(dip, "Device Identifier", dip->di_device_id, PNL);
    }
#endif /* defined(SCSI) */

    return;
}

/*
 * ReportDeviceInfoX() - Report Device Information Extended.
 *
 * Inputs:
 *	dip = The device information pointer.
 *      eip = The error information pointer.
 *
 * Return Value:
 *	void
 */
void
ReportDeviceInfoX(struct dinfo *dip, error_info_t *eip)
{
    Offset_t offset;
    large_t starting_lba, ending_lba;
    hbool_t eio_flag = os_isIoError(eip->ei_error);

    set_device_info(dip, eip->ei_bytes, dip->di_buffer_index, eio_flag);
    offset = eip->ei_offset;
    starting_lba =  makeLBA(dip, offset);
    ending_lba = makeLBA(dip, (offset + eip->ei_bytes));
    if (ending_lba && (eip->ei_bytes > 0)) ending_lba--;
    
    PrintDecHex(dip, "Device Size", dip->di_dsize, PNL);
    PrintLongDecHex(dip, "File Offset", offset, PNL);
    PrintLongDecHex(dip, "Starting LBA", starting_lba, PNL);
    PrintLongDecHex(dip, "Ending LBA", ending_lba, PNL);
    /* Display the 512-byte LBA to match up with analyzers/traces. */
    if ( offset && (dip->di_dsize > BLOCK_SIZE) ) {
	PrintLongDecHex(dip, "512 byte LBA", (offset / BLOCK_SIZE), PNL);
    }
    /* Additional information for miscompares (data corruptions). */
    if ( strstr(eip->ei_op, miscompare_op) ) {
	/* Note: Always display, even if same as starting offset. */
	PrintLongDecHex(dip, "Error File Offset", dip->di_error_offset, PNL);
	PrintLongDecHex(dip, "Starting Error LBA", dip->di_error_lba, PNL);
	if ( dip->di_error_lba && (dip->di_dsize > BLOCK_SIZE) ) {
	    PrintLongDecHex(dip, "512 byte Error LBA", (dip->di_error_offset / BLOCK_SIZE), PNL);
	}
	PrintDecimal(dip, "Corruption Buffer Index", dip->di_buffer_index, DNL);
	Lprintf(dip, " (byte index into read buffer)\n");
	PrintDecimal(dip, "Corruption Block Index", dip->di_block_index, DNL);
	Lprintf(dip, " (byte index in miscompare block)\n");
    }
    return;
}

/*
 * set_device_info() - Sets Device Information for Errors.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	iosize = The request I/O size.
 *	buffer_index = Buffer index of corruption.
 *	eio_error = Indicates if its' and I/O error.
 * 
 * Outputs:
 * 	dip is set with relavent device information.
 *
 * Return Value:
 *	void
 */
void
set_device_info(dinfo_t *dip, size_t iosize, uint32_t buffer_index, hbool_t eio_error)
{
    large_t lba;
    Offset_t offset;

    if (dip->di_random_access == False) return;

    offset = getFileOffset(dip);
    dip->di_block_index = (buffer_index % dip->di_dsize);
    
    /* This is the LBA where the error/corruption occurred. */
    lba = WhichBlock((offset + buffer_index), dip->di_dsize);
    dip->di_error_lba = lba;
    dip->di_error_offset = (offset + buffer_index);

# if 0
    /* For AIO, save the file offset for retries/raw? */
    if ( dip->di_aio_flag && (eio_error == True) ) {
	dip->di_offset = offset;
    }
#endif /* 0 */
    return;
}
