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
 * Module:	dtaio.c
 * Author:	Robin T. Miller
 * Date:	August 26, 1993 
 *
 * Description:
 *	Functions to handle POSIX Asynchronous I/O requests for 'dt' program.
 *
 * Modification History:
 * 
 * September 20th, 2023 by Robin T. Miller
 *      For all random access devices, limit the data read to what was
 * written. Previously this was enabled only for file systems, but it's
 * possible for direct acces disks to create a premature end of data
 * condition. See dtread.c for further details.
 * 
 * June 5th, 2020 by Robin T. Miller
 *      Fix a case with slices and step option where we went past end of slice,
 * which caused a false data corruption (sigh). Depending on options specifed,
 * we may not set EOF when completing previous request, so the next partial
 * write was set based on the dta limit, *not* the end of the slice offset!
 * 
 * February 11th, 2016 by Robin T. Miller
 * 	When prefilling read buffers, rather than using the inverted pattern,
 * switch to 1) user defined fill pattern, or 2) the thread number. This helps
 * exonerate dt, when folks beleive dt threads are overwriting other buffers.
 *
 * June 9th, 2015 by Robin T. Miller
 * 	Added support for block tags (btags).
 *
 * September 24th, 2014 by Robin T. Miller
 * 	Fix issue with the updated offset being set in di_offset, *before*
 * invoking read_check()/write_check(), which results in the wrong offset
 * being displayed for short read/write requests. Now, di_offset gets the
 * aiocb offset, which matches the setting for all other I/O behaviors.
 * 
 * June 5th, 2014 by Robin T. Miller
 * 	Fix bug where we would loop indefinitely, effectively hanging, when
 * the max data was reached within the inner AIO I/O loops.
 *
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#if defined(AIO)

#include "dt.h"
#if !defined(WIN32)
#  include <aio.h>
#endif /* !defined(WIN32) */
#include <limits.h>
#include <sys/stat.h>

#if !defined(AIO_PRIO_DFL)
#  define AIO_PRIO_DFL	0		/* Default scheduling priority. */
#endif /* !defined(AIO_PRIO_DFL) */

#if defined(_AIO_AIX_SOURCE)
/*
 * Allows testing with legacy AIX AIO (5.1 and older).
 */
#  define POSIX_4D11
  /*
   * Note: The usage of this field could be dangerous, but we need an
   * aiocb field to ease our port to AIX! (may need to revisit this).
   */
#  define aio_fildes aio_fp
#endif /* defined(_AIO_AIX_SOURCE) */

/*
 * Forward References:
 */
static int dtaio_wait(struct dinfo *dip, struct aiocb *acbp);
static int dtaio_waitall(struct dinfo *dip, hbool_t canceling);
static int dtaio_wait_reads(struct dinfo *dip);
static int dtaio_wait_writes(struct dinfo *dip);
static int dtaio_process_read(struct dinfo *dip, struct aiocb *acbp);
static int dtaio_process_write(struct dinfo *dip, struct aiocb *acbp);

#if defined(WIN32)
#  define AIO_NotQed	INVALID_HANDLE_VALUE
#else /* !defined(WIN32) */
#  define AIO_NotQed	-1		/* AIO request not queued flag.	*/
#endif /* defined(WIN32) */

/*
 * Declare the POSIX Asynchronous I/O test functions.
 */
struct dtfuncs aio_funcs = {
    /*	tf_open,		tf_close,		tf_initialize,	  */
	open_file,		dtaio_close_file,	dtaio_initialize,
    /*  tf_start_test,		tf_end_test,				  */
	init_file,		nofunc,
    /*	tf_read_file,		tf_read_data,		tf_cancel_reads,  */
	read_file,		dtaio_read_data,	dtaio_cancel_reads,
    /*	tf_write_file,		tf_write_data,		tf_cancel_writes, */
	write_file,		dtaio_write_data,	nofunc,
    /*	tf_flush_data,		tf_verify_data,		tf_reopen_file,   */
	flush_file,		verify_data,		reopen_file,
    /*	tf_startup,		tf_cleanup,		tf_validate_opts  */
	nofunc,			nofunc,			validate_opts
};

/************************************************************************
 *									*
 * dtaio_close_file() - Close an open file descriptor.			*
 *									*
 * Description:								*
 *	This function does the AIO file descriptor close processing.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Return Value:							*
 *		Returns 0 / -1 = SUCCESS / FAILURE.			*
 *									*
 ************************************************************************/
int
dtaio_close_file (struct dinfo *dip)
{
    int status = SUCCESS;

    if (dip->di_closing || (dip->di_fd == NoFd)) {
	return (status);		/* Closing or not open. */
    }
    /*
     * Avoid cancel'ing I/O more than once using the closing flag.
     * We can get called again by alarm expiring or signal handler.
     */
    dip->di_closing = True;
    if (dip->di_acbs) {
	(void) dtaio_cancel (dip);
	status = dtaio_waitall (dip, False);
    }
    dip->di_closing = False;
    return (close_file (dip));
}

/*
 * Allocate and initialize AIO data structures.
 */
int
dtaio_initialize (struct dinfo *dip)
{
    struct aiocb *acbp;
    size_t size = (sizeof(struct aiocb) * dip->di_aio_bufs);
    int index;
    int status = SUCCESS;

    if ( (dip->di_dtype->dt_dtype == DT_TAPE) && dip->di_raw_flag && (dip->di_aio_bufs > 1) ) {
	Printf(dip, "Sorry, tapes are limited to 1 AIO with raw option!\n");
	dip->di_aio_bufs = 1;
	size = (sizeof(struct aiocb) * dip->di_aio_bufs);
    }

    dip->di_aio_index = 0;
    dip->di_aio_offset = (Offset_t) 0;
    if (dip->di_acbs == NULL) {
	size_t psize = (dip->di_aio_bufs * sizeof(u_char *));
	dip->di_acbs = (struct aiocb *)Malloc(dip, size);
	dip->di_aiobufs = (void **)Malloc(dip, psize);
    }
    for (index = 0, acbp = dip->di_acbs; index < dip->di_aio_bufs; index++, acbp++) {
	if (acbp->aio_buf == NULL) {
	    dip->di_aiobufs[index] = malloc_palign(dip, dip->di_data_alloc_size, dip->di_align_offset);
	    acbp->aio_buf = dip->di_aiobufs[index];
	}
	acbp->aio_fildes = AIO_NotQed;
	acbp->aio_offset = (Offset_t) 0;
	acbp->aio_nbytes = dip->di_block_size;
#if !defined(WIN32)
	acbp->aio_reqprio = AIO_PRIO_DFL;	/* Set default priority. */
# if defined(SCO) || defined(HP_UX)
	/*
	 * Note: The AIO manual recommends setting AIO_RAW, but when
	 *       this is set, EINVAL is returned by aio_read/aio_write!
	 */
#  if defined(SCO)
	acbp->aio_flags = 0;			/* Must be zero to work! */
#  endif /* defined(SCO) */
	acbp->aio_sigevent.sigev_notify = SIGEV_NONE;
#  if 0
	acbp->aio_flags = 0; /*AIO_RAW;*/	/* Required on SVR4.2(?) */
	/*
	 * This signaling method did not exist with the first release
	 * of POSIX AIO.  Perhaps I'll add this completion method in
	 * a future release.  Note: Tru64 Unix now supports this too!
	 */
	acbp->aio_sigevent.sigev_signo = /* use with SIGEV_SIGNAL */;
	acbp->aio_sigevent.sigev_notify = SIGEV_CALLBACK;
	acbp->aio_sigevent.sigev_func = my_aio_completion_function;
	acbp->aio_sigevent.sigev_value = acbp;
#  endif /* 0 */
# endif /* defined(SCO) || defined(HP_UX) */
#endif /* !defined(WIN32) */
	/*
	 * Use first buffer allocated for initial skip reads, etc.
	 */
	if (index == 0) {
	    dip->di_base_buffer = dip->di_data_buffer = (u_char *)acbp->aio_buf;
	}
    }
    return (status);
}

void
dtaio_free_buffers(dinfo_t *dip)
{
    if (dip->di_aio_bufs && dip->di_acbs) {
	int index;
	struct aiocb *acbp;
	for (index = 0, acbp = dip->di_acbs; index < dip->di_aio_bufs; index++, acbp++) {
	    if (acbp->aio_buf) {
		free_palign(dip, dip->di_aiobufs[index]);
		dip->di_aiobufs[index] = NULL;
		acbp->aio_buf = NULL;
	    }
	}
	Free(dip, dip->di_aiobufs);
	dip->di_aiobufs = NULL;
	Free(dip, dip->di_acbs);
	dip->di_acbs = NULL;
    }
    return;
}

/*
 * Cancel outstanding I/O on the specified file descriptor.
 */
int
dtaio_cancel (struct dinfo *dip)
{
    int status;

    if (dip->di_debug_flag) {
	Printf (dip, "Canceling I/O for fd = %d...\n", dip->di_fd);
    }

#if defined(__linux__)
    /* For goofy Linux AIO implemented via POSIX threads, yuck! */
    (void)os_set_thread_cancel_type(dip, PTHREAD_CANCEL_ASYNCHRONOUS);
#endif
    /*
     * Cancel any outstanding AIO's.
     */
#if defined(WIN32)
    /*
     * If the function fails, the return value is zero (0).
     */
    status = SUCCESS;
    if ( !CancelIo(dip->di_fd) ) {
	int error = os_get_error();
	/* Note: Should NOT need this anymore... had a bug! */
	if (error != ERROR_INVALID_HANDLE) {
	    ReportErrorInfo(dip, dip->di_dname, error, "CancelIo", CANCEL_OP, True);
	    status = FAILURE;
	}
    }
#else /* !defined(WIN32) */
    if ((status = aio_cancel(dip->di_fd, (struct aiocb *) 0)) == FAILURE) {
	int error = os_get_error();
	/*
	 * aio_cancel() returns EBADF if the file descriptor is
         * not valid, which could mean we didn't open device yet.
	 */
# if defined(SOLARIS)
        /* Why is EOVERFLOW being returned? */
	if ( (error != EBADF) && (error != EOVERFLOW) ) {
# else /* !defined(SOLARIS) */
	if (error != EBADF) {
# endif /* defined(SOLARIS) */
	    ReportErrorInfo(dip, dip->di_dname, error, "aio_cancel", CANCEL_OP, True);
	}
	return (status);
    }
    if (dip->di_debug_flag) {

	switch (status) {

	    case AIO_ALLDONE:
		Printf(dip, "All requests completed before cancel...\n");
		break;

	    case AIO_CANCELED:
		Printf(dip, "Outstanding requests were canceled...\n");
		break;

	    case AIO_NOTCANCELED:
		Fprintf(dip, "Outstanding (active?) request NOT canceled...\n");
		break;

	    default:
		Fprintf(dip, "Unexpected status of %d from aio_cancel()...\n", status);
		break;
	}
    }
#endif /* defined(WIN32) */
    return (status);
}

int
dtaio_cancel_reads (struct dinfo *dip)
{
    int status;
#if defined(TAPE) && !defined(WIN32)
    struct dtype *dtp = dip->di_dtype;
#endif

    dip->di_aio_data_adjust = dip->di_aio_file_adjust = dip->di_aio_record_adjust = 0L;
    (void) dtaio_cancel (dip);
    status = dtaio_waitall (dip, True);
#if defined(TAPE) && !defined(WIN32) /* no tape support for Windows */ 
    if (dip->di_aio_file_adjust && (dtp->dt_dtype == DT_TAPE) ) {
	daddr_t count = (daddr_t)dip->di_aio_file_adjust;
	/*
	 * Tapes are tricky... we must backup prior to the
	 * last file(s) we processed, then forward space over
	 * its' file mark to be properly positioned (yuck!!!).
	 */
	if (dip->di_end_of_file) count++;
	status = DoBackwardSpaceFile (dip, count);
	if (status == SUCCESS) {
	    status = DoForwardSpaceFile (dip, (daddr_t) 1);
	}
    } else if (dip->di_aio_record_adjust && (dtp->dt_dtype == DT_TAPE) ) {
	/*
	 * If we've read partially into the next file, backup.
	 */
	status = DoBackwardSpaceFile (dip, (daddr_t) 1);
	if (status == SUCCESS) {
	    status = DoForwardSpaceFile (dip, (daddr_t) 1);
	}
    }
#endif /* defined(TAPE) && !defined(WIN32) */
    return (status);
}

static int
dtaio_restart(struct dinfo *dip, struct aiocb *first_acbp)
{
    struct aiocb *acbp = first_acbp;
    int index, error, status = SUCCESS;

    /*
     * Find starting index of this AIO request.
     */
    for (index = 0; index < dip->di_aio_bufs; index++) {
	if (first_acbp == &dip->di_acbs[index]) break;
    }
    if (index == dip->di_aio_bufs) abort(); /* Should NEVER happen! */

    /*
     * Now, wait for and restart all previously active I/O's.
     */
    do {
	/*
	 * Assumes the first request was already waited for!
	 */
	if (dip->di_Debug_flag) {
	    Printf(dip, "Restarting request for acbp at %#lx...\n", acbp);
	}
	if (dip->di_mode == READ_MODE) {
#if defined(WIN32)
	   error = SUCCESS;
	   if ( !ReadFile(acbp->aio_fildes, acbp->aio_buf, (DWORD)acbp->aio_nbytes,
			  NULL, &acbp->overlap) ) error = FAILURE;
#else /* !defined(WIN32) */
# if defined(_AIO_AIX_SOURCE)
	    error = aio_read(acbp->aio_fildes, acbp);
# else /* !defined(_AIO_AIX_SOURCE) */
	    error = aio_read(acbp);
# endif /* defined(_AIO_AIX_SOURCE) */
#endif /* defined(WIN32) */
	    if (error == FAILURE) {
		acbp->aio_fildes = AIO_NotQed;
		ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_AIO_READ, READ_OP, True);
		return (error);
	    }
	} else {
#if defined(WIN32)
	   error = SUCCESS;
	   if ( !WriteFile(acbp->aio_fildes, acbp->aio_buf, (DWORD)acbp->aio_nbytes,
			   NULL, &acbp->overlap) ) error = FAILURE;
#else /* !defined(WIN32) */
# if defined(_AIO_AIX_SOURCE)
	    error = aio_write(acbp->aio_fildes, acbp);
# else /* !defined(_AIO_AIX_SOURCE) */
	    error = aio_write(acbp);
# endif /* defined(_AIO_AIX_SOURCE) */
#endif /* defined(WIN32) */
	    if (error == FAILURE) {
		acbp->aio_fildes = AIO_NotQed;
		ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_AIO_WRITE, WRITE_OP, True);
		return (error);
	    }
	}
	if (++index == dip->di_aio_bufs) index = 0;
	if (index == dip->di_aio_index) break;

	acbp = &dip->di_acbs[index];
	if (acbp->aio_fildes == AIO_NotQed) abort();

	error = dtaio_wait(dip, acbp);
#if !defined(WIN32)
	(void)aio_return(acbp);	/* Why is this here? */
#endif /* !defined(WIN32) */

    } while (True);

    return (status);
}

/*
 * dtaio_wait() - Wait for an AIO Request to Complete.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	acbp = The AIO control block to wait for.
 *
 * Return Value:
 *	Returns Success/Failure = acbp done/error waiting.
 */
static int
dtaio_wait (struct dinfo *dip, struct aiocb *acbp)
{
    int status;

    if (dip->di_Debug_flag) {
	Printf (dip, "Waiting for acbp at %#lx to complete...\n", acbp);
    }
    /*
     * Since we always come here to wait for an I/O request, we'll time
     * here rather than when issuing each aio_{read|write} request.
     */
    ENABLE_NOPROG(dip, AIOWAIT_OP);
    /*
     * TODO: This needs cleaned up!
     * Make Windows functions to mimic AIO API's.
     */
#if defined(WIN32)
    status = SUCCESS;
    acbp->last_error = ERROR_SUCCESS;

    while (!GetOverlappedResult(acbp->aio_fildes, &acbp->overlap, &acbp->bytes_rw, False))  {
	DWORD error = GetLastError();
	if (error == ERROR_IO_INCOMPLETE) {
	    /* Note: Polling is ineffecient (IMO), must be a better way? */
	    /* FYI: This value is in ms, so if too high, kills performance! */
	    /* TODO: Switch to I/O competion ports, remove this nonsense!!! */
	    Sleep(1); /* Not done yet, wait a while then retry. */
	} else {
	    /*
	     * Later we check bytes_rw to know the status of operation
	     * that's why we are inintializing with FAILURE in case of error
	     * in case of success it will have total bytes read/write.
	     */
	    status = FAILURE;
	    acbp->bytes_rw = status;
	    acbp->last_error = error;
	    //ReportErrorInfo(dip, dip->di_dname, os_get_error(), "GetOverlappedResult", OTHER_OP, True);
	    break;
	}
    }
#else /* !defined(WIN32) */
    /*
     * Loop waiting for an I/O request to complete.
     */
    while ((status = aio_error(acbp)) == EINPROGRESS) {
# if defined(POSIX_4D11)
#  if defined(_AIO_AIX_SOURCE)
	if ((status = aio_suspend(1, (struct aiocb **)&acbp)) == FAILURE) {
#  else /* !defined(_AIO_AIX_SOURCE) */
	if ((status = aio_suspend(1, (const struct aiocb **)&acbp)) == FAILURE) {
#  endif /* defined(_AIO_AIX_SOURCE) */
# else /* Beyond draft 11... */
	if ((status = aio_suspend((const struct aiocb **)&acbp,1,NULL)) == FAILURE) {
# endif /* defined(POSIX_4D11) */
	    if (errno != EINTR) {
		ReportErrorInfo(dip, dip->di_dname, os_get_error(), "aio_suspend", SUSPEND_OP, True);
		goto done;
	    }
	}
    }
    if ( (status == FAILURE) && !terminating_flag) {
	ReportErrorInfo(dip, dip->di_dname, os_get_error(), "aio_error", OTHER_OP, True);
    }
done:
#endif /* defined(WIN32) */
    DISABLE_NOPROG(dip);
    return (status);
}

static int
dtaio_waitall(struct dinfo *dip, hbool_t canceling)
{
    struct aiocb *acbp;
    register size_t bsize;
    register ssize_t count;
    ssize_t adjust;
    int index, error, status = SUCCESS;

    /*
     * Loop waiting for all I/O requests to complete.
     */
    for (index = 0; index < dip->di_aio_bufs; index++) {
	acbp = &dip->di_acbs[dip->di_aio_index];
	if (++dip->di_aio_index == dip->di_aio_bufs) dip->di_aio_index = 0;
	if (acbp->aio_fildes == AIO_NotQed) continue;
	if ( (error = dtaio_wait(dip, acbp))) {
	    status = error;
	    if (status == FAILURE) {
		acbp->aio_fildes = AIO_NotQed;
		continue;	/* aio_error() failed! */
	    }
	}
#if defined(WIN32)
	/* bytes_rw is bytes read/write in previous operation or FAILURE */
	count = acbp->bytes_rw;
	error = acbp->last_error;
#else /* !defined(WIN32) */
	count = aio_return(acbp);
#endif /* defined(WIN32) */
	acbp->aio_fildes = AIO_NotQed;
	errno = error;

	if ( (count == FAILURE) && !dip->di_closing && !terminating_flag) {
	    //int error = os_get_error();
	    hbool_t eio_flag = os_isIoError(error);
	    hbool_t isEof_flag = os_isEof(count, error);

	    if ( (isEof_flag == False) && (os_isCancelled(error) == False) ) {
		dip->di_current_acb = acbp;
		ReportErrorInfo(dip, dip->di_dname, error, "dtaio_waitall", OTHER_OP, True);
		ReportDeviceInfo(dip, acbp->aio_nbytes, 0, eio_flag, NotMismatchedData);
		if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		     (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		    if (dip->di_mode == READ_MODE) {
			(void)ExecuteTrigger(dip, "read");
		    } else {
			(void)ExecuteTrigger(dip, "write");
		    }
		}
		status = FAILURE;
		/* adjust counts below */
	    }
	} else if (error) {
	    count = FAILURE;
	}

	bsize = acbp->aio_nbytes;

	/*
	 * Adjust for short records or no data transferred.
	 */
	if (count == FAILURE) {
	    dip->di_aio_data_bytes -= bsize;
	    dip->di_aio_file_bytes -= bsize;
	} else if ( (adjust = (ssize_t)(bsize - count)) ) {
	    if (dip->di_debug_flag) {
		Printf(dip, "Adjusting byte counts by %d bytes...\n", adjust);
	    }
	    dip->di_aio_data_bytes -= adjust;
	    dip->di_aio_file_bytes -= adjust;
	}

	/*
	 * Count files or records to adjust after I/O's complete.
	 */
	if ( is_Eof(dip, count, bsize, (int *) 0) ) {
	    if (!dip->di_end_of_media) dip->di_aio_file_adjust++;
	} else if (count > (ssize_t) 0) {
	    dip->di_aio_record_adjust++;
	    /*
	     * Adjust counts for total statistics.
	     */
	    if (!canceling) {
		if (dip->di_mode == READ_MODE) {
		    dip->di_dbytes_read += count;
		    dip->di_fbytes_read += count;
		} else {
		    dip->di_dbytes_written += count;
		    dip->di_fbytes_written += count;
		}
		dip->di_aio_data_adjust += count;
		if ((size_t)count == bsize) {
                    if (dip->di_mode == READ_MODE) {
		        dip->di_full_reads++;
                    } else {
                        dip->di_full_writes++;
                    }
		} else {
                    if (dip->di_mode == READ_MODE) {
		        dip->di_partial_reads++;
                    } else {
                        dip->di_partial_writes++;
                    }
		}
	    }
	}
    }
    return (status);
}

/*
 * Function to wait for and process read requests.
 */
static int
dtaio_wait_reads(struct dinfo *dip)
{
    struct aiocb *acbp;
    int index, error, status = SUCCESS;

    /*
     * Loop waiting for all I/O requests to complete.
     */
    for (index = 0; index < dip->di_aio_bufs; index++) {
	acbp = &dip->di_acbs[dip->di_aio_index];
	if (++dip->di_aio_index == dip->di_aio_bufs) dip->di_aio_index = 0;
	if (acbp->aio_fildes == AIO_NotQed) continue;
	
	if ( (error = dtaio_process_read(dip, acbp)) == FAILURE) {
	    status = error;
	}
	if ( dip->di_end_of_file ||
	     (dip->di_records_read >= dip->di_record_limit) || (dip->di_fbytes_read >= dip->di_data_limit) ) {
	    break;
	}
    }
    return (status);
}

/*
 * Function to wait for and process write requests.
 */
static int
dtaio_wait_writes(struct dinfo *dip)
{
    struct aiocb *acbp;
    int index, error, status = SUCCESS;

    /*
     * Loop waiting for all I/O requests to complete.
     */
    for (index = 0; index < dip->di_aio_bufs; index++) {
	acbp = &dip->di_acbs[dip->di_aio_index];
	if (++dip->di_aio_index == dip->di_aio_bufs) dip->di_aio_index = 0;
	if (acbp->aio_fildes == AIO_NotQed) continue;
	
	if ( (error = dtaio_process_write(dip, acbp)) == FAILURE) {
	    status = error;
	    if (dip->di_error_count >= dip->di_error_limit) break;
	}
    }
    return (status);
}

/************************************************************************
 *									*
 * dtaio_read_data() - Read and optionally verify data read.		*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
dtaio_read_data(struct dinfo *dip)
{
    register struct aiocb *acbp;
    int error, status = SUCCESS;
    register size_t bsize, dsize;
    large_t data_limit;
    hbool_t check_rwbytes = False;
    hbool_t check_write_limit = False;

    if (dip->di_random_access) {
	if ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
	    dip->di_aio_offset = set_position(dip, (Offset_t)dip->di_rdata_limit, False);
	}
	dip->di_aio_lba = get_lba(dip);
	dip->di_aio_offset = get_position(dip);
    } else {
	dip->di_aio_offset = dip->di_offset;
	dip->di_aio_lba = make_lbdata(dip, dip->di_aio_offset);
    }
    dip->di_aio_data_bytes = dip->di_aio_file_bytes = dip->di_aio_record_count = 0;

    if ( dip->di_last_fbytes_written && dip->di_random_access ) {
	if ( dip->di_files_read == (dip->di_last_files_written - 1) ) {
	    check_write_limit = True;
	    if (dip->di_fDebugFlag) {
		Printf(dip, "DEBUG: Limiting data read on file #%d to " FUF " bytes from last written.\n",
		       (dip->di_files_read + 1), dip->di_last_fbytes_written);
	    }
	}
    }

    dsize = get_data_size(dip, READ_OP);
    data_limit = get_data_limit(dip);

    /* Prime the common btag data, except for IOT pattern. */
    if ( (dip->di_btag_flag == True) && (dip->di_iot_pattern == False) ) {
	update_btag(dip, dip->di_btag, dip->di_offset,
		    (uint32_t)0, (size_t)0, (dip->di_records_read + 1));
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

	if ( (dip->di_maxdata_reached == True) ||
	     dip->di_max_data && (dip->di_maxdata_read >= dip->di_max_data) ) {
	    dip->di_maxdata_reached = True;
	    break;
	}

	if (dip->di_volumes_flag && (dip->di_multi_volume >= dip->di_volume_limit) &&
		  (dip->di_volume_records >= dip->di_volume_records)) {
	    dip->di_volume_records = dip->di_volume_records;
	    break;
	}

	/*
	 * Two loops are used with AIO.  The inner loop queues requests up
	 * to the requested amount, and the outer loop checks the actual
	 * data processed.  This is done mainly for tapes to handle short
	 * reads & to efficiently handle multiple tape files.
	 */
	while ( (dip->di_error_count < dip->di_error_limit) &&
		(dip->di_aio_record_count < dip->di_record_limit) &&
		(dip->di_aio_file_bytes < data_limit) ) {

	    PAUSE_THREAD(dip);
	    if ( THREAD_TERMINATING(dip) ) break;
	    if (dip->di_terminating) break;

	    if ( dip->di_max_data &&
		 ((dip->di_aio_file_bytes + dip->di_maxdata_read) >= dip->di_max_data) ) {
		dip->di_maxdata_reached = True;
		break;
	    }

	    if (dip->di_volumes_flag && (dip->di_multi_volume >= dip->di_volume_limit) &&
		      (dip->di_volume_records >= dip->di_volume_records)) {
		break;
	    }

	    if (dip->di_read_delay) {			/* Optional read delay.	*/
		mySleep(dip, dip->di_read_delay);
	    }

	    /*
	     * If data limit was specified, ensure we don't exceed it.
	     */
	    if ( (dip->di_aio_file_bytes + dsize) > data_limit) {
		bsize = (size_t)(data_limit - dip->di_aio_file_bytes);
	    } else {
		bsize = dsize;
	    }

	    acbp = &dip->di_acbs[dip->di_aio_index];
	    /*
	     * If requested, rotate the data buffer through ROTATE_SIZE bytes
	     * to force various unaligned buffer accesses.
	     */
	    if (dip->di_rotate_flag) {
		dip->di_data_buffer = dip->di_aiobufs[dip->di_aio_index];
		dip->di_data_buffer += (dip->di_rotate_offset++ % ROTATE_SIZE);
		acbp->aio_buf = dip->di_data_buffer;
	    } else {
		dip->di_data_buffer = (u_char *)acbp->aio_buf;
	    }

	    if ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
		/*debug*/ if (!dip->di_aio_offset) abort(); /*debug*/
		bsize = (size_t)MIN((dip->di_aio_offset - dip->di_file_position), (Offset_t)bsize);
		dip->di_aio_offset = (Offset_t)(dip->di_aio_offset - bsize);
	    }

            if (dip->di_debug_flag && (bsize != dsize) && !dip->di_variable_flag) {
                Printf (dip, "Record #%lu, Reading a partial record of %lu bytes...\n",
                                    (dip->di_aio_record_count + 1), bsize);
            }

	    if (dip->di_io_type == RANDOM_IO) {
                /*
		 * BEWARE: The size *must* match the write size, or you'll get
		 * a different offset, since the size is used in calculations.
		 */
		acbp->aio_offset = do_random(dip, False, bsize);
	    } else {
		acbp->aio_offset = dip->di_aio_offset;
	    }

	    /*
	     * If we wrote data, ensure we don't read more than we wrote.
	     */
	    if (check_write_limit) {
		if ( (dip->di_aio_file_bytes + bsize) > dip->di_last_fbytes_written) {
		    dsize = bsize;	/* Save the original intended size. */
		    bsize = (size_t)(dip->di_last_fbytes_written - dip->di_aio_file_bytes);
		    check_rwbytes = True;
		    if (bsize == (size_t) 0) {
			break;
		    }
		    status = check_last_write_info(dip, acbp->aio_offset, bsize, dsize);
		    if (status == FAILURE) break;
		}
	    }

	    acbp->aio_fildes = dip->di_fd;
	    acbp->aio_nbytes = bsize;

	    /*
	     * If we'll be doing a data compare after the read, then
	     * fill the data buffer with the inverted pattern to ensure
	     * the buffer actually gets written into (driver debug mostly).
	     */
	    if ( (dip->di_compare_flag == True) && (dip->di_io_mode == TEST_MODE) ) {
		if (dip->di_prefill_buffer == True) {
		    uint32_t pattern = (dip->di_prefill_pattern) ? dip->di_prefill_pattern : (uint32_t)dip->di_thread_number;
		    if (dip->di_poison_buffer) {
			poison_buffer(dip, dip->di_data_buffer, bsize, pattern);
		    } else {
			init_buffer(dip, dip->di_data_buffer, bsize, pattern);
		    }
		}
		init_padbytes(dip->di_data_buffer, bsize, ~dip->di_pattern);
	    }

	    if (dip->di_Debug_flag) {
		report_io(dip, READ_MODE, (void *)acbp->aio_buf, acbp->aio_nbytes, acbp->aio_offset);
	    }

#if defined(WIN32)
	    /* TODO: Needless to say, this needs cleaned up! */
	    acbp->overlap.hEvent = 0;
	    acbp->overlap.Offset = ((PLARGE_INTEGER)(&acbp->aio_offset))->LowPart;
	    acbp->overlap.OffsetHigh = ((PLARGE_INTEGER)(&acbp->aio_offset))->HighPart;
	    error = ReadFile(acbp->aio_fildes, acbp->aio_buf, (DWORD)acbp->aio_nbytes,
			     NULL, &acbp->overlap);
	    if ( !error && (GetLastError() != ERROR_IO_PENDING) ) {
	    	if (GetLastError() == ERROR_HANDLE_EOF) {
		    /* Messy, but we must handle this here! */
		    acbp->aio_fildes = AIO_NotQed;
		    status = dtaio_wait_reads(dip);
		    dip->di_end_of_file = True;
		    exit_status = END_OF_FILE;
		    break;
		}
		error = FAILURE;
		acbp->aio_fildes = AIO_NotQed;
		ReportErrorInfo(dip, dip->di_dname, os_get_error(), "ReadFile", READ_OP, True);
		return (error);
	    }
#else /* !defined(WIN32) */
# if defined(_AIO_AIX_SOURCE)
	    if ( (error = aio_read(acbp->aio_fildes, acbp)) == FAILURE) {
# else /* !defined(_AIO_AIX_SOURCE) */
	    if ( (error = aio_read(acbp)) == FAILURE) {
# endif /* defined(_AIO_AIX_SOURCE) */
		acbp->aio_fildes = AIO_NotQed;
		ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_AIO_READ, READ_OP, True);
		return (error);
	    }
#endif /* !defined(WIN32) */

	    /*
	     * Must adjust record/data counts here to avoid reading
	     * too much data, even though the reads are incomplete.
	     */
	    dip->di_aio_data_bytes += bsize;
	    dip->di_aio_file_bytes += bsize;
	    dip->di_aio_record_count++;

	    if (dip->di_io_dir == FORWARD) {
		dip->di_aio_offset += bsize;
	    }

	    if (dip->di_step_offset) {
		if (dip->di_io_dir == FORWARD) {
		    dip->di_aio_offset += dip->di_step_offset;
		} else if ((dip->di_aio_offset -= dip->di_step_offset) <= (Offset_t)dip->di_file_position) {
		    dip->di_aio_offset = (Offset_t)dip->di_file_position;
		}
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

	    /*
	     * Always ensure the next control block has completed.
	     */
	    if (++dip->di_aio_index == dip->di_aio_bufs) dip->di_aio_index = 0;

	    /*
	     * Special handling of step option:
	     */
	    if ( (dip->di_io_dir == FORWARD)	       &&
		 dip->di_step_offset && dip->di_slices &&
		 ((dip->di_aio_offset + (Offset_t)dsize) >= dip->di_end_position) ) {
		dsize = (size_t)(dip->di_end_position - dip->di_aio_offset);
		break;
	    } else if ( (dip->di_io_dir == REVERSE) && (dip->di_aio_offset == (Offset_t)dip->di_file_position) ) {
		break;
	    }
	    acbp = &dip->di_acbs[dip->di_aio_index];
	    if (acbp->aio_fildes == AIO_NotQed) continue; /* Never Q'ed. */

	    if ( (status = dtaio_process_read(dip, acbp)) == FAILURE) {
		return (status);
	    }
	    //if ( dip->di_end_of_file ) return (status);
	    if ( dip->di_end_of_file ) break;
	}
	/*
	 * We get to this point after we've Q'ed enough requests to
	 * fulfill the requested record and/or data limit.  We now
	 * wait for these Q'ed requests to complete, adjusting the
	 * global transfer statistics appropriately which reflects
	 * the actual data processed.
	 */
	status = dtaio_wait_reads(dip);
	/*
	 * For regular files, if we've read as much as we've written,
	 * then set a fake EOF to stop this read pass.
	 */
	if ( check_rwbytes &&
	     (dip->di_fbytes_read == dip->di_last_fbytes_written) ) {
	    set_Eof(dip);
	}
	if ( dip->di_end_of_file ) break; /* Stop reading at end of file. */

    }
    return (status);
}

/************************************************************************
 *									*
 * dtaio_process_read() - Process AIO reads & optionally verify data.	*
 *									*
 * Description:								*
 *	This function does waits for the requested AIO read request,	*
 * checks the completion status, and optionally verifies the data read.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *		acbp = The AIO control block.				*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE/WARNING = Ok/Error/Warning.	*
 *									*
 ************************************************************************/
static int
dtaio_process_read(struct dinfo *dip, struct aiocb *acbp)
{
    dinfo_t *odip = dip->di_output_dinfo; /* For copy/verify modes. */
    struct dtfuncs *dtf = dip->di_funcs;
    register size_t bsize, dsize;
    register ssize_t count;
    ssize_t adjust;
    int error, status = SUCCESS;

    dip->di_retry_count = 0;
retry:
    dip->di_current_acb = acbp;
    /* Wait for this async read to complete. */
    error = dtaio_wait(dip, acbp);
#if defined(WIN32)
    /* total bytes read by ReadFile call or FAILURE in case or error */
    count = acbp->bytes_rw;
    error = acbp->last_error;
#else /* !defined(WIN32) */
    count = aio_return (acbp);
#endif /* defined(WIN32) */

    errno = error;
    bsize = acbp->aio_nbytes;

    if (dip->di_history_size) {
	save_history_data(dip,
			  (dip->di_files_read + 1), (dip->di_records_read + 1),
			  READ_MODE, acbp->aio_offset, (u_char *)acbp->aio_buf, bsize, count);
    }

    if (dip->di_volumes_flag && (dip->di_multi_volume >= dip->di_volume_limit) &&
	      (dip->di_volume_records == dip->di_volume_records)) {
	acbp->aio_fildes = AIO_NotQed;
	return (SUCCESS);
    }

    /*
     * Look at errors early, to determine of this is a retriable error.
     */
    if (count == FAILURE) {
	hbool_t eio_flag = os_isIoError(error);
	hbool_t isEof_flag = os_isEof(count, error);

	if (isEof_flag == False) {
	    int rc;
	    INIT_ERROR_INFO(eip, dip->di_dname, OS_AIO_READ, READ_OP,
			    &acbp->aio_fildes, dip->di_oflags,
			    acbp->aio_offset, acbp->aio_nbytes,
			    error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    rc = ReportRetryableError(dip, eip, "Failed AIO reading %s", dip->di_dname);
	    if (rc == RETRYABLE) {
		error = dtaio_restart(dip, acbp);
		if (error) {
		    acbp->aio_fildes = AIO_NotQed;
		    return(error);
		}
		goto retry;
	    }
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		(void)ExecuteTrigger(dip, "read");
	    }
	    acbp->aio_fildes = AIO_NotQed;
	    return (FAILURE);
	}
    } else if (error) {
	count = FAILURE;
    }

    acbp->aio_fildes = AIO_NotQed;
    dip->di_data_buffer = (u_char *)acbp->aio_buf;

    if (dip->di_min_size) {
	dsize = bsize;
    } else {
	dsize = dip->di_block_size;
    }

    /*
     * Adjust for short records or no data transferred.
     */
    if (count == FAILURE) {
	dip->di_aio_data_bytes -= bsize;
	dip->di_aio_file_bytes -= bsize;
    } else if ( (adjust = (ssize_t)(bsize - count)) ) {
	if (dip->di_debug_flag) {
	    Printf(dip, "Adjusting byte counts by %d bytes...\n", adjust);
	}
	dip->di_aio_data_bytes -= adjust;
	dip->di_aio_file_bytes -= adjust;
    }

    /*
     * Process end of file/media conditions and handle multi-volume.
     */
    if ( ( (count == 0) || (count == FAILURE) ) &&
	 ( is_Eof(dip, count, bsize, &status) ) ) {
	if (dip->di_multi_flag) {
	    if ( (dip->di_dtype->dt_dtype == DT_TAPE) &&
		 !dip->di_end_of_logical ) {
		return (status);	/* Expect two file marks @ EOM. */
	    }
	    status = HandleMultiVolume(dip);
	    dip->di_aio_record_count = dip->di_records_read;
	    /*dip->di_aio_file_bytes = dip->di_dbytes_read;*/
	    dip->di_aio_offset = (Offset_t) 0;
	}
	return (status);
    } else {
	dip->di_end_of_file = False;	/* Reset saved end of file state. */
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
	    dip->di_offset = acbp->aio_offset;
	}
	if ((status = check_read(dip, count, bsize)) == FAILURE) {
	    if (dip->di_error_count >= dip->di_error_limit) return (status);
	} else if (dip->di_io_mode == COPY_MODE) {
	    ssize_t wcount;
	    odip->di_offset = acbp->aio_offset;
	    wcount = copy_record(odip, dip->di_data_buffer, count, acbp->aio_offset, &status);
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
		    return(status); /* For disks, stop I/O at end of media. */
		}
	    }
	    if (status == FAILURE) {	/* Write failed! */
		dip->di_error_count++;
	    } else if (wcount != count) {
		Wprintf(dip, "Partial write, write count %d < read count %d, failing!\n", wcount, count);
		Eprintf(dip, "Partial writes are NOT supported, failing the copy operation!\n");
		return(FAILURE);
	    }
	    if ( (dip->di_error_count >= dip->di_error_limit) || dip->di_end_of_file) return(status);
	} else if (dip->di_io_mode == VERIFY_MODE) {
	    ssize_t rcount = verify_record(odip, dip->di_data_buffer, count, acbp->aio_offset, &status);
	    if (odip->di_end_of_file) {
		dip->di_end_of_file = odip->di_end_of_file;
	    } else if (status == FAILURE) {
		dip->di_error_count++;
	    }
	    if ( (dip->di_error_count >= dip->di_error_limit) || dip->di_end_of_file) return(status);
	}
    }

    /*
     * Verify the data (unless disabled).
     */
    if ( (status != FAILURE) && dip->di_compare_flag && (dip->di_io_mode == TEST_MODE)) {
	ssize_t vsize = count;
	if (dip->di_lbdata_flag || dip->di_iot_pattern) {
	    dip->di_aio_lba = make_lbdata(dip, (dip->di_volume_bytes + acbp->aio_offset));
	    if (dip->di_iot_pattern) {
		if (dip->di_btag) {
		    update_buffer_btags(dip, dip->di_btag, acbp->aio_offset,
					dip->di_pattern_buffer, vsize, (dip->di_records_read + 1));
		}
		dip->di_aio_lba = init_iotdata(dip, dip->di_pattern_buffer, vsize, dip->di_aio_lba, dip->di_lbdata_size);
	    }
	}
	status = (*dtf->tf_verify_data)(dip, dip->di_data_buffer, vsize, dip->di_pattern, &dip->di_aio_lba, False);
	/*
	 * Verify the pad bytes (if enabled).
	 */
	if ( (status == SUCCESS) && dip->di_pad_check) {
	    (void)verify_padbytes(dip, dip->di_data_buffer, vsize, ~dip->di_pattern, bsize);
	}
    }
    dip->di_records_read++;
    dip->di_volume_records++;

    /*
     * Special handling of step option:
     */
    if ( (dip->di_io_dir == FORWARD)  	       &&
	 dip->di_step_offset && dip->di_slices &&
	 ((acbp->aio_offset + dip->di_step_offset + (Offset_t)dsize) >= dip->di_end_position) ) {
	set_Eof(dip);
    } else if (dip->di_io_dir == REVERSE) {
	if ( (acbp->aio_offset == (Offset_t) dip->di_file_position) ||
	     (dip->di_step_offset && ((acbp->aio_offset - dip->di_step_offset) <= (Offset_t)dip->di_file_position)) ) {
	    set_Eof(dip);
	    dip->di_beginning_of_file = True;
	}
    }
    return (status);
}

/************************************************************************
 *									*
 * dtaio_write_data() - Write specified data to the output file.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
dtaio_write_data(struct dinfo *dip)
{
    register struct aiocb *acbp;
    int error, status = SUCCESS;
    register size_t bsize, dsize;
    large_t data_limit;
    u_int32 lba = dip->di_lbdata_addr;

    if (dip->di_random_access) {
	if ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
	    dip->di_aio_offset = set_position(dip, (Offset_t)dip->di_rdata_limit, False);
	}
	dip->di_aio_lba = lba = get_lba(dip);
	dip->di_aio_offset = get_position(dip);
    } else {
	dip->di_aio_offset = dip->di_offset;
	dip->di_aio_lba = lba = make_lbdata(dip, dip->di_aio_offset);
    }
    dip->di_aio_data_bytes = dip->di_aio_file_bytes = dip->di_aio_record_count = 0;

    dsize = get_data_size(dip, WRITE_OP);
    data_limit = get_data_limit(dip);

    if ( (dip->di_fill_always == True) || (dip->di_fill_once == True) ) {
	if ( (dip->di_fill_always == True) || (dip->di_pass_count == 0) ) {
	    status = prefill_file(dip, dip->di_block_size, data_limit, dip->di_aio_offset);
	    if (status == FAILURE) {
		return(status);
	    }
	}
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

	if ( (dip->di_maxdata_reached == True) ||
	     dip->di_max_data && (dip->di_maxdata_written >= dip->di_max_data) ) {
	    dip->di_maxdata_reached = True;
	    break;
	}

	if ( (dip->di_volumes_flag == True)			&&
	     (dip->di_multi_volume >= dip->di_volume_limit)	&&
	     (dip->di_volume_records >= dip->di_volume_records)) {
	    dip->di_volume_records = dip->di_volume_records;
	    break;
	}

	/*
	 * Two loops are used with AIO.  The inner loop queues requests up
	 * to the requested amount, and the outer loop checks the actual
	 * data processed.  This is done to handle short reads, which can
	 * happen frequently with random I/O and large block sizes.
	 */
	while ( (dip->di_error_count < dip->di_error_limit) &&
		(dip->di_aio_record_count < dip->di_record_limit) &&
		(dip->di_aio_file_bytes < data_limit) ) {

	    PAUSE_THREAD(dip);
	    if ( THREAD_TERMINATING(dip) ) break;
	    if (dip->di_terminating) break;

	    if ( dip->di_max_data &&
		 ((dip->di_aio_file_bytes + dip->di_maxdata_written) >= dip->di_max_data) ) {
		dip->di_maxdata_reached = True;
		break;
	    }

	    if ( (dip->di_volumes_flag == True)			&&
		 (dip->di_multi_volume >= dip->di_volume_limit)	&&
		 (dip->di_volume_records >= dip->di_volume_records)) {
		break;
	    }

	    if (dip->di_write_delay) {			/* Optional write delay	*/
		mySleep(dip, dip->di_write_delay);
	    }

	    /*
	     * If data limit was specified, ensure we don't exceed it.
	     */
	    if ( (dip->di_aio_file_bytes + dsize) > data_limit) {
		bsize = (size_t)(data_limit - dip->di_aio_file_bytes);
	    } else {
		bsize = dsize;
	    }

	    acbp = &dip->di_acbs[dip->di_aio_index];
	    /*
	     * If requested, rotate the data buffer through ROTATE_SIZE bytes
	     * to force various unaligned buffer accesses.
	     */
	    if (dip->di_rotate_flag) {
		dip->di_data_buffer = dip->di_aiobufs[dip->di_aio_index];
		dip->di_data_buffer += (dip->di_rotate_offset++ % ROTATE_SIZE);
		acbp->aio_buf = dip->di_data_buffer;
	    } else {
		dip->di_data_buffer = (u_char *) acbp->aio_buf;
	    }

	    if ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
		/*debug*/ if (!dip->di_aio_offset) abort(); /*debug*/
		bsize = (size_t)MIN((dip->di_aio_offset - dip->di_file_position), (Offset_t)bsize);
		dip->di_aio_offset = (Offset_t)(dip->di_aio_offset - bsize);
	    }

            if (dip->di_debug_flag && (bsize != dsize) && !dip->di_variable_flag) {
                Printf(dip, "Record #%lu, Writing a partial record of %d bytes...\n",
		       (dip->di_aio_record_count + 1), bsize);
            }

	    if (dip->di_io_type == RANDOM_IO) {
		acbp->aio_offset = do_random(dip, False, bsize);
	    } else {
		acbp->aio_offset = dip->di_aio_offset;
	    }

	    if (dip->di_iot_pattern || dip->di_lbdata_flag) {
		lba = make_lbdata(dip, (dip->di_volume_bytes + acbp->aio_offset));
	    }

	    /*
	     * Initialize the data buffer with a pattern.
	     */
	    if ((dip->di_io_mode == TEST_MODE) && dip->di_compare_flag) {
	        if (dip->di_iot_pattern) {
		    lba = init_iotdata(dip, dip->di_data_buffer, bsize, lba, dip->di_lbdata_size);
		} else {
		    fill_buffer(dip, dip->di_data_buffer, bsize, dip->di_pattern);
		}
	    }

	    /*
	     * Initialize the logical block data (if enabled).
	     */
	    if (dip->di_lbdata_flag && dip->di_lbdata_size && !dip->di_iot_pattern) {
		lba = winit_lbdata(dip, (dip->di_volume_bytes + acbp->aio_offset),
				   dip->di_data_buffer, bsize, lba, dip->di_lbdata_size);
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
		update_buffer_btags(dip, dip->di_btag, acbp->aio_offset,
				    dip->di_data_buffer, bsize, (dip->di_aio_record_count + 1));
	    }

	    acbp->aio_fildes = dip->di_fd;
            acbp->aio_nbytes = bsize;

	    if (dip->di_Debug_flag) {
		report_io(dip, WRITE_MODE, (void *)acbp->aio_buf, acbp->aio_nbytes, acbp->aio_offset);
	    }
	    
#if defined(WIN32)
	    /* TODO: Clean this up! */
	    acbp->overlap.hEvent = 0;
	    acbp->overlap.Offset = ((PLARGE_INTEGER)(&acbp->aio_offset))->LowPart;
	    acbp->overlap.OffsetHigh = ((PLARGE_INTEGER)(&acbp->aio_offset))->HighPart;
	    error = WriteFile(acbp->aio_fildes, acbp->aio_buf, (DWORD)acbp->aio_nbytes,
			      NULL, &acbp->overlap);
	    if ((!error) && (GetLastError() != ERROR_IO_PENDING)) {
		error = FAILURE;
		acbp->aio_fildes = AIO_NotQed;
		/*
		 * Unlike POSIX AIO, WriteFile() returns ERROR_DISK_FULL
		 * when queuing the request, so handle the condition!
		 */
	    	if ( is_Eof(dip, error, bsize, &error) ) {
		    break;	/* Process outstanding requests below. */
		} else {
		    ReportErrorInfo(dip, dip->di_dname, os_get_error(), "WriteFile", WRITE_OP, True);
		    return (error);
		}
	    }
#else /* !defined(WIN32) */
# if defined(_AIO_AIX_SOURCE)
	    if ( (error = aio_write(acbp->aio_fildes, acbp)) == FAILURE) {
# else /* !defined(_AIO_AIX_SOURCE) */
	    if ( (error = aio_write(acbp)) == FAILURE) {
# endif /* defined(_AIO_AIX_SOURCE) */
		acbp->aio_fildes = AIO_NotQed;
		ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_AIO_WRITE, WRITE_OP, True);
		return (error);
	    }
#endif /* defined(WIN32) */
	    /*
	     * Must adjust record/data counts here to avoid writing
	     * too much data, even though the writes are incomplete.
	     */
	    dip->di_aio_data_bytes += bsize;
	    dip->di_aio_file_bytes += bsize;
	    dip->di_aio_record_count++;

	    if (dip->di_io_dir == FORWARD) {
		dip->di_aio_offset += bsize;
	    } 

	    if (dip->di_step_offset) {
		if (dip->di_io_dir == FORWARD) {
		    dip->di_aio_offset += dip->di_step_offset;
		} else if ((dip->di_aio_offset -= dip->di_step_offset) <= (Offset_t) dip->di_file_position) {
		    dip->di_aio_offset = (Offset_t)dip->di_file_position;
		}
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

	    /*
	     * Always ensure the next control block has completed.
	     */
	    if (++dip->di_aio_index == dip->di_aio_bufs) dip->di_aio_index = 0;

	    /*
	     * Special handling of step option:
	     */
	    if ( (dip->di_io_dir == FORWARD)	       &&
		 dip->di_step_offset && dip->di_slices &&
		 ((dip->di_aio_offset + (Offset_t)dsize) >= dip->di_end_position) ) {
        	dsize = (size_t)(dip->di_end_position - dip->di_aio_offset);
		break;
	    } else if ( (dip->di_io_dir == REVERSE) && (dip->di_aio_offset == (Offset_t)dip->di_file_position) ) {
		break;
	    }
	    acbp = &dip->di_acbs[dip->di_aio_index];
	    if (acbp->aio_fildes == AIO_NotQed) continue; /* Never Q'ed. */

	    if ( (status = dtaio_process_write(dip, acbp)) == FAILURE) {
		return (status);
	    }
	    if (dip->di_end_of_file) break;
	}
	/*
	 * We get to this point after we've Q'ed enough requests to
	 * fulfill the requested record and/or data limit.  We now
	 * wait for these Q'ed requests to complete, adjusting the
	 * global transfer statistics appropriately which reflects
	 * the actual data processed.
	 */
	status = dtaio_wait_writes(dip);

	/*
	 * For regular files encountering premature end of file due
	 * to "file system full" (ENOSPC), then truncate the file at
	 * the last data we wish to process during the read pass.
	 */
	if ( dip->di_discarded_write_data		&&
	     (dip->di_dtype->dt_dtype == DT_REGULAR)	&&
	     (dip->di_io_dir == FORWARD)		&&
	     (dip->di_io_type == SEQUENTIAL_IO)		&&
	     (dip->di_slices == 0) ) {
	    (void)dt_ftruncate_file(dip, dip->di_dname, dip->di_fd, dip->di_fbytes_written, NULL, EnableErrors); 
	}

	if (dip->di_end_of_file) break;
    }
   return (status);
}

/************************************************************************
 *									*
 * dtaio_process_write() - Process AIO write requests.			*
 *									*
 * Description:								*
 *	This function does waits for the requested AIO write request	*
 * and checks the completion status.					*
 *									*
 * Inputs:	dip = The device info pointer.				*
 *		acbp = The AIO control block.				*
 *									*
 * Outputs:	Returns SUCCESS/FAILURE/WARNING = Ok/Error/Partial.	*
 *									*
 ************************************************************************/
static int
dtaio_process_write(struct dinfo *dip, struct aiocb *acbp)
{
    register size_t bsize, dsize;
    register ssize_t count;
    ssize_t adjust;
    int error, status = SUCCESS;

    dip->di_retry_count = 0;
retry:
    dip->di_current_acb = acbp;
    error = dtaio_wait(dip, acbp);
#if defined(WIN32)
    /* total bytes wrote by WriteFile call or FAILURE in case of error */
    count = acbp->bytes_rw; 
    error = acbp->last_error;
#else /* !defined(WIN32) */
    count = aio_return(acbp);
#endif /* defined(WIN32) */

    errno = error;
    bsize = acbp->aio_nbytes;

    if (dip->di_history_size) {
	save_history_data(dip,
			  (dip->di_files_written + 1), (dip->di_records_written + 1),
			  WRITE_MODE, acbp->aio_offset, (u_char *)acbp->aio_buf, bsize, count);
    }

    if (dip->di_volumes_flag && (dip->di_multi_volume >= dip->di_volume_limit) &&
	      (dip->di_volume_records == dip->di_volume_records)) {
	acbp->aio_fildes = AIO_NotQed;
	return (SUCCESS);
    }
    /*
     * Look at errors early, to determine of this is a retriable error.
     */
    if (count == FAILURE) {
	//int error = os_get_error();
	hbool_t eio_flag = os_isIoError(error);
	hbool_t isEof_flag = os_isEof(count, error);

	if (isEof_flag == False) {
	    int rc;
	    INIT_ERROR_INFO(eip, dip->di_dname, OS_AIO_WRITE, WRITE_OP,
			    &acbp->aio_fildes, dip->di_oflags,
			    acbp->aio_offset, acbp->aio_nbytes,
			    error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    rc = ReportRetryableError(dip, eip, "Failed AIO writing %s", dip->di_dname);
	    if (rc == RETRYABLE) {
		error = dtaio_restart(dip, acbp);
		if (error) {
		    acbp->aio_fildes = AIO_NotQed;
		    return(error);
		}
		goto retry;
	    }
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		(void)ExecuteTrigger(dip, "write");
	    }
	    acbp->aio_fildes = AIO_NotQed;
	    return (FAILURE);
	}
    } else if (error) {
	count = FAILURE;
    }

    acbp->aio_fildes = AIO_NotQed;

    /*
     * Handle ENOSPC special when writes complete out of order:
     *
     * dt: End of media detected, count = -1, errno = 28 [file #1, record #4113]
     *     < Note: Record #4114 was a full record! >
     * dt: WARNING: Record #4115, attempted to write 262144 bytes, wrote only 122880 bytes.
     * dt: End of media detected, count = -1, errno = 28 [file #1, record #4116]
     *
     * This is necessary to avoid false miscompares during the read pass!
     *
     * Note: The data is still written (of course), but we won't verify during reads.
     *       If the file is kept and used for subsequent reads, then that's a problem!
     */
    if ( dip->di_end_of_file && (count != FAILURE) &&
	 (dip->di_dtype->dt_dtype == DT_REGULAR) ) {
	if (dip->di_debug_flag) {
	    Printf(dip, "EOF set, discarding record data of %u bytes, offset " FUF "\n",
		   count, acbp->aio_offset);
	}
	dip->di_discarded_write_data += count;
	return (WARNING);
    }

    dsize = bsize;

    /*
     * Adjust for short records or no data transferred.
     */
    if (count == FAILURE) {
	dip->di_aio_data_bytes -= bsize;
	dip->di_aio_file_bytes -= bsize;
    } else if ( (adjust = (ssize_t)(bsize - count)) ) {
	dip->di_aio_data_bytes -= adjust;
	dip->di_aio_file_bytes -= adjust;
    }

    /*
     * Note: Don't adjust these counts, if we've hit EOF already.
     */
    if ( (count > (ssize_t) 0) && (dip->di_end_of_file == False) ) {
	dip->di_dbytes_written += count;
	dip->di_fbytes_written += count;
	dip->di_vbytes_written += count;
	dip->di_maxdata_written += count;
    }

    /*
     * Process end of file/media conditions and handle multi-volume.
     */
    if ( ( (count == 0) || (count == FAILURE) ) &&
	 ( is_Eof(dip, count, bsize, &status) ) ) {
	if (dip->di_last_write_size == 0) {
	    dip->di_last_write_size = count;
	    dip->di_last_write_attempted = acbp->aio_nbytes;
	    dip->di_last_write_offset = acbp->aio_offset;
	}
	if (dip->di_multi_flag) {
	    status = HandleMultiVolume(dip);
	    dip->di_aio_record_count = dip->di_records_written;
	    /*dip->di_aio_file_bytes = dip->di_dbytes_written;*/
	    dip->di_aio_offset = (Offset_t) 0;
	}
	return (status);
    }

    if (count > (ssize_t) 0) {
        if ((size_t)count == dsize) {
            dip->di_full_writes++;
        } else {
            dip->di_partial_writes++;
	    /*
	     * After the first partial write to a regular file, we set a
	     * premature EOF, to discard further writes above.  This is
	     * necessary, since subsequent writes may succeed, but our
	     * read pass will try to read this entire record, and report
	     * a false data corruption (so we can't read past this point).
	     */
	    if (dip->di_dtype->dt_dtype == DT_REGULAR) {
		if (count < (ssize_t)bsize) {
		    dip->di_no_space_left = True;
		    dip->di_file_system_full = True;
		    if (dip->di_last_write_size == 0) {
			dip->di_last_write_size = count;
			dip->di_last_write_attempted = acbp->aio_nbytes;
			dip->di_last_write_offset = acbp->aio_offset;
		    }
		    set_Eof(dip);
		}
	    }
        }
	dip->di_offset = acbp->aio_offset;
    }
    status = check_write(dip, count, bsize, acbp->aio_offset);
    if (status == FAILURE) {
	if (dip->di_error_count >= dip->di_error_limit) return(status);
    } else if ( (dip->di_io_mode == MIRROR_MODE) || (dip->di_io_mode == VERIFY_MODE) ) {
	dinfo_t *idip = dip->di_output_dinfo;
	ssize_t rcount = verify_record(idip, (u_char *)acbp->aio_buf, count, acbp->aio_offset, &status);
	/* TODO: Need to cleanup multiple device support! */
	/* For now, propagate certain information to writer. */
	if (idip->di_end_of_file) {
	    dip->di_end_of_file = idip->di_end_of_file;
	} else if (status == FAILURE) {
	    dip->di_error_count++;
	}
	if ( (dip->di_error_count >= dip->di_error_limit) || dip->di_end_of_file) return(status);
    }

    dip->di_records_written++;
    dip->di_volume_records++;

    /*
     * Flush data *before* verify (required for buffered mode to catch ENOSPC).
     */ 
    if ( dip->di_fsync_frequency && ((dip->di_records_written % dip->di_fsync_frequency) == 0) ) {
	status = (*dip->di_funcs->tf_flush_data)(dip);
	if ( (status == FAILURE) && (dip->di_error_count >= dip->di_error_limit) ) return (status);
    }

    if ( (status != FAILURE) && dip->di_raw_flag) {
	status = write_verify(dip, (u_char *)acbp->aio_buf, count, dsize, acbp->aio_offset);
	if ( (status == FAILURE) && (dip->di_error_count >= dip->di_error_limit) ) {
	    return (status);
	}
    }

    /*
     * Special handling of step option:
     */
    if ( (dip->di_io_dir == FORWARD)           &&
	 dip->di_step_offset && dip->di_slices &&
	 ((acbp->aio_offset + dip->di_step_offset + (Offset_t)dsize) >= dip->di_end_position) ) {
	set_Eof(dip);
    } else if (dip->di_io_dir == REVERSE) {
	if ( (acbp->aio_offset == (Offset_t) dip->di_file_position) ||
	     (dip->di_step_offset && ((acbp->aio_offset - dip->di_step_offset) <= (Offset_t) dip->di_file_position)) ) {
	    set_Eof(dip);
	    dip->di_beginning_of_file = True;
	}
    }
    return (status);
}

#endif /* defined(AIO) */
