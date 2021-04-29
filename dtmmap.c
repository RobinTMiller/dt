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
 * Module:	dtmmap.c
 * Author:	Robin T. Miller
 * Date:	September 4, 1993
 *
 * Description:
 *	Functions to do memory mapped I/O for 'dt' program.
 *
 * Modification History:
 *
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#if defined(MMAP)

#include "dt.h"
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>

#if !defined(MAP_FILE)
#  define MAP_FILE	0
#endif /* !defined(MAP_FILE) */

/*
 * Forward References:
 */
void reference_data (u_char *buffer, size_t count);

/*
 * Declare the memory mapped test functions.
 */
struct dtfuncs mmap_funcs = {
    /*	tf_open,		tf_close,		tf_initialize,	  */
	open_file,		close_file,		nofunc,
    /*  tf_start_test,		tf_end_test,				  */
	mmap_file,		nofunc,
    /*	tf_read_file,		tf_read_data,		tf_cancel_reads,  */
	read_file,		mmap_read_data,		nofunc,
    /*	tf_write_file,		tf_write_data,		tf_cancel_writes, */
	write_file,		mmap_write_data,	nofunc,
    /*	tf_flush_data,		tf_verify_data,		tf_reopen_file,   */
	mmap_flush,		verify_data,		mmap_reopen_file,
    /*	tf_startup,		tf_cleanup,		tf_validate_opts  */
	nofunc,			nofunc,			mmap_validate_opts
};

/************************************************************************
 *									*
 * mmap_file()	Memory map the input or output file.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Return Value:							*
 *		Returns 0 / -1 = SUCESS / FAILURE.			*
 *									*
 ************************************************************************/
int
mmap_file (struct dinfo *dip)
{
	int fd = dip->di_fd;
	int status = SUCCESS;

	/*
	 * For memory mapped I/O, map the file to a buffer.
	 */
	if (dip->di_mode == READ_MODE) {
	    dip->di_mmap_buffer = mmap(NULL, dip->di_data_limit,
				       PROT_READ, (MAP_FILE|MAP_PRIVATE), fd, (off_t) 0);
	} else { /* Output file */
	    /*
	     * Set the output file to the specified limit before
	     * memory mapping the file.
	     */
	    status = ftruncate(fd, dip->di_data_limit);
	    if (status == FAILURE) {
		ReportErrorInfo(dip, dip->di_dname, os_get_error(), "ftruncate", TRUNCATE_OP, True);
		return(status);
	    }
	    dip->di_mmap_buffer = mmap(NULL, dip->di_data_limit,
				       (PROT_READ|PROT_WRITE),
				       (MAP_FILE|MAP_SHARED), fd, (off_t) 0);
	}
	if (dip->di_mmap_buffer == (u_char *) -1) {
	    ReportErrorInfo(dip, dip->di_dname, os_get_error(), "mmap", MMAP_OP, True);
	    status = FAILURE;
	} else {
	    dip->di_mmap_bufptr = dip->di_mmap_buffer;
	}
	/*
	 * File positioning options currently ignored... maybe later.
	 */
	//dip->di_mmap_bufptr += dip->di_file_position;
	return (status);
}

/************************************************************************
 *									*
 * mmap_flush()	Flush memory map file data to permanent storage.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Return Value:							*
 *		Returns 0 / -1 = SUCCESS / FAILURE.			*
 *									*
 ************************************************************************/
int
mmap_flush (struct dinfo *dip)
{
    int status = SUCCESS;

    /*
     * Sync out modified pages and invalid the address range to
     * force them to be obtained from the file system during the
     * read pass.
     */
    if (dip->di_mode == WRITE_MODE) {
	if (dip->di_noprog_flag && optiming_table[MSYNC_OP].opt_timing_flag) {
	    dip->di_optype = MSYNC_OP;
	    dip->di_initiated_time = time((time_t *)0);
	}
	status = msync(dip->di_mmap_buffer, dip->di_dbytes_written, MS_INVALIDATE);
	if (dip->di_noprog_flag) {
	    dip->di_optype = NONE_OP;
	    dip->di_initiated_time = (time_t) 0;
	}
	if (status == FAILURE) {
	    ReportErrorInfo(dip, dip->di_dname, os_get_error(), "msync", MSYNC_OP, True);
	}
    }
    return(status);
}

/************************************************************************
 *									*
 * mmap_reopen_file() - Reopen memory mapped input or output file.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		oflags = The device/file open flags.			*
 *									*
 * Return Value:							*
 *		Returns 0 / -1 = SUCESS / FAILURE.			*
 *									*
 ************************************************************************/
int
mmap_reopen_file (struct dinfo *dip, int oflags)
{
    /*
     * For memory mapped files, remove the mappings before closing
     * the file.
     */
    if (dip->di_mmap_flag) {
	if (munmap(dip->di_mmap_buffer, dip->di_data_limit) == FAILURE) {
	    ReportErrorInfo(dip, dip->di_dname, os_get_error(), "munmap", MUNMAP_OP, True);
	    return (FAILURE);
	}
	dip->di_mmap_bufptr = dip->di_mmap_buffer = (u_char *) 0;
    }

    return ( reopen_file(dip, oflags) );
}

/************************************************************************
 *									*
 * mmap_validate_opts() - Validate Memory Mapped Test Options.		*
 *									*
 * Description:								*
 *	This function verifies the options specified for memory mapped	*
 * file testing are valid.						*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Return Value:							*
 *		Returns SUCESS / FAILURE = Valid / Invalid Options.	*
 *									*
 ************************************************************************/
int
mmap_validate_opts (struct dinfo *dip)
{
    int status = SUCCESS;

    /*
     * For memory mapped I/O, ensure the user specified a limit, and
     * that the block size is a multiple of the page size (a MUST!).
     */
    if (dip->di_mmap_flag) {
	if (dip->di_data_limit == INFINITY) {
	    Fprintf(dip, "You must specify a data limit for memory mapped I/O.\n");
	    status = FAILURE;
	} else if (dip->di_block_size % page_size) {
	    Fprintf(dip,
		    "Please specify a block size modulo of the page size (%d).\n", page_size);
	    status = FAILURE;
	} else if (dip->di_aio_flag) {
	    Fprintf(dip, "Cannot enable async I/O with memory mapped I/O.\n");
	    status = FAILURE;
	} else {
	    status = validate_opts(dip);
	}
    }
    return(status);
}

/************************************************************************
 *									*
 * mmap_read_data() - Read and optionally verify memory mapped data.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
mmap_read_data (struct dinfo *dip)
{
	ssize_t count;
	size_t bsize, dsize;
	int status = SUCCESS;
	struct dtfuncs *dtf = dip->di_funcs;
	u_int32 lba;

	/*
	 * For variable length records, initialize to minimum record size.
	 */
	if (dip->di_min_size) {
            if (dip->di_variable_flag) {
                dsize = get_variable (dip);
            } else {
                dsize = dip->di_min_size;
            }
	} else {
	    dsize = dip->di_block_size;
	}

	/*
	 * Now read and optionally verify the input records.
	 */
	while ( (dip->di_error_count < dip->di_error_limit) &&
		(dip->di_fbytes_read < dip->di_data_limit) &&
		(dip->di_records_read < dip->di_record_limit) ) {

	    PAUSE_THREAD(dip);
	    if ( THREAD_TERMINATING(dip) ) break;
	    if (dip->di_terminating) break;

	    if ( dip->di_max_data && (dip->di_maxdata_read >= dip->di_max_data) ) {
		break;
	    }

	    if (dip->di_read_delay) {			/* Optional read delay.	*/
		mySleep(dip, dip->di_read_delay);
	    }

	    /*
	     * If data limit was specified, ensure we don't exceed it.
	     */
	    if ( (dip->di_fbytes_read + dsize) > dip->di_data_limit) {
		bsize = (dip->di_data_limit - dip->di_fbytes_read);
		if (dip->di_debug_flag) {
		    Printf (dip, "Reading partial record of %d bytes...\n", bsize);
		}
	    } else {
		bsize = dsize;
	    }

	    count = bsize;			/* Paged in by system.	*/
	    lba = make_lbdata (dip, dip->di_offset);

	    if ((dip->di_io_mode == TEST_MODE) && dip->di_compare_flag) {
		if (dip->di_iot_pattern) {
		    lba = init_iotdata (dip, dip->di_pattern_buffer, count, lba, dip->di_lbdata_size);
		}
	    }

	    /*
	     * Stop reading when end of file is reached.
	     */
	    if (count == (ssize_t) 0) {		/* Pseudo end of file. */
		if (dip->di_debug_flag) {
		    Printf (dip, "End of memory mapped file detected...\n");
		}
		dip->di_end_of_file = True;
		exit_status = END_OF_FILE;
		break;
	    } else {
		dip->di_dbytes_read += count;
		dip->di_fbytes_read += count;
	        if ((status = check_read (dip, count, bsize)) == FAILURE) {
		    break;
		}
	    }

	    if ((size_t)count == dsize) {
		dip->di_full_reads++;
	    } else {
		dip->di_partial_reads++;
	    }

	    /*
	     * Verify the data (unless disabled).
	     */
	    if (dip->di_compare_flag) {
		status = (*dtf->tf_verify_data)(dip, dip->di_mmap_bufptr, count, dip->di_pattern, &lba, False);
	    } else {
		/*
		 * Must reference the data to get it paged in.
		 */
		reference_data (dip->di_mmap_bufptr, count);
	    }
	    dip->di_offset += count;
	    dip->di_mmap_bufptr += count;

	    /*
	     * For variable length records, adjust the next record size.
	     */
	    if (dip->di_min_size) {
		if (dip->di_variable_flag) {
		    dsize = get_variable (dip);
		} else {
		    dsize += dip->di_incr_count;
		    if (dsize > dip->di_max_size) dsize = dip->di_min_size;
		}
	    }

	    if ( (dip->di_fbytes_read >= dip->di_data_limit) ||
		 (++dip->di_records_read >= dip->di_record_limit) ) {
		break;
	    }

#ifdef notdef
	/*
	 * Can't do this right now... if it's not mapped via mmap(), you'll
	 * get a "Segmentation Fault" and core dump.  Need more logic...
	 */
	    if (dip->di_step_offset) dip->di_mmap_bufptr += dip->di_step_offset;
#endif
	}
	return (status);
}

/************************************************************************
 *									*
 * reference_data() - Reference Data of Memory Mapped File.		*
 *									*
 * Description:								*
 *	This function simply references each data byte to force pages	*
 * to be mapped in by the system (memory mapped file I/O).		*
 *									*
 * Inputs:	buffer = Data buffer to reference.			*
 *		count = Number of bytes to reference.			*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
reference_data (u_char *buffer, size_t count)
{
	size_t i = (size_t) 0;
	u_char *bptr = buffer;
	static volatile u_char data;

	while (i++ < count) {
		data = *bptr++;
	}
}

/************************************************************************
 *									*
 * mman_write_data() - Write data to memory mapped output file.		*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
mmap_write_data (struct dinfo *dip)
{
	ssize_t count;
	size_t bsize, dsize;
	int status = SUCCESS;
	u_int32 lba;

	/*
	 * For variable length records, initialize to minimum record size.
	 */
	if (dip->di_min_size) {
	    dsize = dip->di_min_size;
	} else {
	    dsize = dip->di_block_size;
	}

	/*
	 * Now write the specifed number of records.
	 */
	while ( (dip->di_fbytes_written < dip->di_data_limit) &&
		(dip->di_records_written < dip->di_record_limit) ) {

	    PAUSE_THREAD(dip);
	    if ( THREAD_TERMINATING(dip) ) break;
	    if (dip->di_terminating) break;

	    if ( dip->di_max_data && (dip->di_maxdata_written >= dip->di_max_data) ) {
		break;
	    }

	    if (dip->di_write_delay) {			/* Optional write delay	*/
		mySleep(dip, dip->di_write_delay);
	    }

	    /*
	     * If data limit was specified, ensure we don't exceed it.
	     */
	    if ( (dip->di_fbytes_written + dsize) > dip->di_data_limit) {
		bsize = (dip->di_data_limit - dip->di_fbytes_written);
		if (dip->di_debug_flag) {
		    Printf (dip, "Writing partial record of %d bytes...\n",
								bsize);
		}
	    } else {
		bsize = dsize;
	    }

	    count = bsize;
	    lba = make_lbdata (dip, dip->di_offset);

	    if ((dip->di_io_mode == TEST_MODE) && dip->di_compare_flag) {
	        if (dip->di_iot_pattern) {
		    lba = init_iotdata (dip, dip->di_mmap_bufptr, count, lba, dip->di_lbdata_size);
		} else {
		    fill_buffer (dip, dip->di_mmap_bufptr, count, dip->di_pattern);
		}
	    }

	    /*
	     * Initialize the logical block data (if enabled).
	     */
	    if (dip->di_lbdata_flag && dip->di_lbdata_size && !dip->di_iot_pattern) {
		lba = init_lbdata (dip, dip->di_mmap_bufptr, count, lba, dip->di_lbdata_size);
	    }

#if defined(TIMESTAMP)
            /*
             * If timestamps are enabled, initialize buffer accordingly.
             */
            if (dip->di_timestamp_flag) {
                init_timestamp(dip, dip->di_mmap_bufptr, count, dip->di_lbdata_size);
            }
#endif /* defined(TIMESTAMP) */

	    dip->di_offset += count;
	    dip->di_mmap_bufptr += count;
	    dip->di_dbytes_written += count;
	    dip->di_fbytes_written += count;

	    /*
	     * Stop writing when end of file is reached.
	     */
	    if (count == (ssize_t) 0) {		/* Pseudo end of file. */
		if (dip->di_debug_flag) {
		    Printf (dip, "End of memory mapped file reached...\n");
		}
		dip->di_end_of_file = True;
		exit_status = END_OF_FILE;
		break;
	    }

	    if ((status = check_write (dip, count, bsize, dip->di_offset)) == FAILURE) {
		break;
	    } else {
		if ((size_t)count == dsize) {
		    dip->di_full_writes++;
		} else {
		    dip->di_partial_writes++;
		}
	    }

	    /*
	     * For variable length records, adjust the next record size.
	     */
	    if (dip->di_min_size) {
		dsize += dip->di_incr_count;
		if (dsize > dip->di_max_size) dsize = dip->di_min_size;
	    }

	    ++dip->di_records_written;

	    if ( dip->di_fsync_frequency && ((dip->di_records_written % dip->di_fsync_frequency) == 0) ) {
		status = (*dip->di_funcs->tf_flush_data)(dip);
		if ( (status == FAILURE) && (dip->di_error_count >= dip->di_error_limit) ) {
		    return (status);
		}
	    }
#ifdef notdef
	    /*
	     * Can't do this right now... if it's not mapped via mmap(), you'll
	     * get a "Segmentation Fault" and core dump.  Need more logic...
	     */
		if (dip->di_step_offset) dip->di_mmap_bufptr += dip->di_step_offset;
#endif
	}
	return (status);
}

#endif /* defined(MMAP) */
