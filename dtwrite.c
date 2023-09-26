/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2023			    *
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
 * Module:	dtwrite.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	Write routines for generic data test program.
 * 
 * Modification History:
 * 
 * September 20, 2023 by Robin T. Miller
 *      Added debug instrumentation to simulate a premature end of data.
 *      For Linux this is: error = 28 - No space left on device
 * 
 * August 5th, 2021 by Robin T. Miller
 *      Added support for NVMe disks.
 * 
 * March 21st, 2021 by Robin T. Miller
 *      Add support for forcing FALSE data corruptiong for debugging.
 * 
 * July 9th, 2020 by Robin T. Miller (on behalf of dcb314)
 * 	Fix incorrect comparision of maxdata written in iolock function.
 * 	
 * May 5th, 2020 by Robin T. Miller
 *      Use high resolution timer for more accurate I/O timing. This is
 * implemented on Windows, but Unix systems still use gettimeofday() API.
 * 
 * May 28th, 2019 by Robin T. Miller
 *      Don't adjust offset when write error occurs (count is -1), since this
 * causes the wrong offset when we've specified an error limit.
 * 
 * January 5th, 2018 by Robin T. Miller
 *      When doing percentages and verifying the write data, then exclude
 * the read verify statistics so our read/write percentages are accurate.
 * Note: By default, read-after-write is enabled to verify data written.
 * 
 * January 5th, 2018 by Robin T. Miller
 *      Added separate read/write percentage option is and also I/O lock
 * to control multiple threads to the same disk or file.
 *
 * January 4th, 2018 by Robin T. Miller
 *      When prefilling a file, exclude the record limit and fill to the
 * data limit, otherwise random I/O may read a premature end of file.
 * 
 * December 28th, 2017 by Robin T. Miller
 *      Added support for multiple threads via I/O lock and shared data.
 * 
 * September 1st, 2017 by Robin T. Miller
 *      Add support for separate read/write random percentages.
 * 
 * December 21st, 2016 by Robin T. Miller
 *      Only use pwrite() for random access devices, and normal write() for
 * other device types such as pipes or tapes. This update allows dt to write
 * to stdout ('of=-') for generating data for other tools.
 *
 * December 13th, 2015 by Robin T. Miller
 * 	Switch from incr_position() to set_position() now that we are
 * using pwrite() API, and the internal file offset is *not* updated.
 * Note: This goes away altogether once we cleanup get_position() usage.
 * 	Without this change, the step= option was broken. :-(
 *
 * December 12th, 2015 by Robin T. Miller
 * 	Added support for initializing files always or once.
 * 	This is required for read percentage and/or debugging.
 * 
 * November 21th, 2015 by Robin T. Miller
 * 	Added support for read/write and sequential/random percentages.
 *
 * June 9th, 2015 by Robin T. Miller
 * 	Added support for block tags (btags).
 *
 * February 5th, 2015 by Robin T. Miller
 * 	Add file locking support.
 *
 * September 23rd, 2014 by Robin T. Miller
 * 	After a short write, ensure the update file descriptor offset is
 * what's expected. This sanity check is being added for ESX with NFS.
 *
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#include "dt.h"
#if !defined(_QNX_SOURCE) && !defined(WIN32)
#  include <sys/file.h>
#endif /* !defined(_QNX_SOURCE) && !defined(WIN32) */

/*
 * Forward References:
 */
int prefill_file_iolock(dinfo_t *dip, size_t block_size, large_t data_limit, Offset_t starting_offset);
int write_data_iolock(struct dinfo *dip);

/* ---------------------------------------------------------------------- */

int
prefill_file(dinfo_t *dip, size_t block_size, large_t data_limit, Offset_t starting_offset)
{
    Offset_t offset = starting_offset;
    void *data_buffer = dip->di_data_buffer;
    large_t data_written = 0;
    large_t records_written = 0;
    ssize_t count;
    uint32_t pattern;
    size_t bsize, dsize = block_size;
    int status = SUCCESS;

    if (dip->di_user_fpattern == True) {
	pattern = dip->di_fill_pattern;
    } else {
	pattern = ~dip->di_pattern;
    }
    /* Note: Debug instead of verbose since too noisy with many files/threads! */
    if (dip->di_debug_flag || dip->di_Debug_flag) {
	Printf(dip, "Filling %s at offset "FUF", block size "SUF", data limit "LUF", pattern 0x%08x...\n",
	       dip->di_dname, offset, block_size, data_limit, pattern);
    }

    init_buffer(dip, data_buffer, block_size, pattern);

    while ( (data_written < data_limit) &&
	    (dip->di_error_count < dip->di_error_limit) &&
	    (data_written < data_limit) ) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

	if ( (data_written + dsize) > data_limit) {
	    bsize = (size_t)(data_limit - data_written);
	} else {
	    bsize = dsize;
	}

	if (dip->di_Debug_flag) {
	    large_t iolba = (offset / dip->di_dsize);
	    long files = (dip->di_files_written + 1);
	    long records = (long)(records_written + 1);
	    report_record(dip, files, records, iolba, offset, WRITE_MODE, data_buffer, bsize);
	    /* Note: This needs updated with the record number! */
	    //report_io(dip, WRITE_MODE, data_buffer, bsize, offset);
	}
	/* Note: The noprog logic requires this to be updated! (sigh) */
	dip->di_offset = offset;
	do {
	    count = write_record(dip, data_buffer, bsize, dsize, offset, &status);
	} while (status == RETRYABLE);
	if (status == FAILURE) break;
	if (dip->di_end_of_file) break;

	/* Ugly, but we don't wish to gather fill statistics. */
	if ((size_t)count == dsize) {
	    dip->di_full_writes--;
	} else {
	    dip->di_partial_writes--;
	}
	offset += count;
	data_written += count;
	records_written++;
	dip->di_records_written++;
    }
    /* Flush the file system data to detect write failures! */
    if (dip->di_fsync_flag == True) {
	int rc = dt_flush_file(dip, dip->di_dname, &dip->di_fd, NULL, True);
	if (rc == FAILURE) status = rc;
    }
    /* 
     * Reset offset and stats back to where we started. 
     * Note: Rather ugly, but wish to use common code above. 
     */
    dip->di_dbytes_written -= data_written;
    dip->di_fbytes_written -= data_written;
    dip->di_vbytes_written -= data_written;
    dip->di_maxdata_written -= data_written;
    dip->di_records_written -= (u_long)records_written;

    dip->di_offset = starting_offset;
    offset = set_position(dip, starting_offset, False);
    if (offset == (Offset_t)-1) status = FAILURE;

    return(status);
}

/************************************************************************
 *									*
 * write_data() - Write specified data to the output file.		*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
write_data(struct dinfo *dip)
{
    dinfo_t *idip = dip->di_output_dinfo; /* For mirror mode. */
    struct dtfuncs *dtf = dip->di_funcs;
#if defined(DT_IOLOCK)
    io_global_data_t *iogp = dip->di_job->ji_opaque;
#endif
    register ssize_t count;
    register size_t bsize, dsize;
    large_t data_limit;
    Offset_t sequential_offset;
    int status = SUCCESS;
    Offset_t lock_offset = 0;
    hbool_t lock_full_range = False;
    hbool_t partial = False;
    lbdata_t lba;
    iotype_t iotype = dip->di_io_type;
    optype_t optype = WRITE_OP;
    int probability_reads = 0, probability_random = 0;
    int random_percentage = dip->di_random_percentage;
    hbool_t compare_flag = dip->di_compare_flag;
    hbool_t percentages_flag = False;
    hbool_t read_after_write_flag = dip->di_raw_flag;
    uint32_t loop_usecs;
    struct timeval loop_start_time, loop_end_time;

#if defined(DT_IOLOCK)
    /* Note: Temporary until we define a new I/O behavior! */
    if (iogp) {
	return( write_data_iolock(dip) );
    }
#endif /* defined(DT_IOLOCK) */

    dsize = get_data_size(dip, optype);
    data_limit = get_data_limit(dip);

    if ( (dip->di_fill_always == True) || (dip->di_fill_once == True) ) {
	if ( (dip->di_fill_always == True) || (dip->di_pass_count == 0) ) {
	    status = prefill_file(dip, dip->di_block_size, data_limit, dip->di_offset);
	    if (status == FAILURE) {
		return(status);
	    }
	}
    }

    if (dip->di_random_access) {
	if ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
	    dip->di_offset = set_position(dip, (Offset_t)dip->di_rdata_limit, False);
	    if (idip) {
		idip->di_offset = set_position(idip, (Offset_t)idip->di_rdata_limit, False);
	    }
	}
	lba = get_lba(dip);
	sequential_offset = dip->di_offset = get_position(dip);
	if (idip) {
	    idip->di_offset = get_position(idip);
	}
    } else {
	lba = make_lbdata(dip, dip->di_offset);
    }

    if ( (dip->di_lock_files == True) && dt_test_lock_mode(dip, LOCK_RANGE_FULL) ) {
	lock_full_range = True;
	lock_offset = dip->di_offset;
	status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				LOCK_TYPE_WRITE, lock_offset, (Offset_t)data_limit);
	if (status == FAILURE) return(status);
    }
    if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	dip->di_actual_total_usecs = 0;
	dip->di_target_total_usecs = 0;
    }
    if (dip->di_read_percentage || dip->di_random_percentage ||
	dip->di_random_rpercentage || dip->di_random_wpercentage) {
	percentages_flag = True;
    }

    /*
     * Now write the specifed number of records.
     */
    while ( (dip->di_error_count < dip->di_error_limit) &&
	    (dip->di_fbytes_written < data_limit) &&
	    (dip->di_records_written < dip->di_record_limit) ) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_start_time, NULL);
	    if (dip->di_records_written) {
		/* Adjust the actual usecs to adjust for possible usleep below! */
		dip->di_actual_total_usecs += timer_diff(&loop_end_time, &loop_start_time);
	    }
	}

	if ( dip->di_max_data && (dip->di_maxdata_written >= dip->di_max_data) ) {
	    dip->di_maxdata_reached = True;
	    break;
	}

	if (dip->di_volumes_flag && (dip->di_multi_volume >= dip->di_volume_limit) &&
		  (dip->di_volume_records >= dip->di_volume_records)) {
	    break;
	}

	/*
	 * Setup for read/write and/or random/sequential percentages (if enabled).
	 */
	if (percentages_flag == True) {
	    int read_percentage = dip->di_read_percentage;

	    if ( ((dip->di_fbytes_read + dip->di_fbytes_written) >= data_limit) ||
		 ((dip->di_records_read + dip->di_records_written) >= dip->di_record_limit) ) {
		dip->di_mode = WRITE_MODE;
		set_Eof(dip);
		break;
	    }
	    if (read_percentage == -1) {
		read_percentage = (int)(get_random(dip) % 100);
	    }
	    if (read_percentage) {
		probability_reads  = (int)(get_random(dip) % 100);
	    }
	    probability_random = (int)(get_random(dip) % 100);

	    if (probability_reads < read_percentage) {
		optype = READ_OP;
		dip->di_mode = READ_MODE;
		compare_flag = False;
		read_after_write_flag = False;
		if (dip->di_min_size == 0) {
		    dsize = get_data_size(dip, optype);
		}
	    } else {
		optype = WRITE_OP;
		dip->di_mode = WRITE_MODE;
		compare_flag = dip->di_compare_flag;
		if (dip->di_verify_flag == True) {
		    read_after_write_flag = dip->di_raw_flag;
		} else {
		    /* Writing only, no reading/verifying! */
		    read_after_write_flag = False;
		}
		if (dip->di_min_size == 0) {
		    dsize = get_data_size(dip, optype);
		}
	    }
	    if ( (optype == READ_OP) && dip->di_random_rpercentage) {
		random_percentage = dip->di_random_rpercentage;
	    } else if ((optype == WRITE_OP) && dip->di_random_wpercentage) {
		random_percentage = dip->di_random_wpercentage;
	    } else {
		random_percentage = dip->di_random_percentage;
	    }
	    if (probability_random < random_percentage) {
		iotype = RANDOM_IO;
	    } else {
		iotype = SEQUENTIAL_IO;
		dip->di_offset = sequential_offset;
	    }
	    if ( (dip->di_fbytes_read + dip->di_fbytes_written + dsize) > data_limit) {
		bsize = (size_t)(data_limit - (dip->di_fbytes_read + dip->di_fbytes_written));
	    } else {
		bsize = dsize;
	    }
	} else {
	    if ( (dip->di_fbytes_written + dsize) > data_limit) {
		bsize = (size_t)(data_limit - dip->di_fbytes_written);
	    } else {
		bsize = dsize;
	    }
	}

	if (dip->di_write_delay) {
	    mySleep(dip, dip->di_write_delay);
	}

	if ( (iotype == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
	    bsize = MIN((size_t)(dip->di_offset - dip->di_file_position), bsize);
	    dip->di_offset = set_position(dip, (Offset_t)(dip->di_offset - bsize), False);
	    if (idip) {
		idip->di_offset = set_position(idip, (Offset_t)(idip->di_offset - bsize), False);
	    }
	} else if (iotype == RANDOM_IO) {
	    dip->di_offset = do_random(dip, True, bsize);
	    if (idip) {
		idip->di_offset = dip->di_offset;
		set_position(idip, idip->di_offset, False);
	    }
	}

        if (dip->di_debug_flag && (bsize != dsize) && !dip->di_variable_flag) {
	    if (optype == READ_OP) {
		Printf (dip, "Record #%lu, Reading a partial record of %lu bytes...\n",
			(dip->di_records_read + 1), bsize);
	    } else {
		Printf (dip, "Record #%lu, Writing a partial record of %lu bytes...\n",
			(dip->di_records_written + 1), bsize);
	    }
        }

	if (dip->di_iot_pattern || dip->di_lbdata_flag) {
	    lba = make_lbdata(dip, (Offset_t)(dip->di_volume_bytes + dip->di_offset));
	}

	/*
	 * If requested, rotate the data buffer through ROTATE_SIZE
	 * bytes to force various unaligned buffer accesses.
	 */
	if (dip->di_rotate_flag) {
	    dip->di_data_buffer = (dip->di_base_buffer + (dip->di_rotate_offset++ % ROTATE_SIZE));
	}

	/*
	 * Initialize the data buffer with a pattern.
	 */
	if ( (compare_flag == True) && (optype == WRITE_OP) &&
	     ( (dip->di_io_mode == MIRROR_MODE) || (dip->di_io_mode == TEST_MODE) ) ) {
	    if (dip->di_iot_pattern) {
		lba = init_iotdata(dip, dip->di_data_buffer, bsize, lba, dip->di_lbdata_size);
	    } else {
		fill_buffer(dip, dip->di_data_buffer, bsize, dip->di_pattern);
	    }
	    /*
	     * Initialize the logical block data (if enabled).
	     */
	    if ( dip->di_lbdata_flag && dip->di_lbdata_size && (dip->di_iot_pattern == False) ) {
		lba = init_lbdata(dip, dip->di_data_buffer, bsize, lba, dip->di_lbdata_size);
	    }
#if defined(TIMESTAMP)
	    /*
	     * If timestamps are enabled, initialize buffer accordingly.
	     */
	    if (dip->di_timestamp_flag) {
		init_timestamp(dip, dip->di_data_buffer, bsize, dip->di_lbdata_size);
	    }
#endif /* defined(TIMESTAMP) */
	    if (dip->di_btag) {
		update_buffer_btags(dip, dip->di_btag, dip->di_offset,
				    dip->di_data_buffer, bsize, (dip->di_records_written + 1));
	    }
	}

	if (dip->di_Debug_flag) {
	    report_io(dip, (optype == WRITE_OP) ? WRITE_MODE : READ_MODE,
		      dip->di_data_buffer, bsize, dip->di_offset);
	}

	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    lock_offset = dip->di_offset;
	    /* Lock a partial byte range! */
	    status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				    LOCK_TYPE_WRITE, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}

	dip->di_retry_count = 0;
	do {
	    if (optype == READ_OP) {
		count = read_record(dip, dip->di_data_buffer, bsize, dsize, dip->di_offset, &status);
	    } else {
		count = write_record(dip, dip->di_data_buffer, bsize, dsize, dip->di_offset, &status);
	    }
	} while (status == RETRYABLE);
	if (dip->di_end_of_file) break;	/* Stop writing at end of file. */

	if (status == FAILURE) {
	    if (dip->di_error_count >= dip->di_error_limit) break;
	} else {
	    partial = (count < (ssize_t)bsize) ? True : False;
	}
	if ( (status == SUCCESS) && (dip->di_io_mode == MIRROR_MODE) ) {
	    ssize_t rcount = verify_record(idip, dip->di_data_buffer, count, dip->di_offset, &status);
	    /* TODO: Need to cleanup multiple device support! */
	    /* For now, propagate certain information to writer. */
	    if (idip->di_end_of_file) {
		dip->di_end_of_file = idip->di_end_of_file;
		break;
	    }
	    if (status == FAILURE) { /* Read or verify failed! */
		dip->di_error_count++;
		if (dip->di_error_count >= dip->di_error_limit) break;
	    }
	}
	/*
	 * If we had a partial transfer, perhaps due to an error, adjust
	 * the logical block address in preparation for the next request.
	 */
	if (dip->di_iot_pattern && ((size_t)count < bsize)) {
	    size_t resid = (bsize - count);
	    lba -= (lbdata_t)howmany(resid, (size_t)dip->di_lbdata_size);
	}

	if (optype == READ_OP) {
	    dip->di_records_read++;
	} else {
	    dip->di_records_written++;
	}
	dip->di_volume_records++;

	/*
	 * Flush data *before* verify (required for buffered mode to catch ENOSPC).
	 */ 
	if ( dip->di_fsync_frequency && ((dip->di_records_written % dip->di_fsync_frequency) == 0) ) {
	    status = (*dtf->tf_flush_data)(dip);
	    if ( (status == FAILURE) && (dip->di_error_count >= dip->di_error_limit) ) break;
	}

	if ( (count > (ssize_t) 0) && read_after_write_flag) {
	    /* Release write lock and apply a read lock (as required). */
	    if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
		/* Unlock a partial byte range! */
		status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
					LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)bsize);
		if (status == FAILURE) break;
		/* Lock a partial byte range! */
		status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
					LOCK_TYPE_READ, lock_offset, (Offset_t)bsize);
		if (status == FAILURE) break;
	    }
	    status = write_verify(dip, dip->di_data_buffer, count, dsize, dip->di_offset);
	    if (status == FAILURE) {
		if (dip->di_error_count >= dip->di_error_limit) {
		    break;
		}
	    } else if (percentages_flag) {
		/* Note: Undue the read statistics, so percentages are more accurate. */
		dip->di_records_read--;
		dip->di_dbytes_read -= count;
		dip->di_fbytes_read -= count;
		dip->di_vbytes_read -= count;
		dip->di_maxdata_read -= count;
		if ((size_t)count == dsize) {
		    dip->di_full_reads--;
		} else {
		    dip->di_partial_reads--;
		}
	    }
	} else if ( (optype == READ_OP) && (status != FAILURE) ) {
	    if ( (compare_flag == True) && (dip->di_io_mode == TEST_MODE) ) {
		ssize_t vsize = count;
		status = (*dtf->tf_verify_data)(dip, dip->di_data_buffer, vsize, dip->di_pattern, &lba, False);
	    }
	}

	/*
	 * After the first partial write to a regular file, we set a premature EOF, 
	 * to avoid any further writes. This logic is necessary, since subsequent 
	 * writes may succeed, but our read pass will try to read an entire record, 
	 * and will report a false data corruption, depending on the data pattern 
	 * and I/O type, so we cannot read past this point to be safe.
	 * Note: A subsequent write may return ENOSPC, but not always!
	 */
	if ( partial &&	(optype == WRITE_OP) && (dip->di_dtype->dt_dtype == DT_REGULAR) ) {
	    dip->di_last_write_size = count;
	    dip->di_last_write_attempted = dsize;
	    dip->di_last_write_offset = dip->di_offset;
	    dip->di_no_space_left = True;
	    dip->di_file_system_full = True;
	    set_Eof(dip);
	    break;
	}

	/*
	 * For variable length records, adjust the next record size.
	 */
	if (dip->di_min_size) {
	    if (dip->di_variable_flag) {
		dsize = get_variable(dip);
	    } else {
		dsize += dip->di_incr_count;
		if (dsize > dip->di_max_size) dsize = dip->di_min_size;
	    }
	}

	if (dip->di_io_dir == FORWARD) {
	    if (count > 0) {
		dip->di_offset += count;	/* Maintain our own position too! */
		if (idip) idip->di_offset += count;
	    }
	} else if ( (iotype == SEQUENTIAL_IO) &&
		    (dip->di_offset == (Offset_t)dip->di_file_position) ) {
	    set_Eof(dip);
	    dip->di_beginning_of_file = True;
	    break;
	}

	if (dip->di_step_offset) {
	    if (dip->di_io_dir == FORWARD) {
		dip->di_offset = set_position(dip, (dip->di_offset + dip->di_step_offset), True);
		if (idip) idip->di_offset = set_position(dip, (idip->di_offset + idip->di_step_offset), True);
		/* Linux returns EINVAL when seeking too far! */
		if (dip->di_offset == (Offset_t)-1) {
		    set_Eof(dip);
		    break;
		}
		/* 
		 * This check prevents us from writing past the end of a slice.
		 * Note: Without slices, we expect to encounter end of file/media.
		 */ 
		if ( dip->di_slices &&
		     ((dip->di_offset + (Offset_t)dsize) >= dip->di_end_position) ) {
		    set_Eof(dip);
    		    break;
		}
	    } else {
		dip->di_offset -= dip->di_step_offset;
		if (dip->di_offset <= (Offset_t)dip->di_file_position) {
		    set_Eof(dip);
		    dip->di_beginning_of_file = True;
		    break;
		}
		if (idip) {
		    idip->di_offset -= idip->di_step_offset;
		    if (idip->di_offset <= (Offset_t)idip->di_file_position) {
			set_Eof(dip);
			dip->di_beginning_of_file = True;
			break;
		    }
		}
	    }
	}
	/* Maintain our offset for sequential/random percentages. */
	if (iotype == SEQUENTIAL_IO) {
	    sequential_offset = dip->di_offset;
	}
	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    /* Unlock a partial byte range! */
	    status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				    LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}
	/* For IOPS, track usecs and delay as necessary. */
	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_end_time, NULL);
	    loop_usecs = (uint32_t)timer_diff(&loop_start_time, &loop_end_time);
            dip->di_target_total_usecs += dip->di_iops_usecs;
	    if (read_after_write_flag == True) {
		dip->di_target_total_usecs += dip->di_iops_usecs; /* Two I/O's! */
	    }
            dip->di_actual_total_usecs += loop_usecs;
            if (dip->di_target_total_usecs > dip->di_actual_total_usecs) {
		unsigned int usecs = (unsigned int)(dip->di_target_total_usecs - dip->di_actual_total_usecs);
		mySleep(dip, usecs);
            }
	}
    }
    if (lock_full_range == True) {
	int rc = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)data_limit);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

#if defined(DT_IOLOCK)

int
prefill_file_iolock(dinfo_t *dip, size_t block_size, large_t data_limit, Offset_t starting_offset)
{
    io_global_data_t *iogp = dip->di_job->ji_opaque;
    void *data_buffer = dip->di_data_buffer;
    large_t data_written = 0;
    large_t records_written = 0;
    ssize_t count;
    uint32_t pattern;
    size_t bsize, dsize = block_size;
    u_long io_record = 0;
    int status = SUCCESS;

    (void)dt_acquire_iolock(dip, iogp);
    if (dip->di_random_access) {
	if (iogp->io_initialized == False) {
	    iogp->io_starting_offset = iogp->io_sequential_offset = dip->di_offset = get_position(dip);
	    iogp->io_initialized = True;
	}
    } else {
	iogp->io_starting_offset = iogp->io_sequential_offset = dip->di_offset;
    }
    (void)dt_release_iolock(dip, iogp);

    if (dip->di_user_fpattern == True) {
	pattern = dip->di_fill_pattern;
    } else {
	pattern = ~dip->di_pattern;
    }
    /* Note: Debug instead of verbose since too noisy with many files/threads! */
    if ( (dip->di_debug_flag || dip->di_Debug_flag) && (dip->di_thread_number == 1) ) {
	Printf(dip, "Filling %s at offset "FUF", block size "SUF", data limit "LUF", pattern 0x%08x...\n",
	       dip->di_dname, iogp->io_starting_offset, block_size, data_limit, pattern);
    }

    init_buffer(dip, data_buffer, block_size, pattern);

    while ( (iogp->io_end_of_file == False) &&
	    (iogp->io_bytes_written < data_limit) &&
	    (dip->di_error_count < dip->di_error_limit) ) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

	(void)dt_acquire_iolock(dip, iogp);
	/*
	 * With multiple threads, we must check limits after unlocking.
	 */
	if ( (iogp->io_end_of_file == True) ||
	     (iogp->io_bytes_written >= data_limit) ) {
	    set_Eof(dip);
	    iogp->io_end_of_file = dip->di_end_of_file;
	    (void)dt_release_iolock(dip, iogp);
	    break;
	}
	if ( (iogp->io_bytes_written + dsize) > data_limit) {
	    bsize = (size_t)(data_limit - iogp->io_bytes_written);
	} else {
	    bsize = dsize;
	}
	dip->di_offset = iogp->io_sequential_offset;
	iogp->io_sequential_offset += bsize;
	/* Note: Must set these before I/O for other threads! */
	iogp->io_bytes_written += bsize;
	iogp->io_records_written++;
	io_record = iogp->io_records_written;
	(void)dt_release_iolock(dip, iogp);

	if (dip->di_Debug_flag) {
	    large_t iolba = (dip->di_offset / dip->di_dsize);
	    long files = (dip->di_files_written + 1);
	    report_record(dip, files, io_record,
			  iolba, dip->di_offset,
			  WRITE_MODE, dip->di_data_buffer, bsize);
	}

	do {
	    count = write_record(dip, data_buffer, bsize, dsize, dip->di_offset, &status);
	} while (status == RETRYABLE);

	if (status == FAILURE) break;
	if (dip->di_end_of_file) break;

	if ((size_t)count == dsize) {
	    dip->di_full_writes--;
	} else {
	    dip->di_partial_writes--;
	}
	data_written += count;
	records_written++;
	/* Note: This is maintained for external functions. */
	dip->di_records_written++;
    }
    if (dip->di_end_of_file == False) {
	set_Eof(dip);
    }
    iogp->io_end_of_file = dip->di_end_of_file;

    if (dip->di_fsync_flag == True) {
	int rc = dt_flush_file(dip, dip->di_dname, &dip->di_fd, NULL, True);
	if (rc == FAILURE) status = rc;
    }
    wait_for_threads_done(dip);
    /* Reset statistics. */
    dip->di_dbytes_written -= data_written;
    dip->di_fbytes_written -= data_written;
    dip->di_vbytes_written -= data_written;
    dip->di_maxdata_written -= data_written;
    dip->di_records_written -= (u_long)records_written;

    return(status);
}

/************************************************************************
 *									*
 * write_data_iolock() - Write specified data to the output file.       * 
 *      							        * 
 * Description: 						        * 
 * 	This function supports writing with multiple threads.	        * 
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
write_data_iolock(struct dinfo *dip)
{
    struct dtfuncs *dtf = dip->di_funcs;
    io_global_data_t *iogp = dip->di_job->ji_opaque;
    register ssize_t count;
    register size_t bsize, dsize;
    large_t data_limit;
    int status = SUCCESS;
    Offset_t lock_offset = 0;
    hbool_t lock_full_range = False;
    hbool_t partial = False;
    lbdata_t lba;
    iotype_t iotype = dip->di_io_type;
    optype_t optype = WRITE_OP;
    int probability_reads = 0, probability_random = 0;
    int random_percentage = dip->di_random_percentage;
    hbool_t compare_flag = dip->di_compare_flag;
    hbool_t percentages_flag = False;
    hbool_t read_after_write_flag = dip->di_raw_flag;
    uint32_t loop_usecs;
    u_long io_record = 0;
    struct timeval loop_start_time, loop_end_time;

    dsize = get_data_size(dip, optype);
    data_limit = get_data_limit(dip);

    if ( (dip->di_fill_always == True) || (dip->di_fill_once == True) ) {
	if ( (dip->di_fill_always == True) || (dip->di_pass_count == 0) ) {
	    status = prefill_file_iolock(dip, dip->di_block_size, data_limit, dip->di_offset);
	    if (status == FAILURE) {
		return(status);
	    }
	}
    }

    (void)dt_acquire_iolock(dip, iogp);
    if (dip->di_random_access) {
	if (iogp->io_initialized == False) {
	    if ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
		dip->di_offset = set_position(dip, (Offset_t)dip->di_rdata_limit, False);
	    }
	    lba = get_lba(dip);
	    iogp->io_starting_offset = iogp->io_sequential_offset = dip->di_offset = get_position(dip);
	    iogp->io_initialized = True;
	} else {
	    lba = make_lbdata(dip, iogp->io_starting_offset);
	}
    } else {
	lba = make_lbdata(dip, dip->di_offset);
	iogp->io_starting_offset = iogp->io_sequential_offset = dip->di_offset;
    }
    (void)dt_release_iolock(dip, iogp);

    if ( (dip->di_lock_files == True) && dt_test_lock_mode(dip, LOCK_RANGE_FULL) ) {
	lock_full_range = True;
	lock_offset = dip->di_offset;
	status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				LOCK_TYPE_WRITE, lock_offset, (Offset_t)data_limit);
	if (status == FAILURE) return(status);
    }
    if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	dip->di_actual_total_usecs = 0;
	dip->di_target_total_usecs = 0;
    }
    if (dip->di_read_percentage || dip->di_random_percentage ||
	dip->di_random_rpercentage || dip->di_random_wpercentage) {
	percentages_flag = True;
    }

    /*
     * Now write the specifed number of records.
     */
    while ( (iogp->io_end_of_file == False) &&
	    (dip->di_error_count < dip->di_error_limit) &&
	    (iogp->io_bytes_written < data_limit) &&
	    (iogp->io_records_written < dip->di_record_limit) ) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_start_time, NULL);
	    if (dip->di_records_written) {
		/* Adjust the actual usecs to adjust for possible usleep below! */
		dip->di_actual_total_usecs += timer_diff(&loop_end_time, &loop_start_time);
	    }
	}

	if ( dip->di_max_data && (dip->di_maxdata_written >= dip->di_max_data) ) {
	    dip->di_maxdata_reached = True;
	    break;
	}

	if (dip->di_volumes_flag && (dip->di_multi_volume >= dip->di_volume_limit) &&
		  (dip->di_volume_records >= dip->di_volume_records)) {
	    break;
	}

	(void)dt_acquire_iolock(dip, iogp);
	/*
	 * With multiple threads, we must check limits after unlocking.
	 */
	if ( (iogp->io_end_of_file == True) ||
	     (iogp->io_bytes_written >= data_limit) ||
	     (iogp->io_records_written >= dip->di_record_limit) ) {
	    set_Eof(dip);
	    iogp->io_end_of_file = dip->di_end_of_file;
	    (void)dt_release_iolock(dip, iogp);
	    break;
	}

	/*
	 * Setup for read/write and/or random/sequential percentages (if enabled).
	 */
	if (percentages_flag) {
	    int read_percentage = dip->di_read_percentage;

	    if ( ((iogp->io_bytes_read + iogp->io_bytes_written) >= data_limit) ||
		 ((iogp->io_records_read + iogp->io_records_written) >= dip->di_record_limit) ) {
		dip->di_mode = WRITE_MODE;
		set_Eof(dip);
		iogp->io_end_of_file = dip->di_end_of_file;
		(void)dt_release_iolock(dip, iogp);
		break;
	    }
	    if (read_percentage == -1) {
		read_percentage = (int)(get_random(dip) % 100);
	    }
	    if (read_percentage) {
		probability_reads  = (int)(get_random(dip) % 100);
	    }
	    probability_random = (int)(get_random(dip) % 100);

	    if (probability_reads < read_percentage) {
		optype = READ_OP;
		dip->di_mode = READ_MODE;
		compare_flag = False;
		read_after_write_flag = False;
		if (dip->di_min_size == 0) {
		    dsize = get_data_size(dip, optype);
		}
	    } else {
		optype = WRITE_OP;
		dip->di_mode = WRITE_MODE;
		compare_flag = dip->di_compare_flag;
		if (dip->di_verify_flag == True) {
		    read_after_write_flag = dip->di_raw_flag;
		} else {
		    /* Writing only, no reading/verifying! */
		    read_after_write_flag = False;
		}
		if (dip->di_min_size == 0) {
		    dsize = get_data_size(dip, optype);
		}
	    }
	    if ( (optype == READ_OP) && dip->di_random_rpercentage) {
		random_percentage = dip->di_random_rpercentage;
	    } else if ((optype == WRITE_OP) && dip->di_random_wpercentage) {
		random_percentage = dip->di_random_wpercentage;
	    } else {
		random_percentage = dip->di_random_percentage;
	    }
	    if ( (iogp->io_bytes_read + iogp->io_bytes_written + dsize) > data_limit) {
		bsize = (size_t)(data_limit - (iogp->io_bytes_read + iogp->io_bytes_written));
	    } else {
		bsize = dsize;
	    }
	    if (probability_random < random_percentage) {
		iotype = RANDOM_IO;
	    } else {
		iotype = SEQUENTIAL_IO;
		dip->di_offset = iogp->io_sequential_offset;
		if (dip->di_io_dir == REVERSE) {
		    bsize = MIN((size_t)(iogp->io_sequential_offset - dip->di_file_position), bsize);
		    dip->di_offset = set_position(dip, (Offset_t)(iogp->io_sequential_offset - bsize), False);
		    iogp->io_sequential_offset = dip->di_offset;
		} else {
		    iogp->io_sequential_offset += bsize;
		}
	    }
	    if (optype == READ_OP) {
		iogp->io_bytes_read += bsize;
		iogp->io_records_read++;
		io_record = iogp->io_records_read;
	    } else {
		iogp->io_bytes_written += bsize;
		iogp->io_records_written++;
		io_record = iogp->io_records_written;
	    }
	} else { /* percentages_flag is False */
	    if ( (iogp->io_bytes_written + dsize) > data_limit) {
		bsize = (size_t)(data_limit - iogp->io_bytes_written);
	    } else {
		bsize = dsize;
	    }
	    if (iotype == SEQUENTIAL_IO) {
		dip->di_offset = iogp->io_sequential_offset;
		if (dip->di_io_dir == REVERSE) {
		    bsize = MIN((size_t)(dip->di_offset - dip->di_file_position), bsize);
		    dip->di_offset = set_position(dip, (Offset_t)(dip->di_offset - bsize), False);
		    iogp->io_sequential_offset = dip->di_offset;
		} else {
		    iogp->io_sequential_offset += bsize;
		}
	    }
	    /* Note: Must set these before I/O for other threads! */
	    iogp->io_bytes_written += bsize;
	    iogp->io_records_written++;
	    io_record = iogp->io_records_written;
	} /* end of percentages_flag */

	if ( (iotype == SEQUENTIAL_IO) && dip->di_step_offset) {
	    Offset_t offset = iogp->io_sequential_offset;
	    if (dip->di_io_dir == FORWARD) {
		/* Note: Useful for debug, but we don't need to set position! */
		offset = set_position(dip, (offset + dip->di_step_offset), True);
		/* Linux returns EINVAL when seeking too far! */
		if (offset == (Offset_t)-1) {
		    set_Eof(dip);
		    break;
		}
		/* 
		 * This check prevents us from writing past the end of a slice.
		 * Note: Without slices, we expect to encounter end of file/media.
		 */ 
		if ( dip->di_slices &&
		     ((offset + (Offset_t)dsize) >= dip->di_end_position) ) {
		    set_Eof(dip);
		    break;
		}
	    } else { /* io_dir = REVERSE */
		offset -= dip->di_step_offset;
		if (offset <= (Offset_t)dip->di_file_position) {
		    set_Eof(dip);
		    dip->di_beginning_of_file = True;
		    break;
		}
	    }
	    iogp->io_sequential_offset = offset;
	}
	(void)dt_release_iolock(dip, iogp);

	if (dip->di_write_delay) {
	    mySleep(dip, dip->di_write_delay);
	}

	if (iotype == RANDOM_IO) {
	    dip->di_offset = do_random(dip, True, bsize);
	}

        if (dip->di_debug_flag && (bsize != dsize) && !dip->di_variable_flag) {
	    if (optype == READ_OP) {
		Printf (dip, "Record #%lu, Reading a partial record of %lu bytes...\n",
			io_record, bsize);
	    } else {
		Printf (dip, "Record #%lu, Writing a partial record of %lu bytes...\n",
			io_record, bsize);
	    }
        }

	if (dip->di_iot_pattern || dip->di_lbdata_flag) {
	    lba = make_lbdata(dip, (Offset_t)(dip->di_volume_bytes + dip->di_offset));
	}

	/*
	 * If requested, rotate the data buffer through ROTATE_SIZE
	 * bytes to force various unaligned buffer accesses.
	 */
	if (dip->di_rotate_flag) {
	    dip->di_data_buffer = (dip->di_base_buffer + (dip->di_rotate_offset++ % ROTATE_SIZE));
	}

	/*
	 * Initialize the data buffer with a pattern.
	 */
	if ( (compare_flag == True) && (optype == WRITE_OP) && (dip->di_io_mode == TEST_MODE) ) {
	    if (dip->di_iot_pattern) {
		lba = init_iotdata(dip, dip->di_data_buffer, bsize, lba, dip->di_lbdata_size);
	    } else {
		fill_buffer(dip, dip->di_data_buffer, bsize, dip->di_pattern);
	    }
	    /*
	     * Initialize the logical block data (if enabled).
	     */
	    if ( dip->di_lbdata_flag && dip->di_lbdata_size && (dip->di_iot_pattern == False) ) {
		lba = init_lbdata(dip, dip->di_data_buffer, bsize, lba, dip->di_lbdata_size);
	    }
#if defined(TIMESTAMP)
	    /*
	     * If timestamps are enabled, initialize buffer accordingly.
	     */
	    if (dip->di_timestamp_flag) {
		init_timestamp(dip, dip->di_data_buffer, bsize, dip->di_lbdata_size);
	    }
#endif /* defined(TIMESTAMP) */
	    if (dip->di_btag) {
		update_buffer_btags(dip, dip->di_btag, dip->di_offset,
				    dip->di_data_buffer, bsize, io_record);
	    }
	}

	if (dip->di_Debug_flag) {
	    large_t iolba = make_lbdata(dip, dip->di_offset);
	    long files = (dip->di_files_written + 1);
	    report_record(dip, files, io_record,
			  iolba, dip->di_offset,
			  (optype == WRITE_OP) ? WRITE_MODE : READ_MODE,
			   dip->di_data_buffer, bsize);
	}

	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    lock_offset = dip->di_offset;
	    /* Lock a partial byte range! */
	    status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				    LOCK_TYPE_WRITE, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}

	dip->di_retry_count = 0;
	do {
	    if (optype == READ_OP) {
		count = read_record(dip, dip->di_data_buffer, bsize, dsize, dip->di_offset, &status);
	    } else {
		count = write_record(dip, dip->di_data_buffer, bsize, dsize, dip->di_offset, &status);
	    }
	} while (status == RETRYABLE);

	if (dip->di_end_of_file) break;

	if (status == FAILURE) {
	    if (dip->di_error_count >= dip->di_error_limit) break;
	} else {
	    partial = (count < (ssize_t)bsize) ? True : False;
	}
	/*
	 * If we had a partial transfer, perhaps due to an error, adjust
	 * the logical block address in preparation for the next request.
	 */
	if (dip->di_iot_pattern && ((size_t)count < bsize)) {
	    size_t resid = (bsize - count);
	    lba -= (lbdata_t)howmany(resid, (size_t)dip->di_lbdata_size);
	}

	if (optype == READ_OP) {
	    dip->di_records_read++;
	} else {
	    dip->di_records_written++;
	}
	dip->di_volume_records++;

	/*
	 * Flush data *before* verify (required for buffered mode to catch ENOSPC).
	 */ 
	if ( dip->di_fsync_frequency && ((dip->di_records_written % dip->di_fsync_frequency) == 0) ) {
	    status = (*dtf->tf_flush_data)(dip);
	    if ( (status == FAILURE) && (dip->di_error_count >= dip->di_error_limit) ) break;
	}

	if ( (count > (ssize_t) 0) && read_after_write_flag) {
	    /* Release write lock and apply a read lock (as required). */
	    if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
		/* Unlock a partial byte range! */
		status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
					LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)bsize);
		if (status == FAILURE) break;
		/* Lock a partial byte range! */
		status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
					LOCK_TYPE_READ, lock_offset, (Offset_t)bsize);
		if (status == FAILURE) break;
	    }
	    status = write_verify(dip, dip->di_data_buffer, count, dsize, dip->di_offset);
	    if (status == FAILURE) {
		if (dip->di_error_count >= dip->di_error_limit) {
		    break;
		}
	    } else if (percentages_flag) {
		/* Note: Undue the read statistics, so percentages are more accurate. */
		dip->di_records_read--;
		dip->di_dbytes_read -= count;
		dip->di_fbytes_read -= count;
		dip->di_vbytes_read -= count;
		dip->di_maxdata_read -= count;
		if ((size_t)count == dsize) {
		    dip->di_full_reads--;
		} else {
		    dip->di_partial_reads--;
		}
	    }
	} else if ( (optype == READ_OP) && (status != FAILURE) ) {
	    if ( (compare_flag == True) && (dip->di_io_mode == TEST_MODE) ) {
		ssize_t vsize = count;
		status = (*dtf->tf_verify_data)(dip, dip->di_data_buffer, vsize, dip->di_pattern, &lba, False);
	    }
	}

	/*
	 * After the first partial write to a regular file, we set a premature EOF, 
	 * to avoid any further writes. This logic is necessary, since subsequent 
	 * writes may succeed, but our read pass will try to read an entire record, 
	 * and will report a false data corruption, depending on the data pattern 
	 * and I/O type, so we cannot read past this point to be safe.
	 * Note: A subsequent write may return ENOSPC, but not always!
	 */
	if ( partial &&	(optype == WRITE_OP) && (dip->di_dtype->dt_dtype == DT_REGULAR) ) {
	    dip->di_last_write_size = count;
	    dip->di_last_write_attempted = dsize;
	    dip->di_last_write_offset = dip->di_offset;
	    dip->di_no_space_left = True;
	    dip->di_file_system_full = True;
	    set_Eof(dip);
	    break;
	}

	/*
	 * For variable length records, adjust the next record size.
	 */
	if (dip->di_min_size) {
	    if (dip->di_variable_flag) {
		dsize = get_variable(dip);
	    } else {
		dsize += dip->di_incr_count;
		if (dsize > dip->di_max_size) dsize = dip->di_min_size;
	    }
	}

	if (dip->di_io_dir == FORWARD) {
	    dip->di_offset += count;	/* Maintain our own position too! */
	} else if ( (iotype == SEQUENTIAL_IO) &&
		    (dip->di_offset == (Offset_t)dip->di_file_position) ) {
	    set_Eof(dip);
	    dip->di_beginning_of_file = True;
	    break;
	}

	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    /* Unlock a partial byte range! */
	    status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				    LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}
	/* For IOPS, track usecs and delay as necessary. */
	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_end_time, NULL);
	    loop_usecs = (uint32_t)timer_diff(&loop_start_time, &loop_end_time);
            dip->di_target_total_usecs += dip->di_iops_usecs;
	    if (read_after_write_flag == True) {
		dip->di_target_total_usecs += dip->di_iops_usecs; /* Two I/O's! */
	    }
            dip->di_actual_total_usecs += loop_usecs;
            if (dip->di_target_total_usecs > dip->di_actual_total_usecs) {
		unsigned int usecs = (unsigned int)(dip->di_target_total_usecs - dip->di_actual_total_usecs);
		mySleep(dip, usecs);
            }
	}
    }
    /* Propagate end of file for other threads and outer loops. */
    if (dip->di_end_of_file == False) {
	set_Eof(dip);
    }
    iogp->io_end_of_file = dip->di_end_of_file;

    if (lock_full_range == True) {
	int rc = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)data_limit);
	if (rc == FAILURE) status = rc;
    }
    return (status);
}

#endif /* defined(DT_IOLOCK) */

/************************************************************************
 *									*
 * check_write() - Check status of last write operation.		*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		count = Number of bytes written.			*
 *		size  = Number of bytes expected.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE/WARNING = Ok/Error/Warning	*
 *									*
 ************************************************************************/
int
check_write(struct dinfo *dip, ssize_t count, size_t size, Offset_t offset)
{
    int status = SUCCESS;

    if ((size_t)count != size) {
	if (count == FAILURE) {
	    INIT_ERROR_INFO(eip, dip->di_dname, OS_WRITE_FILE_OP, WRITE_OP, &dip->di_fd, dip->di_oflags,
			    offset, size, os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    status = ReportRetryableError(dip, eip, "Failed writing %s", eip->ei_file);
	    if (status == RETRYABLE) return(status);
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		(void)ExecuteTrigger(dip, "write");
	    }
	} else {
	    Offset_t updated_offset = 0;
	    /*
	     * For writes at end of file or writes at end of block
	     * devices, we'll write less than the requested count.
	     * Short writes also occur from file system full!
	     * In this case, we'll treat this as a warning since
	     * this is to be expected.
	     *
	     * NOTE:  The raw device should be used for disks.
	     */
	    if ( (dip->di_debug_flag || dip->di_verbose_flag || ((size_t)count > size)) &&
		 (dip->di_io_mode == TEST_MODE) ) {
		if (dip->di_multiple_files) {
		    Wprintf(dip, 
			    "File %s, record #%lu, offset "FUF", attempted to write %lu bytes, wrote only %lu bytes.\n",
			   dip->di_dname, (dip->di_records_written + 1), offset, size, count);
		} else {
		    Wprintf(dip, 
			    "Record #%lu, offset "FUF", attempted to write %lu bytes, wrote only %lu bytes.\n",
			   (dip->di_records_written + 1), offset, size, count);
		}
	    }
	    if ((size_t)count < size) {	/* Partial write is a warning. */
		if ( (dip->di_fsfile_flag == True) && (dip->di_verbose_flag == True) ) {
		    (void)report_filesystem_free_space(dip);
		}
		dip->di_warning_errors++;
		return (WARNING);
	    }
	    /* TODO: Do we really reach this code? */
	    ReportDeviceInfo(dip, count, 0, False, NotMismatchedData);
	    RecordErrorTimes(dip, True);
	}
	dip->di_write_errors++;
	status = FAILURE;
    }
    return (status);
}

/************************************************************************
 *									*
 * copy_record() - Copy record to device or file.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		buffer = The data buffer to write.			*
 * 		bsize = The number of bytes to write.		        * 
 * 		offset = The current file offset.		        * 
 *		status = Pointer to status variable.			*
 *									*
 * Outputs:	status = SUCCESS/FAILURE/WARNING = Ok/Error/Warning	*
 *		Return value is number of bytes from write() request.	*
 *									*
 ************************************************************************/
ssize_t
copy_record (	struct dinfo	*dip,
		u_char		*buffer,
		size_t		bsize,
		Offset_t	offset,
		int		*status )
{
    ssize_t count;

    if (dip->di_Debug_flag) {
	report_io(dip, WRITE_MODE, buffer, bsize, offset);
    }
    dip->di_retry_count = 0;
    do {
	count = write_record(dip, buffer, bsize, bsize, offset, status);
    } while (*status == RETRYABLE);
    /* TODO: Get this into write_record() where it belongs! */
    if (count > (ssize_t) 0) {
	dip->di_records_written++;
	/* Note: More information on partial write, caller may flag as error! */
	if ( (count < (ssize_t)bsize) && (dip->di_verbose_flag == True) ) {
	    INIT_ERROR_INFO(eip, dip->di_dname, OS_WRITE_FILE_OP, WRITE_OP, &dip->di_fd, dip->di_oflags,
			    dip->di_offset, count, os_get_error(), logLevelWarn, PRT_NOFLAGS, (RPT_NOHISTORY|RPT_NORETRYS|RPT_WARNING));
	    (void)ReportRetryableError(dip, eip, "Partial write to %s", eip->ei_file);
	}
    }
    return(count);
}

/************************************************************************
 *									*
 * write_record() - Write record to device or file.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		buffer = The data buffer to write.			*
 *		bsize = The number of bytes to write.			*
 * 		dsize = The users' requested size.		        * 
 * 		offset = The current file offset.		        * 
 *		status = Pointer to status variable.			*
 *									*
 * Outputs:	status = SUCCESS/FAILURE/WARNING = Ok/Error/Warning	*
 *		Return value is number of bytes from write() request.	*
 * 		The status may also be RETRYABLE for retryable errors.	* 
 *									*
 ************************************************************************/
ssize_t
write_record(
	struct dinfo	*dip,
	u_char		*buffer,
	size_t		bsize,
	size_t		dsize,
	Offset_t	offset,
	int		*status )
{
    ssize_t count;

    /* Force a FALSE corruption (if requested), and records match! */
    if (dip->di_force_corruption && (dip->di_corrupt_writes == (dip->di_records_written + 1)) ) {
	corrupt_buffer(dip, buffer, (int32_t)bsize, dip->di_corrupt_writes);
    }
retry:
    *status = SUCCESS;
    ENABLE_NOPROG(dip, WRITE_OP);
    if (dip->di_nvme_io_flag == True) {
	count = nvmeWriteData(dip, buffer, bsize, offset);
    } else if (dip->di_scsi_io_flag == True) {
	count = scsiWriteData(dip, buffer, bsize, offset);
    } else if (dip->di_random_access == False) {
	count = os_write_file(dip->di_fd, buffer, bsize);
    } else {
	count = os_pwrite_file(dip->di_fd, buffer, bsize, offset);
    }
    DISABLE_NOPROG(dip);

#if 0
    /*DEBUG*/
    /* 
     * Force a premature "disk full" error to verify reads are limited.
     * FYI: I'm leaving this code, since forcing certain error conditions 
     * may be useful to add in the future, like force corruptions above.
     */ 
    if ( dip->di_records_written == 5 ) {
        os_set_error(OS_ERROR_DISK_FULL);
        count = FAILURE;
    }
    /*DEBUG*/
#endif /* 0 */

    if (dip->di_history_size) {
	long files, records;
	/* 
	 * Note: We cannot report read/write records with percentage, otherwise 
	 * the record numbers will NOT match extended error reporting and btags! 
	 */
	if ( False /*dip->di_read_percentage*/ ) {
	    files = (dip->di_files_read + dip->di_files_written) + 1;
	    records = (dip->di_records_read + dip->di_records_written) + 1;
	} else {
	    files = (dip->di_files_written + 1);
	    records = (dip->di_records_written + 1);
	}
	save_history_data(dip, files, records,
			  WRITE_MODE, offset, buffer, bsize, count);
    }

    if ( (count == FAILURE) && is_Eof(dip, count, bsize, status) ) {
	dip->di_last_write_size = count;
	dip->di_last_write_offset = offset;
	if (dip->di_multi_flag) {
	    *status = HandleMultiVolume(dip);
	    offset = dip->di_offset = (Offset_t)0;
	    if (*status == SUCCESS) goto retry;
	}
    } else {
	if (count > (ssize_t)0) {
	    dip->di_dbytes_written += count;
	    dip->di_fbytes_written += count;
	    dip->di_vbytes_written += count;
	    dip->di_maxdata_written += count;
	    if ((size_t)count == dsize) {
		dip->di_full_writes++;
	    } else {
		dip->di_partial_writes++;
	    }
	}
	*status = check_write(dip, count, bsize, offset);
    }
    return(count);
}

/************************************************************************
 *									*
 * write_verify() - Verify the record just written.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		buffer = The data buffer written.			*
 *		bsize = The number of bytes written.			*
 * 		dsize = The users' requested size.		        * 
 * 		offset = The starting record offset.			*
 *									*
 * Outputs:	status = SUCCESS/FAILURE/WARNING = Ok/Error/Warning	*
 *									*
 ************************************************************************/
int
write_verify(
	struct dinfo	*dip,
	u_char		*buffer,
	size_t		bsize,
	size_t		dsize,
	Offset_t	offset )
{
    u_char *vbuffer = dip->di_verify_buffer;
    ssize_t count;
    lbdata_t lba = 0;
    int status = SUCCESS;

    if (dip->di_read_delay) {			/* Optional read delay.	*/
	mySleep(dip, dip->di_read_delay);
    }

    if (dip->di_dtype->dt_dtype == DT_TAPE) {
#if !defined(__QNXNTO__) && !defined(AIX) && !defined(WIN32) && defined(TAPE)
	status = DoBackwardSpaceRecord(dip, 1);
	if (status) return(status);
#endif /* !defined(__QNXNTO__) && !defined(AIX) && !defined(WIN32) && defined(TAPE) */
    } else { /* assume random access */
	/* Note: This goes away with pread/pwrite API's! */
	Offset_t npos = set_position(dip, offset, False);
	if (npos != offset) {
	    Fprintf(dip, "ERROR: Wrong seek offset, (npos " FUF " != offset)" FUF "!\n",
		    npos, offset);
	    return(FAILURE);
	}
    }
    if (dip->di_iot_pattern || dip->di_lbdata_flag) {
	lba = make_lbdata(dip, (Offset_t)(dip->di_volume_bytes + offset));
    }

    if (dip->di_rotate_flag) {
	vbuffer = (dip->di_verify_buffer + ((dip->di_rotate_offset -1) % ROTATE_SIZE));
    }

    /*
     * If we'll be doing a data compare after the read, then
     * fill the verify buffer with the inverted pattern to ensure
     * the buffer actually gets written into (driver debug mostly).
     */
    if ( (dip->di_compare_flag == True) &&
	 ( (dip->di_io_mode == MIRROR_MODE) || (dip->di_io_mode == TEST_MODE) ) ) {
	init_padbytes(vbuffer, bsize, ~dip->di_pattern);
    }

    if (dip->di_Debug_flag) {
	report_io(dip, READ_MODE, vbuffer, bsize, offset);
    }

    dip->di_retry_count = 0;
    do {
	count = read_record(dip, vbuffer, bsize, dsize, offset, &status);
    } while (status == RETRYABLE);

    if (dip->di_end_of_file) {
	ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_READ_FILE_OP, READ_OP, True);
	if (dip->di_dtype->dt_dtype != DT_TAPE) {
	    (void)set_position(dip, offset, False);
	}
	return (FAILURE);
    }

    /*
     * Verify the data (unless disabled).
     */
    if ( (status != FAILURE) && (dip->di_compare_flag == True) &&
	 ( (dip->di_io_mode == MIRROR_MODE) || (dip->di_io_mode == TEST_MODE) ) ) {
	register ssize_t vsize = count;
	int result;

	/*
	 * Normally the buffers are exact, but with random I/O and timestamps
	 * enabled, overwrites will (occasionally) cause miscompares with AIO.
	 * Since we are doing a read after write, we should match all the time.
	 * 
	 * Note: This memcmp() works for IOT or patterns without a prefix string.
	 * 	 Messy and duplicates verify code, so needs cleaned up!
	 */
        result = memcmp(buffer, vbuffer, vsize); /* optimization! */
	if ( (result != 0) || (dip->di_dump_btags == True) ) {
	    if ( dip->di_btag_flag || dip->di_iot_pattern || (dip->di_prefix_string == NULL)) {
		/* Save the current pattern information. */
		u_char *pptr = dip->di_pattern_bufptr;
		u_char *pend = dip->di_pattern_bufend;
		u_char *pbase = dip->di_pattern_buffer;
		size_t psize = dip->di_pattern_bufsize;
		/*
		 * Note: This Mickey Mouse setup is required so we can use the existing
		 * verification functions which handle timestamps, corruption analysis,
		 * and retry logic employed in the standard read/verify data paths.
		 * TODO: Expand API's and cleanup the mess! (IOT is via pattern buffer)
		 */
		/* The data written becomes the data to compare with. */
		setup_pattern(dip, buffer, vsize, False);
		/* Now, verify the data! */
		status = (*dip->di_funcs->tf_verify_data)(dip, vbuffer, vsize, dip->di_pattern, &lba, True);
		/* Restore the original pattern buffer information. */
		dip->di_pattern_bufptr = pptr;
		dip->di_pattern_bufend = pend;
		dip->di_pattern_buffer = pbase;
		dip->di_pattern_bufsize = psize;
	    } else {
		/* 
		 * Note: This method fails w/AIO and overwriting timestamps! 
		 */
		if (dip->di_lbdata_flag) {
		    status = verify_lbdata(dip, buffer, vbuffer, vsize, &lba);
		}
		if (status == SUCCESS) {
		    status = verify_buffers(dip, buffer, vbuffer, vsize);
		}
		if ( (status != SUCCESS) && dip->di_retryDC_flag &&
		     dip->di_random_access && (dip->di_retrying == False)) {
		    (void)verify_reread(dip, vbuffer, vsize, dip->di_pattern, &lba);
		}
	    }
	}
	/*
	 * Verify the pad bytes (if enabled).
	 */
	if ( (status == SUCCESS) && dip->di_pad_check) {
	    (void) verify_padbytes(dip, vbuffer, vsize, ~dip->di_pattern, bsize);
	}
    }

    /*
     * We expect to read as much as we wrote, or else we've got a problem!
     */
    if ((size_t)count < bsize) {
	/* check_read() reports info regarding the short record read. */
	ReportDeviceInfo(dip, count, 0, False, NotMismatchedData);
	RecordErrorTimes(dip, True);
	status = FAILURE;
	if (dip->di_dtype->dt_dtype != DT_TAPE) {
	    (void)set_position(dip, offset, False);
	}
    }
    dip->di_records_read++;
    return (status);
}
