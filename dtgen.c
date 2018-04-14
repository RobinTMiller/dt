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
 * Module:	dtgen.c
 * Author:	Robin T. Miller
 * Date:	September 8, 1993
 *
 * Description:
 *	Generic test functions for the 'dt' program.
 *
 * Modification History:
 * 
 * August 19th, 2015 by Robin T. Miller
 * 	If testing to the Linux memory file system (tmpfs), disable DIO,
 * otherwise EINVAL occurs. This helps avoid false failures and requiring
 * automation to change, which is sadly not always an easy operation! ;(
 * 
 * November 15th, 2014 by Robin T. Miller
 * 	Added sanity check to catch when the pattern buffer is less than
 * the device size when the IOT pattern is selected. Previously, a bs=256
 * would initially setup the wrong pattern buffer size, and even though the
 * block size was later adjusted, the IOT pattern was too small which lead
 * to a false corruption esp. with random I/O. (sigh) Long standing issue!
 * 
 * July 9th, 2014 by Robin T. Miller
 * 	Fixed regression in read_file(), where maxdata_reached flag was
 * initialized to True versus False (stupid cut/paste error on my part!).
 *
 * April 30th, 2014 by Robin T. Miller
 * 	Added isDirectIO() function which handles both buffer modes and
 * direct I/O checks. Switch to using this API in set_open_flags, and also
 * reset the Direct I/O flag (O_DIRECT) if the caller passed this in, otherwise
 * flags=direct enabled DIO even though the buffer mode should have disabled.
 * 
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#include "dt.h"
#if !defined(WIN32)
#  include <sys/file.h>
#endif /* !defined(WIN32) */

/*
 * Forward References:
 */
int restart_write_file(dinfo_t *dip);
int do_copy_eof_handling(dinfo_t *dip, hbool_t next_dir);
char *handle_multiple_files(dinfo_t *dip, char *file, int *oflags, int *status);
int common_open(dinfo_t *dip, char *FileName,
		uint32_t DesiredAccess, uint32_t CreationDisposition,
		uint32_t FileAttributes, uint32_t ShareMode);
void set_open_flags(dinfo_t *dip, char *FileName,
		    uint32_t *DesiredAccess, uint32_t *CreationDisposition,
		    uint32_t *FileAttributes, uint32_t *ShareMode);

/*
 * Declare the generic (default) test functions.
 */
struct dtfuncs generic_funcs = {
    /*	tf_open,		tf_close,		tf_initialize,	  */
	open_file,		close_file,		initialize,
    /*  tf_start_test,		tf_end_test,				  */
	init_file,		nofunc,
    /*	tf_read_file,		tf_read_data,		tf_cancel_reads,  */
	read_file,		read_data,		nofunc,
    /*	tf_write_file,		tf_write_data,		tf_cancel_writes, */
	write_file,		write_data,		nofunc,
    /*	tf_flush_data,		tf_verify_data,		tf_reopen_file,   */
	flush_file,		verify_data,		reopen_file,
    /*	tf_startup,		tf_cleanup,		tf_validate_opts  */
	nofunc,			nofunc,			validate_opts,
    /*	tf_report_btag,		tf_update_btag		tf_verify_btag	  */
	NULL,			NULL,			NULL,
};

char *
handle_multiple_files(dinfo_t *dip, char *file, int *oflags, int *status)
{
    *status = SUCCESS;
    /*
     * Note: This doesn't really belong here, move when there is time!
     */
    if ( (dip->di_retrying == False) && dip->di_fsfile_flag &&
	 (dip->di_dirpath || dip->di_subdir || dip->di_file_limit) ) {
	/* Update the file path and the prefix (as required). */
	file = make_file_name(dip);
	/*
	 * Handle case where we are reading multiple files, but the file 
	 * did not get created by the write workload, either because of a 
	 * file system full, or limited by the maxdata percentage.
	 */
	if (dip->di_input_file && (dip->di_file_number > 1)) {
	    if (dt_file_exists(dip, file) == False) {
		*status = WARNING;	/* Stop reading now. */
	    }
	}
	/* Handle case where previous write pass did not create all the files. */
	/* Note: Upper layers *should* set the file create flag appropriately! */
	if ( dip->di_output_file && !(*oflags & O_CREAT) &&
	     (dip->di_delete_per_pass || dip->di_pass_count) ) {
	    if (os_file_exists(dip->di_dname) == False) {
		*oflags |= O_CREAT;
		if (dip->di_debug_flag) {
		    Printf(dip, "File %s did not exist, so creating...\n", dip->di_dname);
		}
	    }
	}
    }
    return(file);
}

/*
 * common_open() - Common Open File Handling.
 *
 * Description:
 *	This function does the common open processing for open/reopen.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	FileName = The file name to open.
 * 	DesiredAccess = The desired access. (or POSIX open flags)
 * 	CreationDisposition = The creation disposition. (Windows)
 * 	FileAttributes = The file attributes (Windows)
 * 	ShareMode = The share mode. (Windows)
 *
 * Return Value:
 *      Returns SUCCESS/FAILURE/WARNING =
 *      Success / Failure / Warning = No more files (or FS full)
 */
int
common_open(dinfo_t *dip, char *FileName,
	    uint32_t DesiredAccess, uint32_t CreationDisposition,
	    uint32_t FileAttributes, uint32_t ShareMode)
{
    char *file = FileName;
    int oflags = DesiredAccess;
    hbool_t first_time = True;
    int status = SUCCESS;

    init_open_defaults(dip);
    /*
     * Note: This is a stepping stone towards switching to our os_open_file() API! 
     */
    if (dip->di_open_delay) {
	mySleep(dip, dip->di_open_delay);
    }

    /* TODO: Let's move this elsewhere! */
    if ( (strlen(file) == 1) && (*file == '-') ) {
	if (dip->di_debug_flag) {
	    Printf(dip, "Dup'ing standard %s...\n",
		   (dip->di_ftype == INPUT_FILE) ? "input" : "output");
	}
	dip->di_logheader_flag = False;
	if (dip->di_ftype == INPUT_FILE) {
	    dip->di_stdin_flag = True;
	    /* TODO: Make OS specific macros/functions! */
#if defined(WIN32)
	    dip->di_fd = GetStdHandle(STD_INPUT_HANDLE);
#else /* !defined(WIN32) */
	    dip->di_fd = dup(fileno(stdin));
#endif /* defined(WIN32) */
	} else { /* stdout processing */
	    ofp = efp; 		/* Redirect output to stderr. */
	    dip->di_stdout_flag = True;
#if defined(WIN32)
	    dip->di_fd = GetStdHandle(STD_OUTPUT_HANDLE);
#else /* !defined(WIN32) */
	    dip->di_fd = dup(fileno(stdout));
#endif /* defined(WIN32) */
	    dip->di_verify_flag = False;
	}
	if (dip->di_fd < 0) {
#if defined(WIN32)
	    ReportErrorInfo(dip, file, os_get_error(), "GetStdHandle", OTHER_OP, True);
#else /* !defined(WIN32) */
	    ReportErrorInfo(dip, file, os_get_error(), "dup", OTHER_OP, True);
#endif /* defined(WIN32) */
	    status = FAILURE;
	}
    } else { /* End of stdin/stdout '-' checks! */
	dip->di_oflags = oflags;
	dip->di_retry_count = 0;
#if defined(MacDarwin) || defined(SOLARIS)
	oflags &= ~O_DIRECT;	/* Clear the psuedo-flag. */
#endif /* defined(MacDarwin) || defined(SOLARIS) */
retry:
	if (dip->di_debug_flag) {
#if defined(WIN32)
	    Printf(dip, "Opening %s file %s, open flags = %#x, disposition = %#x, attributes = %#x, sharemode = %#x...\n",
		   (dip->di_ftype == INPUT_FILE) ? "input" : "output",
		   file, oflags, CreationDisposition, FileAttributes, ShareMode);
#else /* !defined(WIN32) */
	    Printf(dip, "Opening %s file %s, open flags = %#o (%#x)...\n",
		    (dip->di_ftype == INPUT_FILE) ? "input" : "output",
							file, oflags, oflags);
#endif /* defined(WIN32) */
	    if (first_time == True) {
		first_time = False;	/* Avoid being too chatty during retries! */
		if (dip->di_extended_errors == True) {
		    ReportOpenInformation(dip, file, OS_OPEN_FILE_OP, oflags,
					  CreationDisposition, FileAttributes, ShareMode, False);
		}
	    }
	}
	ENABLE_NOPROG(dip, OPEN_OP);
	if (dip->di_ftype == INPUT_FILE) {
#if defined(WIN32)
	    dip->di_fd = CreateFile(file, oflags, ShareMode, NULL,
				    CreationDisposition, FileAttributes, NULL);
#else /* !defined(WIN32) */
	    dip->di_fd = open(file, oflags);
#endif /* defined(WIN32) */
	} else {
#if defined(WIN32)
	    dip->di_fd = CreateFile(file, oflags, ShareMode, NULL,
				    CreationDisposition, FileAttributes, NULL);
#else /* !defined(WIN32) */
	    dip->di_fd = open(file, oflags, 0666);
#endif /* defined(WIN32) */
	}
	DISABLE_NOPROG(dip);
	if (dip->di_fd == NoFd) {
	    int rc;
	    char *op = OS_OPEN_FILE_OP;
	    INIT_ERROR_INFO(eip, file, op, OPEN_OP, &dip->di_fd, oflags, (Offset_t)0, (size_t)0,
			    os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOXERRORS);
	    if ( isFsFullOk(dip, op, file) ) return(WARNING);
	    rc = ReportRetryableError(dip, eip, "Failed to open %s", file);
	    if (rc == RETRYABLE) goto retry;
	    if (dip->di_extended_errors == True) {
		ReportOpenInformation(dip, file, OS_OPEN_FILE_OP, oflags,
				      CreationDisposition, FileAttributes, ShareMode, True);
	    }
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		(void)ExecuteTrigger(dip, "open");
	    }
	    status = FAILURE;
	}
    }
    if ( (status != FAILURE) && dip->di_debug_flag) {
	Printf(dip, "%s file %s successfully opened, fd = %d\n",
	       (dip->di_ftype == INPUT_FILE) ? "Input" : "Output", file, dip->di_fd);
    }
    if ( (status == SUCCESS) && isFileSystemFile(dip) ) {
	dip->di_inode = os_get_fileID(file, dip->di_fd);
    }

#if defined(MacDarwin) || defined(SOLARIS)
    if (status == SUCCESS) {
	hbool_t dio_flag = isDirectIO(dip);
	/* Note: Direct I/O must be enabled *after* open() on Solaris! (no O_DIRECT flag) */
	if (dio_flag == True) {
	    status = os_DirectIO(dip, file, True); /* enable DIO! */
	} else if ( (dip->di_buffer_mode == BUFFERED_IO) && (dio_flag == True) ) {
	    status = os_DirectIO(dip, file, False); /* For NFS, DIO is sticky, so disable! */
	}
    }
#endif /* defined(MacDarwin) || defined(SOLARIS) */

#if defined(WIN32)
    if (status == SUCCESS) {
	status = HandleSparseFile(dip, FileAttributes);
    }
#endif /* defined(WIN32) */

    return (status);
}

void
set_open_flags(dinfo_t *dip, char *FileName,
	       uint32_t *DesiredAccess, uint32_t *CreationDisposition,
	       uint32_t *FileAttributes, uint32_t *ShareMode)
{
    char *file = FileName;
    int oflags = *DesiredAccess;

#if defined(WIN32)
    SetupWindowsFlags(dip, file, oflags, CreationDisposition, FileAttributes);
    if (dip->di_mode == READ_MODE) {
	*CreationDisposition = OPEN_EXISTING; /* File should already exist! */
    }
    /* Keep these flags while clearing any other POSIX flags. */
    oflags &= (GENERIC_READ | GENERIC_WRITE | FILE_APPEND_DATA);
    /* Note: Switch to these during cleanup! */
    dip->di_DesiredAccess = oflags;
    dip->di_CreationDisposition = *CreationDisposition;
    dip->di_FlagsAndAttributes = *FileAttributes;
    dip->di_ShareMode = *ShareMode;
#else /* !defined(WIN32) */
    if (dip->di_mode == READ_MODE) {
	oflags &= ~O_CREAT;	/* The file should already exist! */
    }
#endif /* defined(WIN32) */
    /* 
     * Note to Self: The buffer mode is invoked in the main I/O loop, by calling
     * SetupBufferingMode(). It is peformed there, since we want multiple files
     * to use the same buffering mode.
     * 
     * Also Note: For Windows, we must set the direct I/O psuedo-flag accordingly,
     * since the retry disconnect support uses the OS open API, since uses this
     * flag to enable pass through. Furthermore, this function will rely on this
     * flag (O_DIRECT) being set, when we switch to the same OS open function.
     * FWIW: I have not made the switch now, for fear of regressions and more
     * unit testing, than I have time for at present.
     * 
     * FYI: For file systems, these read/write cache flags are enabled by default.
     * These flags can be modified via disable options, or indirectly by specifying
     * various buffer modes (bufmodes= option). Now one could argue we should only
     * look at these for file system files, but Linux disks can also be unbuffered.
     * Linux does *not* have a character interface, instead using block interface
     * for raw disks too. Although this buffering provides high peformance, via
     * multiple kernel threads flushing buffers, we enable DIO for Linux disks,
     * to avoid missing write flushing errors, and mimic other OS's. Nonetheless,
     * the read/write cache flags can override dt's internal setting for Linux,
     * for folks who desire this (much) higher throughput with synchronous I/O.
     * 
     * Also note that Solaris *must* enable DIO *after* the file is open'ed!
     * 
     * Gee, this simple feature has far reaching implications, eh? :-)
     */
    if ( isDirectIO(dip) == True ) {
	oflags |= O_DIRECT;
    } else {
	oflags &= ~(O_DIRECT);
    }
    *DesiredAccess = oflags;
    return;
}

void
init_open_defaults(dinfo_t *dip)
{
    dip->di_end_of_file = False;
    dip->di_end_of_media = False;
    dip->di_end_of_logical = False;
    dip->di_beginning_of_file = False;
    dip->di_file_system_full = False;
    dip->di_no_space_left = False;

    dip->di_error = 0;
    dip->di_offset = (Offset_t)0;
    dip->di_block_index = 0;
    dip->di_error_lba = 0;
    dip->di_error_offset = (Offset_t)0;
    dip->di_inode = (os_ino_t)0;
    
    /*
     * Reset counters in preparation for the next file.
     */
    if (dip->di_mode == READ_MODE) {
	dip->di_fbytes_read = (large_t) 0;
	dip->di_records_read = (large_t) 0;
    } else {
	dip->di_fbytes_written = (large_t) 0;
	dip->di_records_written = (large_t) 0;
	if (dip->di_raw_flag) {
	    dip->di_fbytes_read = (large_t) 0;
	    dip->di_records_read = (large_t) 0;
	}
    }
    return;
}

/*
 * open_file() - Open an Input/Output File for Read/Write.
 *
 * Description:
 *	This function does the default (generic) open processing.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *	oflags = The device/file open flags.
 *
 * Return Value:
 *      Returns SUCCESS/FAILURE/WARNING =
 *      Success / Failure / Warning = No more files (multiple input files)
 */
int
open_file(struct dinfo *dip, int oflags)
{
    char *file = dip->di_dname;
    int status = SUCCESS;
#if defined(WIN32)
    DWORD CreationDisposition = 0;
    DWORD FileAttributes = 0;
    DWORD ShareMode = (FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE);
#else /* !defined(WIN32) */
    uint32_t CreationDisposition = 0, FileAttributes = 0, ShareMode = 0;
#endif /* defined(WIN32) */

    if (dip->di_open_delay) {			/* Optional open delay.	*/
	mySleep(dip, dip->di_open_delay);
    }

    /*
     * Note: This doesn't really belong here, move when there is time!
     */
    file = handle_multiple_files(dip, file, &oflags, &status);
    /* Note: Callers should convert WARNING into SUCCESS! */
    if (status == WARNING) return(status);

    /*
     * Setup open flags, unless stdin/stdout was specified.
     */
    if ( (strlen(file) == 1) && (*file != '-') ) {
#if defined(WIN32)
	SetupWindowsFlags(dip, file, oflags, &CreationDisposition, &FileAttributes);
	/* This is messy and needs cleaned up! Clears POSIX flags!  */
	oflags &= (GENERIC_READ | GENERIC_WRITE | FILE_APPEND_DATA);
	/* Note: Switch to these during cleanup! */
	dip->di_DesiredAccess = oflags;
	dip->di_CreationDisposition = CreationDisposition;
	dip->di_FlagsAndAttributes = FileAttributes;
	dip->di_ShareMode = ShareMode;
#else /* !defined(WIN32) */
	/* Note: Eventually, O_DIRECT will be used for Windows in os_open_file()! */
	if ( (dip->di_mode == READ_MODE) && (dip->di_read_cache_flag == False) ) {
	    oflags |= O_DIRECT;
	} else if ( (dip->di_mode == WRITE_MODE) && (dip->di_write_cache_flag == False) ) {
	    oflags |= O_DIRECT;
	}
#endif /* defined(WIN32) */
    } else {
	set_open_flags(dip, file, (uint32_t *)&oflags,
		       &CreationDisposition, &FileAttributes, &ShareMode);
    }

    /* Note: The open flags == DesiredAccess for Windows! */
    status = common_open(dip, file, oflags, CreationDisposition, FileAttributes, ShareMode);
    return(status);
}

/*
 * reopen_file() - Close and Reopen an Existing File.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *	oflags = The device/file open flags.
 *
 * Return Value:
 *      Returns SUCCESS/FAILURE/WARNING =
 *      Success / Failure / Warning = No more files (multiple input files)
 */
/* Note: This is mostly a dup of open_file()... clean this up! */
int
reopen_file(struct dinfo *dip, int oflags)
{
    struct dtfuncs *dtf = dip->di_funcs;
    char *file = dip->di_dname;
    int status = SUCCESS;
#if defined(WIN32)
    DWORD CreationDisposition = 0;
    DWORD FileAttributes = 0;
    DWORD ShareMode = (FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE);
#else /* !defined(WIN32) */
    uint32_t CreationDisposition = 0, FileAttributes = 0, ShareMode = 0;
#endif /* defined(WIN32) */

    /*
     * For stdin or stdout, do not attempt close/open.
     */
    if ( (strlen(file) == 1) && (*file == '-') ) return (status);

    file = handle_multiple_files(dip, file, &oflags, &status);
    /* Note: Callers should convert WARNING into SUCCESS! */
    if (status == WARNING) return(status);

    set_open_flags(dip, file, (uint32_t *)&oflags,
		   &CreationDisposition, &FileAttributes, &ShareMode);

    if (dip->di_fd != NoFd) {
	status = (*dtf->tf_close)(dip);
    }

    /* Note: The open flags == DesiredAccess for Windows! */
    status = common_open(dip, file, oflags, CreationDisposition, FileAttributes, ShareMode);
    return(status);
}

/************************************************************************
 *									*
 * close_file() - Close an open file descriptor.			*
 *									*
 * Description:								*
 *	This function does the default (generic) close processing.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Return Value:							*
 *		Returns 0 / -1 = SUCCESS / FAILURE.			*
 *									*
 ************************************************************************/
int
close_file(struct dinfo *dip)
{
    char *file = dip->di_dname;
    int status = SUCCESS;
    int rc = SUCCESS;

    if (dip->di_closing || (dip->di_fd == NoFd)) {
	return (status);
    }

    dip->di_closing = True;
    if (dip->di_close_delay) {
	mySleep(dip, dip->di_close_delay);
    }
    if (dip->di_debug_flag) {
	Printf (dip, "Closing file %s, fd = %d...\n", file, dip->di_fd);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, CLOSE_OP);
	status = os_close_file(dip->di_fd);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    char *op = OS_CLOSE_FILE_OP;
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, op, CLOSE_OP, NULL, 0, (Offset_t)0, (size_t)0,
			    os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
    
	    dip->di_file_system_full = os_isDiskFull(error);
	    /* If restarting on file system full, log as a warning. */
	    if ( dip->di_file_system_full && dip->di_fsfull_restart ) {
		eip->ei_log_level = logLevelWarn;
		eip->ei_prt_flags = PRT_NOFLAGS;
		eip->ei_rpt_flags = (RPT_WARNING|RPT_NOHISTORY);
	    }
	    rc = ReportRetryableError(dip, eip, "Failed closing %s", file);

	    /* Note: We are not retrying file system full errors right now. */
	    /* Also Note: New FS full restart logic now handles this condition! */
	    if ( dip->di_file_system_full && dip->di_fsfull_restart ) {
		status = SUCCESS;
		break;
	    }
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    dip->di_fd = NoFd;
    dip->di_closing = False;
    
    if (status == FAILURE) {
	if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
	     (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
	    (void)ExecuteTrigger(dip, "close");
	}
    }
    return(status);
}

/************************************************************************
 *									*
 * initialize() - Do the default program initialization.		*
 *									*
 * Description:								*
 *	This function does the default (generic) test initialization.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Always SUCCESS right now.				*
 *									*
 ************************************************************************/
int
initialize(struct dinfo *dip)
{
    if (dip->di_data_buffer == NULL) {
	dip->di_base_buffer = dip->di_data_buffer =
	    malloc_palign(dip, dip->di_data_alloc_size, dip->di_align_offset);
    }
    return (SUCCESS);
}

/************************************************************************
 *									*
 * init_file() - Initial file processing.				*
 *									*
 * Description:								*
 *	This function is used to process options before starting tests.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Outputs:	Returns SUCCESS or FAILURE				*
 *									*
 ************************************************************************/
int
init_file(struct dinfo *dip)
{
    int status = SUCCESS;

    dip->di_offset = dip->di_last_position = (Offset_t)0;
    /*
     * If the lba option is specified, and we're a disk device,
     * then setup a file position to be seek'ed to below.
     */
    if ( dip->di_lbdata_addr && !dip->di_user_position && isDiskDevice(dip) ) {
	dip->di_file_position = make_position(dip, dip->di_lbdata_addr);
	if ( (dip->di_io_type == RANDOM_IO) && (dip->di_rdata_limit <= (large_t)dip->di_file_position) ) {
	    Eprintf(dip, "Please specify a random data limit > lba file position!\n");
	    return(FAILURE);
	}
    }

    status = dt_post_open_setup(dip);
    if (status == FAILURE) return(status);

    /*
     * Seek to specified offset (if requested).
     */
    if (dip->di_file_position) {
	dip->di_last_position = set_position(dip, dip->di_file_position, False);
    }

    /*
     * Seek to specified record (if requested).
     */
    if (dip->di_seek_count) {
	dip->di_last_position = seek_file(dip, dip->di_fd, dip->di_seek_count, dip->di_block_size, SEEK_CUR);
	if (dip->di_last_position == (Offset_t)FAILURE) return(FAILURE);
	show_position (dip, dip->di_last_position);
    }

    /* TODO: Revisit this once proper multiple device support is added! */
#if 0
    /*
     * Allow seek/position options with copy/mirror/verify modes.
     */ 
    if ( dip->di_output_dinfo && dip->di_random_access && (dip->di_io_mode != TEST_MODE) ) {
	dinfo_t *odip = dip->di_output_dinfo;
	if (odip->di_ofile_position) {
	    odip->di_last_position = set_position(odip, odip->di_ofile_position, False);
	} else {
	    odip->di_last_position = set_position(odip, odip->di_last_position, False);
	}
	if (odip->di_last_position == (Offset_t)FAILURE) return(FAILURE);
	show_position(odip, odip->di_last_position);
	odip->di_offset = odip->di_last_position;
    }
#endif /* 0 */

    /*
     * Skip over input record(s) (if requested).
     */
    if (dip->di_skip_count) {
	status = skip_records(dip, dip->di_skip_count, dip->di_data_buffer, dip->di_block_size);
	if (dip->di_debug_flag && (status != FAILURE)) {
	    Printf(dip, "Successfully skipped %d records.\n", dip->di_skip_count);
	}
    }
    if (dip->di_last_position == (Offset_t)FAILURE) {
	status = FAILURE;
    } else {
	dip->di_offset = dip->di_last_position;
    }
    return (status);
}

/************************************************************************
 *									*
 * flush_file() - Flush file data to disk.				*
 *									*
 * Description:								*
 *	This function is used to flush the file data to disk after the	*
 * write pass of each test.						*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Return Value:							*
 *		Returns SUCCESS / FAILURE = Ok / Sync Failed.		*
 *									*
 ************************************************************************/
int
flush_file(struct dinfo *dip)
{
    char *file = dip->di_dname;
    int status = SUCCESS;
    int rc = SUCCESS;

    if ( (dip->di_fd == NoFd) || (dip->di_fsync_flag == False) ) {
	return(status);
    }
    /*
     * Ensure data is sync'ed to disk file.
     */
    dip->di_flushing = True;
    if (dip->di_debug_flag) {
	Printf (dip, "Flushing data on record #%u to file %s, fd = %d...\n",
		dip->di_records_written, file, dip->di_fd);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, FSYNC_OP);
	status = os_flush_file(dip->di_fd);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    char *op = OS_FLUSH_FILE_OP;
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, op, FSYNC_OP, &dip->di_fd, dip->di_oflags,
			    dip->di_offset, (size_t)0, os_get_error(),
			    logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    /* If restarting on file system full, log as a warning. */
	    dip->di_file_system_full = os_isDiskFull(error);
	    if ( dip->di_file_system_full && is_fsfull_restartable(dip) ) {
		eip->ei_log_level = logLevelWarn;
		eip->ei_prt_flags = PRT_NOFLAGS;
		eip->ei_rpt_flags = (RPT_WARNING|RPT_NOHISTORY);
	    }
	    rc = ReportRetryableError(dip, eip, "Failed flushing data to %s", file);

	    /* Note: We are not retrying file system full errors right now. */
	    /* Also Note: New FS full restart logic now handles this condition! */
	    if ( dip->di_file_system_full && dip->di_fsfull_restart ) {
		status = SUCCESS;
		break;
	    }
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );
    
    dip->di_flushing = False;

    if ( dip->di_fsfile_flag &&	(dip->di_debug_flag || dip->di_file_system_full) ) {
	large_t filesize = os_get_file_size(dip->di_dname, dip->di_fd);
	if (filesize != (large_t)FAILURE) {
	    Printf(dip, "After flushing, the file size is " LUF " bytes.\n", filesize);
	}
    }
    if (status == FAILURE) {
	if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
	     (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
	    (void)ExecuteTrigger(dip, "fsync");
	}
    }
    return (status);
}

/*
 * do_copy_eof_handling() - Do output file EOF during multiple file copy.
 */
int
do_copy_eof_handling(dinfo_t *dip, hbool_t next_dir)
{
    int status;

    dip->di_file_number++;
    dip->di_end_of_file = False;
    status = end_file_processing(dip);
    if (status == SUCCESS) {
	if (next_dir == True) {
	    status = process_next_subdir(dip);
	}
	if (status == SUCCESS) {
	    status = process_next_file(dip);
	}
	if (status == WARNING) status = SUCCESS;
    }
    return(status);
}

/************************************************************************
 *									*
 * read_file - Read and optionally verify data in the test file.	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Return Value:							*
 *		Returns SUCCESS/FAILURE = Ok / Error.			*
 *									*
 ************************************************************************/
int
read_file(struct dinfo *dip)
{
    int status;
    struct dtfuncs *dtf = dip->di_funcs;
    unsigned int files_read = 0;

    dip->di_maxdata_reached = False;
    if (dip->di_lbdata_addr) {
	dip->di_offset = make_offset(dip, dip->di_lbdata_addr);
    }

    /*
     * Loop reading/comparing data until we've detected end of file
     * or we've reached the data limit, file limit, or record limit.
     */
    do {					/* Read/compare data. */
	if (dip->di_file_limit && dip->di_fsincr_flag) {
	    dip->di_record_limit = (dip->di_files_read + 1);
	}
#if defined(TAPE)
read_some_more:
#endif
	if ((status = (*dtf->tf_read_data)(dip)) == FAILURE) break;
	if ( THREAD_TERMINATING(dip) ) break;

	if (dip->di_volumes_flag && (dip->di_multi_volume >= dip->di_volume_limit) &&
		  (dip->di_volume_records >= dip->di_volume_records)) {
	    break;
	}
#if defined(TAPE)
	/*
	 * Handle reading multiple disk/tapes and multiple files.
	 */
	if (dip->di_end_of_file && dip->di_multi_flag &&
	    (dip->di_dtype->dt_dtype == DT_TAPE) &&
	    ((dip->di_records_read != dip->di_record_limit) &&
	     (dip->di_fbytes_read != dip->di_data_limit)) ) {
	    /* Check for logical end of tape. */
	    (void) (*dtf->tf_cancel_reads)(dip);
	    if ((status = read_eom(dip)) != SUCCESS) break;
	    if ( dip->di_end_of_file == False ) goto read_some_more;
	}
#endif /* defined(TAPE) */
	if (dip->di_end_of_file) {
	    files_read++;
	    dip->di_files_read++;
	}
	if ( dip->di_fsfile_flag ) {
	    /* Note: EOF may not be set with exact data reads. */
	    if (dip->di_end_of_file == False) {
		files_read++;
		dip->di_files_read++;
	    }
	    if (dip->di_output_file) {
		if ( (dip->di_dbytes_read == dip->di_last_dbytes_written) ||
		     (dip->di_files_read >= dip->di_last_files_written) ) {
		    if (dip->di_eDebugFlag) {
			if (dip->di_multiple_files) {
			    Printf(dip, "DEBUG: File name: %s\n", dip->di_dname);
			}
			Printf(dip, "DEBUG: Finished reading files/data after file #%u, bytes read " FUF ", last written " FUF "\n",
			       dip->di_files_read, dip->di_dbytes_read, dip->di_last_dbytes_written);
		    }
		    break;	/* Don't read more files or data than were written (possible ENOSPC). */
		}
	    }
	}

	/*
	 * Don't exceed the max data or files (if specified).
	 */
	if (dip->di_maxdata_reached == True) break;
	if ( dip->di_max_data && (dip->di_maxdata_read >= dip->di_max_data) ) {
	    break;
	}
	if ( dip->di_max_files && (dip->di_files_read == dip->di_max_files) ) {
	    break;
	}
	if ( ( (dip->di_dtype->dt_dtype == DT_TAPE) || dip->di_fsfile_flag ) &&
	     (dip->di_file_limit && (files_read < dip->di_file_limit)) ) {
	    /*
	     * Normally, we handle EOF conditions on the fly, but with lbdata
	     * or the IOT pattern, we must cancel outstanding I/O's so that
	     * aio_offset (for LBA) is accurate on subsequent files.
	     */
	    if ( (dip->di_lbdata_flag || dip->di_iot_pattern) &&
		 (dip->di_dtype->dt_dtype == DT_TAPE) ) {
		(void) (*dtf->tf_cancel_reads)(dip);
	    }
	    /*
	     * An exact record or data limit keeps us short of file marks,
	     * so we read and check for an expected end of file here.
	     */
	    if (dip->di_end_of_file == False) {
#if defined(TAPE)
		if (dip->di_dtype->dt_dtype == DT_TAPE) {
# if defined(AIX) || defined(WIN32)
		    if ((status = read_eof(dip)) != SUCCESS) break;
# else /* !defined(AIX) */
		    status = DoForwardSpaceFile (dip, (daddr_t) 1);
		    if (status != SUCCESS) break;
# endif /* defined(AIX) || defined(WIN32) */
		}
#endif /* defined(TAPE) */
	    }
	    if (files_read < dip->di_file_limit) {
		dip->di_file_number++;
		dip->di_end_of_file = False;
		if ( dip->di_fsfile_flag ) {
		    status = end_file_processing(dip);
		    if (status == SUCCESS) {
			status = process_next_file(dip);
			if (status == WARNING) {	/* No more input files! */
			    status = SUCCESS;
			    break;
			}
		    }
		    if (status == FAILURE) break;
		    if (dip->di_output_dinfo) {
			status = do_copy_eof_handling(dip->di_output_dinfo, False);
			if (status == FAILURE) {
			    dip->di_error_count++; /* Propogate to reader (for now). */
			    break;
			}
		    }
		} else {
		    dip->di_fbytes_read = (large_t) 0;
		    dip->di_records_read = (large_t) 0;
		}
		continue;
	    }
	} /* End of file limit check. */

	/*
	 * Process Multiple Directories.
	 */
	if ( dip->di_fsfile_flag ) {
	    if ( (status = end_file_processing(dip)) == FAILURE) break;
	    status = process_next_subdir(dip);
	    if (status == SUCCESS) {
		status = process_next_file(dip);
		if (status == WARNING) {	/* No more input files! */
		    status = SUCCESS;
		    break;
		}
		if (status == FAILURE) break;
		/* Initialize files per directory. */
		files_read = 0;
		if (dip->di_output_dinfo) {
		    status = do_copy_eof_handling(dip->di_output_dinfo, True);
		    if (status == FAILURE) {
			dip->di_error_count++; /* Propogate to reader (for now). */
			break;
		    }
		}
		continue;
	    } else { /* Failure or no more directories. */
		if (status == WARNING) status = SUCCESS;
		break;
	    }
	}
    } while ( (dip->di_end_of_file == False)		    &&
	      (dip->di_error_count < dip->di_error_limit)   &&
	      (dip->di_records_read < dip->di_record_limit) &&
	      (dip->di_fbytes_read < dip->di_data_limit) );

    /*
     * We cancel the reads here incase multiple files were being
     * read, so reads continue while we process each file mark.
     */
    if (dip->di_fd != NoFd) {
	int rc;
	(void) (*dtf->tf_cancel_reads)(dip);
	rc = end_file_processing(dip);
	if (rc == FAILURE) status = rc;
    }
    return (status);
}

/************************************************************************
 *									*
 * write_file() - Write data to the test file/device.			*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *									*
 * Return Value:							*
 *		Returns SUCCESS/FAILURE = Ok/Error.			*
 *									*
 ************************************************************************/
int
write_file (struct dinfo *dip)
{
    int status;
    struct dtfuncs *dtf = dip->di_funcs;
    unsigned int files_written = 0;

    dip->di_maxdata_reached = False;
    if (dip->di_lbdata_addr) {
	dip->di_offset = make_offset(dip, dip->di_lbdata_addr);
    }

    /*
     * Initialize the data buffer with a pattern if compare is
     * disabled, so we honor the user specified pattern.
     */
    if ( (dip->di_io_mode == MIRROR_MODE) || (dip->di_io_mode == TEST_MODE) &&
	 (dip->di_compare_flag == False) && (dip->di_mmap_flag == False) ) {
	if (dip->di_iot_pattern) {
	    lbdata_t lba = (lbdata_t)make_lbdata(dip, dip->di_offset);
	    (void)init_iotdata(dip, dip->di_data_buffer, dip->di_block_size, lba, dip->di_lbdata_size);
	} else {
	    fill_buffer(dip, dip->di_data_buffer, dip->di_block_size, dip->di_pattern);
	}
    }

    /*
     * Loop writing data until end of media, file limit, or data limit reached.
     */
    do {					/* Write data pattern.	*/
	if (dip->di_file_limit && dip->di_fsincr_flag) {
	    dip->di_record_limit = (dip->di_files_written + 1);
	}
	/* Now done as part of the open initialization. */
	dip->di_last_write_size = (size_t) 0;
	dip->di_last_write_attempted = (size_t) 0;
	dip->di_last_write_offset = (Offset_t) 0;

	if ((status = (*dtf->tf_write_data)(dip)) == FAILURE) break;
	if ( THREAD_TERMINATING(dip) ) break;

	/*
	 * Handle writing multiple disk/tape files.
	 */
	dip->di_files_written++, files_written++;

	if ( dip->di_volumes_flag &&
	     (dip->di_multi_volume >= dip->di_volume_limit) &&
	     (dip->di_volume_records >= dip->di_volume_records)) {
	    break;
	}
	/*
	 * Don't exceed the max data or files (if specified).
	 */
	if (dip->di_maxdata_reached == True) break;
	if ( dip->di_max_data && (dip->di_maxdata_written >= dip->di_max_data) ) {
	    break;
	}
	if ( dip->di_max_files && (dip->di_files_written == dip->di_max_files) ) {
	    break;
	}
	if ( ( (dip->di_dtype->dt_dtype == DT_TAPE) || dip->di_fsfile_flag ) &&
	     (dip->di_file_limit && (files_written < dip->di_file_limit)) ) {
	    /*
	     * For tapes, write a file mark for all but last file.
	     * The last file mark(s) are written when closing tape.
	     */
#if !defined(AIX) && !defined(WIN32) && defined(TAPE)
            if ( (dip->di_dtype->dt_dtype == DT_TAPE) && (files_written + 1) < dip->di_file_limit) {
                status = DoWriteFileMark (dip, (daddr_t) 1);
                if (status != SUCCESS) break;
            }
#endif /* !defined(AIX) && !defined(WIN32) && defined(TAPE) */
            if (files_written < dip->di_file_limit) {
                dip->di_file_number++;
                if ( dip->di_fsfile_flag ) {
		    if (dip->di_file_system_full || dip->di_no_space_left) break;
		    if ( (status = end_file_processing(dip)) == FAILURE) break;
		    if (dip->di_file_system_full) break;
                    status = process_next_file(dip);
		    if (status == FAILURE) break;
		    if (status == WARNING) {
			status = SUCCESS;
			break;
		    }
                } else {
		    dip->di_fbytes_written = (large_t) 0;
		    dip->di_records_written = (large_t) 0;
		}
                continue;           	/* Write the next file. */
            }
        } /* End of tape/file system file limit check. */

	/*
	 * Process Multiple Directories.
	 */
	if ( dip->di_fsfile_flag ) {
	    if (dip->di_file_system_full || dip->di_no_space_left) break;
	    if ( (status = end_file_processing(dip)) == FAILURE) break;
	    if (dip->di_file_system_full) break;
	    status = process_next_subdir(dip);
	    if (status == SUCCESS) {
		status = process_next_file(dip);
		if (status == FAILURE) break;
		if (status == WARNING) { /* No more files or file system full. */
		    status = SUCCESS;
		    break;
		}
		/* Initialize files per directory. */
		files_written = 0;
		continue;
	    } else { /* Failure or no more directories. */
		if (status == WARNING) {
		    status = SUCCESS;
		}
		break;
	    }
	}
    } while ( (dip->di_end_of_file == False)		       &&
	      (dip->di_error_count < dip->di_error_limit)      &&
              (dip->di_records_written < dip->di_record_limit) &&
              (dip->di_fbytes_written < dip->di_data_limit) );

    if (dip->di_fd != NoFd) {
	int rc = end_file_processing(dip);
	if (rc == FAILURE) status = rc;
    }
    return (status);
}

#if 0
int
restart_write_file(dinfo_t *dip)
{
    int status = WARNING;

    /* Special handling for file system full. */
    /* 
     * We cannot retry any operation requiring random number generator,
     * since we require both I/O sizes and random offsets to exactly 
     * match between the write pass and the read pass.
     */ 
    if ( dip->di_file_system_full &&
	 ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_variable_flag == False) ) ) {
	status = handle_file_system_full(dip, False);
	if (status == SUCCESS) {
	    Wprintf(dip, "Restarting file writing after file system full detected!\n");
	}
	/* Avoid breaking out of do/while loop! */
	dip->di_fbytes_written = (large_t) 0;
	dip->di_records_written = (large_t) 0;
    }
    return(status);
}
#endif /* 0 */

/*
 * validate_opts() - Generic Validate Option Test Criteria.
 *
 * Description:
 *	This function verifies the options specified are valid for the
 * test criteria selected.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *
 * Note: Direct I/O checks *must* use di_open_flags, since di_oflags
 * 	 is *not* initialized until the device is open'ed, which is no
 * 	 longer true with the threaded I/O version (open'ed in thread).
 *
 * Return Value:
 *	Returns SUCCESS / FAILURE = Valid / Invalid Options.
 */
int
validate_opts(struct dinfo *dip)
{
    hbool_t dio_sanity_checks;

    /* 
     * Remember: The device/file is NOT open while validating options! 
     */
    if (dip->di_bypass_flag == True) return(SUCCESS);

    /* If DIO or buffer modes are enabled, do various DIO sanity checks! */
    /* Note: These sanity checks are required for SAN, but not NFS/CIFS! */
    dio_sanity_checks = ( dip->di_dio_flag || (dip->di_bufmode_count != 0) );

    /* TODO: Move to OS specific files! */

    if (dip->di_filesystem_type) {
	if ( EQS(dip->di_filesystem_type, "nfs") ) {
	    dio_sanity_checks = False;
	    if (dip->di_dio_flag) {
		dip->di_fsalign_flag = False;
	    }
	} else if ( EQS(dip->di_filesystem_type, "tmpfs") ) {
	    if (dio_sanity_checks == True) {
		/* Note: This may be true for other OS's as well! */
		if (dip->di_verbose_flag) {
		    Wprintf(dip, "Memory file system detected, disabling Direct I/O!\n");
		}
		/* Reset all the ways DIO can be enabled! */
		dip->di_dio_flag = False;
		dip->di_fsalign_flag = False;
		dip->di_bufmode_count = 0;
		dip->di_open_flags &= ~O_DIRECT;
		dip->di_read_cache_flag = True;
		dip->di_write_cache_flag = True;
	    }
	}
    }
#if defined(WIN32)
    if (dip->di_fsfile_flag) {
	/* 
	 * TODO: This is the correct check, but we cannot request this information
	 * until the file is open'ed and we have a handle, so ineffective today!
	 */
	if ( dip->di_protocol_version && EQS(dip->di_protocol_version, "SMB") ) {
	    dio_sanity_checks = False;
	}
	/* This helps for UNC paths, but not with drive letters!  */
	if ( dip->di_dir &&
	     (EQL(dip->di_dir, "\\\\", 2) || EQL(dip->di_dir, "//", 2)) ) {
	    dio_sanity_checks = False;
	}
    }
#endif /* defined(WIN32) */

    /*
     * Verify min/max options:
     */
    if ( isDiskDevice(dip) || (dip->di_btag_flag == True) ) {
	/* Note: This is to overcome hardcoded automation! (sigh) */
	if ( dip->di_min_size && (dip->di_min_size < dip->di_dsize) ) {
	    if (dip->di_verbose_flag) {
		Wprintf(dip, "Setting the minimum block size to %u for disk or btags.\n", dip->di_dsize);
	    }
	    dip->di_min_size = dip->di_dsize;
	}
	if ( dip->di_max_size && (dip->di_max_size < dip->di_dsize) ) {
	    if (dip->di_verbose_flag) {
		Wprintf(dip, "Setting the maximum block size to %u for disk or btags.\n", dip->di_dsize);
	    }
	    dip->di_max_size = dip->di_dsize;
	}
	if ( (!dip->di_variable_flag && dip->di_incr_count) && (dip->di_incr_count % dip->di_dsize) ) {
	    if (dip->di_verbose_flag) {
		Wprintf(dip, "Setting the increment count to %u for disk or btags.\n", dip->di_dsize);
	    }
	    dip->di_incr_count = dip->di_dsize;
	}
	/* This is the original sanity checks for disks! */
	if (dip->di_min_size || dip->di_max_size) {
	    size_t value;
	    char *emsg = NULL;
	    if (dip->di_dsize > dip->di_min_size) {
		value = dip->di_min_size; emsg = "min size";
	    } else if (dip->di_dsize > dip->di_max_size) {
		value = dip->di_max_size; emsg = "max size";
	    }
	    if (emsg) {
		Eprintf(dip, "Please specify %s (%u) greater than device size %u of bytes.\n",
			emsg, value, dip->di_dsize);
		return(FAILURE);
	    }
	}
	/* 
	 * Catch when user specifies initial block size < the device size!
	 * Note: This *cannot* be caught earlier, since device is not setup.
	 * FYI: False corruptions occur when the IOT pattern buffer is too small!
	 */
	if (dip->di_iot_pattern && (dip->di_pattern_bufsize < dip->di_block_size) ) {
	    size_t pattern_size = dip->di_block_size;
	    void *pattern_buffer = dip->di_pattern_buffer;
	    if (pattern_buffer) free_palign(dip, pattern_buffer);
	    pattern_buffer = malloc_palign(dip, pattern_size, 0);
	    setup_pattern(dip, pattern_buffer, pattern_size, True);
	}
    }
    if (dip->di_variable_flag && ((dip->di_min_size == 0) || (dip->di_max_size == 0)) ) {
	Eprintf(dip, "Please specifiy a min= and max= value with variable I/O sizes!\n");
	return(FAILURE);
    }
#if defined(O_TRUNC)
    if ( (dip->di_ftype == OUTPUT_FILE) &&
	 (dip->di_slices && (dip->di_write_flags & O_TRUNC)) ) {
	if (dip->di_verbose_flag) {
	    Wprintf(dip, "Disabling file truncate flag, not valid with multiple slices!\n");
	}
	dip->di_write_flags &= ~O_TRUNC;
    }
#endif /* defined(O_TRUNC) */

    /*
     * Checks for slices with file system options.
     */
    if ( dip->di_slices	&& (dip->di_ftype == OUTPUT_FILE) && dip->di_fsfile_flag ) {
	
	if (dip->di_dispose_mode != KEEP_FILE) {
	    dip->di_dispose_mode = KEEP_FILE;
	    if (dip->di_verbose_flag) {
		Wprintf(dip, "Multiple slices to the same file, setting dispose=keep!\n");
	    }
	}
	if (dip->di_delete_per_pass) {
	    if (dip->di_verbose_flag) {
		Wprintf(dip, "Disabling delete per pass flag, not valid with multiple slices!\n");
	    }
	    dip->di_delete_per_pass = False;
	}
    } /* if ( dip->di_slices... */

    if ( (dip->di_io_dir == REVERSE) || (dip->di_io_type == RANDOM_IO) ) {
	if ( !dip->di_random_access ) {
	    Eprintf(dip, "Random I/O or reverse direction, is only valid for random access device!\n");
	    return(FAILURE);
	}
	if ( (dip->di_dtype->dt_dtype == DT_REGULAR) && (dip->di_user_capacity == 0) ) {
	    Eprintf(dip, "Please specify a data limit, record count, or capacity for random I/O.\n");
	    return(FAILURE);
	}
    }
    /*
     * Check for when non-modulo request sizes are not permitted.
     */
#if defined(__linux__) || defined(WIN32)
    if ( (dip->di_fsfile_flag == True)  && (dio_sanity_checks == True) ) {

	/* Note: This may be true for other OS's too, but I'm not sure which! */
	if ((dip->di_fsalign_flag == False) &&
	    (dip->di_variable_flag || dip->di_variable_limit || dip->di_random_io)) {
	    if (dip->di_verbose_flag) {
		Wprintf(dip, "Enabling FS alignment for variable I/O sizes and/or data limits!\n");
	    }
	    dip->di_fsalign_flag = True;	/* Avoid false errors due to alignment! */
	}
    }
#endif /* defined(__linux__) || defined(WIN32) */
#if defined(__linux__)
    if ((dip->di_dtype->dt_dtype == DT_REGULAR) && (dio_sanity_checks == True)) {
	/*
	 * This is a bandaide to override automation *not* smart about file systems.
	 * Note: We're adjusting a few things we can do safely, but we can't do all!
	 * Also Note: Many other sanity checks are missed by Mickey Mousing this here!
	 */ 
	if (dip->di_filesystem_type && EQS(dip->di_filesystem_type, "xfs")) {
	    if (dip->di_device_size < XFS_DIO_BLOCK_SIZE) {
		if (dip->di_verbose_flag) {
		    Wprintf(dip, "Setting the device size to %u for XFS filesystem.\n",  XFS_DIO_BLOCK_SIZE);
		}
		dip->di_device_size = dip->di_dsize = XFS_DIO_BLOCK_SIZE;
	    }
	    if (dip->di_min_size < dip->di_dsize) {
		if (dip->di_verbose_flag) {
		    Wprintf(dip, "Setting the minimum block size to %u for XFS filesystem.\n", dip->di_dsize);
		}
		dip->di_min_size = dip->di_dsize;
	    }
	    if (dip->di_max_size < dip->di_dsize) {
		if (dip->di_verbose_flag) {
		    Wprintf(dip, "Setting the maximum block size to %u for XFS filesystem.\n", dip->di_dsize);
		}
		dip->di_max_size = dip->di_dsize;
	    }
	    if ( (!dip->di_variable_flag && dip->di_incr_count) && (dip->di_incr_count % dip->di_dsize) ) {
		if (dip->di_verbose_flag) {
		    Wprintf(dip, "Setting the increment count to %u for XFS filesystem.\n", dip->di_dsize);
		}
		dip->di_incr_count = dip->di_dsize;
	    }
	}
    }
    if ( (dip->di_random_access && (dip->di_dtype->dt_dtype != DT_REGULAR)) || (dio_sanity_checks == True) ) {
#elif defined(WIN32)
    if ( (dip->di_random_access && (dip->di_dtype->dt_dtype != DT_REGULAR)) || (dio_sanity_checks == True) ) {
#else /* all other OS's */
    if (dip->di_random_access && (dip->di_dtype->dt_dtype != DT_REGULAR) ) {
#endif
	size_t value;
	char *emsg = NULL;
	if (dip->di_block_size % dip->di_dsize) {
	    value = dip->di_block_size; emsg = "block size";
	} else if (dip->di_min_size && (dip->di_min_size % dip->di_dsize)) {
	    value = dip->di_min_size; emsg = "min size";
	} else if (dip->di_max_size && (dip->di_max_size % dip->di_dsize)) {
	    value = dip->di_max_size; emsg = "max size";
	} else if ( ((dip->di_variable_flag == False) && dip->di_incr_count) &&
		    (dip->di_incr_count % dip->di_dsize) ) {
	    value = dip->di_incr_count; emsg = "incr count";
	}
	if (emsg) {
	    Eprintf(dip, "Please specify a %s (%u) modulo the device size of %u bytes!\n",
		    emsg, value, dip->di_dsize);
	    return(FAILURE);
	}
    }
    /*
     * Don't allow non-modulo data limits, since most disk drivers will error 
     * when the request size is NOT modulo the sector size or DIO is enabled.
     * For example: 'ReadFile', errno = 87 - The parameter is incorrect.
     */
#if defined(__linux__)
    if ( (dip->di_dtype->dt_dtype == DT_DISK) || (dio_sanity_checks == True) ) {
#elif defined(WIN32)
    if ( (dip->di_dtype->dt_dtype == DT_DISK) || (dio_sanity_checks == True) ) {
#else /* all other OS's */
    if (dip->di_dtype->dt_dtype == DT_DISK) {
#endif /* defined(__linux__) */
	if (dip->di_data_limit &&
	    (dip->di_data_limit != INFINITY) && (dip->di_data_limit % dip->di_dsize) ) {
#if 1
	    large_t adjusted_data_limit = (dip->di_data_limit - (dip->di_data_limit % dip->di_dsize));
	    LogMsg(dip, dip->di_ofp, logLevelWarn, PRT_NOFLAGS,
		   "The data limit was adjusted from "LUF" to "LUF" bytes, modulo the device size of %u bytes!\n",
		   dip->di_data_limit, adjusted_data_limit, dip->di_dsize);
	    /* For disks, if we know the real device size, make sure we're still modulo this size! */
	    if (dip->di_rdsize && (dip->di_dsize != dip->di_rdsize) && (adjusted_data_limit % dip->di_rdsize) ) {
		adjusted_data_limit = (adjusted_data_limit - (adjusted_data_limit % dip->di_rdsize));
		LogMsg(dip, dip->di_ofp, logLevelWarn, PRT_NOFLAGS,
		       "The data limit further adjusted to "LUF" bytes, modulo the *real* device size of %u bytes!\n",
		       adjusted_data_limit, dip->di_rdsize);
	    }
	    LogMsg(dip, dip->di_ofp, logLevelInfo, PRT_NOFLAGS,
		   "Note: If this rounding is undesirable, please specify a data limit or capacity modulo the device size.\n");
	    /* Finally, save the adjusted data limit! */
	    dip->di_data_limit = adjusted_data_limit;
#else /* 0 */
	    Eprintf(dip, "Please specify a data limit (" LUF ") modulo the device size of %u bytes!\n",
		    dip->di_data_limit, dip->di_dsize);
	    return(FAILURE);
#endif /* 1 */
	}
    }
#if defined(__linux__) || defined(WIN32)
    if ( (dio_sanity_checks == True) && (dip->di_align_offset || dip->di_rotate_flag) ) {
	LogMsg (dip, dip->di_efp, logLevelWarn, 0,
		"This OS does NOT support unaligned buffers with direct I/O, disabling misalignments!\n");
	dip->di_align_offset = 0;
	dip->di_rotate_flag = False;
    }
#endif /* defined(__linux__) || defined(WIN32) */

    return(SUCCESS);
}

/*
 * SetupBufferingMode() - Setups up the File System Buffering Mode.
 *
 * Inputs:
 * 	dip = Pointer to device information.
 * 	oflags = Pointer to the open flags.
 *
 * Return Value:
 *	void
 */
void
SetupBufferingMode(dinfo_t *dip, int *oflags)
{
    if (dip->di_bufmode_count == 0) return;

    dip->di_buffer_mode = dip->di_buffer_modes[dip->di_bufmode_index];

    switch (dip->di_buffer_mode) {

	case BUFFERED_IO:
	    if (dip->di_debug_flag) {
		Printf(dip, "Setting buffering mode to: buffered (cache reads and writes)\n");
	    }
	    dip->di_bufmode_type = "buffered";
    	    dip->di_dio_flag = False;
	    *oflags &= ~(O_DIRECT);
	    dip->di_read_cache_flag = True;
	    dip->di_write_cache_flag = True;
	    break;

	case UNBUFFERED_IO:
	    if (dip->di_debug_flag) {
		Printf(dip, "Setting buffering mode to: unbuffered (aka direct I/O)\n");
	    }
	    dip->di_bufmode_type = "unbuffered";
    	    dip->di_dio_flag = True;
	    *oflags |= O_DIRECT;
	    dip->di_read_cache_flag = False;
	    dip->di_write_cache_flag = False;
	    break;

	case CACHE_READS:
	    if (dip->di_debug_flag) {
		Printf(dip, "Setting buffering mode to: cache reads\n");
	    }
	    dip->di_bufmode_type = "cache reads";
    	    dip->di_dio_flag = False;
	    *oflags &= ~(O_DIRECT);
	    dip->di_read_cache_flag = True;
	    dip->di_write_cache_flag = False;
	    break;

	case CACHE_WRITES:
	    if (dip->di_debug_flag) {
		Printf(dip, "Setting buffering mode to: cache writes\n");
	    }
	    dip->di_bufmode_type = "cache writes";
    	    dip->di_dio_flag = False;
	    *oflags &= ~(O_DIRECT);
	    dip->di_read_cache_flag = False;
	    dip->di_write_cache_flag = True;
	    break;
	default:
	    Eprintf(dip, "Programming error, illegal buffer mode: %d\n",  dip->di_buffer_mode);
	    /* Note: This must go for service based I/O tool! */
	    exit (FATAL_ERROR);
	    break;
    }
    if (++dip->di_bufmode_index == dip->di_bufmode_count) {
	dip->di_bufmode_index = 0;
    }
    return;
}

hbool_t
isDirectIO(dinfo_t *dip)
{
    hbool_t dio_flag = False;

    if (dip->di_dio_flag				      		    ||
	((dip->di_mode == READ_MODE) && (dip->di_read_cache_flag == False)) ||
	((dip->di_mode == WRITE_MODE) && (dip->di_write_cache_flag == False)) ) {
	dio_flag = True;
    }
    return(dio_flag);
}
