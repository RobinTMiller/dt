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
 * Module:	dtread.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	Read routines for generic data test program.
 *
 * Modification History:
 * 
 * September 20th, 2023 by Robin T. Miller
 *      For all random access devices, limit the data read to what was
 * written. Previously this was enabled only for file systems, but it's
 * possible for direct acces disks to create a premature end of data
 * condition. For example during retries the disk driver may fail Read
 * Capacity during error recovery/disk discovery returning an ENOSPC.
 *     I believe thinly provisioned disks may also return ENOSPC when
 * there's insufficient backend disk space (over provisioned).
 * 
 * August 5th, 2021 by Robin T. Miller
 *      Added support for NVMe disks.
 * 
 * March 21st, 2021 by Robin T. Miller
 *      Add support for forcing FALSE data corruptiong for debugging.
 * 
 * May 5th, 2020 by Robin T. Miller
 *      Use high resolution timer for more accurate I/O timing. This is
 * implemented on Windows, but Unix systems still use gettimeofday() API.
 * 
 * March 7th, 2020 by Robin T. Miller
 *      Apply new logic in FindCapacity() to properly handle file position.
 * 
 * March 6th, 2020 by Robin T. Miller
 *      Update SetupCapacityPercentage() to better handle slices with a file
 * position and capacity percentage, Starting offset is for mixed FS/disk test.
 * 
 * May 28th, 2019 by Robin T. Miller
 *      Don't adjust offset when read error occurs (count is -1), since this
 * causes the wrong offset when we've specified an error limit.
 * 
 * May 27th, 2019 by Robin T. Miller
 *      Add support for capacity percentage. This is to help exceeding backend
 * storage when volumes are over-provisioned (thin provisioned LUNs).
 * 
 * December 28th, 2017 by Robin T. Miller
 *      Added support for multiple threads via I/O lock and shared data.
 * 
 * September 1st, 2017 by Robin T. Miller
 *      Add support for random read percentage only.
 * 
 * December 21st, 2016 by Robin T. Miller
 *      Only use pread() for random access devices, and normal read() for
 * other device types such as pipes or tapes. This update allows dt to read
 * from stdin ('if=-') for verifying data from other tools or itself across
 * network sockets, etc. Trying to keep dt general purpose! ;)
 *
 * November 4th, 2016 by Robin T. Miller
 *      Add support for random percentages.
 * 
 * February 5th, 2016 by Robin T. Miller
 * 	Update read_record() to initialize the buffer prior to reads.
 * 	This helps diagnose when the actual read data is NOT returned.
 * 	Currently, the previosu read data is reported, can be misleading.
 * 
 * December 13th, 2015 by Robin T. Miller
 * 	Switch from incr_position() to set_position() now that we are
 * using pread() API, and the internal file offset is *not* updated.
 * Note: This goes away altogether once we cleanup get_position() usage.
 * 	Without this change, the step= option was broken. :-(
 * 
 * June 13th, 2015 by Robin T. Miller
 * 	Update verify_record() to use the data buffer rather than the
 * pattern buffer for verifying source device data. This is required for
 * block tags (btags), but also to prevent misleading corruption reporting.
 * 
 * June 9th, 2015 by Robin T. Miller
 * 	Added support for block tags (btags).
 *
 * February 5th, 2015 by Robin T. Miller
 * 	Add file locking support.
 *
 * September 23rd, 2014 by Robin T. Miller
 * 	In check_read(), report errors for short reads with random I/O,
 * to avoid false data corruptions. Also sanity check the updated file
 * descriptor offset after short reads, again to avoid false corruption.
 * Solaris VM w/ESX on NFS is returning short reads and writes, and for
 * reads my analysis indicates the fd offset is incorrect for short read!
 * The offset was updated with the request size, leading to wrong data!
 * 
 * March 20th, 2014, by Robin T. Miller
 * 	In read_record() see if we're in read or write mode, since this API
 * is used during write mode when doing read-after-write (enable=raw). Also,
 * add a file position argument, which we need for an accurate AIO history.
 * We do not wish to use di_offset, since this points to the next offset.
 * 
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#include "dt.h"
#if !defined(_QNX_SOURCE) && !defined(WIN32)
#  include <sys/file.h>
#endif /* !defined(_QNX_SOURCE) && !defined(WIN32) */
#include <sys/stat.h>

/*
 * Forward References:
 */
int read_data_iolock(struct dinfo *dip);
large_t SetupCapacityPercentage(dinfo_t *dip, large_t bytes);

/* ---------------------------------------------------------------------- */

int
check_last_write_info(dinfo_t *dip, Offset_t offset, size_t bsize, size_t dsize)
{
    int status = SUCCESS;
    /*
     * SANITY CHECK: Make sure the read offset matches the last write offset.
     *		     Would rather die now, than report a false data corruption!
     */
    if ( (dip->di_last_write_offset != offset) ||
	 ((ssize_t)dip->di_last_write_size > 0) && (dip->di_last_write_size != bsize) ) {
	/* Check for partial read request for matching last write offset! */
	if ( dip->di_last_write_offset == (offset + (Offset_t)bsize) ) {
	    return(status);
	}
	ReportErrorNumber(dip);
	Fprintf(dip, "Programming ERROR: Incorrect I/O offset or size for last write!\n");
	Fprintf(dip, "Expected (write) offset: " FUF ", attempted: %d, actual: %d\n",
		dip->di_last_write_offset, (int)dip->di_last_write_attempted,
		(int)dip->di_last_write_size);
	Fprintf(dip, " Current (read) offset: " FUF ", attempting: %d, actual: %d\n",
		offset, (int)dsize, (int)bsize);
	if (dip->di_history_size) {
	    dump_history_data(dip);
	}
	status = FAILURE;
    }
    return (status);
}

/************************************************************************
 *									*
 * read_data() - Read and optionally verify data read.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
read_data(struct dinfo *dip)
{
    dinfo_t *odip = dip->di_output_dinfo; /* For copy/verify modes. */
#if defined(DT_IOLOCK)
    io_global_data_t *iogp = dip->di_job->ji_opaque;
#endif
    register ssize_t count;
    register size_t bsize, dsize;
    large_t data_limit;
    Offset_t sequential_offset;
    int status = SUCCESS;
    struct dtfuncs *dtf = dip->di_funcs;
    Offset_t lock_offset = 0;
    hbool_t lock_full_range = False;
    hbool_t check_rwbytes = False;
    hbool_t check_write_limit = False;
    lbdata_t lba;
    iotype_t iotype = dip->di_io_type;
    uint32_t loop_usecs;
    struct timeval loop_start_time, loop_end_time;
    int probability_random = 0;
    int random_percentage = (dip->di_random_rpercentage) ? dip->di_random_rpercentage : dip->di_random_percentage;

#if defined(DT_IOLOCK)
    /* Note: Temporary until we define a new I/O behavior! */
    if (iogp) {
	return( read_data_iolock(dip) );
    }
#endif /* defined(DT_IOLOCK) */

    dsize = get_data_size(dip, READ_OP);
    data_limit = get_data_limit(dip);

    if (dip->di_random_access) {
	if ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
	    dip->di_offset = set_position(dip, (Offset_t)dip->di_rdata_limit, False);
	    if (odip) {
		odip->di_offset = set_position(odip, (Offset_t)odip->di_rdata_limit, False);
	    }
	}
	lba = get_lba(dip);
	sequential_offset = dip->di_offset = get_position(dip);
	if (odip && odip->di_random_access) {
	    odip->di_offset = get_position(odip);
	}
    } else {
	lba = make_lbdata(dip, dip->di_offset);
    }
    if ( dip->di_last_fbytes_written && dip->di_random_access ) {
	if ( dip->di_files_read == (dip->di_last_files_written - 1) ) {
	    check_write_limit = True;
	    if (dip->di_eDebugFlag) {
		Printf(dip, "DEBUG: Limiting data read on file #%d to " FUF " bytes from last written.\n",
		       (dip->di_files_read + 1), dip->di_last_fbytes_written);
	    }
	}
    }
    
    /* Prime the common btag data, except for IOT pattern. */
    if ( (dip->di_btag_flag == True) && (dip->di_iot_pattern == False) ) {
	update_btag(dip, dip->di_btag, dip->di_offset,
		    (uint32_t)0, (size_t)0, (dip->di_records_read + 1));
    }
    if ( (dip->di_lock_files == True) && dt_test_lock_mode(dip, LOCK_RANGE_FULL) ) {
	lock_full_range = True;
	lock_offset = dip->di_offset;
	status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				LOCK_TYPE_READ, lock_offset, (Offset_t)data_limit);
	if (status == FAILURE) return(status);
    }
    if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	dip->di_actual_total_usecs = 0;
	dip->di_target_total_usecs = 0;
    }

    /*
     * Now read and optionally verify the input records.
     */
    while ( (dip->di_error_count < dip->di_error_limit) &&
	    (dip->di_fbytes_read < data_limit) &&
	    (dip->di_records_read < dip->di_record_limit) ) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_start_time, NULL);
	    if (dip->di_records_read) {
		/* Adjust the actual usecs to adjust for possible usleep below! */
		dip->di_actual_total_usecs += timer_diff(&loop_end_time, &loop_start_time);
	    }
	}

	if ( dip->di_max_data && (dip->di_maxdata_read >= dip->di_max_data) ) {
	    dip->di_maxdata_reached = True;
	    break;
	}

	if ( dip->di_volumes_flag &&
	     (dip->di_multi_volume >= dip->di_volume_limit) &&
	     (dip->di_volume_records >= dip->di_volume_records)) {
	    break;
	}

	if (random_percentage) {
	    probability_random = (int)(get_random(dip) % 100);
	    if (probability_random < random_percentage) {
		iotype = RANDOM_IO;
	    } else {
		iotype = SEQUENTIAL_IO;
		dip->di_offset = sequential_offset;
	    }
	}

	if (dip->di_read_delay) {			/* Optional read delay.	*/
	    mySleep(dip, dip->di_read_delay);
	}

	/*
	 * If data limit was specified, ensure we don't exceed it.
	 */
	if ( (dip->di_fbytes_read + dsize) > data_limit) {
	    bsize = (size_t)(data_limit - dip->di_fbytes_read);
	} else {
	    bsize = dsize;
	}

	if ( (iotype == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
	    bsize = (size_t)MIN((dip->di_offset - dip->di_file_position), (Offset_t)bsize);
	    dip->di_offset = set_position(dip, (Offset_t)(dip->di_offset - bsize), False);
	    if (odip) {
		odip->di_offset = set_position(odip, (Offset_t)(odip->di_offset - bsize), False);
	    }
	} else if (iotype == RANDOM_IO) {
	    /*
	     * BEWARE: The size *must* match the write size, or you'll get
	     * a different offset, since the size is used in calculations.
	     */
	    dip->di_offset = do_random(dip, True, bsize);
	    if (odip) {
		odip->di_offset = dip->di_offset;
		set_position(odip, odip->di_offset, False);
	    }
	}

	/*
	 * If we wrote data, ensure we don't read more than we wrote.
	 */
	if (check_write_limit) {
	    if ( (dip->di_fbytes_read + bsize) > dip->di_last_fbytes_written) {
		dsize = bsize;	/* Save the original intended size. */
		bsize = (size_t)(dip->di_last_fbytes_written - dip->di_fbytes_read);
		check_rwbytes = True;
		if (bsize == (size_t) 0) {
		    set_Eof(dip);
		    break;
		}
		status = check_last_write_info(dip, dip->di_offset, bsize, dsize);
		if (status == FAILURE) break;
	    }
	}

        if (dip->di_debug_flag && (bsize != dsize) && !dip->di_variable_flag) {
            Printf (dip, "Record #%lu, Reading a partial record of %lu bytes...\n",
                                    (dip->di_records_read + 1), bsize);
        }

	if (dip->di_iot_pattern || dip->di_lbdata_flag) {
	    lba = make_lbdata(dip, (Offset_t)(dip->di_volume_bytes + dip->di_offset));
	}

	/*
	 * If requested, rotate the data buffer through ROTATE_SIZE bytes
	 * to force various unaligned buffer accesses.
	 */
	if (dip->di_rotate_flag) {
	    dip->di_data_buffer = (dip->di_base_buffer + (dip->di_rotate_offset++ % ROTATE_SIZE));
	}

	/*
	 * If we'll be doing a data compare after the read, then
	 * fill the data buffer with the inverted pattern to ensure
	 * the buffer actually gets written into (driver debug mostly).
	 */
	if ( (dip->di_io_mode == TEST_MODE) && (dip->di_compare_flag == True) ) {
	    /* Note: Initializing the data buffer moved to read_record()! */
	    init_padbytes(dip->di_data_buffer, bsize, ~dip->di_pattern);
	    if (dip->di_iot_pattern) {
		if (dip->di_btag) {
		    update_buffer_btags(dip, dip->di_btag, dip->di_offset,
					dip->di_pattern_buffer, bsize, (dip->di_records_read + 1));
		}
		lba = init_iotdata(dip, dip->di_pattern_buffer, bsize, lba, dip->di_lbdata_size);
	    }
	}

	if (dip->di_Debug_flag) {
	    report_io(dip, READ_MODE, dip->di_data_buffer, bsize, dip->di_offset);
	}
	
	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    lock_offset = dip->di_offset;
	    /* Lock a partial byte range! */
	    status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				    LOCK_TYPE_READ, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}

	dip->di_retry_count = 0;
	do {
	    count = read_record(dip, dip->di_data_buffer, bsize, dsize, dip->di_offset, &status);
	} while (status == RETRYABLE);
	if (dip->di_end_of_file) break;		/* Stop reading at end of file. */

	if (status == FAILURE) {
	    if (dip->di_error_count >= dip->di_error_limit) break;
	} else if (dip->di_io_mode == COPY_MODE) {
	    ssize_t wcount = copy_record(odip, dip->di_data_buffer, count, odip->di_offset, &status);
	    /* TODO: Need to cleanup multiple device support! */
	    /* For now, propagate certain information to reader. */
	    if (odip->di_end_of_file) {
		dip->di_end_of_file = odip->di_end_of_file;
		if (dip->di_fsfile_flag) {
		    /* Note: Not trying to handle file system full, too messy! */
		    /* Failing at this point is a must, to avoid false corruptions! */
		    Eprintf(dip, "The file system is full, failing the copy operation!\n");
		    return(FAILURE);
		} else {
		    break; /* For disks, stop I/O at end of media. */
		}
	    }
	    if (status == FAILURE) {	/* Write failed! */
		dip->di_error_count++;
	    } else if (wcount != count) {
		Wprintf(dip, "Partial write, write count %d < read count %d, failing!\n", wcount, count);
		Eprintf(dip, "Partial writes are NOT supported, failing the copy operation!\n");
		return(FAILURE);
	    }
	    if ( (dip->di_error_count >= dip->di_error_limit) || dip->di_end_of_file) break;
	} else if (dip->di_io_mode == VERIFY_MODE) {
	    ssize_t rcount = verify_record(odip, dip->di_data_buffer, count, odip->di_offset, &status);
	    if (status == FAILURE) {
		dip->di_error_count++;
	    } else if (odip->di_end_of_file) {
		dip->di_end_of_file = odip->di_end_of_file;
	    }
	    if ( (dip->di_error_count >= dip->di_error_limit) || dip->di_end_of_file) break;
	}

	/*
	 * Verify the data (unless disabled).
	 */
	if ( (status != FAILURE) && dip->di_compare_flag && (dip->di_io_mode == TEST_MODE) ) {
	    ssize_t vsize = count;
	    status = (*dtf->tf_verify_data)(dip, dip->di_data_buffer, vsize, dip->di_pattern, &lba, False);
	    /*
	     * Verify the pad bytes (if enabled).
	     */
	    if ( (status == SUCCESS) && dip->di_pad_check) {
		(void) verify_padbytes(dip, dip->di_data_buffer, vsize, ~dip->di_pattern, bsize);
	    }
	}

	/*
	 * If we had a partial transfer, perhaps due to an error, adjust
	 * the logical block address in preparation for the next request.
	 */
	if ( (status != FAILURE) && dip->di_iot_pattern && ((size_t)count < bsize)) {
	    size_t resid = (bsize - count);
	    lba -= (lbdata_t)howmany((lbdata_t)resid, dip->di_lbdata_size);
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

	dip->di_records_read++;
	dip->di_volume_records++;

	if (dip->di_io_dir == FORWARD) {
	    if (count > 0) {
		dip->di_offset += count;	/* Maintain our own position too! */
		if (odip) odip->di_offset += count;
	    }
	} else if ( (iotype == SEQUENTIAL_IO) &&
		    (dip->di_offset == (Offset_t)dip->di_file_position) ) {
	    set_Eof(dip);
	    break;
	}

	if (dip->di_step_offset) {
	    if (dip->di_io_dir == FORWARD) {
		dip->di_offset = set_position(dip, (dip->di_offset + dip->di_step_offset), True);
		if (odip) odip->di_offset = set_position(dip, (odip->di_offset + odip->di_step_offset), True);
		/* Linux returns EINVAL when seeking too far! */
		if (dip->di_offset == (Offset_t)-1) {
		    set_Eof(dip);
		    break;
		}
		/* Note: See comments in dtwrite.c WRT this logic! */
		if ( dip->di_slices &&
		     ((dip->di_offset + (Offset_t)dsize) >= dip->di_end_position) ) {
		    set_Eof(dip);
		    break;
		}
	    } else {
		dip->di_offset -= dip->di_step_offset;
		if (dip->di_offset <= (Offset_t)dip->di_file_position) {
		    set_Eof(dip);
		    break;
		}
		if (odip) {
		    odip->di_offset -= odip->di_step_offset;
		    if (odip->di_offset <= (Offset_t)odip->di_file_position) {
			set_Eof(dip); /* Stop reading! */
			break;
		    }
		}
	    }
	}

	/*
	 * For regular files, if we've read as much as we've written,
	 * then set a fake EOF to stop this read pass.
	 */
	if ( check_rwbytes &&
	     (dip->di_fbytes_read == dip->di_last_fbytes_written) ) {
	    set_Eof(dip);
	    break;
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

/************************************************************************
 *									*
 * read_data_iolock() - Read and optionally verify data read.		*
 *									*
 * Description: 						        * 
 * 	This function supports reading with multiple threads.	        * 
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
read_data_iolock(struct dinfo *dip)
{
    io_global_data_t *iogp = dip->di_job->ji_opaque;
    register ssize_t count;
    register size_t bsize, dsize;
    large_t data_limit;
    int status = SUCCESS;
    struct dtfuncs *dtf = dip->di_funcs;
    Offset_t lock_offset = 0;
    hbool_t lock_full_range = False;
    hbool_t check_rwbytes = False;
    u_long io_record = 0;
    lbdata_t lba;
    iotype_t iotype = dip->di_io_type;
    uint32_t loop_usecs;
    struct timeval loop_start_time, loop_end_time;
    int probability_random = 0;
    int random_percentage = (dip->di_random_rpercentage) ? dip->di_random_rpercentage : dip->di_random_percentage;

    dsize = get_data_size(dip, READ_OP);
    data_limit = get_data_limit(dip);

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

    /* Prime the common btag data, except for IOT pattern. */
    if ( (dip->di_btag_flag == True) && (dip->di_iot_pattern == False) ) {
	update_btag(dip, dip->di_btag, dip->di_offset, (uint32_t)0, (size_t)0, io_record);
    }
    if ( (dip->di_lock_files == True) && dt_test_lock_mode(dip, LOCK_RANGE_FULL) ) {
	lock_full_range = True;
	lock_offset = dip->di_offset;
	status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				LOCK_TYPE_READ, lock_offset, (Offset_t)data_limit);
	if (status == FAILURE) return(status);
    }
    if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	dip->di_actual_total_usecs = 0;
	dip->di_target_total_usecs = 0;
    }

    /*
     * Now read and optionally verify the input records.
     */
    while ( (iogp->io_end_of_file == False) &&
	    (dip->di_error_count < dip->di_error_limit) &&
	    (iogp->io_bytes_read < data_limit) &&
	    (iogp->io_records_read < dip->di_record_limit) ) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_start_time, NULL);
	    if (dip->di_records_read) {
		/* Adjust the actual usecs to adjust for possible usleep below! */
		dip->di_actual_total_usecs += timer_diff(&loop_end_time, &loop_start_time);
	    }
	}

	if ( dip->di_max_data && (dip->di_maxdata_read >= dip->di_max_data) ) {
	    dip->di_maxdata_reached = True;
	    break;
	}

	if ( dip->di_volumes_flag &&
	     (dip->di_multi_volume >= dip->di_volume_limit) &&
	     (dip->di_volume_records >= dip->di_volume_records)) {
	    break;
	}

	(void)dt_acquire_iolock(dip, iogp);
	/*
	 * Setup the random/sequential percentages (if enabled).
	 */
	if (random_percentage) {
	    probability_random = (int)(get_random(dip) % 100);
	    if (probability_random < random_percentage) {
		iotype = RANDOM_IO;
	    } else {
		iotype = SEQUENTIAL_IO;
		dip->di_offset = iogp->io_sequential_offset;
	    }
	}

	if (dip->di_read_delay) {			/* Optional read delay.	*/
	    mySleep(dip, dip->di_read_delay);
	}

	/*
	 * With multiple threads, we must check limits after unlocking.
	 */
	if ( (iogp->io_end_of_file == True) ||
	     (iogp->io_bytes_read >= data_limit) ||
	     (iogp->io_records_read >= dip->di_record_limit) ) {
	    set_Eof(dip);
	    iogp->io_end_of_file = dip->di_end_of_file;
	    (void)dt_release_iolock(dip, iogp);
	    break;
	}

	/*
	 * If data limit was specified, ensure we don't exceed it.
	 */
	if ( (iogp->io_bytes_read + dsize) > data_limit) {
	    bsize = (size_t)(data_limit - iogp->io_bytes_read);
	} else {
	    bsize = dsize;
	}

       if (iotype == SEQUENTIAL_IO) {
	    dip->di_offset = iogp->io_sequential_offset;
	    if (dip->di_io_dir == REVERSE) {
		bsize = (size_t)MIN((dip->di_offset - dip->di_file_position), (Offset_t)bsize);
		dip->di_offset = set_position(dip, (Offset_t)(dip->di_offset - bsize), False);
		iogp->io_sequential_offset = dip->di_offset;
	    } else {
		iogp->io_sequential_offset += bsize;
	    }
	} else if (iotype == RANDOM_IO) {
	    /*
	     * BEWARE: The size *must* match the write size, or you'll get
	     * a different offset, since the size is used in calculations.
	     */
	    dip->di_offset = do_random(dip, True, bsize);
	}
	iogp->io_bytes_read += bsize;
	iogp->io_records_read++;
	io_record = iogp->io_records_read;

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
		if (offset <= (Offset_t) dip->di_file_position) {
		    set_Eof(dip);
		    dip->di_beginning_of_file = True;
		    break;
		}
	    }
	    iogp->io_sequential_offset = offset;
	}
	(void)dt_release_iolock(dip, iogp);

        if (dip->di_debug_flag && (bsize != dsize) && !dip->di_variable_flag) {
            Printf(dip, "Record #%lu, Reading a partial record of %lu bytes...\n",
		   io_record, bsize);
        }

	if (dip->di_iot_pattern || dip->di_lbdata_flag) {
	    lba = make_lbdata(dip, (Offset_t)(dip->di_volume_bytes + dip->di_offset));
	}

	/*
	 * If requested, rotate the data buffer through ROTATE_SIZE bytes
	 * to force various unaligned buffer accesses.
	 */
	if (dip->di_rotate_flag) {
	    dip->di_data_buffer = (dip->di_base_buffer + (dip->di_rotate_offset++ % ROTATE_SIZE));
	}

	/*
	 * If we'll be doing a data compare after the read, then
	 * fill the data buffer with the inverted pattern to ensure
	 * the buffer actually gets written into (driver debug mostly).
	 */
	if ( (dip->di_io_mode == TEST_MODE) && (dip->di_compare_flag == True) ) {
	    /* Note: Initializing the data buffer moved to read_record()! */
	    init_padbytes(dip->di_data_buffer, bsize, ~dip->di_pattern);
	    if (dip->di_iot_pattern) {
		if (dip->di_btag) {
		    update_buffer_btags(dip, dip->di_btag, dip->di_offset,
					dip->di_pattern_buffer, bsize, io_record);
		}
		lba = init_iotdata(dip, dip->di_pattern_buffer, bsize, lba, dip->di_lbdata_size);
	    }
	}

	if (dip->di_Debug_flag) {
	    large_t iolba = make_lbdata(dip, dip->di_offset);
	    long files = (dip->di_files_read + 1);
	    report_record(dip, files, io_record,
			  iolba, dip->di_offset,
			  READ_MODE, dip->di_data_buffer, bsize);
	}
	
	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    lock_offset = dip->di_offset;
	    /* Lock a partial byte range! */
	    status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
				    LOCK_TYPE_READ, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}

	dip->di_retry_count = 0;
	do {
	    count = read_record(dip, dip->di_data_buffer, bsize, dsize, dip->di_offset, &status);
	} while (status == RETRYABLE);

	if (status == FAILURE) {
	    if (dip->di_error_count >= dip->di_error_limit) break;
	}

	/*
	 * Verify the data (unless disabled).
	 */
	if ( (status != FAILURE) && dip->di_compare_flag && (dip->di_io_mode == TEST_MODE) ) {
	    ssize_t vsize = count;
	    status = (*dtf->tf_verify_data)(dip, dip->di_data_buffer, vsize, dip->di_pattern, &lba, False);
	    /*
	     * Verify the pad bytes (if enabled).
	     */
	    if ( (status == SUCCESS) && dip->di_pad_check) {
		int rc = verify_padbytes(dip, dip->di_data_buffer, vsize, ~dip->di_pattern, bsize);
		if (rc == FAILURE) status = rc;
	    }
	}

	/*
	 * If we had a partial transfer, perhaps due to an error, adjust
	 * the logical block address in preparation for the next request.
	 */
	if (dip->di_iot_pattern && ((size_t)count < bsize)) {
	    size_t resid = (bsize - count);
	    lba -= (lbdata_t)howmany((lbdata_t)resid, dip->di_lbdata_size);
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

	dip->di_records_read++;
	dip->di_volume_records++;

	if (dip->di_io_dir == FORWARD) {
	    dip->di_offset += count;	/* Maintain our own position too! */
	} else if ( (iotype == SEQUENTIAL_IO) &&
		    (dip->di_offset == (Offset_t)dip->di_file_position) ) {
	    set_Eof(dip);
	    break;
	}

	/*
	 * For regular files, if we've read as much as we've written,
	 * then set a fake EOF to stop this read pass.
	 */
	if ( check_rwbytes &&
	     (dip->di_fbytes_read == dip->di_last_fbytes_written) ) {
	    set_Eof(dip);
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
    return(status);
}

#endif /* defined(DT_IOLOCK) */

/************************************************************************
 *									*
 * check_read() - Check status of last read operation.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		count = Number of bytes read.				*
 *		size  = Number of bytes expected.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE/WARNING = Ok/Error/Warning	*
 *									*
 ************************************************************************/
int
check_read(struct dinfo *dip, ssize_t count, size_t size)
{
    int status = SUCCESS;

    if ((size_t)count != size) {
	if (count == FAILURE) {
	    INIT_ERROR_INFO(eip, dip->di_dname, OS_READ_FILE_OP, READ_OP, &dip->di_fd, dip->di_oflags,
			    dip->di_offset, size, os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (dip->di_retrying == True) {
		eip->ei_prt_flags = PRT_NOFLAGS;
		eip->ei_rpt_flags = (RPT_NODEVINFO|RPT_NOHISTORY);
	    }
	    status = ReportRetryableError(dip, eip, "Failed reading %s", dip->di_dname);
	    if (status == RETRYABLE) return(status);
	    if (dip->di_retrying == False) {
		if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		     (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		    (void)ExecuteTrigger(dip, "read");
		}
	    }
	} else {
	    FILE *fp;
	    int prt_flags;
	    logLevel_t log_level;
	    hbool_t short_read_error;
	    /*
	     * Short reads with random I/O are now turned into errors, since
	     * continuing will report a *false* data corruption, when reading
	     * too much past the last record written! (misleading and wrong)
	     * Note: The random I/O offset should no longer encounter EOM.
	    */
	    if ( ((size_t)count < size) && (dip->di_io_type == RANDOM_IO) ) {
		fp = dip->di_efp;
		log_level = logLevelError;
		prt_flags = PRT_SYSLOG;
		short_read_error = True;
	    } else {
		fp = dip->di_ofp;
		log_level = logLevelWarn;
		prt_flags = PRT_NOFLAGS;
		short_read_error = False;
	    }
	    /*
	     * For reads at end of file or reads at end of block
	     * devices, we'll read less than the requested count.
	     * In this case, we'll treat this as a warning since
	     * this is to be expected.  In the case of tape, the
	     * next read will indicate end of tape (in my driver).
	     *
	     * NOTE:  The raw device should be used for disks.
	     */
	    if ( (dip->di_debug_flag || dip->di_verbose_flag || ((size_t)count > size)) &&
		 (dip->di_io_mode == TEST_MODE) ) {
		if (dip->di_multiple_files) {
		    LogMsg(dip, fp, log_level, prt_flags,
			   "File %s, record #%lu, offset "FUF", attempted to read %lu bytes, read only %lu bytes.\n",
			   dip->di_dname, (dip->di_records_read + 1), dip->di_offset, size, count);
		} else {
		    LogMsg(dip, fp, log_level, prt_flags,
			   "Record #%lu, offset "FUF", attempted to read %lu bytes, read only %lu bytes.\n",
			   (dip->di_records_read + 1), dip->di_offset, size, count);
		}
	    }
	    if (short_read_error == True) {
		INIT_ERROR_INFO(eip, dip->di_dname, OS_READ_FILE_OP, READ_OP, &dip->di_fd, dip->di_oflags,
				dip->di_offset, size, SUCCESS, log_level, prt_flags, RPT_NOFLAGS);
		(void)ReportErrorInfoX(dip, eip, NULL);
	    } else {
		if ((size_t)count < size) {	/* Partial read is a warning. */
		    dip->di_warning_errors++;
		    return (WARNING);
		}
		ReportDeviceInfo(dip, count, 0, False, NotMismatchedData);
		RecordErrorTimes(dip, True);
	    }
	}
	dip->di_read_errors++;
	status = FAILURE;
    }
    return (status);
}

/*
 * This function is envoked when reading multiple tape files, to
 * position past an expected file mark.  This is especially important
 * when using the lbdata or iot options, since encountering an expected
 * EOF throws off the offset being maintained, resulting in an lba error.
 */
int
read_eof(struct dinfo *dip)
{
    ssize_t count;
    size_t bsize = dip->di_block_size;
    int status = SUCCESS;

    if (dip->di_debug_flag) {
	Printf(dip, "Processing end of file... [file #%lu, record #%lu]\n",
			(dip->di_files_read + 1), (dip->di_records_read + 1));
    }
    dip->di_eof_processing = True;
    dip->di_retry_count = 0;
    do {
	count = read_record(dip, dip->di_data_buffer, bsize, bsize, dip->di_offset, &status);
    } while (status == RETRYABLE);
    dip->di_eof_processing = False;
    if (!dip->di_end_of_file) {
	Fprintf(dip, "ERROR: File %lu, Record %lu, expected EOF was NOT detected!\n",
		(dip->di_files_read + 1), (dip->di_records_read + 1));
	ReportDeviceInfo (dip, count, 0, False, NotMismatchedData);
	RecordErrorTimes(dip, True);
	dip->di_read_errors++;
	status = FAILURE;
    }
    return (status);
}

/*
 * This function is called after EOF is detected, to read the next record
 * which checks for reaching the end of logical tape (i.e. two successive
 * file marks).  For multi-volume tapes, the user will be prompted for the
 * next volume via read_record(), and the end of file flag gets reset when
 * the tape is re-open'ed.
 */
int
read_eom(struct dinfo *dip)
{
    ssize_t count;
    size_t bsize = dip->di_block_size;
    int status = SUCCESS;

    if (dip->di_debug_flag) {
	Printf(dip, "Processing end of media... [file #%lu, record #%lu]\n",
			(dip->di_files_read + 1), (dip->di_records_read + 1));
    }
    dip->di_eom_processing = True;
    do {
	count = read_record(dip, dip->di_data_buffer, bsize, bsize, dip->di_offset, &status);
    } while (status == RETRYABLE);
    dip->di_eom_processing = False;

    if (dip->di_multi_flag) {
	if (dip->di_end_of_file) {
	    Fprintf(dip, "ERROR: File %lu, Record %lu, expected EOM was NOT detected!\n",
			(dip->di_files_read + 1), (dip->di_records_read + 1));
	    ReportDeviceInfo(dip, count, 0, False, NotMismatchedData);
	    RecordErrorTimes(dip, True);
	    return (FAILURE);
	}
    } else if ( !dip->di_end_of_logical ) {
	Fprintf(dip, "ERROR: File %lu, Record %lu, expected EOM was NOT detected!\n",
		(dip->di_files_read + 1), (dip->di_records_read + 1));
	ReportDeviceInfo(dip, count, 0, False, NotMismatchedData);
	RecordErrorTimes(dip, True);
	dip->di_read_errors++;
	return (FAILURE);
    }
    return (SUCCESS);	/* We don't care about the read status! */
}

/************************************************************************
 *									*
 * read_record() - Read record from device or file.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		buffer = The data buffer to read into.			*
 *		bsize = The number of bytes read.			*
 * 		dsize = The users' requested size.		        * 
 * 		offset = The starting record offset.		        * 
 *		status = Pointer to return status.			*
 *									*
 * Outputs:	status = SUCCESS/FAILURE/WARNING = Ok/Error/Warning	*
 *		Return value is number of bytes from read() request.	*
 * 		The status may also be RETRYABLE for retryable errors.	* 
 *									*
 ************************************************************************/
ssize_t
read_record (	struct dinfo	*dip,
		u_char		*buffer,
		size_t		bsize,
		size_t		dsize,
		Offset_t	offset,
		int		*status )
{
    ssize_t count;

    /*
     * To catch cases where read data is NOT returned properly, initialize 
     * the read buffer with a data pattern. 
     * Note: To avoid the performance hit, this is not always done! 
     */
    if ( (dip->di_compare_flag == True) && (dip->di_prefill_buffer == True) ) {
	uint32_t pattern = (dip->di_prefill_pattern) ? dip->di_prefill_pattern : (uint32_t)dip->di_thread_number;
	if (dip->di_poison_buffer) {
	    poison_buffer(dip, buffer, bsize, pattern);
	} else {
	    init_buffer(dip, buffer, bsize, pattern);
	}
    }

retry:
    *status = SUCCESS;

    ENABLE_NOPROG(dip, READ_OP);
    if (dip->di_nvme_io_flag == True) {
	count = nvmeReadData(dip, buffer, bsize, offset);
    } else if (dip->di_scsi_io_flag == True) {
	count = scsiReadData(dip, buffer, bsize, offset);
    } else if (dip->di_random_access == False) {
	count = os_read_file(dip->di_fd, buffer, bsize);
    } else {
	count = os_pread_file(dip->di_fd, buffer, bsize, offset);
    }
    DISABLE_NOPROG(dip);

    if (dip->di_history_size && (dip->di_retrying == False)) {
	/* Note: We may be in write mode, used during read-after-write! */
	hbool_t read_mode = (dip->di_mode == READ_MODE);
	long files, records;
	/* 
	 * Note: We cannot report read/write records with percentage, otherwise 
	 * the record numbers will NOT match extended error reporting and btags! 
	 */
	if ( False /*dip->di_read_percentage*/ ) {
	    files = (dip->di_files_read + dip->di_files_written) + 1;
	    records = (dip->di_records_read + dip->di_records_written) + 1;
	} else {
	    files = (read_mode) ? (dip->di_files_read + 1) : (dip->di_files_written + 1);
	    /* Note: The records written has already been bumped! */
	    records = (read_mode) ? (dip->di_records_read + 1) : dip->di_records_written;
	}
	save_history_data(dip, files, records, READ_MODE, offset, buffer, bsize, count);
    }

    if ( ( (count == 0) || (count == FAILURE) ) &&
	 ( is_Eof(dip, count, bsize, status) ) ) {
	if (dip->di_multi_flag &&
	    (!dip->di_stdin_flag || (dip->di_ftype == OUTPUT_FILE)) ) {
	    if ( (dip->di_dtype->dt_dtype == DT_TAPE) && !dip->di_end_of_logical ) {
		return (count);	/* Expect two file marks @ EOM. */
	    }
	    *status = HandleMultiVolume (dip);
	    dip->di_offset = (Offset_t) 0;
	    if ( !dip->di_eof_processing && !dip->di_eom_processing ) {
		if (*status == SUCCESS) goto retry;
	    }
	}
	return (count);	/* Stop reading at end of file. */
    }

    if ( dip->di_eof_processing || dip->di_eom_processing ) {
	return (count);
    }
    dip->di_end_of_file = False;	/* Reset saved end of file state. */

    /* Force a FALSE corruption (if requested), and records match! */
    if (dip->di_force_corruption && (dip->di_corrupt_reads == (dip->di_records_read + 1)) ) {
	corrupt_buffer(dip, buffer, (int32_t)count, dip->di_corrupt_reads);
    }

    /*
     * If something was read, adjust counts and statistics.
     */
    if (count > (ssize_t) 0) {
	dip->di_dbytes_read += count;
	dip->di_fbytes_read += count;
	dip->di_vbytes_read += count;
	dip->di_maxdata_read += count;
	if ((size_t)count == dsize) {
	    dip->di_full_reads++;
	} else {
	    dip->di_partial_reads++;
	}
    }

    *status = check_read(dip, count, bsize);

    return (count);
}

/*
 * verify_record() - Verify record with selected output device/file.
 *
 * Description:
 * 	This function is called for MIRROR and VERIFY modes, to copy the
 * data from the source device with the target device.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *	buffer = The data buffer to compare. (the source device data)
 * 	bsize = The number of bytes read.
 * 	offset = The current file offset.
 *	status = Pointer to status variable.
 *
 * Outputs:
 * 	status = SUCCESS/FAILURE/WARNING = Ok/Error/Warning
 *	Return value is number of bytes from read() request.
 */
ssize_t
verify_record (	struct dinfo	*dip,
		uint8_t		*buffer,
		size_t		bsize,
		Offset_t	offset,
		int		*status )
{
    struct dtfuncs *dtf = dip->di_funcs;
    lbdata_t lba = make_lba(dip, offset);
    uint8_t *pptr, *pend, *pbase = dip->di_pattern_buffer;
    size_t psize;
    ssize_t count;

    if (dip->di_Debug_flag) {
	report_io(dip, READ_MODE, dip->di_data_buffer, bsize, offset);
    }

    /* Save the current pattern information (if any). */
    if (pbase) {
	pptr = dip->di_pattern_bufptr;
	pend = dip->di_pattern_bufend;
	psize = dip->di_pattern_bufsize;
    }
    /*
     * TODO: Re-write this using the verify buffer (when I have time).
     */
    dip->di_retry_count = 0;
    do {
	count = read_record(dip, dip->di_data_buffer, bsize, bsize, offset, status);
    } while (*status == RETRYABLE);
    if ( (*status == FAILURE) || dip->di_end_of_file) return(count);

    /* 
     * Setup the pattern buffer with the expected data buffer. 
     *  
     * Yea *real* ugly, but required to use existing code.
     * TODO: Create data structure for verify functions to use!
     */
    setup_pattern(dip, buffer, count, False);

    *status = (*dtf->tf_verify_data)(dip, dip->di_data_buffer, count, dip->di_pattern, &lba, True);

    /* Restore the previous pattern buffer (if any). */
    if (dip->di_pattern_buffer = pbase) {
	dip->di_pattern_bufptr = pptr;
	dip->di_pattern_bufend = pend;
	dip->di_pattern_bufsize = psize;
    }
    dip->di_records_read++;
    return(count);
}

/************************************************************************
 *									*
 * FindCapacity() - Find capacity of a random access device.	        * 
 * 								        * 
 * Description:							        * 
 * 	This function is called anytime the disk capacity is required,	*
 * which includes random I/O and multiple slices. If a capacity exists	*
 * from user defined or OS obtained, use this value, otherwise use a	*
 * series of seek/reads to find the disk capacity.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Fills in device capacity and data limit on success.	*
 *									*
 * Return Value: Returns SUCCESS/FAILURE/WARNING = Ok/Error/Warning	*
 *									*
 ************************************************************************/
//#define DEBUG 1
int
FindCapacity(struct dinfo *dip)
{
    register u_int32 dsize = dip->di_rdsize;
    Offset_t	lba, max_seek = (MAX_SEEK - dsize), offset;
    long	adjust = ((250 * MBYTE_SIZE) / dsize);
    int		attempts = 0;
    ssize_t	count, last = 0;
    large_t	capacity_bytes = 0;
    large_t	user_capacity = dip->di_user_capacity;
    u_char	*buffer;
    int		mode = O_RDONLY;
    HANDLE	fd, saved_fd = NoFd;
    int		status = SUCCESS;
    hbool_t	temp_fd = False;
    hbool_t	expect_error = True;

//#if defined(DEBUG)
    /* This comes in handy at times, for debugging! */
    if (dip->di_debug_flag && dip->di_Debug_flag) {
	Printf(dip, "FindCapacity: offset = "FUF"\n", dip->di_file_position);
	Printf(dip, "FindCapacity: rdsize = %u\n", dip->di_rdsize);
	Printf(dip, "FindCapacity: data_limit = " LUF "\n", dip->di_data_limit);
	Printf(dip, "FindCapacity: rdata_limit = " LUF "\n", dip->di_rdata_limit);
	Printf(dip, "FindCapacity: user_capacity = " LUF "\n", dip->di_user_capacity);
	Printf(dip, "FindCapacity: slices = %u, slice_number = %d\n", dip->di_slices, dip->di_slice_number);
    }
//#endif /* defined(DEBUG) */

    /* 
     * For disks, ensure we don't exceed the capacity, esp. for slices and random I/O. 
     *  
     * Note: This must be done early, esp. to apply capacity percentage, and setup limits. 
     * But for random I/O and slices, we must be in range of disk blocks to avoid false seek 
     * failures, due to accessing non-existant offsets! Understood? (Note to self! ;) 
     *  
     * Please Note: This is tricky logic, and if not done right, leads to false failures! 
     */
    if ( isDiskDevice(dip) && user_capacity && dip->di_file_position) {
        /* All this setup is done magically by functions in dtinfo.c! */
        uint32_t disk_block_size = (dip->di_rdsize) ? dip->di_rdsize : BLOCK_SIZE;
	large_t capacity_bytes = (dip->di_capacity * disk_block_size);
        /* Our goal here is to make the adjust capacity, look like a smaller disk! */
        large_t disk_capacity = (capacity_bytes - dip->di_file_position);
        /* Note: This user capacity is overloaded, cleanup one day! */
        user_capacity = min(disk_capacity, user_capacity);
    }
    /*
     * The user capacity is setup for disk devices, so this appears to be the best common 
     * location to calculate capacity percentage. 
     */
    if (dip->di_capacity_percentage && (user_capacity || dip->di_user_capacity) ) {
	user_capacity = SetupCapacityPercentage(dip, user_capacity);
    }
    if ( user_capacity && (dip->di_user_capacity != user_capacity) ) {
	if (dip->di_debug_flag || dip->di_Debug_flag || dip->di_rDebugFlag) {
	    Printf (dip, "Previous user capacity "LDF", adjusted capacity "LDF"\n",
		    dip->di_user_capacity, user_capacity);
	}
        dip->di_user_capacity = user_capacity;
    }

    /* Note; For SCSI I/O, the user capacity should already be setup! */
    /*
     * Use the user specified capacity (if specified).
     * 
     * Note: The user_capacity will be set from the OS specific API's for disks,
     * or with the current file size (if it already exists) for regular files.
     */
    if (dip->di_user_capacity) {
	SetupTransferLimits(dip, dip->di_user_capacity);
	goto set_random_limit;
    } else if (dip->di_data_limit && (dip->di_data_limit != INFINITY) ) {
	goto set_random_limit;
    }

    if (dip->di_debug_flag || dip->di_Debug_flag || dip->di_rDebugFlag) {
	Printf (dip, "Attempting to calculate capacity via seek/read algorithm...\n");
    }
    
    buffer = Malloc(dip, dsize);
    if (buffer == NULL) return(FAILURE);

    /*
     * If the device is open in write mode, open another file descriptor for reading.
     */
    if ( dip->di_aio_flag || (dip->di_fd == NoFd) || (dip->di_mode == WRITE_MODE) ) {
	temp_fd = True;
	saved_fd = dip->di_fd;
	if ( (fd = open(dip->di_dname, O_RDONLY, 0)) == NoFd) {
	    Fprintf(dip, "Failed to open device %s for reading!\n", dip->di_dname);
	    ReportErrorInfo(dip, dip->di_dname, os_get_error(), "FindCapacity() "OS_OPEN_FILE_OP, OPEN_OP, False);
	    Free(dip, buffer);
	    return (FAILURE);
	}
	dip->di_fd = fd;
    }

    /*
     * Algorithm:
     *	There maybe a better may, but this works...
     */
    lba = adjust;
    adjust /= 2;

    while (True) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

	attempts++;
#if defined(DEBUG)
	Printf(dip, "lba = " LUF ", adjust = %lu\n", lba, adjust);
#endif /* defined(DEBUG) */
	/*
	 * We cannot seek past the maximum allowable file position,
	 * otherwise lseek() fails, and 'dt' exits.  So, we must
	 * limit seeks appropriately, and break out of this loop
	 * if we hit the upper seek limit.
	 */
	if ( (Offset_t)(lba * dsize) < (Offset_t)0 ) {
	    lba = (max_seek / dsize);
	}
	/* Note: Linux returns EINVAL for seeks past end of media! */
	offset = set_position(dip, (Offset_t)(lba * dsize), expect_error);
	/*
	 * If seek fails, probably too big, adjust below and retry.
	 */
	if (offset == (Offset_t) -1) {
	    count = -1;
#if defined(DEBUG)
	    Printf(dip, "seek failed: offset = "FUF", lba = " LUF ", adjust = %lu, errno = %d\n",
		   (Offset_t)(lba * dsize), lba, adjust, errno);
#endif /* defined(DEBUG) */
	} else {
	    count = read(dip->di_fd, buffer, dsize);
	}
	if (count == dsize) {
	    if (lba == (Offset_t)(max_seek / dsize)) break;
	    lba += adjust;
	    if (adjust == 1) break;
	} else {
	    int error = os_get_error();
	    hbool_t isEof = os_isEof(count, error);
	    if ( (isEof == True) || (offset == (Offset_t)-1) ) {
		if (last) adjust /= 2;
		if (!adjust) adjust++;
		lba -= adjust;
                if (lba == 1) {
                    Fprintf(dip, "The LBA has reached one (1), which likely indicates an issue.\n");
                    Eprintf(dip, "The find capacity logic expects at least one read() to succeed!\n");
                    status = FAILURE;
                    break;
                }
	    } else {
		ReportErrorInfo(dip, dip->di_dname, error, "FindCapacity() "OS_READ_FILE_OP, READ_OP, False);
		status = FAILURE;
		break;
	    }
	}
	last = count;
    }
    Free(dip, buffer);
    if (temp_fd) {
	(void)close(dip->di_fd);
	dip->di_fd = saved_fd;
    } else {
	(void)set_position(dip, (Offset_t) 0, False);
    }

    /*
     * If the read failed, set the lba to the last successful read,
     * and continue.  Won't be perfect, but at least we can run.
     * Note: Workaround for problem seen on Linux w/ATAPI CD-ROM's.
     */
    if (status == FAILURE) {
#if defined(DEBUG)
	Printf(dip, "failed, last good lba was " LUF ", adjust was %ld\n",
							 lba, adjust);
#endif /* defined(DEBUG) */
	lba -= adjust;
	if (lba) {
	    exit_status = SUCCESS;	/* We continue after reporting errors! */
	} else {
	    return(status);
	}
    }

#if defined(DEBUG)
    Printf (dip, "read attempts was %d, the max lba is " LUF "\n", attempts, lba);
#endif /* defined(DEBUG) */

    capacity_bytes = (large_t)(lba * dsize);
    if (dip->di_capacity_percentage && capacity_bytes) {
	dip->di_user_capacity = SetupCapacityPercentage(dip, capacity_bytes);
    }
    SetupTransferLimits(dip, capacity_bytes);

set_random_limit:
    /*
     * The proper data limit is necessary for random I/O processing.
     */
    if (dip->di_random_io) {
	if ( (dip->di_rdata_limit == (large_t)0) || (dip->di_rdata_limit > dip->di_data_limit) ) {
	    dip->di_rdata_limit = dip->di_data_limit;
	}
	if (dip->di_debug_flag || dip->di_Debug_flag || dip->di_rDebugFlag || (status == FAILURE)) {
	    Printf (dip, "Random data limit set to " LUF " bytes (%.3f Mbytes), " LUF " blocks.\n",
		dip->di_rdata_limit, ((double)dip->di_rdata_limit/(double)MBYTE_SIZE), (dip->di_rdata_limit / dsize));
	}
#if 0
        /* This is no longer a valid check, since we adjust the capacity based on file position. */
	if (dip->di_rdata_limit <= (large_t)dip->di_file_position) {
	    LogMsg (dip, dip->di_efp, logLevelCrit, 0,
		    "Please specify a random data limit (" LUF ") > file position (" FUF ")!\n",
		    dip->di_rdata_limit, dip->di_file_position);
	    return(FAILURE);
	}
#endif /* 0 */
    } else if (dip->di_debug_flag || dip->di_Debug_flag || (status == FAILURE)) {
	    Printf (dip, "Data limit set to " LUF " bytes (%.3f Mbytes), " LUF " blocks.\n",
		    dip->di_data_limit, ((double)dip->di_data_limit/(double)MBYTE_SIZE),
		    (dip->di_data_limit / dsize));
#if 0
        /* This is not a valid check if data limits or capacity percentages are specified! */
	if ((large_t)dip->di_file_position > dip->di_data_limit) {
	    LogMsg (dip, dip->di_efp, logLevelCrit, 0,
		    "Please specify a file position (" FUF ") < data limit (" LUF ")!\n",
		    dip->di_file_position, dip->di_data_limit);
	    return(FAILURE);
	}
#endif /* 0 */
    }
    return (SUCCESS);
}

large_t
SetupCapacityPercentage(dinfo_t *dip, large_t bytes)
{
    large_t data_limit;
    data_limit = (large_t)((double)bytes * ((double)dip->di_capacity_percentage / 100.0));
    return( data_limit );
}

void
SetupTransferLimits(dinfo_t *dip, large_t bytes)
{
    if (bytes) {
	/* Assumes the device type defaults have been setup! */
	dip->di_capacity = (bytes / dip->di_rdsize);
	dip->di_storage_size = bytes;
	dip->di_data_limit = dip->di_storage_size;
	if (!dip->di_record_limit) dip->di_record_limit = INFINITY;
    }
    return;
}
